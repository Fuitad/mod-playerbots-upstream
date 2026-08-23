/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RandomBotMaintenanceStrategy.h"

void RandomBotMaintenanceStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("random bot needs repair", {NextAction("random bot repair", 105.0f)}));
    triggers.push_back(new TriggerNode("random bot needs vendor", {NextAction("random bot vendor", 104.0f)}));
    triggers.push_back(new TriggerNode("random bot needs mount", {NextAction("random bot mount", 103.0f)}));
}
