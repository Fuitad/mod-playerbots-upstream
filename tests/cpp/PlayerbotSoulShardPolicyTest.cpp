/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include "Ai/Class/Warlock/SoulShardPolicy.h"
#include "gtest/gtest.h"

TEST(PlayerbotSoulShardPolicyTest, AMasterlessWarlockKeepsFiveAndNoMore)
{
    // Five covers Healthstone, Soulstone and a summon with room to spare. At the cap exactly
    // nothing is destroyed; the cap is what the bot may keep, not the point at which it is over.
    EXPECT_FALSE(HoldingTooManySoulShards(5, /*hasRealMaster*/ false));
    EXPECT_TRUE(HoldingTooManySoulShards(6, false));
    EXPECT_EQ(SoulShardCap(false), 5u);
}

TEST(PlayerbotSoulShardPolicyTest, TheUpstreamCapIsUnreachableForABaglessWarlock)
{
    // The reason this exists. A soul shard does not stack, so upstream's 26 is 26 bag slots,
    // more than the 16-slot backpack of a warlock that owns no bags: the bag fills and looting
    // stops before the trigger can ever fire. Measured 2026-09-03: all 22 online warlocks held
    // shards, 120 slots between them, averaging 5.5 each, none anywhere near 26.
    EXPECT_FALSE(HoldingTooManySoulShards(16, /*hasRealMaster*/ true));
    EXPECT_FALSE(HoldingTooManySoulShards(25, true));
    // The same holding is over the line the moment nobody is directing the bot.
    EXPECT_TRUE(HoldingTooManySoulShards(16, false));
}

TEST(PlayerbotSoulShardPolicyTest, ABotWithARealMasterKeepsUpstreamsReserve)
{
    // A player directing a warlock may want a deep reserve before a raid, and that bot's bags are
    // the player's problem rather than the population's.
    EXPECT_EQ(SoulShardCap(true), 26u);
    EXPECT_FALSE(HoldingTooManySoulShards(26, true));
    EXPECT_TRUE(HoldingTooManySoulShards(27, true));
}
