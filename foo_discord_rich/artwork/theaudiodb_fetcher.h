#pragma once

#include <functional>
#include <optional>
#include <stdexcept>

#include <foobar2000/SDK/abort_callback.h>

namespace drp::theaudiodb
{

class RateLimitedException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class AuthenticationRejectedException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

bool IsRateLimited( const qwr::u8string& configuredApiKey );
bool IsApiKeyRejected( const qwr::u8string& configuredApiKey );

/// Clears process-local authentication-rejection state for one key. Call this
/// immediately before an explicit user-requested retest. Rate limits remain in
/// force and cannot be bypassed by retesting.
void ResetRejectedApiKeyState( const qwr::u8string& configuredApiKey );

using RequestValidityCheck = std::function<bool()>;

/// @throw qwr::QwrException
/// @throw RateLimitedException
/// @throw AuthenticationRejectedException
/// @throw exception_aborted
std::optional<qwr::u8string> FetchArt(
    const qwr::u8string& artist,
    const qwr::u8string& album,
    const qwr::u8string& configuredApiKey,
    abort_callback& aborter,
    RequestValidityCheck requestIsCurrent = {} );

} // namespace drp::theaudiodb
