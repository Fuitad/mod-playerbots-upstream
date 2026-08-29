/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * It owns one decision: which nearby gameobject, if any, a bot standing at a quest POI should
 * operate to advance its current objective, and how (use it directly, or queue it for the loot
 * pipeline).
 */

#ifndef _PLAYERBOT_QUESTGAMEOBJECTPOLICY_H
#define _PLAYERBOT_QUESTGAMEOBJECTPOLICY_H

#include "Define.h"

#include <cstddef>
#include <vector>

// How a chosen gameobject is operated. Chests go through the existing loot pipeline
// (LootObjectStack -> LootAction -> OpenLootAction), which owns locks, skills and bag space.
// Goobers are used directly (CMSG_GAMEOBJ_USE -> GameObject::Use -> Player::KillCreditGO);
// they carry no loot id (GameObjectTemplate::GetLootId covers CHEST and FISHINGHOLE only), so
// the loot pipeline can never reach them. Everything else is left alone: questgivers belong to
// SearchQuestGiverAndAcceptOrReward, and no other type appears in a low-level RequiredNpcOrGo
// objective.
enum class QuestGoInteraction : uint8
{
    Skip = 0,
    Use,
    Loot,
};

// quest_template.RequiredNpcOrGo: > 0 names a creature to kill, < 0 names a gameobject entry
// (same sign convention as Player::HasQuestForGO). Only the negative case is ours.
[[nodiscard]] inline uint32 QuestObjectiveGoEntry(int32 requiredNpcOrGo)
{
    return requiredNpcOrGo < 0 ? static_cast<uint32>(-requiredNpcOrGo) : 0u;
}

[[nodiscard]] inline QuestGoInteraction QuestGoInteractionForType(bool isGoober, bool isChest)
{
    if (isGoober)
        return QuestGoInteraction::Use;
    if (isChest)
        return QuestGoInteraction::Loot;
    return QuestGoInteraction::Skip;
}

// One nearby gameobject, reduced to the facts that decide whether it is worth walking to.
struct QuestGoCandidateFacts
{
    // Spawned, GO_STATE_READY, selectable, not in use, and ActivateToQuest(bot) - the same
    // per-player quest gate the client renders sparkles with.
    bool usable = false;
    // Entry match for a RequiredNpcOrGo objective, or a chest that can drop the needed item.
    bool matchesObjective = false;
    QuestGoInteraction interaction = QuestGoInteraction::Skip;
    // Squared distance from the bot: nearest eligible candidate wins.
    float distanceSq = 0.0f;
    // Squared distance from the POI the bot was sent to. Candidates beyond the anchor radius
    // are ignored so this seek can never fight the quest-poi-approach return radius
    // (QuestPoiApproachPolicy.h): walking to a gameobject must not pull the bot far enough
    // from its POI that the approach policy immediately drags it back, which would oscillate.
    float anchorDistanceSq = 0.0f;
};

inline constexpr size_t QUEST_GO_NO_CANDIDATE = ~static_cast<size_t>(0);

// Nearest usable candidate that matches the objective and has a defined interaction, within
// the anchor radius. maxAnchorDistanceSq <= 0 disables the anchor cap. Strict less-than keeps
// the scan stable: an equal candidate never displaces the incumbent.
[[nodiscard]] inline size_t BestQuestGoCandidateIndex(std::vector<QuestGoCandidateFacts> const& candidates,
                                                      float maxAnchorDistanceSq)
{
    size_t best = QUEST_GO_NO_CANDIDATE;
    float bestDistanceSq = -1.0f;
    for (size_t i = 0; i < candidates.size(); ++i)
    {
        QuestGoCandidateFacts const& candidate = candidates[i];
        if (!candidate.usable || !candidate.matchesObjective ||
            candidate.interaction == QuestGoInteraction::Skip)
            continue;
        if (maxAnchorDistanceSq > 0.0f && candidate.anchorDistanceSq > maxAnchorDistanceSq)
            continue;
        if (bestDistanceSq < 0.0f || candidate.distanceSq < bestDistanceSq)
        {
            bestDistanceSq = candidate.distanceSq;
            best = i;
        }
    }
    return best;
}

#endif
