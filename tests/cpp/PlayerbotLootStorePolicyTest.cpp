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

TEST(PlayerbotLootStorePolicy, AQuestNeededItemIsNeverSubjectToTheBagSpaceGuard)
{
    EXPECT_FALSE(LootStoreBagSpaceGuardApplies(false, 100, true));
    EXPECT_FALSE(LootStoreBagSpaceGuardApplies(false, 81, true));
}

TEST(PlayerbotLootStorePolicy, AMasterlessBotWithFullBagsFiltersOrdinaryItems)
{
    EXPECT_TRUE(LootStoreBagSpaceGuardApplies(false, 81, false));
    EXPECT_TRUE(LootStoreBagSpaceGuardApplies(false, 100, false));
}

TEST(PlayerbotLootStorePolicy, TheGuardStaysOffAtOrBelowTheThreshold)
{
    EXPECT_FALSE(LootStoreBagSpaceGuardApplies(false, 80, false));
    EXPECT_FALSE(LootStoreBagSpaceGuardApplies(false, 0, false));
}

TEST(PlayerbotLootStorePolicy, ABotWithARealMasterIsNeverFiltered)
{
    EXPECT_FALSE(LootStoreBagSpaceGuardApplies(true, 100, false));
}
