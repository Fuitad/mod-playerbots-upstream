/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE (equip-empty-slot). Not present upstream, so it can never conflict on a merge.
 *
 * Whether an item should fill an equipment slot that currently holds nothing.
 *
 * Upstream decides equipping from the stat score alone: ItemUsageValue sets shouldEquip only when
 * StatsWeightCalculator returns a non-zero score, and an item that scores zero is refused even when
 * the destination slot is EMPTY. A tabard has no stats, so it scores zero, so a bot that receives
 * one after character creation can never wear it. Measured live 2026-09-03: 36 of 200 online bots
 * carried a Guild Tabard in their bags while 148 wore one, the 148 having been dressed by
 * PlayerbotFactory, which does not go through this comparison. The same refusal applies to shirts
 * and to any statless item.
 *
 * The score is the right question when REPLACING something: is this better than what I have. It is
 * the wrong question for an empty slot, where the honest comparison is against nothing. So an empty
 * slot is filled whenever the bot is allowed to use the item at all.
 *
 * "Allowed to use" is not the same as "scores zero", and conflating them is how this fix could
 * cause harm: RandomItemMgr::CanEquipWeapon and CanEquipArmor also clear shouldEquip, and they mean
 * the bot's class cannot wear the item. Plate on a mage scores zero AND is unusable; a tabard
 * scores zero and is perfectly usable. Only the second may fill a slot.
 */

#ifndef _PLAYERBOT_EQUIPEMPTYSLOTPOLICY_H
#define _PLAYERBOT_EQUIPEMPTYSLOTPOLICY_H

#include "Define.h"

[[nodiscard]] inline bool ShouldFillEmptyEquipSlot(bool botCanUseItemClass, bool scoreSaysEquip)
{
    // The score still counts: an item that scores well is equipped whatever else is true, which
    // keeps every pre-existing acceptance path intact. The new half is the first term.
    return botCanUseItemClass || scoreSaysEquip;
}

#endif
