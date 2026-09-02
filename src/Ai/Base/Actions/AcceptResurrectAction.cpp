/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL UPSTREAM-FILE: this fork changes 3 region(s) of this upstream file.

#include "AcceptResurrectAction.h"
// PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful

#include "Event.h"
// PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
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

    // PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
    bool const success = bot->IsAlive();
    if (success)
        botAI->StartPostReviveRepairSafety();
    // PLB-LOCAL(revive-outcome): same verdict as upstream, expressed as an outcome.
    botAI->RecordReviveAttempt(GameTime::GetGameTimeMS().count(),
                               success ? PlayerbotReviveOutcome::Succeeded : PlayerbotReviveOutcome::Failed,
                               bot->IsAlive());
    return success;
}
