/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option), any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * Local additions live here by preference precisely for that reason: every symbol this file owns
 * is one that an upstream pull cannot touch. Prefer adding to a file like this over editing an
 * upstream one, and keep the edit in the upstream file down to the call that reaches in here.
 */

#include "Ai/Base/Actions/AcceptResurrectAction.h"
#include "Ai/Base/Actions/CorpseWalkPolicy.h"
#include "Ai/Base/Actions/DeathRecoveryPolicy.h"
#include "Ai/Base/Actions/GenericActions.h"
#include "Ai/Base/Actions/PhysicalDeathCountPolicy.h"
#include "Ai/Base/Actions/PullActions.h"
#include "Ai/Base/Actions/RepairAllAction.h"
#include "Ai/Base/Actions/ReviveFromCorpseAction.h"
#include "Bot/Engine/AiObjectContext.h"
#include "Bot/PlayerbotMgr.h"
#include "Bot/Recovery/PlayerbotRecoveryPolicy.h"
#include "Corpse.h"
#include "IntegrationTestFixture.h"
#include "PlayerbotAIConfig.h"
#include "gtest/gtest.h"

namespace
{
using playerbots::recovery::CorpseReclaimEligibility;

class PlayerbotRecoveryActionTest : public IntegrationTestFixture
{
protected:
    void SetUp() override
    {
        IntegrationTestFixture::SetUp();
        static bool contextsBuilt = false;
        if (!contextsBuilt)
        {
            AiObjectContext::BuildAllSharedContexts();
            contextsBuilt = true;
        }

        originalEnabled = sPlayerbotAIConfig.enabled;
        sPlayerbotAIConfig.enabled = true;
        bot = CreateTestPlayer(301, "RecoveryBot");
        sPlayerbotsMgr.AddPlayerbotData(bot, true);
        botAI = sPlayerbotsMgr.GetPlayerbotAI(bot);
        ASSERT_NE(botAI, nullptr);
    }

    void TearDown() override
    {
        delete botAI;
        sPlayerbotAIConfig.enabled = originalEnabled;
        IntegrationTestFixture::TearDown();
    }

    TestPlayer* bot = nullptr;
    PlayerbotAI* botAI = nullptr;
    bool originalEnabled = false;
};

class CombatBlockedPlayerbotAI : public PlayerbotAI
{
public:
    bool CanInitiateCombat() override
    {
        ++checks;
        return false;
    }

    uint32 checks = 0;
};

class TestPullRequestAction : public PullRequestAction
{
public:
    explicit TestPullRequestAction(PlayerbotAI* botAI) : PullRequestAction(botAI, "test pull request") {}

private:
    Unit* GetPullTarget(Event) override { return nullptr; }
};

class TestRepairAllAction : public RepairAllAction
{
public:
    explicit TestRepairAllAction(PlayerbotAI* botAI) : RepairAllAction(botAI) {}

private:
    bool ExecutePaidRepair() override { return true; }
};

class TestFindCorpseAction : public FindCorpseAction
{
public:
    TestFindCorpseAction(PlayerbotAI* botAI, bool recoveryAccepted)
        : FindCorpseAction(botAI), recoveryAccepted(recoveryAccepted)
    {
    }

private:
    Corpse* GetBotCorpse() const override { return const_cast<Corpse*>(&corpse); }
    bool RecoverAtHomebind() override { return recoveryAccepted; }

