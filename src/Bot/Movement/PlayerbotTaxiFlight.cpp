/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#include "PlayerbotTaxiFlight.h"

#include <cmath>

#include "DBCStores.h"
#include "Log.h"
#include "MotionMaster.h"
#include "MoveSpline.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "WaypointMovementGenerator.h"

uint32 PlayerbotTaxiFareFromHops(std::span<uint32 const> hopCosts, float reputationDiscount)
{
    // Player::ActivateTaxiPathTo: totalcost = uint32(ceil(totalcost * discount)).
    uint64 total = 0;
    for (uint32 const cost : hopCosts)
        total += cost;
    double const discounted = std::ceil(static_cast<double>(total) * static_cast<double>(reputationDiscount));
    return discounted <= 0.0 ? 0u : static_cast<uint32>(discounted);
}

std::optional<uint32> PlayerbotTaxiFare(Player* bot, std::vector<uint32> const& nodes, uint32 flightMasterEntry)
{
    if (!bot || nodes.size() < 2)
        return std::nullopt;
    std::vector<uint32> hops;
    hops.reserve(nodes.size() - 1);
    for (size_t index = 1; index < nodes.size(); ++index)
    {
        uint32 path = 0;
        uint32 cost = 0;
        sObjectMgr->GetTaxiPath(nodes[index - 1], nodes[index], path, cost);
        if (!path)
            return std::nullopt;
        hops.push_back(cost);
    }
    float discount = 1.0f;
    if (CreatureTemplate const* flightMaster = sObjectMgr->GetCreatureTemplate(flightMasterEntry))
    {
        if (FactionTemplateEntry const* faction = sFactionTemplateStore.LookupEntry(flightMaster->faction))
            discount = bot->GetReputationPriceDiscount(faction);
    }
    return PlayerbotTaxiFareFromHops(hops, discount);
}

bool PlayerbotCanAffordTaxi(Player* bot, std::vector<uint32> const& nodes, uint32 flightMasterEntry)
{
    std::optional<uint32> const fare = PlayerbotTaxiFare(bot, nodes, flightMasterEntry);
    return fare && *fare <= bot->GetMoney();
}

PlayerbotTaxiMapHandoffResult ContinuePlayerbotTaxiFlightAcrossMap(Player* bot)
{
    if (!bot || !bot->IsInWorld() || !bot->HasUnitState(UNIT_STATE_IN_FLIGHT) || !bot->movespline ||
        !bot->movespline->Finalized())
        return PlayerbotTaxiMapHandoffResult::NotNeeded;

    uint32 const destinationId = bot->m_taxi.GetTaxiDestination();
    TaxiNodesEntry const* const destination = sTaxiNodesStore.LookupEntry(destinationId);
    if (!destination || destination->map_id == bot->GetMapId() ||
        bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FLIGHT_MOTION_TYPE)
        return PlayerbotTaxiMapHandoffResult::NotNeeded;

    auto* const flight = dynamic_cast<FlightPathMovementGenerator*>(bot->GetMotionMaster()->top());
    if (!flight)
        return PlayerbotTaxiMapHandoffResult::NotNeeded;

    std::optional<PlayerbotTaxiMapHandoffPlan> const plan = PlanPlayerbotTaxiMapHandoff(*flight, destination->map_id);
    if (!plan)
    {
        LOG_ERROR("playerbots", "Bot {} cannot continue taxi flight across map: no path node on map {}", bot->GetName(),
                  destination->map_id);
        return PlayerbotTaxiMapHandoffResult::NotNeeded;
    }

    flight->SetCurrentNodeAfterTeleport();
    if (flight->GetCurrentNode() != plan->nodeIndex)
        return PlayerbotTaxiMapHandoffResult::NotNeeded;

    flight->SkipCurrentNode();
    if (!bot->TeleportTo(plan->mapId, plan->x, plan->y, plan->z, bot->GetOrientation(), TELE_TO_NOT_LEAVE_TAXI))
    {
        LOG_ERROR("playerbots", "Bot {} taxi flight map handoff to map {} was rejected", bot->GetName(), plan->mapId);
        return PlayerbotTaxiMapHandoffResult::TeleportRejected;
    }

    LOG_DEBUG("playerbots", "Bot {} continued taxi flight across map {} to map {}", bot->GetName(), bot->GetMapId(),
              plan->mapId);
    return PlayerbotTaxiMapHandoffResult::Continued;
}

std::optional<PlayerbotTaxiMapHandoffPlan> PlanPlayerbotTaxiMapHandoff(FlightPathMovementGenerator& flight,
                                                                       uint32 destinationMapId)
{
    TaxiPathNodeList const& path = flight.GetPath();
    uint32 const current = flight.GetCurrentNode();
    if (current >= path.size())
        return std::nullopt;

    uint32 const currentMapId = path[current]->mapid;
    for (uint32 index = current + 1; index < path.size(); ++index)
    {
        TaxiPathNodeEntry const* const node = path[index];
        if (node->mapid == currentMapId)
            continue;
        if (node->mapid != destinationMapId)
            return std::nullopt;

        return PlayerbotTaxiMapHandoffPlan{index, node->mapid, node->x, node->y, node->z};
    }

    return std::nullopt;
}
