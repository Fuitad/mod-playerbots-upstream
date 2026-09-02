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

#include <variant>

namespace DeathProbe
{
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
    DeathProbeScript()
        : PlayerScript("DeathProbeScript", {PLAYERHOOK_ON_PLAYER_JUST_DIED, PLAYERHOOK_ON_PLAYER_KILLED_BY_CREATURE})
    {
    }

    void OnPlayerJustDied(Player* player) override
    {
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(player);
        if (!botAI || !sRandomPlayerbotMgr.IsRandomBot(player))
            return;
        uint32 const questId = DeathProbe::QuestInProgress(botAI);
        LOG_DEBUG("playerbots",
                  "[DeathProbe] {} DIED lvl {} class {} zone {} area {} rpg {} quest {} broken {} money {}c t {}",
                  player->GetName(), player->GetLevel(), player->getClass(), player->GetZoneId(), player->GetAreaId(),
                  static_cast<uint32>(botAI->rpgInfo.GetStatus()), questId, DeathProbe::BrokenEquipmentSlots(player),
                  player->GetMoney(), getMSTime());
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
            LOG_DEBUG("playerbots", "[DeathProbe] {} DEATH-COOLDOWN quest {} deaths {} blamed {}", player->GetName(),
                      questId, record.deaths, QuestStayLostToDeaths(record.deaths));
        }
    }

    void OnPlayerKilledByCreature(Creature* killer, Player* killed) override
    {
        if (!killer || !killed || !GET_PLAYERBOT_AI(killed) || !sRandomPlayerbotMgr.IsRandomBot(killed))
            return;
        LOG_DEBUG("playerbots", "[DeathProbe] {} KILLEDBY {} entry {} lvl {} rank {} botlvl {} attackers {}",
                  killed->GetName(), killer->GetName(), killer->GetEntry(), killer->GetLevel(),
                  killer->GetCreatureTemplate() ? killer->GetCreatureTemplate()->rank : 0, killed->GetLevel(),
                  killed->getAttackers().size());
    }
};

#endif
