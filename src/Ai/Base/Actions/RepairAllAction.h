/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL UPSTREAM-FILE: this fork changes 1 region(s) of this upstream file.
// Each is tagged PLB-LOCAL(<sha>) where a marker could be placed safely; run
// tools/plb_local_markers.py --check for the authoritative list. docs/local-changes.md.

#ifndef PLAYERBOTS_REPAIRALLACTION_H
#define PLAYERBOTS_REPAIRALLACTION_H

#include "Action.h"

class PlayerbotAI;

class RepairAllAction : public Action
{
public:
    RepairAllAction(PlayerbotAI* botAI) : Action(botAI, "repair") {}

    bool Execute(Event event) override;
// PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful

protected:
    virtual bool ExecutePaidRepair();
};

#endif
