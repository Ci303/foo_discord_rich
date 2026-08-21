#include <stdafx.h>

#include "credential_store.h"

#include <WinCred.h>
#include <qwr/final_action.h>

#pragma comment( lib, "Advapi32.lib" )

namespace
{

constexpr wchar_t kTheAudioDbCredentialTarget[] = L"Ci303/foo_discord_rich/TheAudioDB";
constexpr wchar_t kTheAudioDbCredentialUser[] = L"TheAudioDB supporter API key";

[[noreturn]] void ThrowCredentialError( qwr::u8string_view operation, DWORD error )
{
    throw qwr::QwrException( "Windows Credential Manager could not {} the TheAudioDB API key (error {})", operation, error );
}

} // namespace

namespace drp::credentials
{

std::optional<qwr::u8string> ReadTheAudioDbApiKey()
{
    PCREDENTIALW credential = nullptr;
    if ( !CredReadW( kTheAudioDbCredentialTarget, CRED_TYPE_GENERIC, 0, &credential ) )
    {
        const auto error = GetLastError();
        if ( error == ERROR_NOT_FOUND )
        {
            return std::nullopt;
        }
        ThrowCredentialError( "read", error );
    }

    const auto freeCredential = qwr::final_action( [&] {
        if ( credential )
        {
            SecureZeroMemory( credential->CredentialBlob, credential->CredentialBlobSize );
            CredFree( credential );
        }
    } );

    if ( credential->CredentialBlobSize == 0 )
    {
        return std::nullopt;
    }
    if ( credential->CredentialBlobSize > CRED_MAX_CREDENTIAL_BLOB_SIZE )
    {
        throw qwr::QwrException( "The stored TheAudioDB credential is too large" );
    }

    return qwr::u8string{
        reinterpret_cast<const char*>( credential->CredentialBlob ),
        credential->CredentialBlobSize };
}

void WriteTheAudioDbApiKey( qwr::u8string_view apiKey )
{
    if ( apiKey.empty() || apiKey.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE )
    {
        throw qwr::QwrException( "The TheAudioDB API key cannot be stored because its size is invalid" );
    }

    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>( kTheAudioDbCredentialTarget );
    credential.CredentialBlobSize = static_cast<DWORD>( apiKey.size() );
    credential.CredentialBlob = reinterpret_cast<LPBYTE>( const_cast<char*>( apiKey.data() ) );
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = const_cast<LPWSTR>( kTheAudioDbCredentialUser );

    if ( !CredWriteW( &credential, 0 ) )
    {
        ThrowCredentialError( "store", GetLastError() );
    }
}

bool ClearTheAudioDbApiKey()
{
    if ( CredDeleteW( kTheAudioDbCredentialTarget, CRED_TYPE_GENERIC, 0 ) )
    {
        return true;
    }

    const auto error = GetLastError();
    if ( error == ERROR_NOT_FOUND )
    {
        return false;
    }
    ThrowCredentialError( "clear", error );
}

} // namespace drp::credentials
