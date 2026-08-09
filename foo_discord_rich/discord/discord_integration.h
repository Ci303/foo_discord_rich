#pragma once

#include <discord/presence_data.h>

namespace drp
{

class DiscordAdapter
{
    friend class drp::PresenceModifier;

public:
    static DiscordAdapter& GetInstance();

    void Initialize();
    void Finalize();
    void OnSettingsChanged();

    drp::PresenceModifier GetPresenceModifier();

private:
    bool HasPresence() const;
    void SendPresence();
    void ClearPresence();
    void ScheduleCallbackPump();
    void PumpCallbacks( uint64_t generation );

private:
    DiscordAdapter() = default;

    static void OnReady( const DiscordUser* request );
    static void OnDisconnected( int errorCode, const char* message );
    static void OnErrored( int errorCode, const char* message );

private:
    bool isInitialized_ = false;
    bool isConnected_ = false;
    bool hasPresence_ = false;
    uint64_t callbackGeneration_ = 0;
    qwr::u8string appToken_;
    drp::internal::PresenceData presenceData_;
};

} // namespace drp
