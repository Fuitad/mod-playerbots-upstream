/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include "Ai/World/Rpg/QuestStayAnchorPolicy.h"
#include "gtest/gtest.h"

TEST(PlayerbotQuestStayAnchorPolicyTest, TheSpawnNearestThePoiPointWins)
{
    // The defect this pins: quest 47's POI marks the mine entrance while the gold-dust kobolds
    // spawn inside the cave; the stay must anchor on the spawn closest to the surveyed point,
    // not on the point itself.
    std::vector<SpawnAnchorPoint> const spawns = {
        {100.0f, 100.0f, 10.0f},
        {30.0f, 40.0f, -20.0f},  // 50y from origin, nearest
        {200.0f, 0.0f, 5.0f},
    };

    EXPECT_EQ(NearestSpawnAnchorIndex(spawns, 0.0f, 0.0f, QUEST_ANCHOR_MAX_SNAP_DISTANCE), 1u);
}

TEST(PlayerbotQuestStayAnchorPolicyTest, ASpawnBeyondTheSnapCapIsAnotherCluster)
{
    // A spawn cluster hundreds of yards away belongs to a different POI blob; snapping across
    // would send the bot to the wrong side of the zone. No spawn within the cap keeps the POI
    // point unchanged, which is also the credit-dummy case (9303: entry 16534 never spawns).
    std::vector<SpawnAnchorPoint> const spawns = {
        {500.0f, 0.0f, 0.0f},
    };

    EXPECT_EQ(NearestSpawnAnchorIndex(spawns, 0.0f, 0.0f, QUEST_ANCHOR_MAX_SNAP_DISTANCE), QUEST_ANCHOR_NO_SPAWN);
    EXPECT_EQ(NearestSpawnAnchorIndex({}, 0.0f, 0.0f, QUEST_ANCHOR_MAX_SNAP_DISTANCE), QUEST_ANCHOR_NO_SPAWN);
}

TEST(PlayerbotQuestStayAnchorPolicyTest, AZeroCapDisablesTheDistanceFilter)
{
    std::vector<SpawnAnchorPoint> const spawns = {
        {500.0f, 0.0f, 0.0f},
        {900.0f, 0.0f, 0.0f},
    };

    EXPECT_EQ(NearestSpawnAnchorIndex(spawns, 0.0f, 0.0f, 0.0f), 0u);
}

TEST(PlayerbotQuestStayAnchorPolicyTest, CreatureSpawnsWithinCountsOnlyCreaturesInRange)
{
    // The defect this pins: a named creature on its respawn timer near the anchor must be visible
    // to the stay-end verdict as a reason to come back, and a gameobject spawn must not count.
    std::vector<SpawnAnchorPoint> const spawns = {
        {100.0f, 100.0f, 0.0f, 1, true},   // 0y from the anchor
        {160.0f, 100.0f, 0.0f, 2, true},   // 60y away
        {200.0f, 100.0f, 0.0f, 3, true},   // 100y away, out of range
        {100.0f, 100.0f, 0.0f, 4, false},  // gameobject at the anchor
    };
    EXPECT_EQ(CreatureSpawnsWithin(spawns, 100.0f, 100.0f, 75.0f), 2u);
    EXPECT_EQ(CreatureSpawnsWithin(spawns, 100.0f, 100.0f, 10.0f), 1u);
    EXPECT_EQ(CreatureSpawnsWithin({}, 100.0f, 100.0f, 75.0f), 0u);
}
