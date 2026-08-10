/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PLAYERBOTEXTENSION_H
#define PLAYERBOTS_PLAYERBOTEXTENSION_H

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
};

class PlayerbotExtensionRegistry
{
public:
    bool Register(PlayerbotExtension& extension);
    bool Unregister(PlayerbotExtension& extension);
    bool InitializeTradeSkills(Player* player);

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
