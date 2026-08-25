/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL UPSTREAM-FILE: this fork changes 3 region(s) of this upstream file.
// Each is tagged PLB-LOCAL(<sha>) where a marker could be placed safely; run
// tools/plb_local_markers.py --check for the authoritative list. docs/local-changes.md.

#include "RepairAllAction.h"
#include "ChatHelper.h"
#include "Event.h"
#include "Playerbots.h"

bool RepairAllAction::Execute(Event /*event*/)
// PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
{
    if (!ExecutePaidRepair())
    {
        botAI->TellError("Cannot find any npc to repair at");
        return false;
    }

    (void)botAI->IsPostReviveRepairPending();
    return true;
}

bool RepairAllAction::ExecutePaidRepair()
{
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (ObjectGuid const guid : npcs)
    {
        Creature* unit = bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_REPAIR);
        if (!unit)
            continue;

        if (bot->HasUnitState(UNIT_STATE_DIED))
            bot->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

        bot->SetFacingToObject(unit);
        float discountMod = bot->GetReputationPriceDiscount(unit);

        uint32 botMoney = bot->GetMoney();
        // PLB-LOCAL(aff67526d8ca): feat(economy): random bots repair, vendor trash and buy mounts with real gold
        bool const economyBot = sPlayerbotAIConfig.economyManagedSupplies && sRandomPlayerbotMgr.IsRandomBot(bot);
        bool const useGoldCheat = botAI->HasCheat(BotCheatMask::gold) && !economyBot;
        if (useGoldCheat)
        {
            bot->SetMoney(10000000);
        }

        // Repair weapons first.
        uint32 totalCost = bot->DurabilityRepair(EQUIPMENT_SLOT_MAINHAND, true, discountMod, false);
        totalCost += bot->DurabilityRepair(EQUIPMENT_SLOT_RANGED, true, discountMod, false);
        totalCost += bot->DurabilityRepair(EQUIPMENT_SLOT_OFFHAND, true, discountMod, false);

        totalCost += bot->DurabilityRepairAll(true, discountMod, false);

        // PLB-LOCAL(aff67526d8ca): feat(economy): random bots repair, vendor trash and buy mounts with real gold
        if (useGoldCheat)
        {
            bot->SetMoney(botMoney);
        }

        if (totalCost > 0)
        {
            std::ostringstream out;
            out << "Repair: " << chat->formatMoney(totalCost) << " (" << unit->GetName() << ")";
            botAI->TellMasterNoFacing(out.str());

            bot->PlayDistanceSound(1116);
        }

        return true;
    }

    return false;
}
