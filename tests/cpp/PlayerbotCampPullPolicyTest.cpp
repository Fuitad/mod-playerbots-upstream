/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include <algorithm>

#include "Ai/World/Rpg/DeathProbe.h"
#include "gtest/gtest.h"

TEST(PlayerbotCampPullPolicyTest, AKillerInsideItsOwnAggroReachOfTheFirstTargetIsACampPull)
{
    // The failure this exists to name: the bot picks one creature out of a camp, its neighbours
    // join because the pull happened inside their aggro reach, and the bot dies to one it never
    // touched. Measured at 200 bots on 2026-09-02, four deaths in ten had the killer untouched.
    EXPECT_EQ(ClassifyKiller(true, false, /*distance*/ 12.0f, /*aggroRange*/ 20.0f), KillerOrigin::CampNeighbour);
    // On the boundary the neighbour still notices the pull.
    EXPECT_EQ(ClassifyKiller(true, false, 20.0f, 20.0f), KillerOrigin::CampNeighbour);
}

TEST(PlayerbotCampPullPolicyTest, AKillerOutsideThatReachArrivedFromElsewhere)
{
    // A patrol, a wanderer, or the bot's own travel dragging the fight across a spawn. Ranking
    // grind candidates on fewest neighbours cannot prevent this one, so it must not be counted
    // against that fix.
    EXPECT_EQ(ClassifyKiller(true, false, 21.0f, 20.0f), KillerOrigin::Wanderer);
    EXPECT_EQ(ClassifyKiller(true, false, 300.0f, 8.0f), KillerOrigin::Wanderer);
}

TEST(PlayerbotCampPullPolicyTest, TheCreatureTheBotChoseToFightIsNeverACampPull)
{
    // Distance is zero when the killer IS the first target, which would otherwise read as the
    // tightest possible camp pull. Losing a fight you picked is a different failure from being
    // joined by the neighbours.
    EXPECT_EQ(ClassifyKiller(true, true, 0.0f, 20.0f), KillerOrigin::FirstTarget);
    EXPECT_EQ(ClassifyKiller(true, true, 0.0f, 0.0f), KillerOrigin::FirstTarget);
}

TEST(PlayerbotCampPullPolicyTest, ADeathWithNoRecordedFightIsReportedUnknownNotGuessed)
{
    // Falling, drowning, and a death inside a fight that began before this probe was loaded. The
    // distance argument is meaningless there and must not be classified.
    EXPECT_EQ(ClassifyKiller(false, false, 0.0f, 20.0f), KillerOrigin::Unknown);
    EXPECT_EQ(ClassifyKiller(false, true, 5.0f, 20.0f), KillerOrigin::Unknown);
}

TEST(PlayerbotCampPullPolicyTest, EachOriginPrintsADistinctName)
{
    // The names are the grep keys for the death read, so they have to stay distinct and stable.
    EXPECT_STREQ(KillerOriginName(KillerOrigin::FirstTarget), "firsttarget");
    EXPECT_STREQ(KillerOriginName(KillerOrigin::CampNeighbour), "campneighbour");
    EXPECT_STREQ(KillerOriginName(KillerOrigin::Wanderer), "wanderer");
    EXPECT_STREQ(KillerOriginName(KillerOrigin::Unknown), "unknown");
}

TEST(PlayerbotCampPullPolicyTest, AnEngagementIsHeldThroughAFightAndReplacedWhenStale)
{
    // Combat is re-entered mid-fight when a second attacker joins or the bot drops out for a tick.
    // Replacing the engagement on every entry would rewrite the fight's origin to whichever
    // creature joined last, which is exactly the camp neighbour the probe is trying to identify.
    FirstEngagement none;
    EXPECT_TRUE(ShouldReplaceEngagement(none, 1000, CAMP_PULL_ENGAGEMENT_MAX_AGE_SECONDS));

    FirstEngagement held;
    held.since = 1000;
    EXPECT_FALSE(ShouldReplaceEngagement(held, 1000 + 30, CAMP_PULL_ENGAGEMENT_MAX_AGE_SECONDS));
    // A fight nobody ended: without the age cap a stale target would follow the bot into its next
    // death and report a nonsense distance.
    EXPECT_TRUE(ShouldReplaceEngagement(held, 1000 + CAMP_PULL_ENGAGEMENT_MAX_AGE_SECONDS + 1,
                                        CAMP_PULL_ENGAGEMENT_MAX_AGE_SECONDS));
}

TEST(PlayerbotCampPullPolicyTest, DeathProbeSubscribesToTheWholeFightLifecycle)
{
    std::vector<uint16> const hooks = DeathProbe::EnabledPlayerHooks();
    auto const hasHook = [&hooks](uint16 hook) { return std::find(hooks.begin(), hooks.end(), hook) != hooks.end(); };

    EXPECT_TRUE(hasHook(PLAYERHOOK_ON_PLAYER_ENTER_COMBAT));
    EXPECT_TRUE(hasHook(PLAYERHOOK_ON_PLAYER_LEAVE_COMBAT));
    EXPECT_TRUE(hasHook(PLAYERHOOK_ON_PLAYER_KILLED_BY_CREATURE));
    EXPECT_TRUE(hasHook(PLAYERHOOK_ON_PLAYER_JUST_DIED));
}

TEST(PlayerbotCampPullPolicyTest, ADeathTransitionKeepsTheEngagementForTheKillerHook)
{
    EXPECT_TRUE(ShouldClearEngagementOnLeaveCombat(true));
    EXPECT_FALSE(ShouldClearEngagementOnLeaveCombat(false));
}
