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

/*
 * Whether worn gear justifies a repair trip. Merely worn gear waits until the bot can pay for
 * the whole repair. Broken gear (an item at zero durability) goes regardless of the purse: the
 * economy stands aside while gear is broken and a bot with broken weapons cannot kill, so the
 * coins never arrive on their own. Measured live 2026-09-01: Vavapu, level 8 for 14.8 of her
 * 19.8 played hours, 40c, four items at zero durability, 300 s at a quest POI with 18 targets
 * and no kill. The repairer repairs item by item, so a small purse still buys a weapon back.
 */
[[nodiscard]] inline bool RepairTripWorthPlanning(bool hasBrokenEquipment, std::uint32_t repairCost,
                                                  std::uint32_t repairBudget)
{
    if (hasBrokenEquipment)
        return true;
    return repairCost && repairCost <= repairBudget;
}
/*
 * What a visit to the repairer achieved. The generic repair action reports success whenever a
 * repairer is in reach, whatever the purse bought, so a bot with two copper and three broken
 * items heard "repaired" every five seconds and never backed off (Vavapu, 2026-09-01, 22:00:
 * "sold first ... money 2c" then "repaired" in an endless five second loop, gear still broken).
 * The verdict here reads the gear instead of the action's word.
 *
 * WeaponStarved is the case the purse must be protected in: the weapon is the item a bot cannot
 * fight without, the repairer takes weapons first but skips one it cannot afford, and the generic
 * pass then spends whatever is left on armour. Vavapu's 40c went to a vest and gloves while her
 * axe and gun stayed at zero. When a weapon is still broken after the weapon pass, nothing else
 * is bought: the bot leaves with its coins and comes back when it can afford the weapon.
 */
enum class RepairVisitOutcome : std::uint8_t
{
    Repaired,
    WeaponStarved,
    Unaffordable
};

[[nodiscard]] inline RepairVisitOutcome RepairVisitVerdict(bool weaponStillBroken, std::uint32_t durabilityBefore,
                                                           std::uint32_t durabilityAfter)
{
    if (weaponStillBroken)
        return RepairVisitOutcome::WeaponStarved;
    if (durabilityAfter > durabilityBefore)
        return RepairVisitOutcome::Repaired;
    return RepairVisitOutcome::Unaffordable;
}
[[nodiscard]] bool IsVendorTrash(std::uint32_t quality, bool vendorUsage);

/*
 * Whether a vendor trip should start now.
 *
 * A single grey in the bags used to be enough, whatever the bot was doing. Smashlix, 2026-09-05,
 * a level 18 hunter with a forced economy trip to Astranaar 1592 yards away: every 300 yards out
 * she looted one grey, the vendor trigger walked her back to the Stonetalon Peak vendor, and the
 * trip never advanced (11 vendor trips and 4 stuck teleports in 33 minutes). While a forced
 * travel target is held, only bags that are too full to keep looting justify the detour; a lone
 * grey waits until the trip is over. Pierre: put the vendor trigger fix in this deploy.
 */
inline constexpr std::uint32_t VENDOR_BAG_SPACE_URGENT_PERCENT = 80;

[[nodiscard]] inline bool VendorTripWanted(std::uint32_t bagSpacePercent, bool hasVendorTrash,
                                           bool forcedTripInFlight)
{
    if (bagSpacePercent > VENDOR_BAG_SPACE_URGENT_PERCENT)
        return true;
    return hasVendorTrash && !forcedTripInFlight;
}

/*
 * The repair floor stipend.
 *
 * A bot at zero copper with broken gear has no way back: repairs need coin, coin comes from quest
 * turn-ins and grey loot, and the same bot that cannot repair is the one that finishes no quests.
 * Colina, 2026-09-05, level 13 for 14.6 hours, 0c, four broken pieces, five "unaffordable" repair
 * visits in half an hour. Pierre: implement a floor stipend, and watch how often it triggers so the
 * bots do not stop earning because they rely on it.
 *
 * Deliberately stingy: only a bot with a broken item (not merely worn gear), only when its purse
 * is under STIPEND_PURSE_CEILING, only the shortfall up to STIPEND_MAX_COPPER, and at most once
 * per STIPEND_COOLDOWN_MS for that bot. Every grant is logged with the bot's running grant count
 * so the dashboard can count grants per window and repeat recipients since restart.
 */
inline constexpr std::uint32_t STIPEND_PURSE_CEILING = 500;
inline constexpr std::uint32_t STIPEND_MAX_COPPER = 2000;
inline constexpr std::uint32_t STIPEND_COOLDOWN_MS = 6 * 60 * 60 * 1000;

struct StipendFacts
{
    bool hasBrokenEquipment = false;
    // STIPEND_COOLDOWN_MS has passed since this bot's last grant, or it never had one.
    bool cooldownElapsed = false;
    std::uint32_t purseCopper = 0;
    std::uint32_t repairCostCopper = 0;
};

