/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

// PLB-LOCAL UPSTREAM-FILE: this fork changes 17 region(s) of this upstream file.

#include "ReleaseSpiritAction.h"
// PLB-LOCAL(66dbf2793b3a): fix(recovery): stop reporting a waiting ghost as a failed revive.
// Upstream: No corresponding block at the merge base. (base 8d9f6aa6bc6d).

#include "Corpse.h"
#include "Event.h"
#include "GameGraveyard.h"
// PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
#include "GameTime.h"
#include "Log.h"
#include "NearestNpcsValue.h"
#include "ObjectDefines.h"
#include "ObjectGuid.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "ServerFacade.h"
// PLB-LOCAL(37c8545d7510): feat(economy): gate glyphs, free repairs, fares and gifts on EconomyManagedSupplies
#include "PlayerbotAIConfig.h"

// ReleaseSpiritAction implementation
bool ReleaseSpiritAction::Execute(Event event)
{
    if (bot->IsAlive())
    {
        if (!bot->InBattleground())
        {
            botAI->TellMasterNoFacing(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                "release_spirit_not_dead_wait", "I am not dead, will wait here", {}));
            // -follow in bg is overwriten each tick with +follow
            // +stay in bg causes stuttering effect as bot is cycled between +stay and +follow each tick
            botAI->ChangeStrategy("-follow,+stay", BOT_STATE_NON_COMBAT);
        }

        return false;
    }

    if (bot->GetCorpse() && bot->HasPlayerFlag(PLAYER_FLAGS_GHOST))
    {
        // PLB-LOCAL(66dbf2793b3a): fix(recovery): stop reporting a waiting ghost as a failed revive.
        // Upstream: botAI->TellMasterNoFacing(PlayerbotTextMgr::instance().GetBotTextOrDefault( "release_spirit_alre...
        botAI->TellMasterNoFacing(PlayerbotTextMgr::instance().GetBotTextOrDefault("release_spirit_already_spirit",
                                                                                   "I am already a spirit", {}));
        return false;
    }

    const WorldPacket& packet = event.getPacket();
    // PLB-LOCAL BEGIN(66dbf2793b3a): fix(recovery): stop reporting a waiting ghost as a failed revive.
    // Upstream: const std::string message = !packet.empty() && packet.GetOpcode() == CMSG_REPOP_REQUEST ? PlayerbotT...
    const std::string message =
        !packet.empty() && packet.GetOpcode() == CMSG_REPOP_REQUEST
            ? PlayerbotTextMgr::instance().GetBotTextOrDefault("release_spirit_releasing", "Releasing...", {})
            : PlayerbotTextMgr::instance().GetBotTextOrDefault("release_spirit_meet_graveyard",
                                                               "Meet me at the graveyard", {});
    // PLB-LOCAL END(66dbf2793b3a)
    botAI->TellMasterNoFacing(message);

    // PLB-LOCAL(37c8545d7510): feat(economy): gate glyphs, free repairs, fares and gifts on EconomyManagedSupplies
    if (!sPlayerbotAIConfig.economyManagedSupplies)
        bot->DurabilityRepairAll(false, 1.0f, false);
    LogRelease("released");

    WorldPacket releasePacket(CMSG_REPOP_REQUEST);
    releasePacket << uint8(0);
    bot->GetSession()->HandleRepopRequestOpcode(releasePacket);

    return true;
}

void ReleaseSpiritAction::LogRelease(const std::string& releaseMsg) const
{
    const std::string teamPrefix = bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H";

    // PLB-LOCAL(66dbf2793b3a): fix(recovery): stop reporting a waiting ghost as a failed revive.
    // Upstream: LOG_DEBUG("playerbots", "Bot {} {}:{} <{}> {}", bot->GetGUID().ToString().c_str(), teamPrefix, bot->...
    LOG_DEBUG("playerbots", "Bot {} {}:{} <{}> {}", bot->GetGUID().ToString().c_str(), teamPrefix, bot->GetLevel(),
              bot->GetName().c_str(), releaseMsg.c_str());
}

