/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * Temporary diagnostic: one log line per random bot death naming what killed it and what the bot
 * was doing. Measured live 2026-09-01: 100 revives in the first 13 minutes after a restart, most
 * of them with no quest stay in progress (the death-rotate fix only covers a stay), and nothing in
 * the log says whether the killer was an elite, a level gap, a broken weapon, or a walk through a
 * hostile zone. Remove once the death cause is settled.
 */

#ifndef _PLAYERBOT_DEATHPROBE_H
#define _PLAYERBOT_DEATHPROBE_H

#include <variant>

#include "Ai/Base/Actions/DeathRecoveryPolicy.h"
#include "Ai/World/Rpg/CampPullPolicy.h"
#include "Ai/World/Rpg/DeathProbeState.h"
#include "Ai/World/Rpg/FightLedgers.h"
#include "Ai/World/Rpg/QuestDeathCooldown.h"
#include "Ai/World/Rpg/QuestDropPolicy.h"
#include "Creature.h"
#include "CreatureData.h"
#include "Item.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "ScriptMgr.h"
#include "Timer.h"

namespace DeathProbe
{
inline std::vector<uint16> EnabledPlayerHooks()
{
    return {PLAYERHOOK_ON_PLAYER_ENTER_COMBAT, PLAYERHOOK_ON_PLAYER_LEAVE_COMBAT, PLAYERHOOK_ON_PLAYER_JUST_DIED,
            PLAYERHOOK_ON_PLAYER_KILLED_BY_CREATURE};
}

inline uint32 BrokenEquipmentSlots(Player* bot)
{
    uint32 broken = 0;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (item && item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY) > 0 &&
            item->GetUInt32Value(ITEM_FIELD_DURABILITY) == 0)
            ++broken;
    }
    return broken;
}

inline uint32 QuestInProgress(PlayerbotAI* botAI)
{
    if (auto const* doQuest = std::get_if<NewRpgInfo::DoQuest>(&botAI->rpgInfo.data))
        return doQuest->questId;
    return 0;
}
}  // namespace DeathProbe

class DeathProbeScript : public PlayerScript
{
public:
    DeathProbeScript() : PlayerScript("DeathProbeScript", DeathProbe::EnabledPlayerHooks()) {}

