#pragma once

#include <algorithm>
#include <cwctype>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <Windows.h>

#include <utils/validation.h>

namespace drp::artwork
{

inline bool IsValidTheAudioDbApiKey( std::string_view apiKey )
{
    if ( apiKey.empty() || apiKey.size() > 128 )
    {
        return false;
    }

    return std::ranges::all_of( apiKey, []( unsigned char ch ) {
        return ( ch >= '0' && ch <= '9' )
               || ( ch >= 'a' && ch <= 'z' )
               || ( ch >= 'A' && ch <= 'Z' )
               || ch == '-' || ch == '_';
    } );
}

inline bool IsEligibleTheAudioDbSupporterKey( std::string_view apiKey )
{
    return apiKey != "123" && IsValidTheAudioDbApiKey( apiKey );
}

class BoundedTheAudioDbKeySet
{
public:
    explicit BoundedTheAudioDbKeySet( size_t capacity )
        : capacity_( capacity )
    {
    }

    bool Contains( std::string_view apiKey ) const
    {
        return std::ranges::any_of( keys_, [apiKey]( const auto& key ) {
            return key == apiKey;
        } );
    }

    void Insert( std::string apiKey )
    {
        if ( capacity_ == 0 || Contains( apiKey ) )
        {
            return;
        }
        if ( keys_.size() >= capacity_ )
        {
            keys_.erase( keys_.begin() );
        }
        keys_.push_back( std::move( apiKey ) );
    }

    bool Erase( std::string_view apiKey )
    {
        const auto it = std::ranges::find_if( keys_, [apiKey]( const auto& key ) {
            return key == apiKey;
        } );
        if ( it == keys_.end() )
        {
            return false;
        }
        keys_.erase( it );
        return true;
    }

    size_t Size() const
    {
        return keys_.size();
    }

private:
    size_t capacity_ = 0;
    std::vector<std::string> keys_;
};

inline std::string NormaliseTheAudioDbLookupText( std::string value )
{
    const auto first = value.find_first_not_of( " \t\r\n" );
    if ( first == std::string::npos )
    {
        return {};
    }
    const auto last = value.find_last_not_of( " \t\r\n" );
    value = value.substr( first, last - first + 1 );
    for ( auto& ch: value )
    {
        if ( ch >= 'A' && ch <= 'Z' )
        {
            ch = static_cast<char>( ch - 'A' + 'a' );
        }
    }
    return value;
}

inline std::wstring NormaliseTheAudioDbLookupText( std::wstring value )
{
    const auto first = std::find_if_not( value.begin(), value.end(), []( wchar_t ch ) {
        return std::iswspace( ch ) != 0;
    } );
    const auto last = std::find_if_not( value.rbegin(), value.rend(), []( wchar_t ch ) {
                          return std::iswspace( ch ) != 0;
                      } )
                          .base();
    if ( first >= last )
    {
        return {};
    }
    value = std::wstring( first, last );
    if ( value.size() > static_cast<size_t>( ( std::numeric_limits<int>::max )() ) )
    {
        return {};
    }

    const auto sourceLength = static_cast<int>( value.size() );
    const auto requiredLength = NormalizeString( NormalizationC, value.data(), sourceLength, nullptr, 0 );
    if ( requiredLength <= 0 )
    {
        return value;
    }

    std::wstring normalised( static_cast<size_t>( requiredLength ), L'\0' );
    const auto written = NormalizeString( NormalizationC, value.data(), sourceLength, normalised.data(), requiredLength );
    if ( written <= 0 )
    {
        return value;
    }
    normalised.resize( static_cast<size_t>( written ) );
    return normalised;
}

inline bool IsTheAudioDbLookupMatch( std::wstring left, std::wstring right )
{
    left = NormaliseTheAudioDbLookupText( std::move( left ) );
    right = NormaliseTheAudioDbLookupText( std::move( right ) );
    if ( left.empty() || right.empty()
         || left.size() > static_cast<size_t>( ( std::numeric_limits<int>::max )() )
         || right.size() > static_cast<size_t>( ( std::numeric_limits<int>::max )() ) )
    {
        return false;
    }

    return CompareStringOrdinal(
               left.data(),
               static_cast<int>( left.size() ),
               right.data(),
               static_cast<int>( right.size() ),
               TRUE )
           == CSTR_EQUAL;
}

struct TheAudioDbAlbumCandidate
{
    std::wstring artist;
    std::wstring album;
    std::string hqArtworkUrl;
    std::string artworkUrl;
};

inline std::optional<std::string> SelectTheAudioDbArtworkUrl(
    const std::vector<TheAudioDbAlbumCandidate>& candidates,
    const std::wstring& expectedArtist,
    const std::wstring& expectedAlbum )
{
    for ( const auto& candidate: candidates )
    {
        if ( !IsTheAudioDbLookupMatch( candidate.artist, expectedArtist )
             || !IsTheAudioDbLookupMatch( candidate.album, expectedAlbum ) )
        {
            continue;
        }
        if ( validation::IsSecureImageUrl( candidate.hqArtworkUrl ) )
        {
            return candidate.hqArtworkUrl;
        }
        if ( validation::IsSecureImageUrl( candidate.artworkUrl ) )
        {
            return candidate.artworkUrl;
        }
    }
    return std::nullopt;
}

} // namespace drp::artwork
