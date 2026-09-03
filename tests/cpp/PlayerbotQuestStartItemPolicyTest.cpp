/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include "Ai/World/Rpg/QuestStartItemPolicy.h"
#include "gtest/gtest.h"

namespace
{
QuestStartItemFacts Eligible()
{
    QuestStartItemFacts facts;
    facts.questExists = true;
    facts.canTakeQuest = true;
    facts.canAddQuest = true;
    return facts;
}
}  // namespace

TEST(PlayerbotQuestStartItemPolicyTest, AnEligibleItemIsUsedAtOnce)
{
    // Maisie carrying "A Letter to Yvette" (item 2839, starts quest 361, min level 4) is the case
    // this exists for: the item was looted, the quest was never taken, and the slot never came back.
    EXPECT_EQ(QuestStartItemDecision(Eligible()), QuestStartItemVerdict::Use);
}

TEST(PlayerbotQuestStartItemPolicyTest, AFullQuestLogKeepsTheItemForLater)
{
    // Pierre's one exception. The bot is otherwise eligible, so the item stays in the bag and is
    // used once a slot frees, rather than being reported as ineligible and forgotten.
    QuestStartItemFacts facts = Eligible();
    facts.canAddQuest = false;
    EXPECT_EQ(QuestStartItemDecision(facts), QuestStartItemVerdict::LogFull);
}

TEST(PlayerbotQuestStartItemPolicyTest, AnItemForAQuestAlreadyTakenOrRewardedIsRedundant)
{
    // Distinct from ineligible: nothing will ever make these usable, so a caller can treat them as
    // vendorable dead weight instead of holding them against a future level.
    QuestStartItemFacts taken = Eligible();
    taken.alreadyOnQuest = true;
    EXPECT_EQ(QuestStartItemDecision(taken), QuestStartItemVerdict::Redundant);

    QuestStartItemFacts rewarded = Eligible();
    rewarded.alreadyRewarded = true;
    EXPECT_EQ(QuestStartItemDecision(rewarded), QuestStartItemVerdict::Redundant);
}

TEST(PlayerbotQuestStartItemPolicyTest, AnUnderlevelledBotKeepsTheItemRatherThanReportingAFullLog)
{
    // Ordering matters: eligibility is judged before log space, so a level-8 quest on a level-5 bot
    // reads as "not yet", not as "log full". Reversing the two would blame the log for a level gap
    // and send someone looking at quest slots.
    QuestStartItemFacts facts = Eligible();
    facts.canTakeQuest = false;
    facts.canAddQuest = false;
    EXPECT_EQ(QuestStartItemDecision(facts), QuestStartItemVerdict::NotEligibleYet);
}

TEST(PlayerbotQuestStartItemPolicyTest, AnItemPointingAtNoQuestIsNamedSeparately)
{
    // A dangling StartQuest is data damage, not a bot decision, and should not be silently counted
    // with the items a bot is legitimately holding.
    QuestStartItemFacts facts;
    facts.canTakeQuest = true;
    facts.canAddQuest = true;
    EXPECT_EQ(QuestStartItemDecision(facts), QuestStartItemVerdict::NoSuchQuest);
}

TEST(PlayerbotQuestStartItemPolicyTest, EveryVerdictPrintsADistinctName)
{
    EXPECT_STREQ(QuestStartItemVerdictName(QuestStartItemVerdict::Use), "use");
    EXPECT_STREQ(QuestStartItemVerdictName(QuestStartItemVerdict::LogFull), "logfull");
    EXPECT_STREQ(QuestStartItemVerdictName(QuestStartItemVerdict::NotEligibleYet), "noteligible");
    EXPECT_STREQ(QuestStartItemVerdictName(QuestStartItemVerdict::Redundant), "redundant");
    EXPECT_STREQ(QuestStartItemVerdictName(QuestStartItemVerdict::NoSuchQuest), "nosuchquest");
}
