/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.
//
// The errand registry behind MaintenanceErrandPolicy.h: one claimed destination per bot, refreshed
// by the maintenance action on every tick it travels and released when the errand ends. World
// movement (NewRpgBaseAction::MoveFarTo and MoveRandomNear) consults it before steering.

#ifndef PLAYERBOTS_MAINTENANCEERRAND_H
#define PLAYERBOTS_MAINTENANCEERRAND_H

#include "MaintenanceErrandPolicy.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "Timer.h"
#include "TravelMgr.h"

#include <mutex>
#include <unordered_map>

namespace playerbots::maintenance
{
struct ErrandClaim
{
    WorldPosition destination;
    std::uint32_t claimedMs = 0;
};

inline std::mutex errandMutex;
inline std::unordered_map<ObjectGuid::LowType, ErrandClaim> errandClaims;

inline void ClaimErrand(Player* bot, WorldPosition const& destination)
{
    std::lock_guard<std::mutex> lock(errandMutex);
    errandClaims[bot->GetGUID().GetCounter()] = {destination, getMSTime()};
}

inline void ReleaseErrand(Player* bot)
{
    std::lock_guard<std::mutex> lock(errandMutex);
    errandClaims.erase(bot->GetGUID().GetCounter());
}

// Whether a movement request toward `destination` must yield to the bot's claimed errand.
[[nodiscard]] inline bool ErrandBlocksMove(Player* bot, WorldPosition const& destination)
{
    std::lock_guard<std::mutex> lock(errandMutex);
    auto const it = errandClaims.find(bot->GetGUID().GetCounter());
    if (it == errandClaims.end())
        return false;
    return ErrandBlocksOtherMove(true, GetMSTimeDiffToNow(it->second.claimedMs),
                                 it->second.destination == destination);
}

// Whether the bot has a live errand at all (a random wander has no destination to compare).
[[nodiscard]] inline bool ErrandActive(Player* bot)
{
    std::lock_guard<std::mutex> lock(errandMutex);
    auto const it = errandClaims.find(bot->GetGUID().GetCounter());
    return it != errandClaims.end() && ErrandBlocksOtherMove(true, GetMSTimeDiffToNow(it->second.claimedMs), false);
}
}  // namespace playerbots::maintenance

#endif
