/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/* PLB-LOCAL FILE. Local diagnostic state shared by map update threads. */

#ifndef _PLAYERBOT_DEATHPROBESTATE_H
#define _PLAYERBOT_DEATHPROBESTATE_H

#include <mutex>
#include <optional>
#include <unordered_map>

#include "CampPullPolicy.h"

// Return values, never references or iterators. No player, AI, logging or other ledger call runs
// while this state is locked; combat hooks can originate from different map update workers.
class DeathProbeState
{
public:
    void NoteKillerGap(uint32 bot, int32 gap)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _killerGaps[bot] = gap;
    }

    [[nodiscard]] std::optional<int32> TakeKillerGap(uint32 bot)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto const found = _killerGaps.find(bot);
        if (found == _killerGaps.end())
            return std::nullopt;
        int32 const gap = found->second;
        _killerGaps.erase(found);
        return gap;
    }

    void NoteEngagement(uint32 bot, FirstEngagement const& incoming)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        FirstEngagement& held = _engagements[bot];
        if (ShouldReplaceEngagement(held, incoming.since, CAMP_PULL_ENGAGEMENT_MAX_AGE_SECONDS))
            held = incoming;
    }

    [[nodiscard]] std::optional<FirstEngagement> TakeEngagement(uint32 bot)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto const found = _engagements.find(bot);
        if (found == _engagements.end())
            return std::nullopt;
        FirstEngagement const held = found->second;
        _engagements.erase(found);
        return held;
    }

    void ClearEngagement(uint32 bot)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _engagements.erase(bot);
    }

private:
    std::mutex _mutex;
    std::unordered_map<uint32, int32> _killerGaps;
    std::unordered_map<uint32, FirstEngagement> _engagements;
};

#endif
