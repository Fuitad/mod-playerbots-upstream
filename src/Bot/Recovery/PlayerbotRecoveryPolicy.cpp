/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option), any later version.
 */

#include "PlayerbotRecoveryPolicy.h"

void PlayerbotReviveAttemptTracker::RecordPhysicalDeath()
{
    std::scoped_lock lock(mutex);
    ++currentDeathGeneration;
}

void PlayerbotReviveAttemptTracker::Record(std::uint64_t timestampMs, bool success, bool aliveAfter)
{
    std::scoped_lock lock(mutex);
    snapshot = {
        .available = true,
        .timestampMs = timestampMs,
        .success = success,
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
