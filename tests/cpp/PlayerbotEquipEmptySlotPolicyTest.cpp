/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include "Ai/Base/Value/EquipEmptySlotPolicy.h"
#include "gtest/gtest.h"

TEST(PlayerbotEquipEmptySlotPolicyTest, AStatlessItemFillsAnEmptySlotItCanUse)
{
    // The Guild Tabard. No stats, so StatsWeightCalculator returns zero and upstream refused it a
    // slot holding nothing. 36 of 200 bots carried one unworn on 2026-09-03 while 148 wore one,
    // the difference being that the 148 were dressed by PlayerbotFactory, which never asks this.
    EXPECT_TRUE(ShouldFillEmptyEquipSlot(/*botCanUseItemClass*/ true, /*scoreSaysEquip*/ false));
}

TEST(PlayerbotEquipEmptySlotPolicyTest, AnItemTheBotsClassCannotWearIsStillRefused)
{
    // The failure this fix must not cause. CanEquipWeapon and CanEquipArmor also produce a zero
    // score, but they mean something entirely different: plate on a mage is unusable, not merely
    // unscored. Treating the two the same would dress every bot in whatever it happened to loot.
    EXPECT_FALSE(ShouldFillEmptyEquipSlot(/*botCanUseItemClass*/ false, /*scoreSaysEquip*/ false));
}

TEST(PlayerbotEquipEmptySlotPolicyTest, AWellScoredItemIsStillEquippedWhateverElseIsTrue)
{
    // Every acceptance path that worked before this change must keep working, so a positive score
    // is sufficient on its own. This is the term that makes the change purely additive.
    EXPECT_TRUE(ShouldFillEmptyEquipSlot(/*botCanUseItemClass*/ true, /*scoreSaysEquip*/ true));
    EXPECT_TRUE(ShouldFillEmptyEquipSlot(/*botCanUseItemClass*/ false, /*scoreSaysEquip*/ true));
}
