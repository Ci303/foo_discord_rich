#include "stdafx.h"

#include <artwork/fetcher.h>
#include <discord/discord_integration.h>
#include <fb2k/config.h>

#include <qwr/abort_callback.h>

DECLARE_COMPONENT_VERSION(
    DRP_NAME,
    DRP_VERSION,
    DRP_NAME_WITH_VERSION " by Ci303, forked from the original component by TheQwertiest" );

VALIDATE_COMPONENT_FILENAME( DRP_DLL_NAME );

namespace
{

class ComponentInitQuit : public initquit
{
public:
    void on_init() override
    {
        drp::config::MigrateLegacyDiscordApplicationId();
        drp::config::SanitiseArtworkDisplayPolicy();
        drp::ArtworkFetcher::Get().Initialize();
        drp::DiscordAdapter::GetInstance().Initialize();
    }

    void on_quit() override
    {
        qwr::GlobalAbortCallback::GetInstance().Abort();
        drp::ArtworkFetcher::Get().Finalize();
        drp::DiscordAdapter::GetInstance().Finalize();
    }
};

initquit_factory_t<ComponentInitQuit> g_initquit;

} // namespace
