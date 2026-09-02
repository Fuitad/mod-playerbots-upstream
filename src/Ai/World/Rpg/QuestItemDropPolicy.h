/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * A quest can hand the bot its tool through a drop: quest_template.ItemDrop names an item the
 * quest wants collected on the way, and for a "use X on Y" objective that item IS the tool. Kyle's
 * Gone Missing (11129) wants Tender Strider Meat (33009, a 50 percent quest drop from plainstriders)
 * fed to Kyle the Frenzied (23616, objective index 1). quest_poi surveys the drop area under
 * objective index 10 + i for ItemDrop[i]; the core's own quest log reads the same convention.
 * Upstream only ever selected npc, gameobject and item objectives, so the drop POI was never
 * picked, the bot went straight to Kyle with no meat, and every stay ended in an abandon (seven
 * bots held 11129 at 22:00 on 2026-09-01, 0 meat between them).
 */

#ifndef _PLAYERBOT_QUESTITEMDROPPOLICY_H
#define _PLAYERBOT_QUESTITEMDROPPOLICY_H

#include "Define.h"

#include <vector>

// quest_poi's objective index for ItemDrop[i] is QUEST_OBJECTIVES_COUNT (4) plus
// QUEST_ITEM_OBJECTIVES_COUNT (6) plus i. Verified against acore_world.quest_poi on 2026-09-01:
// indices 10 to 13 carry 564 rows, and 11129's index 10 blobs cover the plainstrider fields, not Kyle.
inline constexpr int32 QUEST_ITEMDROP_OBJECTIVE_BASE = 10;

[[nodiscard]] inline bool IsItemDropObjectiveIndex(int32 objectiveIdx, int32 itemDropCount)
{
    return objectiveIdx >= QUEST_ITEMDROP_OBJECTIVE_BASE && objectiveIdx < QUEST_ITEMDROP_OBJECTIVE_BASE + itemDropCount;
}

// Which objectives the bot should work now. A missing tool comes first and alone: the npc or
// gameobject objective that needs it cannot be progressed without it, so travelling there is a
// five minute stay with nothing to do. Once every tool is held the ordinary objectives are worked.
// Drops that are not tools (no use spell) are collected alongside the ordinary objectives, nearest
// POI first, exactly as before. The caller only lists a drop that quest_poi surveys under its own
// index: 106 quests carry a spell-bearing drop with no drop blob (mostly Northrend dailies whose
// drop is filled by the objective itself), and those keep the upstream path untouched.
[[nodiscard]] inline std::vector<int32> QuestObjectivesToWork(std::vector<int32> const& ordinaryObjectives,
                                                             std::vector<int32> const& missingToolDrops,
                                                             std::vector<int32> const& missingPlainDrops)
{
    if (!missingToolDrops.empty())
        return missingToolDrops;
    std::vector<int32> work = ordinaryObjectives;
    work.insert(work.end(), missingPlainDrops.begin(), missingPlainDrops.end());
    return work;
}

#endif
