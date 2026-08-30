/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#ifndef _PLAYERBOT_QUESTSTAYANCHORPOLICY_H
#define _PLAYERBOT_QUESTSTAYANCHORPOLICY_H

#include <cstddef>
#include <cstdint>
#include <vector>

// One spawn of an objective's source entity, reduced to the coordinates the stay anchor needs.
// The z comes from the spawn row itself: a cave spawn's z is the cave floor, where
// GetHeight(x, y, MAX_HEIGHT) would return the terrain surface above it.
struct SpawnAnchorPoint
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline constexpr size_t QUEST_ANCHOR_NO_SPAWN = ~static_cast<size_t>(0);

// The snap stays local to the POI cluster the picker chose: a spawn farther than this from the
// POI point belongs to another cluster (or another cave) and must not drag the stay across it.
inline constexpr float QUEST_ANCHOR_MAX_SNAP_DISTANCE = 400.0f;

// The spawn nearest (2D) to the chosen POI point, or QUEST_ANCHOR_NO_SPAWN when none is within
// maxSnapDistance. Measured live 2026-08-30: quest 47's POI marks the mine ENTRANCE while every
// gold-dust kobold spawns inside the cave, so bots burned whole stays on the surface with
// targets 0 (four blamed abandons). Anchoring the stay on the real spawn point puts the bot,
// the grind scan and the seek radius where the objective actually lives.
[[nodiscard]] inline size_t NearestSpawnAnchorIndex(std::vector<SpawnAnchorPoint> const& spawns, float poiX,
                                                    float poiY, float maxSnapDistance)
{
    size_t best = QUEST_ANCHOR_NO_SPAWN;
    float bestDistSq = maxSnapDistance > 0.0f ? maxSnapDistance * maxSnapDistance : -1.0f;
    for (size_t i = 0; i < spawns.size(); ++i)
    {
        float const dx = spawns[i].x - poiX;
        float const dy = spawns[i].y - poiY;
        float const distSq = dx * dx + dy * dy;
        if (bestDistSq < 0.0f || distSq < bestDistSq)
        {
            bestDistSq = distSq;
            best = i;
        }
    }
    return best;
}

#endif
