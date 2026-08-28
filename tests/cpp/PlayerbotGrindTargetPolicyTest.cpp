/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include "Ai/Base/Value/GrindTargetPolicy.h"
#include "gtest/gtest.h"

namespace
{
GrindCandidateFacts Candidate(bool neededForQuest, float distance)
{
    GrindCandidateFacts facts;
    facts.neededForQuest = neededForQuest;
    facts.distance = distance;
    return facts;
}
}  // namespace

TEST(PlayerbotGrindTargetPolicyTest, TheFirstCandidateAlwaysWins)
{
    EXPECT_TRUE(GrindCandidatePreferred(Candidate(false, 40.0f), Candidate(false, 0.0f), true, false));
    EXPECT_TRUE(GrindCandidatePreferred(Candidate(false, 40.0f), Candidate(false, 0.0f), false, false));
}

TEST(PlayerbotGrindTargetPolicyTest, WorkingAQuestPrefersTheObjectiveOverTheNearerCreature)
{
    // The defect this pins: at a quest POI the objective creature is often a minority of the local
    // spawns, so a nearest-first choice keeps landing on creatures that advance nothing. Five
    // minutes of that and NewRpgDoQuestAction abandons the quest for lack of progress.
    GrindCandidateFacts const objectiveFarAway = Candidate(true, 25.0f);
    GrindCandidateFacts const bystanderUnderfoot = Candidate(false, 5.0f);

    EXPECT_TRUE(GrindCandidatePreferred(objectiveFarAway, bystanderUnderfoot, true, true));
    EXPECT_FALSE(GrindCandidatePreferred(bystanderUnderfoot, objectiveFarAway, true, true));
}

TEST(PlayerbotGrindTargetPolicyTest, NotWorkingAQuestLeavesNearestFirstExactlyAsUpstream)
{
    // Wandering and idle grinding must not change: upstream picks the nearest eligible unit and
    // this policy has to agree, or the fix would quietly redirect every bot that is not questing.
    GrindCandidateFacts const objectiveFarAway = Candidate(true, 25.0f);
    GrindCandidateFacts const bystanderUnderfoot = Candidate(false, 5.0f);

    EXPECT_FALSE(GrindCandidatePreferred(objectiveFarAway, bystanderUnderfoot, false, true));
    EXPECT_TRUE(GrindCandidatePreferred(bystanderUnderfoot, objectiveFarAway, false, true));
}

TEST(PlayerbotGrindTargetPolicyTest, WithinTheSameClassTheNearerCandidateStillWins)
{
    EXPECT_TRUE(GrindCandidatePreferred(Candidate(true, 5.0f), Candidate(true, 25.0f), true, true));
    EXPECT_FALSE(GrindCandidatePreferred(Candidate(true, 25.0f), Candidate(true, 5.0f), true, true));
    EXPECT_TRUE(GrindCandidatePreferred(Candidate(false, 5.0f), Candidate(false, 25.0f), true, true));
    EXPECT_FALSE(GrindCandidatePreferred(Candidate(false, 25.0f), Candidate(false, 5.0f), true, true));
}

TEST(PlayerbotGrindTargetPolicyTest, AnEqualDistanceDoesNotDisplaceTheIncumbent)
{
    // Strict less-than keeps the scan stable: an equal candidate must not churn the choice.
    EXPECT_FALSE(GrindCandidatePreferred(Candidate(false, 10.0f), Candidate(false, 10.0f), true, true));
    EXPECT_FALSE(GrindCandidatePreferred(Candidate(true, 10.0f), Candidate(true, 10.0f), true, true));
}
