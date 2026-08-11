#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "Bot/Extension/PlayerbotExtension.h"

namespace
{
class RecordingExtension final : public PlayerbotExtension
{
public:
    explicit RecordingExtension(bool handlesTradeSkills = false) : handlesTradeSkills(handlesTradeSkills) {}

    bool InitializeTradeSkills(Player*) override
    {
        ++tradeSkillCalls;
        return handlesTradeSkills;
    }

    bool HandleBotEvent(PlayerbotAI*, PlayerbotEvent const& event) override
    {
        ++eventCalls;
        lastEvent = event;
        return handlesEvent;
    }

    void OnWorldUpdate(std::uint32_t diff) override
    {
        ++worldUpdateCalls;
        lastWorldDiff = diff;
    }

    void OnBotUpdate(PlayerbotAI*, PlayerbotAIUpdate const& update) override
    {
        ++botUpdateCalls;
        lastBotUpdate = update;
    }

    void OnActionExecuted(PlayerbotAI*, std::string_view name, bool success, std::uint64_t timestampMs) override
    {
        ++actionCalls;
        lastActionName = name;
        lastActionSuccess = success;
        lastActionTimestampMs = timestampMs;
    }

    void OnBotDeath(PlayerbotAI*, std::uint64_t timestampMs) override
    {
        ++deathCalls;
        lastDeathTimestampMs = timestampMs;
    }

    void OnBotRemoved(PlayerbotAI*) override { ++removedCalls; }

    bool HandleRemoteCommand(std::string_view command, std::string& response) override
    {
        ++remoteCommandCalls;
        lastRemoteCommand = command;
        if (!handlesRemoteCommand)
            return false;

        response = remoteResponse;
        return true;
    }

    bool IsObjectiveAvailable(PlayerbotAI*, PlayerbotObjective const& objective, std::uint64_t timestampMs) override
    {
        ++objectiveCalls;
        lastObjective = objective;
        lastObjectiveTimestampMs = timestampMs;
        return allowsObjective;
    }

    bool HandleRandomBotAccountCleanup() override
    {
        ++cleanupCalls;
        return handlesCleanup;
    }

    bool PrepareBotPurge(std::vector<std::uint32_t> const& botGuids) override
    {
        ++purgeCalls;
        lastPurgedBotGuids = botGuids;
        return allowsPurge;
    }

    void OnBotPurge(std::vector<std::uint32_t> const& botGuids) override
    {
        ++purgedCalls;
        lastCompletedPurgeGuids = botGuids;
    }

    bool handlesTradeSkills;
    bool handlesEvent = false;
    bool handlesRemoteCommand = false;
    bool handlesCleanup = false;
    bool allowsObjective = true;
    bool allowsPurge = true;
    std::string remoteResponse = "handled";
    unsigned int tradeSkillCalls = 0;
    unsigned int eventCalls = 0;
    unsigned int worldUpdateCalls = 0;
    unsigned int botUpdateCalls = 0;
    unsigned int actionCalls = 0;
    unsigned int deathCalls = 0;
    unsigned int removedCalls = 0;
    unsigned int remoteCommandCalls = 0;
    unsigned int objectiveCalls = 0;
    unsigned int cleanupCalls = 0;
    unsigned int purgeCalls = 0;
    unsigned int purgedCalls = 0;
    std::uint32_t lastWorldDiff = 0;
    PlayerbotAIUpdate lastBotUpdate;
    std::string lastActionName;
    bool lastActionSuccess = false;
    std::uint64_t lastActionTimestampMs = 0;
    std::uint64_t lastDeathTimestampMs = 0;
    std::string lastRemoteCommand;
    PlayerbotObjective lastObjective;
    std::vector<std::uint32_t> lastPurgedBotGuids;
    std::vector<std::uint32_t> lastCompletedPurgeGuids;
    std::uint64_t lastObjectiveTimestampMs = 0;
    PlayerbotEvent lastEvent{PlayerbotEventType::Level, 0, {}};
};

void Require(bool condition, char const* message)
{
    if (condition)
        return;

    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}
}  // namespace

