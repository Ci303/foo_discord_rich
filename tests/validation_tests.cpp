#include <utils/validation.h>

#include <cassert>

int main()
{
    using drp::validation::IsSecureImageUrl;
    using drp::validation::IsCacheEntryFresh;
    using drp::validation::TruncateDiscordText;

    assert( IsSecureImageUrl( "https://example.test/art.jpg" ) );
    assert( !IsSecureImageUrl( "http://example.test/art.jpg" ) );
    assert( !IsSecureImageUrl( "https://example.test/bad url.jpg" ) );
    assert( !IsSecureImageUrl( std::string( "https://" ) + std::string( 247, 'a' ) ) );

    assert( IsCacheEntryFresh( 100, 160, 60 ) );
    assert( !IsCacheEntryFresh( 100, 161, 60 ) );
    assert( !IsCacheEntryFresh( 0, 160, 60 ) );
    assert( !IsCacheEntryFresh( 200, 160, 60 ) );

    assert( TruncateDiscordText( L"x" ) == L"x " );
    assert( TruncateDiscordText( std::wstring( 127, L'x' ) ).size() == 127 );
    assert( TruncateDiscordText( std::wstring( 128, L'x' ) ) == std::wstring( 124, L'x' ) + L"..." );

    std::wstring surrogateBoundary( 123, L'x' );
    surrogateBoundary.push_back( 0xD83D );
    surrogateBoundary.push_back( 0xDE00 );
    surrogateBoundary.append( 10, L'x' );
    const auto truncated = TruncateDiscordText( surrogateBoundary );
    assert( truncated.size() == 126 );
    assert( truncated.substr( truncated.size() - 3 ) == L"..." );
    return 0;
}
