/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
// PLB-LOCAL UPSTREAM-FILE: this fork changes 10 region(s) of this upstream file.

#include "GrindTargetValue.h"

// PLB-LOCAL(grind-quest-priority): ranking policy for eligible grind candidates.
#include "GrindTargetPolicy.h"
// PLB-LOCAL(grind-poi-stay-engage): use-tool detection for the stay exception below.
#include "Ai/World/Rpg/Action/NewRpgQuestUseTarget.h"
#include "NewRpgInfo.h"
#include "Playerbots.h"
#include "ReputationMgr.h"
#include "ServerFacade.h"
#include "SharedDefines.h"

Unit* GrindTargetValue::Calculate()
{
    uint32 memberCount = 1;
    Group* group = bot->GetGroup();
    if (group)
        memberCount = group->GetMembersCount();

    Unit* target = nullptr;
    uint32 assistCount = 0;
    while (!target && assistCount < memberCount)
    {
        target = FindTargetForGrinding(assistCount++);
    }

    return target;
}

Unit* GrindTargetValue::FindTargetForGrinding(uint32 assistCount)
{
    Group* group = bot->GetGroup();
    Player* master = GetMaster();

    if (master && (master == bot || master->GetMapId() != bot->GetMapId() || master->IsBeingTeleported() ||
                   !GET_PLAYERBOT_AI(master)))
        master = nullptr;

    GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
    for (ObjectGuid const guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        return unit;
    }

    GuidVector targets = *context->GetValue<GuidVector>("possible targets");
    if (targets.empty())
        return nullptr;

    float distance = 0;
    Unit* result = nullptr;
    std::unordered_map<uint32, bool> needForQuestMap;
    // PLB-LOCAL BEGIN(grind-quest-priority): carry the incumbent's quest relevance alongside its
    // distance, and note once whether the bot is working a quest at all.
    // Upstream: neither existed, because selection compared distance only.
    bool resultNeededForQuest = false;
    uint32 resultNeighbours = 0;
    bool const questPriorityActive = botAI->rpgInfo.GetStatus() == RPG_DO_QUEST;
    // PLB-LOCAL END(grind-quest-priority)
    // PLB-LOCAL BEGIN(grind-poi-stay-engage): once the POI stay is running, the eligibility gate
    // below stops demanding quest relevance. See GrindCandidateNeedsQuestRelevance for the
    // measurements. Upstream: no such state was read here.
    //
    // Exception: a USE-credited stay (blackjack a peon, inoculate an owlkin) keeps the relevance
    // demand. Lifting it there made bots brawl bystanders at relevance 4.0, which outranks the
    // do-quest tick at 3.0, so the tool was never used: measured live 2026-08-29 on the fresh
    // cohort, six REACHes on Lazy Peons across four bots with zero blackjack uses while one of
    // them fought scorpids 90 yards from the peons for a whole stay.
    bool stayingAtQuestPoi = false;
    if (questPriorityActive)
        if (auto const* doQuest = std::get_if<NewRpgInfo::DoQuest>(&botAI->rpgInfo.data))
            stayingAtQuestPoi =
                doQuest->lastReachPOI != 0 &&
                !QuestObjectiveHasUseTool(botAI, sObjectMgr->GetQuestTemplate(doQuest->questId),
                                          doQuest->objectiveIdx);
    // PLB-LOCAL END(grind-poi-stay-engage)

    for (ObjectGuid const guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;

        if (!unit->IsInWorld() || unit->IsDuringRemoveFromWorld())
            continue;

        if (unit->ToCreature() && !unit->ToCreature()->GetCreatureTemplate()->lootid &&
            bot->GetReactionTo(unit) >= REP_NEUTRAL)
            continue;

        if (!bot->IsHostileTo(unit) && unit->GetNpcFlags() != UNIT_NPC_FLAG_NONE)
            continue;

        // PLB-LOCAL BEGIN(grind-gray-objective): a gray creature the bot's quest needs stays
        // eligible. See GrindCandidateGrayEligible for the measurement.
        // Upstream: `if (!bot->isHonorOrXPTarget(unit)) continue;`
        if (!bot->isHonorOrXPTarget(unit))
        {
            if (needForQuestMap.find(unit->GetEntry()) == needForQuestMap.end())
                needForQuestMap[unit->GetEntry()] = needForQuest(unit);
            if (!GrindCandidateGrayEligible(false, needForQuestMap[unit->GetEntry()]))
                continue;
        }
        // PLB-LOCAL END(grind-gray-objective)

        if (abs(bot->GetPositionZ() - unit->GetPositionZ()) > INTERACTION_DISTANCE)
            continue;

        if (!bot->InBattleground() && GetTargetingPlayerCount(unit) > assistCount)
            continue;

        // if (!bot->InBattleground() && master && master->GetDistance(unit) >= sPlayerbotAIConfig.grindDistance &&
        // !sRandomPlayerbotMgr.IsRandomBot(bot)) continue;

        // Bots in bot-groups no have a more limited range to look for grind target
        if (!bot->InBattleground() && master && botAI->HasStrategy("follow", BotState::BOT_STATE_NON_COMBAT) &&
            ServerFacade::instance().GetDistance2d(master, unit) > sPlayerbotAIConfig.lootDistance)
        {
            if (botAI->HasStrategy("debug grind", BotState::BOT_STATE_NON_COMBAT))
                botAI->TellMaster(chat->FormatWorldobject(unit) + " ignored (far from master).");
            continue;
        }

        if (!bot->InBattleground() && (int)unit->GetLevel() - (int)bot->GetLevel() > 4 && !unit->GetGUID().IsPlayer())
            continue;

        if (Creature* creature = unit->ToCreature())
            if (CreatureTemplate const* CreatureTemplate = creature->GetCreatureTemplate())
                if (CreatureTemplate->rank > CREATURE_ELITE_NORMAL && !AI_VALUE(bool, "can fight elite"))
                    continue;

        if (!bot->IsWithinLOSInMap(unit))
        {
            continue;
        }

        bool inactiveGrindStatus = botAI->rpgInfo.GetStatus() != RPG_WANDER_RANDOM && botAI->rpgInfo.GetStatus() != RPG_IDLE;

        float aggroRange = 30.0f;
        if (unit->ToCreature())
            aggroRange = std::min(30.0f, unit->ToCreature()->GetAggroRange(bot) + 10.0f);
        bool outOfAggro = unit->ToCreature() && bot->GetDistance(unit) > aggroRange;
        // PLB-LOCAL BEGIN(grind-poi-stay-engage): during a POI stay any eligible candidate may be
        // ground; the quest-priority ranking still puts objective creatures first. Upstream:
        // `if (inactiveGrindStatus && outOfAggro)`, which left bots idle at their POIs whenever no
        // quest-relevant creature was in sight.
        if (GrindCandidateNeedsQuestRelevance(inactiveGrindStatus, outOfAggro, stayingAtQuestPoi))
        // PLB-LOCAL END(grind-poi-stay-engage)
        {
            if (needForQuestMap.find(unit->GetEntry()) == needForQuestMap.end())
                needForQuestMap[unit->GetEntry()] = needForQuest(unit);

            if (!needForQuestMap[unit->GetEntry()])
                continue;
        }

        // PLB-LOCAL BEGIN(grind-quest-priority): while the bot is working a quest, rank a candidate
        // that advances that quest above a merely nearer one. The eligibility rules above are
        // untouched; only the choice between survivors changes, and only when a quest is in hand.
        // Upstream: both branches below picked purely by smallest distance, so at a POI where the
        // objective creature is a minority of the local spawns the bot kept attacking bystanders,
        // made no objective progress, and NewRpgDoQuestAction abandoned the quest after its five
        // minute poiStayTime. See GrindTargetPolicy.h for the measurements.
        bool candidateNeededForQuest = false;
        if (questPriorityActive)
        {
            if (needForQuestMap.find(unit->GetEntry()) == needForQuestMap.end())
                needForQuestMap[unit->GetEntry()] = needForQuest(unit);
            candidateNeededForQuest = needForQuestMap[unit->GetEntry()];
        }
        // PLB-LOCAL BEGIN(grind-camp-pull): the other hostile creatures that would join this pull,
        // each within its own aggro reach of the candidate. See GrindCandidateFacts::hostileNeighbours.
        // Upstream: nothing here, the choice was distance only.
        uint32 hostileNeighbours = 0;
        for (ObjectGuid const otherGuid : targets)
        {
            if (otherGuid == guid)
                continue;
            Unit* other = botAI->GetUnit(otherGuid);
            if (!other || !other->IsAlive() || !other->IsInWorld() || !bot->IsHostileTo(other))
                continue;
            Creature* otherCreature = other->ToCreature();
            float const reach = otherCreature ? otherCreature->GetAggroRange(bot) : 0.0f;
            if (reach > 0.0f && other->GetDistance(unit) <= reach)
                ++hostileNeighbours;
        }
        // PLB-LOCAL END(grind-camp-pull)

        if (group)
        {
            Group::MemberSlotList const& groupSlot = group->GetMemberSlots();
            for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); itr++)
            {
                Player* member = ObjectAccessor::FindPlayer(itr->guid);
                if (!member || !member->IsAlive())
                    continue;

                float d = member->GetDistance(unit);
                if (GrindCandidatePreferred({candidateNeededForQuest, d, hostileNeighbours},
                                            {resultNeededForQuest, distance, resultNeighbours}, questPriorityActive,
                                            result != nullptr))
                {
                    distance = d;
                    result = unit;
                    resultNeededForQuest = candidateNeededForQuest;
                    resultNeighbours = hostileNeighbours;
                }
            }
        }
        else
        {
            float newdistance = bot->GetDistance(unit);
            if (GrindCandidatePreferred({candidateNeededForQuest, newdistance, hostileNeighbours},
                                        {resultNeededForQuest, distance, resultNeighbours}, questPriorityActive,
                                        result != nullptr))
            {
                distance = newdistance;
                result = unit;
                resultNeededForQuest = candidateNeededForQuest;
                resultNeighbours = hostileNeighbours;
            }
        }
        // PLB-LOCAL END(grind-quest-priority)
    }

    return result;
}

