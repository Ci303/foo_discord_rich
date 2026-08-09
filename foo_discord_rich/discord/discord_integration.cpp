#include <stdafx.h>

#include "discord_integration.h"

#include <fb2k/config.h>

namespace drp
{

DiscordAdapter& DiscordAdapter::GetInstance()
{
    static DiscordAdapter discordHandler;
    return discordHandler;
}

void DiscordAdapter::Initialize()
{
    // migrate old settings
    if ( config::bottomTextQuery_v1_deprecated.GetValue() != config::bottomTextQuery_v1_deprecated.GetDefaultValue() )
    {
        config::bottomTextQuery = config::bottomTextQuery_v1_deprecated.GetValue();
        config::middleTextQuery = "";
        config::bottomTextQuery_v1_deprecated = config::bottomTextQuery_v1_deprecated.GetDefaultValue();
    }
    appToken_ = config::discordAppToken;
    if ( appToken_.empty() )
    {
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": app token is empty, Discord Rich Presence is disabled";
        isInitialized_ = false;
        hasPresence_ = false;
        return;
    }

    DiscordEventHandlers handlers{};

    handlers.ready = OnReady;
    handlers.disconnected = OnDisconnected;
    handlers.errored = OnErrored;

    Discord_Initialize( appToken_.c_str(), &handlers, 1, nullptr );
    isInitialized_ = true;
    isConnected_ = false;
    hasPresence_ = true; ///< Discord may use default app handler, which we need to override
    ++callbackGeneration_;
    Discord_RunCallbacks();
    ScheduleCallbackPump();

    auto pm = GetPresenceModifier();
    pm.UpdateImage();
    pm.Disable(); ///< we don't want to activate presence yet
}

void DiscordAdapter::Finalize()
{
    if ( !isInitialized_ )
    {
        hasPresence_ = false;
        return;
    }

    Discord_ClearPresence();
    Discord_Shutdown();
    hasPresence_ = false;
    isConnected_ = false;
    isInitialized_ = false;
    ++callbackGeneration_;
}

void DiscordAdapter::OnSettingsChanged()
{
    if ( appToken_ != static_cast<std::string>( config::discordAppToken ) )
    {
        Finalize();
        Initialize();
    }

    auto pm = GetPresenceModifier();
    pm.UpdateImage();
    pm.UpdateSmallImage();
    pm.UpdateTrack();
    if ( !config::isEnabled )
    {
        pm.Disable();
    }
}

bool DiscordAdapter::HasPresence() const
{
    return hasPresence_;
}

void DiscordAdapter::SendPresence()
{
    if ( !isInitialized_ )
    {
        return;
    }

    if ( config::isEnabled )
    {
        Discord_UpdatePresence( &presenceData_.presence );
        hasPresence_ = true;
    }
    else
    {
        Discord_ClearPresence();
        hasPresence_ = false;
    }
    Discord_RunCallbacks();
}

void DiscordAdapter::ClearPresence()
{
    if ( !isInitialized_ )
    {
        hasPresence_ = false;
        return;
    }

    Discord_ClearPresence();
    hasPresence_ = false;

    Discord_RunCallbacks();
}

void DiscordAdapter::ScheduleCallbackPump()
{
    const auto generation = callbackGeneration_;
    fb2k::callLater( 5.0, [generation] {
        DiscordAdapter::GetInstance().PumpCallbacks( generation );
    } );
}

void DiscordAdapter::PumpCallbacks( uint64_t generation )
{
    if ( !isInitialized_ || generation != callbackGeneration_ )
    {
        return;
    }

    Discord_RunCallbacks();
    ScheduleCallbackPump();
}

PresenceModifier DiscordAdapter::GetPresenceModifier()
{
    return PresenceModifier( *this, presenceData_ );
}

void DiscordAdapter::OnReady( const DiscordUser* request )
{
    GetInstance().isConnected_ = true;
    FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": connected to " << ( request && request->username ? request->username : "<null>" );
    fb2k::inMainThread( [] {
        auto& adapter = GetInstance();
        if ( adapter.isInitialized_ && config::isEnabled && playback_control::get()->is_playing() )
        {
            adapter.SendPresence();
        }
    } );
}

void DiscordAdapter::OnDisconnected( int errorCode, const char* message )
{
    GetInstance().isConnected_ = false;
    FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": disconnected with code " << errorCode;
    if ( message )
    {
        FB2K_console_formatter() << message;
    }
}

void DiscordAdapter::OnErrored( int errorCode, const char* message )
{
    GetInstance().isConnected_ = false;
    FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": error " << errorCode;
    if ( message )
    {
        FB2K_console_formatter() << message;
    }
}

} // namespace drp
