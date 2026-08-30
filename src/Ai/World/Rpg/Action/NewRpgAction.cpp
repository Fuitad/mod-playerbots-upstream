/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NewRpgAction.h"

// PLB-LOCAL(quest-poi-nearest-candidate): nearest-candidate selection.
#include "Ai/World/Rpg/QuestPoiPointPolicy.h"

// PLB-LOCAL(quest-poi-approach): approach policy for a POI the bot has drifted away from.
#include "Ai/World/Rpg/QuestPoiApproachPolicy.h"
// PLB-LOCAL(quest-gameobject-objective): operate the gameobject the current quest objective
// needs while standing at its POI. Pure decisions live in QuestGameObjectPolicy.h, world glue
// in NewRpgQuestGameObject.cpp; this file carries only the call site.
#include "Ai/World/Rpg/Action/NewRpgQuestGameObject.h"
// PLB-LOCAL(quest-use-target): operate the creature a use-credited objective needs (blackjack a
// peon, inoculate an owlkin, cast a racial on a survivor). Pure decisions in
// QuestUseTargetPolicy.h, world glue in NewRpgQuestUseTarget.cpp; this file carries the call site.
#include "Ai/World/Rpg/Action/NewRpgQuestUseTarget.h"
// PLB-LOCAL(quest-stay-kill-probe): temporary diagnostic, see the header's banner. Playerbots.h is
// pulled in for AI_VALUE so the stay-end records can sample the grind pipeline's own values.
#include "Ai/World/Rpg/QuestStayKillProbe.h"
// PLB-LOCAL(quest-stay-use-tracker): counts objective interactions per stay so the stay-end
// verdict (QuestStayEndDecision in QuestDropPolicy.h) can rotate instead of abandoning a stay
// that dispatched its tool but lost the race for a creditable target.
#include "Ai/World/Rpg/QuestDropPolicy.h"
#include "Ai/World/Rpg/QuestStayUseTracker.h"
#include "Playerbots.h"
#include "AreaDefines.h"
#include "BroadcastHelper.h"
#include "ChatHelper.h"
#include "GossipDef.h"
// PLB-LOCAL(quest-abandon-probe): GameObject facts for the GOLOOT diagnostic line.
#include "GameObject.h"
#include "IVMapMgr.h"
// PLB-LOCAL(quest-gameobject-objective): LootObjectStack::Add hands quest chests to the loot pipeline.
#include "LootObjectStack.h"
#include "NewRpgInfo.h"
#include "NewRpgStrategy.h"
#include "Object.h"
#include "ObjectAccessor.h"
#include "ObjectDefines.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "PathGenerator.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotTextMgr.h"
#include "QuestDef.h"
#include "Random.h"
#include "SharedDefines.h"
#include "Timer.h"
#include "TravelMgr.h"
#include "G3D/Vector2.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <unordered_map>

// PLB-LOCAL BEGIN(quest-abandon-probe): temporary diagnostic. The approach branches below hold
// the tick with no log line, so a bot that never closes on its seek target (failed path, combat
// resets) burns a stay invisibly. One line per bot per 15s, same throttle idiom as the
// loot-skip probes. Remove with the quest-abandon-probe family.
namespace
{
std::mutex approachProbeMutex;
std::unordered_map<ObjectGuid::LowType, time_t> approachProbeLastLog;

bool ApproachProbeDue(Player* bot)
{
    std::lock_guard<std::mutex> lock(approachProbeMutex);
    time_t& last = approachProbeLastLog[bot->GetGUID().GetCounter()];
    time_t const now = time(nullptr);
    if (now - last < 15)
        return false;
    last = now;
    return true;
}
}  // namespace
// PLB-LOCAL END(quest-abandon-probe)

void TellRpgStatusAction::WhisperStatusChange(Player* owner, std::string const& statusName)
{
    std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
        RPG_STATUS_CHANGED_KEY, RPG_STATUS_CHANGED_DEFAULT,
        {{"%status", statusName}});
    bot->Whisper(msg, LANG_UNIVERSAL, owner);
}

