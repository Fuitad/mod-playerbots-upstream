/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
// PLB-LOCAL(working-tree): Uncommitted local change.
// Upstream: No corresponding block at the merge base. (base 8d9f6aa6bc6d).

// PLB-LOCAL UPSTREAM-FILE: this fork changes 1 region(s) of this upstream file.

#ifndef PLAYERBOTS_EXTERNALEVENTHELPER_H
#define PLAYERBOTS_EXTERNALEVENTHELPER_H

#include "Common.h"
#include <map>

class AiObjectContext;
class Player;
class WorldPacket;

class ExternalEventHelper
{
public:
    ExternalEventHelper(AiObjectContext* aiObjectContext) : aiObjectContext(aiObjectContext) {}

    bool ParseChatCommand(std::string const command, Player* owner = nullptr);
    void HandlePacket(std::map<uint16, std::string>& handlers, WorldPacket const& packet, Player* owner = nullptr);
    bool HandleCommand(std::string const name, std::string const param, Player* owner = nullptr);

    // PLB-LOCAL(99e4c7d19107): feat(extensions): complete generic module event seams
    // Non-executing mirror of ParseChatCommand: true when the text would be consumed
    // as a chat command (trigger lookup or item-link auto trade), without firing any
    // trigger. Must stay in sync with ParseChatCommand's resolution order.
    bool IsChatCommand(std::string const& command);

private:
    AiObjectContext* aiObjectContext;
};

#endif
