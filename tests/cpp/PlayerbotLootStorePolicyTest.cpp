/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include "Ai/Base/Actions/LootStorePolicy.h"
#include "gtest/gtest.h"

TEST(PlayerbotLootStorePolicy, AMaterialTripStoresItsOwnYieldOnAFullBagBot)
{
    // The case that killed the material economy. The guard kept a stackable item only when the bot
    // already held a stack of it with room, and a material trip exists precisely because the bot
    // needs an item it does not have, so it arrived holding none and was refused its own yield.
    // Bot 1038 at 84% bags mined one copper vein five times in two minutes and stored no ore,
    // leaving the vein activated so its claim re-opened it forever. 161 of 200 online bots were
    // above the line.
    EXPECT_FALSE(LootStoreBagSpaceGuardApplies(false, 84, false));
    EXPECT_FALSE(LootStoreBagSpaceGuardApplies(false, 100, false));
}

TEST(PlayerbotLootStorePolicy, TheGuardIsOffAtEveryBagLevel)
{
    // Nothing between empty and full re-arms it. Whatever the core's own bag capacity allows is
    // what gets stored, which is what a player would see.
    for (uint8 percent = 0; percent < 101; ++percent)
        EXPECT_FALSE(LootStoreBagSpaceGuardApplies(false, percent, false)) << "at " << int(percent) << "%";
}

TEST(PlayerbotLootStorePolicy, AQuestNeededItemIsStillNeverFiltered)
{
    // The quest exemption was the first symptom fixed: every quest item is max-stack 1, so a bot
    // with a filled backpack skipped the quest item out of every chest it opened, released, and
    // re-armed the chest for the next identical failure. It must stay true now that the whole
    // rule is off, so re-introducing a bag rule cannot silently revive that failure.
    EXPECT_FALSE(LootStoreBagSpaceGuardApplies(false, 100, true));
    EXPECT_FALSE(LootStoreBagSpaceGuardApplies(false, 81, true));
}

TEST(PlayerbotLootStorePolicy, ABotWithARealMasterIsStillNeverFiltered)
{
    // Unchanged, and the reason switching masterless bots off is not a new behaviour so much as
    // the same behaviour a mastered bot already had.
    EXPECT_FALSE(LootStoreBagSpaceGuardApplies(true, 100, false));
    EXPECT_FALSE(LootStoreBagSpaceGuardApplies(true, 0, true));
}
