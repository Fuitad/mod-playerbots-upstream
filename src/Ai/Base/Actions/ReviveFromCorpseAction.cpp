/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL UPSTREAM-FILE: this fork changes 14 region(s) of this upstream file.
// Each is tagged PLB-LOCAL(<sha>) where a marker could be placed safely; run
// tools/plb_local_markers.py --check for the authoritative list. docs/local-changes.md.

#include "ReviveFromCorpseAction.h"
// PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful

#include "Corpse.h"
// PLB-LOCAL(revive-safety): the hostile-in-reach count scans the grid directly.
#include "CellImpl.h"
#include "Creature.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Event.h"
#include "FleeManager.h"
#include "GameGraveyard.h"
// PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
#include "GameTime.h"
#include "MapMgr.h"
// PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
#include "PhysicalDeathCountPolicy.h"
#include "PlayerbotRecoveryPolicy.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "ServerFacade.h"
#include "Timer.h"

#include <algorithm>
#include <cmath>
#include <list>

// PLB-LOCAL BEGIN(revive-safety): hostiles whose aggro reach, plus a margin, covers the ghost.
// Shared by the revive gate and the corpse walk so both judge the same radius; see
// PlayerbotRecoveryPolicy.h, ReviveAtBodyAllowed, for the measurement.
namespace
{
// A ghost cannot see living creatures, so any list built through the bot's own visibility (the
// "possible targets" values, and upstream's "no mobs near" test on top of them) is empty for the
// whole corpse walk. Measured live 2026-09-01 22:57: Sugandhi revived at her body with the gate
// counting zero and was in combat with the Defias Smuggler that killed her two seconds later. The
// scan below walks the grid directly and judges hostility by faction, which a ghost keeps. The
// radius is wider than any aggro reach (the core caps attack distance at 45 yards) plus the margin.
constexpr float REVIVE_HOSTILE_SCAN_YARDS = 60.0f;

uint32 HostilesInReach(PlayerbotAI* /*botAI*/, Player* bot, Creature** nearest = nullptr)
{
    std::list<Creature*> creatures;
    Acore::AnyUnitInObjectRangeCheck check(bot, REVIVE_HOSTILE_SCAN_YARDS);
    Acore::CreatureListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(bot, creatures, check);
    Cell::VisitObjects(bot, searcher, REVIVE_HOSTILE_SCAN_YARDS);

    uint32 count = 0;
    float nearestDist = REVIVE_HOSTILE_SCAN_YARDS + 1.0f;
    if (nearest)
        *nearest = nullptr;
    for (Creature* creature : creatures)
    {
        if (!creature->IsAlive() || creature->IsCritter() || creature->IsCivilian() || creature->IsPet() ||
            creature->IsTotem() || !creature->IsHostileTo(bot))
            continue;
        float const reach = creature->GetAttackDistance(bot) + playerbots::recovery::REVIVE_AGGRO_MARGIN_YARDS;
        if (!bot->IsWithinDist(creature, reach, false))
            continue;
        ++count;
        float const dist = bot->GetDistance2d(creature);
        if (nearest && dist < nearestDist)
        {
            nearestDist = dist;
            *nearest = creature;
        }
    }
    return count;
}
}  // namespace
// PLB-LOCAL END(revive-safety)

