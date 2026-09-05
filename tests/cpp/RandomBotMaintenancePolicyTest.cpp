/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#include <limits>

#include "Ai/Base/Actions/MaintenanceErrandPolicy.h"
#include "Ai/Base/Actions/RandomBotMaintenancePolicy.h"
#include "gtest/gtest.h"

namespace
{
using playerbots::maintenance::MountLevelThresholds;
using playerbots::maintenance::MountSpellEffects;
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

TEST(RandomBotMaintenancePolicyTest, MountSpellMeetsTierClassifiesLearnedMountsByTier)
{
    // A learned 100% ground mount.
    MountSpellEffects ground;
    ground.mountAura = true;
    ground.speed1 = 100;

    EXPECT_TRUE(playerbots::maintenance::MountSpellMeetsTier(MountTier::Ground, ground));
    EXPECT_TRUE(playerbots::maintenance::MountSpellMeetsTier(MountTier::FastGround, ground));
    EXPECT_FALSE(playerbots::maintenance::MountSpellMeetsTier(MountTier::Flying, ground));

    // Not a mount at all: no SPELL_AURA_MOUNTED on the first effect.
    MountSpellEffects notAMount = ground;
    notAMount.mountAura = false;
    EXPECT_FALSE(playerbots::maintenance::MountSpellMeetsTier(MountTier::Ground, notAMount));

    // Passive and removed/inactive spells are not ridable.
    MountSpellEffects passive = ground;
    passive.passive = true;
    EXPECT_FALSE(playerbots::maintenance::MountSpellMeetsTier(MountTier::Ground, passive));

    MountSpellEffects inactive = ground;
    inactive.active = false;
    EXPECT_FALSE(playerbots::maintenance::MountSpellMeetsTier(MountTier::Ground, inactive));

    // Flight is recognised from either the flight-speed aura or the 54729 special case, and the
    // faster of the two speed effects is the one that counts.
    MountSpellEffects flying;
    flying.mountAura = true;
    flying.flightSpeedAura = true;
    flying.speed1 = 0;
    flying.speed2 = 280;
    EXPECT_TRUE(playerbots::maintenance::MountSpellMeetsTier(MountTier::FastFlying, flying));

    MountSpellEffects specialCase = flying;
    specialCase.flightSpeedAura = false;
    specialCase.alwaysFlying = true;
    EXPECT_TRUE(playerbots::maintenance::MountSpellMeetsTier(MountTier::FastFlying, specialCase));
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

TEST(RandomBotMaintenancePolicyTest, RoutineMaintenanceWaitsForTheQuestButUrgentNeedsGoNow)
{
    // The defect this pins: a vendor errand fired mid-quest dragged a bot straight off the Lazy
    // Peons objective it had just reached. Routine trips defer while a quest is being worked;
    // broken gear or bags too full to loot must still go immediately, and outside quest work
    // nothing defers.
    EXPECT_TRUE(playerbots::maintenance::DeferRoutineMaintenanceDuringQuest(true, false));
    EXPECT_FALSE(playerbots::maintenance::DeferRoutineMaintenanceDuringQuest(true, true));
    EXPECT_FALSE(playerbots::maintenance::DeferRoutineMaintenanceDuringQuest(false, false));
    EXPECT_FALSE(playerbots::maintenance::DeferRoutineMaintenanceDuringQuest(false, true));
}

TEST(RandomBotMaintenancePolicyTest, BrokenGearEarnsARepairTripWhateverThePurseHolds)
{
    using playerbots::maintenance::RepairTripWorthPlanning;
    // Vavapu (2026-09-01): 40c against a repair cost far above it, four items at zero durability,
    // and no trip ever planned. Broken gear must go regardless.
    EXPECT_TRUE(RepairTripWorthPlanning(true, 300, 40));
    EXPECT_TRUE(RepairTripWorthPlanning(true, 300, 0));
    // Merely worn gear keeps the affordability gate.
    EXPECT_TRUE(RepairTripWorthPlanning(false, 300, 300));
    EXPECT_FALSE(RepairTripWorthPlanning(false, 300, 299));
    EXPECT_FALSE(RepairTripWorthPlanning(false, 0, 1000));
}

TEST(RandomBotMaintenancePolicyTest, ARepairVisitIsJudgedByTheGearNotByTheActionsWord)
{
    using playerbots::maintenance::RepairVisitOutcome;
    using playerbots::maintenance::RepairVisitVerdict;
    // Vavapu, 2026-09-01: "repaired" logged every five seconds with 2c and nothing repaired.
    EXPECT_EQ(RepairVisitVerdict(false, 45, 45), RepairVisitOutcome::Unaffordable);
    EXPECT_EQ(RepairVisitVerdict(false, 45, 46), RepairVisitOutcome::Repaired);
    // A weapon the purse cannot cover ends the visit whatever the armour pass would have bought.
    EXPECT_EQ(RepairVisitVerdict(true, 45, 45), RepairVisitOutcome::WeaponStarved);
    EXPECT_EQ(RepairVisitVerdict(true, 45, 90), RepairVisitOutcome::WeaponStarved);
}

TEST(RandomBotMaintenancePolicyTest, AClaimedErrandOwnsMovementUntilItEndsOrItsLeaseLapses)
{
    using playerbots::maintenance::ErrandBlocksOtherMove;
    using playerbots::maintenance::MAINTENANCE_ERRAND_LEASE_MS;
    // Ensetsu's repairer drifted 35 -> 334 yards while the RPG loop steered (2026-09-01): a live
    // claim must refuse any other destination.
    EXPECT_TRUE(ErrandBlocksOtherMove(true, 0, false));
    EXPECT_TRUE(ErrandBlocksOtherMove(true, MAINTENANCE_ERRAND_LEASE_MS - 1, false));
    // The errand's own destination is always allowed.
    EXPECT_FALSE(ErrandBlocksOtherMove(true, 0, true));
    // A claim nobody refreshes lapses, so a silent owner cannot freeze the bot.
    EXPECT_FALSE(ErrandBlocksOtherMove(true, MAINTENANCE_ERRAND_LEASE_MS, false));
    // No errand, no yielding.
    EXPECT_FALSE(ErrandBlocksOtherMove(false, 0, false));
}

TEST(RandomBotMaintenancePolicyTest, TheFloorStipendCoversOnlyABrokeBotsShortfallOncePerCooldown)
{
    using playerbots::maintenance::STIPEND_MAX_COPPER;
    using playerbots::maintenance::STIPEND_PURSE_CEILING;
    using playerbots::maintenance::StipendAmount;
    using playerbots::maintenance::StipendFacts;

    // Colina, 2026-09-05: 0c, four broken pieces, repair 350c. She gets exactly the shortfall.
    StipendFacts facts;
    facts.hasBrokenEquipment = true;
    facts.cooldownElapsed = true;
    facts.purseCopper = 0;
    facts.repairCostCopper = 350;
    EXPECT_EQ(StipendAmount(facts), 350u);
    // A purse that covers part of it gets only the rest.
    facts.purseCopper = 100;
    EXPECT_EQ(StipendAmount(facts), 250u);
    // A purse that covers the repair gets nothing, however small.
    facts.purseCopper = 350;
    EXPECT_EQ(StipendAmount(facts), 0u);
    // Worn but not broken gear is not a floor case.
    facts.purseCopper = 0;
    facts.hasBrokenEquipment = false;
    EXPECT_EQ(StipendAmount(facts), 0u);
    facts.hasBrokenEquipment = true;
    // A bot with silver in its purse is not broke; it earns the rest.
    facts.purseCopper = STIPEND_PURSE_CEILING + 1;
    facts.repairCostCopper = STIPEND_PURSE_CEILING + 400;
    EXPECT_EQ(StipendAmount(facts), 0u);
    facts.purseCopper = STIPEND_PURSE_CEILING;
    EXPECT_EQ(StipendAmount(facts), 400u);
    // The grant is capped: a huge bill is covered in part and the bot repairs item by item.
    facts.purseCopper = 0;
    facts.repairCostCopper = STIPEND_MAX_COPPER * 3;
    EXPECT_EQ(StipendAmount(facts), STIPEND_MAX_COPPER);
    // One grant per cooldown, so the stipend cannot become an income.
    facts.cooldownElapsed = false;
    EXPECT_EQ(StipendAmount(facts), 0u);
}

TEST(RandomBotMaintenancePolicyTest, ALoneGreyDoesNotInterruptAForcedTrip)
{
    using playerbots::maintenance::VENDOR_BAG_SPACE_URGENT_PERCENT;
    using playerbots::maintenance::VendorTripWanted;

    // Smashlix, 2026-09-05: one grey looted 300 yards into a 1592 yard economy trip must not turn
    // the bot around. The same grey with no trip in flight still sends her to the vendor.
    EXPECT_FALSE(VendorTripWanted(40, true, true));
    EXPECT_TRUE(VendorTripWanted(40, true, false));
    // Nothing to sell, nothing to do, trip or not.
    EXPECT_FALSE(VendorTripWanted(40, false, false));
    // Bags too full to keep looting override the trip.
    EXPECT_TRUE(VendorTripWanted(VENDOR_BAG_SPACE_URGENT_PERCENT + 1, false, true));
    EXPECT_FALSE(VendorTripWanted(VENDOR_BAG_SPACE_URGENT_PERCENT, false, true));
}

TEST(RandomBotMaintenancePolicyTest, AHearthThatNeverLandsIsRetriedAMinuteApartThreeTimesThenDropped)
{
    using playerbots::maintenance::HEARTH_GIVE_UP_MS;
    using playerbots::maintenance::HEARTH_MAX_ATTEMPTS;
    using playerbots::maintenance::HEARTH_RETRY_MS;
    using playerbots::maintenance::HearthAttemptAllowed;

    // Coyahneblahe, 2026-09-05 09:00: the action reported success every ten seconds and the
    // stone never went on cooldown. The first attempt is free.
    EXPECT_TRUE(HearthAttemptAllowed(0, 0));
    // Ten seconds later, with no cooldown showing, no.
    EXPECT_FALSE(HearthAttemptAllowed(1, 10 * 1000));
    EXPECT_TRUE(HearthAttemptAllowed(1, HEARTH_RETRY_MS));
    EXPECT_TRUE(HearthAttemptAllowed(HEARTH_MAX_ATTEMPTS - 1, HEARTH_RETRY_MS));
    // After the cap, a minute is not enough; the stone's own cooldown length is.
    EXPECT_FALSE(HearthAttemptAllowed(HEARTH_MAX_ATTEMPTS, HEARTH_RETRY_MS));
    EXPECT_FALSE(HearthAttemptAllowed(HEARTH_MAX_ATTEMPTS, HEARTH_GIVE_UP_MS - 1));
    EXPECT_TRUE(HearthAttemptAllowed(HEARTH_MAX_ATTEMPTS, HEARTH_GIVE_UP_MS));
}

TEST(RandomBotMaintenancePolicyTest, AReadyHearthstoneIsSpentWheneverItShortensTheWalk)
{
    using playerbots::maintenance::HearthShortcutFacts;
    using playerbots::maintenance::HearthShortcutWorthwhile;

    HearthShortcutFacts facts;
    facts.hearthReady = true;
    facts.freeToCast = true;
    facts.destinationOnHomeMap = true;
    facts.botOnDestinationMap = true;
    facts.walkYards = 2000.0f;
    facts.homeYards = 400.0f;
    // A vendor 2000 yards away on foot (286 s) and 400 from the home inn (cast 10 + exit 20 +
    // 57 s): hearth.
    EXPECT_TRUE(HearthShortcutWorthwhile(facts));

    // Live 2026-09-05 08:27: a 172 yard walk (25 s) with the inn 18 yards from the goal. The
    // hearth route is 33 s, slower than walking, and it was cast under the old fixed floor.
    facts.walkYards = 172.0f;
    facts.homeYards = 18.0f;
    EXPECT_FALSE(HearthShortcutWorthwhile(facts));

    // The hearth route has to beat the walk by a full cast time. 300 to 100 yards: walk 43 s,
    // hearth 44 s, no. 400 to 100 yards: walk 57 s, hearth 44 s, saves 13 s, yes.
    facts.walkYards = 300.0f;
    facts.homeYards = 100.0f;
    EXPECT_FALSE(HearthShortcutWorthwhile(facts));
    facts.walkYards = 400.0f;
    EXPECT_TRUE(HearthShortcutWorthwhile(facts));

    // And it has to cut the trip at least in half, so the stone is kept for the walks it changes.
    // 800 to 400 yards is exactly half (and 27 s faster): yes. 800 to 401: no.
    facts.walkYards = 800.0f;
    facts.homeYards = 400.0f;
    EXPECT_TRUE(HearthShortcutWorthwhile(facts));
    facts.homeYards = 401.0f;
    EXPECT_FALSE(HearthShortcutWorthwhile(facts));

    // Home further from the goal than the bot already is: walk.
    facts.homeYards = 3000.0f;
    EXPECT_FALSE(HearthShortcutWorthwhile(facts));

    // The bot is on another continent and the goal is on the home map: the stone is the way there,
    // whatever the meaningless cross-map distance says.
    facts.botOnDestinationMap = false;
    facts.walkYards = 0.0f;
    EXPECT_TRUE(HearthShortcutWorthwhile(facts));

    // The goal is not on the home map: the stone cannot help.
    facts.destinationOnHomeMap = false;
    EXPECT_FALSE(HearthShortcutWorthwhile(facts));

    // Stone on cooldown, or a cast that combat or a taxi would break: walk.
    facts.destinationOnHomeMap = true;
    facts.botOnDestinationMap = true;
    facts.walkYards = 2000.0f;
    facts.homeYards = 400.0f;
    facts.hearthReady = false;
    EXPECT_FALSE(HearthShortcutWorthwhile(facts));
    facts.hearthReady = true;
    facts.freeToCast = false;
    EXPECT_FALSE(HearthShortcutWorthwhile(facts));
}
}  // namespace
