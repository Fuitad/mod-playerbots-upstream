/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AcceptResurrectAction.h"

#include "Event.h"
#include "GameTime.h"
#include "Playerbots.h"

bool AcceptResurrectAction::Execute(Event event)
{
    if (bot->IsAlive())
        return false;

    WorldPacket p(event.getPacket());
    p.rpos(0);
    ObjectGuid guid;
    p >> guid;

    WorldPacket packet(CMSG_RESURRECT_RESPONSE, 8 + 1);
    packet << guid;
    packet << uint8(1);                                        // accept
    bot->GetSession()->HandleResurrectResponseOpcode(packet);  // queue the packet to get around race condition

    bool const success = bot->IsAlive();
    if (success)
        botAI->StartPostReviveRepairSafety();
    botAI->RecordReviveAttempt(GameTime::GetGameTimeMS().count(), success, bot->IsAlive());
    return success;
}
