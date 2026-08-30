/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include "QuestObjectiveSpawnPoints.h"

#include "ObjectMgr.h"
#include "QuestDef.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>

namespace
{
std::mutex spawnCacheMutex;
std::unordered_map<uint64, std::vector<SpawnAnchorPoint>> spawnCache;

// item id -> source entries, built once from the questitem stores. These are the same tables
// the loot pipeline's IsNeededForQuest logic relies on, so a source listed here is one the
// pipeline can actually credit.
std::once_flag reverseIndexBuilt;
std::unordered_map<uint32, std::vector<uint32>> itemToCreatureEntries;
std::unordered_map<uint32, std::vector<uint32>> itemToGameObjectEntries;

void BuildReverseIndexes()
{
    for (auto const& [entry, tmpl] : *sObjectMgr->GetCreatureTemplates())
        if (CreatureQuestItemList const* items = sObjectMgr->GetCreatureQuestItemList(entry))
            for (uint32 itemId : *items)
                if (itemId)
                    itemToCreatureEntries[itemId].push_back(entry);

    for (auto const& [entry, tmpl] : *sObjectMgr->GetGameObjectTemplates())
        if (GameObjectQuestItemList const* items = sObjectMgr->GetGameObjectQuestItemList(entry))
            for (uint32 itemId : *items)
                if (itemId)
                    itemToGameObjectEntries[itemId].push_back(entry);
}
}  // namespace

std::vector<SpawnAnchorPoint> QuestObjectiveSpawnPointsFor(Quest const* quest, int32 objectiveIdx, uint32 mapId)
{
    if (!quest)
        return {};

    std::vector<uint32> creatureEntries;
    std::vector<uint32> gameObjectEntries;
    if (objectiveIdx >= 0 && objectiveIdx < QUEST_OBJECTIVES_COUNT)
    {
        int32 const entry = quest->RequiredNpcOrGo[objectiveIdx];
        if (entry > 0)
            creatureEntries.push_back(static_cast<uint32>(entry));
        else if (entry < 0)
            gameObjectEntries.push_back(static_cast<uint32>(-entry));
        else
            return {};
    }
    else if (objectiveIdx >= QUEST_OBJECTIVES_COUNT &&
             objectiveIdx < QUEST_OBJECTIVES_COUNT + QUEST_ITEM_OBJECTIVES_COUNT)
    {
        uint32 const itemId = quest->RequiredItemId[objectiveIdx - QUEST_OBJECTIVES_COUNT];
        if (!itemId)
            return {};
        std::call_once(reverseIndexBuilt, BuildReverseIndexes);
        if (auto it = itemToCreatureEntries.find(itemId); it != itemToCreatureEntries.end())
            creatureEntries = it->second;
        if (auto it = itemToGameObjectEntries.find(itemId); it != itemToGameObjectEntries.end())
            gameObjectEntries = it->second;
    }
    else
        return {};

    if (creatureEntries.empty() && gameObjectEntries.empty())
        return {};

    uint64 const key = (static_cast<uint64>(quest->GetQuestId()) << 32) |
                       (static_cast<uint64>(static_cast<uint16>(objectiveIdx)) << 16) | mapId;
    {
        std::lock_guard<std::mutex> lock(spawnCacheMutex);
        if (auto it = spawnCache.find(key); it != spawnCache.end())
            return it->second;
    }

    auto const wantsCreature = [&](uint32 id)
    { return id && std::find(creatureEntries.begin(), creatureEntries.end(), id) != creatureEntries.end(); };

    std::vector<SpawnAnchorPoint> points;
    if (!creatureEntries.empty())
        for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
            if (data.mapid == mapId && (wantsCreature(data.id) || wantsCreature(data.id2) || wantsCreature(data.id3)))
                points.push_back({data.posX, data.posY, data.posZ});

    if (!gameObjectEntries.empty())
        for (auto const& [spawnId, data] : sObjectMgr->GetAllGOData())
            if (data.mapid == mapId && std::find(gameObjectEntries.begin(), gameObjectEntries.end(), data.id) !=
                                           gameObjectEntries.end())
                points.push_back({data.posX, data.posY, data.posZ});

    std::lock_guard<std::mutex> lock(spawnCacheMutex);
    return spawnCache.emplace(key, std::move(points)).first->second;
}
