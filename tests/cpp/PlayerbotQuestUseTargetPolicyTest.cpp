/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include "Ai/World/Rpg/QuestUseTargetPolicy.h"
#include "gtest/gtest.h"

#include <algorithm>

namespace
{
QuestUseCandidateFacts Candidate(bool matchesEntry, bool alive, float distanceSq, float anchorDistanceSq = 0.0f,
                                 bool sleeping = false)
{
    QuestUseCandidateFacts facts;
    facts.matchesEntry = matchesEntry;
    facts.alive = alive;
    facts.distanceSq = distanceSq;
    facts.anchorDistanceSq = anchorDistanceSq;
    facts.sleeping = sleeping;
    return facts;
}

constexpr float NoAnchorCap = 0.0f;
}  // namespace

TEST(PlayerbotQuestUseTargetPolicyTest, TheProvidedItemOutranksAKnownSpellAndNoToolMeansKill)
{
    // A quest that hands the bot a tool item is credited through that item; a spell quest has no
    // item and needs the known racial; a quest with neither is a genuine kill quest and must be
    // left to the grind strategy untouched.
    EXPECT_EQ(QuestUseModeForFacts(true, true), QuestUseMode::Item);
    EXPECT_EQ(QuestUseModeForFacts(true, false), QuestUseMode::Item);
    EXPECT_EQ(QuestUseModeForFacts(false, true), QuestUseMode::Spell);
    EXPECT_EQ(QuestUseModeForFacts(false, false), QuestUseMode::None);
}

TEST(PlayerbotQuestUseTargetPolicyTest, SpellQuestsCarryTheirPerClassSpellFamilies)
{
    // Rescue the Survivors! is credited by any class's Gift of the Naaru; Thirst Unending by
    // Mana Tap. A quest outside the table has no spell tool at all.
    std::vector<uint32> const rescue = QuestUseSpellsForQuest(9283);
    EXPECT_NE(std::find(rescue.begin(), rescue.end(), 59544u), rescue.end());  // priest variant
    EXPECT_NE(std::find(rescue.begin(), rescue.end(), 28880u), rescue.end());  // original variant
    std::vector<uint32> const thirst = QuestUseSpellsForQuest(8346);
    ASSERT_EQ(thirst.size(), 1u);
    EXPECT_EQ(thirst[0], 28734u);
    EXPECT_TRUE(QuestUseSpellsForQuest(5441).empty());  // Lazy Peons is item-credited, not spell
}

TEST(PlayerbotQuestUseTargetPolicyTest, TheNearestLivingMatchingCreatureWins)
{
    std::vector<QuestUseCandidateFacts> const candidates = {
        Candidate(true, true, 900.0f),
        Candidate(false, true, 1.0f),   // nearer, but the wrong creature
        Candidate(true, false, 4.0f),   // nearer and matching, but dead
        Candidate(true, true, 25.0f),
    };

    EXPECT_EQ(BestQuestUseTargetIndex(candidates, NoAnchorCap), 3u);
}

TEST(PlayerbotQuestUseTargetPolicyTest, ACandidateBeyondBothRadiiIsIgnored)
{
    // The cap bounds the seek from two reference points (see QuestUseCandidateInRange): a
    // candidate far from the POI anchor AND far from the bot would drag the bot clean off the
    // quest area, so only the in-range one may win, even though the far one is nearer the bot.
    float const maxAnchorDistanceSq = 75.0f * 75.0f;
    std::vector<QuestUseCandidateFacts> const candidates = {
        Candidate(true, true, 80.0f * 80.0f, 90.0f * 90.0f),
        Candidate(true, true, 85.0f * 85.0f, 40.0f * 40.0f),
    };

    EXPECT_EQ(BestQuestUseTargetIndex(candidates, maxAnchorDistanceSq), 1u);
    EXPECT_EQ(BestQuestUseTargetIndex(candidates, NoAnchorCap), 0u);
}

TEST(PlayerbotQuestUseTargetPolicyTest, NoCandidatesMeansNoChoice)
{
    EXPECT_EQ(BestQuestUseTargetIndex({}, NoAnchorCap), QUEST_USE_NO_CANDIDATE);
    std::vector<QuestUseCandidateFacts> const onlyForeign = {Candidate(false, true, 1.0f)};
    EXPECT_EQ(BestQuestUseTargetIndex(onlyForeign, NoAnchorCap), QUEST_USE_NO_CANDIDATE);
}

