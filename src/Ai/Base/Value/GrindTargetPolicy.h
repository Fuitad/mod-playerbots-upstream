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
    return candidate.distance < incumbent.distance;
}

#endif
