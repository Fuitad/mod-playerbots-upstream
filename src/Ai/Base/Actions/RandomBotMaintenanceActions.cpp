/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#include "RandomBotMaintenanceActions.h"

#include "Ai/World/Rpg/QuestStartItemPolicy.h"

#include "MaintenanceErrand.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <unordered_map>
#include <mutex>
#include <unordered_set>

#include "Bag.h"
#include "BudgetValues.h"
#include "ChatHelper.h"
#include "Event.h"
#include "GameEventMgr.h"
#include "ItemUsageValue.h"
#include "NPCPackets.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "Trainer.h"
#include "TravelMgr.h"

using namespace playerbots::maintenance;

namespace
{
struct NpcDestination
{
    uint32 entry = 0;
    uint32 itemId = 0;
    WorldPosition position;
};

MountLevelThresholds GetMountLevels()
{
    return {
        sPlayerbotAIConfig.useGroundMountAtMinLevel,
        sPlayerbotAIConfig.useFastGroundMountAtMinLevel,
        sPlayerbotAIConfig.useFlyMountAtMinLevel,
        sPlayerbotAIConfig.useFastFlyMountAtMinLevel,
    };
}

template <typename Visitor>
bool VisitBagItems(Player* bot, Visitor&& visitor)
{
    for (uint32 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            if (visitor(item))
                return true;
    }

    for (uint32 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
    {
        Item* bagItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bagSlot);
        Bag* bag = bagItem ? bagItem->ToBag() : nullptr;
        if (!bag)
            continue;

        for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
        {
            if (Item* item = bag->GetItemByPos(slot))
                if (visitor(item))
                    return true;
        }
    }

    return false;
}

bool IsFriendlyNpc(PlayerbotAI* botAI, CreatureTemplate const* creatureTemplate)
{
    FactionTemplateEntry const* faction = sFactionTemplateStore.LookupEntry(creatureTemplate->faction);
    if (!faction)
        return false;

    return Unit::GetFactionReactionTo(botAI->GetBot()->GetFactionTemplateEntry(), faction) >= REP_NEUTRAL;
}

// Spawned npcs a maintenance trip can target, keyed by map. The old TravelMgr rpg destination table is
// never loaded in this fork (LoadQuestTravelTable has no caller), so the cache is built here once from the
// creature spawn data, the same source PrepareDestinationCache reads.
struct MaintenanceNpcSpawn
{
    uint32 entry = 0;
    WorldPosition position;
};

std::unordered_map<uint32, std::vector<MaintenanceNpcSpawn>> const& MaintenanceNpcSpawns()
{
    static std::unordered_map<uint32, std::vector<MaintenanceNpcSpawn>> spawns;
    static bool built = false;
    if (built)
        return spawns;

    built = true;
    // Seasonal spawns (game_event_creature) sit in the spawn table all year but only exist in the
    // world during their event. Live on 2026-08-23 bots walked to Brewfest and Hallow's End vendors
    // and stood on an empty spot, so every event bound guid is left out regardless of event state.
    std::unordered_set<ObjectGuid::LowType> eventBound;
    for (auto const& guids : sGameEventMgr->GameEventCreatureGuids)
        eventBound.insert(guids.begin(), guids.end());

    uint32 const wanted = UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_REPAIR | UNIT_NPC_FLAG_TRAINER;
    for (auto const& [guid, creatureData] : sObjectMgr->GetAllCreatureData())
    {
        if (eventBound.contains(guid))
            continue;
        CreatureTemplate const* creatureTemplate = sObjectMgr->GetCreatureTemplate(creatureData.id);
        if (!creatureTemplate || (creatureTemplate->npcflag & wanted) == 0)
            continue;
        if (creatureData.spawnMask == 0 || creatureData.movementType != IDLE_MOTION_TYPE)
            continue;
        spawns[creatureData.mapid].push_back(
            {creatureData.id,
             WorldPosition(creatureData.mapid, creatureData.posX, creatureData.posY, creatureData.posZ)});
    }
    uint32 total = 0;
    for (auto const& [mapId, list] : spawns)
        total += list.size();
    LOG_INFO("playerbots", "Random bot maintenance cached {} vendor, repair and trainer spawns across {} maps.",
             total, spawns.size());
    return spawns;
}

bool FindNearestDestination(PlayerbotAI* botAI, std::function<bool(uint32)> const& acceptsEntry,
                            NpcDestination& selected)
{
    Player* bot = botAI->GetBot();
    auto const& byMap = MaintenanceNpcSpawns();
    auto const it = byMap.find(bot->GetMapId());
    if (it == byMap.end())
        return false;

    float nearestDistance = std::numeric_limits<float>::max();
    std::unordered_map<uint32, bool> friendlyByEntry;
    for (MaintenanceNpcSpawn const& spawn : it->second)
    {
        if (!acceptsEntry(spawn.entry))
            continue;

        float const distance = bot->GetDistance(spawn.position.GetPositionX(), spawn.position.GetPositionY(), spawn.position.GetPositionZ());
        if (distance >= nearestDistance)
            continue;

        auto friendly = friendlyByEntry.find(spawn.entry);
        if (friendly == friendlyByEntry.end())
        {
            CreatureTemplate const* creatureTemplate = sObjectMgr->GetCreatureTemplate(spawn.entry);
            friendly = friendlyByEntry
                           .emplace(spawn.entry, creatureTemplate && IsFriendlyNpc(botAI, creatureTemplate))
                           .first;
        }
        if (!friendly->second)
            continue;

        nearestDistance = distance;
        selected.entry = spawn.entry;
        selected.position = spawn.position;
    }

    return selected.entry != 0;
}

bool HasNpcFlag(uint32 entry, NPCFlags flag)
{
    CreatureTemplate const* creatureTemplate = sObjectMgr->GetCreatureTemplate(entry);
    return creatureTemplate && (creatureTemplate->npcflag & flag) != 0;
}

uint32 MountSpellForItem(ItemTemplate const* itemTemplate, std::vector<uint32> const& allowedSpells)
{
    if (!itemTemplate || itemTemplate->Class != ITEM_CLASS_MISC || itemTemplate->SubClass != ITEM_SUBCLASS_JUNK_MOUNT)
    {
        return 0;
    }

    for (_Spell const& effect : itemTemplate->Spells)
    {
        if (std::find(allowedSpells.begin(), allowedSpells.end(), effect.SpellId) != allowedSpells.end())
            return effect.SpellId;
    }

    return 0;
}

bool LearnedMountMeetsTier(Player* bot, MountTier tier)
{
    for (auto const& entry : bot->GetSpellMap())
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(entry.first);
        if (!spellInfo)
            continue;

        // The tier decision lives in MountSpellMeetsTier, which is pure and unit tested; only the
        // SpellInfo probing is local. Any other reader of the spellbook should call that predicate
        // rather than repeat the effect-index layout below.
        MountSpellEffects effects;
        effects.mountAura = spellInfo->Effects[0].ApplyAuraName == SPELL_AURA_MOUNTED;
        effects.passive = spellInfo->IsPassive();
        effects.active = entry.second->State != PLAYERSPELL_REMOVED && entry.second->Active;
        effects.flightSpeedAura =
            spellInfo->Effects[1].ApplyAuraName == SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED ||
            spellInfo->Effects[2].ApplyAuraName == SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED;
        effects.alwaysFlying = spellInfo->Id == 54729;
        effects.speed1 = spellInfo->Effects[1].BasePoints;
        effects.speed2 = spellInfo->Effects[2].BasePoints;

        if (MountSpellMeetsTier(tier, effects))
            return true;
    }

