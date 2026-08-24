/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option), any later version.
 */

#include "Ai/Base/Actions/AcceptResurrectAction.h"
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
    tracker.Record(1000, false, false);
    snapshot = tracker.Inspect();
    EXPECT_TRUE(snapshot.available);
    EXPECT_EQ(snapshot.timestampMs, 1000u);
    EXPECT_FALSE(snapshot.success);
    EXPECT_FALSE(snapshot.aliveAfter);
    EXPECT_EQ(snapshot.attemptGeneration, 1u);
    EXPECT_EQ(snapshot.currentDeathGeneration, 1u);
    EXPECT_TRUE(snapshot.currentCycle);

    tracker.Record(2000, true, true);
    snapshot = tracker.Inspect();
    EXPECT_EQ(snapshot.timestampMs, 2000u);
    EXPECT_TRUE(snapshot.success);
    EXPECT_TRUE(snapshot.aliveAfter);
    EXPECT_TRUE(snapshot.currentCycle);

    tracker.RecordPhysicalDeath();
    snapshot = tracker.Inspect();
    EXPECT_EQ(snapshot.attemptGeneration, 1u);
    EXPECT_EQ(snapshot.currentDeathGeneration, 2u);
    EXPECT_FALSE(snapshot.currentCycle);
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
