/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * A quest that just killed the bot waits before it is picked again.
 *
 * Measured live 2026-09-02, 00:21 to 00:28, once ghosts reached their bodies again: 9 of the 10
 * body revives followed by a death within two minutes died on the SAME quest and RPG status as
 * before. The picker takes the lowest-level quest, which is the one the bot just died on, so the
 * bot walked straight back to the anchor and the mob standing there. The death-rotate rule ended a
 * stay only after two deaths, which still allowed one relapse per stay.
 *
 * The rule: the first death in a stay ends the stay without blame and puts the quest on a
 * ten-minute cooldown for this bot; a second death on the same quest inside the cooldown is the
 * old two-death verdict (lowPriorityQuest for the process). Cooldowns live in memory per process.
 */

#ifndef _PLAYERBOT_QUESTDEATHCOOLDOWN_H
#define _PLAYERBOT_QUESTDEATHCOOLDOWN_H

#include "Define.h"
#include "ObjectGuid.h"

#include <mutex>
#include <unordered_map>

inline constexpr uint32 QUEST_DEATH_COOLDOWN_MS = 10 * 60 * 1000;

struct QuestDeathRecord
{
    uint32 deaths = 0;
    uint32 lastDeathMs = 0;
};

[[nodiscard]] inline bool QuestOnDeathCooldown(QuestDeathRecord const& record, uint32 nowMs)
{
    return record.deaths > 0 && nowMs - record.lastDeathMs < QUEST_DEATH_COOLDOWN_MS;
}

// Deaths outside the cooldown do not accumulate: a quest the bot died on an hour ago starts
// fresh, so the process-lifetime blame needs two deaths close together.
[[nodiscard]] inline QuestDeathRecord RecordQuestDeath(QuestDeathRecord record, uint32 nowMs)
{
    if (!QuestOnDeathCooldown(record, nowMs))
        record.deaths = 0;
    ++record.deaths;
    record.lastDeathMs = nowMs;
    return record;
}

namespace QuestDeathCooldown
{
inline std::mutex registryMutex;
inline std::unordered_map<ObjectGuid::LowType, std::unordered_map<uint32, QuestDeathRecord>> registry;

inline QuestDeathRecord Note(ObjectGuid::LowType botGuid, uint32 questId, uint32 nowMs)
{
    std::lock_guard<std::mutex> lock(registryMutex);
    QuestDeathRecord& record = registry[botGuid][questId];
    record = RecordQuestDeath(record, nowMs);
    return record;
}

inline bool Active(ObjectGuid::LowType botGuid, uint32 questId, uint32 nowMs)
{
    std::lock_guard<std::mutex> lock(registryMutex);
    auto const bot = registry.find(botGuid);
    if (bot == registry.end())
        return false;
    auto const quest = bot->second.find(questId);
    return quest != bot->second.end() && QuestOnDeathCooldown(quest->second, nowMs);
}
}  // namespace QuestDeathCooldown

#endif