    return false;
}

uint32 FindCarriedMountItem(PlayerbotAI* botAI, MountTier tier)
{
    Player* bot = botAI->GetBot();
    std::vector<uint32> const allowedSpells = AllowedMountSpells(bot->getRace(), bot->GetTeamId(), tier);
    uint32 itemId = 0;
    VisitBagItems(
        bot,
        [bot, &allowedSpells, &itemId](Item* item)
        {
            if (bot->CanUseItem(item) != EQUIP_ERR_OK || !MountSpellForItem(item->GetTemplate(), allowedSpells))
                return false;

            itemId = item->GetEntry();
            return true;
        });
    return itemId;
}

bool FindMountVendorDestination(PlayerbotAI* botAI, MountTier tier, NpcDestination& selected)
{
    Player* bot = botAI->GetBot();
    AiObjectContext* context = botAI->GetAiObjectContext();
    uint32 const budget = AI_VALUE2(uint32, "free money for", static_cast<uint32>(NeedMoneyFor::anything));
    if (!budget)
        return false;

    std::vector<uint32> const allowedSpells = AllowedMountSpells(bot->getRace(), bot->GetTeamId(), tier);
    WorldPosition botPosition(bot);
    float nearestDistance = std::numeric_limits<float>::max();

    for (TravelDestination* destination : sTravelMgr.getRpgTravelDestinations(bot, true, true, 200000.0f))
    {
        uint32 const entry = destination->getEntry();
        CreatureTemplate const* creatureTemplate = sObjectMgr->GetCreatureTemplate(entry);
        if (!entry || !creatureTemplate || !(creatureTemplate->npcflag & UNIT_NPC_FLAG_VENDOR) ||
            !IsFriendlyNpc(botAI, creatureTemplate))
        {
            continue;
        }

        VendorItemData const* vendorItems = sObjectMgr->GetNpcVendorItemList(entry);
        if (!vendorItems)
            continue;

        std::vector<MountVendorCandidate> candidates;
        float const discount =
            bot->GetReputationPriceDiscount(sFactionTemplateStore.LookupEntry(creatureTemplate->faction));
        for (VendorItem const* vendorItem : vendorItems->m_items)
        {
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(vendorItem->item);
            if (!itemTemplate || vendorItem->ExtendedCost || bot->CanUseItem(itemTemplate) != EQUIP_ERR_OK)
                continue;

            uint32 const mountSpell = MountSpellForItem(itemTemplate, allowedSpells);
            if (!mountSpell)
                continue;

            uint32 const price = static_cast<uint32>(std::floor(itemTemplate->BuyPrice * discount));
            candidates.push_back({itemTemplate->ItemId, mountSpell, price, true});
        }

        uint32 const itemId = SelectMountItem(bot->getRace(), bot->GetTeamId(), tier, budget, candidates);
        if (!itemId)
            continue;

        std::vector<WorldPosition*> const points = destination->nextPoint(&botPosition, true);
        if (points.empty())
            continue;

        float const distance = destination->distanceTo(&botPosition);
        if (distance >= nearestDistance)
            continue;

        nearestDistance = distance;
        selected.entry = entry;
        selected.itemId = itemId;
        selected.position = *points.front();
    }

    return selected.entry != 0;
}

