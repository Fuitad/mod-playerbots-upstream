/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RANDOMPLAYERBOTADMISSION_H
#define PLAYERBOTS_RANDOMPLAYERBOTADMISSION_H

#include <cstdint>
#include <string>
#include <vector>

struct RandomPlayerbotAdmissionEvent
{
    std::string name;
    std::uint32_t value = 0;
    std::string data;
};

struct RandomPlayerbotAdmission
{
    std::uint32_t characterGuid = 0;
    std::vector<RandomPlayerbotAdmissionEvent> events;
};

[[nodiscard]] std::string ValidateRandomPlayerbotAdmissions(std::vector<RandomPlayerbotAdmission> const& admissions);
[[nodiscard]] bool ShouldClearRandomPlayerbotAdmissions(bool preserveAdmissions);
[[nodiscard]] std::uint32_t ResolveRandomPlayerbotAccountCount(bool preserveAdmissions,
                                                               std::uint32_t existingAccountCount,
                                                               std::uint32_t calculatedAccountCount);
[[nodiscard]] bool IsRandomPlayerbotAdmissionAccountType(std::uint8_t accountType);

#endif
