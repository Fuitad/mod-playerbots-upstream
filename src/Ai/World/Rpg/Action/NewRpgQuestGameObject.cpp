/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 * See NewRpgQuestGameObject.h.
 */

#include "NewRpgQuestGameObject.h"

#include "Ai/World/Rpg/QuestGameObjectPolicy.h"
#include "Ai/World/Rpg/QuestSpellFocusPolicy.h"
#include "Ai/World/Rpg/Action/QuestObjectiveSpawnPoints.h"
#include "GameObject.h"
#include "LootMgr.h"
#include "LootObjectStack.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "QuestDef.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "WorldSession.h"

namespace
{
// A chest counts for an item objective when its curated quest item list carries the item, or,
// because gameobject_questitem covers only about half of the chest-sourced quest items in the
// world database, when its loot template holds quest loot for this player at all.
bool ChestCanDropQuestItem(GameObject const* go, Player* bot, uint32 neededItemId)
{
    if (GameObjectQuestItemList const* items = sObjectMgr->GetGameObjectQuestItemList(go->GetEntry()))
        for (uint32 itemId : *items)
            if (itemId == neededItemId)
                return true;

    return LootTemplates_Gameobject.HaveQuestLootForPlayer(go->GetGOInfo()->GetLootId(), bot);
}
}  // namespace

