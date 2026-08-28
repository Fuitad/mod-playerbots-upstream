/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include "Ai/World/Rpg/QuestPoiReachPolicy.h"
#include "gtest/gtest.h"

namespace
{
QuestPoiReachFacts Facts(bool sameZone, float distance, uint32 areaLevel, uint32 botLevel)
{
    QuestPoiReachFacts facts;
    facts.sameMap = true;
    facts.sameZone = sameZone;
    facts.distanceYards = distance;
    facts.destinationAreaLevel = areaLevel;
    facts.botLevel = botLevel;
    facts.botAtMaxLevel = botLevel >= 80u;
    return facts;
}
}  // namespace

TEST(PlayerbotQuestPoiReachPolicyTest, AnotherMapIsRefusedBecauseNothingDrivesTheBoatOrPortal)
{
    QuestPoiReachFacts facts = Facts(false, 100.0f, 5u, 10u);
    facts.sameMap = false;
    EXPECT_EQ(ClassifyQuestPoiReach(facts), QuestPoiReach::WrongMap);
    EXPECT_FALSE(QuestPoiAdmissible(QuestPoiReach::WrongMap));
}

TEST(PlayerbotQuestPoiReachPolicyTest, WorkInTheBotsOwnZoneStaysLocalAndWorkable)
{
    EXPECT_EQ(ClassifyQuestPoiReach(Facts(true, 0.0f, 40u, 8u)), QuestPoiReach::Local);
    EXPECT_EQ(ClassifyQuestPoiReach(Facts(true, 1499.0f, 40u, 8u)), QuestPoiReach::Local);
    EXPECT_TRUE(QuestPoiAdmissible(QuestPoiReach::Local));
    // A high area level does not block the bot's own zone: it is already standing there.
    EXPECT_EQ(ClassifyQuestPoiReach(Facts(true, 10.0f, 60u, 8u)), QuestPoiReach::Local);
}

TEST(PlayerbotQuestPoiReachPolicyTest, ALevelAppropriateNeighbourZoneIsNowReachable)
{
    // The defect this fixes: upstream refused every POI outside the current zone, so a quest whose
    // objective sat next door could never be worked and never abandoned.
    EXPECT_EQ(ClassifyQuestPoiReach(Facts(false, 1800.0f, 10u, 12u)), QuestPoiReach::DistantSafe);
    EXPECT_TRUE(QuestPoiAdmissible(QuestPoiReach::DistantSafe));
    // An area declaring no level at all is not treated as dangerous.
    EXPECT_EQ(ClassifyQuestPoiReach(Facts(false, 1800.0f, 0u, 8u)), QuestPoiReach::DistantSafe);
}

TEST(PlayerbotQuestPoiReachPolicyTest, AZoneAboveTheBotsLevelIsRefusedSoItDoesNotDieCrossing)
{
    EXPECT_EQ(ClassifyQuestPoiReach(Facts(false, 1800.0f, 25u, 8u)), QuestPoiReach::DistantUnsafe);
    EXPECT_FALSE(QuestPoiAdmissible(QuestPoiReach::DistantUnsafe));
    // Exactly at the bot's level is fine; one above is not.
    EXPECT_EQ(ClassifyQuestPoiReach(Facts(false, 1800.0f, 12u, 12u)), QuestPoiReach::DistantSafe);
    EXPECT_EQ(ClassifyQuestPoiReach(Facts(false, 1800.0f, 13u, 12u)), QuestPoiReach::DistantUnsafe);
    // At max level the gate lifts, matching ExploreTravelDestination::isActive.
    EXPECT_EQ(ClassifyQuestPoiReach(Facts(false, 1800.0f, 70u, 80u)), QuestPoiReach::DistantSafe);
}

TEST(PlayerbotQuestPoiReachPolicyTest, BeyondWalkingRangeIsRefusedRegardlessOfLevel)
{
    EXPECT_EQ(ClassifyQuestPoiReach(Facts(false, 2499.0f, 1u, 80u)), QuestPoiReach::DistantSafe);
    EXPECT_EQ(ClassifyQuestPoiReach(Facts(false, 2500.0f, 1u, 80u)), QuestPoiReach::TooFar);
    EXPECT_FALSE(QuestPoiAdmissible(QuestPoiReach::TooFar));
    // A far POI in the bot's own zone is past the local radius, so it is judged as a trip.
    EXPECT_EQ(ClassifyQuestPoiReach(Facts(true, 1600.0f, 1u, 10u)), QuestPoiReach::DistantSafe);
}

