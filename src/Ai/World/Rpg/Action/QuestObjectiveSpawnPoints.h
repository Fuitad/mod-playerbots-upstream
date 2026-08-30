/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#ifndef _PLAYERBOT_QUESTOBJECTIVESPAWNPOINTS_H
#define _PLAYERBOT_QUESTOBJECTIVESPAWNPOINTS_H

#include "Ai/World/Rpg/QuestStayAnchorPolicy.h"
#include "Define.h"

#include <vector>

class Quest;

// Every spawn on the given map that can progress the given objective: the required creature or
// gameobject entry for a kill/interact objective, and every creature or gameobject whose
// questitem list carries the required item for an item objective. Results are cached per
// (quest, objective, map); spawn data never changes after startup. Returns by value so callers
// never hold references into the cache.
[[nodiscard]] std::vector<SpawnAnchorPoint> QuestObjectiveSpawnPointsFor(Quest const* quest, int32 objectiveIdx,
                                                                         uint32 mapId);

#endif
