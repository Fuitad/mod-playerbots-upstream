/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * When the corpse walk counts as arrived. Upstream stopped the walk five yards inside the reclaim
 * radius, which left a ring (34 to 39 yards from the corpse) where the walk kept trying to close
 * the last yards, the move failed, and the revive that the reclaim radius already allowed never
 * ran: Sinette, 2026-09-02 00:44, stood 34 yards from her body with nothing hostile near for three
 * minutes, logging a spirit fallback every 47 seconds, until the manager teleported her home.
 */

#ifndef _PLAYERBOT_CORPSEWALKPOLICY_H
#define _PLAYERBOT_CORPSEWALKPOLICY_H

// One yard inside the radius the server accepts, so a rounding difference between the walk's
// distance and the server's cannot leave the ghost standing just outside it.
inline constexpr float CORPSE_WALK_ARRIVAL_MARGIN_YARDS = 1.0f;

inline bool CorpseWalkArrived(float corpseDist, float reclaimRadius)
{
    return corpseDist < reclaimRadius - CORPSE_WALK_ARRIVAL_MARGIN_YARDS;
}

#endif