int main()
{
    PlayerbotExtensionRegistry registry;
    std::vector<PlayerbotExtension*> calls;
    RecordingExtension first;
    RecordingExtension second(true);
    RecordingExtension third(true);

    Require(registry.Register(first), "first registration must succeed");
    Require(!registry.Register(first), "duplicate registration must be refused");
    Require(registry.Register(second), "second registration must succeed");
    Require(registry.Register(third), "third registration must succeed");

    registry.ForEach([&calls](PlayerbotExtension& extension) { calls.push_back(&extension); });

    Require(calls == std::vector<PlayerbotExtension*>({&first, &second, &third}),
            "extensions must run once in registration order");

    Require(registry.InitializeTradeSkills(nullptr), "a handling extension must stop trade skill initialization");
    Require(first.tradeSkillCalls == 1, "non-handling extensions must be consulted");
    Require(second.tradeSkillCalls == 1, "the first handling extension must be called");
    Require(third.tradeSkillCalls == 0, "extensions after the first handler must not be called");

    second.handlesEvent = true;
    PlayerbotEvent const event{PlayerbotEventType::QuestCompleted, 42, "A Difficult Journey"};
    Require(registry.HandleBotEvent(nullptr, event), "a handling extension must stop bot event delivery");
    Require(first.eventCalls == 1, "non-handling extensions must receive bot events");
    Require(second.eventCalls == 1, "the first handling extension must receive the bot event");
    Require(third.eventCalls == 0, "extensions after the first event handler must not be called");
    Require(second.lastEvent.type == PlayerbotEventType::QuestCompleted && second.lastEvent.subjectId == 42 &&
                second.lastEvent.subject == "A Difficult Journey",
            "bot event identity must reach the extension unchanged");

    PlayerbotAIUpdate const update{50, 7, 3};
    registry.OnWorldUpdate(25);
    registry.OnBotUpdate(nullptr, update);
    registry.OnActionExecuted(nullptr, "travel", true, 1234);
    registry.OnBotDeath(nullptr, 1250);
    registry.OnBotRemoved(nullptr);
    for (RecordingExtension const* extension : {&first, &second, &third})
    {
        Require(extension->worldUpdateCalls == 1 && extension->lastWorldDiff == 25,
                "every extension must receive world updates");
        Require(extension->botUpdateCalls == 1 && extension->lastBotUpdate.elapsedMs == 50 &&
                    extension->lastBotUpdate.dueLatenessMs == 7 && extension->lastBotUpdate.durationMs == 3,
                "every extension must receive the complete bot update observation");
        Require(extension->actionCalls == 1 && extension->lastActionName == "travel" && extension->lastActionSuccess &&
                    extension->lastActionTimestampMs == 1234,
                "every extension must receive action outcomes");
        Require(extension->deathCalls == 1 && extension->lastDeathTimestampMs == 1250,
                "every extension must receive bot deaths");
        Require(extension->removedCalls == 1, "every extension must receive bot removal");
    }

    second.handlesRemoteCommand = true;
    third.handlesRemoteCommand = true;
    std::string response;
    Require(registry.HandleRemoteCommand("telemetry", response), "the first command handler must handle the command");
    Require(response == second.remoteResponse, "the first command handler response must be preserved");
    Require(first.remoteCommandCalls == 1 && second.remoteCommandCalls == 1 && third.remoteCommandCalls == 0,
            "remote command dispatch must stop at the first handler");

    PlayerbotObjective const objective{PlayerbotObjectiveKind::Quest, 42, 3, 0, 1.0f, 2.0f, 3.0f};
    second.allowsObjective = false;
    Require(!registry.IsObjectiveAvailable(nullptr, objective, 1300),
            "one extension must be able to veto an objective");
    Require(first.objectiveCalls == 1 && second.objectiveCalls == 1 && third.objectiveCalls == 0,
            "objective checks must stop at the first veto");
    Require(second.lastObjective.kind == PlayerbotObjectiveKind::Quest && second.lastObjective.subjectId == 42 &&
                second.lastObjective.objectiveIndex == 3 && second.lastObjectiveTimestampMs == 1300,
            "objective identity must reach the extension unchanged");

    second.handlesCleanup = true;
    Require(registry.HandleRandomBotAccountCleanup(), "the first cleanup handler must claim the cleanup pass");
    Require(first.cleanupCalls == 1 && second.cleanupCalls == 1 && third.cleanupCalls == 0,
            "cleanup dispatch must stop at the first handler");

    std::vector<std::uint32_t> const purgedBotGuids = {42, 84};
    third.allowsPurge = false;
    Require(!registry.PrepareBotPurge(purgedBotGuids), "one extension must be able to refuse a bot purge");
    for (RecordingExtension const* extension : {&first, &second, &third})
    {
        Require(extension->purgeCalls == 1 && extension->lastPurgedBotGuids == purgedBotGuids,
                "every extension must receive the complete bot purge cohort before a refusal is returned");
    }

    registry.OnBotPurge(purgedBotGuids);
    for (RecordingExtension const* extension : {&first, &second, &third})
    {
        Require(extension->purgedCalls == 1 && extension->lastCompletedPurgeGuids == purgedBotGuids,
                "every extension must receive a confirmed bot purge cohort");
    }

    Require(registry.Unregister(first), "registered extension must unregister");
    Require(!registry.Unregister(first), "missing extension must not unregister twice");

    calls.clear();
    registry.ForEach([&calls](PlayerbotExtension& extension) { calls.push_back(&extension); });
    Require(calls == std::vector<PlayerbotExtension*>({&second, &third}), "unregistered extensions must not be called");

    return EXIT_SUCCESS;
}
