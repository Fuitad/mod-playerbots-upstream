/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#ifndef PLAYERBOTS_PLAYERBOTTAXIFLIGHT_H
#define PLAYERBOTS_PLAYERBOTTAXIFLIGHT_H

#include <optional>

#include "Define.h"

class FlightPathMovementGenerator;
class Player;

struct PlayerbotTaxiMapHandoffPlan
{
    uint32 nodeIndex = 0;
    uint32 mapId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

enum class PlayerbotTaxiMapHandoffResult
{
    NotNeeded,
    Continued,
    TeleportRejected
};

PlayerbotTaxiMapHandoffResult ContinuePlayerbotTaxiFlightAcrossMap(Player* bot);
std::optional<PlayerbotTaxiMapHandoffPlan> PlanPlayerbotTaxiMapHandoff(FlightPathMovementGenerator& flight,
                                                                       uint32 destinationMapId);

#endif