QuestGameObjectTarget FindQuestObjectiveGameObject(Player* bot, Quest const* quest, int32 objectiveIdx,
                                                   GuidVector const& nearbyGameObjects, float anchorX,
                                                   float anchorY, float anchorRadius, QuestGoSeekDiag* diag)
{
    if (!bot || !quest)
        return {};

    uint32 requiredGoEntry = 0;
    uint32 neededItemId = 0;
    // An item nothing loots, made by the quest's tool at a spell focus (QuestSpellFocusPolicy.h).
    // The tool has to be in the bags for the focus to be worth walking to.
    uint32 focusId = 0;
    uint32 toolItemEntry = 0;
    if (objectiveIdx >= 0 && objectiveIdx < QUEST_OBJECTIVES_COUNT)
    {
        requiredGoEntry = QuestObjectiveGoEntry(quest->RequiredNpcOrGo[objectiveIdx]);
        if (!requiredGoEntry)
            return {};
    }
    else if (objectiveIdx >= QUEST_OBJECTIVES_COUNT &&
             objectiveIdx < QUEST_OBJECTIVES_COUNT + QUEST_ITEM_OBJECTIVES_COUNT)
    {
        neededItemId = quest->RequiredItemId[objectiveIdx - QUEST_OBJECTIVES_COUNT];
        if (!neededItemId)
            return {};
        QuestObjectiveSources const sources = QuestObjectiveSourceEntriesFor(quest, objectiveIdx);
        if (sources.spellFocusId && bot->GetItemByEntry(sources.spellFocusToolItem))
        {
            focusId = sources.spellFocusId;
            toolItemEntry = sources.spellFocusToolItem;
        }
    }
    else
        return {};

    std::vector<QuestGoCandidateFacts> facts;
    std::vector<QuestGameObjectTarget> targets;
    facts.reserve(nearbyGameObjects.size());
    targets.reserve(nearbyGameObjects.size());

    for (ObjectGuid const& guid : nearbyGameObjects)
    {
        GameObject* go = ObjectAccessor::GetGameObject(*bot, guid);
        if (!go || !go->GetGOInfo())
            continue;

        bool const isGoober = go->GetGoType() == GAMEOBJECT_TYPE_GOOBER;
        bool const isChest = go->GetGoType() == GAMEOBJECT_TYPE_CHEST;
        bool const isSpellFocus = focusId && go->GetGoType() == GAMEOBJECT_TYPE_SPELL_FOCUS;

        QuestGoCandidateFacts candidate;
        candidate.interaction = QuestGoInteractionForType(isGoober, isChest, isSpellFocus);
        if (candidate.interaction == QuestGoInteraction::Skip)
            continue;

        // A focus is never activated to a quest and carries no state of its own: spawned is the
        // whole of usable, the same test Spell::CheckSpellFocus applies (GameObjectFocusCheck).
        candidate.usable = candidate.interaction == QuestGoInteraction::CastAtFocus
                               ? go->isSpawned()
                               : go->isSpawned() && go->GetGoState() == GO_STATE_READY &&
                                     !go->HasFlag(GAMEOBJECT_FLAGS, GO_FLAG_NOT_SELECTABLE) &&
                                     !go->HasFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE) && go->ActivateToQuest(bot);

        // A chest is only a candidate when the loot pipeline itself would open it. Without this
        // agreement the seek and the pipeline can disagree forever: measured live 2026-08-29, a
        // bot on "Webwood Venom" (spider drops) was parked at a Moonpetal Lily for a full stay,
        // re-queueing it 120 times while the pipeline kept refusing. LootObject::Refresh (run by
        // the constructor) applies the pipeline's needed-quest-item rule; IsLootPossible adds the
        // lock, skill and vertical-offset gates. The offset gate compares the bot's own z, which
        // is meaningless for a candidate still tens of yards away, so the full check applies only
        // within interaction range; a far candidate that fails it on arrival simply drops out of
        // the candidate set next tick instead of looping.
        if (candidate.usable && candidate.interaction == QuestGoInteraction::Loot)
        {
            LootObject lootProbe(bot, guid);
            candidate.usable = !lootProbe.IsEmpty() &&
                               (!go->IsWithinDistInMap(bot, go->GetInteractionDistance()) ||
                                lootProbe.IsLootPossible(bot));
        }

        if (requiredGoEntry)
            candidate.matchesObjective = go->GetEntry() == requiredGoEntry;
        else if (isSpellFocus)
            candidate.matchesObjective = go->GetGOInfo()->spellFocus.focusId == focusId;
        else if (isChest)
            candidate.matchesObjective = ChestCanDropQuestItem(go, bot, neededItemId);

        candidate.distanceSq = bot->GetExactDistSq(go);
        candidate.anchorDistanceSq = go->GetExactDist2dSq(anchorX, anchorY);

        // PLB-LOCAL(quest-abandon-probe): temporary diagnostic, see QuestGoSeekDiag.
        if (diag)
        {
            ++diag->nearbyGos;
            if (candidate.matchesObjective)
            {
                ++diag->matching;
                if (candidate.usable)
                {
                    ++diag->usableMatching;
                    if (QuestGoCandidateInRange(candidate,
                                                anchorRadius > 0.0f ? anchorRadius * anchorRadius : 0.0f))
                        ++diag->inRange;
                }
            }
        }

        QuestGameObjectTarget target;
        target.guid = guid;
        target.entry = go->GetEntry();
        target.needsLoot = candidate.interaction == QuestGoInteraction::Loot;
        if (candidate.interaction == QuestGoInteraction::CastAtFocus)
        {
            target.castAtFocus = true;
            target.toolItemEntry = toolItemEntry;
            target.focusDist = go->GetGOInfo()->spellFocus.dist;
        }

        facts.push_back(candidate);
        targets.push_back(target);
    }

    size_t const best = BestQuestGoCandidateIndex(facts, anchorRadius > 0.0f ? anchorRadius * anchorRadius : 0.0f);
    if (best == QUEST_GO_NO_CANDIDATE)
        return {};

    return targets[best];
}

bool IsQuestGameObjectWithinInteraction(Player* bot, ObjectGuid guid)
{
    if (!bot)
        return false;

    GameObject* go = ObjectAccessor::GetGameObject(*bot, guid);
    return go && go->IsWithinDistInMap(bot, go->GetInteractionDistance());
}

bool UseQuestGameObject(Player* bot, ObjectGuid guid)
{
    if (!bot)
        return false;

    GameObject* go = ObjectAccessor::GetGameObject(*bot, guid);
    if (!go || !go->isSpawned())
        return false;

    if (!go->IsWithinDistInMap(bot, go->GetInteractionDistance()))
        return false;

    if (bot->isMoving())
        bot->StopMoving();

    WorldPacket data(CMSG_GAMEOBJ_USE, 8);
    data << guid;
    bot->GetSession()->HandleGameObjectUseOpcode(data);
    return true;
}
