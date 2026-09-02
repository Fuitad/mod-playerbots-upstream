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

TEST(PlayerbotGrindTargetPolicyTest, ALoneCreatureOutranksOneStandingAmongOthers)
{
    // The defect this pins: at 200 bots on 2026-09-02, four deaths in ten had the killer untouched
    // at the bot's death, 18 of 30 to creatures two or more levels below the bot, 20 of 30 inside a
    // quest stay: the bot pulled one mob out of a camp and the neighbours finished it. A creature
    // with no hostile neighbour in its aggro reach is the safer pull, whatever the distance.
    GrindCandidateFacts lone = Candidate(false, 20.0f);
    GrindCandidateFacts crowded = Candidate(false, 5.0f);
    crowded.hostileNeighbours = 2;

    EXPECT_TRUE(GrindCandidatePreferred(lone, crowded, true, true));
    EXPECT_FALSE(GrindCandidatePreferred(crowded, lone, true, true));
    // Fewer neighbours also wins over more.
    GrindCandidateFacts lessCrowded = Candidate(false, 25.0f);
    lessCrowded.hostileNeighbours = 1;
    EXPECT_TRUE(GrindCandidatePreferred(lessCrowded, crowded, true, true));
    // The quest objective still outranks a lone bystander: the stay exists to progress the quest.
    GrindCandidateFacts crowdedObjective = Candidate(true, 15.0f);
    crowdedObjective.hostileNeighbours = 2;
    EXPECT_TRUE(GrindCandidatePreferred(crowdedObjective, lone, true, true));
    // Off a quest the same preference holds: a wandering bot should not pull camps either.
    EXPECT_TRUE(GrindCandidatePreferred(lone, crowded, false, true));
}

TEST(PlayerbotGrindTargetPolicyTest, AnEqualDistanceDoesNotDisplaceTheIncumbent)
{
    // Strict less-than keeps the scan stable: an equal candidate must not churn the choice.
    EXPECT_FALSE(GrindCandidatePreferred(Candidate(false, 10.0f), Candidate(false, 10.0f), true, true));
    EXPECT_FALSE(GrindCandidatePreferred(Candidate(true, 10.0f), Candidate(true, 10.0f), true, true));
}

TEST(PlayerbotGrindTargetPolicyTest, DuringAPoiStayAnyEligibleCandidateMayBeGround)
{
    // The defect this pins: bots stood idle at their POIs for whole five-minute stays (71% of
    // abandons with zero kills) because every out-of-aggro bystander was filtered as not
    // quest-relevant. Once the stay is running, the relevance demand must lift.
    EXPECT_FALSE(GrindCandidateNeedsQuestRelevance(true, true, true));
}

TEST(PlayerbotGrindTargetPolicyTest, TravellingToThePoiStillDemandsQuestRelevance)
{
    // Before the stay starts (still walking to the POI) upstream's focus rule stands: an
    // out-of-aggro candidate must advance the quest, or the bot detours off its journey.
    EXPECT_TRUE(GrindCandidateNeedsQuestRelevance(true, true, false));
}

TEST(PlayerbotGrindTargetPolicyTest, ActiveGrindingAndInAggroCandidatesNeverNeededRelevance)
{
    // Upstream never demanded relevance for wander/idle grinding or for candidates inside aggro
    // range; the stay exemption must not change either.
    EXPECT_FALSE(GrindCandidateNeedsQuestRelevance(false, true, false));
    EXPECT_FALSE(GrindCandidateNeedsQuestRelevance(true, false, false));
}

TEST(PlayerbotGrindTargetPolicyTest, AGrayCreatureTheQuestNeedsStaysEligible)
{
    // The defect this pins: Skirmish at Echo Ridge at bot level 10, Kobold Laborers level 3 give
    // no experience, and the XP gate dropped every one of them before the quest was consulted.
    EXPECT_TRUE(GrindCandidateGrayEligible(false, true));
    EXPECT_FALSE(GrindCandidateGrayEligible(false, false));
    EXPECT_TRUE(GrindCandidateGrayEligible(true, false));
}
