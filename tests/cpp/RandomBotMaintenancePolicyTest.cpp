/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#include <limits>

#include "Ai/Base/Actions/RandomBotMaintenancePolicy.h"
#include "gtest/gtest.h"

namespace
{
using playerbots::maintenance::MountLevelThresholds;
using playerbots::maintenance::MountTier;
using playerbots::maintenance::MountVendorCandidate;
using playerbots::maintenance::RepairPlan;

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

/*
 * The live incident this exists to prevent: on 2026-08-25 a level 28 hunter with a zero durability
 * bow was handed a repair NPC 13906 yards away and walked toward it, dying the whole way. "Nearest"
 * is not "reachable", and a bot that cannot fight cannot make that journey.
 */
TEST(RandomBotRepairPlanTest, AnUnreachableRepairerWithBrokenGearHearthsInsteadOfWalking)
{
    EXPECT_EQ(playerbots::maintenance::ChooseRepairPlan(true, true, true, 13906.0f, true), RepairPlan::Hearth);

    // Just past the walking bound is already too far; the bound itself is still walkable.
    EXPECT_EQ(playerbots::maintenance::ChooseRepairPlan(
                  true, true, true, playerbots::maintenance::MAINTENANCE_MAX_WALK_YARDS + 1.0f, true),
              RepairPlan::Hearth);
    EXPECT_EQ(playerbots::maintenance::ChooseRepairPlan(true, true, true,
                                                        playerbots::maintenance::MAINTENANCE_MAX_WALK_YARDS, true),
              RepairPlan::Travel);
}

/*
 * A target latched while the bot stood next to it, re-evaluated after the bot travelled away.
 *
 * The action used to consult the plan only when it had no target at all, so the walking bound was
 * applied once at selection and never again. Live on 2026-08-25 a bot with merely worn gear sat
 * calling MoveFarTo against a repairer 6978 yards away, stationary, every tick. The plan itself was
 * always right about that distance; nothing asked it a second time.
 */
TEST(RandomBotRepairPlanTest, AStaleTargetThatDriftedOutOfRangeIsNoLongerWalkable)
{
    // Worn gear, no longer reachable: not worth a hearth, and definitely not worth the walk.
    EXPECT_EQ(playerbots::maintenance::ChooseRepairPlan(true, false, true, 6978.0f, true), RepairPlan::None);
    EXPECT_NE(playerbots::maintenance::ChooseRepairPlan(true, false, true, 6978.0f, true), RepairPlan::Travel);

    // The same target while the bot is still beside it stays walkable, so re-checking every tick
    // cannot thrash a bot that is legitimately on its way.
    EXPECT_EQ(playerbots::maintenance::ChooseRepairPlan(true, false, true, 12.0f, true), RepairPlan::Travel);

    // Broken gear at that distance hearths rather than marching.
    EXPECT_EQ(playerbots::maintenance::ChooseRepairPlan(true, true, true, 6978.0f, true), RepairPlan::Hearth);
}

TEST(RandomBotRepairPlanTest, AReachableRepairerIsWalkedToWhateverTheGearState)
{
    EXPECT_EQ(playerbots::maintenance::ChooseRepairPlan(true, true, true, 6.0f, true), RepairPlan::Travel);
    EXPECT_EQ(playerbots::maintenance::ChooseRepairPlan(true, false, true, 915.0f, true), RepairPlan::Travel);

    // A hearth is never spent on a walk the bot can make, even when it is available.
    EXPECT_NE(playerbots::maintenance::ChooseRepairPlan(true, true, true, 100.0f, true), RepairPlan::Hearth);
}

/*
 * Worn gear is not an emergency. A bot with a chipped sword can still fight, so it waits to pass a
 * repairer on its own business rather than burning a hearth it may need when something is broken.
 */
TEST(RandomBotRepairPlanTest, OnlyBrokenGearJustifiesSpendingTheHearth)
{
    EXPECT_EQ(playerbots::maintenance::ChooseRepairPlan(true, false, true, 99999.0f, true), RepairPlan::None);
    EXPECT_EQ(playerbots::maintenance::ChooseRepairPlan(true, false, false, 0.0f, true), RepairPlan::None);
    EXPECT_EQ(playerbots::maintenance::ChooseRepairPlan(true, true, false, 0.0f, true), RepairPlan::Hearth);
}

TEST(RandomBotRepairPlanTest, NoHearthLeavesTheBotStrandedRatherThanSilent)
{
    // Reported rather than swallowed: this is the state a human needs to see in the log.
    EXPECT_EQ(playerbots::maintenance::ChooseRepairPlan(true, true, true, 13906.0f, false), RepairPlan::Stranded);
    EXPECT_EQ(playerbots::maintenance::ChooseRepairPlan(true, true, false, 0.0f, false), RepairPlan::Stranded);
}

TEST(RandomBotRepairPlanTest, NothingToRepairAlwaysDoesNothing)
{
    EXPECT_EQ(playerbots::maintenance::ChooseRepairPlan(false, true, true, 10.0f, true), RepairPlan::None);
    EXPECT_EQ(playerbots::maintenance::ChooseRepairPlan(false, false, false, 0.0f, false), RepairPlan::None);
}

/*
 * A non finite or negative distance is a broken measurement, not a short walk. Treating it as
 * reachable would send the bot walking toward a position it can never arrive at.
 */
TEST(RandomBotRepairPlanTest, AnUnusableDistanceIsNotTreatedAsReachable)
{
    EXPECT_EQ(playerbots::maintenance::ChooseRepairPlan(true, true, true, std::numeric_limits<float>::infinity(), true),
              RepairPlan::Hearth);
    EXPECT_EQ(
        playerbots::maintenance::ChooseRepairPlan(true, true, true, std::numeric_limits<float>::quiet_NaN(), true),
        RepairPlan::Hearth);
    EXPECT_EQ(playerbots::maintenance::ChooseRepairPlan(true, true, true, -1.0f, true), RepairPlan::Hearth);
}
}  // namespace