TEST(PlayerbotQuestPoiReachPolicyTest, LocalWorkIsTakenBeforeAnyTrip)
{
    // This is the fallback the design depends on: when a distant objective is refused as unsafe, or
    // simply loses to a local one, the bot keeps doing local work instead of idling.
    EXPECT_TRUE(QuestPoiPreferred(QuestPoiReach::Local, 1400.0f, QuestPoiReach::DistantSafe, 50.0f, true));
    EXPECT_FALSE(QuestPoiPreferred(QuestPoiReach::DistantSafe, 50.0f, QuestPoiReach::Local, 1400.0f, true));
}

TEST(PlayerbotQuestPoiReachPolicyTest, WithinTheSameClassTheNearestPoiWins)
{
    // Upstream picked at random despite calling the result nearestPoi.
    EXPECT_TRUE(QuestPoiPreferred(QuestPoiReach::Local, 20.0f, QuestPoiReach::Local, 900.0f, true));
    EXPECT_FALSE(QuestPoiPreferred(QuestPoiReach::Local, 900.0f, QuestPoiReach::Local, 20.0f, true));
    EXPECT_TRUE(QuestPoiPreferred(QuestPoiReach::DistantSafe, 1700.0f, QuestPoiReach::DistantSafe, 2400.0f, true));
    EXPECT_FALSE(QuestPoiPreferred(QuestPoiReach::Local, 20.0f, QuestPoiReach::Local, 20.0f, true));
}

TEST(PlayerbotQuestPoiReachPolicyTest, TheFirstCandidateAlwaysWins)
{
    EXPECT_TRUE(QuestPoiPreferred(QuestPoiReach::DistantSafe, 2400.0f, QuestPoiReach::Local, 0.0f, false));
}

namespace
{
QuestChoiceFacts Choice(QuestPoiReach reach, uint32 questLevel, float distance)
{
    QuestChoiceFacts facts;
    facts.reach = reach;
    facts.questLevel = questLevel;
    facts.distanceYards = distance;
    return facts;
}
}  // namespace

TEST(PlayerbotQuestPoiReachPolicyTest, AQuestWorkableHereBeatsOneNeedingATrip)
{
    // The hazard this closes: admitting cross-zone objectives puts more quests in the log's
    // available set, and upstream then drew from that set at random. A bot would walk to another
    // zone while a quest it could finish on the spot went untouched.
    EXPECT_TRUE(QuestChoicePreferred(Choice(QuestPoiReach::Local, 40u, 1400.0f),
                                     Choice(QuestPoiReach::DistantSafe, 2u, 20.0f), true));
    EXPECT_FALSE(QuestChoicePreferred(Choice(QuestPoiReach::DistantSafe, 2u, 20.0f),
                                      Choice(QuestPoiReach::Local, 40u, 1400.0f), true));
}

TEST(PlayerbotQuestPoiReachPolicyTest, WithinAClassTheLowerLevelQuestGoesFirst)
{
    EXPECT_TRUE(
        QuestChoicePreferred(Choice(QuestPoiReach::Local, 5u, 900.0f), Choice(QuestPoiReach::Local, 9u, 30.0f), true));
    EXPECT_FALSE(
        QuestChoicePreferred(Choice(QuestPoiReach::Local, 9u, 30.0f), Choice(QuestPoiReach::Local, 5u, 900.0f), true));
    EXPECT_TRUE(QuestChoicePreferred(Choice(QuestPoiReach::DistantSafe, 6u, 2400.0f),
                                     Choice(QuestPoiReach::DistantSafe, 11u, 1600.0f), true));
}

TEST(PlayerbotQuestPoiReachPolicyTest, DistanceOnlyBreaksATieOnLevel)
{
    EXPECT_TRUE(
        QuestChoicePreferred(Choice(QuestPoiReach::Local, 7u, 40.0f), Choice(QuestPoiReach::Local, 7u, 800.0f), true));
    EXPECT_FALSE(
        QuestChoicePreferred(Choice(QuestPoiReach::Local, 7u, 800.0f), Choice(QuestPoiReach::Local, 7u, 40.0f), true));
    EXPECT_FALSE(
        QuestChoicePreferred(Choice(QuestPoiReach::Local, 7u, 40.0f), Choice(QuestPoiReach::Local, 7u, 40.0f), true));
}

TEST(PlayerbotQuestPoiReachPolicyTest, TheFirstQuestConsideredAlwaysWins)
{
    EXPECT_TRUE(QuestChoicePreferred(Choice(QuestPoiReach::DistantSafe, 60u, 2400.0f),
                                     Choice(QuestPoiReach::Local, 1u, 0.0f), false));
}
