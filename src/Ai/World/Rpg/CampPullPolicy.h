/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE (camp-pull). Not present upstream, so it can never conflict on a merge.
 *
 * Where a bot's killer came from. The KILLEDBY line already says how the fight went, through the
 * killer's remaining health: measured at 200 bots on 2026-09-02, four deaths in ten had the killer
 * untouched, meaning the bot never landed a blow on the thing that killed it. That number stayed at
 * 30 to 36 percent after GrindCandidatePreferred started ranking on fewest hostile neighbours, and a
 * fresh window on 2026-09-03 read 22 of 119.
 *
 * The untouched share cannot say WHY, because it does not record what the bot was fighting when the
 * killer arrived. Two very different failures produce the same untouched killer: the bot pulled one
 * creature out of a camp and its neighbours joined, or something wandered in from elsewhere. Only
 * the first is a target-selection problem, and only the first is fixable by ranking candidates
 * differently. So the death has to carry the fight's FIRST target and the killer's distance from it.
 */

#ifndef _PLAYERBOT_CAMPPULLPOLICY_H
#define _PLAYERBOT_CAMPPULLPOLICY_H

#include "Define.h"

#include <ctime>

// The creature the bot engaged first in the current fight, with where it stood at that moment.
// Position is captured at engagement rather than read at death because the killer's own aggro
// reach is measured from where the pull happened, and a fleeing or moved creature would otherwise
// rewrite the fight's geometry after the fact.
struct FirstEngagement
{
    uint32 entry = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    // ObjectGuid::LowType, spelled as its underlying uint32 so this header stays free of core
    // includes and the policy test can build without a server. Verified at ObjectGuid.h:125.
    uint32 guidLow = 0;
    time_t since = 0;
};

// Where the killer came from, relative to the fight the bot actually started.
enum class KillerOrigin : uint8
{
    // No engagement was recorded: the bot died without this probe seeing it enter combat, which
    // covers falling, drowning and a death inside a fight that began before a restart.
    Unknown = 0,
    // The bot was killed by the creature it chose to fight. Target selection worked; the bot simply
    // lost. A high untouched share here would mean the bot cannot hurt what it picks.
    FirstTarget,
    // The killer stood within its own aggro reach of the first target when the fight began, so
    // pulling that first target brought this one along. This is the camp pull.
    CampNeighbour,
    // The killer was outside its own aggro reach of the first target: it arrived from elsewhere,
    // through a patrol, a wander, or the bot's own travel dragging the fight across a spawn.
    Wanderer,
};

[[nodiscard]] inline KillerOrigin ClassifyKiller(bool haveFirstEngagement, bool killerIsFirstTarget,
                                                 float killerToFirstTargetDistance, float killerAggroRadius)
{
    if (!haveFirstEngagement)
        return KillerOrigin::Unknown;
    if (killerIsFirstTarget)
        return KillerOrigin::FirstTarget;
    // Aggro reach is the killer's own, not a fixed radius: an elite and a critter of the same level
    // do not notice a pull from the same distance, and Creature::GetAggroRange already accounts for
    // the level difference and the server's aggro rate.
    return killerToFirstTargetDistance <= killerAggroRadius ? KillerOrigin::CampNeighbour
                                                            : KillerOrigin::Wanderer;
}

[[nodiscard]] inline char const* KillerOriginName(KillerOrigin origin)
{
    switch (origin)
    {
        case KillerOrigin::FirstTarget:
            return "firsttarget";
        case KillerOrigin::CampNeighbour:
            return "campneighbour";
        case KillerOrigin::Wanderer:
            return "wanderer";
        default:
            return "unknown";
    }
}

// A fight is the span between entering and leaving combat. Combat can be re-entered within the same
// engagement (a second attacker joins, the bot is knocked out of combat for a tick), so an
// engagement is replaced only when none is held; the leave hook clears it. Matches how
// CombatStuckPolicy treats a gap between ticks as a new span rather than trusting a single event.
[[nodiscard]] inline bool ShouldReplaceEngagement(FirstEngagement const& held, time_t now, time_t maxAgeSeconds)
{
    if (held.since == 0)
        return true;
    return now - held.since > maxAgeSeconds;
}

// A fight nobody ended. Without this a bot that leaves combat without the hook firing would carry a
// stale first target into its next death and report a nonsense distance.
inline constexpr time_t CAMP_PULL_ENGAGEMENT_MAX_AGE_SECONDS = 10 * 60;

#endif
