/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include "Ai/World/Rpg/QuestGameObjectPolicy.h"
#include "gtest/gtest.h"

namespace
{
QuestGoCandidateFacts Candidate(bool usable, bool matchesObjective, QuestGoInteraction interaction,
                                float distanceSq, float anchorDistanceSq = 0.0f)
{
    QuestGoCandidateFacts facts;
    facts.usable = usable;
    facts.matchesObjective = matchesObjective;
    facts.interaction = interaction;
    facts.distanceSq = distanceSq;
    facts.anchorDistanceSq = anchorDistanceSq;
    return facts;
}

constexpr float NoAnchorCap = 0.0f;
}  // namespace

TEST(PlayerbotQuestGameObjectPolicyTest, OnlyANegativeRequiredNpcOrGoNamesAGameObjectEntry)
{
    // quest_template sign convention (see Player::HasQuestForGO): < 0 is a gameobject entry,
    // > 0 is a creature to kill, 0 is an empty slot.
    EXPECT_EQ(QuestObjectiveGoEntry(-1731), 1731u);
    EXPECT_EQ(QuestObjectiveGoEntry(0), 0u);
    EXPECT_EQ(QuestObjectiveGoEntry(1731), 0u);
}

TEST(PlayerbotQuestGameObjectPolicyTest, GoobersAreUsedChestsAreLootedEverythingElseIsSkipped)
{
    // Goobers have no loot id, so only a direct use can grant KillCreditGO; chests must go
    // through the loot pipeline, which owns locks, skills and bag space. Any other type is
    // outside this fix and must stay untouched.
    EXPECT_EQ(QuestGoInteractionForType(true, false), QuestGoInteraction::Use);
    EXPECT_EQ(QuestGoInteractionForType(false, true), QuestGoInteraction::Loot);
    EXPECT_EQ(QuestGoInteractionForType(false, false), QuestGoInteraction::Skip);
}

TEST(PlayerbotQuestGameObjectPolicyTest, NoCandidatesMeansNoChoice)
{
    EXPECT_EQ(BestQuestGoCandidateIndex({}, NoAnchorCap), QUEST_GO_NO_CANDIDATE);
}

TEST(PlayerbotQuestGameObjectPolicyTest, TheNearestMatchingUsableCandidateWins)
{
    // The defect this pins: with several matching gameobjects in the POI area the bot should
    // walk to the closest one, not scan-order-first, or it zigzags across the whole field.
    std::vector<QuestGoCandidateFacts> const candidates = {
        Candidate(true, true, QuestGoInteraction::Use, 900.0f),
        Candidate(true, true, QuestGoInteraction::Use, 25.0f),
        Candidate(true, true, QuestGoInteraction::Loot, 100.0f),
    };

    EXPECT_EQ(BestQuestGoCandidateIndex(candidates, NoAnchorCap), 1u);
}

TEST(PlayerbotQuestGameObjectPolicyTest, UnusableSkippedOrForeignCandidatesNeverWin)
{
    // A despawned or in-use object, a type this fix does not operate, and an object for some
    // other objective must all lose to a farther legitimate candidate - and when only they
    // remain, the answer is "no candidate", which leaves upstream behaviour untouched.
    std::vector<QuestGoCandidateFacts> const losers = {
        Candidate(false, true, QuestGoInteraction::Use, 1.0f),   // not usable (despawned/in use)
        Candidate(true, false, QuestGoInteraction::Use, 1.0f),   // does not advance the objective
        Candidate(true, true, QuestGoInteraction::Skip, 1.0f),   // type this fix does not touch
    };

    EXPECT_EQ(BestQuestGoCandidateIndex(losers, NoAnchorCap), QUEST_GO_NO_CANDIDATE);

    std::vector<QuestGoCandidateFacts> withWinner = losers;
    withWinner.push_back(Candidate(true, true, QuestGoInteraction::Use, 2500.0f));

    EXPECT_EQ(BestQuestGoCandidateIndex(withWinner, NoAnchorCap), 3u);
}

TEST(PlayerbotQuestGameObjectPolicyTest, ACandidateBeyondThePoiAnchorRadiusIsIgnored)
{
    // The anchor cap exists so the seek never fights QuestPoiNeedsApproach's return radius:
    // a gameobject close to the bot but far from the assigned POI would drag the bot out,
    // whereupon the approach policy drags it back, forever.
    float const maxAnchorDistanceSq = 75.0f * 75.0f;

    std::vector<QuestGoCandidateFacts> const candidates = {
        Candidate(true, true, QuestGoInteraction::Use, 25.0f, 90.0f * 90.0f),    // near bot, off anchor
        Candidate(true, true, QuestGoInteraction::Use, 2500.0f, 40.0f * 40.0f),  // far from bot, on anchor
    };

    EXPECT_EQ(BestQuestGoCandidateIndex(candidates, maxAnchorDistanceSq), 1u);

    // A zero (or negative) cap disables the anchor filter entirely.
    EXPECT_EQ(BestQuestGoCandidateIndex(candidates, NoAnchorCap), 0u);
}

TEST(PlayerbotQuestGameObjectPolicyTest, AnEqualDistanceDoesNotDisplaceTheIncumbent)
{
    // Strict less-than keeps the scan stable: an equal candidate must not churn the choice
    // between ticks, or the bot oscillates between two equidistant objects.
    std::vector<QuestGoCandidateFacts> const candidates = {
        Candidate(true, true, QuestGoInteraction::Use, 100.0f),
        Candidate(true, true, QuestGoInteraction::Use, 100.0f),
    };

    EXPECT_EQ(BestQuestGoCandidateIndex(candidates, NoAnchorCap), 0u);
}
