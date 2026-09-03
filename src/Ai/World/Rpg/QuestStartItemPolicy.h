/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE (quest-start-item). Not present upstream, so it can never conflict on a merge.
 *
 * Whether a bot should use an item in its bags that starts a quest. Items with a StartQuest are
 * deliberately looted (IsLootAllowed returns true for them in LootAction.cpp), and nothing has ever
 * used one: the only acceptance path in the module is RpgStartQuestAction, which talks to an NPC.
 * So they accumulate. Measured live 2026-09-03 at 200 bots: 129 such items held by 73 bots, one bot
 * carrying four, while 159 of 200 bots were over the 80% bag line. Each one is a quest that never
 * happens and a bag slot that never comes back.
 *
 * Pierre, 2026-09-03: a bot should use one the moment it gets it, unless its quest log is full.
 */

#ifndef _PLAYERBOT_QUESTSTARTITEMPOLICY_H
#define _PLAYERBOT_QUESTSTARTITEMPOLICY_H

#include "Define.h"

// One held item that starts a quest, reduced to the facts that decide whether to use it.
struct QuestStartItemFacts
{
    // The item's StartQuest resolves to a real quest template. A dangling id is data damage and
    // using the item would do nothing.
    bool questExists = false;
    // Player::CanTakeQuest: level, race, class, prerequisites, exclusivity, and not already taken
    // or rewarded. The one call that already encodes every acceptance rule, so the policy does not
    // restate any of them.
    bool canTakeQuest = false;
    // Player::CanAddQuest: room in the quest log, and room for whatever the quest hands over.
    bool canAddQuest = false;
    // Already in the log or already rewarded. CanTakeQuest covers both, kept separate so the
    // caller can log WHY an item is being carried rather than only that it is.
    bool alreadyOnQuest = false;
    bool alreadyRewarded = false;
};

enum class QuestStartItemVerdict : uint8
{
    Use = 0,
    // The quest log is full. Pierre's one exception: the bot keeps the item and uses it later.
    LogFull,
    // Level, race, class or a prerequisite. The bot may qualify later, so the item is kept.
    NotEligibleYet,
    // Already taken or already rewarded. The item is dead weight and nothing here will use it.
    Redundant,
    // StartQuest points at nothing.
    NoSuchQuest,
};

[[nodiscard]] inline QuestStartItemVerdict QuestStartItemDecision(QuestStartItemFacts const& facts)
{
    if (!facts.questExists)
        return QuestStartItemVerdict::NoSuchQuest;
    if (facts.alreadyOnQuest || facts.alreadyRewarded)
        return QuestStartItemVerdict::Redundant;
    if (!facts.canTakeQuest)
        return QuestStartItemVerdict::NotEligibleYet;
    // Checked after eligibility so a full log is only reported for a quest the bot could take.
    if (!facts.canAddQuest)
        return QuestStartItemVerdict::LogFull;
    return QuestStartItemVerdict::Use;
}

[[nodiscard]] inline char const* QuestStartItemVerdictName(QuestStartItemVerdict verdict)
{
    switch (verdict)
    {
        case QuestStartItemVerdict::Use:
            return "use";
        case QuestStartItemVerdict::LogFull:
            return "logfull";
        case QuestStartItemVerdict::NotEligibleYet:
            return "noteligible";
        case QuestStartItemVerdict::Redundant:
            return "redundant";
        default:
            return "nosuchquest";
    }
}

#endif
