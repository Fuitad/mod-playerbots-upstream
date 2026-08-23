/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RANDOMBOTMAINTENANCETRIGGERS_H
#define PLAYERBOTS_RANDOMBOTMAINTENANCETRIGGERS_H

#include "Trigger.h"

class RandomBotRepairTrigger : public Trigger
{
public:
    explicit RandomBotRepairTrigger(PlayerbotAI* botAI) : Trigger(botAI, "random bot needs repair", 5) {}

    bool IsActive() override;
};

class RandomBotVendorTrigger : public Trigger
{
public:
    explicit RandomBotVendorTrigger(PlayerbotAI* botAI) : Trigger(botAI, "random bot needs vendor", 5) {}

    bool IsActive() override;
};

class RandomBotMountTrigger : public Trigger
{
public:
    explicit RandomBotMountTrigger(PlayerbotAI* botAI) : Trigger(botAI, "random bot needs mount", 5) {}

    bool IsActive() override;
};

#endif
