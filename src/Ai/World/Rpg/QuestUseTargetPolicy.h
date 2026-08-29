/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE (quest-use-target). Not present upstream, so it can never conflict on a merge.
 *
 * Pure decisions for quests whose creature objective is credited by USING something on the
 * creature rather than killing it: wake a Lazy Peon with the Foreman's Blackjack, inoculate a
 * Nestlewood Owlkin, Mana Tap an Arcane Wraith, cast Gift of the Naaru on a Draenei Survivor.
 * The grind strategy can only kill, so these quests burned their whole POI stay with heavy
 * combat and zero credit (measured live 2026-08-29: bots abandoned them with 3 to 21 kills and
 * the objective counter at 0).
 */

#ifndef _PLAYERBOT_QUESTUSETARGETPOLICY_H
#define _PLAYERBOT_QUESTUSETARGETPOLICY_H

#include "Define.h"

#include <array>
#include <cstddef>
#include <vector>

// How the objective creature is operated.
enum class QuestUseMode : uint8
{
    None = 0,  // no tool: a genuine kill quest, leave it to the grind strategy
    Item,      // use the quest's provided source item on the creature
    Spell,     // cast a known racial/quest spell on the creature
};

// The quest's provided item is the tool only when it actually has an on-use spell; a readable
// letter or a plain starter item does not qualify, and without a tool the objective is a kill.
[[nodiscard]] inline QuestUseMode QuestUseModeForFacts(bool holdsSourceItemWithUseSpell, bool knowsQuestSpell)
{
    if (holdsSourceItemWithUseSpell)
        return QuestUseMode::Item;
    if (knowsQuestSpell)
        return QuestUseMode::Spell;
    return QuestUseMode::None;
}

// Spell-credited quests carry no tool item, so the spell has to be known here. The ids are the
// per-class variants; the bot casts the first one it knows. An id the bot does not know is
// filtered by Player::HasSpell at the call site, so an over-broad list is harmless while a
// missing id only means that class skips the quest, exactly as it does today.
[[nodiscard]] inline std::vector<uint32> QuestUseSpellsForQuest(uint32 questId)
{
    switch (questId)
    {
        case 9283:  // Rescue the Survivors! - Gift of the Naaru, one spell id per class
            // The full family as found in this server's own Spell.dbc (8 ids).
            return {28880, 57901, 59542, 59543, 59544, 59545, 59547, 59548};
        case 8346:  // Thirst Unending - Mana Tap
            return {28734};
        default:
            return {};
    }
}

// One nearby creature, reduced to the facts that decide whether to walk to it.
struct QuestUseCandidateFacts
{
    bool matchesEntry = false;
    bool alive = true;
    float distanceSq = 0.0f;
    float anchorDistanceSq = 0.0f;
};

inline constexpr size_t QUEST_USE_NO_CANDIDATE = ~static_cast<size_t>(0);

// Nearest living creature of the objective entry within the POI anchor radius, mirroring
// BestQuestGoCandidateIndex so this seek can never fight the quest-poi-approach return radius.
[[nodiscard]] inline size_t BestQuestUseTargetIndex(std::vector<QuestUseCandidateFacts> const& candidates,
                                                    float maxAnchorDistanceSq)
{
    size_t best = QUEST_USE_NO_CANDIDATE;
    float bestDistanceSq = -1.0f;
    for (size_t i = 0; i < candidates.size(); ++i)
    {
        QuestUseCandidateFacts const& candidate = candidates[i];
        if (!candidate.matchesEntry || !candidate.alive)
            continue;
        if (maxAnchorDistanceSq > 0.0f && candidate.anchorDistanceSq > maxAnchorDistanceSq)
            continue;
        if (bestDistanceSq < 0.0f || candidate.distanceSq < bestDistanceSq)
        {
            bestDistanceSq = candidate.distanceSq;
            best = i;
        }
    }
    return best;
}

#endif
