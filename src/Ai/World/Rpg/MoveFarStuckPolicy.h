/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * PLB-LOCAL(movefar-stuck): local-only file, no upstream counterpart.
 *
 * Destination-independent stuck detection for MoveFarTo.
 *
 * MoveFarTo already tracks progress toward a single destination, but that
 * tracker is reset whenever the caller passes a different destination. Several
 * subsystems share it: quest travel passes a quest POI, random bot maintenance
 * passes a vendor. A bot that both want to move alternates destinations every
 * tick, so the per-destination counter never reaches its threshold and the
 * teleport recovery never fires. Observed on a live realm: one bot logged 1517
 * quest pathing failures and 490 vendor pathing failures over fifty minutes
 * while standing still, and was rescued zero times.
 *
 * This policy watches the bot's own displacement instead. If something keeps
 * asking the bot to travel far and the bot has not physically moved for the
 * rescue window, it is stuck regardless of which destination was last asked
 * for.
 */

#ifndef _PLAYERBOT_MOVEFARSTUCKPOLICY_H
#define _PLAYERBOT_MOVEFARSTUCKPOLICY_H

#include "Define.h"

// What the caller knows at the moment MoveFarTo is entered.
struct MoveFarStuckFacts
{
    // False before the first sample is taken, and after any reset.
    bool tracking = false;
    // False when the bot changed map since the sample was taken; a cross-map
    // displacement is not comparable, so it can only mean "resample".
    bool sameMap = true;
    // How far the bot is now from where the sample was taken.
    float displacementYards = 0.0f;
    // How long ago the sample was taken.
    uint32 elapsedMs = 0;
    // Movement that counts as real progress and restarts the window.
    float resetRadius = 5.0f;
    // How long the bot may stay put before it is rescued.
    uint32 rescueAfterMs = 120 * 1000;
};

enum class MoveFarStuckVerdict
{
    // Take a fresh sample here: nothing is being tracked yet, or the bot moved.
    Resample,
    // The bot has not moved, but not for long enough to act on.
    Wait,
    // The bot has been asked to travel far and has not moved. Rescue it.
    Rescue,
};

[[nodiscard]] inline MoveFarStuckVerdict EvaluateMoveFarStuck(MoveFarStuckFacts const& facts)
{
    if (!facts.tracking || !facts.sameMap)
        return MoveFarStuckVerdict::Resample;

    if (facts.displacementYards > facts.resetRadius)
        return MoveFarStuckVerdict::Resample;

    if (facts.elapsedMs >= facts.rescueAfterMs)
        return MoveFarStuckVerdict::Rescue;

    return MoveFarStuckVerdict::Wait;
}

#endif
