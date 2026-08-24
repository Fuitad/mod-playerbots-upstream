/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

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
