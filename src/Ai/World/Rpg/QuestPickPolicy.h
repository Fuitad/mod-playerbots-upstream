/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#ifndef _PLAYERBOT_QUESTPICKPOLICY_H
#define _PLAYERBOT_QUESTPICKPOLICY_H

#include "Define.h"

#include <cstddef>
#include <vector>

// Indices of the quests tied at the LOWEST quest level, so the caller can pick randomly among
// them. Lower-level quests go first because the bot outlevels them while higher-level ones only
// ripen (Pierre, 2026-08-30: Muzeze carried a level-5 quest to level 9 and the gray-drop policy
// retired it untried). A quest level <= 0 scales with the player, can never be outleveled, and
// therefore sorts last.
[[nodiscard]] inline std::vector<size_t> LowestLevelQuestIndices(std::vector<int32> const& questLevels)
{
    std::vector<size_t> best;
    int32 bestLevel = 0;
    bool haveBest = false;
    for (size_t i = 0; i < questLevels.size(); ++i)
    {
        if (questLevels[i] <= 0)
            continue;
        if (!haveBest || questLevels[i] < bestLevel)
        {
            bestLevel = questLevels[i];
            best.clear();
            haveBest = true;
        }
        if (questLevels[i] == bestLevel)
            best.push_back(i);
    }
    if (haveBest)
        return best;
    // Only scaling quests: all equally urgent.
    for (size_t i = 0; i < questLevels.size(); ++i)
        best.push_back(i);
    return best;
}

#endif
