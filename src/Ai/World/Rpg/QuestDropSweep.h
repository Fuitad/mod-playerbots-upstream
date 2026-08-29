/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * Executes the QuestDropPolicy verdicts against a bot's quest log. The decision itself lives in
 * QuestDropPolicy.h and is unit tested; this is the glue that gathers the facts and performs the
 * drop.
 */

#ifndef _PLAYERBOT_QUESTDROPSWEEP_H
#define _PLAYERBOT_QUESTDROPSWEEP_H

#include "Log.h"
#include "NewRpgInfo.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "QuestDef.h"
#include "QuestDropPolicy.h"
#include "QuestPackets.h"
#include "WorldSession.h"

#include <unordered_set>

// Walks the quest log and hard-drops every quest QuestDropDecision condemns. The drop goes through
// a synthesized CMSG_QUESTLOG_REMOVE_QUEST handled by WorldSession::HandleQuestLogRemoveQuest, the
// same path a player's abandon takes (source item removal, AbandonQuest, RemoveActiveQuest,
// timed-quest and PvP-flag bookkeeping, SetQuestSlot), which is also how OrganizeQuestLog already
// drops quests. The handler can refuse (an unequippable source item), so the slot is re-read
// before the drop is counted.
//
// reachable(questId) must answer whether GetQuestPOIPosAndObjectiveIdx finds a workable POI for
// any incomplete objective of the quest; it is only consulted when the verdict depends on it.
template <typename ReachableFn>
inline uint32 DropStaleGrayQuests(Player* bot, std::unordered_set<uint32> const& givenUpQuests,
                                  NewRpgStatistic& statistic, ReachableFn&& reachable)
{
    uint32 dropped = 0;
    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 const questId = bot->GetQuestSlotQuestId(slot);
        if (!questId)
            continue;

        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (!quest)
            continue;

        QuestDropFacts facts;
        facts.botLevel = bot->GetLevel();
        facts.questLevel = quest->GetQuestLevel();
        facts.complete = bot->GetQuestStatus(questId) == QUEST_STATUS_COMPLETE;
        facts.givenUp = givenUpQuests.find(questId) != givenUpQuests.end();
        if (QuestDropNeedsReachability(facts))
            facts.reachable = reachable(questId);

        QuestDropVerdict const verdict = QuestDropDecision(facts);
        if (verdict == QuestDropVerdict::Keep)
            continue;

        WorldPacket packet(CMSG_QUESTLOG_REMOVE_QUEST);
        packet << (uint8)slot;
        WorldPackets::Quest::QuestLogRemoveQuest removeQuest(std::move(packet));
        removeQuest.Read();
        bot->GetSession()->HandleQuestLogRemoveQuest(removeQuest);

        // The handler rejects the cancel when the quest's source item cannot be unequipped.
        if (bot->GetQuestSlotQuestId(slot) == questId)
            continue;

        statistic.questDropped++;
        ++dropped;
        LOG_DEBUG("playerbots", "[QuestProbe] {} DROP quest {} reason {}", bot->GetName(), questId,
                  QuestDropReasonName(verdict));
    }
    return dropped;
}

#endif
