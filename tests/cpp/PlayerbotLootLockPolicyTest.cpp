/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include "Mgr/Item/LootLockPolicy.h"
#include "gtest/gtest.h"

TEST(PlayerbotLootLockPolicyTest, EveryAnyoneCanOpenLockTypeIsAccepted)
{
    // The defect this pins: quest plants and ground containers (Cactus Apple, Moonpetal Lily,
    // Tirisfal Pumpkin, Corrupted Flower) use these lock types, map to SKILL_NONE, and were
    // refused by the loot pipeline, so their quests could never progress.
    EXPECT_TRUE(LockRowOpensWithoutSkill(LOCKTYPE_OPEN));
    EXPECT_TRUE(LockRowOpensWithoutSkill(LOCKTYPE_TREASURE));
    EXPECT_TRUE(LockRowOpensWithoutSkill(LOCKTYPE_QUICK_OPEN));
    EXPECT_TRUE(LockRowOpensWithoutSkill(LOCKTYPE_OPEN_KNEELING));
    EXPECT_TRUE(LockRowOpensWithoutSkill(LOCKTYPE_SLOW_OPEN));
}

TEST(PlayerbotLootLockPolicyTest, SkillGatedAndMechanicalLockTypesStayRefused)
{
    // Profession locks keep flowing through SkillByLockType with a real skill check, and lock
    // types the pipeline cannot operate (hit-to-open, explosives, traps, vehicle) must not be
    // admitted as freely openable.
    EXPECT_FALSE(LockRowOpensWithoutSkill(LOCKTYPE_PICKLOCK));
    EXPECT_FALSE(LockRowOpensWithoutSkill(LOCKTYPE_HERBALISM));
    EXPECT_FALSE(LockRowOpensWithoutSkill(LOCKTYPE_MINING));
    EXPECT_FALSE(LockRowOpensWithoutSkill(LOCKTYPE_DISARM_TRAP));
    EXPECT_FALSE(LockRowOpensWithoutSkill(LOCKTYPE_OPEN_TINKERING));
    EXPECT_FALSE(LockRowOpensWithoutSkill(LOCKTYPE_OPEN_ATTACKING));
    EXPECT_FALSE(LockRowOpensWithoutSkill(LOCKTYPE_BLASTING));
    EXPECT_FALSE(LockRowOpensWithoutSkill(LOCKTYPE_OPEN_FROM_VEHICLE));
    EXPECT_FALSE(LockRowOpensWithoutSkill(0));
}
