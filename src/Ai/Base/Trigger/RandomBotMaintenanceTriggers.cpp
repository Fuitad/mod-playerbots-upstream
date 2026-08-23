/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RandomBotMaintenanceTriggers.h"

#include "RandomBotMaintenanceActions.h"

bool RandomBotRepairTrigger::IsActive() { return playerbots::maintenance::NeedsRepair(botAI); }

bool RandomBotVendorTrigger::IsActive() { return playerbots::maintenance::NeedsVendor(botAI); }

bool RandomBotMountTrigger::IsActive() { return playerbots::maintenance::NeedsMount(botAI); }
