/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE (soul-shard-cap). Not present upstream, so it can never conflict on a merge.
 *
 * How many soul shards a warlock keeps before it destroys the surplus.
 *
 * Upstream fires TooManySoulShardsTrigger at 26. A soul shard does not stack, so 26 shards is 26
 * bag slots, which is MORE than the entire 16-slot backpack of a warlock that owns no bags. For
 * those bots the trigger cannot fire at all: the bag fills first and looting stops. Measured live
 * 2026-09-03 at 200 bots: all 22 online warlocks were carrying shards, 120 slots between them,
 * 5.5 each, while 43 bots of the population had no bag beyond the backpack.
 *
 * A bot with a real master keeps upstream's 26, because a player directing a warlock may well want
 * a deep reserve before a raid and that bot's bags are the player's problem. A masterless random
 * bot keeps 5, which covers Healthstone, Soulstone and a summon with room to spare, and Pierre
 * set that number on 2026-09-03.
 */

#ifndef _PLAYERBOT_SOULSHARDPOLICY_H
#define _PLAYERBOT_SOULSHARDPOLICY_H

#include "Define.h"

inline constexpr uint32 SOUL_SHARD_CAP_MASTERLESS = 5;
inline constexpr uint32 SOUL_SHARD_CAP_WITH_MASTER = 26;

[[nodiscard]] inline uint32 SoulShardCap(bool hasRealMaster)
{
    return hasRealMaster ? SOUL_SHARD_CAP_WITH_MASTER : SOUL_SHARD_CAP_MASTERLESS;
}

// True when the warlock is holding more shards than its cap allows, so the surplus is destroyed.
// At the cap exactly nothing is destroyed: the cap is what the bot is allowed to keep.
[[nodiscard]] inline bool HoldingTooManySoulShards(uint32 shardCount, bool hasRealMaster)
{
    return shardCount > SoulShardCap(hasRealMaster);
}

#endif
