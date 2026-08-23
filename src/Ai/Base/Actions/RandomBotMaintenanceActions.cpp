/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RandomBotMaintenanceActions.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <unordered_map>

#include "Bag.h"
#include "BudgetValues.h"
#include "ChatHelper.h"
#include "Event.h"
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
    uint32 const wanted = UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_REPAIR | UNIT_NPC_FLAG_TRAINER;
    for (auto const& [guid, creatureData] : sObjectMgr->GetAllCreatureData())
    {
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
        if (!spellInfo || spellInfo->Effects[0].ApplyAuraName != SPELL_AURA_MOUNTED ||
            entry.second->State == PLAYERSPELL_REMOVED || !entry.second->Active || spellInfo->IsPassive())
        {
            continue;
        }

        bool const flying = spellInfo->Effects[1].ApplyAuraName == SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED ||
                            spellInfo->Effects[2].ApplyAuraName == SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED ||
                            spellInfo->Id == 54729;
        int32 const speed = std::max(spellInfo->Effects[1].BasePoints, spellInfo->Effects[2].BasePoints);
        if (MountMeetsTier(tier, flying, speed))
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
    return repairCost && repairCost <= AI_VALUE2(uint32, "free money for", static_cast<uint32>(NeedMoneyFor::repair));
}

bool playerbots::maintenance::NeedsVendor(PlayerbotAI* botAI)
{
    if (!IsEligible(botAI))
        return false;

    AiObjectContext* context = botAI->GetAiObjectContext();
    if (AI_VALUE(uint8, "bag space") > 80)
        return true;

    return VisitBagItems(
        botAI->GetBot(),
        [botAI](Item* item)
        {
            ItemUsage const usage =
                botAI->GetAiObjectContext()->GetValue<ItemUsage>("item usage", item->GetEntry())->Get();
            return IsVendorTrash(item->GetTemplate()->Quality, usage == ITEM_USAGE_VENDOR);
        });
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

    if (FindNearbyNpc(botAI, targetEntry, UNIT_NPC_FLAG_REPAIR) &&
        botAI->DoSpecificAction("repair", Event("random bot repair"), true))
    {
        LOG_DEBUG("playerbots", "[Maintenance] {} repair: repaired at npc {}", bot->GetName(), targetEntry);
        targetEntry = 0;
        return true;
    }

    if (!targetEntry)
    {
        NpcDestination destination;
        if (!FindNearestDestination(
                botAI, [](uint32 entry) { return HasNpcFlag(entry, UNIT_NPC_FLAG_REPAIR); }, destination))
        {
            LOG_DEBUG("playerbots", "[Maintenance] {} repair: no repair destination from map {} zone {}",
                      bot->GetName(), bot->GetMapId(), bot->GetZoneId());
            return false;
        }

        targetEntry = destination.entry;
        targetPosition = destination.position;
        LOG_DEBUG("playerbots", "[Maintenance] {} repair: heading to npc {} at {:.0f} yd", bot->GetName(),
                  targetEntry, bot->GetDistance(targetPosition));
    }

    bool const moving = MoveFarTo(targetPosition);
    if (!moving)
        LOG_DEBUG("playerbots", "[Maintenance] {} repair: MoveFarTo returned false, npc {} at {:.0f} yd, isMoving={}",
                  bot->GetName(), targetEntry, bot->GetDistance(targetPosition), bot->isMoving());
    return moving;
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
