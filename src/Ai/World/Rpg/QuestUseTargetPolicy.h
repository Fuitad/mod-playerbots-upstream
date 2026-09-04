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

#include <array>
#include <cstddef>
#include <vector>

#include "Define.h"

// How the objective creature is operated.
enum class QuestUseMode : uint8
{
    None = 0,    // no tool: a genuine kill quest, leave it to the grind strategy
    Item,        // use the quest's provided source item on the creature
    Spell,       // cast a known racial/quest spell on the creature
    PickPocket,  // enter stealth and cast Pick Pocket on a living required-item source
};

[[nodiscard]] inline char const* QuestUseModeName(QuestUseMode mode)
{
    switch (mode)
    {
        case QuestUseMode::Item:
            return "item";
        case QuestUseMode::Spell:
            return "spell";
        case QuestUseMode::PickPocket:
            return "pickpocket";
        default:
            return "none";
    }
}

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
        case 9489:  // Cleansing the Scar - Power Word: Fortitude on an Eversong Ranger
            // smart_scripts rows 3 to 10 on creature 15938 each credit the quest on spell hit of
            // one Power Word: Fortitude rank, so any rank the bot knows counts. The credit is
            // No Repeat per ranger and the quest needs six, which is what UsedThisStay is for.
            return {1243, 1244, 1245, 2791, 10937, 10938, 25389, 48161};
        // Priest garment quests: npc_garments_of_quests credits Lesser Heal rank 2 (2052) on the
        // wounded guard, then Power Word: Fortitude rank 1 (1243) once healed, in that order. The
        // list is a SEQUENCE for these, see QuestUseSpellForAttempt. Measured live 2026-08-31 and
        // 2026-09-01: three abandons and gray-drops with usemode 0 before the table knew them.
        case 5621:  // Garments of the Moon (night elf)
        case 5624:  // Garments of the Light (human)
        case 5625:  // Garments of the Light (dwarf)
        case 5648:  // Garments of Spirituality (troll)
        case 5650:  // Garments of Darkness (undead)
            return {2052, 1243};
        default:
            return {};
    }
}

// Whether the quest's credit script fires once per creature, so a second use on the same one is
// wasted and the seek has to move on. Deliberately a named list rather than a heuristic: the
// garment quests (5621 and friends) are the counter-example, they need Lesser Heal and then Power
// Word: Fortitude on the SAME wounded guard, and skipping the guard after the first cast would
// strand them. Only quests whose smart_scripts credit row carries No Repeat belong here.
[[nodiscard]] inline bool QuestUseCreditsOncePerCreature(uint32 questId)
{
    // 9489 Cleansing the Scar: creature 15938 rows 3 to 10, "Quest Credit (No Repeat)", six needed.
    return questId == 9489;
}

// Which of the bot's known quest spells to cast on this attempt. Single-spell quests always cast
// their one spell; multi-spell quests walk the list in order so a credit script that wants spell
// A then spell B sees both, and keep cycling so a lost cast is retried on the next pass.
[[nodiscard]] inline uint32 QuestUseSpellForAttempt(std::vector<uint32> const& knownSpells, uint32 attempt)
{
    if (knownSpells.empty())
        return 0;
    return knownSpells[attempt % knownSpells.size()];
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
    // A creature this stay has already been cast on. Scripts that credit No Repeat per creature
    // give nothing for a second cast: Cleansing the Scar (9489) needs six DISTINCT Eversong
    // Rangers, and the bot stands next to the one it just buffed, so the nearest candidate is
    // exactly the wrong one from the second cast onwards.
    bool usedThisStay = false;
    float distanceSq = 0.0f;
    float anchorDistanceSq = 0.0f;
};

inline constexpr size_t QUEST_USE_NO_CANDIDATE = ~static_cast<size_t>(0);

// How close the bot walks before using the tool. Arm's length (INTERACTION_DISTANCE, 5.5 yards)
// was the rule for every tool, and a wandering owlkin rarely stays inside it: Yuhelmeric,
// 2026-09-02 03:45, antidote in the bag, ten candidates, one use in 310 seconds. A tool with a
// range is used from a yard inside it, capped so a long-range tool is not fired across the camp.
inline constexpr float QUEST_USE_ARMS_LENGTH = 5.5f;
inline constexpr float QUEST_USE_MAX_ENGAGE_DISTANCE = 30.0f;

[[nodiscard]] inline float QuestUseEngageDistance(float toolMaxRange)
{
    float const fromRange = toolMaxRange - 1.0f;
    if (fromRange <= QUEST_USE_ARMS_LENGTH)
        return QUEST_USE_ARMS_LENGTH;
    return fromRange < QUEST_USE_MAX_ENGAGE_DISTANCE ? fromRange : QUEST_USE_MAX_ENGAGE_DISTANCE;
}

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
// A sleeping candidate outranks any awake one; distance breaks ties within each group. When the
// tool only credits a sleeper (the Foreman's Blackjack's spell carries the sleep aura as its
// target condition), an awake candidate is no candidate: Lazy Peons, 2026-09-02 05:56, 2,558
// whacks across seven stays of 100 to 133 attempts each and one turn-in, all on awake peons.
[[nodiscard]] inline size_t BestQuestUseTargetIndexAmong(std::vector<QuestUseCandidateFacts> const& candidates,
                                                         float maxAnchorDistanceSq, bool requiresSleeping,
                                                         bool skipUsed)
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
        if (requiresSleeping && !candidate.sleeping)
            continue;
        if (skipUsed && candidate.usedThisStay)
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

// A creature already cast on this stay is taken only when no fresh one qualifies, so a cast that
// was lost (out of range, interrupted, resisted) is still retried rather than stranding the stay.
[[nodiscard]] inline size_t BestQuestUseTargetIndex(std::vector<QuestUseCandidateFacts> const& candidates,
                                                    float maxAnchorDistanceSq, bool requiresSleeping = false,
                                                    bool oncePerCreature = false)
{
    if (oncePerCreature)
    {
        size_t const fresh = BestQuestUseTargetIndexAmong(candidates, maxAnchorDistanceSq, requiresSleeping, true);
        if (fresh != QUEST_USE_NO_CANDIDATE)
            return fresh;
    }
    return BestQuestUseTargetIndexAmong(candidates, maxAnchorDistanceSq, requiresSleeping, false);
}

#endif
