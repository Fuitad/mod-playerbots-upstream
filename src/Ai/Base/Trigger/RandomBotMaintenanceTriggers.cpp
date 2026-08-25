/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#include "RandomBotMaintenanceTriggers.h"

#include "RandomBotMaintenanceActions.h"

bool RandomBotRepairTrigger::IsActive() { return playerbots::maintenance::NeedsRepair(botAI); }

bool RandomBotVendorTrigger::IsActive() { return playerbots::maintenance::NeedsVendor(botAI); }

bool RandomBotMountTrigger::IsActive() { return playerbots::maintenance::NeedsMount(botAI); }
