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

PlayerbotExtensionRegistry& GetPlayerbotExtensionRegistry()
{
    static PlayerbotExtensionRegistry registry;
    return registry;
}
