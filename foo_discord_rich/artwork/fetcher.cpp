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

namespace fs = std::filesystem;

namespace
{

using ArtPinIdToArtUrl = std::unordered_map<qwr::u8string, std::optional<qwr::u8string>>;

const std::chrono::seconds kRequestProcessingDelay{ 2 };
constexpr std::uintmax_t kMaxCacheFileBytes = 4 * 1024 * 1024;
constexpr size_t kMaxArtPinIdBytes = 1024;

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

bool HasCachedArt( const ArtPinIdToArtUrl& cache, const qwr::u8string& artPinId )
{
    return cache.find( artPinId ) != cache.end();
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
        if ( qwr::unicode::ToWide( imageKey ).length() <= 254 )
        {
            return false;
        }

        drp::LogError( fmt::format( "Failed to process art URL `{}`:\nlength is bigger than 254", imageKey ) );
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

        const auto cacheIt = artPinIdToArtUrl_.find( *artPinIdOpt );
        if ( cacheIt != artPinIdToArtUrl_.end() )
        {
            return cacheIt->second;
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
        const auto cachePath = GetCacheFilePath();
        if ( !fs::exists( cachePath ) )
        {
            return;
        }
        if ( fs::file_size( cachePath ) > kMaxCacheFileBytes )
        {
            throw qwr::QwrException( "Art cache is too large to load safely: {}", cachePath.u8string() );
        }

        const auto content = qwr::file::ReadFile( cachePath, CP_UTF8 );

        decltype( artPinIdToArtUrl_ ) loadedCache;
        json::parse( content ).get_to( loadedCache );

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

        const auto content = json( cacheCopy ).dump( 2 );
        qwr::file::WriteFile( cachePath, content, false );
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
    }
    catch ( const fs::filesystem_error& e )
    {
        LogError( fmt::format( "Failed to clear cache: {}", e.what() ) );
    }
}

std::filesystem::path ArtworkFetcher::GetCacheFilePath()
{
    static const auto cachePath = drp::path::ImageDir() / "art_urls.v2.0.1.json";
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
                    if ( HasCachedArt( artPinIdToArtUrl_, *artPinIdOpt ) )
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

        auto artUrlOpt = std::visit( [&]( const auto& arg ) { return ProcessFetchRequest( arg ); }, *lastRequest );
        if ( !artUrlOpt && ( qwr::GlobalAbortCallback::GetInstance().is_aborting() || token.stop_requested() ) )
        { // do not save nullopt if interrupted, because it might actually had the image
            return;
        }

        if ( artUrlOpt && IsDiscordImageKeyInvalid( *artUrlOpt ) )
        {
            artUrlOpt.reset();
        }

        const auto artPinIdOpt = GenerateArtPinId( *lastRequest );
        if ( !artPinIdOpt )
        {
            lastRequest.reset();
            continue;
        }
        {
            std::unique_lock lock( mutex_ );

            artPinIdToArtUrl_.try_emplace( *artPinIdOpt, artUrlOpt );

            if ( lastRequest == currentRequestOpt_ )
            {
                currentRequestOpt_.reset();

                if ( artUrlOpt && !token.stop_requested() && !qwr::GlobalAbortCallback::GetInstance().is_aborting() )
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

std::optional<qwr::u8string> ArtworkFetcher::ProcessFetchRequest( const MusicBrainzFetchRequest& request )
{
    try
    {
        return musicbrainz::FetchArt( request.artist, request.album, request.userReleaseMbidOpt );
    }
    catch ( const qwr::QwrException& e )
    {
        LogError( e.what() );
        return std::nullopt;
    }
    catch ( const exception_aborted& /*e*/ )
    {
        return std::nullopt;
    }
    catch ( const std::exception& e )
    {
        LogError( fmt::format( "Unexpected MusicBrainz art fetch failure: {}", e.what() ) );
        return std::nullopt;
    }
    catch ( ... )
    {
        LogError( "Unexpected MusicBrainz art fetch failure" );
        return std::nullopt;
    }
}

std::optional<qwr::u8string> ArtworkFetcher::ProcessFetchRequest( const UploadRequest& request )
{
    try
    {
        return UploadArt( request.handle, request.uploadCommand );
    }
    catch ( const qwr::QwrException& e )
    {
        LogError( e.what() );
        return std::nullopt;
    }
    catch ( const exception_aborted& /*e*/ )
    {
        return std::nullopt;
    }
    catch ( const pfc::exception& e )
    {
        LogError( fmt::format( "Art upload failed: {}", e.what() ) );
        return std::nullopt;
    }
    catch ( ... )
    {
        LogError( "Unexpected art upload failure" );
        return std::nullopt;
    }
}

} // namespace drp
