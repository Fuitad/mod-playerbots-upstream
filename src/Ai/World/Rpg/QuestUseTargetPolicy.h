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

// The creature entries a use-seek accepts as the objective target. Some quests count a credit
// DUMMY that never stands in the world: Inoculation (9303) requires entry 16534 "Inoculated
// Nestlewood Owlkin", granted by the crystal's spell, while the creatures actually spawned are
// entry 16518. Matching only the required entry finds nothing forever (measured live 2026-08-30:
// three bots at the POI with owlkin around them, zero candidates, blamed abandons). The tool
// spell's target-condition entries name the real-world creature, so both are accepted.
[[nodiscard]] inline std::vector<uint32> QuestUseAcceptedEntries(int32 requiredEntry,
                                                                std::vector<uint32> const& toolSpellTargetEntries)
{
    std::vector<uint32> accepted;
    if (requiredEntry > 0)
        accepted.push_back(static_cast<uint32>(requiredEntry));
    for (uint32 entry : toolSpellTargetEntries)
    {
        if (!entry)
            continue;
        bool known = false;
        for (uint32 have : accepted)
            if (have == entry)
            {
                known = true;
                break;
            }
        if (!known)
            accepted.push_back(entry);
    }
    return accepted;
}

// One nearby creature, reduced to the facts that decide whether to walk to it.
struct QuestUseCandidateFacts
{
    bool matchesEntry = false;
    bool alive = true;
    // Some use-credited scripts only accept a sleeping target: the Foreman's Blackjack wakes a
    // Lazy Peon and credits nothing on one already working. Measured live 2026-08-29: 15 whacks
    // on awake peons, zero credits. A sleeping candidate outranks any awake one; when nothing
    // sleeps (Inoculation owlkin never do), the awake nearest still wins, so scripts without a
    // sleep requirement are unaffected.
    bool sleeping = false;
    float distanceSq = 0.0f;
    float anchorDistanceSq = 0.0f;
};

inline constexpr size_t QUEST_USE_NO_CANDIDATE = ~static_cast<size_t>(0);

// A candidate is in range when it sits within the cap of the POI anchor OR of the bot itself.
// The anchor-only cap filtered a live owlkin standing beside a bot that had drifted 45y chasing
// combat (Abaka, measured live 2026-08-30: usecand 6/1/1 yet zero engagements in a 316s stay).
// The stay latch (lastReachPOI) keeps the POI approach disabled while this seek runs, so
// accepting a bot-near candidate cannot fight the approach radius.
[[nodiscard]] inline bool QuestUseCandidateInRange(QuestUseCandidateFacts const& candidate, float maxDistanceSq)
{
    return maxDistanceSq <= 0.0f || candidate.anchorDistanceSq <= maxDistanceSq ||
           candidate.distanceSq <= maxDistanceSq;
}

// Nearest living creature of the objective entry within range (see QuestUseCandidateInRange).
// A sleeping candidate outranks any awake one; distance breaks ties within each group.
[[nodiscard]] inline size_t BestQuestUseTargetIndex(std::vector<QuestUseCandidateFacts> const& candidates,
                                                    float maxAnchorDistanceSq)
{
    size_t best = QUEST_USE_NO_CANDIDATE;
    float bestDistanceSq = -1.0f;
    size_t bestSleeping = QUEST_USE_NO_CANDIDATE;
    float bestSleepingDistanceSq = -1.0f;
    for (size_t i = 0; i < candidates.size(); ++i)
    {
        QuestUseCandidateFacts const& candidate = candidates[i];
        if (!candidate.matchesEntry || !candidate.alive)
            continue;
        if (!QuestUseCandidateInRange(candidate, maxAnchorDistanceSq))
            continue;
        if (bestDistanceSq < 0.0f || candidate.distanceSq < bestDistanceSq)
        {
            bestDistanceSq = candidate.distanceSq;
            best = i;
        }
        if (candidate.sleeping && (bestSleepingDistanceSq < 0.0f || candidate.distanceSq < bestSleepingDistanceSq))
        {
            bestSleepingDistanceSq = candidate.distanceSq;
            bestSleeping = i;
        }
    }
    return bestSleeping != QUEST_USE_NO_CANDIDATE ? bestSleeping : best;
}

#endif
