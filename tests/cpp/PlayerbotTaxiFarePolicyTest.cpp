/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include <array>

#include "Bot/Movement/PlayerbotTaxiFlight.h"
#include "gtest/gtest.h"

TEST(PlayerbotTaxiFarePolicyTest, TheFareIsEveryHopSummedThenDiscountedAndRoundedUp)
{
    // Player::ActivateTaxiPathTo sums the hops, applies the flight master's reputation discount
    // and rounds up; a bot that estimates lower walks to the master and is refused there.
    std::array<uint32 const, 2> const hops{330u, 110u};
    EXPECT_EQ(PlayerbotTaxiFareFromHops(hops, 1.0f), 440u);
    // Honored: five percent off, 418, no rounding needed.
    EXPECT_EQ(PlayerbotTaxiFareFromHops(hops, 0.95f), 418u);
    // 730 at ten percent off is 657 exactly; 731 would be 657.9 and rounds up to 658.
    std::array<uint32 const, 1> const single{731u};
    EXPECT_EQ(PlayerbotTaxiFareFromHops(single, 0.9f), 658u);
    // A discount the core would never send still cannot make a ride free or negative.
    EXPECT_EQ(PlayerbotTaxiFareFromHops(single, 0.0f), 0u);
    EXPECT_EQ(PlayerbotTaxiFareFromHops({}, 1.0f), 0u);
}
