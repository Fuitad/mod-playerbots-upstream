/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#ifndef _PLAYERBOT_QUESTBLACKLISTPOLICY_H
#define _PLAYERBOT_QUESTBLACKLISTPOLICY_H

#include "Define.h"

// Quests the RPG system cannot complete by design, so bots never accept them and never select
// them from the log. Unlike lowPriorityQuest this survives restarts (it is code), and unlike the
// gray-drop policy it applies at any level.
[[nodiscard]] inline bool QuestIsRpgBlacklisted(uint32 questId)
{
    switch (questId)
    {
        // Red Snapper - Very Tasty!: credit needs the Draenei Fishing Net used on transient
        // Red Snapper School pools along the Azuremyst coast. The schools are short-lived
        // fishing-pool gameobjects outside every seek's model; three blamed live abandons
        // measured 2026-08-30 before Pierre called the blacklist.
        case 9452:
            return true;
        default:
            return false;
    }
}

#endif
