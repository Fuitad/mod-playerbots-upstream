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

// The entities that can progress the given objective: the required creature or gameobject entry
// for a kill/interact objective, and every creature or gameobject whose questitem list carries
// the required item for an item objective.
struct QuestObjectiveSources
{
    std::vector<uint32> creatureEntries;
    std::vector<uint32> gameObjectEntries;
};

[[nodiscard]] QuestObjectiveSources QuestObjectiveSourceEntriesFor(Quest const* quest, int32 objectiveIdx);

// Every spawn on the given map of the objective's source entities. Results are cached per
// (quest, objective, map); spawn data never changes after startup. Returns by value so callers
// never hold references into the cache.
[[nodiscard]] std::vector<SpawnAnchorPoint> QuestObjectiveSpawnPointsFor(Quest const* quest, int32 objectiveIdx,
                                                                         uint32 mapId);

// Every spawn on the given map of the creatures and gameobjects that END the quest (its
// questenders). The reward POI only carries x and y; the ender's own spawn row carries the z the
// bot must stand at. Measured live 2026-09-01: Muren Stormpike at z 510 in Ironforge with the bot
// parked at z 445 under him, and Furl Scornbrow on a ledge at z 76 with the bot 50y below, both
// reward stays timing out with the giver "visible" but unreachable. Cached like the objective
// spawn points.
[[nodiscard]] std::vector<SpawnAnchorPoint> QuestEnderSpawnPointsFor(Quest const* quest, uint32 mapId);

#endif
