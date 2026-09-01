/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include "Ai/World/Rpg/QuestBlacklistPolicy.h"
#include "Ai/World/Rpg/QuestDropPolicy.h"
#include "Ai/World/Rpg/QuestPickPolicy.h"
#include "gtest/gtest.h"

namespace
{
QuestDropFacts Facts(uint8 botLevel, int32 questLevel, bool complete, bool givenUp, bool reachable)
{
    QuestDropFacts facts;
    facts.botLevel = botLevel;
    facts.questLevel = questLevel;
    facts.complete = complete;
    facts.givenUp = givenUp;
    facts.reachable = reachable;
    return facts;
}
}  // namespace

// Expected gray boundaries are hand-derived from the formula spec in Formulas.h:
// level <= 5 -> 0; 6..39 -> L - 5 - L/10; 40..59 -> L - 1 - L/5; 60+ -> L - 9.
TEST(PlayerbotQuestDropPolicyTest, GrayBoundaryMatchesTheCoreFormula)
{
    // Bot 30: gray at 30 - 5 - 3 = 22.
    EXPECT_TRUE(QuestIsGrayFor(30, 22));
    EXPECT_FALSE(QuestIsGrayFor(30, 23));
    // Bot 10: gray at 10 - 5 - 1 = 4.
    EXPECT_TRUE(QuestIsGrayFor(10, 4));
    EXPECT_FALSE(QuestIsGrayFor(10, 5));
    // Bot 45: gray at 45 - 1 - 9 = 35.
    EXPECT_TRUE(QuestIsGrayFor(45, 35));
    EXPECT_FALSE(QuestIsGrayFor(45, 36));
    // Bot 80: gray at 80 - 9 = 71.
    EXPECT_TRUE(QuestIsGrayFor(80, 71));
    EXPECT_FALSE(QuestIsGrayFor(80, 72));
}

TEST(PlayerbotQuestDropPolicyTest, LowLevelBotsAndScalingQuestsAreNeverGray)
{
    // Bot 5 grays at level 0, and quest levels start at 1.
    EXPECT_FALSE(QuestIsGrayFor(5, 1));
    // Level <= 0 marks a quest that scales with the player.
    EXPECT_FALSE(QuestIsGrayFor(80, 0));
    EXPECT_FALSE(QuestIsGrayFor(80, -1));
}

TEST(PlayerbotQuestDropPolicyTest, ACompleteQuestIsNeverDropped)
{
    // Ready to turn in, however gray, given up, and unreachable it is.
    EXPECT_EQ(QuestDropDecision(Facts(80, 10, true, true, false)), QuestDropVerdict::Keep);
    EXPECT_EQ(QuestDropDecision(Facts(80, 10, true, false, false)), QuestDropVerdict::Keep);
}

TEST(PlayerbotQuestDropPolicyTest, AQuestStillDoableAtLevelIsNeverDropped)
{
    // Not gray: neither the give-up record nor unreachability alone may drop it.
    EXPECT_EQ(QuestDropDecision(Facts(30, 28, false, true, true)), QuestDropVerdict::Keep);
    EXPECT_EQ(QuestDropDecision(Facts(30, 28, false, false, false)), QuestDropVerdict::Keep);
    EXPECT_EQ(QuestDropDecision(Facts(30, 28, false, true, false)), QuestDropVerdict::Keep);
}

TEST(PlayerbotQuestDropPolicyTest, AGrayGivenUpQuestIsDropped)
{
    EXPECT_EQ(QuestDropDecision(Facts(30, 15, false, true, true)), QuestDropVerdict::DropGivenUp);
    // Given up wins as the reason even when the quest is also unreachable.
    EXPECT_EQ(QuestDropDecision(Facts(30, 15, false, true, false)), QuestDropVerdict::DropGivenUp);
}

TEST(PlayerbotQuestDropPolicyTest, AGrayUnreachableQuestIsDropped)
{
    EXPECT_EQ(QuestDropDecision(Facts(30, 15, false, false, false)), QuestDropVerdict::DropUnreachable);
}

