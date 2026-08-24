/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RandomBotMaintenancePolicy.h"

#include <algorithm>

namespace playerbots::maintenance
{
namespace
{
constexpr std::uint8_t TEAM_ALLIANCE_ID = 0;
constexpr std::uint8_t RACE_HUMAN_ID = 1;
constexpr std::uint8_t RACE_ORC_ID = 2;
constexpr std::uint8_t RACE_DWARF_ID = 3;
constexpr std::uint8_t RACE_NIGHT_ELF_ID = 4;
constexpr std::uint8_t RACE_UNDEAD_ID = 5;
constexpr std::uint8_t RACE_TAUREN_ID = 6;
constexpr std::uint8_t RACE_GNOME_ID = 7;
constexpr std::uint8_t RACE_TROLL_ID = 8;
constexpr std::uint8_t RACE_BLOOD_ELF_ID = 10;
constexpr std::uint8_t RACE_DRAENEI_ID = 11;

constexpr std::uint32_t APPRENTICE_RIDING_SPELL = 33388;
constexpr std::uint32_t JOURNEYMAN_RIDING_SPELL = 33391;
constexpr std::uint32_t EXPERT_RIDING_SPELL = 34090;
constexpr std::uint32_t ARTISAN_RIDING_SPELL = 34091;

bool Contains(std::vector<std::uint32_t> const& values, std::uint32_t value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}
}  // namespace

LevelupMaintenancePlan BuildLevelupMaintenancePlan(bool randomBot, bool economyManagedSupplies,
                                                    bool autoUpgradeEquip)
{
    if (!randomBot)
        return {};

    return {
        .cleanupConsumables = !economyManagedSupplies,
        .provisionConsumables = !economyManagedSupplies,
        .upgradeEquipment = autoUpgradeEquip,
    };
}

bool ShouldRepairItem(std::uint32_t durability, std::uint32_t maximumDurability, std::uint32_t thresholdPercent)
{
    if (!maximumDurability)
        return false;

    if (!durability)
        return true;

    std::uint64_t const threshold = std::min<std::uint32_t>(thresholdPercent, 100);
    return static_cast<std::uint64_t>(durability) * 100 < static_cast<std::uint64_t>(maximumDurability) * threshold;
}

bool IsVendorTrash(std::uint32_t quality, bool vendorUsage)
{
    constexpr std::uint32_t POOR_QUALITY = 0;
    constexpr std::uint32_t NORMAL_QUALITY = 1;
    return quality == POOR_QUALITY || (quality == NORMAL_QUALITY && vendorUsage);
}

MountTier RequiredMountTier(std::uint32_t level, MountLevelThresholds const& thresholds)
{
    if (level >= thresholds.fastFlying)
        return MountTier::FastFlying;
    if (level >= thresholds.flying)
        return MountTier::Flying;
    if (level >= thresholds.fastGround)
        return MountTier::FastGround;
    if (level >= thresholds.ground)
        return MountTier::Ground;
    return MountTier::None;
}

std::uint32_t RequiredRidingSkill(MountTier tier)
{
    switch (tier)
    {
        case MountTier::Ground:
            return 75;
        case MountTier::FastGround:
            return 150;
        case MountTier::Flying:
            return 225;
        case MountTier::FastFlying:
            return 300;
        case MountTier::None:
            return 0;
    }

    return 0;
}

std::uint32_t NextRidingSpell(std::uint32_t ridingSkill, MountTier targetTier)
{
    std::uint32_t const targetSkill = RequiredRidingSkill(targetTier);
    if (ridingSkill >= targetSkill)
        return 0;
    if (ridingSkill < 75)
        return APPRENTICE_RIDING_SPELL;
    if (ridingSkill < 150)
        return JOURNEYMAN_RIDING_SPELL;
    if (ridingSkill < 225)
        return EXPERT_RIDING_SPELL;
    return ARTISAN_RIDING_SPELL;
}

std::uint32_t RidingTrainerEntry(std::uint8_t race, std::uint8_t team, std::uint32_t ridingSpellId)
{
    if (ridingSpellId == EXPERT_RIDING_SPELL || ridingSpellId == ARTISAN_RIDING_SPELL)
        return team == TEAM_ALLIANCE_ID ? 20511 : 20500;

    switch (race)
    {
        case RACE_HUMAN_ID:
            return 4732;
        case RACE_ORC_ID:
            return 4752;
        case RACE_DWARF_ID:
            return 4772;
        case RACE_NIGHT_ELF_ID:
            return 4753;
        case RACE_UNDEAD_ID:
            return 4773;
        case RACE_TAUREN_ID:
            return 3690;
        case RACE_GNOME_ID:
            return 7954;
        case RACE_TROLL_ID:
            return 7953;
        case RACE_BLOOD_ELF_ID:
            return 16280;
        case RACE_DRAENEI_ID:
            return 20914;
        default:
            return 0;
    }
}

std::vector<std::uint32_t> AllowedMountSpells(std::uint8_t race, std::uint8_t team, MountTier tier)
{
    if (tier == MountTier::None)
        return {};

    if (tier == MountTier::Flying)
        return team == TEAM_ALLIANCE_ID ? std::vector<std::uint32_t>{32235, 32239, 32240}
                                        : std::vector<std::uint32_t>{32244, 32245, 32243};

    if (tier == MountTier::FastFlying)
        return team == TEAM_ALLIANCE_ID ? std::vector<std::uint32_t>{32242, 32289, 32290, 32292}
                                        : std::vector<std::uint32_t>{32295, 32297, 32246, 32296};

    bool const fast = tier == MountTier::FastGround;
    switch (race)
    {
        case RACE_HUMAN_ID:
            return fast ? std::vector<std::uint32_t>{23228, 23227, 23229}
                        : std::vector<std::uint32_t>{470, 6648, 458, 472};
        case RACE_ORC_ID:
            return fast ? std::vector<std::uint32_t>{23250, 23252, 23251} : std::vector<std::uint32_t>{6654, 6653, 580};
        case RACE_DWARF_ID:
            return fast ? std::vector<std::uint32_t>{23238, 23239, 23240}
                        : std::vector<std::uint32_t>{6899, 6777, 6898};
        case RACE_NIGHT_ELF_ID:
            return fast ? std::vector<std::uint32_t>{23219, 23220, 63637}
                        : std::vector<std::uint32_t>{10789, 8394, 10793};
        case RACE_UNDEAD_ID:
            return fast ? std::vector<std::uint32_t>{17465, 23246, 66846}
                        : std::vector<std::uint32_t>{17463, 17464, 17462};
        case RACE_TAUREN_ID:
            return fast ? std::vector<std::uint32_t>{23249, 23248, 23247}
                        : std::vector<std::uint32_t>{18990, 18989, 64657};
        case RACE_GNOME_ID:
            return fast ? std::vector<std::uint32_t>{23225, 23223, 23222}
                        : std::vector<std::uint32_t>{10969, 17453, 10873, 17454};
        case RACE_TROLL_ID:
            return fast ? std::vector<std::uint32_t>{23241, 23242, 23243}
                        : std::vector<std::uint32_t>{10796, 10799, 8395};
        case RACE_DRAENEI_ID:
            return fast ? std::vector<std::uint32_t>{35713, 35712, 35714}
                        : std::vector<std::uint32_t>{34406, 35711, 35710};
        case RACE_BLOOD_ELF_ID:
            return fast ? std::vector<std::uint32_t>{35025, 35025, 35027}
                        : std::vector<std::uint32_t>{33660, 35020, 35022, 35018};
        default:
            return {};
    }
}

bool MountMeetsTier(MountTier tier, bool flying, std::int32_t speed)
{
    switch (tier)
    {
        case MountTier::Ground:
            return !flying && speed >= 59;
        case MountTier::FastGround:
            return !flying && speed >= 99;
        case MountTier::Flying:
            return flying && speed >= 149;
        case MountTier::FastFlying:
            return flying && speed >= 279;
        case MountTier::None:
            return true;
    }

    return false;
}

std::uint32_t SelectMountItem(std::uint8_t race, std::uint8_t team, MountTier tier, std::uint32_t budget,
                              std::vector<MountVendorCandidate> const& candidates)
{
    std::vector<std::uint32_t> const allowedSpells = AllowedMountSpells(race, team, tier);
    MountVendorCandidate const* selected = nullptr;
    for (MountVendorCandidate const& candidate : candidates)
    {
        if (!candidate.itemId || !candidate.usable || candidate.price > budget ||
            !Contains(allowedSpells, candidate.mountSpellId))
        {
            continue;
        }

        if (!selected || candidate.price < selected->price ||
            (candidate.price == selected->price && candidate.itemId < selected->itemId))
        {
            selected = &candidate;
        }
    }

    return selected ? selected->itemId : 0;
}
}  // namespace playerbots::maintenance
