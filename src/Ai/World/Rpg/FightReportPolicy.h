/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE (fight-report). Not present upstream, so it can never conflict on a merge.
 *
 * What a bot's last fight looked like, reported at its death. The death probe already says who
 * killed the bot and from where; it could not say whether the bot ever fought back. On 2026-09-05
 * about 60 percent of deaths were to mobs at or below the bot's level, a level 14 warrior sat 155
 * seconds in combat with every action failing on every tick, and the earlier "power" reading was
 * an artifact (the core zeroes power before the death hook runs). Pierre: fight report.
 *
 * The ledger is filled from three places: damage dealt and taken (UnitScript::OnDamage), action
 * results while in combat (the engine's action dispatch), and the combat enter and leave hooks
 * that open and close it. The verdict separates a fight the bot lost from a fight it never had.
 */

#ifndef _PLAYERBOT_FIGHTREPORTPOLICY_H
#define _PLAYERBOT_FIGHTREPORTPOLICY_H

#include <string>
#include <unordered_map>

#include "Define.h"

struct FightLedger
{
    // getMSTime() when the bot entered combat; zero when no fight is open.
    uint32 startMs = 0;
    // Health percent when the fight opened. The first live lines on 2026-09-05 13:21 showed fights
    // of 5 to 15 seconds with 60 to 200 damage dealt: a bot that starts a fight at a third of its
    // health loses it before its actions matter.
    uint32 startHealthPct = 0;
    // Landed hits and their damage, and damage taken, since the fight opened.
    uint32 hits = 0;
    uint32 dealt = 0;
    uint32 taken = 0;
    // Combat-time action results and the name that failed most.
    uint32 actionsOk = 0;
    uint32 actionsFailed = 0;
    std::unordered_map<std::string, uint32> failures;
};

enum class FightVerdict : uint8
{
    NoContact = 0,  // never landed a hit: the bot did not fight, whatever it tried
    Outdamaged,     // hit back, but took three times what it dealt
    LostExchange,   // a fight it could have won
};

[[nodiscard]] inline char const* FightVerdictName(FightVerdict verdict)
{
    switch (verdict)
    {
        case FightVerdict::NoContact:
            return "nocontact";
        case FightVerdict::Outdamaged:
            return "outdamaged";
        default:
            return "lostexchange";
    }
}

inline constexpr uint32 FIGHT_OUTDAMAGED_RATIO = 3;

[[nodiscard]] inline FightVerdict ClassifyFight(FightLedger const& ledger)
{
    if (ledger.hits == 0)
        return FightVerdict::NoContact;
    if (ledger.taken >= ledger.dealt * FIGHT_OUTDAMAGED_RATIO)
        return FightVerdict::Outdamaged;
    return FightVerdict::LostExchange;
}

inline void NoteFightDamageDealt(FightLedger& ledger, uint32 damage)
{
    if (!ledger.startMs)
        return;
    ++ledger.hits;
    ledger.dealt += damage;
}

inline void NoteFightDamageTaken(FightLedger& ledger, uint32 damage)
{
    if (!ledger.startMs)
        return;
    ledger.taken += damage;
}

inline void NoteFightAction(FightLedger& ledger, std::string const& action, bool success)
{
    if (!ledger.startMs)
        return;
    if (success)
        ++ledger.actionsOk;
    else
    {
        ++ledger.actionsFailed;
        ++ledger.failures[action];
    }
}

// The action that failed most often this fight, or "none".
[[nodiscard]] inline std::string TopFightFailure(FightLedger const& ledger)
{
    std::string top = "none";
    uint32 best = 0;
    for (auto const& [name, count] : ledger.failures)
        if (count > best)
        {
            best = count;
            top = name;
        }
    return top;
}

#endif
