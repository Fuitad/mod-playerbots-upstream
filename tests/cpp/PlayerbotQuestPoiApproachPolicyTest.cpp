/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include "Ai/World/Rpg/QuestPoiApproachPolicy.h"
#include "gtest/gtest.h"

namespace
{
QuestPoiApproachFacts At(float distance, bool reached)
{
    QuestPoiApproachFacts facts;
    facts.distanceYards = distance;
    facts.reached = reached;
    facts.arriveRadius = 10.0f;
    facts.returnRadius = 75.0f;
    return facts;
}
}  // namespace

TEST(PlayerbotQuestPoiApproachPolicyTest, BeforeArrivingTheBotWalksInAsUpstreamDid)
{
    EXPECT_TRUE(QuestPoiNeedsApproach(At(800.0f, false)));
    EXPECT_TRUE(QuestPoiNeedsApproach(At(10.1f, false)));
    EXPECT_FALSE(QuestPoiNeedsApproach(At(10.0f, false)));
    EXPECT_FALSE(QuestPoiNeedsApproach(At(0.0f, false)));
}

TEST(PlayerbotQuestPoiApproachPolicyTest, AfterDriftingOffAReachedPoiTheBotWalksBack)
{
    // The defect: upstream latched this branch off the moment the POI was first reached, so a bot
    // pulled away by combat or gathering never returned and abandoned the quest five minutes later.
    // Measured: 28 of 33 abandons happened more than 15 yards from the POI, all of them after a
    // successful arrival, every one with a zero objective counter.
    EXPECT_TRUE(QuestPoiNeedsApproach(At(800.0f, true)));
    EXPECT_TRUE(QuestPoiNeedsApproach(At(200.0f, true)));
    EXPECT_TRUE(QuestPoiNeedsApproach(At(75.1f, true)));
}

TEST(PlayerbotQuestPoiApproachPolicyTest, WorkingNearTheObjectiveIsNotInterrupted)
{
    // The return radius is the configured grind distance, the radius in which a bot may legitimately
    // be fighting for this objective. Inside it the bot is left alone, which is why the latch existed.
    EXPECT_FALSE(QuestPoiNeedsApproach(At(75.0f, true)));
    EXPECT_FALSE(QuestPoiNeedsApproach(At(40.0f, true)));
    EXPECT_FALSE(QuestPoiNeedsApproach(At(11.0f, true)));
    EXPECT_FALSE(QuestPoiNeedsApproach(At(0.0f, true)));
}

TEST(PlayerbotQuestPoiApproachPolicyTest, TheTwoRadiiAreIndependent)
{
    // A distance between the arrive radius and the return radius means "approach" before arrival and
    // "stay put" after it. Collapsing the two radii into one would break one case or the other.
    EXPECT_TRUE(QuestPoiNeedsApproach(At(30.0f, false)));
    EXPECT_FALSE(QuestPoiNeedsApproach(At(30.0f, true)));
}

TEST(PlayerbotQuestPoiApproachPolicyTest, AConfiguredReturnRadiusIsHonoured)
{
    QuestPoiApproachFacts tight = At(30.0f, true);
    tight.returnRadius = 20.0f;
    EXPECT_TRUE(QuestPoiNeedsApproach(tight));

    QuestPoiApproachFacts loose = At(300.0f, true);
    loose.returnRadius = 500.0f;
    EXPECT_FALSE(QuestPoiNeedsApproach(loose));
}