bool ReviveFromCorpseAction::Execute(Event event)
{
    Player* groupLeader = botAI->GetGroupLeader();
    Corpse* corpse = bot->GetCorpse();

    // follow group Leader when group Leader revives
    WorldPacket& p = event.getPacket();
    if (!p.empty() && p.GetOpcode() == CMSG_RECLAIM_CORPSE && groupLeader && !corpse && bot->IsAlive())
    {
        if (ServerFacade::instance().IsDistanceLessThan(AI_VALUE2(float, "distance", "group leader"),
                                                        sPlayerbotAIConfig.farDistance))
        {
            if (!botAI->HasStrategy("follow", BOT_STATE_NON_COMBAT))
            {
                botAI->TellMasterNoFacing("Welcome back!");
                botAI->ChangeStrategy("+follow,-stay", BOT_STATE_NON_COMBAT);
                return true;
            }
        }
    }

    // PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
    uint64 const timestampMs = GameTime::GetGameTimeMS().count();
    uint64 const now = GameTime::GetGameTime().count();
    bool const hasCorpse = corpse != nullptr;
    bool const reclaimDelayElapsed =
        hasCorpse &&
        corpse->GetGhostTime() + bot->GetCorpseReclaimDelay(corpse->GetType() == CORPSE_RESURRECTABLE_PVP) <= now;
    playerbots::recovery::CorpseReclaimEligibility const eligibility{
        .playerAlive = bot->IsAlive(),
        .inArena = bot->InArena(),
        .ghost = bot->HasPlayerFlag(PLAYER_FLAGS_GHOST),
        .hasCorpse = hasCorpse,
        .reclaimDelayElapsed = reclaimDelayElapsed,
        .corpseInMap = hasCorpse && corpse->IsInMap(bot),
        .withinReclaimRadius = hasCorpse && corpse->IsWithinDist(bot, CORPSE_RECLAIM_RADIUS, true),
    };
    if (!playerbots::recovery::CanReclaimCorpse(eligibility))
    {
        // PLB-LOCAL(revive-outcome): upstream recorded `false` here, which is a failed revive.
        // Not a failure. This trigger fires every tick a ghost waits out its reclaim delay, so
        // recording these as failures made an ordinary corpse run read as a revive loop.
        botAI->RecordReviveAttempt(timestampMs, playerbots::recovery::CorpseReviveOutcome(eligibility, false, false),
                                   bot->IsAlive());
        return false;
    }

    if (groupLeader)
    // PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
    {
        if (!GET_PLAYERBOT_AI(groupLeader) && groupLeader->isDead() && groupLeader->GetCorpse() &&
            ServerFacade::instance().IsDistanceLessThan(AI_VALUE2(float, "distance", "group leader"),
                                                        sPlayerbotAIConfig.farDistance))
        {
            // PLB-LOCAL(revive-outcome): upstream recorded `false` here too. A choice, not a
            // failure: the bot waits to be resurrected by its human leader.
            botAI->RecordReviveAttempt(timestampMs, playerbots::recovery::CorpseReviveOutcome(eligibility, true, false),
                                       bot->IsAlive());
            return false;
        }
    }

    // PLB-LOCAL BEGIN(revive-safety): a random bot does not take its body back inside a hostile's
    // reach; the corpse walk keeps moving it to a safer spot. See PlayerbotRecoveryPolicy.h.
    // Upstream: nothing here, the revive went ahead whatever stood at the corpse.
    if (!botAI->HasGameClientMaster())
    {
        uint32 const hostiles = HostilesInReach(botAI, bot);
        uint32 const deadSeconds =
            static_cast<uint32>(std::max<int64>(0, static_cast<int64>(now) - corpse->GetGhostTime()));
        if (!playerbots::recovery::ReviveAtBodyAllowed(hostiles, deadSeconds))
        {
            LOG_DEBUG("playerbots", "[DeathProbe] {} REVIVE-DEFERRED hostiles {} dead {}s", bot->GetName(), hostiles,
                      deadSeconds);
            botAI->RecordReviveAttempt(timestampMs, playerbots::recovery::CorpseReviveOutcome(eligibility, true, false),
                                       bot->IsAlive());
            return false;
        }
    }
    // PLB-LOCAL END(revive-safety)

    LOG_DEBUG("playerbots", "Bot {} {}:{} <{}> revives at body", bot->GetGUID().ToString().c_str(),
              bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->GetLevel(), bot->GetName().c_str());
    // PLB-LOCAL(death-probe): timestamped twin of the line above, for relapse timing. Temporary.
    LOG_DEBUG("playerbots", "[DeathProbe] {} REVIVED body t {}", bot->GetName(), getMSTime());

    // PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
    bot->GetMotionMaster()->Clear();
    bot->StopMoving();

    WorldPacket packet(CMSG_RECLAIM_CORPSE);
    packet << bot->GetGUID();
    bot->GetSession()->HandleReclaimCorpseOpcode(packet);

    bool const success = bot->IsAlive();
    if (success)
        botAI->StartPostReviveRepairSafety();
    // PLB-LOCAL(revive-outcome): same verdict as upstream, routed through the shared classifier.
    botAI->RecordReviveAttempt(timestampMs, playerbots::recovery::CorpseReviveOutcome(eligibility, false, success),
                               bot->IsAlive());
    return success;
}

