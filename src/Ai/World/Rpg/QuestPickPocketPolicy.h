/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#ifndef _PLAYERBOT_QUESTPICKPOCKETPOLICY_H
#define _PLAYERBOT_QUESTPICKPOCKETPOLICY_H

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Define.h"

inline constexpr uint32 QUEST_PICK_POCKET_SPELL = 921;

struct QuestPickPocketSourceFact
{
    uint32 creatureEntry = 0;
    uint32 itemId = 0;
    int32 reference = 0;
};

using QuestPickPocketSourceIndex = std::unordered_map<uint32, std::vector<uint32>>;

inline void IndexRequiredPickPocketSource(QuestPickPocketSourceIndex& sources,
                                          std::unordered_set<uint32> const& requiredItems,
                                          QuestPickPocketSourceFact const& fact)
{
    if (!fact.creatureEntry || !fact.itemId || fact.reference || !requiredItems.contains(fact.itemId))
        return;

    std::vector<uint32>& entries = sources[fact.itemId];
    if (std::find(entries.begin(), entries.end(), fact.creatureEntry) == entries.end())
        entries.push_back(fact.creatureEntry);
}

enum class QuestPickPocketStep : uint8
{
    Unavailable = 0,
    EnterStealth,
    PickPocket,
};

[[nodiscard]] inline bool QuestPickPocketAvailable(bool hasSource, bool rogue, bool knowsPickPocket)
{
    return hasSource && rogue && knowsPickPocket;
}

[[nodiscard]] inline float QuestPickPocketEngageDistance(float spellMaxRange)
{
    return std::max(1.0f, spellMaxRange - 1.0f);
}

[[nodiscard]] inline QuestPickPocketStep NextQuestPickPocketStep(bool rogue, bool knowsPickPocket, bool targetAlive,
                                                                 bool targetHostile, bool stealthed)
{
    if (!rogue || !knowsPickPocket || !targetAlive || !targetHostile)
        return QuestPickPocketStep::Unavailable;
    return stealthed ? QuestPickPocketStep::PickPocket : QuestPickPocketStep::EnterStealth;
}

#endif