// AutoReleaseSpiritAction implementation
bool AutoReleaseSpiritAction::Execute(Event /*event*/)
{
    // PLB-LOCAL(37c8545d7510): feat(economy): gate glyphs, free repairs, fares and gifts on EconomyManagedSupplies
    if (!sPlayerbotAIConfig.economyManagedSupplies)
        bot->DurabilityRepairAll(false, 1.0f, false);
    LogRelease("auto released");

    WorldPacket packet(CMSG_REPOP_REQUEST);
    packet << uint8(0);
    bot->GetSession()->HandleRepopRequestOpcode(packet);

    LogRelease("releases spirit");

    if (bot->InBattleground())
    {
        return HandleBattlegroundSpiritHealer();
    }

    botAI->SetNextCheckDelay(1000);
    return true;
}

bool AutoReleaseSpiritAction::isUseful()
{
    if (!bot->isDead() || bot->InArena())
        return false;

    if (bot->InBattleground())
        return ShouldDelayBattlegroundRelease();

    if (bot->HasPlayerFlag(PLAYER_FLAGS_GHOST))
        return false;

    return ShouldAutoRelease();
}

bool AutoReleaseSpiritAction::HandleBattlegroundSpiritHealer()
{
    constexpr uint32_t RESURRECT_DELAY = 15;
    const time_t now = time(nullptr);

    // PLB-LOCAL(66dbf2793b3a): fix(recovery): stop reporting a waiting ghost as a failed revive.
    // Upstream: if ((now - m_bgGossipTime < RESURRECT_DELAY) && bot->HasAura(SPELL_WAITING_FOR_RESURRECT)) (base 8d9...
    if ((now - m_bgGossipTime < RESURRECT_DELAY) && bot->HasAura(SPELL_WAITING_FOR_RESURRECT))
    {
        return false;
    }

    float bgRange = 2000.0f;
    GuidVector npcs = NearestNpcsValue(botAI, bgRange);
    Unit* spiritHealer = nullptr;

    for (auto const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsFriendlyTo(bot) && unit->IsSpiritService())
        {
            spiritHealer = unit;
            break;
        }
    }

    if (!spiritHealer)
        return false;

    if (bot->GetDistance(spiritHealer) >= INTERACTION_DISTANCE)
    {
        // Bot needs to actually click spirit-healer in BG to get res timer going
        // and in IOC it's not within clicking range when they res in own base

        // Teleport to nearest friendly Spirit Healer when not currently in range of one.
        bot->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TELEPORTED | AURA_INTERRUPT_FLAG_CHANGE_MAP);
        // PLB-LOCAL(66dbf2793b3a): fix(recovery): stop reporting a waiting ghost as a failed revive.
        // Upstream: bot->TeleportTo(bot->GetMapId(), spiritHealer->GetPositionX(), spiritHealer->GetPositionY(), spi...
        bot->TeleportTo(bot->GetMapId(), spiritHealer->GetPositionX(), spiritHealer->GetPositionY(),
                        spiritHealer->GetPositionZ(), 0.f);
        RESET_AI_VALUE(bool, "combat::self target");
        RESET_AI_VALUE(WorldPosition, "current position");
    }
    else if (!IsSelfBot(bot))
    {
        m_bgGossipTime = now;
        WorldPacket packet(CMSG_GOSSIP_HELLO);
        packet << spiritHealer->GetGUID();
        bot->GetSession()->HandleGossipHelloOpcode(packet);
    }

    return true;
}

bool AutoReleaseSpiritAction::ShouldAutoRelease() const
{
    if (!bot->GetGroup())
        return true;

    Player* groupLeader = botAI->GetGroupLeader();
    if (!groupLeader || groupLeader == bot)
        return true;

    if (!IsRealPlayer(botAI->GetMaster()))
        return true;

    // PLB-LOCAL(66dbf2793b3a): fix(recovery): stop reporting a waiting ghost as a failed revive.
    // Upstream: if (IsRealPlayer(botAI->GetMaster()) && groupLeader->GetMapId() == bot->GetMapId() && bot->GetMap()...
    if (IsRealPlayer(botAI->GetMaster()) && groupLeader->GetMapId() == bot->GetMapId() && bot->GetMap() &&
        (bot->GetMap()->IsRaid() || bot->GetMap()->IsDungeon()))
    {
        return false;
    }

    // PLB-LOCAL(66dbf2793b3a): fix(recovery): stop reporting a waiting ghost as a failed revive.
    // Upstream: return ServerFacade::instance().IsDistanceGreaterThan( AI_VALUE2(float, "distance", "group leader"),...
    return ServerFacade::instance().IsDistanceGreaterThan(AI_VALUE2(float, "distance", "group leader"),
                                                          sPlayerbotAIConfig.sightDistance);
}