Creature* FindNearbyNpc(PlayerbotAI* botAI, uint32 entry, NPCFlags flag)
{
    AiObjectContext* context = botAI->GetAiObjectContext();
    for (ObjectGuid const guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Creature* creature = botAI->GetBot()->GetNPCIfCanInteractWith(guid, flag);
        if (creature && (!entry || creature->GetEntry() == entry))
            return creature;
    }

    return nullptr;
}

bool TrainRiding(PlayerbotAI* botAI, Creature* trainerNpc, uint32 ridingSpell)
{
    AiObjectContext* context = botAI->GetAiObjectContext();
    Trainer::Trainer* trainer = sObjectMgr->GetTrainer(trainerNpc->GetEntry());
    Trainer::Spell const* trainerSpell = trainer ? trainer->GetSpell(ridingSpell) : nullptr;
    if (!trainerSpell || !trainer->CanTeachSpell(botAI->GetBot(), trainerSpell))
        return false;

    float const discount = botAI->GetBot()->GetReputationPriceDiscount(trainerNpc);
    uint32 const price = static_cast<uint32>(std::floor(trainerSpell->MoneyCost * discount));
    if (price > AI_VALUE2(uint32, "free money for", static_cast<uint32>(NeedMoneyFor::spells)))
        return false;

    WorldPacket packet(CMSG_TRAINER_BUY_SPELL, 12);
    packet << trainerNpc->GetGUID() << ridingSpell;
    WorldPackets::NPC::TrainerBuySpell buySpell(std::move(packet));
    buySpell.Read();
    botAI->GetBot()->GetSession()->HandleTrainerBuySpellOpcode(buySpell);
    return botAI->GetBot()->HasSpell(ridingSpell);
}
}  // namespace

bool playerbots::maintenance::IsEligible(PlayerbotAI* botAI)
{
    return botAI && sPlayerbotAIConfig.economyManagedSupplies && sRandomPlayerbotMgr.IsRandomBot(botAI->GetBot()) &&
           !botAI->IsAltBot();
}

bool playerbots::maintenance::NeedsRepair(PlayerbotAI* botAI)
{
    if (!IsEligible(botAI))
        return false;

    Player* bot = botAI->GetBot();
    AiObjectContext* context = botAI->GetAiObjectContext();
    bool worn = false;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (item &&
            ShouldRepairItem(item->GetUInt32Value(ITEM_FIELD_DURABILITY),
                             item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY), sPlayerbotAIConfig.economyRepairThreshold))
        {
            worn = true;
            break;
        }
    }

    if (!worn)
        return false;

    uint32 const repairCost = AI_VALUE(uint32, "repair cost");
    return RepairTripWorthPlanning(HasBrokenEquipment(botAI), repairCost,
                                   AI_VALUE2(uint32, "free money for", static_cast<uint32>(NeedMoneyFor::repair)));
}

/*
 * Whether any equipped item is actually at zero durability, as opposed to merely worn.
 *
 * NeedsRepair covers both, because passing a repairer with a scuffed sword is worth a stop. This
 * is the urgent half: at zero durability the item contributes nothing, so a bot in this state
 * cannot win a fight and will keep dying until it is repaired. Only this justifies a hearth.
 */
