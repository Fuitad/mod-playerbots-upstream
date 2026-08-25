/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#include "Bot/Population/RandomPlayerbotAdmission.h"
#include "gtest/gtest.h"

TEST(RandomPlayerbotAdmissionTest, AcceptsOneOpaquePersistentEventPerExactGuid)
{
    std::vector<RandomPlayerbotAdmission> const admissions = {
        {662, {{"career plan", 2, "serialized-plan"}}},
        {663, {{"career plan", 2, "other-plan"}}},
    };

    EXPECT_TRUE(ValidateRandomPlayerbotAdmissions(admissions).empty());
}

TEST(RandomPlayerbotAdmissionTest, RefusesDuplicateGuidsEventsAndReservedEventNames)
{
    EXPECT_EQ(
        ValidateRandomPlayerbotAdmissions({{662, {{"career plan", 2, "plan"}}}, {662, {{"career plan", 2, "plan"}}}}),
        "duplicate_guid");
    EXPECT_EQ(ValidateRandomPlayerbotAdmissions({{662, {{"career plan", 2, "plan"}, {"career plan", 2, "other"}}}}),
              "duplicate_event");
    EXPECT_EQ(ValidateRandomPlayerbotAdmissions({{662, {{"add", 1, ""}}}}), "reserved_event");
    EXPECT_EQ(ValidateRandomPlayerbotAdmissions({{662, {{"logout", 1, ""}}}}), "reserved_event");
}

TEST(RandomPlayerbotAdmissionTest, RefusesEmptyOrNonpersistentAdmissions)
{
    EXPECT_EQ(ValidateRandomPlayerbotAdmissions({}), "empty_admissions");
    EXPECT_EQ(ValidateRandomPlayerbotAdmissions({{0, {{"career plan", 2, "plan"}}}}), "invalid_guid");
    EXPECT_EQ(ValidateRandomPlayerbotAdmissions({{662, {}}}), "empty_events");
    EXPECT_EQ(ValidateRandomPlayerbotAdmissions({{662, {{"career plan", 0, "plan"}}}}), "zero_event_value");
    EXPECT_EQ(ValidateRandomPlayerbotAdmissions({{662, {{"", 2, "plan"}}}}), "empty_event_name");
}

TEST(RandomPlayerbotAdmissionTest, ClearsStartupAdmissionsUnlessExplicitlyPreserved)
{
    EXPECT_TRUE(ShouldClearRandomPlayerbotAdmissions(false));
    EXPECT_FALSE(ShouldClearRandomPlayerbotAdmissions(true));
}

TEST(RandomPlayerbotAdmissionTest, PreservedAdmissionsKeepTheirExactAccountCensus)
{
    EXPECT_EQ(ResolveRandomPlayerbotAccountCount(false, 16, 18), 18u);
    EXPECT_EQ(ResolveRandomPlayerbotAccountCount(true, 16, 18), 16u);
    EXPECT_TRUE(IsRandomPlayerbotAdmissionAccountType(0));
    EXPECT_TRUE(IsRandomPlayerbotAdmissionAccountType(1));
    EXPECT_FALSE(IsRandomPlayerbotAdmissionAccountType(2));
}

TEST(RandomPlayerbotAdmissionTest, DiscoversAndHydratesRuntimeAccountCacheWhenDefaultFactoryIsFenced)
{
    std::vector<std::uint32_t> accountCache = {111};
    std::vector<std::uint32_t> const discoveredAccountIds = {395, 396, 397};
    bool discoveryCalled = false;

    std::vector<std::uint32_t> const reconciledAccountIds =
        DiscoverAndHydrateRandomPlayerbotAccountCache(accountCache,
                                                      [&]()
                                                      {
                                                          discoveryCalled = true;
                                                          return discoveredAccountIds;
                                                      });

    EXPECT_TRUE(discoveryCalled);
    EXPECT_EQ(reconciledAccountIds, discoveredAccountIds);
    EXPECT_EQ(accountCache, discoveredAccountIds);
}
