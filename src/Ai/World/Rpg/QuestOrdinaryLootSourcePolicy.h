/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#ifndef _PLAYERBOT_QUESTORDINARYLOOTSOURCEPOLICY_H
#define _PLAYERBOT_QUESTORDINARYLOOTSOURCEPOLICY_H

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Define.h"

struct QuestCreatureLootSourceFact
{
    uint32 creatureEntry = 0;
    uint32 itemId = 0;
    int32 reference = 0;
};

using QuestCreatureLootSourceIndex = std::unordered_map<uint32, std::vector<uint32>>;

inline void IndexDirectRequiredCreatureLootSource(QuestCreatureLootSourceIndex& sources,
                                                  std::unordered_set<uint32> const& requiredItems,
                                                  QuestCreatureLootSourceFact const& fact)
{
    if (!fact.creatureEntry || !fact.itemId || fact.reference || !requiredItems.contains(fact.itemId))
        return;

    std::vector<uint32>& entries = sources[fact.itemId];
    if (std::find(entries.begin(), entries.end(), fact.creatureEntry) == entries.end())
        entries.push_back(fact.creatureEntry);
}

#endif