bool playerbots::maintenance::DoingQuestNow(PlayerbotAI* botAI)
{
    return botAI && botAI->rpgInfo.GetStatus() == RPG_DO_QUEST;
}

bool playerbots::maintenance::CriticallyFullBags(PlayerbotAI* botAI)
{
    if (!botAI)
        return false;

    AiObjectContext* context = botAI->GetAiObjectContext();
    return AI_VALUE(uint8, "bag space") > 95;
}

/*
 * The two gear readings RepairVisitVerdict is fed: every equipped item's durability summed, and
 * whether any weapon slot is still at zero. Both read the items directly so the verdict cannot be
 * fooled by an action that reports success without spending anything.
 */
uint32 playerbots::maintenance::EquippedDurabilitySum(Player* bot)
{
    uint32 total = 0;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            total += item->GetUInt32Value(ITEM_FIELD_DURABILITY);
    return total;
}

bool playerbots::maintenance::HasBrokenWeapon(Player* bot)
{
    for (uint8 const slot : {EQUIPMENT_SLOT_MAINHAND, EQUIPMENT_SLOT_RANGED, EQUIPMENT_SLOT_OFFHAND})
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (item && item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY) > 0 &&
            item->GetUInt32Value(ITEM_FIELD_DURABILITY) == 0)
            return true;
    }
    return false;
}

bool playerbots::maintenance::HasBrokenEquipment(PlayerbotAI* botAI)
{
    if (!IsEligible(botAI))
        return false;

    Player* bot = botAI->GetBot();
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (item && item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY) > 0 &&
            item->GetUInt32Value(ITEM_FIELD_DURABILITY) == 0)
        {
            return true;
        }
    }
    return false;
}

/*
 * Whether the hearth can actually be spent right now: the bot holds one and it is off cooldown.
 *
 * Checked before choosing the plan rather than after, so a bot with no hearth is reported Stranded
 * and logged, instead of silently attempting an action that cannot fire and looking like it simply
 * declined to repair.
 */
bool playerbots::maintenance::HearthstoneReady(Player* bot)
{
    if (!bot || bot->InBattleground())
        return false;

    Item* const hearthstone = bot->GetItemByEntry(HEARTHSTONE_ITEM_ID);
    if (!hearthstone)
        return false;

    return !bot->HasSpellCooldown(HEARTHSTONE_SPELL_ID);
}

bool playerbots::maintenance::HearthShortcutFor(Player* bot, WorldPosition const& destination)
{
    if (!bot || destination == WorldPosition())
        return false;

    HearthShortcutFacts facts;
    facts.hearthReady = HearthstoneReady(bot);
    facts.freeToCast = bot->IsAlive() && !bot->IsInCombat() && !bot->IsInFlight() && !bot->IsNonMeleeSpellCast(false);
    facts.destinationOnHomeMap = destination.GetMapId() == bot->m_homebindMapId;
    facts.botOnDestinationMap = destination.GetMapId() == bot->GetMapId();
    facts.walkYards = bot->GetDistance(destination);
    facts.homeYards = destination.GetExactDist(bot->m_homebindX, bot->m_homebindY, bot->m_homebindZ);
    if (!HearthShortcutWorthwhile(facts))
        return false;

    LOG_INFO("playerbots",
             "[Maintenance] {} hearthing to cut travel: destination {:.0f} yd away on foot, {:.0f} yd from home",
             bot->GetName(), facts.botOnDestinationMap ? facts.walkYards : -1.0f, facts.homeYards);
    return true;
}

bool playerbots::maintenance::NeedsVendor(PlayerbotAI* botAI)
{
    if (!IsEligible(botAI))
        return false;

    AiObjectContext* context = botAI->GetAiObjectContext();
    uint8 const bagSpace = AI_VALUE(uint8, "bag space");
    // A forced travel target is a trip somebody owns (economy errands force theirs); see
    // VendorTripWanted for why a lone grey must not interrupt it.
    TravelTarget* const travel = AI_VALUE(TravelTarget*, "travel target");
    bool const forcedTripInFlight = travel && travel->isForced() && travel->isActive();
    if (VendorTripWanted(bagSpace, false, forcedTripInFlight))
        return true;

    bool const hasVendorTrash = VisitBagItems(
        botAI->GetBot(),
        [botAI](Item* item)
        {
            ItemUsage const usage =
                botAI->GetAiObjectContext()->GetValue<ItemUsage>("item usage", item->GetEntry())->Get();
            return IsVendorTrash(item->GetTemplate()->Quality, usage == ITEM_USAGE_VENDOR);
        });
    return VendorTripWanted(bagSpace, hasVendorTrash, forcedTripInFlight);
}

