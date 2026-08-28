/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * It owns one decision: which of a quest POI's surveyed points the bot should aim at.
 */

#ifndef _PLAYERBOT_QUESTPOIPOINTPOLICY_H
#define _PLAYERBOT_QUESTPOIPOINTPOLICY_H

#include "Define.h"

#include <utility>
#include <vector>

// Upstream averaged a POI's vertices under random weights:
//     std::vector<float> weights = GenerateRandomWeights(qPoi.points.size());
//     for (i) { dx += point.x * weights[i]; dy += point.y * weights[i]; }
// quest_poi_points rows outline the objective AREA, so their weighted mean is a location that need
// not be near anything. Measured 110 to 132 yards from the nearest objective spawn, and it applies to
// the majority of objectives: 4868 of 8615 objective POIs carry five or more vertices, while turn-in
// POIs are 98% single point, where averaging and choosing agree.
//
// Live consequence, from the temporary probe on 2026-08-28: assigned POIs averaged 801 yards away,
// 143 of 215 beyond 600 yards, and bots reached only 16% of them.
//
// Returning an index rather than a position is the point: the caller aims at a real surveyed vertex,
// never a synthesised one.
[[nodiscard]] inline size_t NearestPoiPointIndex(std::vector<std::pair<float, float>> const& points, float fromX,
                                                 float fromY)
{
    size_t best = 0;
    float bestDistanceSquared = -1.0f;
    for (size_t i = 0; i < points.size(); i++)
    {
        float const dx = points[i].first - fromX;
        float const dy = points[i].second - fromY;
        float const distanceSquared = dx * dx + dy * dy;
        if (bestDistanceSquared < 0.0f || distanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = distanceSquared;
            best = i;
        }
    }
    return best;
}

#endif
