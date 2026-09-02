/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * It owns one decision: which of two eligible grind candidates a bot should attack. The upstream
 * value keeps its own eligibility rules and only asks this file to rank what survived them, so the
 * edit in GrindTargetValue.cpp stays down to the comparison itself.
 */

#ifndef _PLAYERBOT_GRINDTARGETPOLICY_H
#define _PLAYERBOT_GRINDTARGETPOLICY_H

#include "Define.h"

// One eligible grind candidate, reduced to the two facts that decide between candidates.
struct GrindCandidateFacts
{
    // The bot holds an incomplete quest that this creature advances.
    bool neededForQuest = false;
    float distance = 0.0f;
    // Other hostile creatures standing within their own aggro reach of this one: the adds that
    // join the fight when it is pulled. Measured at 200 bots on 2026-09-02: four deaths in ten had
    // the killer untouched at the bot's death, 18 of 30 to creatures two or more levels below the
    // bot and 20 of 30 inside a quest stay; the bot pulled one mob out of a camp and its neighbours
    // finished the fight.
    uint32 hostileNeighbours = 0;
};

// Ranks one candidate against the incumbent best.
//
// Why this exists: a bot working a quest reaches the quest POI and then leaves the killing to the
// grind strategy. Upstream applies its needForQuest filter only to candidates OUTSIDE aggro range,
// so every creature within roughly 20 to 30 yards is eligible whatever the quest is, and the
// nearest one wins. At a POI where the objective is a minority of the local population, the bot
// spends its whole stay killing the wrong creatures, makes no objective progress, and the quest is
// abandoned after NewRpgDoQuestAction's five minute poiStayTime.
//
// Measured on the live realm on 2026-08-28: quests that were abandoned had the objective creature
// at 13.9% of the spawns within 60 yards of the POI, against 33.6% for quests that were turned in,
// and 43% of all resolved quest attempts ended in abandonment.
//
// So while the bot is working a quest, a quest-advancing candidate outranks a merely nearer one.
// Within the same class, and whenever no quest is being worked, nearest still wins, which leaves
// wandering and idle grinding exactly as upstream had it.
[[nodiscard]] inline bool GrindCandidatePreferred(GrindCandidateFacts const& candidate,
                                                  GrindCandidateFacts const& incumbent,
                                                  bool questPriorityActive, bool hasIncumbent)
{
    if (!hasIncumbent)
        return true;
    if (questPriorityActive && candidate.neededForQuest != incumbent.neededForQuest)
        return candidate.neededForQuest;
    // A creature with fewer hostile neighbours is the safer pull, whatever the distance; only
    // among equally crowded candidates does nearest win. See hostileNeighbours above.
    if (candidate.hostileNeighbours != incumbent.hostileNeighbours)
        return candidate.hostileNeighbours < incumbent.hostileNeighbours;
    return candidate.distance < incumbent.distance;
}

// Whether an out-of-aggro candidate must be quest-relevant to stay eligible at all.
//
// Upstream demands quest relevance for every out-of-aggro candidate whenever the bot is in any
// focused RPG status, which includes the whole POI stay of RPG_DO_QUEST. Measured on the live
// realm on 2026-08-29: 71% of abandoned quests saw ZERO creature kills during the entire five
// minute stay, and the largest zero-kill bucket (19 of 35) had visible candidates with grind
// target selection returning null - the bot stood in a field of creatures it refused to attack
// because none advanced the quest, and only fought when something aggroed it first.
//
// So once the bot has REACHED its POI (the stay is running), any eligible creature in range may
// be ground: the quest-priority ranking above still puts objective creatures first whenever they
// exist, so this only stops the bot idling when they do not. While still travelling to the POI,
// and in every other focused status, upstream's relevance requirement stands unchanged.
[[nodiscard]] inline bool GrindCandidateNeedsQuestRelevance(bool inactiveGrindStatus, bool outOfAggro,
                                                            bool stayingAtQuestPoi)
{
    return inactiveGrindStatus && outOfAggro && !stayingAtQuestPoi;
}

// Whether a creature that yields no experience (gray to the bot) may still be ground.
//
// Upstream drops every non-XP creature before any other check. A bot working a low quest it
// still holds (the lowest-level-first picker sends it there on purpose) then stands among the
// objective creatures and attacks none of them: measured live 2026-09-01, Skirmish at Echo Ridge
// (quest level 5, Kobold Laborers level 3) at bot level 10 ended four stays with 16 to 50 targets
// in range and zero kills. A creature the bot's quest needs is eligible whatever its level.
[[nodiscard]] inline bool GrindCandidateGrayEligible(bool yieldsExperience, bool neededForQuest)
{
    return yieldsExperience || neededForQuest;
}

#endif