bool FindCorpseAction::Execute(Event /*event*/)
{
    if (bot->InBattleground())
        return false;

    Player* groupLeader = botAI->GetGroupLeader();
    Corpse* corpse = GetBotCorpse();
    if (!corpse)
        return false;

    // if (groupLeader)
    // {
    //     if (!GET_PLAYERBOT_AI(groupLeader) &&
    //         ServerFacade::instance().IsDistanceLessThan(AI_VALUE2(float, "distance", "group leader"),
    //         sPlayerbotAIConfig.farDistance)) return false;
    // }

    // PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
    uint32 dCount = AI_VALUE(uint32, "death count");

    if (!botAI->HasGameClientMaster())
    {
        if (dCount >= 5)
        {
            // LOG_INFO("playerbots", "Bot {} {}:{} <{}>: died too many times, was revived and teleported",
            //     bot->GetGUID().ToString().c_str(), bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->GetLevel(),
            //     bot->GetName().c_str());
            bool const recovered = RecoverAtHomebind();
            context->GetValue<uint32>("death count")
                ->Set(playerbots::recovery::DeathCountAfterForcedRecovery(dCount, recovered));
            return recovered;
        }
    }

    WorldPosition botPos(bot);
    WorldPosition corpsePos(corpse);
    WorldPosition moveToPos = corpsePos;
    WorldPosition leaderPos(groupLeader);

    float reclaimDist = CORPSE_RECLAIM_RADIUS - 5.0f;
    float corpseDist = botPos.distance(corpsePos);
    int64 deadTime = time(nullptr) - corpse->GetGhostTime();

    bool moveToLeader = groupLeader && groupLeader != bot && leaderPos.fDist(corpsePos) < reclaimDist;

    // Should we ressurect? If so, return false.
    if (corpseDist < reclaimDist)
    {
        if (moveToLeader)  // We are near group leader.
        {
            if (botPos.fDist(leaderPos) < sPlayerbotAIConfig.spellDistance)
                return false;
        }
        else if (deadTime > 8 * MINUTE)  // We have walked too long already.
            return false;
        // PLB-LOCAL(revive-safety): the same reach-plus-margin the revive gate uses, so the walk
        // does not stop at a spot the gate then refuses. Upstream:
        //     GuidVector units = AI_VALUE(GuidVector, "possible targets no los");
        //     if (botPos.getUnitsAggro(units, bot) == 0) return false;
        else if (HostilesInReach(botAI, bot) == 0)  // There are no mobs near.
            return false;
    }

    // If we are getting close move to a save ressurrection spot instead of just the corpse.
    // PLB-LOCAL BEGIN(revive-safety): the wait spot comes from the hostile scan, not from the
    // ghost's visibility list. Upstream ran FleeManager over "possible targets no los", which a
    // ghost never fills, so with a hostile at the corpse the manager was not useful, the move to
    // the bot's own position failed, and the fallback below took the spirit healer at once
    // (Buoyantboy, 2026-09-01 23:04: spirit healer 1.6 s after release). The ghost now walks to
    // the far edge of the reclaim radius, away from the nearest hostile, and holds there; the
    // revive gate scans around the ghost, so it revives as soon as that spot is out of reach.
    bool holdingAtWaitSpot = false;
    Creature* threat = nullptr;
    if (corpseDist < sPlayerbotAIConfig.reactDistance && !moveToLeader)
        (void)HostilesInReach(botAI, bot, &threat);
    if (corpseDist < sPlayerbotAIConfig.reactDistance)
    {
        if (moveToLeader)
            moveToPos = leaderPos;
        else if (threat)
        {
            float const away = threat->GetAngle(corpse);
            float const wx = corpsePos.GetPositionX() + std::cos(away) * reclaimDist;
            float const wy = corpsePos.GetPositionY() + std::sin(away) * reclaimDist;
            float const wz = std::max(bot->GetMap()->GetHeight(wx, wy, corpsePos.GetPositionZ() + 5.0f),
                                      bot->GetMap()->GetWaterLevel(wx, wy));
            moveToPos = WorldPosition(corpsePos.GetMapId(), wx, wy, wz, 0.0f);
            holdingAtWaitSpot = bot->GetDistance2d(wx, wy) < 5.0f;
        }
        else
        // PLB-LOCAL END(revive-safety)
        {
            FleeManager manager(bot, reclaimDist, 0.0, urand(0, 1), moveToPos);

            if (manager.isUseful())
            {
                float rx, ry, rz;
                if (manager.CalculateDestination(&rx, &ry, &rz))
                    moveToPos = WorldPosition(moveToPos.GetMapId(), rx, ry, rz, 0.0);
                else if (!moveToPos.GetReachableRandomPointOnGround(bot, reclaimDist, urand(0, 1)))
                    moveToPos = corpsePos;
            }
        }
    }

    // Actual mobing part.
    bool moved = false;

    if (!botAI->AllowActivity(ALL_ACTIVITY))
    {
        uint32 delay = ServerFacade::instance().GetDistance2d(bot, corpse) /
                       bot->GetSpeed(MOVE_RUN);        // Time a bot would take to travel to it's corpse.
        delay = std::min(delay, uint32(10 * MINUTE));  // Cap time to get to corpse at 10 minutes.

        if (deadTime > delay)
        {
            bot->GetMotionMaster()->Clear();
            bot->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TELEPORTED | AURA_INTERRUPT_FLAG_CHANGE_MAP);
            bot->TeleportTo(moveToPos.GetMapId(), moveToPos.GetPositionX(), moveToPos.GetPositionY(),
                            moveToPos.GetPositionZ(), 0);
        }

        moved = true;
    }
    else
    {
        if (bot->isMoving())
            moved = true;
        else
        {
            if (deadTime < 10 * MINUTE && dCount < 5)  // Look for corpse up to 30 minutes.
            {
                moved = MoveTo(moveToPos.GetMapId(), moveToPos.GetPositionX(), moveToPos.GetPositionY(),
                               moveToPos.GetPositionZ(), false, false);
            }

            // PLB-LOCAL(revive-safety): holding at the wait spot is progress, not a failed walk.
            if (!moved && holdingAtWaitSpot)
                moved = true;

            if (!moved)
            // PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
            {
                // PLB-LOCAL(death-probe): why the walk gave the corpse up. Temporary diagnostic.
                LOG_DEBUG("playerbots",
                          "[DeathProbe] {} SPIRIT-FALLBACK corpseDist {:.0f} dead {}s deaths {} holding {} threat {}",
                          bot->GetName(), corpseDist, deadTime, dCount, holdingAtWaitSpot,
                          threat ? threat->GetEntry() : 0);
                moved = botAI->DoSpecificAction("spirit healer", Event(), true);
            }
        }
    }

    return moved;
}