bool playerbots::maintenance::NeedsMount(PlayerbotAI* botAI)
{
    if (!IsEligible(botAI))
        return false;

    Player* bot = botAI->GetBot();
    MountTier const tier = RequiredMountTier(bot->GetLevel(), GetMountLevels());
    return tier != MountTier::None && !LearnedMountMeetsTier(bot, tier);
}

bool RandomBotRepairAction::Execute(Event /*event*/)
{
    if (!NeedsRepair(botAI))
        return false;

    if (unaffordableAt && GetMSTimeDiffToNow(unaffordableAt) < 300000)
        return false;

    if (Creature* repairer = FindNearbyNpc(botAI, targetEntry, UNIT_NPC_FLAG_REPAIR))
    {
        // A purse below the repair cost sells first when the repairer also buys: the economy
        // stands aside while gear is broken, so nothing else turns Vavapu's ore and belts into
        // the coins her blunderbuss needs (RandomBotMaintenancePolicy.h, RepairTripWorthPlanning).
        AiObjectContext* context = botAI->GetAiObjectContext();
        if (repairer->IsVendor() &&
            AI_VALUE(uint32, "repair cost") >
                AI_VALUE2(uint32, "free money for", static_cast<uint32>(NeedMoneyFor::repair)))
        {
            bool const soldGray = botAI->DoSpecificAction("sell", Event("random bot repair", "gray"), true);
            bool const soldVendor = botAI->DoSpecificAction("sell", Event("random bot repair", "vendor"), true);
            LOG_DEBUG("playerbots", "[Maintenance] {} repair: sold first at npc {} gray={} vendor={} money {}c",
                      bot->GetName(), targetEntry, soldGray, soldVendor, bot->GetMoney());
        }
        // The verdict reads the gear, not the repair action's return value, which is true whenever
        // a repairer is in reach (RandomBotMaintenancePolicy.h, RepairVisitVerdict). Weapons go
        // first on their own, and a weapon the purse cannot cover ends the visit before the generic
        // pass can spend the coins on armour.
        uint32 const durabilityBefore = EquippedDurabilitySum(bot);
        float const discount = bot->GetReputationPriceDiscount(repairer);
        // Player::DurabilityRepair takes a packed position (bag << 8 | slot); a bare equipment slot
        // resolves to bag 0, finds nothing, and repairs nothing. Upstream's own weapons-first lines
        // in RepairAllAction pass the bare slot and have never repaired a weapon; measured live
        // 2026-09-01 22:50, Vavapu at 368c with a 1c axe still "unaffordable".
        for (uint8 const slot : {EQUIPMENT_SLOT_MAINHAND, EQUIPMENT_SLOT_RANGED, EQUIPMENT_SLOT_OFFHAND})
            (void)bot->DurabilityRepair(static_cast<uint16>((INVENTORY_SLOT_BAG_0 << 8) | slot), true, discount,
                                        false);
        bool const weaponStillBroken = HasBrokenWeapon(bot);
        if (!weaponStillBroken)
            (void)botAI->DoSpecificAction("repair", Event("random bot repair"), true);
        switch (RepairVisitVerdict(weaponStillBroken, durabilityBefore, EquippedDurabilitySum(bot)))
        {
            case RepairVisitOutcome::Repaired:
                LOG_DEBUG("playerbots", "[Maintenance] {} repair: repaired at npc {} money {}c", bot->GetName(),
                          targetEntry, bot->GetMoney());
                targetEntry = 0;
                unaffordableAt = 0;
                playerbots::maintenance::ReleaseErrand(bot);
                return true;
            case RepairVisitOutcome::WeaponStarved:
                LOG_DEBUG("playerbots",
                          "[Maintenance] {} repair: weapon unaffordable at npc {} with {}c, keeping the purse",
                          bot->GetName(), targetEntry, bot->GetMoney());
                break;
            case RepairVisitOutcome::Unaffordable:
                // Standing at the repairer with nothing repaired means the purse cannot cover one
                // item even after selling. Parking here with the errand claimed would freeze the
                // bot; let it go and earn, and replan in five minutes.
                LOG_DEBUG("playerbots", "[Maintenance] {} repair: unaffordable at npc {} with {}c, backing off",
                          bot->GetName(), targetEntry, bot->GetMoney());
                break;
        }
        targetEntry = 0;
        unaffordableAt = getMSTime();
        playerbots::maintenance::ReleaseErrand(bot);
        return false;
    }

    /*
     * A latched target is re-checked, not trusted. The plan used to be consulted only when there was
     * no target at all, so a repairer chosen while the bot stood next to it stayed selected after the
     * bot travelled away on other business, and the walking bound never applied again. Live on
     * 2026-08-25 that left a bot calling MoveFarTo against a repairer 6978 yards away, stationary,
     * every tick: the exact march this action exists to prevent, reached by the back door.
     */
    if (targetEntry)
    {
        RepairPlan const latched = ChooseRepairPlan(true, HasBrokenEquipment(botAI), true,
                                                   bot->GetDistance(targetPosition), HearthstoneReady(bot));
        if (latched != RepairPlan::Travel)
        {
            LOG_DEBUG("playerbots", "[Maintenance] {} repair: dropping stale target {} now {:.0f} yd away",
                      bot->GetName(), targetEntry, bot->GetDistance(targetPosition));
            targetEntry = 0;
            playerbots::maintenance::ReleaseErrand(bot);
        }
    }

    if (!targetEntry)
    {
        NpcDestination destination;
        bool const found = FindNearestDestination(
            botAI, [](uint32 entry) { return HasNpcFlag(entry, UNIT_NPC_FLAG_REPAIR); }, destination);
        float const distance = found ? bot->GetDistance(destination.position) : 0.0f;
        RepairPlan const plan =
            ChooseRepairPlan(true, HasBrokenEquipment(botAI), found, distance, HearthstoneReady(bot));

        switch (plan)
        {
            case RepairPlan::Travel:
                targetEntry = destination.entry;
                targetPosition = destination.position;
                LOG_DEBUG("playerbots", "[Maintenance] {} repair: heading to npc {} at {:.0f} yd", bot->GetName(),
                          targetEntry, distance);
                break;
            case RepairPlan::Hearth:
                /*
                 * Nothing within walking distance and the gear is broken, so the bot cannot fight
                 * its way anywhere. An inn always has a repairer, so the hearth converts an
                 * unreachable errand into a reachable one instead of a long march it will die on.
                 */
                LOG_INFO("playerbots",
                         "[Maintenance] {} repair: nearest repairer {:.0f} yd away with broken gear, hearthing",
                         bot->GetName(), found ? distance : -1.0f);
                return botAI->DoSpecificAction("hearthstone", Event("random bot repair"), true);
            case RepairPlan::Stranded:
                LOG_WARN("playerbots",
                         "[Maintenance] {} repair: broken gear, nearest repairer {:.0f} yd, hearth unavailable",
                         bot->GetName(), found ? distance : -1.0f);
                return false;
            case RepairPlan::None:
                LOG_DEBUG("playerbots", "[Maintenance] {} repair: no reachable repair destination from map {} zone {}",
                          bot->GetName(), bot->GetMapId(), bot->GetZoneId());
                return false;
        }
    }

    // The claim makes every other movement request yield until the errand ends or its lease lapses;
    // see MaintenanceErrandPolicy.h for the measurement.
    playerbots::maintenance::ClaimErrand(bot, targetPosition);
    bool const moving = MoveFarTo(targetPosition);
    if (!moving)
        LOG_DEBUG("playerbots", "[Maintenance] {} repair: MoveFarTo returned false, npc {} at {:.0f} yd, isMoving={}",
                  bot->GetName(), targetEntry, bot->GetDistance(targetPosition), bot->isMoving());

    /*
     * Still on the way counts as working. MoveFarTo reports false while the bot is closing normally
     * (observed at six yards from the vendor), so returning it verbatim told every caller the repair
     * had failed while it was in fact about to succeed.
     */
    return moving || bot->isMoving();
}

