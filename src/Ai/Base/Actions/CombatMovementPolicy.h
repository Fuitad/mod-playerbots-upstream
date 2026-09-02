/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * Whether a fight may take the bot's legs away from the movement it was on.
 *
 * Forced travel (an economy gathering trip, any forced travel target) moves at MOVEMENT_FORCED,
 * the band above MOVEMENT_COMBAT. Upstream only stops a walk below the combat band when an attack
 * starts, and refuses a combat move while a higher one is in flight, so a random bot on a trip
 * keeps walking through the fight: the spline overwrites its facing every tick, "set facing"
 * (ACTION_MOVE + 7, above every attack) fires again and again, and neither a swing nor a cast
 * lands. Measured live 2026-09-01: Hemewmew, a level 11 shaman on a gathering trip, spent 12 of
 * the 20 seconds before her death in "set facing" and cast nothing until the spline ended; 45 of
 * 64 killers in the same window were within two levels of the bot and one was elite.
 *
 * MOVEMENT_FORCED is otherwise only issued by raid and dungeon mechanics, which a solo random bot
 * never runs, so for a bot without a game client master the fight wins. Everyone else keeps the
 * upstream rule.
 */

#ifndef PLAYERBOTS_COMBATMOVEMENTPOLICY_H
#define PLAYERBOTS_COMBATMOVEMENTPOLICY_H

#include "Ai/Base/Value/LastMovementValue.h"

namespace playerbots::combat
{
// An attack start stops the walk the bot is on. Upstream: only below the combat band.
[[nodiscard]] inline bool AttackStopsMovement(MovementPriority current, bool moving, bool controlled,
                                              bool soloRandomBot)
{
    if (!moving || controlled)
        return false;
    if (current < MovementPriority::MOVEMENT_COMBAT)
        return true;
    return soloRandomBot && current == MovementPriority::MOVEMENT_FORCED;
}

// A combat move may replace a forced walk for a solo random bot in combat. Upstream: never.
[[nodiscard]] inline bool CombatMoveOverridesForced(MovementPriority requested, MovementPriority current,
                                                    bool inCombat, bool soloRandomBot)
{
    return soloRandomBot && inCombat && requested == MovementPriority::MOVEMENT_COMBAT &&
           current == MovementPriority::MOVEMENT_FORCED;
}
}  // namespace playerbots::combat

#endif