Corpse* FindCorpseAction::GetBotCorpse() const { return bot->GetCorpse(); }

bool FindCorpseAction::RecoverAtHomebind() { return sRandomPlayerbotMgr.RecoverAtHomebind(bot); }

bool FindCorpseAction::isUseful()
{
    if (bot->InBattleground())
        return false;

    return bot->GetCorpse();
}

GraveyardStruct const* SpiritHealerAction::GetGrave(bool startZone)
{
    GraveyardStruct const* ClosestGrave = nullptr;
    GraveyardStruct const* NewGrave = nullptr;

    ClosestGrave = sGraveyard->GetClosestGraveyard(bot, bot->GetTeamId());

    if (!startZone && ClosestGrave)
        return ClosestGrave;

    if (botAI->HasStrategy("follow", BOT_STATE_NON_COMBAT) && botAI->GetGroupLeader() && botAI->GetGroupLeader() != bot)
    {
        Player* groupLeader = botAI->GetGroupLeader();
        if (groupLeader && groupLeader != bot)
        {
            ClosestGrave = sGraveyard->GetClosestGraveyard(groupLeader, bot->GetTeamId());

            if (ClosestGrave)
                return ClosestGrave;
        }
    }
    else if (startZone && AI_VALUE(uint8, "durability"))
    {
        TravelTarget* travelTarget = AI_VALUE(TravelTarget*, "travel target");

        if (travelTarget->getPosition())
        {
            WorldPosition travelPos = *travelTarget->getPosition();
            if (travelPos.GetMapId() != uint32(-1))
            {
                uint32 areaId = 0;
                uint32 zoneId = 0;
                sMapMgr->GetZoneAndAreaId(bot->GetPhaseMask(), zoneId, areaId, travelPos.GetMapId(),
                                          travelPos.GetPositionX(), travelPos.GetPositionY(), travelPos.GetPositionZ());
                ClosestGrave = sGraveyard->GetClosestGraveyard(
                    travelPos.GetMapId(), travelPos.GetPositionX(), travelPos.GetPositionY(), travelPos.GetPositionZ(),
                    bot->GetTeamId(), areaId, zoneId, bot->getClass() == CLASS_DEATH_KNIGHT);

                if (ClosestGrave)
                    return ClosestGrave;
            }
        }
    }

    std::vector<uint32> races;

    if (bot->GetTeamId() == TEAM_ALLIANCE)
        races = {RACE_HUMAN, RACE_DWARF, RACE_GNOME, RACE_NIGHTELF, RACE_DRAENEI};
    else
        races = {RACE_ORC, RACE_TROLL, RACE_TAUREN, RACE_UNDEAD_PLAYER, RACE_BLOODELF};

    float graveDistance = -1;

    WorldPosition botPos(bot);

    for (auto race : races)
    {
        for (uint32 cls = 0; cls < MAX_CLASSES; cls++)
        {
            PlayerInfo const* info = sObjectMgr->GetPlayerInfo(race, cls);
            if (!info)
                continue;

            uint32 areaId = 0;
            uint32 zoneId = 0;
            sMapMgr->GetZoneAndAreaId(bot->GetPhaseMask(), zoneId, areaId, info->mapId, info->positionX,
                                      info->positionY, info->positionZ);

            NewGrave = sGraveyard->GetClosestGraveyard(info->mapId, info->positionX, info->positionY, info->positionZ,
                                                       bot->GetTeamId(), areaId, zoneId, cls == CLASS_DEATH_KNIGHT);
            if (!NewGrave)
                continue;

            WorldPosition gravePos(NewGrave->Map, NewGrave->x, NewGrave->y, NewGrave->z);

            float newDist = botPos.fDist(gravePos);

            if (graveDistance < 0 || newDist < graveDistance)
            {
                ClosestGrave = NewGrave;
                graveDistance = newDist;
            }
        }
    }

    return ClosestGrave;
}