bool TellRpgStatusAction::Execute(Event event)
{
    Player* owner = event.getOwner();
    if (!owner)
        return false;

    std::string const text = event.getParam();
    if (text.empty())
    {
        std::string out = botAI->rpgInfo.ToString();
        bot->Whisper(out.c_str(), LANG_UNIVERSAL, owner);
        return true;
    }

    Player* master = botAI->GetMaster();
    bool isMaster = master && master->GetGUID() == owner->GetGUID();
    bool isGM = owner->GetSession() && owner->GetSession()->GetSecurity() >= SEC_GAMEMASTER;
    if (!isMaster && !isGM)
    {
        std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "rpg_debug_permission_error",
            "Only your master or a GM can change my rpg status.", {});
        bot->Whisper(msg, LANG_UNIVERSAL, owner);
        return false;
    }

    std::string name = text;
    uint32 questId = 0;
    static std::string const doQuestPrefix = "do quest ";
    size_t doQuestPos = text.find(doQuestPrefix);
    if (doQuestPos != std::string::npos)
    {
        name = "do quest";
        std::string idStr = text.substr(doQuestPos + doQuestPrefix.length());
        try
        {
            questId = static_cast<uint32>(std::stoul(idStr));
        }
        catch (std::exception const&)
        {
            questId = 0;
        }
    }

    NewRpgStatus status = NewRpgInfo::StatusFromString(name);
    NewRpgInfo& info = botAI->rpgInfo;

    if (status == RPG_IDLE)
    {
        info.ChangeToIdle();
        WhisperStatusChange(owner, "IDLE");
        return true;
    }
    else if (status == RPG_REST)
    {
        info.ChangeToRest();
        bot->SetStandState(UNIT_STAND_STATE_SIT);
        WhisperStatusChange(owner, "REST");
        return true;
    }
    else if (status == RPG_WANDER_RANDOM)
    {
        info.ChangeToWanderRandom();
        WhisperStatusChange(owner, "WANDER_RANDOM");
        return true;
    }
    else if (status == RPG_WANDER_NPC)
    {
        info.ChangeToWanderNpc();
        WhisperStatusChange(owner, "WANDER_NPC");
        return true;
    }
    else if (status == RPG_GO_GRIND)
    {
        WorldPosition pos = SelectRandomGrindPos(bot);
        if (pos == WorldPosition())
        {
            std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
                "rpg_no_grind_pos_error", "No grind position available.", {});
            bot->Whisper(msg, LANG_UNIVERSAL, owner);
            return false;
        }
        info.ChangeToGoGrind(pos);
        WhisperStatusChange(owner, "GO_GRIND");
        return true;
    }
    else if (status == RPG_GO_CAMP)
    {
        WorldPosition pos = SelectRandomCampPos(bot);
        if (pos == WorldPosition())
        {
            std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
                "rpg_no_camp_pos_error", "No camp position available.", {});
            bot->Whisper(msg, LANG_UNIVERSAL, owner);
            return false;
        }
        info.ChangeToGoCamp(pos);
        WhisperStatusChange(owner, "GO_CAMP");
        return true;
    }
    else if (status == RPG_TRAVEL_FLIGHT)
    {
        uint32 flightMasterEntry = 0;
        WorldPosition flightMasterPos;
        std::vector<uint32> path;
        if (!SelectRandomFlightTaxiNode(flightMasterEntry, flightMasterPos, path))
        {
            std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
                "rpg_no_flight_path_error", "No flight path available.", {});
            bot->Whisper(msg, LANG_UNIVERSAL, owner);
            return false;
        }
        info.ChangeToTravelFlight(flightMasterEntry, flightMasterPos, std::move(path));
        WhisperStatusChange(owner, "TRAVEL_FLIGHT");
        return true;
    }
    else if (status == RPG_OUTDOOR_PVP)
    {
        info.ChangeToOutdoorPvp();
        WhisperStatusChange(owner, "OUTDOOR_PVP");
        return true;
    }
    else if (status == RPG_DO_QUEST)
    {
        if (!questId)
        {
            for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
            {
                uint32 qid = bot->GetQuestSlotQuestId(slot);
                if (!qid)
                    continue;
                std::vector<POIInfo> poi;
                if (GetQuestPOIPosAndObjectiveIdx(qid, poi, true))
                {
                    questId = qid;
                    break;
                }
            }
        }
        if (!questId)
        {
            std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
                "rpg_no_quest_error", "No quest available; use 'do quest <id>'.", {});
            bot->Whisper(msg, LANG_UNIVERSAL, owner);
            return false;
        }
        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        QuestStatus questStatus = bot->GetQuestStatus(questId);
        if (!quest || (questStatus != QUEST_STATUS_INCOMPLETE && questStatus != QUEST_STATUS_COMPLETE))
        {
            std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
                "rpg_invalid_quest_error", "Invalid quest %quest_id",
                {{"%quest_id", std::to_string(questId)}});
            bot->Whisper(msg, LANG_UNIVERSAL, owner);
            return false;
        }
        info.ChangeToDoQuest(questId, quest);
        WhisperStatusChange(owner, "DO_QUEST " + std::to_string(questId));
        return true;
    }

    std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
        "rpg_unknown_status_error",
        "Unknown rpg status. Options: idle, rest, wander random, wander npc, "
        "go grind, go camp, do quest [<id>], travel flight, outdoor pvp.", {});
    bot->Whisper(msg, LANG_UNIVERSAL, owner);
    return false;
}

bool StartRpgDoQuestAction::Execute(Event event)
{
    Player* owner = event.getOwner();
    if (!owner)
        return false;

    std::string const text = event.getParam();
    PlayerbotChatHandler ch(owner);
    uint32 questId = ch.extractQuestId(text);
    const Quest* quest = sObjectMgr->GetQuestTemplate(questId);
    if (quest)
    {
        botAI->rpgInfo.ChangeToDoQuest(questId, quest);
        bot->Whisper("Start to do quest " + std::to_string(questId), LANG_UNIVERSAL, owner);
        return true;
    }
    bot->Whisper("Invalid quest " + text, LANG_UNIVERSAL, owner);
    return false;
}

