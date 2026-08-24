/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option), any later version.
 */

#ifndef PLAYERBOTS_PLAYERBOTRECOVERYPOLICY_H
#define PLAYERBOTS_PLAYERBOTRECOVERYPOLICY_H

#include <cstdint>
#include <mutex>

struct PlayerbotReviveAttemptSnapshot
{
    bool available = false;
    std::uint64_t timestampMs = 0;
    bool success = false;
    bool aliveAfter = false;
    std::uint64_t attemptGeneration = 0;
    std::uint64_t currentDeathGeneration = 0;
    bool currentCycle = false;
};

class PlayerbotReviveAttemptTracker
{
public:
    void RecordPhysicalDeath();
    void Record(std::uint64_t timestampMs, bool success, bool aliveAfter);
    [[nodiscard]] PlayerbotReviveAttemptSnapshot Inspect() const;

private:
    mutable std::mutex mutex;
    PlayerbotReviveAttemptSnapshot snapshot;
    std::uint64_t currentDeathGeneration = 0;
};

namespace playerbots::recovery
{
struct CorpseReclaimEligibility
{
    bool playerAlive = false;
    bool inArena = false;
    bool ghost = false;
    bool hasCorpse = false;
    bool reclaimDelayElapsed = false;
    bool corpseInMap = false;
    bool withinReclaimRadius = false;
};

[[nodiscard]] bool CanReclaimCorpse(CorpseReclaimEligibility const& eligibility);
[[nodiscard]] bool ShouldCountPhysicalDeath(bool alreadyInDeadEngine, bool alive, bool inBattleground,
                                            bool hasRealPlayerMaster);
[[nodiscard]] bool IsHomebindRecoverySuccessful(bool aliveAfterRevive, bool teleportAccepted);
[[nodiscard]] bool ShouldRequireRepairBeforeCombat(bool economyManagedSupplies, bool randomBot, bool alive,
                                                   bool hasBrokenEquipment);
}  // namespace playerbots::recovery

#endif
