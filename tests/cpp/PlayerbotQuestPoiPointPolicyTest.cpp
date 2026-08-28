/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include "Ai/World/Rpg/QuestPoiPointPolicy.h"
#include "gtest/gtest.h"

TEST(PlayerbotQuestPoiPointPolicyTest, TheAimedPointIsAlwaysOneOfTheSurveyedVertices)
{
    // Upstream aimed at a random weighted MEAN of the polygon's vertices, a location that need not
    // coincide with any of them. Returning an index makes synthesising a point impossible.
    std::vector<std::pair<float, float>> const polygon = {
        {-8561.0f, -218.0f}, {-8530.0f, -200.0f}, {-8538.0f, -183.0f},
        {-8556.0f, -148.0f}, {-8635.0f, -111.0f}, {-8672.0f, -124.0f}};

    size_t const idx = NearestPoiPointIndex(polygon, -8660.0f, -120.0f);
    EXPECT_LT(idx, polygon.size());
    EXPECT_FLOAT_EQ(polygon[idx].first, -8672.0f);
    EXPECT_FLOAT_EQ(polygon[idx].second, -124.0f);

    // The mean of these six vertices is roughly (-8582, -164), which is no vertex at all.
    for (auto const& vertex : polygon)
        EXPECT_FALSE(vertex.first == -8582.0f && vertex.second == -164.0f);
}

TEST(PlayerbotQuestPoiPointPolicyTest, TheNearestVertexWinsFromEitherEnd)
{
    std::vector<std::pair<float, float>> const line = {{0.0f, 0.0f}, {100.0f, 0.0f}, {200.0f, 0.0f}};
    EXPECT_EQ(NearestPoiPointIndex(line, -10.0f, 0.0f), 0u);
    EXPECT_EQ(NearestPoiPointIndex(line, 105.0f, 0.0f), 1u);
    EXPECT_EQ(NearestPoiPointIndex(line, 500.0f, 0.0f), 2u);
    // Distance is two dimensional, not one.
    EXPECT_EQ(NearestPoiPointIndex(line, 0.0f, 500.0f), 0u);
}

TEST(PlayerbotQuestPoiPointPolicyTest, ASinglePointPolygonIsReturnedUnchanged)
{
    // 98% of turn-in POIs carry exactly one point, where choosing and averaging agree.
    std::vector<std::pair<float, float>> const single = {{-8903.0f, -163.0f}};
    EXPECT_EQ(NearestPoiPointIndex(single, 4000.0f, -9000.0f), 0u);
}
