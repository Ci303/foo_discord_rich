#pragma once

#include <string_view>

namespace drp::config
{

inline constexpr char kLegacyDiscordApplicationId[] = "507982587416018945";
inline constexpr char kDefaultDiscordApplicationId[] = "1536157545863847938";

constexpr bool ShouldMigrateLegacyDiscordApplicationId( bool migrationComplete, std::string_view currentApplicationId )
{
    return !migrationComplete && currentApplicationId == kLegacyDiscordApplicationId;
}

} // namespace drp::config
