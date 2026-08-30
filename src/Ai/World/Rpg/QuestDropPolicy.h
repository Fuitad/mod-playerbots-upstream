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

// How a finished POI stay ends. A stay with objective progress rotates normally. A stay with no
// progress used to abandon unconditionally, which is right for a kill stay (five minutes of
// grinding with nothing to show means the place cannot progress the quest) but wrong for an
// interaction-credited stay: the bot dispatched its tool (blackjacked a peon, queued a quest
// chest) and simply lost the race for a creditable target. Measured live 2026-08-29: a bot used
// the Foreman's Blackjack 115 times in one 304s stay, earned nothing because only SLEEPING peons
// credit and only one contested peon was in range, and the abandon then parked the quest in
// lowPriorityQuest, which the quest rotation skips unconditionally for the life of the process.
// Rotating without the mark keeps the retry alive; a quest whose interactions can never credit is
// still bounded by the gray hard-drop above.
enum class QuestStayEndVerdict : uint8
{
    Progressed,
    RotateWithoutBlame,
    Abandon,
};

// relevantKills counts kills of the objective's OWN source creatures during the stay (resolved
// through QuestObjectiveSourceEntriesFor). Killing the right creatures without the drop landing
// is trying and losing the drop roll, not proof the place cannot progress the quest; bystander
// kills never reach this parameter, so the old excuse-by-unrelated-combat hole stays closed.
//
// candidateSightings counts stay ticks on which a seek RETURNED a usable objective candidate
// (gameobject or use-target), whether or not the approach converged before it was contested
// away. A sighted candidate proves the place can progress the quest; losing the race for it is
// the contention class (Pierre, 2026-08-30), not grounds for the process-lifetime abandon mark.
[[nodiscard]] inline QuestStayEndVerdict QuestStayEndDecision(bool hasProgression, uint32 interactionAttempts,
                                                              uint32 relevantKills = 0,
                                                              uint32 candidateSightings = 0)
{
    if (hasProgression)
        return QuestStayEndVerdict::Progressed;
    if (interactionAttempts > 0 || relevantKills > 0 || candidateSightings > 0)
        return QuestStayEndVerdict::RotateWithoutBlame;
    return QuestStayEndVerdict::Abandon;
}

#endif
