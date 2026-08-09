#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace drp::validation
{

inline std::wstring TruncateDiscordText( std::wstring text )
{
    if ( text.size() == 1 )
    {
        text += L' ';
    }
    else if ( text.size() > 127 )
    {
        size_t end = 124;
        if ( end && end < text.size()
             && text[end - 1] >= 0xD800 && text[end - 1] <= 0xDBFF
             && text[end] >= 0xDC00 && text[end] <= 0xDFFF )
        {
            --end;
        }
        text.resize( end );
        text += L"...";
    }
    return text;
}

inline bool IsSecureImageUrl( std::string_view value )
{
    if ( !value.starts_with( "https://" ) || value.size() > 254 )
    {
        return false;
    }
    return std::none_of( value.begin(), value.end(), []( unsigned char ch ) {
        return ch <= 0x20 || ch == 0x7f;
    } );
}

inline bool IsCacheEntryFresh( int64_t fetchedAt, int64_t now, int64_t lifetimeSeconds )
{
    if ( fetchedAt <= 0 || now < fetchedAt || lifetimeSeconds < 0 )
    {
        return false;
    }
    return now - fetchedAt <= lifetimeSeconds;
}

} // namespace drp::validation
