/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * World glue for QuestUseTargetPolicy.h: finds the creature a use-credited quest objective needs
 * among the cached nearby units and operates it (provided quest item, or a known quest spell).
 * Called from one tagged site in NewRpgDoQuestAction::DoIncompleteQuest (NewRpgAction.cpp, tag
 * quest-use-target).
 */

#ifndef _PLAYERBOT_NEWRPGQUESTUSETARGET_H
#define _PLAYERBOT_NEWRPGQUESTUSETARGET_H

#include "ObjectGuid.h"
#include "QuestUseTargetPolicy.h"

class Player;
class PlayerbotAI;
class Quest;

struct QuestUseTarget
{
    // Empty when the objective has no use-tool or no candidate qualifies.
    ObjectGuid guid;
    uint32 entry = 0;
    QuestUseMode mode = QuestUseMode::None;
    // Item mode: the tool's item entry. Spell mode: the known spell id to cast.
    uint32 toolId = 0;
};

[[nodiscard]] QuestUseTarget FindQuestUseTarget(PlayerbotAI* botAI, Quest const* quest, int32 objectiveIdx,
                                                GuidVector const& nearbyUnits, float anchorX, float anchorY,
                                                float anchorRadius);

// Uses the tool on the target. Item mode goes through PlayerbotAI::ImbueItem (CMSG_USE_ITEM on a
// unit target); spell mode through PlayerbotAI::CastSpell. Returns true when the use was
// dispatched.
bool EngageQuestUseTarget(PlayerbotAI* botAI, QuestUseTarget const& target);

#endif
