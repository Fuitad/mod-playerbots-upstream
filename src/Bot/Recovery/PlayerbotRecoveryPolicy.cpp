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

#include "PlayerbotRecoveryPolicy.h"

void PlayerbotReviveAttemptTracker::RecordPhysicalDeath()
{
    std::scoped_lock lock(mutex);
    ++currentDeathGeneration;
}

char const* PlayerbotReviveOutcomeName(PlayerbotReviveOutcome outcome)
{
    switch (outcome)
    {
        case PlayerbotReviveOutcome::Ineligible:
            return "ineligible";
        case PlayerbotReviveOutcome::Declined:
            return "declined";
        case PlayerbotReviveOutcome::Failed:
            return "failed";
        case PlayerbotReviveOutcome::Succeeded:
            return "succeeded";
    }
    return "ineligible";
}

void PlayerbotReviveAttemptTracker::Record(std::uint64_t timestampMs, PlayerbotReviveOutcome outcome, bool aliveAfter)
{
    std::scoped_lock lock(mutex);
    snapshot = {
        .available = true,
        .timestampMs = timestampMs,
        .outcome = outcome,
        // Derived rather than passed, so the flag and the outcome cannot disagree.
        .success = outcome == PlayerbotReviveOutcome::Succeeded,
        .aliveAfter = aliveAfter,
        .attemptGeneration = currentDeathGeneration,
        .currentDeathGeneration = currentDeathGeneration,
        .currentCycle = true,
    };
}

PlayerbotReviveAttemptSnapshot PlayerbotReviveAttemptTracker::Inspect() const
{
    std::scoped_lock lock(mutex);
    PlayerbotReviveAttemptSnapshot result = snapshot;
    result.currentDeathGeneration = currentDeathGeneration;
    result.currentCycle = result.available && result.attemptGeneration == currentDeathGeneration;
    return result;
}

bool playerbots::recovery::CanReclaimCorpse(CorpseReclaimEligibility const& eligibility)
{
    return !eligibility.playerAlive && !eligibility.inArena && eligibility.ghost && eligibility.hasCorpse &&
           eligibility.reclaimDelayElapsed && eligibility.corpseInMap && eligibility.withinReclaimRadius;
}

PlayerbotReviveOutcome playerbots::recovery::CorpseReviveOutcome(CorpseReclaimEligibility const& eligibility,
                                                                 bool deferringToDeadHumanLeader,
                                                                 bool aliveAfterReclaim)
{
    // Checked first, because a bot that cannot reclaim yet has not attempted anything. This is the
    // ordinary state of every ghost still serving its reclaim delay.
    if (!CanReclaimCorpse(eligibility))
        return PlayerbotReviveOutcome::Ineligible;

    // Able to act and choosing not to, so its human leader can resurrect it instead.
    if (deferringToDeadHumanLeader)
        return PlayerbotReviveOutcome::Declined;

    return aliveAfterReclaim ? PlayerbotReviveOutcome::Succeeded : PlayerbotReviveOutcome::Failed;
}

bool playerbots::recovery::ShouldCountPhysicalDeath(bool alreadyInDeadEngine, bool alive, bool inBattleground,
                                                    bool hasRealPlayerMaster)
{
    return !alreadyInDeadEngine && !alive && !inBattleground && !hasRealPlayerMaster;
}

bool playerbots::recovery::IsHomebindRecoverySuccessful(bool aliveAfterRevive, bool teleportAccepted)
{
    return aliveAfterRevive && teleportAccepted;
}

bool playerbots::recovery::ShouldRequireRepairBeforeCombat(bool economyManagedSupplies, bool randomBot, bool alive,
                                                           bool hasBrokenEquipment)
{
    return economyManagedSupplies && randomBot && alive && hasBrokenEquipment;
}
