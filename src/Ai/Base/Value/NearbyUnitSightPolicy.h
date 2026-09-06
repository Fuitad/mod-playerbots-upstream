/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE (nearby-unit-sight). Not present upstream, so it can never conflict on a merge.
 *
 * Whether a unit inside the search range counts as seen. The nearest-units values drop every unit
 * without line of sight, which is right for a mob across a wall and wrong for the vendor a bot is
 * standing beside: Silvermoon's raised platforms, Ironforge's stalls, Orgrimmar's huts and Thunder
 * Bluff's tents all break the trace at a yard or two, so the economy walked bots to their vendor
 * (arrived at 2 yards, 2026-09-06) and found no NPC to buy from, then walked them back to the
 * mailbox and out again for an hour. A player interacts through a railing; a unit within talking
 * distance is seen whether or not the trace succeeds.
 */

#ifndef _PLAYERBOT_NEARBYUNITSIGHTPOLICY_H
#define _PLAYERBOT_NEARBYUNITSIGHTPOLICY_H

// The core's interaction distance is 5.5 yards; a small margin covers a stand point beside a stall.
inline constexpr float NEARBY_UNIT_SEEN_WITHOUT_LOS_YARDS = 8.0f;

[[nodiscard]] inline bool NearbyUnitSeen(float distanceYards, bool inLineOfSight, bool losIgnored)
{
    return losIgnored || inLineOfSight || distanceYards <= NEARBY_UNIT_SEEN_WITHOUT_LOS_YARDS;
}

#endif
