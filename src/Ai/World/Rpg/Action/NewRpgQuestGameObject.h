/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * World glue for QuestGameObjectPolicy.h: finds the gameobject a bot's current quest objective
 * needs among the cached nearby gameobjects, and operates it. Called from one tagged site in
 * NewRpgDoQuestAction::DoIncompleteQuest (NewRpgAction.cpp, tag quest-gameobject-objective).
 */

#ifndef _PLAYERBOT_NEWRPGQUESTGAMEOBJECT_H
#define _PLAYERBOT_NEWRPGQUESTGAMEOBJECT_H

#include "ObjectGuid.h"

class Player;
class Quest;

struct QuestGameObjectTarget
{
    // Empty when no candidate qualifies.
    ObjectGuid guid;
    // For logging.
    uint32 entry = 0;
    // A chest: queue it for the loot pipeline instead of a direct use.
    bool needsLoot = false;
    // A spell focus: walk inside focusDist and self-use toolItemEntry, never operate the object.
    // See QuestSpellFocusPolicy.h.
    bool castAtFocus = false;
    uint32 toolItemEntry = 0;
    uint32 focusDist = 0;
};

// objectiveIdx follows NewRpgInfo::DoQuest: [0, QUEST_OBJECTIVES_COUNT) are kill/use slots,
// [QUEST_OBJECTIVES_COUNT, QUEST_OBJECTIVES_COUNT + QUEST_ITEM_OBJECTIVES_COUNT) are item
// slots. anchorX/anchorY/anchorRadius bound the search around the assigned POI so the seek
// never fights the quest-poi-approach return radius.
// PLB-LOCAL(quest-abandon-probe): temporary diagnostic mirror of QuestUseSeekDiag. Sampled once
// at abandon so a gameobject-sourced quest that died with zero GOLOOT/GOUSE explains itself:
// nothing nearby, nothing matching, matching-but-unusable (despawned, contested, loot-refused),
// or filtered by the range cap.
struct QuestGoSeekDiag
{
    uint32 nearbyGos = 0;
    uint32 matching = 0;
    uint32 usableMatching = 0;
    uint32 inRange = 0;
};

[[nodiscard]] QuestGameObjectTarget FindQuestObjectiveGameObject(Player* bot, Quest const* quest,
                                                                 int32 objectiveIdx,
                                                                 GuidVector const& nearbyGameObjects,
                                                                 float anchorX, float anchorY,
                                                                 float anchorRadius,
                                                                 QuestGoSeekDiag* diag = nullptr);

[[nodiscard]] bool IsQuestGameObjectWithinInteraction(Player* bot, ObjectGuid guid);

// Sends CMSG_GAMEOBJ_USE through the session handler (same idiom as BattleGroundTactics.cpp
// and PlayerbotAI::CastSpell); the handler re-checks interaction distance server-side.
// Returns true when the packet was dispatched.
bool UseQuestGameObject(Player* bot, ObjectGuid guid);

#endif
