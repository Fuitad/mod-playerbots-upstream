/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
// PLB-LOCAL(working-tree): Uncommitted local change.
// Upstream: No corresponding block at the merge base. (base 8d9f6aa6bc6d).

// PLB-LOCAL UPSTREAM-FILE: this fork changes 1 region(s) of this upstream file.

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
        // PLB-LOCAL(d2283a1c2544): style(travel): format forced route trigger.
        // Upstream: new TriggerNode( "far from travel target", { NextAction("move to travel target", 1) } ) ); (base...
        new TriggerNode("far from travel target", {NextAction("move to travel target", ACTION_DEFAULT + 1.0f)}));
}
