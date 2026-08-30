/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE (quest-stay-kill-probe). Not present upstream, so it can never conflict on a merge.
// Two roles now live here. The plain kill counter is a temporary diagnostic (remove with the
// quest-abandon-probe markers). The PER-ENTRY counters are LOAD-BEARING: QuestStayEndDecision
// consumes KillsOfEntriesSinceStayStart so a stay that killed the objective's own drop-source
// creatures rotates without blame instead of abandoning on a failed drop roll. Keep them when
// the diagnostics go.

#ifndef _PLAYERBOT_QUESTSTAYKILLPROBE_H
#define _PLAYERBOT_QUESTSTAYKILLPROBE_H

#include "Player.h"
#include "PlayerScript.h"

#include <mutex>
#include <unordered_map>
#include <vector>

namespace QuestStayKillProbe
{
inline std::mutex probeMutex;
inline std::unordered_map<ObjectGuid::LowType, uint32> totalKills;
inline std::unordered_map<ObjectGuid::LowType, uint32> killsAtStayStart;
inline std::unordered_map<ObjectGuid::LowType, std::unordered_map<uint32, uint32>> totalKillsByEntry;
inline std::unordered_map<ObjectGuid::LowType, std::unordered_map<uint32, uint32>> killsByEntryAtStayStart;

inline void RecordKill(Player* killer, uint32 entry)
{
    std::lock_guard<std::mutex> lock(probeMutex);
    ObjectGuid::LowType const guid = killer->GetGUID().GetCounter();
    ++totalKills[guid];
    if (entry)
        ++totalKillsByEntry[guid][entry];
}

inline void MarkStayStart(Player* bot)
{
    std::lock_guard<std::mutex> lock(probeMutex);
    ObjectGuid::LowType const guid = bot->GetGUID().GetCounter();
    killsAtStayStart[guid] = totalKills[guid];
    killsByEntryAtStayStart[guid] = totalKillsByEntry[guid];
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

// Kills of the given creature entries since the current stay began. Load-bearing: feeds the
// relevantKills parameter of QuestStayEndDecision.
inline uint32 KillsOfEntriesSinceStayStart(Player* bot, std::vector<uint32> const& entries)
{
    std::lock_guard<std::mutex> lock(probeMutex);
    ObjectGuid::LowType const guid = bot->GetGUID().GetCounter();
    auto const totalIt = totalKillsByEntry.find(guid);
    if (totalIt == totalKillsByEntry.end())
        return 0;
    auto const baseIt = killsByEntryAtStayStart.find(guid);
    uint32 sum = 0;
    for (uint32 const entry : entries)
    {
        auto const t = totalIt->second.find(entry);
        if (t == totalIt->second.end())
            continue;
        uint32 base = 0;
        if (baseIt != killsByEntryAtStayStart.end())
            if (auto const b = baseIt->second.find(entry); b != baseIt->second.end())
                base = b->second;
        if (t->second > base)
            sum += t->second - base;
    }
    return sum;
}
}  // namespace QuestStayKillProbe

class QuestStayKillProbeScript : public PlayerScript
{
public:
    QuestStayKillProbeScript() : PlayerScript("QuestStayKillProbeScript", {PLAYERHOOK_ON_CREATURE_KILL}) {}

    void OnPlayerCreatureKill(Player* killer, Creature* killed) override
    {
        QuestStayKillProbe::RecordKill(killer, killed ? killed->GetEntry() : 0);
    }
};

#endif
