/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 * See NewRpgQuestVendor.h.
 */

#include "NewRpgQuestVendor.h"

#include "Creature.h"
#include "CreatureData.h"
#include "ItemTemplate.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"

#include <algorithm>

QuestVendorTarget FindQuestObjectiveVendor(Player* bot, Quest const* quest, int32 objectiveIdx,
                                           GuidVector const& nearbyNpcs)
{
    if (!bot || !quest)
        return {};
    if (objectiveIdx < QUEST_OBJECTIVES_COUNT || objectiveIdx >= QUEST_OBJECTIVES_COUNT + QUEST_ITEM_OBJECTIVES_COUNT)
        return {};

    uint32 const itemIdx = static_cast<uint32>(objectiveIdx - QUEST_OBJECTIVES_COUNT);
    uint32 const itemId = quest->RequiredItemId[itemIdx];
    if (!itemId)
        return {};
    uint32 const required = quest->RequiredItemCount[itemIdx];
    uint32 const have = bot->GetItemCount(itemId, true);
    if (have >= required)
        return {};
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
        return {};

    QuestVendorTarget best;
    float bestDist = 0.0f;
    for (ObjectGuid const& guid : nearbyNpcs)
    {
        Creature* creature = ObjectAccessor::GetCreature(*bot, guid);
        if (!creature || !creature->IsAlive() || !creature->IsVendor() || !creature->IsFriendlyTo(bot))
            continue;
        VendorItemData const* items = creature->GetVendorItems();
        if (!items)
            continue;
        for (uint32 slot = 0; slot < items->GetItemCount(); ++slot)
        {
            VendorItem const* offer = items->GetItem(slot);
            // Extended cost means honor, tokens or reputation: not something a random bot pays.
            if (!offer || offer->item != itemId || offer->ExtendedCost)
                continue;
            float const dist = bot->GetDistance(creature);
            if (!best.guid || dist < bestDist)
            {
                best.guid = guid;
                best.entry = creature->GetEntry();
                best.vendorSlot = slot;
                best.itemId = itemId;
                best.count = required - have;
                bestDist = dist;
            }
            break;
        }
    }
    if (!best.guid)
        return {};

    if (proto->BuyPrice)
    {
        uint32 const affordable = bot->GetMoney() / proto->BuyPrice;
        if (!affordable)
            return {};
        best.count = std::min(best.count, affordable);
    }
    return best;
}

bool BuyQuestObjectiveItem(Player* bot, QuestVendorTarget const& target)
{
    if (!bot || !target.guid || !target.count)
        return false;
    uint32 const before = bot->GetItemCount(target.itemId, true);
    // One unit per call keeps the count independent of the item's BuyCount stacking; the core
    // refuses the call itself when the vendor is out of range or the money or bags run out.
    uint32 const units = std::min<uint32>(target.count, 20);
    for (uint32 i = 0; i < units; ++i)
        if (!bot->BuyItemFromVendorSlot(target.guid, target.vendorSlot, target.itemId, 1, NULL_BAG, NULL_SLOT))
            break;
    return bot->GetItemCount(target.itemId, true) > before;
}