TEST(PlayerbotQuestDropPolicyTest, AGrayButWorkableQuestIsKept)
{
    // Gray alone is not enough: the bot never gave up and can still reach the objectives.
    EXPECT_EQ(QuestDropDecision(Facts(30, 15, false, false, true)), QuestDropVerdict::Keep);
}

TEST(PlayerbotQuestDropPolicyTest, ReachabilityIsOnlyConsultedWhenItCanChangeTheVerdict)
{
    // Only an incomplete, gray, not-given-up quest needs the POI computation.
    EXPECT_TRUE(QuestDropNeedsReachability(Facts(30, 15, false, false, true)));
    // Complete, given up, or still at level: the verdict is already settled without it.
    EXPECT_FALSE(QuestDropNeedsReachability(Facts(30, 15, true, false, true)));
    EXPECT_FALSE(QuestDropNeedsReachability(Facts(30, 15, false, true, true)));
    EXPECT_FALSE(QuestDropNeedsReachability(Facts(30, 28, false, false, true)));
}

TEST(PlayerbotQuestDropPolicyTest, ReasonNamesFeedTheDropProbeLine)
{
    EXPECT_STREQ(QuestDropReasonName(QuestDropVerdict::DropGivenUp), "givenup");
    EXPECT_STREQ(QuestDropReasonName(QuestDropVerdict::DropUnreachable), "unreachable");
    EXPECT_STREQ(QuestDropReasonName(QuestDropVerdict::Keep), "keep");
}

TEST(PlayerbotQuestDropPolicyTest, ProgressAtStayEndOutranksInteractionCount)
{
    EXPECT_EQ(QuestStayEndDecision(true, 0), QuestStayEndVerdict::Progressed);
    EXPECT_EQ(QuestStayEndDecision(true, 7), QuestStayEndVerdict::Progressed);
}

TEST(PlayerbotQuestDropPolicyTest, AFruitlessStayThatDispatchedInteractionsRotatesWithoutBlame)
{
    // The Faillicie case: 115 blackjack uses in one stay, zero credit because only sleeping
    // peons credit and the one in range was contested. The quest must stay eligible.
    EXPECT_EQ(QuestStayEndDecision(false, 1), QuestStayEndVerdict::RotateWithoutBlame);
    EXPECT_EQ(QuestStayEndDecision(false, 115), QuestStayEndVerdict::RotateWithoutBlame);
}

TEST(PlayerbotQuestDropPolicyTest, AFruitlessStayWithNoInteractionAttemptIsAbandoned)
{
    // A stay with nothing to show still means this place cannot progress the quest.
    EXPECT_EQ(QuestStayEndDecision(false, 0), QuestStayEndVerdict::Abandon);
}

TEST(PlayerbotQuestDropPolicyTest, KillsOfTheObjectivesOwnSourcesCountAsTrying)
{
    // The drop-luck class, measured live 2026-08-30: Beringaer killed 18 of quest 6394's own
    // drop-source mobs in one stay and the required item never dropped (counter 0), yet the
    // stay abandoned as if untried. Killing the right creatures IS trying; only the drop roll
    // failed, so the quest must stay eligible.
    EXPECT_EQ(QuestStayEndDecision(false, 0, 18), QuestStayEndVerdict::RotateWithoutBlame);
    EXPECT_EQ(QuestStayEndDecision(false, 0, 1), QuestStayEndVerdict::RotateWithoutBlame);
}

TEST(PlayerbotQuestDropPolicyTest, BystanderKillsStillDoNotExcuseAnUntriedStay)
{
    // Only kills of the objective's OWN source entries reach the third parameter; a stay spent
    // fighting unrelated mobs next to an untouched objective (the old 9303 pattern, 9 bystander
    // kills, zero credit) still abandons.
    EXPECT_EQ(QuestStayEndDecision(false, 0, 0), QuestStayEndVerdict::Abandon);
}

