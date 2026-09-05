/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE (fight-report). Not present upstream, so it can never conflict on a merge.
 *
 * The per-bot fight ledgers behind the death fight report (FightReportPolicy.h). Small on purpose so
 * the engine's action dispatch can include it without pulling the death probe in.
 */

#ifndef _PLAYERBOT_FIGHTLEDGERS_H
#define _PLAYERBOT_FIGHTLEDGERS_H

#include <mutex>
#include <string>
#include <unordered_map>

#include "Define.h"
#include "FightReportPolicy.h"

namespace FightLedgers
{
inline std::mutex& Mutex()
{
    static std::mutex mutex;
    return mutex;
}

inline std::unordered_map<uint32, FightLedger>& Map()
{
    static std::unordered_map<uint32, FightLedger> ledgers;
    return ledgers;
}

// Open a fight for the bot unless one is already open: combat is re-entered mid-fight often.
inline void Open(uint32 botLow, uint32 nowMs, uint32 healthPct)
{
    std::lock_guard<std::mutex> lock(Mutex());
    FightLedger& ledger = Map()[botLow];
    if (!ledger.startMs)
    {
        ledger = FightLedger{};
        ledger.startMs = nowMs ? nowMs : 1u;
        ledger.startHealthPct = healthPct;
    }
}

inline void Close(uint32 botLow)
{
    std::lock_guard<std::mutex> lock(Mutex());
    Map().erase(botLow);
}

inline void Dealt(uint32 botLow, uint32 damage)
{
    std::lock_guard<std::mutex> lock(Mutex());
    auto const it = Map().find(botLow);
    if (it != Map().end())
        NoteFightDamageDealt(it->second, damage);
}

inline void Taken(uint32 botLow, uint32 damage)
{
    std::lock_guard<std::mutex> lock(Mutex());
    auto const it = Map().find(botLow);
    if (it != Map().end())
        NoteFightDamageTaken(it->second, damage);
}

inline void Action(uint32 botLow, std::string const& action, bool success)
{
    std::lock_guard<std::mutex> lock(Mutex());
    auto const it = Map().find(botLow);
    if (it != Map().end())
        NoteFightAction(it->second, action, success);
}

// A copy of the open ledger, or an empty one (startMs 0) when no fight is open.
[[nodiscard]] inline FightLedger Snapshot(uint32 botLow)
{
    std::lock_guard<std::mutex> lock(Mutex());
    auto const it = Map().find(botLow);
    return it == Map().end() ? FightLedger{} : it->second;
}
}  // namespace FightLedgers

#endif
