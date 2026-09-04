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
#include "QuestPickPocketPolicy.h"
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
    // Item mode: the tool's item entry. Spell and Pick Pocket modes: the known spell id to cast.
    uint32 toolId = 0;
    // The spell the tool casts (the item's on-use spell, or the quest spell itself), for its range.
    uint32 useSpellId = 0;
};

// Filled when a diag pointer is passed: why the seek did or did not produce a target.
struct QuestUseSeekDiag
{
    QuestUseMode mode = QuestUseMode::None;
    uint32 nearbyUnits = 0;
    uint32 matchingEntry = 0;
    uint32 aliveMatching = 0;
    // The entry set the seek accepts (required entry plus tool-spell condition entries), so the
    // stay-end verdict can count kills of the real targetable creatures as relevant when the
    // objective credits an unspawnable dummy.
    std::vector<uint32> acceptedEntries;
    // Alive matching candidates that also pass the range cap: the count the seek can actually
    // return from. aliveMatching > 0 with inRange == 0 is the range-filter starvation signature.
    uint32 inRange = 0;
};

[[nodiscard]] QuestUseTarget FindQuestUseTarget(PlayerbotAI* botAI, Quest const* quest, int32 objectiveIdx,
                                                GuidVector const& nearbyUnits, float anchorX, float anchorY,
                                                float anchorRadius, QuestUseSeekDiag* diag = nullptr);

// Uses the tool on the target. Item mode goes through PlayerbotAI::ImbueItem (CMSG_USE_ITEM on a
// unit target); spell mode through PlayerbotAI::CastSpell. Returns true when the use was
// dispatched.
bool EngageQuestUseTarget(PlayerbotAI* botAI, QuestUseTarget const& target);

// Whether the objective is credited by USING a tool on the creature rather than killing it: the
// bot holds the quest's source item with an on-use spell, or knows one of the quest's mapped
// spells. Grind targeting consults this so a use-credited stay is not spent brawling bystanders.
[[nodiscard]] bool QuestObjectiveHasUseTool(PlayerbotAI* botAI, Quest const* quest, int32 objectiveIdx);

#endif
