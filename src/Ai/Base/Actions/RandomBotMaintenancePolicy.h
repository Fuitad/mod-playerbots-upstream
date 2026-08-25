/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#ifndef PLAYERBOTS_RANDOMBOTMAINTENANCEPOLICY_H
#define PLAYERBOTS_RANDOMBOTMAINTENANCEPOLICY_H

#include <cstdint>
#include <vector>

namespace playerbots::maintenance
{
enum class MountTier : std::uint8_t
{
    None,
    Ground,
    FastGround,
    Flying,
    FastFlying
};

struct MountLevelThresholds
{
    std::uint32_t ground = 20;
    std::uint32_t fastGround = 40;
    std::uint32_t flying = 60;
    std::uint32_t fastFlying = 70;
};

struct MountVendorCandidate
{
    std::uint32_t itemId = 0;
    std::uint32_t mountSpellId = 0;
    std::uint32_t price = 0;
    bool usable = false;
};

struct LevelupMaintenancePlan
{
    bool cleanupConsumables = false;
    bool provisionConsumables = false;
    bool upgradeEquipment = false;
};

[[nodiscard]] LevelupMaintenancePlan BuildLevelupMaintenancePlan(bool randomBot, bool economyManagedSupplies,
                                                                 bool autoUpgradeEquip);
/*
 * How far a bot may reasonably walk to reach a repair or vendor NPC.
 *
 * "Nearest" is not the same as "reachable". The spawn catalog is searched by absolute distance with
 * no ceiling, so a bot on the wrong side of a continent is handed a destination it can only reach
 * on foot through everything in between. Live example on 2026-08-25: a level 28 hunter with a zero
 * durability bow was sent to a repair NPC 13906 yards away and walked toward it at roughly 280
 * yards per sample, dying repeatedly the whole way with nothing to fight back with.
 *
 * Sized to be generous for an ordinary errand and clearly wrong for a cross continent march. A
 * fifteen hundred yard walk is a couple of minutes; anything past it is a journey.
 */
inline constexpr float MAINTENANCE_MAX_WALK_YARDS = 1500.0f;

// Verified against acore_world.item_template: entry 6948 "Hearthstone" carries spellid_1 8690, and
// core gates the same cooldown on spell 8690 (SpellEffects.cpp:4208).
inline constexpr std::uint32_t HEARTHSTONE_ITEM_ID = 6948;
inline constexpr std::uint32_t HEARTHSTONE_SPELL_ID = 8690;

/*
 * What a bot that needs repair should do next.
 *
 * Repair outranks the rest of maintenance because a bot with broken gear cannot fight, cannot
 * finish the errand it is on, and takes further durability damage every time it dies trying. The
 * hearth exists for the case the catalog cannot solve: an inn is always somewhere with a repairer,
 * so a bot that cannot reach one on foot can still get itself somewhere useful.
 */
enum class RepairPlan : std::uint8_t
{
    // Nothing is broken enough to act on.
    None = 0,
    // A repairer is close enough to walk to.
    Travel,
    // Gear is broken and no repairer is within reach; go somewhere that has one.
    Hearth,
    // Gear is broken, nothing is reachable, and the hearth is unavailable. Nothing left to try.
    Stranded
};

/*
 * `hasBrokenEquipment` is the urgent case (an item at zero durability), distinct from `needsRepair`
 * which also covers merely worn gear. Only the urgent case justifies spending a hearth: a bot with
 * a chipped sword can finish what it is doing and repair on the way past.
 */
[[nodiscard]] RepairPlan ChooseRepairPlan(bool needsRepair, bool hasBrokenEquipment, bool destinationFound,
                                          float destinationDistanceYards, bool hearthReady);

[[nodiscard]] bool ShouldRepairItem(std::uint32_t durability, std::uint32_t maximumDurability,
                                    std::uint32_t thresholdPercent);
[[nodiscard]] bool IsVendorTrash(std::uint32_t quality, bool vendorUsage);
[[nodiscard]] MountTier RequiredMountTier(std::uint32_t level, MountLevelThresholds const& thresholds);
[[nodiscard]] std::uint32_t RequiredRidingSkill(MountTier tier);
[[nodiscard]] std::uint32_t NextRidingSpell(std::uint32_t ridingSkill, MountTier targetTier);
[[nodiscard]] std::uint32_t RidingTrainerEntry(std::uint8_t race, std::uint8_t team, std::uint32_t ridingSpellId);
[[nodiscard]] std::vector<std::uint32_t> AllowedMountSpells(std::uint8_t race, std::uint8_t team, MountTier tier);
[[nodiscard]] bool MountMeetsTier(MountTier tier, bool flying, std::int32_t speed);
[[nodiscard]] std::uint32_t SelectMountItem(std::uint8_t race, std::uint8_t team, MountTier tier, std::uint32_t budget,
                                            std::vector<MountVendorCandidate> const& candidates);
}  // namespace playerbots::maintenance

#endif
