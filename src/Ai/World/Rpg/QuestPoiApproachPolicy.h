/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * It owns one decision: whether a bot working a quest should walk back toward the POI it was sent to.
 */

#ifndef _PLAYERBOT_QUESTPOIAPPROACHPOLICY_H
#define _PLAYERBOT_QUESTPOIAPPROACHPOLICY_H

#include "Define.h"

// Upstream gated the approach on `bot->GetDistance(data.pos) > 10.0f && !data.lastReachPOI`.
// lastReachPOI is stamped once, on first arrival, and is never cleared while the POI stands, so the
// second half of that condition is a permanent latch: after the bot touches the POI once, the branch
// that walks toward it can never run again. A bot dragged away by combat, looting or gathering keeps
// executing MoveRandomNear(8.0f) as though it were still standing on the objective, however far it
// has drifted, and nothing brings it back. Five minutes later the stay timer fires and the quest is
// abandoned with no progress.
//
// Measured live on 2026-08-28 with a temporary probe: all 33 abandons had reached their POI first,
// 28 of 33 (85%) were more than 15 yards away by the time they gave up, 25 of 33 fired at the clean
// 300 second mark, and every one recorded an objective counter of zero.
//
// So arrival still latches the stay timer, which is what upstream wanted, but it no longer latches
// the movement. A bot that has drifted past the radius it would legitimately grind in walks back.
struct QuestPoiApproachFacts
{
    float distanceYards = 0.0f;
    // data.lastReachPOI != 0: the bot has touched this POI at least once.
    bool reached = false;
    // Upstream's arrival radius.
    float arriveRadius = 10.0f;
    // Drift past this and the bot returns. Defaults to the configured grind distance, because that
    // is exactly the radius in which a bot may legitimately be fighting for this objective; pulling
    // it back any sooner would interrupt the work the POI was chosen for.
    float returnRadius = 75.0f;
};

[[nodiscard]] inline bool QuestPoiNeedsApproach(QuestPoiApproachFacts const& facts)
{
    if (!facts.reached)
        return facts.distanceYards > facts.arriveRadius;
    return facts.distanceYards > facts.returnRadius;
}

#endif
