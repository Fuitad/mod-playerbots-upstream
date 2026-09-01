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

// How far (2D yards, same zone) a COMPLETE quest's turn-in POI may be for the quest to count as
// pickable. Upstream capped it at 1500, the same leash as an objective, which strands every quest
// whose ender sits farther than that from where the bot completes it. Measured 2026-09-01: The
// Missing Fisherman (10428) is auto-complete at Dulliah (-4180, -12493) and turns in to Cowlen on
// Silvermyst Isle (-5358, -11175), 1700 yards away; 13 bots sat at complete with 0 ever rewarded.
// A turn-in is one trip with a guaranteed payoff, so it gets double the objective leash, and no
// same-zone gate: capital cities are their own zone (Mournful, 1679, Kharanos to Ironforge).
constexpr float QUEST_REWARD_POI_MAX_DISTANCE = 3000.0f;

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

// A quest that is already COMPLETE is a turn-in: one trip, a guaranteed reward, a freed log slot.
// It goes before any objective work, whatever its level; the lowest-level rule then orders the
// turn-ins among themselves. Measured 2026-09-01: Mournful carried Muren Stormpike (1679,
// scaling level, so it sorted LAST) at complete for over an hour while grinding four level 7 to
// 12 quests, with the Ironforge turn-in 700y away the whole time.
[[nodiscard]] inline std::vector<size_t> PickableQuestIndices(std::vector<int32> const& questLevels,
                                                              std::vector<bool> const& questComplete)
{
    std::vector<size_t> completeIdx;
    std::vector<int32> completeLevels;
    for (size_t i = 0; i < questLevels.size() && i < questComplete.size(); ++i)
        if (questComplete[i])
        {
            completeIdx.push_back(i);
            completeLevels.push_back(questLevels[i]);
        }
    if (completeIdx.empty())
        return LowestLevelQuestIndices(questLevels);
    std::vector<size_t> best;
    for (size_t k : LowestLevelQuestIndices(completeLevels))
        best.push_back(completeIdx[k]);
    return best;
}

#endif