namespace
{
// Items whose accept was dispatched and did not take, per bot. Cleared only by a restart, which is
// the right lifetime: the reasons an accept fails are not things that change minute to minute, and
// an unbounded retry is what turned this errand into a hot loop.
std::mutex questStartItemRefusalMutex;
std::unordered_map<ObjectGuid::LowType, std::unordered_set<ObjectGuid::LowType>> questStartItemRefusals;
}  // namespace

void playerbots::maintenance::MarkQuestStartItemRefused(Player* bot, ObjectGuid item)
{
    std::lock_guard<std::mutex> lock(questStartItemRefusalMutex);
    questStartItemRefusals[bot->GetGUID().GetCounter()].insert(item.GetCounter());
}

bool playerbots::maintenance::QuestStartItemRefused(Player* bot, ObjectGuid item)
{
    std::lock_guard<std::mutex> lock(questStartItemRefusalMutex);
    auto it = questStartItemRefusals.find(bot->GetGUID().GetCounter());
    return it != questStartItemRefusals.end() && it->second.count(item.GetCounter()) != 0;
}

Item* playerbots::maintenance::FindUsableQuestStartItem(PlayerbotAI* botAI)
{
    if (!botAI)
        return nullptr;

    Player* bot = botAI->GetBot();
    if (!bot)
        return nullptr;

    Item* usable = nullptr;
    VisitBagItems(bot,
                  [&](Item* item)
                  {
                      ItemTemplate const* proto = item->GetTemplate();
                      if (!proto || !proto->StartQuest)
                          return false;
                      if (QuestStartItemRefused(bot, item->GetGUID()))
                          return false;

                      Quest const* quest = sObjectMgr->GetQuestTemplate(proto->StartQuest);
                      QuestStartItemFacts facts;
                      facts.questExists = quest != nullptr;
                      if (quest)
                      {
                          uint32 const questId = quest->GetQuestId();
                          facts.alreadyOnQuest = bot->GetQuestStatus(questId) != QUEST_STATUS_NONE;
                          facts.alreadyRewarded = bot->GetQuestRewardStatus(questId);
                          // CanTakeQuest carries level, race, class, prerequisites and exclusivity;
                          // CanAddQuest carries quest-log room. Neither is restated in the policy.
                          facts.canTakeQuest = bot->CanTakeQuest(quest, false);
                          facts.canAddQuest = bot->CanAddQuest(quest, false);
                      }

                      if (QuestStartItemDecision(facts) != QuestStartItemVerdict::Use)
                          return false;

                      usable = item;
                      return true;
                  });
    return usable;
}

