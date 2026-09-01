/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include "Ai/World/Rpg/QuestRewardBagPolicy.h"
#include "gtest/gtest.h"

namespace
{
BagStackFacts Stack(bool protectedUsage, bool bound, uint32_t sellValue, bool trash)
{
    BagStackFacts f;
    f.protectedUsage = protectedUsage;
    f.bound = bound;
    f.sellValue = sellValue;
    f.trash = trash;
    return f;
}
}  // namespace

TEST(PlayerbotQuestRewardBagPolicyTest, TrashGoesBeforeAuctionMaterialsWhateverTheValue)
{
    std::vector<BagStackFacts> const stacks = {
        Stack(false, false, 5, false),    // cheap eggs (auction grade)
        Stack(false, false, 400, true),   // pricey gray weapon
        Stack(false, false, 900, false),  // tenderloin stack
    };
    EXPECT_EQ(ChooseBagStackToSacrifice(stacks), 1u);
}

TEST(PlayerbotQuestRewardBagPolicyTest, AmongAuctionMaterialsTheCheapestStackGoes)
{
    std::vector<BagStackFacts> const stacks = {
        Stack(false, false, 900, false),
        Stack(false, false, 40, false),
        Stack(false, false, 120, false),
    };
    EXPECT_EQ(ChooseBagStackToSacrifice(stacks), 1u);
}

TEST(PlayerbotQuestRewardBagPolicyTest, ProtectedAndBoundStacksAreNeverSacrificed)
{
    // The defect this pins: a full backpack must cost a stack of gathered eggs, never the quest
    // item the bot is about to hand in or the gear it wears.
    std::vector<BagStackFacts> const stacks = {
        Stack(true, false, 1, false),   // quest item
        Stack(false, true, 1, true),    // soulbound gray
        Stack(false, false, 300, false),
    };
    EXPECT_EQ(ChooseBagStackToSacrifice(stacks), 2u);

    std::vector<BagStackFacts> const onlyProtected = {Stack(true, false, 1, true), Stack(false, true, 1, false)};
    EXPECT_EQ(ChooseBagStackToSacrifice(onlyProtected), QUEST_REWARD_NO_VICTIM);
    EXPECT_EQ(ChooseBagStackToSacrifice({}), QUEST_REWARD_NO_VICTIM);
}
