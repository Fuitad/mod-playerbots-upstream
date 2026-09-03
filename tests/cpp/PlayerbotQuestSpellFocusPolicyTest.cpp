/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include "Ai/World/Rpg/QuestGameObjectPolicy.h"
#include "Ai/World/Rpg/QuestSpellFocusPolicy.h"
#include "gtest/gtest.h"

TEST(PlayerbotQuestSpellFocusPolicyTest, AFocusObjectStandsInOnlyWhenNothingLootsTheItem)
{
    // Learning from the Crystals (9581): the sample 23878 has no loot source anywhere and the
    // pick's spell needs focus 1380, so the Impact Site Crystal is where the item comes from.
    EXPECT_TRUE(SpellFocusIsTheItemSource(/*hasLootSources*/ false, 1380));
    // A tool without a focus requirement says nothing about where the item is made.
    EXPECT_FALSE(SpellFocusIsTheItemSource(false, 0));
    // An item that drops somewhere keeps its loot sources, whatever the tool needs.
    EXPECT_FALSE(SpellFocusIsTheItemSource(true, 1380));
}

TEST(PlayerbotQuestSpellFocusPolicyTest, TheBotStopsAYardInsideTheFocusReach)
{
    // The crystal's reach is 5 yards. Standing at exactly 5 is the edge the core's own distance
    // test can round against, so the walk aims at 4 and 4.0 counts as arrived while 4.5 does not.
    EXPECT_FLOAT_EQ(SpellFocusApproachDistance(5), 4.0f);
    EXPECT_TRUE(WithinSpellFocusReach(4.0f, 5));
    EXPECT_FALSE(WithinSpellFocusReach(4.5f, 5));
    // A one-yard focus cannot shrink to nothing.
    EXPECT_FLOAT_EQ(SpellFocusApproachDistance(1), 1.0f);
}

TEST(PlayerbotQuestSpellFocusPolicyTest, ASpellFocusIsCastAtNotOperated)
{
    // A spell focus has no use handler and no loot: CMSG_GAMEOBJ_USE on it does nothing and the
    // loot pipeline never engages it. The bot casts its tool beside it instead.
    EXPECT_EQ(QuestGoInteractionForType(false, false, true), QuestGoInteraction::CastAtFocus);
    // The two existing interactions are unchanged by the third flag.
    EXPECT_EQ(QuestGoInteractionForType(true, false, false), QuestGoInteraction::Use);
    EXPECT_EQ(QuestGoInteractionForType(false, true, false), QuestGoInteraction::Loot);
}