bool SpiritHealerAction::Execute(Event /*event*/)
{
    Corpse* corpse = bot->GetCorpse();
    if (!corpse)
    {
        botAI->TellError("I am not a spirit");
        // PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
        return false;
    }

    uint32 dCount = AI_VALUE(uint32, "death count");
    int64 deadTime = time(nullptr) - corpse->GetGhostTime();

    GraveyardStruct const* ClosestGrave =
        GetGrave(dCount > 10 || deadTime > 15 * MINUTE || AI_VALUE(uint8, "durability") < 10);

    if (!ClosestGrave)
        return false;

    if (bot->GetDistance2d(ClosestGrave->x, ClosestGrave->y) < sPlayerbotAIConfig.sightDistance)
    {
        GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
        for (GuidVector::iterator i = npcs.begin(); i != npcs.end(); i++)
        {
            Unit* unit = botAI->GetUnit(*i);
            if (unit && unit->HasNpcFlag(UNIT_NPC_FLAG_SPIRITHEALER))
            {
                // PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
                // PLB-LOCAL(death-probe): timestamped twin for relapse timing. Temporary.
                LOG_DEBUG("playerbots", "[DeathProbe] {} REVIVED spirit t {}", bot->GetName(), getMSTime());
                LOG_DEBUG("playerbots", "Bot {} {}:{} <{}> revives at spirit healer", bot->GetGUID().ToString().c_str(),
                          bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->GetLevel(), bot->GetName());
                PlayerbotChatHandler ch(bot);
                bot->ResurrectPlayer(0.5f);
                bot->SpawnCorpseBones();
                context->GetValue<Unit*>("current target")->Set(nullptr);
                bot->SetTarget();
                botAI->TellMaster(PlayerbotTextMgr::instance().GetBotTextOrDefault("hello", "Hello", {}));
                // PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful

                bool const success = bot->IsAlive();
                if (success)
                    botAI->StartPostReviveRepairSafety();
                // PLB-LOCAL(revive-outcome): same verdict as upstream, expressed as an outcome.
                botAI->RecordReviveAttempt(GameTime::GetGameTimeMS().count(),
                                           success ? PlayerbotReviveOutcome::Succeeded : PlayerbotReviveOutcome::Failed,
                                           bot->IsAlive());

                if (success && dCount > 20)
                    context->GetValue<uint32>("death count")->Set(0);

                return success;
            }
        }

        // PLB-LOCAL BEGIN(stranded-ghost): the search found no spirit healer. When the bot is already
        // standing on the graveyard there is nothing left to walk toward, so the move and teleport
        // fallbacks below cannot change anything: MoveNear to a point the bot occupies is a no op, and
        // the teleport sends it where it already is. Reporting either as success is what let a stranded
        // ghost log "spirit healer succeeded" on every tick for 35 minutes while it stayed dead and
        // stationary, so no telemetry and no loop classifier ever saw a failure. Report it truthfully and
        // let the recovery module's stranded ghost watch take the bot from here.
        // Upstream: fell straight through to the fallbacks below and returned true from them.
        if (bot->GetMapId() == ClosestGrave->Map &&
            bot->GetDistance2d(ClosestGrave->x, ClosestGrave->y) <= sPlayerbotAIConfig.tooCloseDistance)
        {
            botAI->RecordReviveAttempt(GameTime::GetGameTimeMS().count(), PlayerbotReviveOutcome::Failed,
                                       bot->IsAlive());
            return false;
        }
        // PLB-LOCAL END(stranded-ghost)
    }

    bool moved = false;

    if (bot->IsWithinLOS(ClosestGrave->x, ClosestGrave->y, ClosestGrave->z))
        moved = MoveNear(ClosestGrave->Map, ClosestGrave->x, ClosestGrave->y, ClosestGrave->z, 0.0);
    else
        moved = MoveTo(ClosestGrave->Map, ClosestGrave->x, ClosestGrave->y, ClosestGrave->z, false, false);

    if (moved)
        return true;

    // if (!IsRealPlayer(botAI->GetMaster()))
    // {
    bot->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TELEPORTED | AURA_INTERRUPT_FLAG_CHANGE_MAP);
    return bot->TeleportTo(ClosestGrave->Map, ClosestGrave->x, ClosestGrave->y, ClosestGrave->z, 0.f);
    // }

    // LOG_INFO("playerbots", "Bot {} {}:{} <{}> can't find a spirit healer", bot->GetGUID().ToString().c_str(),
    //          bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->GetLevel(), bot->GetName().c_str());

    // botAI->TellError("Cannot find any spirit healer nearby");
    return false;
}

bool SpiritHealerAction::isUseful() { return bot->HasPlayerFlag(PLAYER_FLAGS_GHOST); }
