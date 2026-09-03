/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#include "RandomBotMaintenanceTriggers.h"

#include "RandomBotMaintenanceActions.h"

// Routine maintenance yields to quest work: these triggers run at relevance 103 to 105 and own
// the bot outright, so firing one mid-quest drags the bot off its objective (see
// DeferRoutineMaintenanceDuringQuest). Broken gear and bags too full to loot stay urgent.
bool RandomBotRepairTrigger::IsActive()
{
    return playerbots::maintenance::NeedsRepair(botAI) &&
           !playerbots::maintenance::DeferRoutineMaintenanceDuringQuest(
               playerbots::maintenance::DoingQuestNow(botAI), playerbots::maintenance::HasBrokenEquipment(botAI));
}

bool RandomBotVendorTrigger::IsActive()
{
    return playerbots::maintenance::NeedsVendor(botAI) &&
           !playerbots::maintenance::DeferRoutineMaintenanceDuringQuest(
               playerbots::maintenance::DoingQuestNow(botAI), playerbots::maintenance::CriticallyFullBags(botAI));
}

bool RandomBotQuestStartItemTrigger::IsActive()
{
    return playerbots::maintenance::IsEligible(botAI) &&
           playerbots::maintenance::FindUsableQuestStartItem(botAI) != nullptr;
}

bool RandomBotMountTrigger::IsActive()
{
    return playerbots::maintenance::NeedsMount(botAI) &&
           !playerbots::maintenance::DeferRoutineMaintenanceDuringQuest(
               playerbots::maintenance::DoingQuestNow(botAI), false);
}
