/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL UPSTREAM-FILE: this fork changes 2 region(s) of this upstream file.
// Each is tagged PLB-LOCAL(<sha>) where a marker could be placed safely; run
// tools/plb_local_markers.py --check for the authoritative list. docs/local-changes.md.

#ifndef PLAYERBOTS_REVIVEFROMCORPSEACTION_H
#define PLAYERBOTS_REVIVEFROMCORPSEACTION_H

#include "MovementActions.h"

class PlayerbotAI;
// PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
class Corpse;

struct GraveyardStruct;

class ReviveFromCorpseAction : public MovementAction
{
public:
    ReviveFromCorpseAction(PlayerbotAI* botAI) : MovementAction(botAI, "revive from corpse") {}

    bool Execute(Event event) override;
};

class FindCorpseAction : public MovementAction
{
public:
    FindCorpseAction(PlayerbotAI* botAI) : MovementAction(botAI, "find corpse") {}

    bool Execute(Event event) override;
    bool isUseful() override;
// PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful

protected:
    virtual Corpse* GetBotCorpse() const;
    virtual bool RecoverAtHomebind();
};

class SpiritHealerAction : public MovementAction
{
public:
    SpiritHealerAction(PlayerbotAI* botAI, std::string const name = "spirit healer") : MovementAction(botAI, name) {}

    GraveyardStruct const* GetGrave(bool startZone);
    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
