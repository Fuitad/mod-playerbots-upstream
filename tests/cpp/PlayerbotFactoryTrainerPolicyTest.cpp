/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Bot/Factory/PlayerbotTrainerLearningPolicy.h"
#include "gtest/gtest.h"

namespace
{
TEST(PlayerbotFactoryTrainerPolicyTest, FactoryLearnsOnlyFromClassTrainers)
{
    EXPECT_TRUE(playerbots::IsTrainerAutoLearned(Trainer::Type::Class));
    EXPECT_FALSE(playerbots::IsTrainerAutoLearned(Trainer::Type::Tradeskill));
}
}  // namespace
