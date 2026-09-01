/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * World glue for the item objectives a vendor fulfils: some quests ask for something the bot can
 * only BUY (The Chill of Death 375 needs a Coarse Thread, Beer Basted Boar Ribs 384 needs Hot
 * Spices; 15 quests below level 20 list a vendor-sold item). The quest POI for such an objective
 * marks the vendor, so the bot already walks there and then stood for 300s with nothing to kill
 * (measured live 2026-09-01: Valderotaux, 375, kills 0 targets 0, abandon). Called from one
 * tagged site in NewRpgDoQuestAction::DoIncompleteQuest (NewRpgAction.cpp, tag quest-vendor-item).
 */

#ifndef _PLAYERBOT_NEWRPGQUESTVENDOR_H
#define _PLAYERBOT_NEWRPGQUESTVENDOR_H

#include "ObjectGuid.h"

class Player;
class Quest;

struct QuestVendorTarget
{
    // Empty when the objective is not an item objective, the item is already covered, no nearby
    // friendly vendor sells it for plain money, or the bot cannot afford a single unit.
    ObjectGuid guid;
    uint32 entry = 0;
    uint32 vendorSlot = 0;
    uint32 itemId = 0;
    // Units still missing for the objective, capped by what the bot can pay for.
    uint32 count = 0;
};

// The nearest friendly vendor among the cached nearby npcs that sells the objective's item for
// money (no extended cost), with the quantity to buy.
[[nodiscard]] QuestVendorTarget FindQuestObjectiveVendor(Player* bot, Quest const* quest, int32 objectiveIdx,
                                                         GuidVector const& nearbyNpcs);

// Buys through Player::BuyItemFromVendorSlot one unit at a time (the core owns the interaction
// range, money and bag checks). Returns true when at least one unit landed in the bags.
bool BuyQuestObjectiveItem(Player* bot, QuestVendorTarget const& target);

#endif
