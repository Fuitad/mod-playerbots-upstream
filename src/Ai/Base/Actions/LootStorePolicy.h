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
 * earlier as "drop luck" rotations. A quest-needed item was exempted from the guard.
 *
 * 2026-09-03: the guard is off for every bot. The quest exemption above fixed one symptom of a
 * rule that is wrong in general, and the material economy showed the rest of it. The guard keeps a
 * STACKABLE item only when the bot already holds a stack of it with room, so a material trip,
 * which exists precisely because the bot needs an item it does not have, arrives holding none of
 * that item and is refused its own yield every time. Measured live: 161 of 200 online bots above
 * the 80% line and 45 at 100%; 104 of 133 released material paths and 76 of 95 still acquiring
 * belonged to bots above the line; bot 1038 at 84% mined one copper vein five times in two minutes
 * and stored no ore, leaving the vein activated so its claim re-opened it forever. Completions ran
 * at 3 to 4 per 25 to 30 minutes across 200 bots. The rule also carried an off-by-one, keeping a
 * stack only while count + looted < maxStack, so a loot that would exactly fill a stack was
 * refused too.
 *
 * Bots with a real master were never filtered, so switching masterless bots off leaves the core's
 * own bag capacity as the only thing deciding what is stored, which is what a player would see.
 * Bag pressure is answered where it belongs, by selling: bots vendor gray items and white weapons,
 * armor and consumables they cannot use, keeping trade goods and quest items.
 *
 * The predicate is kept rather than deleted. mod-playerbots-economy calls it so its capacity check
 * follows this fork's rule instead of mirroring a copy of the number, and a future rule that is
 * actually about bag space has one place to live.
 */

#ifndef _PLAYERBOT_LOOTSTOREPOLICY_H
#define _PLAYERBOT_LOOTSTOREPOLICY_H

#include "Define.h"

constexpr uint8 LOOT_STORE_BAG_SPACE_GUARD_PERCENT = 80;

// True when StoreLootAction must run its bag-space filtering (max-stack-1 items dropped, stackable
// items kept only with a free stack) before storing the item. Always false now: see the header
// comment. The parameters are kept so the call sites and the economy's capacity check keep reading
// one rule, and so re-introducing a bag rule is an edit here rather than a search for call sites.
[[nodiscard]] inline bool LootStoreBagSpaceGuardApplies(bool /*hasRealMaster*/, uint8 /*bagSpacePercent*/,
                                                        bool /*neededForQuest*/)
{
    return false;
}

#endif