    void OnPlayerJustDied(Player* player) override
    {
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(player);
        if (!botAI || !sRandomPlayerbotMgr.IsRandomBot(player))
            return;
        uint32 const questId = DeathProbe::QuestInProgress(botAI);
        // Power at death: a caster that died dry lost the fight to its mana, one that died full
        // never cast (33 of 79 deaths on 2026-09-02 02:08 to 02:22 were to mobs two or more
        // levels below the bot, 17 of them priests and shamans).
        Powers const powerType = player->getPowerType();
        uint32 const powerPct =
            player->GetMaxPower(powerType) ? player->GetPower(powerType) * 100 / player->GetMaxPower(powerType) : 0;
        LOG_DEBUG("playerbots",
                  "[DeathProbe] {} DIED lvl {} class {} zone {} area {} rpg {} quest {} broken {} money {}c power {}% "
                  "t {} at {:.0f},{:.0f},{:.0f} map {}",
                  player->GetName(), player->GetLevel(), player->getClass(), player->GetZoneId(), player->GetAreaId(),
                  static_cast<uint32>(botAI->rpgInfo.GetStatus()), questId, DeathProbe::BrokenEquipmentSlots(player),
                  player->GetMoney(), powerPct, getMSTime(), player->GetPositionX(), player->GetPositionY(),
                  player->GetPositionZ(), player->GetMapId());
        // The chain record the corpse walk consults (DeathRecoveryPolicy.h). The killer's level gap
        // arrives through OnPlayerKilledByCreature just before this hook; an environmental death
        // leaves it at zero.
        uint32 const guidLow = player->GetGUID().GetCounter();
        // PLB-LOCAL(environmental-death): no creature hook fired, so the world itself killed the bot;
        // the corpse lies where it will happen again. See RecentDeathRecord::lastDeathEnvironmental.
        auto const pendingKillerGap = _state.TakeKillerGap(guidLow);
        bool const environmental = !pendingKillerGap;
        int32 const killerGap = pendingKillerGap.value_or(0);
        RecentDeathRecord const chain = RecentDeaths::Note(guidLow, getMSTime(), killerGap, environmental);
        LOG_DEBUG("playerbots", "[DeathProbe] {} DEATH-CHAIN deaths {} killerGap {} environmental {} homebind {}",
                  player->GetName(), chain.deathsInWindow, killerGap, environmental,
                  RecoverAtHomebindAfterDeath(chain.deathsInWindow, killerGap, environmental));
        // PLB-LOCAL(drowning-alarm): a bot the world killed in water, or outside any zone, drowned or
        // swam to exhaustion; nothing sends a bot there on purpose. Eight bots died that way in open
        // ocean on map 530 on the morning of 2026-09-11 (a gathering leg to a node on the other isle
        // group). Pierre: any bot drowning should raise an alarm. Error level so it reaches the
        // console log, not only the debug file.
        if (environmental && (player->IsInWater() || player->GetZoneId() == 0))
        {
            LOG_ERROR("playerbots", "[DeathProbe] {} DROWNED lvl {} rpg {} quest {} at {:.0f},{:.0f},{:.0f} map {} zone {}",
                      player->GetName(), player->GetLevel(), static_cast<uint32>(botAI->rpgInfo.GetStatus()), questId,
                      player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetMapId(),
                      player->GetZoneId());
        }
        // The quest death cooldown is recorded here, at the death, not from the quest stay tick:
        // after the revive the bot re-reaches the anchor and the stay's death count starts over,
        // so the stay never saw its own death (0 ABANDON-DEATH lines all night on 2026-09-01).
        // See QuestDeathCooldown.h. The second death inside the cooldown carries the blame.
        if (questId)
        {
            QuestDeathRecord const record =
                QuestDeathCooldown::Note(player->GetGUID().GetCounter(), questId, getMSTime());
            if (QuestStayLostToDeaths(record.deaths))
            {
                botAI->lowPriorityQuest.insert(questId);
                botAI->rpgStatistic.questAbandoned++;
            }
            // The cooldown only governs the next pick. A bot revived at its body was still in the
            // same quest stay and walked back into the same mob without picking anything: 14 of
            // 25 body relapses on 2026-09-02 00:42 to 00:56 were same quest, same status. Ending
            // the stay here makes the revived bot pick afresh, and the pick skips the cooled quest.
            botAI->rpgInfo.ChangeToIdle();
            LOG_DEBUG("playerbots", "[DeathProbe] {} DEATH-COOLDOWN quest {} deaths {} blamed {} stay ended",
                      player->GetName(), questId, record.deaths, QuestStayLostToDeaths(record.deaths));
        }
        // PLB-LOCAL(fight-report): what the last fight looked like. See FightReportPolicy.h.
        {
            FightLedger const fight = FightLedgers::Snapshot(guidLow);
            LOG_DEBUG("playerbots",
                      "[DeathProbe] {} FIGHT secs {} hp {}% hits {} dealt {} taken {} actions ok {} fail {} "
                      "topfail {} verdict {}",
                      player->GetName(), fight.startMs ? GetMSTimeDiffToNow(fight.startMs) / 1000 : 0,
                      fight.startHealthPct, fight.hits, fight.dealt, fight.taken, fight.actionsOk, fight.actionsFailed,
                      TopFightFailure(fight), FightVerdictName(ClassifyFight(fight)));
            FightLedgers::Close(guidLow);
        }
        // OnPlayerKilledByCreature runs before this later corpse transition and consumes the
        // engagement for creature deaths. Clear anything left by an environmental death here.
        _state.ClearEngagement(guidLow);
    }

