/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include "QuestObjectiveSpawnPoints.h"
#include "Ai/World/Rpg/QuestItemDropPolicy.h"

#include "DatabaseEnv.h"
#include "Field.h"
#include "ObjectMgr.h"
#include "QueryResult.h"
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

    // A quest item that only exists INSIDE another item (Tallonkai's Jewel 8050 inside the
    // Gnarlpine Necklace 8049 that Ferocitas drops, measured 2026-09-01: the objective had no
    // source, so no anchor, no relevant kills, no respawn sightings, and 8 bots abandoned at his
    // spot) inherits the container's sources. LootTemplate keeps its entries private, so the
    // container relation comes from the world table once, at first use.
    std::unordered_map<uint32, std::vector<uint32>> containerToQuestItems;
    if (QueryResult result = WorldDatabase.Query("SELECT Entry, Item FROM item_loot_template WHERE QuestRequired = 1"))
        do
        {
            Field* fields = result->Fetch();
            containerToQuestItems[fields[0].Get<uint32>()].push_back(fields[1].Get<uint32>());
        } while (result->NextRow());
    for (auto const& [container, questItems] : containerToQuestItems)
    {
        auto const creatures = itemToCreatureEntries.find(container);
        auto const objects = itemToGameObjectEntries.find(container);
        for (uint32 questItem : questItems)
        {
            if (creatures != itemToCreatureEntries.end())
                for (uint32 entry : creatures->second)
                    itemToCreatureEntries[questItem].push_back(entry);
            if (objects != itemToGameObjectEntries.end())
                for (uint32 entry : objects->second)
                    itemToGameObjectEntries[questItem].push_back(entry);
        }
    }
}
}  // namespace

QuestObjectiveSources QuestObjectiveSourceEntriesFor(Quest const* quest, int32 objectiveIdx)
{
    if (!quest)
        return {};

    QuestObjectiveSources sources;
    if (objectiveIdx >= 0 && objectiveIdx < QUEST_OBJECTIVES_COUNT)
    {
        int32 const entry = quest->RequiredNpcOrGo[objectiveIdx];
        if (entry > 0)
            sources.creatureEntries.push_back(static_cast<uint32>(entry));
        else if (entry < 0)
            sources.gameObjectEntries.push_back(static_cast<uint32>(-entry));
    }
    else if ((objectiveIdx >= QUEST_OBJECTIVES_COUNT &&
              objectiveIdx < QUEST_OBJECTIVES_COUNT + QUEST_ITEM_OBJECTIVES_COUNT) ||
             IsItemDropObjectiveIndex(objectiveIdx, QUEST_SOURCE_ITEM_IDS_COUNT))
    {
        // An ItemDrop objective (QuestItemDropPolicy.h) resolves through the same reverse index:
        // the drop's loot sources are its spawns.
        uint32 const itemId = objectiveIdx >= QUEST_ITEMDROP_OBJECTIVE_BASE
                                  ? quest->ItemDrop[objectiveIdx - QUEST_ITEMDROP_OBJECTIVE_BASE]
                                  : quest->RequiredItemId[objectiveIdx - QUEST_OBJECTIVES_COUNT];
        if (!itemId)
            return {};
        std::call_once(reverseIndexBuilt, BuildReverseIndexes);
        if (auto it = itemToCreatureEntries.find(itemId); it != itemToCreatureEntries.end())
            sources.creatureEntries = it->second;
        if (auto it = itemToGameObjectEntries.find(itemId); it != itemToGameObjectEntries.end())
            sources.gameObjectEntries = it->second;
    }
    return sources;
}

std::vector<SpawnAnchorPoint> QuestObjectiveSpawnPointsFor(Quest const* quest, int32 objectiveIdx, uint32 mapId)
{
    if (!quest)
        return {};

    QuestObjectiveSources const sources = QuestObjectiveSourceEntriesFor(quest, objectiveIdx);
    std::vector<uint32> const& creatureEntries = sources.creatureEntries;
    std::vector<uint32> const& gameObjectEntries = sources.gameObjectEntries;

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
                points.push_back({data.posX, data.posY, data.posZ, static_cast<uint32>(spawnId), true});

    if (!gameObjectEntries.empty())
        for (auto const& [spawnId, data] : sObjectMgr->GetAllGOData())
            if (data.mapid == mapId && std::find(gameObjectEntries.begin(), gameObjectEntries.end(), data.id) !=
                                           gameObjectEntries.end())
                points.push_back({data.posX, data.posY, data.posZ, static_cast<uint32>(spawnId), false});

    std::lock_guard<std::mutex> lock(spawnCacheMutex);
    return spawnCache.emplace(key, std::move(points)).first->second;
}

namespace
{
// quest id -> ender entries, inverted once from the core's entry -> quest relation maps.
std::once_flag enderIndexBuilt;
std::unordered_map<uint32, std::vector<uint32>> questToCreatureEnders;
std::unordered_map<uint32, std::vector<uint32>> questToGameObjectEnders;

void BuildEnderIndexes()
{
    for (auto const& [entry, questId] : *sObjectMgr->GetCreatureQuestInvolvedRelationMap())
        questToCreatureEnders[questId].push_back(entry);
    for (auto const& [entry, questId] : *sObjectMgr->GetGOQuestInvolvedRelationMap())
        questToGameObjectEnders[questId].push_back(entry);
}
}  // namespace

std::vector<SpawnAnchorPoint> QuestEnderSpawnPointsFor(Quest const* quest, uint32 mapId)
{
    if (!quest)
        return {};

    std::call_once(enderIndexBuilt, BuildEnderIndexes);
    std::vector<uint32> const* creatureEntries = nullptr;
    std::vector<uint32> const* gameObjectEntries = nullptr;
    if (auto it = questToCreatureEnders.find(quest->GetQuestId()); it != questToCreatureEnders.end())
        creatureEntries = &it->second;
    if (auto it = questToGameObjectEnders.find(quest->GetQuestId()); it != questToGameObjectEnders.end())
        gameObjectEntries = &it->second;
    if (!creatureEntries && !gameObjectEntries)
        return {};

    // Objective index 0xFFFE keeps the ender points apart from every objective's cache entry.
    uint64 const key = (static_cast<uint64>(quest->GetQuestId()) << 32) | (static_cast<uint64>(0xFFFE) << 16) | mapId;
    {
        std::lock_guard<std::mutex> lock(spawnCacheMutex);
        if (auto it = spawnCache.find(key); it != spawnCache.end())
            return it->second;
    }

    auto const wants = [](std::vector<uint32> const* entries, uint32 id)
    { return entries && id && std::find(entries->begin(), entries->end(), id) != entries->end(); };

    std::vector<SpawnAnchorPoint> points;
    if (creatureEntries)
        for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
            if (data.mapid == mapId && (wants(creatureEntries, data.id) || wants(creatureEntries, data.id2) ||
                                        wants(creatureEntries, data.id3)))
                points.push_back({data.posX, data.posY, data.posZ, static_cast<uint32>(spawnId), true});
    if (gameObjectEntries)
        for (auto const& [spawnId, data] : sObjectMgr->GetAllGOData())
            if (data.mapid == mapId && wants(gameObjectEntries, data.id))
                points.push_back({data.posX, data.posY, data.posZ, static_cast<uint32>(spawnId), false});

    std::lock_guard<std::mutex> lock(spawnCacheMutex);
    return spawnCache.emplace(key, std::move(points)).first->second;
}
