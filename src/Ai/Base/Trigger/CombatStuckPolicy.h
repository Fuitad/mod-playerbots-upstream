/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * How long the current fight has lasted, measured by the combat engine's own ticks. Upstream read
 * the age of the "combat::self target" memory value instead, but that value is only sampled while
 * the bot fights (the trigger returns early out of combat and the combat engine does not run then),
 * so it never records the bot leaving combat, its change time freezes at the bot's first fight, and
 * five minutes later every fight reads as "stuck" on every five-second check. Measured 2026-09-02
 * 07:40 at 200 bots: "reset botAI" pairs 5.5 seconds apart inside ordinary fights (bots 977, 1057,
 * 897), each reset dropping the travel strategy and the economy's forced gathering trip with it
 * (45 of 58 named material releases in fourteen minutes said the destination was gone).
 */

#ifndef _PLAYERBOT_COMBATSTUCKPOLICY_H
#define _PLAYERBOT_COMBATSTUCKPOLICY_H

#include <cstdint>
#include <ctime>

inline constexpr uint32_t COMBAT_STUCK_SECONDS = 5 * 60;
inline constexpr uint32_t COMBAT_LONG_STUCK_SECONDS = 15 * 60;
// The trigger is checked every five seconds while the bot fights. A longer gap between two ticks
// means the fight ended in between, and the next tick starts a new span.
inline constexpr uint32_t COMBAT_TICK_GAP_SECONDS = 15;

struct CombatSpan
{
    time_t since = 0;
    time_t lastTick = 0;
};

inline CombatSpan NoteCombatTick(CombatSpan previous, time_t now)
{
    if (previous.lastTick == 0 || now - previous.lastTick > static_cast<time_t>(COMBAT_TICK_GAP_SECONDS))
        return CombatSpan{now, now};
    return CombatSpan{previous.since, now};
}

inline bool CombatStuckFor(CombatSpan span, time_t now, uint32_t seconds)
{
    return span.since != 0 && now - span.since > static_cast<time_t>(seconds);
}

#endif
