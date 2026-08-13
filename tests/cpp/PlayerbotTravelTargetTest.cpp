/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Ai/Base/Actions/MoveToTravelTargetAction.h"
#include "Ai/Base/Value/LastMovementValue.h"
#include "Bot/Engine/AiObjectContext.h"
#include "Bot/Engine/WorldPacket/Event.h"
#include "Bot/PlayerbotAI.h"
#include "Bot/PlayerbotMgr.h"
#include "IntegrationTestFixture.h"
#include "PathGenerator.h"
#include "PlayerbotAIConfig.h"

namespace
{
class PlayerbotTravelTargetTest : public IntegrationTestFixture
{
protected:
    void SetUp() override
    {
        IntegrationTestFixture::SetUp();

        static bool contextsBuilt = false;
        if (!contextsBuilt)
        {
            AiObjectContext::BuildAllSharedContexts();
            contextsBuilt = true;
        }

        originalReactDistance = sPlayerbotAIConfig.reactDistance;
        originalTargetRecalcDistance = sPlayerbotAIConfig.targetPosRecalcDistance;
        originalEnabled = sPlayerbotAIConfig.enabled;
        sPlayerbotAIConfig.enabled = true;
        sPlayerbotAIConfig.reactDistance = 50.0f;
        sPlayerbotAIConfig.targetPosRecalcDistance = 0.1f;

        bot = CreateTestPlayer(201, "TravelBot");
        bot->Relocate(0.0f, 0.0f, 0.0f, 0.0f);
        sPlayerbotsMgr.AddPlayerbotData(bot, true);
        botAI = sPlayerbotsMgr.GetPlayerbotAI(bot);
        ASSERT_NE(botAI, nullptr);
    }

    void TearDown() override
    {
        delete botAI;
        sPlayerbotAIConfig.reactDistance = originalReactDistance;
        sPlayerbotAIConfig.targetPosRecalcDistance = originalTargetRecalcDistance;
        sPlayerbotAIConfig.enabled = originalEnabled;
        IntegrationTestFixture::TearDown();
    }

    TestPlayer* bot = nullptr;
    PlayerbotAI* botAI = nullptr;
    float originalReactDistance = 0.0f;
    float originalTargetRecalcDistance = 0.0f;
    bool originalEnabled = false;
};

TEST_F(PlayerbotTravelTargetTest, AllWalkTravelPathMovesToItsReachableWaypoint)
{
    WorldPosition waypoint(bot->GetMapId(), 40.0f, 0.0f, 0.0f);
    WorldPosition destination(bot->GetMapId(), 1000.0f, 0.0f, 0.0f);
    TravelDestination travelDestination(0.0f, 1.0f);

    TravelTarget* target = botAI->GetAiObjectContext()->GetValue<TravelTarget*>("travel target")->Get();
    target->setTarget(&travelDestination, &destination);
    target->setForced(true);

    LastMovement& movement = botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
    movement.lastPath.addPoint(waypoint, NODE_PATH);
    movement.lastPath.addPoint(destination, NODE_PATH);

    MoveToTravelTargetAction action(botAI);

    EXPECT_TRUE(action.Execute(Event()));
    EXPECT_FLOAT_EQ(movement.lastMoveToX, waypoint.GetPositionX());
    EXPECT_FLOAT_EQ(movement.lastMoveToY, waypoint.GetPositionY());
    EXPECT_FLOAT_EQ(movement.lastMoveToZ, waypoint.GetPositionZ());

    target->releaseVisitors();
}

TEST_F(PlayerbotTravelTargetTest, PathStepUsesRequestedStartAndDestination)
{
    WorldPosition start(bot->GetMapId(), 40.0f, 10.0f, 5.0f);
    WorldPosition destination(bot->GetMapId(), 100.0f, 20.0f, 8.0f);
    PathGenerator path(bot);

    destination.getPathStepFrom(start, path);

    EXPECT_FLOAT_EQ(path.GetStartPosition().x, start.GetPositionX());
    EXPECT_FLOAT_EQ(path.GetStartPosition().y, start.GetPositionY());
    EXPECT_FLOAT_EQ(path.GetStartPosition().z, start.GetPositionZ());
    EXPECT_FLOAT_EQ(path.GetEndPosition().x, destination.GetPositionX());
    EXPECT_FLOAT_EQ(path.GetEndPosition().y, destination.GetPositionY());
    EXPECT_FLOAT_EQ(path.GetEndPosition().z, destination.GetPositionZ());
}
}  // namespace
