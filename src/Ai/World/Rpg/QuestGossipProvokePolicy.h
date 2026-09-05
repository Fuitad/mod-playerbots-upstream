/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE (quest-gossip-provoke). Not present upstream, so it can never conflict on a merge.
 *
 * Pure decisions for quests whose required item drops from a FRIENDLY creature that only turns
 * hostile once a player picks its gossip option. The Deathstalkers (14420): Astor Hadren walks
 * the Undercity road green and cons red the moment you ask "You're Astor Hadren, right?". The
 * Dwarven Spy (8483): Prospector Anvilward starts a walk on his gossip and sets faction 24 at
 * waypoint 7, well after the conversation. Six creatures on this realm share the pattern (6497,
 * 9543, 14523, 18585, 18586, 18588). The quest code could kill, loot, use objects and pickpocket
 * but never talk, so Valderotaux stood 7 yards from Astor on 2026-09-05 and walked away.
 *
 * The trigger is read from the creature's own SmartAI script: the first gossip-select event's
 * menu and option ids. It is fired through the creature's AI directly, which is what the core
 * does once the client has navigated the menus, so no menu navigation is emulated.
 */

#ifndef _PLAYERBOT_QUESTGOSSIPPROVOKEPOLICY_H
#define _PLAYERBOT_QUESTGOSSIPPROVOKEPOLICY_H

#include <optional>
#include <vector>

#include "Define.h"

// One gossip-select event out of a creature's SmartAI script.
struct QuestGossipSelectFact
{
    uint32 menu = 0;
    uint32 option = 0;
};

struct QuestGossipProvokeOption
{
    uint32 menu = 0;
    uint32 option = 0;
};

// The option to fire: the first gossip-select event in script order. Scripts with several
// (Anvilward has one; Astor has one on the submenu) list the provoking one first on this realm.
[[nodiscard]] inline std::optional<QuestGossipProvokeOption> FindQuestGossipProvokeOption(
    std::vector<QuestGossipSelectFact> const& gossipSelects)
{
    if (gossipSelects.empty())
        return std::nullopt;
    return QuestGossipProvokeOption{gossipSelects.front().menu, gossipSelects.front().option};
}

// How long after the conversation the bot keeps shadowing a still-friendly creature. Anvilward
// walks about 90 seconds with two scripted pauses (2.5 s and 15 s) before he turns; Astor turns
// at once. Past the wait the script did not take (someone else provoked him first, he evaded
// back to friendly, or the conversation was refused) and the stay goes on without him.
inline constexpr uint32 QUEST_GOSSIP_PROVOKE_WAIT_MS = 120 * 1000;
// Talking distance: the core's interaction range less a margin for a walker.
inline constexpr float QUEST_GOSSIP_TALK_DISTANCE = 5.0f;
// How close the bot stays while shadowing a provoked walker.
inline constexpr float QUEST_GOSSIP_SHADOW_DISTANCE = 8.0f;

enum class QuestGossipProvokeStep : uint8
{
    Unavailable = 0,  // dead, or provoked and the wait ran out: nothing to do with this creature
    Provoke,          // walk to talking distance and fire the gossip option
    Shadow,           // provoked, still friendly: keep close until the script turns him
    Fight,            // hostile now: the ordinary kill and loot path owns him
};

[[nodiscard]] inline QuestGossipProvokeStep NextQuestGossipProvokeStep(bool targetAlive, bool targetHostile,
                                                                        bool provoked, uint32 sinceProvokeMs)
{
    if (!targetAlive)
        return QuestGossipProvokeStep::Unavailable;
    if (targetHostile)
        return QuestGossipProvokeStep::Fight;
    if (!provoked)
        return QuestGossipProvokeStep::Provoke;
    return sinceProvokeMs < QUEST_GOSSIP_PROVOKE_WAIT_MS ? QuestGossipProvokeStep::Shadow
                                                          : QuestGossipProvokeStep::Unavailable;
}

#endif
