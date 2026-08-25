/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#include "Bot/Extension/PlayerbotExtension.h"
#include "Bot/Engine/AiObjectContext.h"
#include "Bot/PlayerbotAI.h"
#include "Bot/PlayerbotMgr.h"
#include "Group.h"
#include "IntegrationTestFixture.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "PlayerbotAIConfig.h"
#include "WorldPacket.h"

#include <memory>
#include <string_view>
#include <unordered_map>

namespace
{
constexpr uint32 TEST_QUEST_ID = 99999;

class QuestShareActionObserver final : public PlayerbotExtension
{
public:
    void OnActionExecuted(PlayerbotAI* botAI, std::string_view name, bool, std::uint64_t) override
    {
        if (name == "accept quest share")
            ++actionCounts[botAI];
    }

    uint32 GetActionCount(PlayerbotAI* botAI) const
    {
        auto const itr = actionCounts.find(botAI);
        return itr == actionCounts.end() ? 0 : itr->second;
    }

private:
    std::unordered_map<PlayerbotAI*, uint32> actionCounts;
};

class TestPlayerbotMgr final : public PlayerbotMgr
{
public:
    using PlayerbotMgr::PlayerbotMgr;

    void AddOwnedBot(Player* bot) { playerBots[bot->GetGUID()] = bot; }
};

class PlayerbotQuestShareRoutingTest : public IntegrationTestFixture
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

        originalEnabled = sPlayerbotAIConfig.enabled;
        sPlayerbotAIConfig.enabled = true;

        sharer = CreateTestPlayer(101, "QuestSharer");
        groupedBot = CreateTestPlayer(102, "GroupedBot");
        outsideBot = CreateTestPlayer(103, "OutsideBot");
        ownedGroupedBot = CreateTestPlayer(104, "OwnedGroupedBot");
        ObjectAccessor::AddObject(static_cast<Player*>(sharer));

        group = std::make_unique<Group>();
        sharer->SetGroup(group.get(), 0);
        groupedBot->SetGroup(group.get(), 0);
        ownedGroupedBot->SetGroup(group.get(), 0);

        sPlayerbotsMgr.AddPlayerbotData(groupedBot, true);
        sPlayerbotsMgr.AddPlayerbotData(outsideBot, true);
        sPlayerbotsMgr.AddPlayerbotData(ownedGroupedBot, true);
        groupedBotAI = sPlayerbotsMgr.GetPlayerbotAI(groupedBot);
        outsideBotAI = sPlayerbotsMgr.GetPlayerbotAI(outsideBot);
        ownedGroupedBotAI = sPlayerbotsMgr.GetPlayerbotAI(ownedGroupedBot);

        ASSERT_NE(groupedBotAI, nullptr);
        ASSERT_NE(outsideBotAI, nullptr);
        ASSERT_NE(ownedGroupedBotAI, nullptr);
        ASSERT_TRUE(GetPlayerbotExtensionRegistry().Register(observer));
        observerRegistered = true;
    }

    void TearDown() override
    {
        if (observerRegistered)
            GetPlayerbotExtensionRegistry().Unregister(observer);

        delete groupedBotAI;
        delete outsideBotAI;
        delete ownedGroupedBotAI;

        sharer->SetGroup(nullptr);
        groupedBot->SetGroup(nullptr);
        ownedGroupedBot->SetGroup(nullptr);
        group.reset();

        ObjectAccessor::RemoveObject(static_cast<Player*>(sharer));
        sPlayerbotAIConfig.enabled = originalEnabled;

        IntegrationTestFixture::TearDown();
    }

    TestPlayer* sharer = nullptr;
    TestPlayer* groupedBot = nullptr;
    TestPlayer* outsideBot = nullptr;
    TestPlayer* ownedGroupedBot = nullptr;
    PlayerbotAI* groupedBotAI = nullptr;
    PlayerbotAI* outsideBotAI = nullptr;
    PlayerbotAI* ownedGroupedBotAI = nullptr;
    std::unique_ptr<Group> group;
    QuestShareActionObserver observer;
    bool originalEnabled = false;
    bool observerRegistered = false;
};

TEST_F(PlayerbotQuestShareRoutingTest, test_handle_master_incoming_packet_nonowned_group_bot_clears_share_state)
{
    groupedBot->SetDivider(sharer->GetGUID());
    outsideBot->SetDivider(sharer->GetGUID());
    ownedGroupedBot->SetDivider(sharer->GetGUID());

    WorldPacket packet(CMSG_PUSHQUESTTOPARTY);
    packet << TEST_QUEST_ID;

    TestPlayerbotMgr manager(sharer);
    manager.AddOwnedBot(ownedGroupedBot);
    manager.HandleMasterIncomingPacket(packet);

    groupedBotAI->UpdateAIInternal(1, false);
    outsideBotAI->UpdateAIInternal(1, false);
    ownedGroupedBotAI->UpdateAIInternal(1, false);

    EXPECT_TRUE(groupedBot->GetDivider().IsEmpty());
    EXPECT_EQ(observer.GetActionCount(groupedBotAI), 1u);
    EXPECT_EQ(outsideBot->GetDivider(), sharer->GetGUID());
    EXPECT_EQ(observer.GetActionCount(outsideBotAI), 0u);
    EXPECT_TRUE(ownedGroupedBot->GetDivider().IsEmpty());
    EXPECT_EQ(observer.GetActionCount(ownedGroupedBotAI), 1u);
}
}  // namespace