TEST(PlayerbotQuestUseTargetPolicyTest, ASleepingTargetOutranksANearerAwakeOne)
{
    // The defect this pins: the Foreman's Blackjack credits only a sleeping Lazy Peon, and the
    // seek kept sending bots to the nearest awake one - 15 wasted whacks measured live before a
    // single credit landed.
    std::vector<QuestUseCandidateFacts> const candidates = {
        Candidate(true, true, 4.0f),                        // awake, nearest
        Candidate(true, true, 2500.0f, 0.0f, true),         // sleeping, far
        Candidate(true, true, 900.0f, 0.0f, true),          // sleeping, nearer
    };

    EXPECT_EQ(BestQuestUseTargetIndex(candidates, NoAnchorCap), 2u);
}

TEST(PlayerbotQuestUseTargetPolicyTest, NoSleeperFallsBackToTheNearestAwakeTarget)
{
    // Scripts without a sleep requirement (Inoculation owlkin never sleep) must keep working
    // exactly as before.
    std::vector<QuestUseCandidateFacts> const candidates = {
        Candidate(true, true, 900.0f),
        Candidate(true, true, 25.0f),
    };

    EXPECT_EQ(BestQuestUseTargetIndex(candidates, NoAnchorCap), 1u);
}

TEST(PlayerbotQuestUseTargetPolicyTest, ACreditDummyObjectiveAcceptsTheToolSpellsWorldTarget)
{
    // The defect this pins: Inoculation (9303) requires credit dummy 16534, which never stands
    // in the world; the crystal's spell is conditioned onto live entry 16518. Matching only the
    // required entry produced zero candidates through whole stays (three blamed abandons measured
    // live 2026-08-30). Both entries must be accepted, the dummy first.
    std::vector<uint32> const accepted = QuestUseAcceptedEntries(16534, {16518});

    ASSERT_EQ(accepted.size(), 2u);
    EXPECT_EQ(accepted[0], 16534u);
    EXPECT_EQ(accepted[1], 16518u);
}

TEST(PlayerbotQuestUseTargetPolicyTest, AWorldEntryObjectiveWithoutSpellTargetsIsUnchanged)
{
    // Lazy Peons (5441) requires the live entry 10556 directly and its tool spell carries no
    // target conditions; the accepted set must stay exactly the required entry.
    std::vector<uint32> const accepted = QuestUseAcceptedEntries(10556, {});

    ASSERT_EQ(accepted.size(), 1u);
    EXPECT_EQ(accepted[0], 10556u);
}

TEST(PlayerbotQuestUseTargetPolicyTest, AcceptedEntriesDropDuplicatesAndZeroes)
{
    // A spell conditioned onto the same entry the quest already requires, or carrying a zeroed
    // row, must not inflate the set.
    std::vector<uint32> const accepted = QuestUseAcceptedEntries(16518, {0, 16518, 16519, 16519});

    ASSERT_EQ(accepted.size(), 2u);
    EXPECT_EQ(accepted[0], 16518u);
    EXPECT_EQ(accepted[1], 16519u);
}

TEST(PlayerbotQuestUseTargetPolicyTest, ATargetBesideTheBotSurvivesTheAnchorCap)
{
    // The defect this pins: Abaka stood beside a live owlkin (usecand 6/1/1 measured live
    // 2026-08-30) yet burned a 316s stay with zero engagements, because the cap was measured
    // only from the assigned POI point and the bot had drifted 45y chasing combat: a creditable
    // target 10y from the bot sat 100y from the anchor and was filtered every tick. A candidate
    // within the cap of EITHER the anchor or the bot must be accepted.
    std::vector<QuestUseCandidateFacts> const candidates = {
        Candidate(true, true, /*distanceSq*/ 100.0f, /*anchorDistanceSq*/ 10000.0f),
    };

    EXPECT_EQ(BestQuestUseTargetIndex(candidates, /*maxAnchorDistanceSq*/ 5625.0f), 0u);
}

TEST(PlayerbotQuestUseTargetPolicyTest, ATargetFarFromBothBotAndAnchorStaysFiltered)
{
    // The cap still has to bound the seek: a candidate beyond it from both reference points
    // would drag the bot clean off the quest area.
    std::vector<QuestUseCandidateFacts> const candidates = {
        Candidate(true, true, 10000.0f, 10000.0f),
    };

    EXPECT_EQ(BestQuestUseTargetIndex(candidates, 5625.0f), QUEST_USE_NO_CANDIDATE);
}