TEST(PlayerbotQuestDropPolicyTest, ASightedCandidateCountsAsTrying)
{
    // The contention class, measured live 2026-08-30: Jdyalani's Webwood Egg stay ended with six
    // usable in-range eggs (gocand 6/6/6/6) and zero recorded interactions, because the eggs were
    // farmed out during the stay and the approach never converged before they vanished. A seek
    // that RETURNED a candidate at any point proves the place can progress the quest; losing the
    // race for it is not evidence to abandon on.
    EXPECT_EQ(QuestStayEndDecision(false, 0, 0, 1), QuestStayEndVerdict::RotateWithoutBlame);
    EXPECT_EQ(QuestStayEndDecision(false, 0, 0, 40), QuestStayEndVerdict::RotateWithoutBlame);
}

TEST(PlayerbotQuestDropPolicyTest, NoSightingNoKillNoInteractionStillAbandons)
{
    EXPECT_EQ(QuestStayEndDecision(false, 0, 0, 0), QuestStayEndVerdict::Abandon);
}

TEST(PlayerbotQuestDropPolicyTest, TheLowestLevelQuestsArePickedFirst)
{
    // Muzeze carried a level-5 quest to level 9 while the picker kept drawing his higher-level
    // quests uniformly; the gray-drop policy then retired it untried. Low first, ties together.
    std::vector<size_t> const lowest = LowestLevelQuestIndices({10, 5, 8, 5, 12});
    ASSERT_EQ(lowest.size(), 2u);
    EXPECT_EQ(lowest[0], 1u);
    EXPECT_EQ(lowest[1], 3u);
}

TEST(PlayerbotQuestDropPolicyTest, ScalingQuestsSortLastButAloneStayPickable)
{
    // A quest level <= 0 scales with the player and can never be outleveled.
    std::vector<size_t> const mixed = LowestLevelQuestIndices({-1, 7, 0});
    ASSERT_EQ(mixed.size(), 1u);
    EXPECT_EQ(mixed[0], 1u);

    std::vector<size_t> const onlyScaling = LowestLevelQuestIndices({-1, 0});
    ASSERT_EQ(onlyScaling.size(), 2u);
}

TEST(PlayerbotQuestDropPolicyTest, BlacklistedQuestsAreNeverWorthDoing)
{
    // Red Snapper - Very Tasty! needs a fishing net used on transient fishing-pool schools,
    // which no seek models; Pierre blacklisted it on 2026-08-30. Everything else stays open.
    EXPECT_TRUE(QuestIsRpgBlacklisted(9452));
    EXPECT_TRUE(QuestIsRpgBlacklisted(9067));  // The Party Never Ends: multi-hub delivery chase
    EXPECT_TRUE(QuestIsRpgBlacklisted(8346));  // Thirst Unending: unresolvable Mana Tap credit dummy
    EXPECT_TRUE(QuestIsRpgBlacklisted(746));   // Dwarven Digging: two-step tool-use-at-anvil mechanic
    EXPECT_TRUE(QuestIsRpgBlacklisted(921));   // Crown of the Earth: use provided item at the moonwell
    EXPECT_TRUE(QuestIsRpgBlacklisted(310));   // Bitter Rivals: ender barrel only exists while scripted
    EXPECT_TRUE(QuestIsRpgBlacklisted(2399));  // The Sprouted Fronds: same event-spawned ender class
    // Shaman totem chains: redundant reward, cross-zone report-back steps (Pierre, 2026-09-01).
    EXPECT_TRUE(QuestIsRpgBlacklisted(1518));   // Call of Earth, orc/troll
    EXPECT_TRUE(QuestIsRpgBlacklisted(9449));   // Call of Earth, draenei, ends in the Exodar
    EXPECT_TRUE(QuestIsRpgBlacklisted(10491));  // Call of Air, the last id of the family
    EXPECT_FALSE(QuestIsRpgBlacklisted(1515));  // Dogran's Captivity, the id just below the family
    EXPECT_FALSE(QuestIsRpgBlacklisted(9303));
    EXPECT_FALSE(QuestIsRpgBlacklisted(0));
}
