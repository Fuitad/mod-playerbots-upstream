/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE (flight-destination-map). Not present upstream, so it can never conflict on a merge.
 *
 * A random flight destination must sit on the bot's own map. The picker chooses a zone by level
 * bracket and then a taxi node cached for that zone; Ghostlands (bracket 10 to 20) is on map 530
 * while its taxi graph touches Eastern Kingdoms nodes, so a level 14 bot at Stormwind was handed
 * a path ending at Zul'Aman (node 205) and the flight put it down at Thorium Point in Searing
 * Gorge, a level 45 to 51 zone, where it died to level 47 rares for an hour (Dazedcitizen and
 * Thinda, 2026-09-05; 16 to 21 deaths per half hour in zone 51 all afternoon).
 */

#ifndef _PLAYERBOT_FLIGHTDESTINATIONPOLICY_H
#define _PLAYERBOT_FLIGHTDESTINATIONPOLICY_H

#include "Define.h"

[[nodiscard]] inline bool FlightDestinationOnBotMap(uint32 botMapId, uint32 nodeMapId)
{
    return botMapId == nodeMapId;
}

#endif
