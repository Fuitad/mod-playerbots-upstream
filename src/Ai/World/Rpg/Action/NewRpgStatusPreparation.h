/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * Carries the work NewRpgBaseAction::CheckRpgStatusAvailable already did, so
 * NewRpgBaseAction::RandomChangeStatus can act on it instead of recomputing it.
 *
 * Upstream, CheckRpgStatusAvailable answers "is this status possible?" by doing the full
 * selection for that status and then throwing the answer away. RandomChangeStatus then repeats
 * the identical selection to use it. For RPG_GO_GRIND that scan walks the whole per level
 * location cache (481 to 1858 entries on the live realm) with a Map::GetZoneId terrain lookup on
 * every same map candidate inside 2500 yards, and it runs on every RPG status change, for every
 * bot. RPG_GO_CAMP and RPG_TRAVEL_FLIGHT duplicate the same way.
 *
 * RPG_DO_QUEST is deliberately NOT carried here. Its availability probe stops at the first
 * usable quest while the use path scans the whole quest log, so making the probe produce the
 * final pick would slow down every status change where DO_QUEST is probed but not chosen.
 */

#ifndef PLAYERBOTS_NEWRPGSTATUSPREPARATION_H
#define PLAYERBOTS_NEWRPGSTATUSPREPARATION_H

#include <cstdint>
#include <vector>

#include "TravelMgr.h"

// Result of an availability probe, for the statuses whose probe and use do identical work.
// A default constructed instance means "nothing prepared", which is what every status that is
// not carried here leaves behind.
struct NewRpgStatusPreparation
{
    // RPG_GO_GRIND and RPG_GO_CAMP. Default constructed when the status is unavailable.
    WorldPosition pos;

    // RPG_TRAVEL_FLIGHT. flightPath is empty when the status is unavailable.
    uint32 flightMasterEntry = 0;
    WorldPosition flightMasterPos;
    std::vector<uint32> flightPath;
};

#endif