bool RandomBotQuestStartItemAction::Execute(Event /*event*/)
{
    Item* item = playerbots::maintenance::FindUsableQuestStartItem(botAI);
    if (!item)
        return false;

    ItemTemplate const* proto = item->GetTemplate();
    uint32 const questId = proto ? proto->StartQuest : 0;
    if (!questId)
        return false;

    // Accepting is the whole handshake, and using the item is only its first half. ImbueItem sends
    // CMSG_USE_ITEM, which makes the server OFFER the quest and wait for an answer a bot never
    // gives, so the item stayed in the bag and this errand fired again on the next tick: 14,636
    // times across 74 bots in thirty minutes on 2026-09-03, one bot using one item 300 times. The
    // accept opcode carries the questGiver guid, which for an item-started quest is the item.
    // Same path as QuestAction::AcceptQuest, which is protected on a base this action does not
    // share.
    WorldPacket packet(CMSG_QUESTGIVER_ACCEPT_QUEST);
    uint32 const unused = 0;
    packet << item->GetGUID() << questId << unused;
    packet.rpos(0);
    bot->GetSession()->HandleQuestgiverAcceptQuestOpcode(packet);

    bool const accepted = bot->GetQuestStatus(questId) != QUEST_STATUS_NONE;
    if (!accepted)
    {
        // Belt and braces. Even with the right handshake an accept can fail for a reason the
        // policy cannot see, and a failure that leaves the item in the bag is exactly the shape
        // that spun. Remember it and never retry it this run.
        playerbots::maintenance::MarkQuestStartItemRefused(bot, item->GetGUID());
    }

    LOG_DEBUG("playerbots", "[Maintenance] {} quest start item: item {} quest {} accepted {}", bot->GetName(),
              proto->ItemId, questId, accepted ? 1 : 0);
    return accepted;
}

bool RandomBotVendorAction::Execute(Event /*event*/)
{
    if (!NeedsVendor(botAI))
        return false;

    if (!targetEntry && lastAttempt && GetMSTimeDiffToNow(lastAttempt) < 60000)
        return false;

    if (FindNearbyNpc(botAI, targetEntry, UNIT_NPC_FLAG_VENDOR))
    {
        bool const soldGray = botAI->DoSpecificAction("sell", Event("random bot vendor", "gray"), true);
        bool const soldVendor = botAI->DoSpecificAction("sell", Event("random bot vendor", "vendor"), true);
        LOG_DEBUG("playerbots", "[Maintenance] {} vendor: at npc {} sold gray={} vendor={}", bot->GetName(),
                  targetEntry, soldGray, soldVendor);
        targetEntry = 0;
        lastAttempt = getMSTime();
        playerbots::maintenance::ReleaseErrand(bot);
        return soldGray || soldVendor;
    }

    if (!targetEntry)
    {
        NpcDestination destination;
        if (!FindNearestDestination(
                botAI, [](uint32 entry) { return HasNpcFlag(entry, UNIT_NPC_FLAG_VENDOR); }, destination))
        {
            LOG_DEBUG("playerbots", "[Maintenance] {} vendor: no vendor destination from map {} zone {}",
                      bot->GetName(), bot->GetMapId(), bot->GetZoneId());
            return false;
        }

        targetEntry = destination.entry;
        targetPosition = destination.position;
        LOG_DEBUG("playerbots", "[Maintenance] {} vendor: heading to npc {} at {:.0f} yd", bot->GetName(),
                  targetEntry, bot->GetDistance(targetPosition));
    }

    playerbots::maintenance::ClaimErrand(bot, targetPosition);
    bool const moving = MoveFarTo(targetPosition);
    if (!moving)
        LOG_DEBUG("playerbots", "[Maintenance] {} vendor: MoveFarTo returned false, npc {} at {:.0f} yd, isMoving={}",
                  bot->GetName(), targetEntry, bot->GetDistance(targetPosition), bot->isMoving());
    return moving;
}

