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

#include "Ai/World/Rpg/QuestStayUseTracker.h"
#include "ConditionMgr.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Playerbots.h"
#include "QuestDef.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

#include <algorithm>

namespace
{
// A held quest item qualifies as a tool only when it carries an on-use spell: the Foreman's
// Blackjack and the Inoculating Crystal do, a readable starter letter does not. The tool is the
// quest's provided item when it has one, otherwise one of the quest's ItemDrop entries the bot has
// already looted (measured 2026-09-01: Kyle's Gone Missing, quest 11129, feeds Kyle with Tender
// Strider Meat 33009 that drops from plainstriders and is never handed out by the giver).
// useSpellId, when requested, receives that on-use spell.
Item* HeldItemWithUseSpell(Player* bot, uint32 itemEntry, uint32* useSpellId)
{
    if (!itemEntry)
        return nullptr;

    Item* item = bot->GetItemByEntry(itemEntry);
    if (!item)
        return nullptr;

    ItemTemplate const* proto = item->GetTemplate();
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        if (proto->Spells[i].SpellId > 0 && proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
        {
            if (useSpellId)
                *useSpellId = static_cast<uint32>(proto->Spells[i].SpellId);
            return item;
        }

    return nullptr;
}

Item* SourceItemWithUseSpell(Player* bot, Quest const* quest, uint32* useSpellId = nullptr)
{
    if (Item* item = HeldItemWithUseSpell(bot, quest->GetSrcItemId(), useSpellId))
        return item;
    for (uint8 i = 0; i < QUEST_SOURCE_ITEM_IDS_COUNT; ++i)
        if (Item* item = HeldItemWithUseSpell(bot, quest->ItemDrop[i], useSpellId))
            return item;
    return nullptr;
}

// Creature entries the tool spell is conditioned to target, from both places the conditions
// table lands at load: whole-spell cast conditions (source 17, the generic store) and per-effect
// implicit-target conditions (source 13, attached to SpellInfo effects). Inoculation's spell
// 29528 carries a source-17 OBJECT_ENTRY_GUID row naming creature 16518; without reading it the
// seek matches only the quest's credit dummy 16534, which never stands in the world.
void AppendConditionEntries(ConditionList const& conditions, std::vector<uint32>& entries)
{
    for (Condition const* cond : conditions)
    {
        if (!cond || cond->NegativeCondition)
            continue;
        if (cond->ConditionType != CONDITION_OBJECT_ENTRY_GUID)
            continue;
        if (cond->ConditionValue1 != TYPEID_UNIT || !cond->ConditionValue2)
            continue;
        entries.push_back(cond->ConditionValue2);
    }
}

std::vector<uint32> ToolSpellTargetEntries(uint32 spellId)
{
    std::vector<uint32> entries;
    if (!spellId)
        return entries;

    AppendConditionEntries(sConditionMgr->GetConditionsForNotGroupedEntry(CONDITION_SOURCE_TYPE_SPELL, spellId),
                           entries);

    if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId))
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            if (ConditionList const* effectConditions = spellInfo->Effects[i].ImplicitTargetConditions)
                AppendConditionEntries(*effectConditions, entries);

    return entries;
}

// Whether the tool spell only credits a sleeping target: a CONDITION_AURA row on the spell (cast
// or per-effect) whose spell id is a sleep aura. The Foreman's Blackjack's spell 19938 carries
// aura 17743 ("Awaken Peon" condition). See BestQuestUseTargetIndex for the measurement.
bool ConditionsDemandAura(ConditionList const& conditions)
{
    for (Condition const* cond : conditions)
        if (cond && !cond->NegativeCondition && cond->ConditionType == CONDITION_AURA && cond->ConditionValue1)
            return true;
    return false;
}

bool ToolSpellRequiresSleeper(uint32 spellId)
{
    if (!spellId)
        return false;
    if (ConditionsDemandAura(sConditionMgr->GetConditionsForNotGroupedEntry(CONDITION_SOURCE_TYPE_SPELL, spellId)))
        return true;
    if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId))
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            if (ConditionList const* effectConditions = spellInfo->Effects[i].ImplicitTargetConditions)
                if (ConditionsDemandAura(*effectConditions))
                    return true;
    return false;
}

uint32 KnownQuestSpell(Player* bot, uint32 questId, uint32 attempt)
{
    std::vector<uint32> known;
    for (uint32 spellId : QuestUseSpellsForQuest(questId))
        if (bot->HasSpell(spellId))
            known.push_back(spellId);
    return QuestUseSpellForAttempt(known, attempt);
}
}  // namespace

bool QuestObjectiveHasUseTool(PlayerbotAI* botAI, Quest const* quest, int32 objectiveIdx)
{
    if (!botAI || !quest || objectiveIdx < 0 || objectiveIdx >= QUEST_OBJECTIVES_COUNT)
        return false;
    if (quest->RequiredNpcOrGo[objectiveIdx] <= 0)
        return false;

    Player* bot = botAI->GetBot();
    return SourceItemWithUseSpell(bot, quest) != nullptr || KnownQuestSpell(bot, quest->GetQuestId(), 0) != 0;
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
    uint32 useSpellId = 0;
    if (Item* item = SourceItemWithUseSpell(bot, quest, &useSpellId))
    {
        mode = QuestUseMode::Item;
        toolId = item->GetEntry();
    }
    else if (uint32 spellId = KnownQuestSpell(bot, quest->GetQuestId(), QuestStayUseTracker::AttemptsThisStay(bot)))
    {
        mode = QuestUseMode::Spell;
        toolId = spellId;
        useSpellId = spellId;
    }
    else
        return {};  // no tool: a genuine kill quest, the grind strategy owns it

    if (diag)
        diag->mode = mode;

    std::vector<uint32> const acceptedEntries =
        QuestUseAcceptedEntries(requiredEntry, ToolSpellTargetEntries(useSpellId));
    if (diag)
        diag->acceptedEntries = acceptedEntries;

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
        candidate.matchesEntry =
            std::find(acceptedEntries.begin(), acceptedEntries.end(), unit->GetEntry()) != acceptedEntries.end();
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
        if (diag && candidate.matchesEntry && candidate.alive &&
            QuestUseCandidateInRange(candidate, anchorRadius > 0.0f ? anchorRadius * anchorRadius : 0.0f))
            ++diag->inRange;

        QuestUseTarget target;
        target.guid = guid;
        target.entry = unit->GetEntry();
        target.mode = mode;
        target.toolId = toolId;
        target.useSpellId = useSpellId;

        facts.push_back(candidate);
        targets.push_back(target);
    }

    size_t const best = BestQuestUseTargetIndex(facts, anchorRadius > 0.0f ? anchorRadius * anchorRadius : 0.0f,
                                                ToolSpellRequiresSleeper(useSpellId));
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
