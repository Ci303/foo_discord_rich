#include <stdafx.h>

#include "theaudiodb_fetcher.h"

#include <utils/theaudiodb.h>
#include <utils/validation.h>

#include <cpr/cpr.h>
#include <qwr/abort_callback.h>

namespace
{

using namespace drp;

const cpr::Timeout kRequestTimeout{ 10000 };
const cpr::ConnectTimeout kConnectTimeout{ 5000 };
const std::chrono::milliseconds kMinimumRequestInterval{ 2100 };
const std::chrono::seconds kRateLimitCooldown{ 60 };
constexpr size_t kMaxLoggedResponseBytes = 4096;
constexpr int kMaxTransientAttempts = 3;
constexpr size_t kMaxRateLimitedKeys = 16;
const cpr::Header kJsonRequestHeaders{
    { "Accept", "application/json" },
    { "User-Agent", DRP_UNDERSCORE_NAME "/" DRP_VERSION " (" DRP_HOMEPAGE ")" },
};

struct RateLimitState
{
    std::mutex mutex;
    std::unordered_map<qwr::u8string, std::chrono::steady_clock::time_point> retryAfterByApiKey;
};

RateLimitState& GetRateLimitState()
{
    static RateLimitState state;
    return state;
}

std::timed_mutex& GetRequestMutex()
{
    static std::timed_mutex mutex;
    return mutex;
}

void MarkRateLimited( const qwr::u8string& apiKey )
{
    auto& state = GetRateLimitState();
    std::scoped_lock lock{ state.mutex };
    const auto now = std::chrono::steady_clock::now();
    std::erase_if( state.retryAfterByApiKey, [now]( const auto& item ) {
        return item.second <= now;
    } );
    if ( !state.retryAfterByApiKey.contains( apiKey ) && state.retryAfterByApiKey.size() >= kMaxRateLimitedKeys )
    {
        const auto earliest = std::min_element( state.retryAfterByApiKey.begin(), state.retryAfterByApiKey.end(), []( const auto& left, const auto& right ) {
            return left.second < right.second;
        } );
        if ( earliest != state.retryAfterByApiKey.end() )
        {
            state.retryAfterByApiKey.erase( earliest );
        }
    }
    state.retryAfterByApiKey.insert_or_assign( apiKey, now + kRateLimitCooldown );
}

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

cpr::Response GetWithRetry(
    abort_callback& aborter,
    const cpr::Url& url,
    const cpr::Parameters& parameters,
    const theaudiodb::RequestValidityCheck& requestIsCurrent )
{
    cpr::Response response;
    for ( int attempt = 1; attempt <= kMaxTransientAttempts; ++attempt )
    {
        aborter.check();
        if ( requestIsCurrent && !requestIsCurrent() )
        {
            throw exception_aborted();
        }
        ThrottleRequest( aborter );
        aborter.check();
        if ( requestIsCurrent && !requestIsCurrent() )
        {
            throw exception_aborted();
        }
        const cpr::ProgressCallback abortProgress{
            [&aborter]( cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, intptr_t ) {
                return !aborter.is_aborting();
            } };
        response = cpr::Get( url, parameters, kConnectTimeout, kRequestTimeout, kJsonRequestHeaders, abortProgress );
        aborter.check();
        // A 429 is handled by the provider request layer's key-scoped cooldown.
        // Retrying it immediately would only consume more requests while the
        // limit applies.
        if ( response.status_code != 0 && response.status_code < 500 )
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

void LogRequest( const cpr::Response& response )
{
    if ( config::advanced::logWebRequests )
    {
        LogDebug( "Request: https://www.theaudiodb.com/api/v1/json/<redacted>/searchalbum.php?<redacted>" );
    }
    if ( config::advanced::logWebResponses )
    {
        LogDebug(
            "Response:\n"
            "  Code: {}\n"
            "  Body:\n"
            "{}\n",
            response.status_code,
            response.text.substr( 0, kMaxLoggedResponseBytes ) );
    }
}

std::optional<qwr::u8string> GetString( const nlohmann::json& object, qwr::u8string_view key )
{
    const auto it = object.find( key );
    if ( it == object.end() || !it->is_string() )
    {
        return std::nullopt;
    }
    auto value = it->get<qwr::u8string>();
    return value.empty() ? std::nullopt : std::optional<qwr::u8string>{ std::move( value ) };
}

std::optional<qwr::u8string> ParseArtworkUrl(
    const qwr::u8string& responseBody,
    const qwr::u8string& artist,
    const qwr::u8string& album )
{
    try
    {
        const auto root = nlohmann::json::parse( responseBody );
        const auto albumsIt = root.find( "album" );
        if ( albumsIt == root.end() || albumsIt->is_null() )
        {
            return std::nullopt;
        }
        if ( !albumsIt->is_array() )
        {
            throw qwr::QwrException( "TheAudioDB response did not contain an album array" );
        }

        std::vector<artwork::TheAudioDbAlbumCandidate> candidates;
        candidates.reserve( albumsIt->size() );
        for ( const auto& item: *albumsIt )
        {
            if ( !item.is_object() )
            {
                continue;
            }
            const auto responseArtist = GetString( item, "strArtist" );
            const auto responseAlbum = GetString( item, "strAlbum" );
            if ( !responseArtist || !responseAlbum )
            {
                continue;
            }
            candidates.push_back( artwork::TheAudioDbAlbumCandidate{
                .artist = qwr::unicode::ToWide( *responseArtist ),
                .album = qwr::unicode::ToWide( *responseAlbum ),
                .hqArtworkUrl = GetString( item, "strAlbumThumbHQ" ).value_or( "" ),
                .artworkUrl = GetString( item, "strAlbumThumb" ).value_or( "" ) } );
        }

        return artwork::SelectTheAudioDbArtworkUrl(
            candidates,
            qwr::unicode::ToWide( artist ),
            qwr::unicode::ToWide( album ) );
    }
    catch ( const nlohmann::json::exception& e )
    {
        throw qwr::QwrException( "Failed to parse TheAudioDB response: {}", e.what() );
    }
}

} // namespace

namespace drp::theaudiodb
{

bool IsRateLimited( const qwr::u8string& configuredApiKey )
{
    auto& state = GetRateLimitState();
    std::scoped_lock lock{ state.mutex };
    const auto it = state.retryAfterByApiKey.find( configuredApiKey );
    if ( it == state.retryAfterByApiKey.end() )
    {
        return false;
    }
    if ( it->second <= std::chrono::steady_clock::now() )
    {
        state.retryAfterByApiKey.erase( it );
        return false;
    }
    return true;
}

std::optional<qwr::u8string> FetchArt(
    const qwr::u8string& artist,
    const qwr::u8string& album,
    const qwr::u8string& configuredApiKey,
    abort_callback& aborter,
    RequestValidityCheck requestIsCurrent )
{
    const qwr::u8string& apiKey = configuredApiKey;
    if ( !artwork::IsEligibleTheAudioDbSupporterKey( apiKey ) )
    {
        throw qwr::QwrException( "TheAudioDB requires a valid user-owned supporter API key" );
    }
    std::unique_lock requestLock{ GetRequestMutex(), std::defer_lock };
    while ( !requestLock.try_lock_for( std::chrono::milliseconds{ 100 } ) )
    {
        aborter.check();
        if ( requestIsCurrent && !requestIsCurrent() )
        {
            throw exception_aborted();
        }
    }
    aborter.check();
    if ( requestIsCurrent && !requestIsCurrent() )
    {
        throw exception_aborted();
    }
    if ( IsRateLimited( apiKey ) )
    {
        throw RateLimitedException( "TheAudioDB rate limit is active; try again after one minute" );
    }

    const auto response = GetWithRetry(
        aborter,
        cpr::Url{ fmt::format( "https://www.theaudiodb.com/api/v1/json/{}/searchalbum.php", apiKey ) },
        cpr::Parameters{ { "s", artist }, { "a", album } },
        requestIsCurrent );
    LogRequest( response );
    if ( response.status_code == 429 )
    {
        MarkRateLimited( apiKey );
        throw RateLimitedException( "TheAudioDB rate limit reached; try again after one minute" );
    }
    if ( response.status_code == 401 || response.status_code == 403 || response.status_code == 404 )
    {
        throw qwr::QwrException( "TheAudioDB rejected the configured API key" );
    }
    if ( response.status_code != 200 )
    {
        throw qwr::QwrException(
            "Failed to fetch TheAudioDB album data\nCode: {}\nError: {}",
            response.status_code,
            response.reason );
    }

    aborter.check();
    return ParseArtworkUrl( response.text, artist, album );
}

} // namespace drp::theaudiodb