// Copper to grant now; zero means no stipend.
[[nodiscard]] inline std::uint32_t StipendAmount(StipendFacts const& facts)
{
    if (!facts.hasBrokenEquipment || !facts.cooldownElapsed || facts.purseCopper > STIPEND_PURSE_CEILING)
        return 0;
    if (facts.repairCostCopper <= facts.purseCopper)
        return 0;
    std::uint32_t const shortfall = facts.repairCostCopper - facts.purseCopper;
    return shortfall < STIPEND_MAX_COPPER ? shortfall : STIPEND_MAX_COPPER;
}

[[nodiscard]] MountTier RequiredMountTier(std::uint32_t level, MountLevelThresholds const& thresholds);
[[nodiscard]] std::uint32_t RequiredRidingSkill(MountTier tier);
[[nodiscard]] std::uint32_t NextRidingSpell(std::uint32_t ridingSkill, MountTier targetTier);
[[nodiscard]] std::uint32_t RidingTrainerEntry(std::uint8_t race, std::uint8_t team, std::uint32_t ridingSpellId);
[[nodiscard]] std::vector<std::uint32_t> AllowedMountSpells(std::uint8_t race, std::uint8_t team, MountTier tier);
[[nodiscard]] bool MountMeetsTier(MountTier tier, bool flying, std::int32_t speed);
/*
 * One learned spell, reduced to the facts that decide whether it is a mount of a given tier.
 *
 * Deliberately booleans rather than raw aura ids: the caller does the comparison against the core's
 * SPELL_AURA_* constants, which keeps this file free of core headers and therefore testable with
 * plain structs and no fixture.
 *
 * Extracted from LearnedMountMeetsTier so the rule can be tested at all. That function takes a
 * Player and walks its spellbook, so it needed a fixture nothing here has, and the tier rule went
 * uncovered as a result. Split this way the probing stays in the caller and the decision is a pure
 * function over plain data.
 */
struct MountSpellEffects
{
    bool mountAura = false;
    bool passive = false;
    bool active = true;
    bool flightSpeedAura = false;
    bool alwaysFlying = false;
    std::int32_t speed1 = 0;
    std::int32_t speed2 = 0;
};

[[nodiscard]] bool MountSpellMeetsTier(MountTier tier, MountSpellEffects const& effects);
[[nodiscard]] std::uint32_t SelectMountItem(std::uint8_t race, std::uint8_t team, MountTier tier, std::uint32_t budget,
                                            std::vector<MountVendorCandidate> const& candidates);

// Whether a routine maintenance trip must wait because the bot is working a quest. Maintenance
// triggers run at relevance 103 to 105 and own the bot outright, so a vendor errand fired
// mid-quest drags the bot straight off its objective: measured live 2026-08-29, a bot that had
// just reached the Lazy Peons POI at 0 yards was pulled 157 yards to a trash vendor before it
// could swing once. Urgent needs (broken gear, bags too full to loot) still go immediately;
// everything else keeps until the quest status ends.
[[nodiscard]] bool DeferRoutineMaintenanceDuringQuest(bool doingQuest, bool urgent);

/*
 * Spend the hearthstone whenever it shortens a far walk.
 *
 * Until 2026-09-05 the only thing that ever cast a hearthstone was the repair errand, and only
 * once the bot had broken gear and no repairer within walking range: zero casts in a full day of
 * logs, while about 78 of 200 bots at any instant were walking to a vendor, an auctioneer or a
 * quest across the map. Every bot carries the stone and rebinds at the inns it wanders past, so
 * the home inn is usually somewhere on the way. Pierre, 2026-09-05: any time the hearthstone is
 * available and could cut travel time, it should be used.
 *
 * The saving must cover the cast (ten seconds, roughly 70 yards of running) with margin for the
 * walk out of the inn, so a destination only slightly closer to home than to the bot is still
 * walked. A destination on the home map while the bot is on another map is always a saving.
 */
inline constexpr float HEARTH_SHORTCUT_MIN_SAVING_YARDS = 150.0f;

struct HearthShortcutFacts
{
    // HearthstoneReady: the bot holds the stone and it is off cooldown.
    bool hearthReady = false;
    // Nothing that a ten second cast would break: alive, out of combat, not already casting, not on
    // a taxi.
    bool freeToCast = false;
    // The destination lies on the home inn's map. Off that map the stone cannot help.
    bool destinationOnHomeMap = false;
    // The bot itself stands on the destination's map. When false, walkYards is meaningless and the
    // hearth is the only way there.
    bool botOnDestinationMap = false;
    // Bot to destination, as the crow flies.
    float walkYards = 0.0f;
    // Home inn to destination, as the crow flies.
    float homeYards = 0.0f;
};

[[nodiscard]] inline bool HearthShortcutWorthwhile(HearthShortcutFacts const& facts)
{
    if (!facts.hearthReady || !facts.freeToCast || !facts.destinationOnHomeMap)
        return false;
    if (!facts.botOnDestinationMap)
        return true;
    return facts.walkYards - facts.homeYards >= HEARTH_SHORTCUT_MIN_SAVING_YARDS;
}
}  // namespace playerbots::maintenance

#endif
