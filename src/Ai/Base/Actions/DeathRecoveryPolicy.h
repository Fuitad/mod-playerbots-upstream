/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * Which recovery a masterless random bot takes after a death. A body revive puts the bot back
 * where it died; that is right for a lost fight it can win next time and wrong when the killer
 * outmatched it or when it has just died there. Measured 2026-09-02 01:35 to 01:47 at 160 bots:
 * Audacious (level 9) reclaimed her corpse four times beside a level 17 Greater Fleshripper and
 * died to it each time, Unconcerned (16) three times beside Humar the Pridelord (23), both idle
 * between the deaths. The manager's homebind recovery (alive, full health, no sickness) ends such
 * a chain at once.
 */

#ifndef _PLAYERBOT_DEATHRECOVERYPOLICY_H
#define _PLAYERBOT_DEATHRECOVERYPOLICY_H

#include <cstdint>
#include <unordered_map>

// Deaths closer together than this count as one chain.
inline constexpr uint32_t RECENT_DEATH_WINDOW_MS = 600000;
// A killer this many levels above the bot is orange or red to it: not a fight to walk back into.
inline constexpr int32_t OUTMATCHED_KILLER_LEVEL_GAP = 4;

struct RecentDeathRecord
{
    uint32_t deathsInWindow = 0;
    uint32_t lastDeathMs = 0;
    int32_t lastKillerLevelGap = 0;
};

inline bool RecoverAtHomebindAfterDeath(uint32_t deathsInWindow, int32_t killerLevelGap)
{
    return deathsInWindow >= 2 || killerLevelGap >= OUTMATCHED_KILLER_LEVEL_GAP;
}

inline RecentDeathRecord NoteRecentDeath(RecentDeathRecord previous, uint32_t nowMs, int32_t killerLevelGap)
{
    RecentDeathRecord record;
    bool const inWindow = previous.deathsInWindow > 0 && nowMs - previous.lastDeathMs <= RECENT_DEATH_WINDOW_MS;
    record.deathsInWindow = inWindow ? previous.deathsInWindow + 1 : 1;
    record.lastDeathMs = nowMs;
    record.lastKillerLevelGap = killerLevelGap;
    return record;
}

// The vertical cap sets a ghost standing far above or below its corpse onto it. A ghost that ends
// up off the vertical a second time on the same corpse was pulled away again by whatever moved it
// the first time (Dorothe, 2026-09-02 06:28: ten caps in nine minutes over the Ban'ethil Barrow Den,
// the corpse camped 150 yards under the surface, no reclaim between them). Setting it down again
// repeats the loop; the manager's homebind recovery ends it.
inline constexpr uint32_t VERTICAL_CAPS_PER_CORPSE = 1;

struct VerticalCapRecord
{
    uint64_t corpseGhostTime = 0;
    uint32_t caps = 0;
};

inline VerticalCapRecord NoteVerticalCap(VerticalCapRecord previous, uint64_t corpseGhostTime)
{
    VerticalCapRecord record;
    record.corpseGhostTime = corpseGhostTime;
    record.caps = previous.corpseGhostTime == corpseGhostTime ? previous.caps + 1 : 1;
    return record;
}

inline bool RecoverAtHomebindAfterVerticalCap(uint32_t capsOnThisCorpse)
{
    return capsOnThisCorpse > VERTICAL_CAPS_PER_CORPSE;
}

namespace VerticalCaps
{
inline std::unordered_map<uint32_t, VerticalCapRecord>& Registry()
{
    static std::unordered_map<uint32_t, VerticalCapRecord> registry;
    return registry;
}

inline VerticalCapRecord Note(uint32_t botGuidLow, uint64_t corpseGhostTime)
{
    VerticalCapRecord const record = NoteVerticalCap(Registry()[botGuidLow], corpseGhostTime);
    Registry()[botGuidLow] = record;
    return record;
}
}  // namespace VerticalCaps

namespace RecentDeaths
{
inline std::unordered_map<uint32_t, RecentDeathRecord>& Registry()
{
    static std::unordered_map<uint32_t, RecentDeathRecord> registry;
    return registry;
}

inline RecentDeathRecord Note(uint32_t botGuidLow, uint32_t nowMs, int32_t killerLevelGap)
{
    RecentDeathRecord const record = NoteRecentDeath(Registry()[botGuidLow], nowMs, killerLevelGap);
    Registry()[botGuidLow] = record;
    return record;
}

// The record of the bot's current chain, or an empty one once the window has passed.
inline RecentDeathRecord Current(uint32_t botGuidLow, uint32_t nowMs)
{
    auto const it = Registry().find(botGuidLow);
    if (it == Registry().end() || nowMs - it->second.lastDeathMs > RECENT_DEATH_WINDOW_MS)
        return RecentDeathRecord{};
    return it->second;
}
}  // namespace RecentDeaths

#endif
