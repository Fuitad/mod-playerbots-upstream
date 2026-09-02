/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#ifndef PLAYERBOTS_MAINTENANCEERRANDPOLICY_H
#define PLAYERBOTS_MAINTENANCEERRANDPOLICY_H

#include <cstdint>

namespace playerbots::maintenance
{
/*
 * A maintenance errand (walking to a repairer or a vendor) and the RPG loop both steer the bot with
 * MoveFarTo, and the loop runs every tick while the errand's trigger fires every five seconds. The
 * loop's move is always the one in flight, so the errand's MoveFarTo returns false and the bot walks
 * the loop's way: measured live 2026-09-01, Ensetsu's repairer went from 35 to 334 yards away over
 * three errand ticks before the target was dropped as stale, and six bots stood in zero-durability
 * gear at once. While an errand is claimed, every other movement request yields to it.
 *
 * The claim carries a lease: an errand whose owner stopped refreshing it (the action no longer runs
 * because the need vanished, the bot died, or the trigger stopped firing) must not freeze the loop.
 */
inline constexpr std::uint32_t MAINTENANCE_ERRAND_LEASE_MS = 30000;

// True when a movement request must yield: an errand is claimed, its lease is still live, and the
// request is not the errand's own destination.
[[nodiscard]] inline bool ErrandBlocksOtherMove(bool hasErrand, std::uint32_t errandAgeMs, bool sameDestination)
{
    return hasErrand && errandAgeMs < MAINTENANCE_ERRAND_LEASE_MS && !sameDestination;
}
}  // namespace playerbots::maintenance

#endif