bool RandomBotMountAction::Execute(Event /*event*/)
{
    if (!NeedsMount(botAI))
    {
        ClearTarget();
        return false;
    }

    MountTier const tier = RequiredMountTier(bot->GetLevel(), GetMountLevels());
    if (tier != targetTier)
    {
        ClearTarget();
        targetTier = tier;
        nextAttempt = 0;
    }

    if (!targetEntry && nextAttempt && GetMSTimeDiffToNow(nextAttempt) < 60000)
        return false;

    uint32 const ridingSpell = NextRidingSpell(bot->GetPureSkillValue(SKILL_RIDING), tier);
    if (ridingSpell)
    {
        if (targetRidingSpell != ridingSpell)
        {
            ClearTarget();
            targetTier = tier;
            targetRidingSpell = ridingSpell;
            nextAttempt = 0;
        }

        uint32 const trainerEntry = RidingTrainerEntry(bot->getRace(), bot->GetTeamId(), ridingSpell);
        if (!trainerEntry)
            return false;

        if (Creature* trainer = FindNearbyNpc(botAI, trainerEntry, UNIT_NPC_FLAG_TRAINER))
        {
            bool const trained = TrainRiding(botAI, trainer, ridingSpell);
            ClearTarget();
            if (!trained)
                nextAttempt = getMSTime();
            return trained;
        }

        if (!targetEntry)
        {
            NpcDestination destination;
            if (!FindNearestDestination(
                    botAI, [trainerEntry](uint32 entry) { return entry == trainerEntry; }, destination))
            {
                nextAttempt = getMSTime();
                return false;
            }

            targetEntry = destination.entry;
            targetPosition = destination.position;
        }

        return MoveFarTo(targetPosition);
    }

    if (uint32 const carriedItem = FindCarriedMountItem(botAI, tier))
    {
        std::string const itemLink = "Hitem:" + std::to_string(carriedItem) + ":";
        if (!botAI->DoSpecificAction("use", Event("random bot mount", itemLink), true))
            return false;

        botAI->DoSpecificAction("check mount state", Event("random bot mount"), true);
        ClearTarget();
        return true;
    }

    if (targetItemId)
    {
        Creature* vendor = FindNearbyNpc(botAI, targetEntry, UNIT_NPC_FLAG_VENDOR);
        if (!vendor)
            return MoveFarTo(targetPosition);

        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(targetItemId);
        AiObjectContext* context = botAI->GetAiObjectContext();
        uint32 const budget = AI_VALUE2(uint32, "free money for", static_cast<uint32>(NeedMoneyFor::anything));
        uint32 const price =
            itemTemplate
                ? static_cast<uint32>(std::floor(itemTemplate->BuyPrice * bot->GetReputationPriceDiscount(vendor)))
                : 0;
        if (!itemTemplate || !price || price > budget || bot->CanUseItem(itemTemplate) != EQUIP_ERR_OK)
        {
            ClearTarget();
            nextAttempt = getMSTime();
            return false;
        }

        uint32 const oldCount = bot->GetItemCount(targetItemId, false);
        std::string const itemLink = "Hitem:" + std::to_string(targetItemId) + ":";
        botAI->DoSpecificAction("buy", Event("random bot mount", itemLink), true);
        bool const bought = bot->GetItemCount(targetItemId, false) > oldCount;
        ClearTarget();
        if (!bought)
            nextAttempt = getMSTime();
        return bought;
    }

    if (!targetEntry)
    {
        NpcDestination destination;
        if (!FindMountVendorDestination(botAI, tier, destination))
        {
            nextAttempt = getMSTime();
            return false;
        }

        targetEntry = destination.entry;
        targetItemId = destination.itemId;
        targetPosition = destination.position;
    }

    return MoveFarTo(targetPosition);
}

void RandomBotMountAction::ClearTarget()
{
    targetEntry = 0;
    targetItemId = 0;
    targetRidingSpell = 0;
    targetPosition = WorldPosition();
}
