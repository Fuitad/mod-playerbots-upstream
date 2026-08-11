/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "PlayerbotExtension.h"

#include <algorithm>

bool PlayerbotExtensionRegistry::Register(PlayerbotExtension& extension)
{
    if (std::find(extensions.begin(), extensions.end(), &extension) != extensions.end())
        return false;

    extensions.push_back(&extension);
    return true;
}

bool PlayerbotExtensionRegistry::Unregister(PlayerbotExtension& extension)
{
    auto const position = std::find(extensions.begin(), extensions.end(), &extension);
    if (position == extensions.end())
        return false;

    extensions.erase(position);
    return true;
}

bool PlayerbotExtensionRegistry::InitializeTradeSkills(Player* player)
{
    for (PlayerbotExtension* extension : extensions)
    {
        if (extension->InitializeTradeSkills(player))
            return true;
    }

    return false;
}

bool PlayerbotExtensionRegistry::HandleBotEvent(PlayerbotAI* botAI, PlayerbotEvent const& event)
{
    for (PlayerbotExtension* extension : extensions)
    {
        if (extension->HandleBotEvent(botAI, event))
            return true;
    }

    return false;
}

void PlayerbotExtensionRegistry::OnWorldUpdate(std::uint32_t diff)
{
    ForEach([diff](PlayerbotExtension& extension) { extension.OnWorldUpdate(diff); });
}

void PlayerbotExtensionRegistry::OnBotUpdate(PlayerbotAI* botAI, PlayerbotAIUpdate const& update)
{
    ForEach([botAI, &update](PlayerbotExtension& extension) { extension.OnBotUpdate(botAI, update); });
}

void PlayerbotExtensionRegistry::OnActionExecuted(PlayerbotAI* botAI, std::string_view name, bool success,
                                                  std::uint64_t timestampMs)
{
    ForEach([botAI, name, success, timestampMs](PlayerbotExtension& extension)
            { extension.OnActionExecuted(botAI, name, success, timestampMs); });
}

void PlayerbotExtensionRegistry::OnBotDeath(PlayerbotAI* botAI, std::uint64_t timestampMs)
{
    ForEach([botAI, timestampMs](PlayerbotExtension& extension) { extension.OnBotDeath(botAI, timestampMs); });
}

void PlayerbotExtensionRegistry::OnBotRemoved(PlayerbotAI* botAI)
{
    ForEach([botAI](PlayerbotExtension& extension) { extension.OnBotRemoved(botAI); });
}

bool PlayerbotExtensionRegistry::HandleRemoteCommand(std::string_view command, std::string& response)
{
    for (PlayerbotExtension* extension : extensions)
        if (extension->HandleRemoteCommand(command, response))
            return true;

    return false;
}

bool PlayerbotExtensionRegistry::IsObjectiveAvailable(PlayerbotAI* botAI, PlayerbotObjective const& objective,
                                                      std::uint64_t timestampMs)
{
    for (PlayerbotExtension* extension : extensions)
        if (!extension->IsObjectiveAvailable(botAI, objective, timestampMs))
            return false;

    return true;
}

bool PlayerbotExtensionRegistry::HandleRandomBotAccountCleanup()
{
    for (PlayerbotExtension* extension : extensions)
        if (extension->HandleRandomBotAccountCleanup())
            return true;

    return false;
}

bool PlayerbotExtensionRegistry::PrepareBotPurge(std::vector<std::uint32_t> const& botGuids)
{
    bool allowed = true;
    for (PlayerbotExtension* extension : extensions)
        allowed = extension->PrepareBotPurge(botGuids) && allowed;
    return allowed;
}

void PlayerbotExtensionRegistry::OnBotPurge(std::vector<std::uint32_t> const& botGuids)
{
    ForEach([&botGuids](PlayerbotExtension& extension) { extension.OnBotPurge(botGuids); });
}

PlayerbotExtensionRegistry& GetPlayerbotExtensionRegistry()
{
    static PlayerbotExtensionRegistry registry;
    return registry;
}
