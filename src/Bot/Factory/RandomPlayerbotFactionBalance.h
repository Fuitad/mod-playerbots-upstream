/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#ifndef PLAYERBOTS_RANDOMPLAYERBOTFACTIONBALANCE_H
#define PLAYERBOTS_RANDOMPLAYERBOTFACTIONBALANCE_H

#include <array>
#include <vector>

#include "Define.h"
#include "SharedDefines.h"

inline constexpr uint32 RANDOM_PLAYERBOT_CHARACTERS_PER_ACCOUNT = 10;

constexpr uint32 RandomPlayerbotRemainingCharacterSlots(uint32 currentCount)
{
    return currentCount >= RANDOM_PLAYERBOT_CHARACTERS_PER_ACCOUNT
               ? 0
               : RANDOM_PLAYERBOT_CHARACTERS_PER_ACCOUNT - currentCount;
}

class RandomPlayerbotFactionBalance
{
public:
    void RecordExisting(bool alliance) { RecordCreated(alliance); }
    void RecordExisting(bool alliance, uint8 cls) { RecordCreated(alliance, cls); }
    void RecordCreated(bool alliance)
    {
        if (alliance)
            ++_allianceCount;
        else
            ++_hordeCount;
    }
    void RecordCreated(bool alliance, uint8 cls)
    {
        RecordCreated(alliance);
        if (cls > 0 && cls < MAX_CLASSES)
            ++ClassCountsFor(alliance)[cls];
    }

    [[nodiscard]] bool ShouldCreateAlliance() const { return _allianceCount <= _hordeCount; }
    [[nodiscard]] uint8 SelectLeastRepresentedClass(bool alliance, std::vector<uint8> const& availableClasses) const
    {
        std::array<uint32, MAX_CLASSES> const& counts = ClassCountsFor(alliance);
        uint8 selected = 0;
        for (uint8 cls : availableClasses)
        {
            if (cls == 0 || cls >= MAX_CLASSES)
                continue;
            if (selected == 0 || counts[cls] < counts[selected])
                selected = cls;
        }
        return selected;
    }

    [[nodiscard]] uint32 AllianceCount() const { return _allianceCount; }
    [[nodiscard]] uint32 HordeCount() const { return _hordeCount; }
    [[nodiscard]] uint32 ClassCount(bool alliance, uint8 cls) const
    {
        return cls < MAX_CLASSES ? ClassCountsFor(alliance)[cls] : 0;
    }

private:
    std::array<uint32, MAX_CLASSES>& ClassCountsFor(bool alliance)
    {
        return alliance ? _allianceClassCounts : _hordeClassCounts;
    }
    [[nodiscard]] std::array<uint32, MAX_CLASSES> const& ClassCountsFor(bool alliance) const
    {
        return alliance ? _allianceClassCounts : _hordeClassCounts;
    }

    uint32 _allianceCount = 0;
    uint32 _hordeCount = 0;
    std::array<uint32, MAX_CLASSES> _allianceClassCounts{};
    std::array<uint32, MAX_CLASSES> _hordeClassCounts{};
};

#endif
