/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE (quest-spell-focus). Not present upstream, so it can never conflict on a merge.
 *
 * An item objective whose item is MADE, not looted: the quest hands out a tool whose on-use spell
 * needs a spell focus object (GameObjectTemplate type 8), and casting it inside the focus reach
 * creates the objective item. Learning from the Crystals (9581) is the measured case: the Crystal
 * Mining Pick 23875 casts spell 30611, which requires focus 1380, the Impact Site Crystal 181779,
 * within 5 yards. Nothing drops the sample 23878 (zero rows in creature_questitem,
 * gameobject_questitem and every loot template), so the objective had no source, the stay stood
 * beside the crystal with "9 gameobjects nearby, 0 matching" and abandoned (Mihli, 2026-09-03,
 * 524 seconds).
 *
 * The core does the hard part: Spell::CheckSpellFocus finds the focus object itself and fails the
 * cast with SPELL_FAILED_REQUIRES_SPELL_FOCUS when the caster is out of reach. The bot never
 * targets the object. It walks inside the reach and self-uses the tool.
 */

#ifndef _PLAYERBOT_QUESTSPELLFOCUSPOLICY_H
#define _PLAYERBOT_QUESTSPELLFOCUSPOLICY_H

#include "Define.h"

#include <algorithm>

// The focus object stands in as the objective's source only when nothing loots the item. An item
// that also drops somewhere keeps its loot sources: a tool that happens to need a focus is not
// evidence that the focus makes THIS item.
[[nodiscard]] inline bool SpellFocusIsTheItemSource(bool hasLootSources, uint32 focusId)
{
    return !hasLootSources && focusId != 0;
}

// How close to walk. The core's reach test is GameObject::IsWithinDistInMap against the template's
// own distance, so the bot stops a yard inside it rather than on the edge, where a rounding
// difference between the walk target and the check would fail the cast.
[[nodiscard]] inline float SpellFocusApproachDistance(uint32 focusDist)
{
    return std::max(1.0f, static_cast<float>(focusDist) - 1.0f);
}

[[nodiscard]] inline bool WithinSpellFocusReach(float distance, uint32 focusDist)
{
    return distance <= SpellFocusApproachDistance(focusDist);
}

#endif
