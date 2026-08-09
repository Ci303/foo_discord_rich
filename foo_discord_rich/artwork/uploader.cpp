#include <stdafx.h>

#include "uploader.h"

#include <utils/subprocess_executor.h>

#include <component_paths.h>

#include <cpr/cpr.h>
#include <qwr/abort_callback.h>
#include <qwr/final_action.h>
#include <qwr/winapi_error_helpers.h>

namespace
{

std::chrono::seconds kMaxWaitTime{ 10 };
namespace fs = std::filesystem;

}

namespace
{

struct ArtData
{
    album_art_data_ptr pArtData;
    std::optional<qwr::u8string> pathOpt;
};

} // namespace

namespace
{

qwr::u8string CreateTempImageFilePath( qwr::u8string_view extension )
{
    fs::create_directories( drp::path::ImageDir() );
    for ( unsigned attempt = 0; attempt < 100; ++attempt )
    {
        const auto filename = fmt::format(
            "upload-{}-{}-{}.{}",
            GetCurrentProcessId(),
            GetTickCount64(),
            attempt,
            extension );
        const auto path = drp::path::ImageDir() / filename;
        if ( !fs::exists( path ) )
        {
            return path.u8string();
        }
    }
    throw qwr::QwrException( "Failed to allocate a unique temporary artwork path" );
}

std::optional<ArtData> GetArtData( const metadb_handle_ptr& handle, abort_callback& aborter )
{
    if ( handle.is_empty() )
    {
        return std::nullopt;
    }

    const auto pArtMgr = album_art_manager_v3::get();

    const auto handles = pfc::list_single_ref_t<metadb_handle_ptr>( handle );
    const auto artTypeGuid = album_art_ids::cover_front;
    const auto guids = pfc::list_single_ref_t<GUID>( artTypeGuid );

    try
    {
        const auto pArtExtractor = pArtMgr->open_v3( handles, guids, nullptr, aborter );
        if ( !pArtExtractor.is_valid() )
        {
            return std::nullopt;
        }

        const auto pArt = pArtExtractor->query( artTypeGuid, aborter );
        if ( !pArt.is_valid() )
        {
            return std::nullopt;
        }

        const auto pathOpt = [&]() -> std::optional<qwr::u8string> {
            const auto pPathList = pArtExtractor->query_paths( artTypeGuid, aborter );
            if ( !pPathList.is_valid() )
            {
                return std::nullopt;
            }
            if ( !pPathList->get_count() )
            {
                return std::nullopt;
            }

            qwr::u8string path = pPathList->get_path( 0 );
            if ( path == handle->get_location().get_path() )
            { // embedded art
                return std::nullopt;
            }

            if ( constexpr qwr::u8string_view pathPrefix = "file://";
                 path.starts_with( pathPrefix ) )
            {
                path = path.substr( pathPrefix.size() );
            }

            return path;
        }();

        return ArtData{ pArt, pathOpt };
    }
    catch ( const exception_album_art_not_found& /*e*/ )
    {
        return std::nullopt;
    }
}

/// @throw qwr::qwrException
/// @throw exception_aborted
qwr::u8string SaveArtToFile( const album_art_data_ptr& pArtData, abort_callback& aborter )
{
    const auto pApi = fb2k::imageLoaderLite::get();

    const auto mimeOpt = [&]() -> std::optional<qwr::u8string> {
        try
        {
            const auto imageInfo = pApi->getInfo( pArtData->get_ptr(), pArtData->get_size(), aborter );
            if ( !imageInfo.mime )
            {
                return std::nullopt;
            }

            return { imageInfo.mime };
        }
        catch ( const pfc::exception& /*e*/ )
        {
            return std::nullopt;
        }
    }();

    aborter.check();

    const auto fileExtOpt = [&]() -> std::optional<qwr::u8string> {
        if ( !mimeOpt )
        {
            return std::nullopt;
        }
        qwr::u8string_view mime = *mimeOpt;

        static const std::unordered_map<qwr::u8string_view, qwr::u8string_view> mimeToExtension{
            { "image/jpeg", "jpg" },
            { "image/png", "png" },
            { "image/gif", "gif" },
            { "image/webp", "webp" },
            { "image/bmp", "bmp" },
        };
        const auto it = mimeToExtension.find( mime );
        return it == mimeToExtension.end() ? std::nullopt : std::optional<qwr::u8string>{ it->second };
    }();

    const auto imagePath = CreateTempImageFilePath( fileExtOpt.value_or( "jpg" ) );
    try
    {
        service_ptr_t<file> file_ptr;
        filesystem::g_open_write_new( file_ptr, imagePath.c_str(), aborter );
        file_ptr->write( pArtData->get_ptr(), pArtData->get_size(), aborter );
    }
    catch ( const pfc::exception& e )
    {
        throw qwr::QwrException( "Failed to save temporary image file: ", e.what() );
    }

    return imagePath;
}

} // namespace

namespace drp
{

std::optional<qwr::u8string> UploadArt( const metadb_handle_ptr& handle, const qwr::u8string& uploadCommand )
{
    auto& aborter = qwr::GlobalAbortCallback::GetInstance();

    const auto artDataOpt = GetArtData( handle, aborter );
    if ( !artDataOpt )
    {
        return std::nullopt;
    }
    const auto& artData = *artDataOpt;

    aborter.check();

    const auto artPath = [&] {
        if ( artData.pathOpt )
        {
            return *artData.pathOpt;
        }

        return SaveArtToFile( artData.pArtData, aborter );
    }();
    const bool isTemporaryArt = !artData.pathOpt.has_value();
    qwr::final_action removeTemporaryArt( [&] {
        if ( isTemporaryArt )
        {
            std::error_code error;
            fs::remove( fs::u8path( artPath ), error );
            if ( error )
            {
                LogWarning( fmt::format( "Failed to remove temporary artwork: {}", error.message() ) );
            }
        }
    } );

    aborter.check();

    if ( config::advanced::logUploaderCmds )
    {
        LogDebug( "Running configured artwork uploader (local artwork path redacted)" );
    }
    const auto artUrl = [&] {
        SubprocessExecutor uploader{ uploadCommand };

        uploader.Start();
        uploader.WriteData( artPath );
        const auto exitCode = uploader.WaitUntilCompleted( kMaxWaitTime );
        const auto outputOpt = uploader.GetOutput();
        const auto errorOpt = uploader.GetErrorOutput();
        qwr::QwrException::ExpectTrue(
            !exitCode,
            "Uploader failed with error code {}\n"
            "  stdout:\n"
            "{}\n"
            "  stderr:\n"
            "{}\n",
            exitCode,
            outputOpt.value_or( "" ),
            errorOpt.value_or( "" ) );
        qwr::QwrException::ExpectTrue( outputOpt.has_value(), "Uploader process didn't write anything to stdout" );

        return *outputOpt;
    }();
    if ( config::advanced::logUploaderOutput )
    {
        LogDebug( "Artwork uploader returned a URL" );
    }

    return artUrl;
}

} // namespace drp
