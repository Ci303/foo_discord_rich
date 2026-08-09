#include <stdafx.h>

#include "fetcher.h"

#include <artwork/musicbrainz_fetcher.h>
#include <artwork/uploader.h>
#include <discord/discord_integration.h>

#include <component_paths.h>

#include <cpr/cpr.h>
#include <qwr/abort_callback.h>
#include <qwr/algorithm.h>
#include <qwr/file_helpers.h>
#include <qwr/thread_name_setter.h>
#include <qwr/visitor.h>
#include <utils/validation.h>

namespace fs = std::filesystem;

namespace
{

const std::chrono::seconds kRequestProcessingDelay{ 2 };
const std::chrono::seconds kPositiveCacheLifetime{ std::chrono::hours{ 24 * 30 } };
const std::chrono::seconds kNegativeCacheLifetime{ std::chrono::hours{ 6 } };
constexpr std::uintmax_t kMaxCacheFileBytes = 4 * 1024 * 1024;
constexpr size_t kMaxArtPinIdBytes = 1024;
constexpr size_t kMaxCacheEntries = 2048;

}

namespace
{

qwr::u8string GenerateMusicBrainzArtPinId( const qwr::u8string& artist, const qwr::u8string& album )
{
    return artist + "|" + album;
}

std::optional<qwr::u8string> GenerateArtPinId( const drp::ArtworkFetcher::FetchRequest& request )
{
    const auto artPinId = std::visit(
        qwr::Visitor{
            []( const drp::ArtworkFetcher::MusicBrainzFetchRequest& req ) {
                const auto& [artist, album, userReleaseMbidOpt] = req;
                if ( artist.empty() || album.empty() )
                {
                    return qwr::u8string{};
                }
                return GenerateMusicBrainzArtPinId( req.artist, req.album );
            },
            []( const drp::ArtworkFetcher::UploadRequest& req ) {
                return req.artPinId;
            } },
        request );
    if ( artPinId.empty() )
    {
        return std::nullopt;
    }
    if ( artPinId.size() > kMaxArtPinIdBytes )
    {
        drp::LogWarning( "Skipping album art request because the art cache key is too large" );
        return std::nullopt;
    }

    return artPinId;
}

int64_t CurrentUnixTime()
{
    const auto now = std::time( nullptr );
    return now < 0 ? 0 : static_cast<int64_t>( now );
}

bool IsRequestExecutable( const drp::ArtworkFetcher::FetchRequest& request )
{
    return std::visit(
        qwr::Visitor{
            []( const drp::ArtworkFetcher::MusicBrainzFetchRequest& req ) {
                return true;
            },
            []( const drp::ArtworkFetcher::UploadRequest& req ) {
                return !req.uploadCommand.empty();
            } },
        request );
}

bool IsDiscordImageKeyInvalid( const qwr::u8string& imageKey )
{
    try
    {
        qwr::unicode::ToWide( imageKey );
        if ( drp::validation::IsSecureImageUrl( imageKey ) )
        {
            return false;
        }

        drp::LogError( "Failed to process art URL: expected an HTTPS URL without whitespace, at most 254 bytes long" );
        return true;
    }
    catch ( const std::exception& e )
    {
        drp::LogError( fmt::format( "Failed to process art URL: invalid UTF-8 ({})", e.what() ) );
        return true;
    }
}

} // namespace

