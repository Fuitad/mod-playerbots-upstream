/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
// PLB-LOCAL UPSTREAM-FILE: this fork changes 3 region(s) of this upstream file.

#ifndef PLAYERBOTS_STUCKTRIGGERS_H
#define PLAYERBOTS_STUCKTRIGGERS_H

#include "Trigger.h"

// PLB-LOCAL(combat-stuck): the fight span the two combat triggers measure. See CombatStuckPolicy.h.
#include "CombatStuckPolicy.h"

class MoveStuckTrigger : public Trigger
{
public:
    MoveStuckTrigger(PlayerbotAI* botAI) : Trigger(botAI, "move stuck", 5) {}

    bool IsActive() override;
};

class MoveLongStuckTrigger : public Trigger
{
public:
    MoveLongStuckTrigger(PlayerbotAI* botAI) : Trigger(botAI, "move long stuck", 5) {}

    bool IsActive() override;
};

class CombatStuckTrigger : public Trigger
{
public:
    CombatStuckTrigger(PlayerbotAI* botAI) : Trigger(botAI, "combat stuck", 5) {}

    bool IsActive() override;

private:
    CombatSpan _span;  // PLB-LOCAL(combat-stuck)
};

class CombatLongStuckTrigger : public Trigger
{
public:
    CombatLongStuckTrigger(PlayerbotAI* botAI) : Trigger(botAI, "combat long stuck", 5) {}

    bool IsActive() override;

private:
    CombatSpan _span;  // PLB-LOCAL(combat-stuck)
};

#endif
