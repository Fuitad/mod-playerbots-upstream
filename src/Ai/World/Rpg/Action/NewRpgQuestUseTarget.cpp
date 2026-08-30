/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 * See NewRpgQuestUseTarget.h.
 */

#include "NewRpgQuestUseTarget.h"

#include "Item.h"
#include "ItemTemplate.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Playerbots.h"
#include "QuestDef.h"
#include "SharedDefines.h"

namespace
{
// The provided quest item qualifies as a tool only when it carries an on-use spell: the
// Foreman's Blackjack and the Inoculating Crystal do, a readable starter letter does not.
Item* SourceItemWithUseSpell(Player* bot, Quest const* quest)
{
    uint32 const itemEntry = quest->GetSrcItemId();
    if (!itemEntry)
        return nullptr;

    Item* item = bot->GetItemByEntry(itemEntry);
    if (!item)
        return nullptr;

    ItemTemplate const* proto = item->GetTemplate();
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        if (proto->Spells[i].SpellId > 0 && proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
            return item;

    return nullptr;
}

uint32 KnownQuestSpell(Player* bot, uint32 questId)
{
    for (uint32 spellId : QuestUseSpellsForQuest(questId))
        if (bot->HasSpell(spellId))
            return spellId;
    return 0;
}
}  // namespace

bool QuestObjectiveHasUseTool(PlayerbotAI* botAI, Quest const* quest, int32 objectiveIdx)
{
    if (!botAI || !quest || objectiveIdx < 0 || objectiveIdx >= QUEST_OBJECTIVES_COUNT)
        return false;
    if (quest->RequiredNpcOrGo[objectiveIdx] <= 0)
        return false;

    Player* bot = botAI->GetBot();
    return SourceItemWithUseSpell(bot, quest) != nullptr || KnownQuestSpell(bot, quest->GetQuestId()) != 0;
}

QuestUseTarget FindQuestUseTarget(PlayerbotAI* botAI, Quest const* quest, int32 objectiveIdx,
                                  GuidVector const& nearbyUnits, float anchorX, float anchorY,
                                  float anchorRadius, QuestUseSeekDiag* diag)
{
    if (!botAI || !quest)
        return {};

    Player* bot = botAI->GetBot();
    if (objectiveIdx < 0 || objectiveIdx >= QUEST_OBJECTIVES_COUNT)
        return {};

    int32 const requiredEntry = quest->RequiredNpcOrGo[objectiveIdx];
    if (requiredEntry <= 0)
        return {};

    QuestUseMode mode = QuestUseMode::None;
    uint32 toolId = 0;
    if (Item* item = SourceItemWithUseSpell(bot, quest))
    {
        mode = QuestUseMode::Item;
        toolId = item->GetEntry();
    }
    else if (uint32 spellId = KnownQuestSpell(bot, quest->GetQuestId()))
    {
        mode = QuestUseMode::Spell;
        toolId = spellId;
    }
    else
        return {};  // no tool: a genuine kill quest, the grind strategy owns it

    if (diag)
        diag->mode = mode;

    std::vector<QuestUseCandidateFacts> facts;
    std::vector<QuestUseTarget> targets;
    facts.reserve(nearbyUnits.size());
    targets.reserve(nearbyUnits.size());

    for (ObjectGuid const& guid : nearbyUnits)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;

        QuestUseCandidateFacts candidate;
        candidate.matchesEntry = unit->GetEntry() == uint32(requiredEntry);
        candidate.alive = unit->IsAlive();
        candidate.sleeping = unit->getStandState() == UNIT_STAND_STATE_SLEEP;
        if (diag)
        {
            ++diag->nearbyUnits;
            if (candidate.matchesEntry)
            {
                ++diag->matchingEntry;
                if (candidate.alive)
                    ++diag->aliveMatching;
            }
        }
        candidate.distanceSq = bot->GetExactDistSq(unit);
        candidate.anchorDistanceSq = unit->GetExactDist2dSq(anchorX, anchorY);

        QuestUseTarget target;
        target.guid = guid;
        target.entry = unit->GetEntry();
        target.mode = mode;
        target.toolId = toolId;

        facts.push_back(candidate);
        targets.push_back(target);
    }

    size_t const best =
        BestQuestUseTargetIndex(facts, anchorRadius > 0.0f ? anchorRadius * anchorRadius : 0.0f);
    if (best == QUEST_USE_NO_CANDIDATE)
        return {};

    return targets[best];
}

bool EngageQuestUseTarget(PlayerbotAI* botAI, QuestUseTarget const& target)
{
    if (!botAI || !target.guid || target.mode == QuestUseMode::None)
        return false;

    Player* bot = botAI->GetBot();
    Unit* unit = botAI->GetUnit(target.guid);
    if (!unit || !unit->IsAlive())
        return false;

    if (bot->isMoving())
        bot->StopMoving();

    if (target.mode == QuestUseMode::Item)
    {
        Item* item = bot->GetItemByEntry(target.toolId);
        if (!item)
            return false;

        bot->SetTarget(target.guid);
        botAI->ImbueItem(item, TARGET_FLAG_UNIT, target.guid);
        return true;
    }

    return botAI->CastSpell(target.toolId, unit);
}
