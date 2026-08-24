/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PLAYERBOTEXTENSION_H
#define PLAYERBOTS_PLAYERBOTEXTENSION_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class Action;
class Engine;
class Player;
class PlayerbotAI;
class Strategy;
class Trigger;
class UntypedValue;

template <class T>
class SharedNamedObjectContextList;

enum class PlayerbotEventType
{
    Loot,
    QuestAccepted,
    QuestObjectiveProgress,
    QuestObjectiveCompleted,
    QuestFailed,
    QuestCompleted,
    QuestTurnedIn,
    Kill,
    Level
};

struct PlayerbotEvent
{
    PlayerbotEventType type;
    std::uint32_t subjectId = 0;
    std::string subject;
};

struct PlayerbotAIUpdate
{
    std::uint32_t elapsedMs = 0;
    std::uint32_t dueLatenessMs = 0;
    std::uint32_t durationMs = 0;
};

enum class PlayerbotObjectiveKind
{
    Quest,
    Grind,
    Profession
};

struct PlayerbotObjective
{
    PlayerbotObjectiveKind kind = PlayerbotObjectiveKind::Quest;
    std::uint32_t subjectId = 0;
    std::int32_t objectiveIndex = 0;
    std::uint32_t mapId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

class PlayerbotExtension
{
public:
    virtual ~PlayerbotExtension() = default;

    virtual void AddActionContexts(SharedNamedObjectContextList<Action>&) {}
    virtual void AddTriggerContexts(SharedNamedObjectContextList<Trigger>&) {}
    virtual void AddStrategyContexts(SharedNamedObjectContextList<Strategy>&) {}
    virtual void AddValueContexts(SharedNamedObjectContextList<UntypedValue>&) {}

    virtual void AddDefaultCombatStrategies(Player*, PlayerbotAI*, Engine&) {}
    virtual void AddDefaultNonCombatStrategies(Player*, PlayerbotAI*, Engine&) {}
    virtual void AddDefaultDeadStrategies(Player*, PlayerbotAI*, Engine&) {}

    virtual bool InitializeTradeSkills(Player*) { return false; }

    // Hooks run synchronously on their caller. OnWorldUpdate is world thread only. OnBotUpdate and
    // HandleBotEvent inherit their caller thread. Keep them bounded and nonblocking, and never retain
    // raw game pointers for work on another thread. See docs/extension-threading.md.
    virtual bool HandleBotEvent(PlayerbotAI*, PlayerbotEvent const&) { return false; }
    virtual void OnWorldUpdate(std::uint32_t) {}
    virtual void OnBotUpdate(PlayerbotAI*, PlayerbotAIUpdate const&) {}
    virtual void OnActionExecuted(PlayerbotAI*, std::string_view, bool, std::uint64_t) {}
    virtual void OnBotDeath(PlayerbotAI*, std::uint64_t) {}
    virtual void OnBotRemoved(PlayerbotAI*) {}
    virtual bool HandleRemoteCommand(std::string_view, std::string&) { return false; }
    virtual bool IsObjectiveAvailable(PlayerbotAI*, PlayerbotObjective const&, std::uint64_t) { return true; }
    virtual bool HandleRandomBotAccountCleanup() { return false; }
    virtual bool PrepareBotPurge(std::vector<std::uint32_t> const&) { return true; }
    virtual void OnBotPurge(std::vector<std::uint32_t> const&) {}
};

class PlayerbotExtensionRegistry
{
public:
    bool Register(PlayerbotExtension& extension);
    bool Unregister(PlayerbotExtension& extension);
    bool InitializeTradeSkills(Player* player);
    bool HandleBotEvent(PlayerbotAI* botAI, PlayerbotEvent const& event);
    void OnWorldUpdate(std::uint32_t diff);
    void OnBotUpdate(PlayerbotAI* botAI, PlayerbotAIUpdate const& update);
    void OnActionExecuted(PlayerbotAI* botAI, std::string_view name, bool success, std::uint64_t timestampMs);
    void OnBotDeath(PlayerbotAI* botAI, std::uint64_t timestampMs);
    void OnBotRemoved(PlayerbotAI* botAI);
    bool HandleRemoteCommand(std::string_view command, std::string& response);
    bool IsObjectiveAvailable(PlayerbotAI* botAI, PlayerbotObjective const& objective, std::uint64_t timestampMs);
    bool HandleRandomBotAccountCleanup();
    bool PrepareBotPurge(std::vector<std::uint32_t> const& botGuids);
    void OnBotPurge(std::vector<std::uint32_t> const& botGuids);

    template <class Visitor>
    void ForEach(Visitor&& visitor)
    {
        for (PlayerbotExtension* extension : extensions)
            visitor(*extension);
    }

private:
    std::vector<PlayerbotExtension*> extensions;
};

PlayerbotExtensionRegistry& GetPlayerbotExtensionRegistry();

#endif
