/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * Pure decision for StoreLootAction: whether the bag-space guard may drop a loot item. Upstream
 * skips every max-stack-1 item once a masterless bot's bags pass 80% full, to keep junk out of
 * full bags. Every quest item is max-stack 1, so a low-level bot with a filled backpack (measured
 * live 2026-08-31: 13 to 16 of 16 slots on every bot stuck on Solanian's Belongings) opened its
 * quest chest, received the loot, skipped the quest item, released, and re-armed the chest for
 * the next identical failure. The same skip drops quest items from creature loot, which showed up
 * earlier as "drop luck" rotations. A quest-needed item is exempt from the guard.
 */

#ifndef _PLAYERBOT_LOOTSTOREPOLICY_H
#define _PLAYERBOT_LOOTSTOREPOLICY_H

#include "Define.h"

constexpr uint8 LOOT_STORE_BAG_SPACE_GUARD_PERCENT = 80;

// True when StoreLootAction must run its bag-space filtering (max-stack-1 items dropped, stackable
// items kept only with a free stack) before storing the item.
[[nodiscard]] inline bool LootStoreBagSpaceGuardApplies(bool hasRealMaster, uint8 bagSpacePercent,
                                                        bool neededForQuest)
{
    if (hasRealMaster || neededForQuest)
        return false;
    return bagSpacePercent > LOOT_STORE_BAG_SPACE_GUARD_PERCENT;
}

#endif
