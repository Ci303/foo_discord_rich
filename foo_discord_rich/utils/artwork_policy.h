#pragma once

#include <cstdint>

namespace drp::artwork
{

enum class DisplayPolicy : uint8_t
{
    PreferArtwork = 0,
    ApplicationIcon = 1,
    ArtworkOnly = 2
};

constexpr bool IsValidDisplayPolicy( DisplayPolicy policy )
{
    switch ( policy )
    {
    case DisplayPolicy::PreferArtwork:
    case DisplayPolicy::ApplicationIcon:
    case DisplayPolicy::ArtworkOnly:
        return true;
    }

    return false;
}

constexpr DisplayPolicy NormaliseDisplayPolicy( DisplayPolicy policy )
{
    return IsValidDisplayPolicy( policy ) ? policy : DisplayPolicy::PreferArtwork;
}

constexpr bool ShouldResolveArtwork( DisplayPolicy policy )
{
    return NormaliseDisplayPolicy( policy ) != DisplayPolicy::ApplicationIcon;
}

constexpr bool ShouldUseFallbackImage( DisplayPolicy policy )
{
    return NormaliseDisplayPolicy( policy ) != DisplayPolicy::ArtworkOnly;
}

} // namespace drp::artwork
