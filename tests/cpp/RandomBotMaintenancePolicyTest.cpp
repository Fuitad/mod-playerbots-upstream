/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#include "Ai/Base/Actions/RandomBotMaintenancePolicy.h"
#include "gtest/gtest.h"

namespace
{
using playerbots::maintenance::MountLevelThresholds;
using playerbots::maintenance::MountTier;
using playerbots::maintenance::MountVendorCandidate;

TEST(RandomBotMaintenancePolicyTest, ManagedLevelupKeepsEquipmentFloorWithoutSyntheticConsumables)
{
    playerbots::maintenance::LevelupMaintenancePlan const managed =
        playerbots::maintenance::BuildLevelupMaintenancePlan(true, true, true);
    EXPECT_FALSE(managed.cleanupConsumables);
    EXPECT_FALSE(managed.provisionConsumables);
    EXPECT_TRUE(managed.upgradeEquipment);

    playerbots::maintenance::LevelupMaintenancePlan const unmanaged =
        playerbots::maintenance::BuildLevelupMaintenancePlan(true, false, true);
    EXPECT_TRUE(unmanaged.cleanupConsumables);
    EXPECT_TRUE(unmanaged.provisionConsumables);
    EXPECT_TRUE(unmanaged.upgradeEquipment);

    playerbots::maintenance::LevelupMaintenancePlan const nonRandom =
        playerbots::maintenance::BuildLevelupMaintenancePlan(false, true, true);
    EXPECT_FALSE(nonRandom.cleanupConsumables);
    EXPECT_FALSE(nonRandom.provisionConsumables);
    EXPECT_FALSE(nonRandom.upgradeEquipment);
}

TEST(RandomBotMaintenancePolicyTest, RepairThresholdUsesEachEquippedItemsDurability)
{
    EXPECT_FALSE(playerbots::maintenance::ShouldRepairItem(80, 100, 80));
    EXPECT_TRUE(playerbots::maintenance::ShouldRepairItem(79, 100, 80));
    EXPECT_TRUE(playerbots::maintenance::ShouldRepairItem(0, 100, 0));
    EXPECT_FALSE(playerbots::maintenance::ShouldRepairItem(0, 0, 100));
    EXPECT_FALSE(playerbots::maintenance::ShouldRepairItem(100, 100, 250));
}

TEST(RandomBotMaintenancePolicyTest, VendorTrashIsGreyOrWhiteMarkedForVendor)
{
    EXPECT_TRUE(playerbots::maintenance::IsVendorTrash(0, false));
    EXPECT_TRUE(playerbots::maintenance::IsVendorTrash(1, true));
    EXPECT_FALSE(playerbots::maintenance::IsVendorTrash(1, false));
    EXPECT_FALSE(playerbots::maintenance::IsVendorTrash(2, true));
}

TEST(RandomBotMaintenancePolicyTest, MountTierAndRidingProgressFollowConfiguredLevels)
{
    MountLevelThresholds const levels{20, 40, 60, 70};
    EXPECT_EQ(playerbots::maintenance::RequiredMountTier(19, levels), MountTier::None);
    EXPECT_EQ(playerbots::maintenance::RequiredMountTier(20, levels), MountTier::Ground);
    EXPECT_EQ(playerbots::maintenance::RequiredMountTier(40, levels), MountTier::FastGround);
    EXPECT_EQ(playerbots::maintenance::RequiredMountTier(60, levels), MountTier::Flying);
    EXPECT_EQ(playerbots::maintenance::RequiredMountTier(70, levels), MountTier::FastFlying);

    EXPECT_EQ(playerbots::maintenance::NextRidingSpell(0, MountTier::FastFlying), 33388u);
    EXPECT_EQ(playerbots::maintenance::NextRidingSpell(75, MountTier::FastFlying), 33391u);
    EXPECT_EQ(playerbots::maintenance::NextRidingSpell(150, MountTier::FastFlying), 34090u);
    EXPECT_EQ(playerbots::maintenance::NextRidingSpell(225, MountTier::FastFlying), 34091u);
    EXPECT_EQ(playerbots::maintenance::NextRidingSpell(300, MountTier::FastFlying), 0u);
}

TEST(RandomBotMaintenancePolicyTest, MountSelectionRejectsOtherRacesAndUnaffordableItems)
{
    std::vector<MountVendorCandidate> const candidates = {
        {100, 458, 10000, true},
        {101, 580, 5000, true},
        {102, 472, 12000, true},
        {103, 470, 8000, false},
    };

    EXPECT_EQ(playerbots::maintenance::SelectMountItem(1, 0, MountTier::Ground, 11000, candidates), 100u);
    EXPECT_EQ(playerbots::maintenance::SelectMountItem(2, 1, MountTier::Ground, 11000, candidates), 101u);
    EXPECT_EQ(playerbots::maintenance::SelectMountItem(1, 0, MountTier::Ground, 9000, candidates), 0u);
}

TEST(RandomBotMaintenancePolicyTest, EveryPlayableRaceSelectsOnlyItsGroundMount)
{
    struct RaceMount
    {
        std::uint8_t race;
        std::uint32_t spell;
        std::uint32_t item;
    };

    std::vector<RaceMount> const raceMounts = {
        {1, 458, 1001},   {2, 580, 1002},   {3, 6899, 1003},  {4, 10789, 1004},  {5, 17463, 1005},
        {6, 18990, 1006}, {7, 10969, 1007}, {8, 10796, 1008}, {10, 33660, 1010}, {11, 34406, 1011},
    };
    std::vector<MountVendorCandidate> candidates;
    for (RaceMount const& mount : raceMounts)
        candidates.push_back({mount.item, mount.spell, 10000, true});

    for (RaceMount const& mount : raceMounts)
    {
        EXPECT_EQ(playerbots::maintenance::SelectMountItem(mount.race, mount.race == 1 ? 0 : 1, MountTier::Ground,
                                                           10000, candidates),
                  mount.item);
    }
}

TEST(RandomBotMaintenancePolicyTest, RaceAndFactionSelectTheirOwnTrainersAndMounts)
{
    EXPECT_EQ(playerbots::maintenance::RidingTrainerEntry(1, 0, 33388), 4732u);
    EXPECT_EQ(playerbots::maintenance::RidingTrainerEntry(2, 1, 33391), 4752u);
    EXPECT_EQ(playerbots::maintenance::RidingTrainerEntry(11, 0, 33388), 20914u);
    EXPECT_EQ(playerbots::maintenance::RidingTrainerEntry(10, 1, 33388), 16280u);
    EXPECT_EQ(playerbots::maintenance::RidingTrainerEntry(1, 0, 34090), 20511u);
    EXPECT_EQ(playerbots::maintenance::RidingTrainerEntry(2, 1, 34091), 20500u);

    EXPECT_TRUE(playerbots::maintenance::MountMeetsTier(MountTier::FastGround, false, 99));
    EXPECT_FALSE(playerbots::maintenance::MountMeetsTier(MountTier::FastGround, false, 59));
    EXPECT_TRUE(playerbots::maintenance::MountMeetsTier(MountTier::Flying, true, 149));
    EXPECT_FALSE(playerbots::maintenance::MountMeetsTier(MountTier::Flying, false, 279));
}
}  // namespace
