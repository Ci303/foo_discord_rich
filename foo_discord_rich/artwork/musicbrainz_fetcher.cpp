#include <stdafx.h>

#include "musicbrainz_fetcher.h"

#include <utils/artwork_cache_key.h>

#include <cpr/cpr.h>
#include <qwr/abort_callback.h>

namespace
{

using namespace drp;

const cpr::Timeout kRequestTimeout{ 10000 };
const cpr::ConnectTimeout kConnectTimeout{ 5000 };
const std::chrono::milliseconds kMinimumRequestInterval{ 1100 };
constexpr size_t kMaxLoggedResponseBytes = 4096;
constexpr int kMaxTransientAttempts = 3;
const cpr::Header kJsonRequestHeaders{
    { "Accept", "application/json" },
    { "User-Agent", DRP_UNDERSCORE_NAME "/" DRP_VERSION " (" DRP_HOMEPAGE ")" },
};

void ThrottleRequest( abort_callback& aborter )
{
    static std::mutex mutex;
    static std::chrono::steady_clock::time_point lastRequest;
    std::scoped_lock lock{ mutex };
    const auto now = std::chrono::steady_clock::now();
    const auto nextRequest = lastRequest + kMinimumRequestInterval;
    if ( nextRequest > now )
    {
        aborter.sleep( std::chrono::duration<double>( nextRequest - now ).count() );
    }
    lastRequest = std::chrono::steady_clock::now();
}

template <typename... Args>
cpr::Response GetWithRetry( abort_callback& aborter, Args... args )
{
    cpr::Response response;
    for ( int attempt = 1; attempt <= kMaxTransientAttempts; ++attempt )
    {
        aborter.check();
        ThrottleRequest( aborter );
        const cpr::ProgressCallback abortProgress{
            [&aborter]( cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, intptr_t ) {
                return !aborter.is_aborting();
            } };
        response = cpr::Get( args..., abortProgress );
        aborter.check();
        if ( response.status_code != 429 && response.status_code < 500 )
        {
            break;
        }
        if ( attempt < kMaxTransientAttempts )
        {
            aborter.sleep( static_cast<double>( attempt ) );
        }
    }
    return response;
}

void LogRequest( const cpr::Response& resp )
{
    if ( config::advanced::logWebRequests )
    {
        auto requestUrl = resp.url.str();
        if ( const auto queryStart = requestUrl.find( '?' ); queryStart != qwr::u8string::npos )
        {
            requestUrl.resize( queryStart );
            requestUrl += "?<redacted>";
        }
        LogDebug( "Request: {}", requestUrl );
    }
    if ( config::advanced::logWebResponses )
    {
        LogDebug(
            "Response:\n"
            "  Code: {}\n"
            "  Body:\n"
            "{}\n",
            resp.status_code,
            resp.text.substr( 0, kMaxLoggedResponseBytes ) );
    }
}

/// @throw qwr::QwrException
/// @throw exception_aborted
std::optional<qwr::u8string> FetchReleaseMbid( const qwr::u8string& artist, const qwr::u8string& album, abort_callback& aborter )
{
    using json = nlohmann::json;

    auto releaseGroupResp = GetWithRetry( aborter,
        cpr::Url{ "https://www.musicbrainz.org/ws/2/release-group" },
        kConnectTimeout,
        kRequestTimeout,
        kJsonRequestHeaders,
        cpr::Parameters{
            { "query", fmt::format( "artist:\"{}\"+releasegroup:\"{}\"", artist, album ) },
            { "inc", "releases" },
            { "fmt", "json" },
        } );
    LogRequest( releaseGroupResp );
    if ( releaseGroupResp.status_code == 404 )
    {
        return std::nullopt;
    }
    if ( releaseGroupResp.status_code != 200 )
    {
        throw qwr::QwrException( "Failed to fetch MB release group\nCode: {}\nError: {}", releaseGroupResp.status_code, releaseGroupResp.reason );
    }

    aborter.check();

    try
    {
        auto j = nlohmann::json::parse( releaseGroupResp.text );
        const auto& releaseGroups = j.at( "release-groups" );
        if ( releaseGroups.empty() )
        {
            return std::nullopt;
        }

        for ( const auto& release: releaseGroups.front().at( "releases" ) )
        {
            const auto releaseId = release.at( "id" ).get<qwr::u8string>();
            auto releaseResp = GetWithRetry( aborter,
                cpr::Url{ fmt::format( "https://www.musicbrainz.org/ws/2/release/{}", releaseId ) },
                kConnectTimeout,
                kRequestTimeout,
                kJsonRequestHeaders );
            LogRequest( releaseResp );
            if ( releaseResp.status_code == 404 )
            {
                continue;
            }
            if ( releaseResp.status_code != 200 )
            {
                LogWarning( fmt::format( "Skipping MusicBrainz release {} after HTTP {}", releaseId, releaseResp.status_code ) );
                continue;
            }

            auto jRelease = nlohmann::json::parse( releaseResp.text );
            if ( jRelease.at( "cover-art-archive" ).at( "artwork" ).get<bool>() )
            {
                return releaseId;
            }

            aborter.check();
        }

        return std::nullopt;
    }
    catch ( const nlohmann::json::exception& e )
    {
        throw qwr::QwrException( "Failed to parse MusicBrainz response: {}", e.what() );
    }
}

/// @throw qwr::QwrException
std::optional<qwr::u8string> FetchAlbumArtUrl( const qwr::u8string& mbid, abort_callback& aborter )
{
    auto resp = GetWithRetry( aborter,
        cpr::Url{ fmt::format( "https://coverartarchive.org/release/{}/front-1200", mbid ) },
        kConnectTimeout,
        kRequestTimeout,
        cpr::Header{ { "User-Agent", DRP_UNDERSCORE_NAME "/" DRP_VERSION " (" DRP_HOMEPAGE ")" } } );
    LogRequest( resp );
    if ( resp.status_code == 404 )
    {
        return std::nullopt;
    }
    if ( resp.status_code != 200 )
    {
        throw qwr::QwrException( "Failed to fetch album art url\nCode: {}\nError: {}", resp.status_code, resp.reason );
    }

    return resp.url.str();
}
} // namespace

namespace drp::musicbrainz
{

std::optional<qwr::u8string> FetchArt( const qwr::u8string& artist, const qwr::u8string& album, const std::optional<qwr::u8string>& userReleaseMbidOpt )
{
    auto& aborter = qwr::GlobalAbortCallback::GetInstance();

    if ( userReleaseMbidOpt )
    {
        if ( !artwork::IsCanonicalMbid( *userReleaseMbidOpt ) )
        {
            LogWarning( fmt::format( "Invalid MBID detected: `{}`. Skipping...", *userReleaseMbidOpt ) );
        }
        else
        {
            const auto urlOpt = FetchAlbumArtUrl( *userReleaseMbidOpt, aborter );
            return urlOpt;
        }
    }

    aborter.check();

    const auto releaseMbidOpt = FetchReleaseMbid( artist, album, aborter );
    if ( !releaseMbidOpt )
    {
        LogWarning( fmt::format( "No MusicBrainz release with cover art found for artist `{}` and album `{}`", artist, album ) );
        return std::nullopt;
    }

    aborter.check();

    return FetchAlbumArtUrl( *releaseMbidOpt, aborter );
}

} // namespace drp::musicbrainz
