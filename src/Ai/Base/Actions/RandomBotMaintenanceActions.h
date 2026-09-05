/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#ifndef PLAYERBOTS_RANDOMBOTMAINTENANCEACTIONS_H
#define PLAYERBOTS_RANDOMBOTMAINTENANCEACTIONS_H

#include "NewRpgBaseAction.h"
#include "ObjectGuid.h"
#include "RandomBotMaintenancePolicy.h"

class Item;
class PlayerbotAI;

namespace playerbots::maintenance
{
[[nodiscard]] bool IsEligible(PlayerbotAI* botAI);
[[nodiscard]] bool NeedsRepair(PlayerbotAI* botAI);
[[nodiscard]] bool NeedsVendor(PlayerbotAI* botAI);
[[nodiscard]] bool NeedsMount(PlayerbotAI* botAI);
[[nodiscard]] bool HasBrokenEquipment(PlayerbotAI* botAI);
[[nodiscard]] bool HasBrokenWeapon(Player* bot);
[[nodiscard]] uint32 EquippedDurabilitySum(Player* bot);
// Facts feeding DeferRoutineMaintenanceDuringQuest: whether the bot is in RPG_DO_QUEST right now,
// and whether its bags are too full to keep looting (the vendor urgency).
[[nodiscard]] bool DoingQuestNow(PlayerbotAI* botAI);
[[nodiscard]] bool CriticallyFullBags(PlayerbotAI* botAI);
[[nodiscard]] bool HearthstoneReady(Player* bot);
// Whether hearthing now would shorten the walk to `destination` (HearthShortcutWorthwhile over the
// bot's live state). The caller casts through the "hearthstone" action and re-plans from the inn.
[[nodiscard]] bool HearthShortcutFor(Player* bot, WorldPosition const& destination);
// The first item in the bags that starts a quest the bot should take right now.
// Decision in QuestStartItemPolicy.h; null when nothing qualifies.
[[nodiscard]] Item* FindUsableQuestStartItem(PlayerbotAI* botAI);
// An accept that was dispatched and did not take. Remembered so the errand cannot retry the
// same item forever, which is how it first went wrong.
void MarkQuestStartItemRefused(Player* bot, ObjectGuid item);
[[nodiscard]] bool QuestStartItemRefused(Player* bot, ObjectGuid item);
}  // namespace playerbots::maintenance

// Uses a quest-starting item out of the bags. Items with a StartQuest are deliberately looted
// (LootAction.cpp IsLootAllowed) and nothing ever used one, so they accumulated: 129 items across
// 73 of 200 bots on 2026-09-03, in bags that were 159 of 200 over the old 80% loot line. Each one
// is a quest never taken and a slot never returned. Pierre, 2026-09-03: use it the moment it
// arrives unless the quest log is full.
class RandomBotQuestStartItemAction : public NewRpgBaseAction
{
public:
    explicit RandomBotQuestStartItemAction(PlayerbotAI* botAI)
        : NewRpgBaseAction(botAI, "random bot quest start item")
    {
    }

    bool Execute(Event event) override;
};

class RandomBotRepairAction : public NewRpgBaseAction
{
public:
    explicit RandomBotRepairAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "random bot repair") {}

    bool Execute(Event event) override;

private:
    uint32 targetEntry = 0;
    // Set when the bot stood at a repairer and still could not pay for a single item: the errand
    // is released and not replanned for a while, so the bot can go and earn instead of parking.
    uint32 unaffordableAt = 0;
    WorldPosition targetPosition;
};

class RandomBotVendorAction : public NewRpgBaseAction
{
public:
    explicit RandomBotVendorAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "random bot vendor") {}

    bool Execute(Event event) override;

private:
    uint32 targetEntry = 0;
    uint32 lastAttempt = 0;
    WorldPosition targetPosition;
};

class RandomBotMountAction : public NewRpgBaseAction
{
public:
    explicit RandomBotMountAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "random bot mount") {}

    bool Execute(Event event) override;

private:
    void ClearTarget();

    playerbots::maintenance::MountTier targetTier = playerbots::maintenance::MountTier::None;
    uint32 targetEntry = 0;
    uint32 targetItemId = 0;
    uint32 targetRidingSpell = 0;
    uint32 nextAttempt = 0;
    WorldPosition targetPosition;
};

#endif
