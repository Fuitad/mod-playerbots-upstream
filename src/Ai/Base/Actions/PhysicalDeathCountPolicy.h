/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option), any later version.
 */

#ifndef PLAYERBOTS_PHYSICALDEATHCOUNTPOLICY_H
#define PLAYERBOTS_PHYSICALDEATHCOUNTPOLICY_H

#include <cstdint>

namespace playerbots::recovery
{
[[nodiscard]] constexpr std::uint32_t DeathCountAfterPaidRepair(std::uint32_t currentDeathCount)
{
    return currentDeathCount;
}

[[nodiscard]] constexpr std::uint32_t DeathCountAfterForcedRecovery(std::uint32_t currentDeathCount,
                                                                    bool recoveryAccepted)
{
    return recoveryAccepted ? 0u : currentDeathCount;
}
}  // namespace playerbots::recovery

#endif
