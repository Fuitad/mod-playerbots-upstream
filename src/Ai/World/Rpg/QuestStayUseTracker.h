/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE (quest-stay-use-tracker). Not present upstream, so it can never conflict on a
// merge. Load-bearing, unlike the QuestStayKillProbe diagnostics: it counts the objective
// interactions a bot dispatched during the current POI stay (tool used on a creature, quest
// gameobject queued for loot or operated), which QuestStayEndDecision consumes to distinguish
// "this place cannot progress the quest" from "the bot tried and lost the race for a creditable
// target". Kept outside NewRpgInfo::DoQuest deliberately: that struct lives in NewRpgInfo.h,
// which PlayerbotAI.h includes, so growing it rebuilds every module translation unit.

#ifndef _PLAYERBOT_QUESTSTAYUSETRACKER_H
#define _PLAYERBOT_QUESTSTAYUSETRACKER_H

#include "Player.h"

#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace QuestStayUseTracker
{
// Bots tick on the map update workers, so different bots reach this map from different threads
// and even distinct entries share the container's rehashes: same locking as QuestStayKillProbe.
inline std::mutex trackerMutex;
inline std::unordered_map<ObjectGuid::LowType, uint32> attemptsThisStay;
// Stay ticks on which a seek returned a usable objective candidate, converged or not. Feeds the
// candidateSightings parameter of QuestStayEndDecision (the contention class).
inline std::unordered_map<ObjectGuid::LowType, uint32> sightingsThisStay;
// Creatures the bot has already used its tool on during this stay, per bot. Credit scripts that
// fire No Repeat per creature (Cleansing the Scar's Eversong Rangers) never credit a second cast
// on the same one, so the seek has to move on to a fresh creature.
inline std::unordered_map<ObjectGuid::LowType, std::unordered_set<ObjectGuid::LowType>> usedTargetsThisStay;

inline void MarkStayStart(Player* bot)
{
    std::lock_guard<std::mutex> lock(trackerMutex);
    attemptsThisStay[bot->GetGUID().GetCounter()] = 0;
    sightingsThisStay[bot->GetGUID().GetCounter()] = 0;
    usedTargetsThisStay[bot->GetGUID().GetCounter()].clear();
}

inline void RecordAttempt(Player* bot)
{
    std::lock_guard<std::mutex> lock(trackerMutex);
    ++attemptsThisStay[bot->GetGUID().GetCounter()];
}

inline void RecordSighting(Player* bot)
{
    std::lock_guard<std::mutex> lock(trackerMutex);
    ++sightingsThisStay[bot->GetGUID().GetCounter()];
}

inline void RecordUsedTarget(Player* bot, ObjectGuid target)
{
    std::lock_guard<std::mutex> lock(trackerMutex);
    usedTargetsThisStay[bot->GetGUID().GetCounter()].insert(target.GetCounter());
}

[[nodiscard]] inline bool WasTargetUsedThisStay(Player* bot, ObjectGuid target)
{
    std::lock_guard<std::mutex> lock(trackerMutex);
    auto it = usedTargetsThisStay.find(bot->GetGUID().GetCounter());
    return it != usedTargetsThisStay.end() && it->second.count(target.GetCounter()) != 0;
}

[[nodiscard]] inline uint32 AttemptsThisStay(Player* bot)
{
    std::lock_guard<std::mutex> lock(trackerMutex);
    auto it = attemptsThisStay.find(bot->GetGUID().GetCounter());
    return it != attemptsThisStay.end() ? it->second : 0;
}

[[nodiscard]] inline uint32 SightingsThisStay(Player* bot)
{
    std::lock_guard<std::mutex> lock(trackerMutex);
    auto it = sightingsThisStay.find(bot->GetGUID().GetCounter());
    return it != sightingsThisStay.end() ? it->second : 0;
}
}  // namespace QuestStayUseTracker

#endif
