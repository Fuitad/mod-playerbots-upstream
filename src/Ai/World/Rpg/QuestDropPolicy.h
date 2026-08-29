/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * It owns one decision: whether a quest sitting in a random bot's log should be hard-dropped,
 * the way a player abandons a quest, instead of lingering forever.
 */

#ifndef _PLAYERBOT_QUESTDROPPOLICY_H
#define _PLAYERBOT_QUESTDROPPOLICY_H

#include "Define.h"
#include "Formulas.h"

// When the New RPG system gives up on a quest (NewRpgDoQuestAction::DoIncompleteQuest), it only
// records the quest id in botAI->lowPriorityQuest, an in-memory set that dies with the process.
// The quest itself stays in the 25-slot log forever: after every restart the bot re-attempts
// quests it already gave up on, and quests whose POIs are unreachable from where the bot lives
// (other map, beyond 1500y, other zone) are never worked and never removed. Measured live on
// 2026-08-29: logs silt up toward the cap as bots outlevel content.
//
// The rule: drop a quest only when BOTH hold.
//   (a) It is gray for the bot, no XP, by the core's own quest-gray test
//       (Player::GetQuestRate: questLevel <= Acore::XP::GetGrayLevel(botLevel)).
//   (b) The bot has already given up on it (lowPriorityQuest), or it cannot be worked from here
//       (GetQuestPOIPosAndObjectiveIdx found no reachable POI for any incomplete objective).
// A quest that is COMPLETE (ready to turn in) is never dropped, and neither is anything the bot
// could still reasonably do at level.
struct QuestDropFacts
{
    uint8 botLevel = 0;
    // Quest::GetQuestLevel(). A value <= 0 means the quest scales with the player and can never
    // be gray (Player::GetQuestLevel substitutes the player's own level).
    int32 questLevel = 0;
    // QUEST_STATUS_COMPLETE: ready to turn in.
    bool complete = false;
    // The quest id is in botAI->lowPriorityQuest: the RPG system already abandoned it once.
    bool givenUp = false;
    // GetQuestPOIPosAndObjectiveIdx found at least one workable POI for an incomplete objective.
    bool reachable = true;
};

enum class QuestDropVerdict : uint8
{
    Keep,
    DropGivenUp,
    DropUnreachable,
};

[[nodiscard]] inline bool QuestIsGrayFor(uint8 botLevel, int32 questLevel)
{
    if (questLevel <= 0)
        return false;
    return questLevel <= static_cast<int32>(Acore::XP::GetGrayLevel(botLevel));
}

// True while the verdict still depends on facts.reachable, so the caller can skip the POI
// computation whenever its answer cannot change the outcome.
[[nodiscard]] inline bool QuestDropNeedsReachability(QuestDropFacts const& facts)
{
    return !facts.complete && !facts.givenUp && QuestIsGrayFor(facts.botLevel, facts.questLevel);
}

[[nodiscard]] inline QuestDropVerdict QuestDropDecision(QuestDropFacts const& facts)
{
    if (facts.complete)
        return QuestDropVerdict::Keep;
    if (!QuestIsGrayFor(facts.botLevel, facts.questLevel))
        return QuestDropVerdict::Keep;
    if (facts.givenUp)
        return QuestDropVerdict::DropGivenUp;
    if (!facts.reachable)
        return QuestDropVerdict::DropUnreachable;
    return QuestDropVerdict::Keep;
}

[[nodiscard]] inline char const* QuestDropReasonName(QuestDropVerdict verdict)
{
    switch (verdict)
    {
        case QuestDropVerdict::DropGivenUp:
            return "givenup";
        case QuestDropVerdict::DropUnreachable:
            return "unreachable";
        default:
            return "keep";
    }
}

#endif