    // The fight's first target, so a death can say whether the killer came out of the same camp.
    // See CampPullPolicy.h. Combat is re-entered mid-fight often enough (a second attacker joins,
    // the bot drops out for a tick) that the first engagement is kept until combat ends rather than
    // overwritten on every entry.
    void OnPlayerEnterCombat(Player* player, Unit* enemy) override
    {
        if (!player || !enemy || !GET_PLAYERBOT_AI(player) || !sRandomPlayerbotMgr.IsRandomBot(player))
            return;

        // PLB-LOCAL(fight-report): the ledger opens with the fight and closes with it.
        FightLedgers::Open(player->GetGUID().GetCounter(), getMSTime(), static_cast<uint32>(player->GetHealthPct()));

        FirstEngagement const incoming{
            .entry = enemy->GetEntry(),
            .x = enemy->GetPositionX(),
            .y = enemy->GetPositionY(),
            .z = enemy->GetPositionZ(),
            .guidLow = enemy->GetGUID().GetCounter(),
            .since = time(nullptr),
        };
        _state.NoteEngagement(player->GetGUID().GetCounter(), incoming);
    }

    void OnPlayerLeaveCombat(Player* player) override
    {
        if (!player || !ShouldClearEngagementOnLeaveCombat(player->IsAlive()))
            return;
        _state.ClearEngagement(player->GetGUID().GetCounter());
        // PLB-LOCAL(fight-report): a fight the bot survived needs no report.
        FightLedgers::Close(player->GetGUID().GetCounter());
    }

    void OnPlayerKilledByCreature(Creature* killer, Player* killed) override
    {
        if (!killer || !killed || !GET_PLAYERBOT_AI(killed) || !sRandomPlayerbotMgr.IsRandomBot(killed))
            return;
        _state.NoteKillerGap(killed->GetGUID().GetCounter(),
                             static_cast<int32>(killer->GetLevel()) - static_cast<int32>(killed->GetLevel()));

        // Where the killer came from, relative to the fight the bot chose to start. The untouched
        // share alone cannot separate a camp pull from a wanderer, and only the camp pull is
        // answerable by ranking grind candidates differently. See CampPullPolicy.h.
        auto const engagement = _state.TakeEngagement(killed->GetGUID().GetCounter());
        bool const haveFirst = engagement && engagement->since != 0;
        float distance = -1.0f;
        float aggroRange = 0.0f;
        bool killerIsFirst = false;
        if (haveFirst)
        {
            killerIsFirst = killer->GetGUID().GetCounter() == engagement->guidLow;
            distance = killer->GetDistance(engagement->x, engagement->y, engagement->z);
            aggroRange = killer->GetAggroRange(killed);
        }
        KillerOrigin const origin = ClassifyKiller(haveFirst, killerIsFirst, distance, aggroRange);

        // The killer's remaining health says how the fight went: near full, the bot never hurt it.
        LOG_DEBUG("playerbots",
                  "[DeathProbe] {} KILLEDBY {} entry {} lvl {} rank {} botlvl {} killerHealth {}% "
                  "origin {} firstEntry {} firstDist {:.1f} aggroReach {:.1f}",
                  killed->GetName(), killer->GetName(), killer->GetEntry(), killer->GetLevel(),
                  killer->GetCreatureTemplate() ? killer->GetCreatureTemplate()->rank : 0, killed->GetLevel(),
                  static_cast<uint32>(killer->GetHealthPct()), KillerOriginName(origin),
                  haveFirst ? engagement->entry : 0, distance, aggroRange);
    }

private:
    DeathProbeState _state;
};

// PLB-LOCAL(fight-report): damage dealt and taken by random bots, into the open fight ledger.
// Registered next to DeathProbeScript; the ledger is opened by the player combat hooks above.
class FightDamageProbeScript : public UnitScript
{
public:
    FightDamageProbeScript() : UnitScript("FightDamageProbeScript", true, {UNITHOOK_ON_DAMAGE}) {}

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (attacker && attacker->IsPlayer() && IsTrackedBot(attacker->ToPlayer()))
            FightLedgers::Dealt(attacker->GetGUID().GetCounter(), damage);
        if (victim && victim->IsPlayer() && IsTrackedBot(victim->ToPlayer()))
            FightLedgers::Taken(victim->GetGUID().GetCounter(), damage);
    }

private:
    static bool IsTrackedBot(Player* player)
    {
        return player && GET_PLAYERBOT_AI(player) && sRandomPlayerbotMgr.IsRandomBot(player);
    }
};

#endif