bool GrindTargetValue::needForQuest(Unit* target)
{
    QuestStatusMap& questMap = bot->getQuestStatusMap();
    for (auto& quest : questMap)
    {
        Quest const* questTemplate = sObjectMgr->GetQuestTemplate(quest.first);
        if (!questTemplate)
            continue;

        uint32 questId = questTemplate->GetQuestId();
        if (!questId)
            continue;

        QuestStatus status = bot->GetQuestStatus(questId);

        if (status == QUEST_STATUS_INCOMPLETE)
        {
            const QuestStatusData* questStatus = &bot->getQuestStatusMap()[questId];

            if (questTemplate->GetQuestLevel() > bot->GetLevel() + 5)
                continue;

            for (int j = 0; j < QUEST_OBJECTIVES_COUNT; j++)
            {
                int32 entry = questTemplate->RequiredNpcOrGo[j];

                if (entry && entry > 0)
                {
                    int required = questTemplate->RequiredNpcOrGoCount[j];
                    int available = questStatus->CreatureOrGOCount[j];

                    if (required && available < required && target->GetEntry() == uint32(entry))
                        return true;
                }
            }
        }
    }

    if (CreatureTemplate const* data = sObjectMgr->GetCreatureTemplate(target->GetEntry()))
    {
        if (uint32 lootId = data->lootid)
        {
            if (LootTemplates_Creature.HaveQuestLootForPlayer(lootId, bot))
            {
                return true;
            }
        }
    }

    return false;
}

uint32 GrindTargetValue::GetTargetingPlayerCount(Unit* unit)
{
    Group* group = bot->GetGroup();
    if (!group)
        return 0;

    uint32 count = 0;
    Group::MemberSlotList const& groupSlot = group->GetMemberSlots();
    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); itr++)
    {
        Player* member = ObjectAccessor::FindPlayer(itr->guid);
        if (!member || !member->IsAlive() || member == bot)
            continue;

        PlayerbotAI* botAI = GET_PLAYERBOT_AI(member);
        if ((botAI && *botAI->GetAiObjectContext()->GetValue<Unit*>("current target") == unit) ||
            (!botAI && member->GetTarget() == unit->GetGUID()))
            ++count;
    }

    return count;
}