namespace drp
{

drp::ArtworkFetcher& ArtworkFetcher::Get()
{
    static ArtworkFetcher instance;
    return instance;
}

void ArtworkFetcher::Initialize()
{
    LoadCache();
    StartThread();
}

void ArtworkFetcher::Finalize()
{
    StopThread();
    SaveCache();
}

std::optional<qwr::u8string> ArtworkFetcher::GetArtUrl( const FetchRequest& request )
{
    const auto artPinIdOpt = GenerateArtPinId( request );
    if ( !artPinIdOpt )
    {
        return std::nullopt;
    }

{
    std::unique_lock lock( mutex_ );

        auto cacheIt = artPinIdToArtUrl_.find( *artPinIdOpt );
        if ( cacheIt != artPinIdToArtUrl_.end() )
        {
            const auto lifetime = cacheIt->second.artUrl ? kPositiveCacheLifetime : kNegativeCacheLifetime;
            if ( drp::validation::IsCacheEntryFresh( cacheIt->second.fetchedAt, CurrentUnixTime(), lifetime.count() ) )
            {
                return cacheIt->second.artUrl;
            }
            artPinIdToArtUrl_.erase( cacheIt );
        }

        if ( !IsRequestExecutable( request ) )
        {
            return std::nullopt;
        }

        if ( !currentRequestOpt_ || *currentRequestOpt_ != request )
        {
            currentRequestOpt_ = request;
            cv_.notify_all();
        }
    }

    return std::nullopt;
}

void ArtworkFetcher::LoadCache( bool throwOnError )
{
    using json = nlohmann::json;

    try
    {
        auto cachePath = GetCacheFilePath();
        if ( !fs::exists( cachePath ) )
        {
            const auto legacyCachePath = drp::path::ImageDir() / "art_urls.v2.0.1.json";
            if ( !fs::exists( legacyCachePath ) )
            {
                return;
            }
            cachePath = legacyCachePath;
        }
        if ( fs::file_size( cachePath ) > kMaxCacheFileBytes )
        {
            throw qwr::QwrException( "Art cache is too large to load safely: {}", cachePath.u8string() );
        }

        const auto content = qwr::file::ReadFile( cachePath, CP_UTF8 );

        decltype( artPinIdToArtUrl_ ) loadedCache;
        const auto root = json::parse( content );
        const auto& entries = root.contains( "entries" ) ? root.at( "entries" ) : root;
        for ( const auto& [key, value]: entries.items() )
        {
            if ( key.size() > kMaxArtPinIdBytes || loadedCache.size() >= kMaxCacheEntries )
            {
                continue;
            }
            CacheEntry entry;
            if ( value.is_object() )
            {
                entry.artUrl = value.at( "url" ).get<std::optional<qwr::u8string>>();
                entry.fetchedAt = value.at( "fetched_at" ).get<int64_t>();
            }
            else
            {
                entry.artUrl = value.get<std::optional<qwr::u8string>>();
                entry.fetchedAt = 0;
            }
            loadedCache.emplace( key, std::move( entry ) );
        }

        {
            std::unique_lock lock( mutex_ );
            artPinIdToArtUrl_ = std::move( loadedCache );
        }
    }
    catch ( const qwr::QwrException& e )
    {
        LogError( fmt::format( "Failed to load cache: {}", e.what() ) );
        if ( throwOnError )
        {
            throw;
        }
    }
    catch ( const json::exception& e )
    {
        LogError( fmt::format( "Failed to load cache: {}", e.what() ) );
        if ( throwOnError )
        {
            throw;
        }
    }
    catch ( const fs::filesystem_error& e )
    {
        LogError( fmt::format( "Failed to load cache: {}", e.what() ) );
        if ( throwOnError )
        {
            throw;
        }
    }
    catch ( const std::exception& e )
    {
        LogError( fmt::format( "Failed to load cache: {}", e.what() ) );
        if ( throwOnError )
        {
            throw;
        }
    }
}

void ArtworkFetcher::SaveCache()
{
    using json = nlohmann::json;

    try
    {
        const auto cachePath = GetCacheFilePath();
        fs::create_directories( cachePath.parent_path() );

        decltype( artPinIdToArtUrl_ ) cacheCopy;
        {
            std::unique_lock lock( mutex_ );
            cacheCopy = artPinIdToArtUrl_;
        }

        json entries = json::object();
        for ( const auto& [key, entry]: cacheCopy )
        {
            entries[key] = json{ { "url", entry.artUrl }, { "fetched_at", entry.fetchedAt } };
        }
        const auto content = json{ { "version", 3 }, { "entries", std::move( entries ) } }.dump( 2 );
        const auto temporaryPath = fs::u8path( cachePath.u8string() + ".tmp" );
        qwr::file::WriteFile( temporaryPath, content, false );
        if ( !MoveFileExW( temporaryPath.c_str(), cachePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) )
        {
            fs::remove( temporaryPath );
            throw fs::filesystem_error( "Failed to atomically replace art cache", cachePath, std::error_code( GetLastError(), std::system_category() ) );
        }
    }
    catch ( const qwr::QwrException& e )
    {
        LogError( fmt::format( "Failed to save cache: {}", e.what() ) );
    }
    catch ( const json::exception& e )
    {
        LogError( fmt::format( "Failed to save cache: {}", e.what() ) );
    }
    catch ( const fs::filesystem_error& e )
    {
        LogError( fmt::format( "Failed to save cache: {}", e.what() ) );
    }
    catch ( const std::exception& e )
    {
        LogError( fmt::format( "Failed to save cache: {}", e.what() ) );
    }
}

void ArtworkFetcher::ClearCache()
{
    {
        std::unique_lock lock( mutex_ );
        artPinIdToArtUrl_.clear();
        currentRequestOpt_.reset();
    }

    try
    {
        const auto cachePath = GetCacheFilePath();
        if ( fs::exists( cachePath ) )
        {
            fs::remove( cachePath );
        }
        const auto legacyCachePath = drp::path::ImageDir() / "art_urls.v2.0.1.json";
        if ( fs::exists( legacyCachePath ) )
        {
            fs::remove( legacyCachePath );
        }
    }
    catch ( const fs::filesystem_error& e )
    {
        LogError( fmt::format( "Failed to clear cache: {}", e.what() ) );
    }
}

std::filesystem::path ArtworkFetcher::GetCacheFilePath()
{
    static const auto cachePath = drp::path::ImageDir() / "art_urls.v3.json";
    return cachePath;
}

void ArtworkFetcher::StartThread()
{
    pThread_ = std::make_unique<std::jthread>( [this]( std::stop_token token ) {
        try
        {
            ThreadMain( token );
        }
        catch ( const std::exception& e )
        {
            LogError( fmt::format( "Artwork worker stopped unexpectedly: {}", e.what() ) );
        }
        catch ( ... )
        {
            LogError( "Artwork worker stopped unexpectedly" );
        }
    } );
    qwr::SetThreadName( *pThread_, "DRP ArtFetcher" );
}

void ArtworkFetcher::StopThread()
{
    if ( pThread_ )
    {
        pThread_->request_stop();
        cv_.notify_all();
        pThread_.reset();
    }
}

void ArtworkFetcher::ThreadMain( std::stop_token token )
{
    std::optional<FetchRequest> lastRequest;

    while ( !token.stop_requested() )
    {
        {
            std::unique_lock lock( mutex_ );
            cv_.wait_for( lock, kRequestProcessingDelay, [&] {
                return token.stop_requested() || lastRequest != currentRequestOpt_;
            } );

            if ( currentRequestOpt_ )
            {
                if ( const auto artPinIdOpt = GenerateArtPinId( *currentRequestOpt_ );
                     artPinIdOpt )
                {
                    if ( artPinIdToArtUrl_.contains( *artPinIdOpt ) )
                    {
                        currentRequestOpt_.reset();
                        lastRequest.reset();
                        continue;
                    }
                }
            }

            if ( lastRequest != currentRequestOpt_ )
            { // user requested new art, wait again
                lastRequest = currentRequestOpt_;
                continue;
            }

            if ( !lastRequest )
            {
                continue;
            }
        }

        auto outcome = std::visit( [&]( const auto& arg ) { return ProcessFetchRequest( arg ); }, *lastRequest );
        if ( !outcome.artUrl && ( qwr::GlobalAbortCallback::GetInstance().is_aborting() || token.stop_requested() ) )
        { // do not save nullopt if interrupted, because it might actually had the image
            return;
        }

        if ( outcome.artUrl && IsDiscordImageKeyInvalid( *outcome.artUrl ) )
        {
            outcome.artUrl.reset();
            outcome.cacheable = false;
        }

        const auto artPinIdOpt = GenerateArtPinId( *lastRequest );
        if ( !artPinIdOpt )
        {
            lastRequest.reset();
            continue;
        }
        {
            std::unique_lock lock( mutex_ );

            if ( outcome.cacheable )
            {
                if ( artPinIdToArtUrl_.size() >= kMaxCacheEntries )
                {
                    const auto oldest = std::min_element( artPinIdToArtUrl_.begin(), artPinIdToArtUrl_.end(), []( const auto& left, const auto& right ) {
                        return left.second.fetchedAt < right.second.fetchedAt;
                    } );
                    if ( oldest != artPinIdToArtUrl_.end() )
                    {
                        artPinIdToArtUrl_.erase( oldest );
                    }
                }
                artPinIdToArtUrl_.insert_or_assign( *artPinIdOpt, CacheEntry{ outcome.artUrl, CurrentUnixTime() } );
            }

            if ( lastRequest == currentRequestOpt_ )
            {
                currentRequestOpt_.reset();

                if ( outcome.artUrl && !token.stop_requested() && !qwr::GlobalAbortCallback::GetInstance().is_aborting() )
                {
                    fb2k::inMainThread( [] {
                        if ( qwr::GlobalAbortCallback::GetInstance().is_aborting() )
                        {
                            return;
                        }

                        auto pm = DiscordAdapter::GetInstance().GetPresenceModifier();
                        pm.UpdateImage();
                    } );
                }
            }
            lastRequest.reset();
        }
    }
}

ArtworkFetcher::FetchOutcome ArtworkFetcher::ProcessFetchRequest( const MusicBrainzFetchRequest& request )
{
    try
    {
        return { musicbrainz::FetchArt( request.artist, request.album, request.userReleaseMbidOpt ), true };
    }
    catch ( const qwr::QwrException& e )
    {
        LogError( e.what() );
        return {};
    }
    catch ( const exception_aborted& /*e*/ )
    {
        return {};
    }
    catch ( const std::exception& e )
    {
        LogError( fmt::format( "Unexpected MusicBrainz art fetch failure: {}", e.what() ) );
        return {};
    }
    catch ( ... )
    {
        LogError( "Unexpected MusicBrainz art fetch failure" );
        return {};
    }
}

ArtworkFetcher::FetchOutcome ArtworkFetcher::ProcessFetchRequest( const UploadRequest& request )
{
    try
    {
        return { UploadArt( request.handle, request.uploadCommand ), true };
    }
    catch ( const qwr::QwrException& e )
    {
        LogError( e.what() );
        return {};
    }
    catch ( const exception_aborted& /*e*/ )
    {
        return {};
    }
    catch ( const pfc::exception& e )
    {
        LogError( fmt::format( "Art upload failed: {}", e.what() ) );
        return {};
    }
    catch ( ... )
    {
        LogError( "Unexpected art upload failure" );
        return {};
    }
}

} // namespace drp
