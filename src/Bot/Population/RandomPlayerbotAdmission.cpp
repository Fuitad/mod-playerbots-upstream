/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#include "Bot/Population/RandomPlayerbotAdmission.h"

#include <set>

std::string ValidateRandomPlayerbotAdmissions(std::vector<RandomPlayerbotAdmission> const& admissions)
{
    if (admissions.empty())
        return "empty_admissions";
    std::set<std::uint32_t> guids;
    for (auto const& admission : admissions)
    {
        if (!admission.characterGuid)
            return "invalid_guid";
        if (!guids.insert(admission.characterGuid).second)
            return "duplicate_guid";
        if (admission.events.empty())
            return "empty_events";
        std::set<std::string> eventNames;
        for (auto const& event : admission.events)
        {
            if (event.name.empty())
                return "empty_event_name";
            if (event.name == "add" || event.name == "logout")
                return "reserved_event";
            if (!eventNames.insert(event.name).second)
                return "duplicate_event";
            if (!event.value)
                return "zero_event_value";
        }
    }
    return {};
}

bool ShouldClearRandomPlayerbotAdmissions(bool preserveAdmissions) { return !preserveAdmissions; }

std::uint32_t ResolveRandomPlayerbotAccountCount(bool preserveAdmissions, std::uint32_t existingAccountCount,
                                                 std::uint32_t calculatedAccountCount)
{
    return preserveAdmissions ? existingAccountCount : calculatedAccountCount;
}

bool IsRandomPlayerbotAdmissionAccountType(std::uint8_t accountType) { return accountType == 0 || accountType == 1; }

std::vector<std::uint32_t> DiscoverAndHydrateRandomPlayerbotAccountCache(
    std::vector<std::uint32_t>& accountCache, std::function<std::vector<std::uint32_t>()> const& discoverAccountIds)
{
    std::vector<std::uint32_t> const discoveredAccountIds = discoverAccountIds();
    accountCache = discoveredAccountIds;
    return discoveredAccountIds;
}
