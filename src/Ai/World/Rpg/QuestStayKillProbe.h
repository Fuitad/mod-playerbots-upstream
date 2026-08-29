/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE (quest-stay-kill-probe). Not present upstream, so it can never conflict on a merge.
// Temporary diagnostic: counts creature kills per bot so the quest POI stay can report how much
// fighting actually happened between arrival and abandon. Distinguishes "the bot never fights
// during the stay" (scheduling problem) from "the bot fights plenty but the wrong things or the
// drops never come" (targeting or tempo problem). Remove together with the quest-abandon-probe
// markers once the abandon cause is settled.

#ifndef _PLAYERBOT_QUESTSTAYKILLPROBE_H
#define _PLAYERBOT_QUESTSTAYKILLPROBE_H

#include "Player.h"
#include "PlayerScript.h"

#include <mutex>
#include <unordered_map>

namespace QuestStayKillProbe
{
inline std::mutex probeMutex;
inline std::unordered_map<ObjectGuid::LowType, uint32> totalKills;
inline std::unordered_map<ObjectGuid::LowType, uint32> killsAtStayStart;

inline void RecordKill(Player* killer)
{
    std::lock_guard<std::mutex> lock(probeMutex);
    ++totalKills[killer->GetGUID().GetCounter()];
}

inline void MarkStayStart(Player* bot)
{
    std::lock_guard<std::mutex> lock(probeMutex);
    killsAtStayStart[bot->GetGUID().GetCounter()] = totalKills[bot->GetGUID().GetCounter()];
}

inline uint32 KillsSinceStayStart(Player* bot)
{
    std::lock_guard<std::mutex> lock(probeMutex);
    ObjectGuid::LowType const guid = bot->GetGUID().GetCounter();
    uint32 const now = totalKills[guid];
    auto const it = killsAtStayStart.find(guid);
    uint32 const base = it == killsAtStayStart.end() ? 0u : it->second;
    return now >= base ? now - base : now;
}
}  // namespace QuestStayKillProbe

class QuestStayKillProbeScript : public PlayerScript
{
public:
    QuestStayKillProbeScript() : PlayerScript("QuestStayKillProbeScript", {PLAYERHOOK_ON_CREATURE_KILL}) {}

    void OnPlayerCreatureKill(Player* killer, Creature* /*killed*/) override
    {
        QuestStayKillProbe::RecordKill(killer);
    }
};

#endif
