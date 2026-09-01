/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * Which bag stack to give up so a completed quest's reward can be stored. Player::CanRewardQuest
 * refuses when the reward item cannot be placed, and the reward stay then times out and marks the
 * quest low priority for the life of the process. Measured live 2026-09-01: Hemewmew, Botanical
 * Legwork complete, 16 of 16 backpack slots used by white gathering materials the vendor trip
 * never sells (they are auction-classified), 300s beside eight quest givers, abandoned.
 */

#ifndef _PLAYERBOT_QUESTREWARDBAGPOLICY_H
#define _PLAYERBOT_QUESTREWARDBAGPOLICY_H

#include <cstddef>
#include <cstdint>
#include <vector>

// One bag stack, reduced to the facts that decide whether it may be sacrificed and how cheaply.
struct BagStackFacts
{
    // The bot needs it for a quest, wears it, or the usage classifier wants it kept or used.
    bool protectedUsage = false;
    // Soulbound or bind-on-pickup: worthless to anyone else, but also the sign of gear or a quest
    // reward the bot chose; never a sacrifice candidate.
    bool bound = false;
    // Vendor sell price of one unit times the stack count, in copper.
    uint32_t sellValue = 0;
    // True for vendor trash and unclassified items, which go before auction-grade materials.
    bool trash = false;
};

inline constexpr size_t QUEST_REWARD_NO_VICTIM = ~static_cast<size_t>(0);

// Index of the stack to destroy, or QUEST_REWARD_NO_VICTIM when nothing may be sacrificed. Trash
// and unclassified stacks go first, then auction-grade materials, cheapest first, so a hoarding
// gatherer loses one stack of eggs rather than a quest item or the piece it is wearing.
[[nodiscard]] inline size_t ChooseBagStackToSacrifice(std::vector<BagStackFacts> const& stacks)
{
    size_t best = QUEST_REWARD_NO_VICTIM;
    for (size_t i = 0; i < stacks.size(); ++i)
    {
        BagStackFacts const& s = stacks[i];
        if (s.protectedUsage || s.bound)
            continue;
        if (best == QUEST_REWARD_NO_VICTIM)
        {
            best = i;
            continue;
        }
        BagStackFacts const& b = stacks[best];
        if (s.trash != b.trash)
        {
            if (s.trash)
                best = i;
            continue;
        }
        if (s.sellValue < b.sellValue)
            best = i;
    }
    return best;
}

#endif
