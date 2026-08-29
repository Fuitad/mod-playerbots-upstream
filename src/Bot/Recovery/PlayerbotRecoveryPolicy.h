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

#ifndef PLAYERBOTS_PLAYERBOTRECOVERYPOLICY_H
#define PLAYERBOTS_PLAYERBOTRECOVERYPOLICY_H

#include <cstdint>
#include <mutex>

/*
 * Why a revive attempt ended the way it did.
 *
 * Recorded because "did not succeed" was not a usable answer. ReviveFromCorpseAction runs on a
 * `corpse near` trigger, so it fires every tick a ghost stands at its body waiting out the reclaim
 * delay, which is thirty seconds at minimum and escalates with repeated deaths. Every one of those
 * ticks recorded a failed attempt, and the tracker keeps only the newest, so any observer looking
 * during an ordinary corpse run saw a bot apparently failing to revive over and over. It was not
 * failing; it was waiting, which is what the timer is for.
 *
 * Ineligible and Declined are therefore not failures and must not be counted as such. Failed means
 * the reclaim, resurrect, or teleport was actually issued and the bot is still dead.
 */
enum class PlayerbotReviveOutcome : std::uint8_t
{
    // The bot cannot reclaim yet: reclaim delay still running, out of radius, corpse not loaded.
    Ineligible = 0,
    // The bot could have acted and deliberately did not, deferring to a dead human group leader.
    Declined,
    // An attempt was issued and the bot is still dead.
    Failed,
    Succeeded
};

[[nodiscard]] char const* PlayerbotReviveOutcomeName(PlayerbotReviveOutcome outcome);

struct PlayerbotReviveAttemptSnapshot
{
    bool available = false;
    std::uint64_t timestampMs = 0;
    PlayerbotReviveOutcome outcome = PlayerbotReviveOutcome::Ineligible;
    // Kept for callers that only care whether the bot got up. Always outcome == Succeeded.
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
    void Record(std::uint64_t timestampMs, PlayerbotReviveOutcome outcome, bool aliveAfter);
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

/*
 * The label for one pass of the corpse revive action, in one testable place.
 *
 * The action owns the control flow and the side effects; this owns only what the attempt should be
 * called afterwards. Keeping the two apart is what makes the distinction verifiable at all: the
 * action needs a live Corpse, session, and map to run, so a mapping left inline in it was
 * effectively untestable, and the mapping is exactly where the defect was.
 */
[[nodiscard]] PlayerbotReviveOutcome CorpseReviveOutcome(CorpseReclaimEligibility const& eligibility,
                                                         bool deferringToDeadHumanLeader, bool aliveAfterReclaim);
[[nodiscard]] bool ShouldCountPhysicalDeath(bool alreadyInDeadEngine, bool alive, bool inBattleground,
                                            bool hasRealPlayerMaster);
[[nodiscard]] bool IsHomebindRecoverySuccessful(bool aliveAfterRevive, bool teleportAccepted);
// The last fact is a broken main-hand WEAPON, not any broken equipment: banning combat over broken
// armor starved bots of the income they needed to repair (see PlayerbotAI::HasBrokenWeapon).
[[nodiscard]] bool ShouldRequireRepairBeforeCombat(bool economyManagedSupplies, bool randomBot, bool alive,
                                                   bool hasBrokenWeapon);
}  // namespace playerbots::recovery

#endif
