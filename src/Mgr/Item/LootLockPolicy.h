/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE (loot-open-lock). Not present upstream, so it can never conflict on a merge.
//
// One decision: whether a LOCK_KEY_SKILL lock row is openable by anyone, with no profession.
//
// LootObject::Refresh maps lock rows to a required skill through SkillByLockType, which answers
// SKILL_NONE for every "just open it" lock type (OPEN, TREASURE, QUICK_OPEN, OPEN_KNEELING,
// SLOW_OPEN). The old branch required a skill id greater than zero, so every quest plant and
// ground container using those locks - Cactus Apples, Moonpetal Lilies, Tirisfal Pumpkins, the
// Corrupted Flowers of Botanical Legwork - was silently treated as unlootable, and the upstream
// code carried a hardcoded exception for exactly two Serpentbloom gameobject ids instead of the
// class. Measured live 2026-08-29 on a fresh cohort: bots reached these POIs at 1 yard and
// abandoned with zero progress because the loot pipeline refused the container.
//
// Excluded on purpose: OPEN_TINKERING (engineering-gated, no SkillByLockType mapping to check the
// skill), OPEN_ATTACKING (opened by hitting it, which the loot pipeline cannot do), BLASTING
// (needs explosives), DISARM_TRAP/ARM_TRAP, the CLOSE family, GAHZRIDIAN and CALCIFIED (quest
// item gadgets), and OPEN_FROM_VEHICLE.

#ifndef _PLAYERBOT_LOOTLOCKPOLICY_H
#define _PLAYERBOT_LOOTLOCKPOLICY_H

#include "SharedDefines.h"

[[nodiscard]] inline bool LockRowOpensWithoutSkill(uint32 lockTypeIndex)
{
    switch (lockTypeIndex)
    {
        case LOCKTYPE_OPEN:
        case LOCKTYPE_TREASURE:
        case LOCKTYPE_QUICK_OPEN:
        case LOCKTYPE_OPEN_KNEELING:
        case LOCKTYPE_SLOW_OPEN:
            return true;
        default:
            return false;
    }
}

#endif