bool NewRpgStatusUpdateAction::Execute(Event /*event*/)
{
    NewRpgInfo& info = botAI->rpgInfo;
    NewRpgStatus status = info.GetStatus();
    switch (status)
    {
        case RPG_IDLE:
            return RandomChangeStatus({RPG_GO_CAMP, RPG_GO_GRIND, RPG_WANDER_RANDOM, RPG_WANDER_NPC, RPG_DO_QUEST,
                                       RPG_TRAVEL_FLIGHT, RPG_REST, RPG_OUTDOOR_PVP});

        case RPG_GO_GRIND:
        {
            auto& data = std::get<NewRpgInfo::GoGrind>(info.data);
            WorldPosition& originalPos = data.pos;
            assert(data.pos != WorldPosition());
            // GO_GRIND -> WANDER_RANDOM
            if (bot->GetExactDist(originalPos) < 10.0f)
            {
                info.ChangeToWanderRandom();
                return true;
            }
            break;
        }
        case RPG_GO_CAMP:
        {
            auto& data = std::get<NewRpgInfo::GoCamp>(info.data);
            WorldPosition& originalPos = data.pos;
            assert(data.pos != WorldPosition());
            // GO_CAMP -> WANDER_NPC
            if (bot->GetExactDist(originalPos) < 10.0f)
            {
                info.ChangeToWanderNpc();
                return true;
            }
            break;
        }
        case RPG_WANDER_RANDOM:
        {
            // WANDER_RANDOM -> IDLE
            if (info.HasStatusPersisted(statusWanderRandomDuration))
            {
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_WANDER_NPC:
        {
            if (info.HasStatusPersisted(statusWanderNpcDuration))
            {
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_DO_QUEST:
        {
            // DO_QUEST -> IDLE
            if (info.HasStatusPersisted(statusDoQuestDuration))
            {
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_TRAVEL_FLIGHT:
        {
            auto& data = std::get<NewRpgInfo::TravelFlight>(info.data);
            if (data.inFlight && !bot->IsInFlight())
            {
                // flight arrival
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_REST:
        {
            // REST -> IDLE
            if (info.HasStatusPersisted(statusRestDuration))
            {
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_OUTDOOR_PVP:
        {
            if (info.HasStatusPersisted(statusOutDoorPvPDuration))
            {
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        default:
            break;
    }
    return false;
}

bool NewRpgGoGrindAction::Execute(Event /*event*/)
{
    if (SearchQuestGiverAndAcceptOrReward())
        return true;
    if (auto* data = std::get_if<NewRpgInfo::GoGrind>(&botAI->rpgInfo.data))
    {
        if (MoveFarTo(data->pos))
            return true;
        // Small nudge so the next tick's MoveFarTo starts from a
        // slightly different position. Kept small so it doesn't look
        // like the bot is abandoning its destination.
        return MoveRandomNear(10.0f);
    }

    return false;
}

bool NewRpgGoCampAction::Execute(Event /*event*/)
{
    if (SearchQuestGiverAndAcceptOrReward())
        return true;

    if (auto* data = std::get_if<NewRpgInfo::GoCamp>(&botAI->rpgInfo.data))
    {
        if (MoveFarTo(data->pos))
            return true;
        return MoveRandomNear(10.0f);
    }

    return false;
}

bool NewRpgWanderRandomAction::Execute(Event /*event*/)
{
    if (SearchQuestGiverAndAcceptOrReward())
        return true;

    return MoveRandomNear();
}

bool NewRpgWanderNpcAction::Execute(Event /*event*/)
{
    NewRpgInfo& info = botAI->rpgInfo;
    auto* dataPtr = std::get_if<NewRpgInfo::WanderNpc>(&info.data);
    if (!dataPtr)
        return false;
    auto& data = *dataPtr;
    if (!data.npcOrGo)
    {
        // No npc can be found, switch to IDLE
        ObjectGuid npcOrGo = ChooseNpcOrGameObjectToInteract();
        if (npcOrGo.IsEmpty())
        {
            info.ChangeToIdle();
            return true;
        }
        data.npcOrGo = npcOrGo;
        data.lastReach = 0;
        return true;
    }

    WorldObject* object = ObjectAccessor::GetWorldObject(*bot, data.npcOrGo);
    if (object && IsWithinInteractionDist(object))
    {
        if (!data.lastReach)
        {
            data.lastReach = getMSTime();
            if (bot->CanInteractWithQuestGiver(object))
                InteractWithNpcOrGameObjectForQuest(data.npcOrGo);
            return true;
        }

        if (data.lastReach && GetMSTimeDiffToNow(data.lastReach) < npcStayTime)
            return false;

        // has reached the npc for more than `npcStayTime`, select the next target
        data.npcOrGo = ObjectGuid();
        data.lastReach = 0;
    }
    else
    {
        if (MoveWorldObjectTo(data.npcOrGo))
            return true;
        // NPC pathing failed (random offset in a wall, mmap hiccup, etc).
        // Take a small random step so the next tick retries from a
        // different spot instead of staring at the NPC from afar.
        return MoveRandomNear(15.0f);
    }

    return true;
}

bool NewRpgDoQuestAction::Execute(Event /*event*/)
{
    if (SearchQuestGiverAndAcceptOrReward())
        return true;

    NewRpgInfo& info = botAI->rpgInfo;
    auto* dataPtr = std::get_if<NewRpgInfo::DoQuest>(&info.data);
    if (!dataPtr)
        return false;
    auto& data = *dataPtr;
    uint32 questId = data.questId;
    uint8 questStatus = bot->GetQuestStatus(questId);
    switch (questStatus)
    {
        case QUEST_STATUS_INCOMPLETE:
            return DoIncompleteQuest(data);
        case QUEST_STATUS_COMPLETE:
            return DoCompletedQuest(data);
        default:
            break;
    }
    info.ChangeToIdle();
    return true;
}

bool NewRpgDoQuestAction::DoIncompleteQuest(NewRpgInfo::DoQuest& data)
{
    uint32 questId = data.questId;
    if (data.pos != WorldPosition())
    {
        /// @TODO: extract to a new function
        int32 currentObjective = data.objectiveIdx;
        // check if the objective has completed
        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        const QuestStatusData& q_status = bot->getQuestStatusMap().at(questId);
        bool completed = true;
        if (currentObjective < QUEST_OBJECTIVES_COUNT)
        {
            if (q_status.CreatureOrGOCount[currentObjective] < quest->RequiredNpcOrGoCount[currentObjective])
                completed = false;
        }
        else if (currentObjective < QUEST_OBJECTIVES_COUNT + QUEST_ITEM_OBJECTIVES_COUNT)
        {
            if (q_status.ItemCount[currentObjective - QUEST_OBJECTIVES_COUNT] <
                quest->RequiredItemCount[currentObjective - QUEST_OBJECTIVES_COUNT])
                completed = false;
        }
        // the current objective is completed, clear and find a new objective later
        if (completed)
        {
            data.lastReachPOI = 0;
            data.pos = WorldPosition();
            data.objectiveIdx = 0;
        }
    }
    if (data.pos == WorldPosition())
    {
        std::vector<POIInfo> poiInfo;
        if (!GetQuestPOIPosAndObjectiveIdx(questId, poiInfo))
        {
            // can't find a poi pos to go, stop doing quest for now
            botAI->rpgInfo.ChangeToIdle();
            return true;
        }
        // PLB-LOCAL BEGIN(quest-poi-nearest-candidate): choose the NEAREST candidate POI rather than a
        // random one. b0a52817 fixed which vertex is aimed at within a POI; the choice BETWEEN a
        // quest's POIs was still `urand(0, poiInfo.size() - 1)`, which is what actually sets how far
        // the bot is sent.
        // Upstream: `uint32 rndIdx = urand(0, poiInfo.size() - 1);` despite naming the result
        // nearestPoi.
        // Why nearest: measured live, PathGenerator fails 0 times under 100 yards, 494 times between
        // 100 and 400, and 800 times beyond 800 yards. Distance is the single strongest predictor of
        // a bot never arriving, so sending it to the closest candidate is the cheapest available
        // reduction in pathing failure.
        std::vector<std::pair<float, float>> poiCandidates;
        poiCandidates.reserve(poiInfo.size());
        for (POIInfo const& candidate : poiInfo)
            poiCandidates.emplace_back(candidate.pos.x, candidate.pos.y);
        size_t const rndIdx = NearestPoiPointIndex(poiCandidates, bot->GetPositionX(), bot->GetPositionY());
        G3D::Vector2 nearestPoi = poiInfo[rndIdx].pos;
        int32 objectiveIdx = poiInfo[rndIdx].objectiveIdx;
        // PLB-LOCAL END(quest-poi-nearest-candidate)

        float dx = nearestPoi.x, dy = nearestPoi.y;

        // z = MAX_HEIGHT as we do not know accurate z
        // PLB-LOCAL(quest-stay-spawn-anchor): unless the anchor was snapped to a real spawn,
        // whose row carries the true z. A cave spawn's surface height is the terrain above it.
        float dz = poiInfo[rndIdx].hasSpawnZ
                       ? poiInfo[rndIdx].spawnZ
                       : std::max(bot->GetMap()->GetHeight(dx, dy, MAX_HEIGHT), bot->GetMap()->GetWaterLevel(dx, dy));

        // double check for GetQuestPOIPosAndObjectiveIdx
        if (dz == INVALID_HEIGHT || dz == VMAP_INVALID_HEIGHT_VALUE)
            return false;

        WorldPosition pos(bot->GetMapId(), dx, dy, dz);
        data.lastReachPOI = 0;
        data.pos = pos;
        data.objectiveIdx = objectiveIdx;
        // PLB-LOCAL(quest-abandon-probe): temporary diagnostic. Records which POI of the candidate
        // set was drawn, how far it is, and the guessed z, so the abandon record below can be read
        // against it. Upstream logs nothing here. Remove once the abandon cause is settled.
        LOG_DEBUG("playerbots", "[QuestProbe] {} PICK quest {} poi {}/{} obj {} dist {:.0f}y z {:.1f}",
                  bot->GetName(), questId, rndIdx, poiInfo.size(), objectiveIdx, bot->GetDistance2d(dx, dy), dz);
    }

    // PLB-LOCAL BEGIN(quest-poi-approach): also walk back when the bot has drifted off a POI it had
    // already reached. Upstream: `bot->GetDistance(data.pos) > 10.0f && !data.lastReachPOI`, where
    // lastReachPOI latches on first arrival and permanently disables this branch. See
    // QuestPoiApproachPolicy.h for the measurements.
    if (QuestPoiNeedsApproach({bot->GetDistance(data.pos), data.lastReachPOI != 0, 10.0f,
                               sPlayerbotAIConfig.grindDistance}))
    {
        if (MoveFarTo(data.pos))
            return true;
        // Long-range sampler couldn't land a candidate — nudge the
        // bot a short distance so the next tick retries from a
        // different position instead of sitting idle.
        return MoveRandomNear(10.0f);
    }
    // PLB-LOCAL END(quest-poi-approach)
    // Now we are near the quest objective
    // kill mobs and looting quest should be done automatically by grind strategy

    if (!data.lastReachPOI)
    {
        data.lastReachPOI = getMSTime();
        // PLB-LOCAL(quest-abandon-probe): temporary diagnostic. Marks the moment the five minute
        // stay timer starts, which is the only point at which the bot counts as having arrived.
        LOG_DEBUG("playerbots", "[QuestProbe] {} REACH quest {} obj {} dist {:.0f}y", bot->GetName(), questId,
                  data.objectiveIdx, bot->GetExactDist(data.pos));
        // PLB-LOCAL(quest-stay-kill-probe): snapshot the kill counter so the stay-end records can
        // report how many creatures the bot killed between arrival and the stay verdict.
        QuestStayKillProbe::MarkStayStart(bot);
        // PLB-LOCAL(quest-stay-use-tracker): fresh interaction count for this stay.
        QuestStayUseTracker::MarkStayStart(bot);
        return true;
    }
    // stayed at this POI for more than 5 minutes
    if (GetMSTimeDiffToNow(data.lastReachPOI) >= poiStayTime)
    {
        bool hasProgression = false;
        int32 currentObjective = data.objectiveIdx;
        // check if the objective has progression
        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        const QuestStatusData& q_status = bot->getQuestStatusMap().at(questId);
        if (currentObjective < QUEST_OBJECTIVES_COUNT)
        {
            if (q_status.CreatureOrGOCount[currentObjective] != 0 && quest->RequiredNpcOrGoCount[currentObjective])
                hasProgression = true;
        }
        else if (currentObjective < QUEST_OBJECTIVES_COUNT + QUEST_ITEM_OBJECTIVES_COUNT)
        {
            if (q_status.ItemCount[currentObjective - QUEST_OBJECTIVES_COUNT] != 0 &&
                quest->RequiredItemCount[currentObjective - QUEST_OBJECTIVES_COUNT])
                hasProgression = true;
        }
        // PLB-LOCAL BEGIN(quest-stay-use-tracker): a fruitless stay that dispatched objective
        // interactions (tool on a creature, quest gameobject queued or operated) rotates without
        // the lowPriorityQuest mark instead of abandoning: the mark is a hard skip for the life of
        // the process, and losing the race for a creditable target (contested sleeping peons,
        // contested quest chests) is not evidence the quest cannot be done here. Decision table in
        // QuestDropPolicy.h. Upstream: `if (!hasProgression)` abandoned unconditionally.
        QuestStayEndVerdict const stayVerdict =
            QuestStayEndDecision(hasProgression, QuestStayUseTracker::AttemptsThisStay(bot));
        if (stayVerdict == QuestStayEndVerdict::Abandon)
        // PLB-LOCAL END(quest-stay-use-tracker)
        {
            // we has reach the poi for more than 5 mins but no progession
            // may not be able to complete this quest, marked as abandoned
            /// @TODO: It may be better to make lowPriorityQuest a global set shared by all bots (or saved in db)
            botAI->lowPriorityQuest.insert(questId);
            botAI->rpgStatistic.questAbandoned++;
            LOG_DEBUG("playerbots", "[New RPG] {} marked as abandoned quest {}", bot->GetName(), questId);
            // PLB-LOCAL(quest-abandon-probe): temporary diagnostic. Where the bot actually was when
            // it gave up, versus the POI it was assigned, plus the objective counter it judged
            // itself on. Distinguishes "never arrived", "arrived then wandered off" and "stood on
            // the objective and still made no progress", which no reading of the code settles.
            uint32 const probeCount = currentObjective < QUEST_OBJECTIVES_COUNT
                                          ? q_status.CreatureOrGOCount[currentObjective]
                                          : (currentObjective < QUEST_OBJECTIVES_COUNT + QUEST_ITEM_OBJECTIVES_COUNT
                                                 ? q_status.ItemCount[currentObjective - QUEST_OBJECTIVES_COUNT]
                                                 : 0u);
            // PLB-LOCAL(quest-use-target): sample the use-seek's view once at abandon so a
            // use-credited quest that died without a QUSE line explains itself: no tool, no
            // matching creature nearby, or none of them alive.
            QuestUseSeekDiag useDiag;
            QuestGoSeekDiag goDiag;
            if (Quest const* diagQuest = sObjectMgr->GetQuestTemplate(questId))
            {
                (void)FindQuestUseTarget(botAI, diagQuest, data.objectiveIdx,
                                         context->GetValue<GuidVector>("nearest npcs")->Get(),
                                         data.pos.GetPositionX(), data.pos.GetPositionY(),
                                         sPlayerbotAIConfig.grindDistance, &useDiag);
                (void)FindQuestObjectiveGameObject(bot, diagQuest, data.objectiveIdx,
                                                   context->GetValue<GuidVector>("nearest game objects")->Get(),
                                                   data.pos.GetPositionX(), data.pos.GetPositionY(),
                                                   sPlayerbotAIConfig.grindDistance, &goDiag);
            }
            LOG_DEBUG("playerbots",
                      "[QuestProbe] {} ABANDON quest {} obj {} distFromPoi {:.0f}y stayed {}s counter {} lvl {} "
                      "kills {} targets {} grind {} curtgt {} usemode {} usecand {}/{}/{}/{} gocand {}/{}/{}/{}",
                      bot->GetName(), questId, currentObjective, bot->GetExactDist(data.pos),
                      GetMSTimeDiffToNow(data.lastReachPOI) / 1000, probeCount, bot->GetLevel(),
                      QuestStayKillProbe::KillsSinceStayStart(bot),
                      AI_VALUE(GuidVector, "possible targets").size(),
                      AI_VALUE(Unit*, "grind target") ? 1 : 0,
                      [this] {
                          Unit* sel = AI_VALUE(Unit*, "current target");
                          return !sel ? 0 : (sel->IsAlive() ? 1 : 2);
                      }(),
                      static_cast<uint32>(useDiag.mode), useDiag.nearbyUnits, useDiag.matchingEntry,
                      useDiag.aliveMatching, useDiag.inRange, goDiag.nearbyGos, goDiag.matching,
                      goDiag.usableMatching, goDiag.inRange);
            botAI->rpgInfo.ChangeToIdle();
            return true;
        }
        // PLB-LOCAL BEGIN(quest-stay-use-tracker): the tried-but-uncredited stay: keep the quest
        // eligible and rotate to another POI/quest draw. The STAYUSE line is the measurement hook
        // for how often this verdict fires and for which quests.
        if (stayVerdict == QuestStayEndVerdict::RotateWithoutBlame)
        {
            LOG_DEBUG("playerbots", "[QuestProbe] {} STAYUSE quest {} obj {} stayed {}s attempts {} lvl {}",
                      bot->GetName(), questId, currentObjective, GetMSTimeDiffToNow(data.lastReachPOI) / 1000,
                      QuestStayUseTracker::AttemptsThisStay(bot), bot->GetLevel());
            data.lastReachPOI = 0;
            data.pos = WorldPosition();
            data.objectiveIdx = 0;
            return true;
        }
        // PLB-LOCAL END(quest-stay-use-tracker)
        // PLB-LOCAL(quest-stay-kill-probe): temporary diagnostic. The contrast group: a stay that
        // ended with objective progression instead of an abandon, with the same kill delta the
        // abandon record carries. Comparing kills between the two groups separates "the bot never
        // fights during a stay" from "it fights but the wrong things or the drops never come".
        LOG_DEBUG("playerbots",
                  "[QuestProbe] {} STAYOK quest {} obj {} stayed {}s kills {} lvl {} targets {} grind {} curtgt {}",
                  bot->GetName(), questId, currentObjective, GetMSTimeDiffToNow(data.lastReachPOI) / 1000,
                  QuestStayKillProbe::KillsSinceStayStart(bot), bot->GetLevel(),
                  AI_VALUE(GuidVector, "possible targets").size(),
                  AI_VALUE(Unit*, "grind target") ? 1 : 0,
                  [this] {
                      Unit* sel = AI_VALUE(Unit*, "current target");
                      return !sel ? 0 : (sel->IsAlive() ? 1 : 2);
                  }());
        // clear and select another poi later
        data.lastReachPOI = 0;
        data.pos = WorldPosition();
        data.objectiveIdx = 0;
        return true;
    }

    // PLB-LOCAL BEGIN(quest-gameobject-objective): operate the gameobject the current objective
    // needs. Upstream idles here on the comment "kill mobs and looting quest should be done
    // automatically by grind strategy", but the grind strategy only kills creatures: a gameobject
    // objective (RequiredNpcOrGo < 0; every low-level case is a GOOBER, which the loot pipeline
    // cannot reach because its loot id is 0) is never used, and a chest holding the required item
    // is never approached, because the loot pipeline only engages within AiPlayerbot.LootDistance
    // (15y) of the bot while quest chests sit scattered across the whole POI area. Measured live
    // 2026-08-28: 34% of abandoned quests could not be progressed by killing anything.
    // Upstream: falls straight through to MoveRandomNear(8.0f).
    {
        Quest const* qTemplate = sObjectMgr->GetQuestTemplate(questId);
        GuidVector nearbyGos = context->GetValue<GuidVector>("nearest game objects")->Get();
        QuestGameObjectTarget const goTarget =
            qTemplate ? FindQuestObjectiveGameObject(bot, qTemplate, data.objectiveIdx, nearbyGos,
                                                     data.pos.GetPositionX(), data.pos.GetPositionY(),
                                                     sPlayerbotAIConfig.grindDistance)
                      : QuestGameObjectTarget{};
        if (goTarget.guid)
        {
            if (!IsQuestGameObjectWithinInteraction(bot, goTarget.guid))
            {
                bool const moved = MoveWorldObjectTo(goTarget.guid, INTERACTION_DISTANCE);
                // PLB-LOCAL(quest-abandon-probe): temporary diagnostic. A stay that ends with
                // zero GOLOOT/GOUSE next to APPROACH lines means the walk never converged.
                if (ApproachProbeDue(bot))
                {
                    GameObject* dbgGo = ObjectAccessor::GetGameObject(*bot, goTarget.guid);
                    LOG_DEBUG("playerbots", "[QuestProbe] {} APPROACH quest {} go {} dist {:.1f} moved {}",
                              bot->GetName(), questId, goTarget.entry,
                              dbgGo ? bot->GetDistance(dbgGo) : -1.0f, moved);
                }
                if (moved)
                    return true;
                // Movement still pending or path momentarily failed: hold the tick instead of
                // wandering. The wander issues its own movement, which keeps the movement wait
                // alive and starves this approach forever - measured live 2026-08-29, a bot spent
                // a whole 451s stay next to a valid use target without ever closing the distance.
                return true;
            }
            else if (goTarget.needsLoot)
            {
                // A chest: queue it for the loot pipeline (relevance 5..8 outranks this action's
                // 3.0), which owns lock, skill, bag and release handling in OpenLootAction. Within
                // interaction range it is inside LootDistance, so the pipeline engages next tick.
                context->GetValue<LootObjectStack*>("available loot")->Get()->Add(goTarget.guid);
                // PLB-LOCAL(quest-stay-use-tracker): counts toward the stay-end verdict.
                QuestStayUseTracker::RecordAttempt(bot);
                // PLB-LOCAL(quest-abandon-probe): temporary diagnostic, extended with the loot
                // pipeline's view of the queued object at queue time. Measured live 2026-08-30:
                // Templeton queued the same flower five times across a 305s stay and the pipeline
                // never opened it, so the refusal happens on the pipeline's own later ticks; these
                // facts pin whether it was already refusable here (z offset, GO state, lock skill)
                // or went bad between ticks (despawn or another looter).
                {
                    GameObject* dbgGo = ObjectAccessor::GetGameObject(*bot, goTarget.guid);
                    LootObject dbgProbe(bot, goTarget.guid);
                    LOG_DEBUG("playerbots",
                              "[QuestProbe] {} GOLOOT quest {} obj {} go {} dz {:.1f} spawned {} state {} "
                              "flags {} activate {} skill {}/{} possible {}",
                              bot->GetName(), questId, data.objectiveIdx, goTarget.entry,
                              dbgGo ? std::abs(dbgGo->GetPositionZ() - bot->GetPositionZ()) : -1.0f,
                              dbgGo ? dbgGo->isSpawned() : false,
                              dbgGo ? static_cast<uint32>(dbgGo->GetGoState()) : 99,
                              dbgGo ? dbgGo->GetUInt32Value(GAMEOBJECT_FLAGS) : 0,
                              dbgGo ? dbgGo->ActivateToQuest(bot) : false, dbgProbe.skillId,
                              dbgProbe.reqSkillValue, dbgProbe.IsLootPossible(bot));
                }
                return true;
            }
            else if (UseQuestGameObject(bot, goTarget.guid))
            {
                // PLB-LOCAL(quest-stay-use-tracker): counts toward the stay-end verdict.
                QuestStayUseTracker::RecordAttempt(bot);
                // PLB-LOCAL(quest-abandon-probe): measurement hook, same family as PICK/REACH/ABANDON.
                LOG_DEBUG("playerbots", "[QuestProbe] {} GOUSE quest {} obj {} go {}", bot->GetName(),
                          questId, data.objectiveIdx, goTarget.entry);
                // Let the use land (goober animation and loot state) before the next decision.
                return ForceToWait(3000);
            }
        }
    }
    // PLB-LOCAL END(quest-gameobject-objective)

    // PLB-LOCAL BEGIN(quest-use-target): operate the creature a use-credited objective needs.
    // Some creature objectives are credited by USING something on the creature, not killing it:
    // wake a Lazy Peon, inoculate a Nestlewood Owlkin, Mana Tap an Arcane Wraith, cast Gift of
    // the Naaru on a Draenei Survivor. The grind strategy can only kill, so bots burned whole
    // stays in heavy combat with the counter at zero (measured live 2026-08-29: abandons with 3
    // to 21 kills and no credit). A quest without a use-tool returns an empty target here and the
    // kill path below stays exactly as it was.
    {
        Quest const* qTemplate = sObjectMgr->GetQuestTemplate(questId);
        GuidVector nearbyUnits = context->GetValue<GuidVector>("nearest npcs")->Get();
        QuestUseTarget const useTarget =
            qTemplate ? FindQuestUseTarget(botAI, qTemplate, data.objectiveIdx, nearbyUnits,
                                           data.pos.GetPositionX(), data.pos.GetPositionY(),
                                           sPlayerbotAIConfig.grindDistance)
                      : QuestUseTarget{};
        if (useTarget.guid)
        {
            Unit* useUnit = botAI->GetUnit(useTarget.guid);
            if (useUnit && bot->GetDistance(useUnit) > INTERACTION_DISTANCE)
            {
                bool const moved = MoveWorldObjectTo(useTarget.guid, INTERACTION_DISTANCE);
                // PLB-LOCAL(quest-abandon-probe): temporary diagnostic, same as the gameobject
                // approach above.
                if (ApproachProbeDue(bot))
                    LOG_DEBUG("playerbots", "[QuestProbe] {} APPROACH quest {} npc {} dist {:.1f} moved {}",
                              bot->GetName(), questId, useTarget.entry, bot->GetDistance(useUnit), moved);
                if (moved)
                    return true;
                // Hold instead of wandering, same reasoning as the gameobject seek above.
                return true;
            }
            else if (EngageQuestUseTarget(botAI, useTarget))
            {
                // PLB-LOCAL(quest-stay-use-tracker): counts toward the stay-end verdict.
                QuestStayUseTracker::RecordAttempt(bot);
                // PLB-LOCAL(quest-abandon-probe): measurement hook, same family as PICK/REACH/ABANDON.
                LOG_DEBUG("playerbots", "[QuestProbe] {} QUSE quest {} obj {} npc {} mode {} tool {}",
                          bot->GetName(), questId, data.objectiveIdx, useTarget.entry,
                          useTarget.mode == QuestUseMode::Item ? "item" : "spell", useTarget.toolId);
                // Let the use land (cast time, credit, respawn state) before the next decision.
                return ForceToWait(2000);
            }
        }
    }
    // PLB-LOCAL END(quest-use-target)

    // At the POI: keep the bot actively placed but avoid large
    // random 20yd hops that look like pacing back and forth. A small
    // ~8yd wander reads as the bot looking around while grind/loot
    // strategies do their work.
    return MoveRandomNear(8.0f);
}

bool NewRpgDoQuestAction::DoCompletedQuest(NewRpgInfo::DoQuest& data)
{
    uint32 questId = data.questId;
    const Quest* quest = data.quest;

    if (data.objectiveIdx != -1)
    {
        // if quest is completed, back to poi with -1 idx to reward
        BroadcastHelper::BroadcastQuestUpdateComplete(botAI, bot, quest);
        botAI->rpgStatistic.questCompleted++;
        std::vector<POIInfo> poiInfo;
        if (!GetQuestPOIPosAndObjectiveIdx(questId, poiInfo, true))
        {
            // can't find a poi pos to reward, stop doing quest for now
            botAI->rpgInfo.ChangeToIdle();
            return false;
        }
        assert(poiInfo.size() > 0);
        // now we get the place to get rewarded
        float dx = poiInfo[0].pos.x, dy = poiInfo[0].pos.y;
        // z = MAX_HEIGHT as we do not know accurate z
        float dz = std::max(bot->GetMap()->GetHeight(dx, dy, MAX_HEIGHT), bot->GetMap()->GetWaterLevel(dx, dy));

        // double check for GetQuestPOIPosAndObjectiveIdx
        if (dz == INVALID_HEIGHT || dz == VMAP_INVALID_HEIGHT_VALUE)
            return false;

        WorldPosition pos(bot->GetMapId(), dx, dy, dz);
        data.lastReachPOI = 0;
        data.pos = pos;
        data.objectiveIdx = -1;
    }

    if (data.pos == WorldPosition())
        return false;

    // PLB-LOCAL BEGIN(quest-poi-approach): same latch, same fix, on the turn-in walk.
    if (QuestPoiNeedsApproach({bot->GetDistance(data.pos), data.lastReachPOI != 0, 10.0f,
                               sPlayerbotAIConfig.grindDistance}))
    {
        if (MoveFarTo(data.pos))
            return true;
        return MoveRandomNear(10.0f);
    }
    // PLB-LOCAL END(quest-poi-approach)

    // Now we are near the qoi of reward
    // the quest should be rewarded by SearchQuestGiverAndAcceptOrReward
    if (!data.lastReachPOI)
    {
        data.lastReachPOI = getMSTime();
        return true;
    }
    // stayed at this POI for more than 5 minutes
    if (GetMSTimeDiffToNow(data.lastReachPOI) >= poiStayTime)
    {
        // e.g. Can not reward quest to gameobjects
        /// @TODO: It may be better to make lowPriorityQuest a global set shared by all bots (or saved in db)
        botAI->lowPriorityQuest.insert(questId);
        botAI->rpgStatistic.questAbandoned++;
        LOG_DEBUG("playerbots", "[New RPG] {} marked as abandoned quest {}", bot->GetName(), questId);
        botAI->rpgInfo.ChangeToIdle();
        return true;
    }
    return false;
}

bool NewRpgTravelFlightAction::Execute(Event /*event*/)
{
    NewRpgInfo& info = botAI->rpgInfo;
    auto* dataPtr = std::get_if<NewRpgInfo::TravelFlight>(&info.data);
    if (!dataPtr)
        return false;

    auto& data = *dataPtr;
    if (bot->IsInFlight())
    {
        data.inFlight = true;
        return false;
    }

    if (bot->GetDistance(data.flightMasterPos) > INTERACTION_DISTANCE)
        return MoveFarTo(data.flightMasterPos);

    Creature* flightMaster = bot->FindNearestCreature(data.flightMasterEntry, INTERACTION_DISTANCE * 3);
    if (!flightMaster || !flightMaster->IsAlive())
    {
        info.ChangeToIdle();
        return true;
    }
    if (bot->GetDistance(flightMaster) > INTERACTION_DISTANCE)
        return MoveFarTo(flightMaster);

    std::vector<uint32> nodes = data.path;

    botAI->RemoveShapeshift();
    if (bot->IsMounted())
        bot->Dismount();

    bot->GetSession()->SendLearnNewTaxiNode(flightMaster);

    if (!bot->ActivateTaxiPathTo(nodes, flightMaster, 0))
    {
        LOG_DEBUG("playerbots", "[New RPG] {} active taxi path {} (from {} to {}) failed", bot->GetName(),
                  flightMaster->GetEntry(), nodes[0], nodes[nodes.size() - 1]);
        info.ChangeToIdle();
        return true;
    }
    return true;
}
