#include "SteamService.h"

#if defined(SPLITE_ENABLE_STEAM)
#include <steam/steam_api.h>
#endif

namespace splite
{

bool SteamService::Initialize()
{
#if defined(SPLITE_ENABLE_STEAM)
    available_ = SteamAPI_Init();
#else
    available_ = false;
#endif
    return available_;
}

void SteamService::Update()
{
#if defined(SPLITE_ENABLE_STEAM)
    if (available_)
    {
        SteamAPI_RunCallbacks();
    }
#endif
}

void SteamService::Shutdown()
{
#if defined(SPLITE_ENABLE_STEAM)
    if (available_)
    {
        SteamAPI_Shutdown();
    }
#endif
    available_ = false;
}

bool SteamService::IsAvailable() const noexcept
{
    return available_;
}

bool SteamService::UnlockAchievement(const char* achievementId)
{
#if defined(SPLITE_ENABLE_STEAM)
    if (available_ && SteamUserStats())
    {
        return SteamUserStats()->SetAchievement(achievementId) && SteamUserStats()->StoreStats();
    }
#else
    (void)achievementId;
#endif
    return false;
}

} // namespace splite
