#include <cstdlib>
#include <iostream>
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

    bool handlesTradeSkills;
    bool handlesEvent = false;
    unsigned int tradeSkillCalls = 0;
    unsigned int eventCalls = 0;
    PlayerbotEvent lastEvent{PlayerbotEventType::Level};
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
    Require(registry.Unregister(first), "registered extension must unregister");
    Require(!registry.Unregister(first), "missing extension must not unregister twice");

    calls.clear();
    registry.ForEach([&calls](PlayerbotExtension& extension) { calls.push_back(&extension); });
    Require(calls == std::vector<PlayerbotExtension*>({&second, &third}), "unregistered extensions must not be called");

    return EXIT_SUCCESS;
}
