#pragma once

#include <foobar2000/SDK/abort_callback.h>

#include <optional>

namespace drp
{

/// @throw qwr::QwrException
/// @throw exception_aborted
std::optional<qwr::u8string> UploadArt( const metadb_handle_ptr& handle, const qwr::u8string& uploadCommand );

/// @throw qwr::QwrException
/// @throw exception_aborted
std::optional<qwr::u8string> UploadArt(
    const metadb_handle_ptr& handle,
    const qwr::u8string& uploadCommand,
    abort_callback& aborter );

} // namespace drp
