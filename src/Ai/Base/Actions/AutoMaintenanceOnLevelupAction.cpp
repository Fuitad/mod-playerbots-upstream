/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL UPSTREAM-FILE: this fork changes 5 region(s) of this upstream file.
// Each is tagged PLB-LOCAL(<sha>) where a marker could be placed safely; run
// tools/plb_local_markers.py --check for the authoritative list. docs/local-changes.md.

#include "AutoMaintenanceOnLevelupAction.h"
#include "BroadcastHelper.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotFactory.h"
// PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
#include "RandomBotMaintenancePolicy.h"
#include "RandomPlayerbotMgr.h"
#include "SharedDefines.h"
#include "SpellMgr.h"

bool AutoMaintenanceOnLevelupAction::Execute(Event /*event*/)
{
    AutoPickTalents();
    AutoLearnSpell();
    AutoTeleportForLevel();
    AutoUpgradeEquip();

    return true;
}

void AutoMaintenanceOnLevelupAction::AutoTeleportForLevel()
{
    if (!sPlayerbotAIConfig.autoTeleportForLevel || !sRandomPlayerbotMgr.IsRandomBot(bot))
        return;

    if (botAI->HasGameClientMaster())
        return;

    sRandomPlayerbotMgr.RandomTeleportForLevel(bot);
    return;
}

void AutoMaintenanceOnLevelupAction::AutoPickTalents()
{
    if (!sPlayerbotAIConfig.autoPickTalents || !sRandomPlayerbotMgr.IsRandomBot(bot))
        return;

    if (bot->GetFreeTalentPoints() <= 0)
        return;

    PlayerbotFactory factory(bot, bot->GetLevel());
    factory.InitTalentsTree(true, true, true);
    factory.InitPetTalents();
}

void AutoMaintenanceOnLevelupAction::AutoLearnSpell()
{
    std::ostringstream out;
    LearnSpells(&out);

    if (!out.str().empty())
    {
        std::string const temp = out.str();
        out.seekp(0);
        out << "Learned spells: ";
        out << temp;
        out.seekp(-2, out.cur);
        out << ".";
        botAI->TellMaster(out);
    }
    return;
}

void AutoMaintenanceOnLevelupAction::LearnSpells(std::ostringstream* out)
{
    BroadcastHelper::BroadcastLevelup(botAI, bot);
    if (sPlayerbotAIConfig.autoLearnTrainerSpells && sRandomPlayerbotMgr.IsRandomBot(bot))
        LearnTrainerSpells(out);

    if (sPlayerbotAIConfig.autoLearnQuestSpells && sRandomPlayerbotMgr.IsRandomBot(bot))
        LearnQuestSpells(out);
}

void AutoMaintenanceOnLevelupAction::LearnTrainerSpells(std::ostringstream* /*out*/)
{
    PlayerbotFactory factory(bot, bot->GetLevel());
    factory.InitSkills();
    factory.InitClassSpells();
    factory.InitAvailableSpells();
    factory.InitPet();
}

void AutoMaintenanceOnLevelupAction::LearnQuestSpells(std::ostringstream* out)
{
    ObjectMgr::QuestMap const& questTemplates = sObjectMgr->GetQuestTemplates();
    for (ObjectMgr::QuestMap::const_iterator i = questTemplates.begin(); i != questTemplates.end(); ++i)
    {
        Quest const* quest = i->second;

        if (!quest->GetRequiredClasses() || quest->IsRepeatable() || quest->GetMinLevel() < 10 ||
            quest->GetMinLevel() > bot->GetLevel())
        {
            continue;
        }

        if (!bot->SatisfyQuestClass(quest, false) || !bot->SatisfyQuestRace(quest, false) ||
            !bot->SatisfyQuestSkill(quest, false))
        {
            continue;
        }

        int32 spellId = quest->GetRewSpellCast();
        if (!spellId)
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            continue;

        bool found = false;
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            if (spellInfo->Effects[i].Effect == SPELL_EFFECT_LEARN_SPELL && spellInfo->Effects[i].TriggerSpell &&
                !bot->HasSpell(spellInfo->Effects[i].TriggerSpell))
            {
                if (SpellInfo const* triggeredInfo = sSpellMgr->GetSpellInfo(spellInfo->Effects[i].TriggerSpell))
                    if (triggeredInfo->Effects[0].Effect == SPELL_EFFECT_TRADE_SKILL)
                        break;

                found = true;
                break;
            }
        }

        if (!found)
            continue;

        bot->CastSpell(bot, spellId, true);

        uint32 rewSpellId = quest->GetRewSpell();
        if (rewSpellId)
        {
            if (SpellInfo const* rewSpellInfo = sSpellMgr->GetSpellInfo(rewSpellId))
            {
                *out << FormatSpell(rewSpellInfo) << ", ";
                continue;
            }
        }

        *out << FormatSpell(spellInfo) << ", ";
    }
}

std::string const AutoMaintenanceOnLevelupAction::FormatSpell(SpellInfo const* sInfo)
{
    std::ostringstream out;
    std::string const rank = sInfo->Rank[0];

    if (rank.empty())
        out << "|cffffffff|Hspell:" << sInfo->Id << "|h[" << sInfo->SpellName[LOCALE_enUS] << "]|h|r";
    else
        out << "|cffffffff|Hspell:" << sInfo->Id << "|h[" << sInfo->SpellName[LOCALE_enUS] << " " << rank << "]|h|r";

    return out.str();
}

void AutoMaintenanceOnLevelupAction::AutoUpgradeEquip()
{
    // PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
    playerbots::maintenance::LevelupMaintenancePlan const plan =
        playerbots::maintenance::BuildLevelupMaintenancePlan(sRandomPlayerbotMgr.IsRandomBot(bot),
                                                             sPlayerbotAIConfig.economyManagedSupplies,
                                                             sPlayerbotAIConfig.autoUpgradeEquip);
    if (!plan.cleanupConsumables && !plan.provisionConsumables && !plan.upgradeEquipment)
        return;

    PlayerbotFactory factory(bot, bot->GetLevel());

    // PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
    if (plan.cleanupConsumables)
        factory.CleanupConsumables();

    // PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
    if (plan.provisionConsumables)
    {
        factory.InitAmmo();
        factory.InitReagents();
        factory.InitFood();
        factory.InitConsumables();
        factory.InitPotions();
    }

    // PLB-LOCAL(ffd415a247b8): fix(recovery): make random bot revival safe and truthful
    if (plan.upgradeEquipment)
        factory.InitEquipment(true);
}
