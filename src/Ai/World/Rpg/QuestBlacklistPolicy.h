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
        // The Party Never Ends: a multi-hub delivery chase around Silvermoon and beyond.
        // Pierre, 2026-08-30: not worth the travel for a random bot.
        case 9067:
        // Thirst Unending: credit dummy 15468 (Sunstrider Mana Tap Counter) has zero world
        // spawns, the quest has no StartItem, and no conditions rows exist for the Mana Tap
        // racial, so no seek can resolve a target. Four 300s empty-POI abandons in wave 2
        // (one per blood elf reaching level 3) before Pierre called the blacklist.
        case 8346:
        // Dwarven Digging: Broken Tools are created by looting Prospector's Picks from the
        // Bael'dun dwarves and using them at the camp anvil, a two-step tool-use-at-object
        // mechanic outside every seek's model. Pierre called the blacklist on 2026-08-30.
        case 746:
        // Crown of the Earth: the required item is created by using the quest's Crystal Phial
        // while standing at the Dolanaar moonwell, a use-provided-item-at-location mechanic no
        // seek models. Pierre called the blacklist on 2026-08-30.
        case 921:
        // Bitter Rivals and The Sprouted Fronds: the quest ENDER is a gameobject that only exists
        // while a script has it spawned (spawntimesecs -600: Jarven Thunderbrew swaps the guarded
        // barrel for the unguarded one at his patrol waypoint 2; the Sprouted Frond likewise).
        // The turn-in stay is five minutes and waits for a giver that may not appear in that
        // window, then marks the quest low priority for the life of the process. Measured live
        // 2026-09-01: Jovy, quest complete, item in bag, 300s beside the guarded barrel, abandon.
        // They are the only two overworld quests with an event-spawned ender.
        case 310:
        case 2399:
            return true;
        default:
            return false;
    }
}

#endif
