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
#include <span>
#include <vector>

#include "Define.h"

class FlightPathMovementGenerator;
class Player;

/*
 * What a taxi ride costs, as Player::ActivateTaxiPathTo will charge it: every hop's TaxiPath cost
 * summed, then the flight master's reputation discount, rounded up. Measured 2026-09-12 12:20 to
 * 12:50 at 280 bots: 57 random RPG flights and 12 economy flights were refused at the flight
 * master in one window, every one after a walk there, because the purse could not pay (Wilkin,
 * 6c, kept choosing the 730c ride from node 32 to 39). The core's refusal is silent to the bot,
 * so the pick has to know the fare first.
 */
[[nodiscard]] uint32 PlayerbotTaxiFareFromHops(std::span<uint32 const> hopCosts, float reputationDiscount);
// The fare of `nodes` bought from `flightMasterEntry`, or nothing when a hop has no taxi path.
[[nodiscard]] std::optional<uint32> PlayerbotTaxiFare(Player* bot, std::vector<uint32> const& nodes,
                                                      uint32 flightMasterEntry);
[[nodiscard]] bool PlayerbotCanAffordTaxi(Player* bot, std::vector<uint32> const& nodes, uint32 flightMasterEntry);

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
