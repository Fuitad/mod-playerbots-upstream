/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#include "RandomBotMaintenanceStrategy.h"

void RandomBotMaintenanceStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("random bot needs repair", {NextAction("random bot repair", 105.0f)}));
    triggers.push_back(new TriggerNode("random bot needs vendor", {NextAction("random bot vendor", 104.0f)}));
    // Ranked BELOW repair, vendor and mount. It was 106, above all three, on the reasoning that it
    // is the cheapest errand: no travel, one packet. That reasoning was wrong twice over. Taking a
    // quest is never more urgent than keeping a bot's bags and gear working, and when the errand
    // looped on 2026-09-03 the high rank turned a wasteful loop into a live regression: it won the
    // action slot about 488 times a minute for thirty minutes and repair fired 165 times against
    // the vendor's 2846 while bags climbed.
    triggers.push_back(
        new TriggerNode("random bot has quest start item", {NextAction("random bot quest start item", 102.0f)}));
    triggers.push_back(new TriggerNode("random bot needs mount", {NextAction("random bot mount", 103.0f)}));
}
