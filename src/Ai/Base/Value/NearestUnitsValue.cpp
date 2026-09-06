/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
// PLB-LOCAL UPSTREAM-FILE: this fork changes 2 region(s) of this upstream file.

#include "NearestUnitsValue.h"
// PLB-LOCAL(nearby-unit-sight): the sight rule used below.
#include "Ai/Base/Value/NearbyUnitSightPolicy.h"
#include "Playerbots.h"

GuidVector NearestUnitsValue::Calculate()
{
    std::list<Unit*> targets;
    FindUnits(targets);

    GuidVector results;
    for (Unit* unit : targets)
    {
        // PLB-LOCAL(nearby-unit-sight): a unit within talking distance is seen without a line of
        // sight trace; city vendors on platforms and behind stalls failed it at two yards.
        // Upstream: `if (AcceptUnit(unit) && (ignoreLos || bot->IsWithinLOSInMap(unit)))`.
        if (AcceptUnit(unit) &&
            NearbyUnitSeen(bot->GetDistance(unit), ignoreLos || bot->IsWithinLOSInMap(unit), ignoreLos))
            results.push_back(unit->GetGUID());
    }

    return results;
}