bool AutoReleaseSpiritAction::ShouldDelayBattlegroundRelease() const
{
    // The below delays release to spirit with 6 seconds.
    // This prevents currently casted (ranged) spells to be re-directed to the died bot's ghost.

    // If the bot already is a spirit, reset release time and return true
    if (bot->HasPlayerFlag(PLAYER_FLAGS_GHOST))
    {
        botAI->bgReleaseAttemptTime = 0;
        return true;
    }

    // Delay release to spirit.
    const time_t now = time(nullptr);
    constexpr time_t RELEASE_DELAY = 6;

    if (botAI->bgReleaseAttemptTime == 0)
        botAI->bgReleaseAttemptTime = now;

    if (now - botAI->bgReleaseAttemptTime < RELEASE_DELAY)
        return false;

    botAI->bgReleaseAttemptTime = 0;
    return true;
}

bool RepopAction::Execute(Event /*event*/)
{
    // PLB-LOCAL(66dbf2793b3a): fix(recovery): stop reporting a waiting ghost as a failed revive.
    // Upstream: const GraveyardStruct* graveyard = GetGrave( AI_VALUE(uint32, "death count") > 10 || CalculateDeadTi...
    const GraveyardStruct* graveyard =
        GetGrave(AI_VALUE(uint32, "death count") > 10 || CalculateDeadTime() > 30 * MINUTE);

    if (!graveyard)
        return false;

    PerformGraveyardTeleport(graveyard);
    return true;
}

// PLB-LOCAL(66dbf2793b3a): fix(recovery): stop reporting a waiting ghost as a failed revive.
// Upstream: bool RepopAction::isUseful() { return !bot->InBattleground(); } (base 8d9f6aa6bc6d).
bool RepopAction::isUseful() { return !bot->InBattleground(); }

int64 RepopAction::CalculateDeadTime() const
{
    if (Corpse* corpse = bot->GetCorpse())
        return time(nullptr) - corpse->GetGhostTime();

    return bot->isDead() ? 0 : 60 * MINUTE;
}

void RepopAction::PerformGraveyardTeleport(const GraveyardStruct* graveyard) const
{
    bot->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TELEPORTED | AURA_INTERRUPT_FLAG_CHANGE_MAP);
    bot->TeleportTo(graveyard->Map, graveyard->x, graveyard->y, graveyard->z, 0.f);
    RESET_AI_VALUE(bool, "combat::self target");
    RESET_AI_VALUE(WorldPosition, "current position");
}

// SelfResurrectAction implementation for Warlock's Soulstone Resurrection/Shaman's Reincarnation
bool SelfResurrectAction::Execute(Event /*event*/)
{
    if (!bot->IsAlive() && bot->GetUInt32Value(PLAYER_SELF_RES_SPELL))
    {
        WorldPacket packet(CMSG_SELF_RES);
        bot->GetSession()->HandleSelfResOpcode(packet);
        // PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
        bool const success = bot->IsAlive();
        if (success)
            botAI->StartPostReviveRepairSafety();
        // PLB-LOCAL(revive-outcome): same verdict as upstream, expressed as an outcome.
        botAI->RecordReviveAttempt(GameTime::GetGameTimeMS().count(),
                                   success ? PlayerbotReviveOutcome::Succeeded : PlayerbotReviveOutcome::Failed,
                                   bot->IsAlive());
        return success;
    }
    return false;
}
// PLB-LOCAL(66dbf2793b3a): fix(recovery): stop reporting a waiting ghost as a failed revive.
// Upstream: bool SelfResurrectAction::isUseful() { return !bot->IsAlive() && bot->GetUInt32Value(PLAYER_SELF_RES_SPE...
bool SelfResurrectAction::isUseful() { return !bot->IsAlive() && bot->GetUInt32Value(PLAYER_SELF_RES_SPELL); }
