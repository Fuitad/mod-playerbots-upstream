/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#ifndef PLAYERBOTS_RANDOMBOTMAINTENANCESTRATEGY_H
#define PLAYERBOTS_RANDOMBOTMAINTENANCESTRATEGY_H

#include "NonCombatStrategy.h"

class RandomBotMaintenanceStrategy : public NonCombatStrategy
{
public:
    explicit RandomBotMaintenanceStrategy(PlayerbotAI* botAI) : NonCombatStrategy(botAI) {}

    std::string const getName() override { return "random bot maintenance"; }
    uint32 GetType() const override { return STRATEGY_TYPE_NONCOMBAT; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