    mutable Corpse corpse;
    bool recoveryAccepted;
};

TEST(PlayerbotRecoveryPolicyTest, CorpseReclaimRequiresEveryCoreEligibilityCondition)
{
    CorpseReclaimEligibility eligible{
        .playerAlive = false,
        .inArena = false,
        .ghost = true,
        .hasCorpse = true,
        .reclaimDelayElapsed = true,
        .corpseInMap = true,
        .withinReclaimRadius = true,
    };
    EXPECT_TRUE(playerbots::recovery::CanReclaimCorpse(eligible));

    eligible.reclaimDelayElapsed = false;
    EXPECT_FALSE(playerbots::recovery::CanReclaimCorpse(eligible));
    eligible.reclaimDelayElapsed = true;
    eligible.corpseInMap = false;
    EXPECT_FALSE(playerbots::recovery::CanReclaimCorpse(eligible));
    eligible.corpseInMap = true;
    eligible.withinReclaimRadius = false;
    EXPECT_FALSE(playerbots::recovery::CanReclaimCorpse(eligible));
    eligible.withinReclaimRadius = true;
    eligible.ghost = false;
    EXPECT_FALSE(playerbots::recovery::CanReclaimCorpse(eligible));
}

TEST(PlayerbotRecoveryPolicyTest, RejectedAndSuccessfulAttemptsRemainTruthful)
{
    PlayerbotReviveAttemptTracker tracker;

    PlayerbotReviveAttemptSnapshot snapshot = tracker.Inspect();
    EXPECT_FALSE(snapshot.available);
    EXPECT_EQ(snapshot.currentDeathGeneration, 0u);
    EXPECT_FALSE(snapshot.currentCycle);

    tracker.RecordPhysicalDeath();
    tracker.Record(1000, PlayerbotReviveOutcome::Failed, false);
    snapshot = tracker.Inspect();
    EXPECT_TRUE(snapshot.available);
    EXPECT_EQ(snapshot.timestampMs, 1000u);
    EXPECT_EQ(snapshot.outcome, PlayerbotReviveOutcome::Failed);
    EXPECT_FALSE(snapshot.success);
    EXPECT_FALSE(snapshot.aliveAfter);
    EXPECT_EQ(snapshot.attemptGeneration, 1u);
    EXPECT_EQ(snapshot.currentDeathGeneration, 1u);
    EXPECT_TRUE(snapshot.currentCycle);

    tracker.Record(2000, PlayerbotReviveOutcome::Succeeded, true);
    snapshot = tracker.Inspect();
    EXPECT_EQ(snapshot.timestampMs, 2000u);
    EXPECT_EQ(snapshot.outcome, PlayerbotReviveOutcome::Succeeded);
    EXPECT_TRUE(snapshot.success);
    EXPECT_TRUE(snapshot.aliveAfter);
    EXPECT_TRUE(snapshot.currentCycle);

    tracker.RecordPhysicalDeath();
    snapshot = tracker.Inspect();
    EXPECT_EQ(snapshot.attemptGeneration, 1u);
    EXPECT_EQ(snapshot.currentDeathGeneration, 2u);
    EXPECT_FALSE(snapshot.currentCycle);
}

/*
 * `success` is derived from the outcome rather than passed alongside it, so the two cannot
 * disagree on the wire. Only Succeeded is a success; the other three are not, and two of them are
 * not failures either.
 */
/*
 * The defect this whole change exists for.
 *
 * A ghost standing at its own corpse with the reclaim delay still running is doing exactly what
 * the timer asks of it. The corpse revive action runs on a `corpse near` trigger, so it reaches
 * this state every tick for at least thirty seconds and far longer after repeated deaths. Calling
 * that a failed revive is what made two healthy bots read as stuck in a revive loop.
 */
TEST(PlayerbotRecoveryPolicyTest, WaitingOutAReclaimDelayIsIneligibleRatherThanFailed)
{
    playerbots::recovery::CorpseReclaimEligibility waiting{
        .playerAlive = false,
        .inArena = false,
        .ghost = true,
        .hasCorpse = true,
        .reclaimDelayElapsed = false,
        .corpseInMap = true,
        .withinReclaimRadius = true,
    };
    EXPECT_EQ(playerbots::recovery::CorpseReviveOutcome(waiting, false, false), PlayerbotReviveOutcome::Ineligible);

    // Every other way of not being able to reclaim yet reads the same way, for the same reason.
    playerbots::recovery::CorpseReclaimEligibility outOfRange = waiting;
    outOfRange.reclaimDelayElapsed = true;
    outOfRange.withinReclaimRadius = false;
    EXPECT_EQ(playerbots::recovery::CorpseReviveOutcome(outOfRange, false, false), PlayerbotReviveOutcome::Ineligible);

    playerbots::recovery::CorpseReclaimEligibility noCorpse = waiting;
    noCorpse.reclaimDelayElapsed = true;
    noCorpse.hasCorpse = false;
    EXPECT_EQ(playerbots::recovery::CorpseReviveOutcome(noCorpse, false, false), PlayerbotReviveOutcome::Ineligible);
}

TEST(PlayerbotRecoveryPolicyTest, AnEligibleBotDecliningForItsLeaderIsNotAFailure)
{
    playerbots::recovery::CorpseReclaimEligibility const eligible{
        .playerAlive = false,
        .inArena = false,
        .ghost = true,
        .hasCorpse = true,
        .reclaimDelayElapsed = true,
        .corpseInMap = true,
        .withinReclaimRadius = true,
    };
    ASSERT_TRUE(playerbots::recovery::CanReclaimCorpse(eligible));

    EXPECT_EQ(playerbots::recovery::CorpseReviveOutcome(eligible, true, false), PlayerbotReviveOutcome::Declined);

    // Only an issued reclaim that left the bot dead is a failure.
    EXPECT_EQ(playerbots::recovery::CorpseReviveOutcome(eligible, false, false), PlayerbotReviveOutcome::Failed);
    EXPECT_EQ(playerbots::recovery::CorpseReviveOutcome(eligible, false, true), PlayerbotReviveOutcome::Succeeded);
}

/*
 * Ineligibility outranks the deferral: a bot that cannot act has not chosen anything, so reporting
 * a choice it never made would be its own untruth.
 */
TEST(PlayerbotRecoveryPolicyTest, IneligibilityOutranksDecliningToTheLeader)
{
    playerbots::recovery::CorpseReclaimEligibility const waiting{
        .playerAlive = false,
        .inArena = false,
        .ghost = true,
        .hasCorpse = true,
        .reclaimDelayElapsed = false,
        .corpseInMap = true,
        .withinReclaimRadius = true,
    };
    EXPECT_EQ(playerbots::recovery::CorpseReviveOutcome(waiting, true, false), PlayerbotReviveOutcome::Ineligible);
}

TEST(PlayerbotRecoveryPolicyTest, SuccessIsDerivedFromTheOutcomeAndOnlySucceededCounts)
{
    PlayerbotReviveAttemptTracker tracker;
    for (PlayerbotReviveOutcome const outcome :
         {PlayerbotReviveOutcome::Ineligible, PlayerbotReviveOutcome::Declined, PlayerbotReviveOutcome::Failed})
    {
        tracker.Record(1000, outcome, false);
        PlayerbotReviveAttemptSnapshot const snapshot = tracker.Inspect();
        EXPECT_EQ(snapshot.outcome, outcome);
        EXPECT_FALSE(snapshot.success) << PlayerbotReviveOutcomeName(outcome);
    }

    tracker.Record(2000, PlayerbotReviveOutcome::Succeeded, true);
    EXPECT_TRUE(tracker.Inspect().success);
}

TEST(PlayerbotRecoveryPolicyTest, EveryReviveOutcomeHasItsOwnWireName)
{
    EXPECT_STREQ(PlayerbotReviveOutcomeName(PlayerbotReviveOutcome::Ineligible), "ineligible");
    EXPECT_STREQ(PlayerbotReviveOutcomeName(PlayerbotReviveOutcome::Declined), "declined");
    EXPECT_STREQ(PlayerbotReviveOutcomeName(PlayerbotReviveOutcome::Failed), "failed");
    EXPECT_STREQ(PlayerbotReviveOutcomeName(PlayerbotReviveOutcome::Succeeded), "succeeded");
}

TEST(PlayerbotRecoveryPolicyTest, PhysicalDeathCountsOnceBeforeRelease)
{
    EXPECT_TRUE(playerbots::recovery::ShouldCountPhysicalDeath(false, false, false, false));
    EXPECT_FALSE(playerbots::recovery::ShouldCountPhysicalDeath(true, false, false, false));
    EXPECT_FALSE(playerbots::recovery::ShouldCountPhysicalDeath(false, true, false, false));
    EXPECT_FALSE(playerbots::recovery::ShouldCountPhysicalDeath(false, false, true, false));
    EXPECT_FALSE(playerbots::recovery::ShouldCountPhysicalDeath(false, false, false, true));
}

TEST(PlayerbotRecoveryPolicyTest, HomebindRecoveryRequiresReviveAndAcceptedTeleport)
{
    EXPECT_TRUE(playerbots::recovery::IsHomebindRecoverySuccessful(true, true));
    EXPECT_FALSE(playerbots::recovery::IsHomebindRecoverySuccessful(false, true));
    EXPECT_FALSE(playerbots::recovery::IsHomebindRecoverySuccessful(true, false));
}

TEST(PlayerbotRecoveryPolicyTest, PaidRepairPreservesDeathStreakAndAcceptedForcedRecoveryClearsIt)
{
    EXPECT_EQ(playerbots::recovery::DeathCountAfterPaidRepair(5u), 5u);
    EXPECT_EQ(playerbots::recovery::DeathCountAfterForcedRecovery(5u, false), 5u);
    EXPECT_EQ(playerbots::recovery::DeathCountAfterForcedRecovery(5u, true), 0u);
}

TEST(PlayerbotRecoveryPolicyTest, RepairSafetyBlocksCombatOnlyForAliveBrokenManagedRandomBots)
{
    EXPECT_TRUE(playerbots::recovery::ShouldRequireRepairBeforeCombat(true, true, true, true));
    EXPECT_FALSE(playerbots::recovery::ShouldRequireRepairBeforeCombat(false, true, true, true));
    EXPECT_FALSE(playerbots::recovery::ShouldRequireRepairBeforeCombat(true, false, true, true));
    EXPECT_FALSE(playerbots::recovery::ShouldRequireRepairBeforeCombat(true, true, false, true));
    EXPECT_FALSE(playerbots::recovery::ShouldRequireRepairBeforeCombat(true, true, true, false));
}

TEST_F(PlayerbotRecoveryActionTest, RejectedResurrectResponseDoesNotReportSuccess)
{
    bot->KillPlayer();
    ASSERT_FALSE(bot->IsAlive());
    botAI->DoNextAction(false);

    ObjectGuid const acceptedGuid = ObjectGuid::Create<HighGuid::Player>(900u);
    ObjectGuid const rejectedGuid = ObjectGuid::Create<HighGuid::Player>(901u);
    bot->setResurrectRequestData(acceptedGuid, bot->GetMapId(), bot->GetPositionX(), bot->GetPositionY(),
                                 bot->GetPositionZ(), 1u, 0u);
    WorldPacket packet;
    packet << rejectedGuid;
    AcceptResurrectAction action(botAI);

    EXPECT_FALSE(action.Execute(Event("resurrect request", packet)));
    EXPECT_FALSE(bot->IsAlive());
    PlayerbotReviveAttemptSnapshot const snapshot = botAI->InspectReviveAttempt();
    EXPECT_TRUE(snapshot.available);
    EXPECT_FALSE(snapshot.success);
    EXPECT_FALSE(snapshot.aliveAfter);
    EXPECT_EQ(snapshot.attemptGeneration, 1u);
    EXPECT_EQ(snapshot.currentDeathGeneration, 1u);
    EXPECT_TRUE(snapshot.currentCycle);
}

TEST_F(PlayerbotRecoveryActionTest, PaidRepairPreservesStoredPhysicalDeathCount)
{
    botAI->GetAiObjectContext()->GetValue<uint32>("death count")->Set(5u);
    TestRepairAllAction repair(botAI);

    EXPECT_TRUE(repair.Execute(Event()));
    EXPECT_EQ(botAI->GetAiObjectContext()->GetValue<uint32>("death count")->Get(), 5u);
}

TEST_F(PlayerbotRecoveryActionTest, ForcedRecoveryWritesBackOnlyAcceptedOutcome)
{
    botAI->GetAiObjectContext()->GetValue<uint32>("death count")->Set(5u);
    TestFindCorpseAction rejected(botAI, false);
    EXPECT_FALSE(rejected.Execute(Event()));
    EXPECT_EQ(botAI->GetAiObjectContext()->GetValue<uint32>("death count")->Get(), 5u);

    TestFindCorpseAction accepted(botAI, true);
    EXPECT_TRUE(accepted.Execute(Event()));
    EXPECT_EQ(botAI->GetAiObjectContext()->GetValue<uint32>("death count")->Get(), 0u);
}

TEST(PlayerbotRecoveryPolicyTest, ABodyReviveWaitsWhileAHostileIsInReachButNotForever)
{
    using playerbots::recovery::REVIVE_HOSTILE_WAIT_SECONDS;
    using playerbots::recovery::ReviveAtBodyAllowed;
    // Alma, 2026-09-01: revived beside the wolf that killed her and died six seconds later.
    EXPECT_FALSE(ReviveAtBodyAllowed(1, 0));
    EXPECT_FALSE(ReviveAtBodyAllowed(3, REVIVE_HOSTILE_WAIT_SECONDS - 1));
    EXPECT_TRUE(ReviveAtBodyAllowed(0, 0));
    // The wait is bounded so a camped corpse does not keep a bot dead all night.
    EXPECT_TRUE(ReviveAtBodyAllowed(1, REVIVE_HOSTILE_WAIT_SECONDS));
}

TEST(PlayerbotRecoveryPolicyTest, TheCorpseWalkArrivesAnywhereTheReclaimRadiusAllows)
{
    float const reclaimRadius = 39.0f;
    // Sinette, 2026-09-02 00:44: 34 yards from her body, nothing hostile near, and the walk kept
    // failing to close the last yards instead of letting the revive run.
    EXPECT_TRUE(CorpseWalkArrived(34.4f, reclaimRadius));
    EXPECT_TRUE(CorpseWalkArrived(37.9f, reclaimRadius));
    // Outside the radius the server would refuse the reclaim, so the walk goes on.
    EXPECT_FALSE(CorpseWalkArrived(38.5f, reclaimRadius));
    EXPECT_FALSE(CorpseWalkArrived(60.0f, reclaimRadius));
}

TEST(PlayerbotRecoveryPolicyTest, ASecondDeathOrAnOutmatchedKillerSendsTheBotHome)
{
    // A lost fight the bot can win next time: back to the body.
    EXPECT_FALSE(RecoverAtHomebindAfterDeath(1, 0));
    EXPECT_FALSE(RecoverAtHomebindAfterDeath(1, 3));
    // Audacious, 2026-09-02 01:40: level 9, killed four times by a level 17 Greater Fleshripper.
    EXPECT_TRUE(RecoverAtHomebindAfterDeath(1, 8));
    EXPECT_TRUE(RecoverAtHomebindAfterDeath(2, 0));
    // An environmental death (fall, drowning) goes home at once: the corpse lies where the
    // world kills. Teldrassil tree edge, 2026-09-06: two bots died twice each on their corpse.
    EXPECT_TRUE(RecoverAtHomebindAfterDeath(1, 0, true));
    EXPECT_FALSE(RecoverAtHomebindAfterDeath(1, 0, false));
    RecentDeathRecord const fall = NoteRecentDeath(RecentDeathRecord{}, 5000, 0, true);
    EXPECT_TRUE(fall.lastDeathEnvironmental);
    EXPECT_EQ(fall.deathsInWindow, 1u);
    RecentDeathRecord const fight = NoteRecentDeath(fall, 6000, 2, false);
    EXPECT_FALSE(fight.lastDeathEnvironmental);
    EXPECT_EQ(fight.deathsInWindow, 2u);

    // Deaths ten minutes apart are separate chains; closer ones count up.
    RecentDeathRecord first = NoteRecentDeath(RecentDeathRecord{}, 1000, 0);
    EXPECT_EQ(first.deathsInWindow, 1u);
    RecentDeathRecord second = NoteRecentDeath(first, 1000 + RECENT_DEATH_WINDOW_MS, 2);
    EXPECT_EQ(second.deathsInWindow, 2u);
    EXPECT_EQ(second.lastKillerLevelGap, 2);
    RecentDeathRecord later = NoteRecentDeath(second, second.lastDeathMs + RECENT_DEATH_WINDOW_MS + 1, 0);
    EXPECT_EQ(later.deathsInWindow, 1u);
}

TEST(PlayerbotRecoveryPolicyTest, TheVerticalCapSetsAGhostOntoItsCorpseOnceThenSendsItHome)
{
    // Dorothe, 2026-09-02 06:28: set onto her camped Barrow Den corpse ten times, never revived.
    VerticalCapRecord first = NoteVerticalCap(VerticalCapRecord{}, 1788344854);
    EXPECT_EQ(first.caps, 1u);
    EXPECT_FALSE(RecoverAtHomebindAfterVerticalCap(first.caps));
    VerticalCapRecord second = NoteVerticalCap(first, 1788344854);
    EXPECT_EQ(second.caps, 2u);
    EXPECT_TRUE(RecoverAtHomebindAfterVerticalCap(second.caps));
    // A later death is a new corpse and gets its own single cap.
    VerticalCapRecord nextCorpse = NoteVerticalCap(second, 1788345900);
    EXPECT_EQ(nextCorpse.caps, 1u);
    EXPECT_FALSE(RecoverAtHomebindAfterVerticalCap(nextCorpse.caps));
}

TEST(PlayerbotRecoveryPolicyTest, ACorpseStepKeepsToTheGhostsOwnFloor)
{
    // Jdyalani, 2026-09-02 01:20: on Teldrassil's tree at 134, the terrain grid under the tree
    // offered a step at minus 342, and the ghost went below the world.
    EXPECT_FALSE(CorpseStepHeightPlausible(134.0f, -342.0f));
    EXPECT_TRUE(CorpseStepHeightPlausible(134.0f, 72.0f));
    EXPECT_TRUE(CorpseStepHeightPlausible(116.0f, 64.0f));
}

TEST(PlayerbotCombatSafetyTest, PetAndPullEntrypointsHonorCentralCombatSafety)
{
    CombatBlockedPlayerbotAI botAI;
    PetAttackAction petAttack(&botAI);
    TestPullRequestAction pullRequest(&botAI);

    EXPECT_FALSE(petAttack.Execute(Event()));
    EXPECT_FALSE(pullRequest.Execute(Event()));
    EXPECT_EQ(botAI.checks, 2u);
}
}  // namespace
