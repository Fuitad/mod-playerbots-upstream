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
        // Shaman totem chains (every "Call of Earth / Fire / Water / Air" quest, all 53 ids): the
        // reward is a totem spell a bot already learns from its trainer, and the "report back"
        // steps end in another zone (9449 in the Exodar, 2983/2984 in Orgrimmar), which the pick
        // rule never reaches, so 11 bots sat at complete with the slot wasted. Pierre called the
        // blacklist on 2026-09-01: "since bots get all the spells, I'm not sure if it's necessary".
        // clang-format off
        case 63: case 96: case 100: case 220: case 1103:
        case 1516: case 1517: case 1518: case 1519: case 1520: case 1521:
        case 1522: case 1523: case 1524: case 1525: case 1526: case 1527:
        case 1528: case 1529: case 1530: case 1531: case 1532:
        case 1534: case 1535: case 1536:
        case 2983: case 2984: case 2985: case 2986:
        case 9449: case 9450: case 9451:
        case 9461: case 9462: case 9464: case 9465: case 9467: case 9468:
        case 9500: case 9501: case 9502: case 9503: case 9504: case 9508: case 9509:
        case 9547: case 9551: case 9552: case 9553: case 9554: case 9555:
        case 10490: case 10491:
        // clang-format on
        // Powering our Defenses: credit comes from using the quest's runestone gameobjects in a
        // sequence the stay never completes (five-minute stays at the stone with every use
        // candidate refused; four abandons across two bots on 2026-09-02 alone, once after each
        // restart because the given-up list lives in memory). Pierre called the blacklist on
        // 2026-09-02.
        case 8490:
        // Ulag the Cleaver: Ulag is summoned by using the mausoleum gate, an object-use event
        // no seek models; 926 and 325 second stays with nothing to kill on 2026-09-02. Pierre
        // called the blacklist on 2026-09-02.
        case 1819:
        // Tree's Company: the quest's disguise is self-cast and the credit comes from standing
        // near the target while disguised, a use-on-self-then-approach mechanic outside every
        // seek's model. Pierre called the blacklist on 2026-09-02.
        case 9531:
        // Ferocitas the Dream Eater: the objective is 7 Gnarlpine Mystics (RequiredNpcOrGo2 7235)
        // with the Gnarlpine Necklace as a quest drop, and the necklace carries no use spell, so
        // the jewel-inside-the-necklace mechanic the guides describe does not exist in this data.
        // Four abandons in 148 minutes on 2026-09-03, the largest single cause in that window, and
        // not one failure but three: two stays of about 305s with 2 to 3 kills and no credit, one
        // with the objective alive at 65.9 yards behind no line of sight, one the probe could not
        // classify at all. Pierre called the blacklist on 2026-09-03 after the data did not match
        // the described mechanic.
        case 2459:
        // Dry Times: every required item is vendor-only. Tabeth reached the Cask of Merlot
        // objective beside its source, then stayed 1,108 seconds with no purchase, use, kill,
        // or matching gameobject before abandoning. Pierre called the blacklist on 2026-09-04.
        case 116:
        // The Dwarven Spy: Prospector Anvilward starts friendly. Selecting his gossip option
        // runs a waypoint sequence whose seventh point changes him to hostile faction 24, then
        // the bot can kill him for his head. No seek models that scripted gossip sequence.
        // Pierre called the blacklist on 2026-09-04.
        case 8483:
        // Kyle's Gone Missing: feed Kyle the Frenzied a Tender Strider Meat (ItemDrop 33009) that
        // drops from plainstriders. Coyahneblahe stayed 795 seconds on the meat phase on
        // 2026-09-04, killed 10 creatures, saw 18 targets and gained no credit; the feed step
        // (use the meat on a friendly creature after the drop lands) is outside every seek's
        // model. Pierre called the blacklist on 2026-09-04.
        case 11129:
            return true;
        default:
            return false;
    }
}

#endif
