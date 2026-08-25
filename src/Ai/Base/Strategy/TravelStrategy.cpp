/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL UPSTREAM-FILE: this fork changes 1 region(s) of this upstream file.
// Each is tagged PLB-LOCAL(<sha>) where a marker could be placed safely; run
// tools/plb_local_markers.py --check for the authoritative list. docs/local-changes.md.

#include "TravelStrategy.h"
#include "Playerbots.h"

TravelStrategy::TravelStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

std::vector<NextAction> TravelStrategy::getDefaultActions()
{
    return {
        NextAction("travel", 1.0f)
    };
}

void TravelStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode(
            "no travel target",
            {
                NextAction("choose travel target", 6.f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode("far from travel target", {NextAction("move to travel target", ACTION_DEFAULT + 1.0f)}));
}
