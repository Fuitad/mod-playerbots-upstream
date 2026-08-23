/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RANDOMBOTMAINTENANCEACTIONS_H
#define PLAYERBOTS_RANDOMBOTMAINTENANCEACTIONS_H

#include "NewRpgBaseAction.h"
#include "RandomBotMaintenancePolicy.h"

class PlayerbotAI;

namespace playerbots::maintenance
{
[[nodiscard]] bool IsEligible(PlayerbotAI* botAI);
[[nodiscard]] bool NeedsRepair(PlayerbotAI* botAI);
[[nodiscard]] bool NeedsVendor(PlayerbotAI* botAI);
[[nodiscard]] bool NeedsMount(PlayerbotAI* botAI);
}  // namespace playerbots::maintenance

class RandomBotRepairAction : public NewRpgBaseAction
{
public:
    explicit RandomBotRepairAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "random bot repair") {}

    bool Execute(Event event) override;

private:
    uint32 targetEntry = 0;
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
