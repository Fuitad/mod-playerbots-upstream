/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 *
 * It owns one decision: whether a bot may pursue a quest POI that lies outside the zone it is
 * standing in, and how such a POI ranks against a local one. The edit in NewRpgBaseAction.cpp is
 * kept down to gathering the facts and calling in here.
 */

#ifndef _PLAYERBOT_QUESTPOIREACHPOLICY_H
#define _PLAYERBOT_QUESTPOIREACHPOLICY_H

#include "Define.h"

// Upstream refused every quest POI outside the bot's current zone, beyond 1500 yards, or on another
// map, and then idled the quest. A quest whose objective sits in the next zone could therefore never
// be worked and never be abandoned: it occupied a log slot permanently. The same three filters ran on
// the turn-in path, so a COMPLETED quest whose giver was one zone away could never be handed in.
//
// Measured on the live realm on 2026-08-28: 210 of 1756 held incomplete quests, 12%, failed the map
// and distance test alone across 200 bots, and that is a floor because the zone test is stricter
// still and could not be evaluated offline.
enum class QuestPoiReach : uint8
{
    // In the bot's own zone and close by. Always workable, and preferred.
    Local,
    // Another zone on the same map, close enough to walk, and the destination is level appropriate.
    DistantSafe,
    // Another zone whose area level is above the bot's. Refused: the bot would die crossing.
    DistantUnsafe,
    // Same map but past the walking range.
    TooFar,
    // Another map. Refused here; reaching one needs a boat, zeppelin or portal, which quest pursuit
    // does not yet drive.
    WrongMap
};

struct QuestPoiReachFacts
{
    bool sameMap = false;
    bool sameZone = false;
    float distanceYards = 0.0f;
    // AreaTableEntry::area_level of the destination. Zero means the area declares none.
    uint32 destinationAreaLevel = 0;
    uint32 botLevel = 1;
    bool botAtMaxLevel = false;
    // Upstream's own local radius.
    float localRangeYards = 1500.0f;
    // Walking range for a cross-zone trip. 2500 is the cap already used elsewhere in this codebase
    // for a bot walking to a location, so a cross-zone quest trip is no longer than trips the bot
    // already makes.
    float travelRangeYards = 2500.0f;
};

// The level gate deliberately mirrors ExploreTravelDestination::isActive in TravelMgr.cpp: an area
// whose declared level exceeds the bot's is off limits until max level. Reusing that rule keeps one
// definition of "too dangerous for this bot" rather than inventing a second.
[[nodiscard]] inline QuestPoiReach ClassifyQuestPoiReach(QuestPoiReachFacts const& facts)
{
    if (!facts.sameMap)
        return QuestPoiReach::WrongMap;
    if (facts.sameZone && facts.distanceYards < facts.localRangeYards)
        return QuestPoiReach::Local;
    if (facts.distanceYards >= facts.travelRangeYards)
        return QuestPoiReach::TooFar;
    if (facts.destinationAreaLevel && facts.destinationAreaLevel > facts.botLevel && !facts.botAtMaxLevel)
        return QuestPoiReach::DistantUnsafe;
    return QuestPoiReach::DistantSafe;
}

[[nodiscard]] inline bool QuestPoiAdmissible(QuestPoiReach reach)
{
    return reach == QuestPoiReach::Local || reach == QuestPoiReach::DistantSafe;
}

// Local work is always taken before a trip, so a bot only walks to another zone once nothing nearby
// is left. That is also the fallback when a distant objective is refused as unsafe: the bot keeps
// working whatever is local instead of idling.
[[nodiscard]] inline bool QuestPoiPreferred(QuestPoiReach candidate, float candidateDistance, QuestPoiReach incumbent,
                                            float incumbentDistance, bool hasIncumbent)
{
    if (!hasIncumbent)
        return true;
    if (candidate != incumbent)
        return candidate == QuestPoiReach::Local;
    return candidateDistance < incumbentDistance;
}

// Which quest in the log to work on next.
//
// Upstream picked one at RANDOM from every quest with a reachable POI
// (`availableQuests[urand(0, availableQuests.size() - 1)]`), with no regard for level or locality.
// That was survivable while POIs were confined to the bot's own zone. Once cross-zone objectives are
// admitted the log fills with distant candidates, and a random pick would send a bot walking across a
// zone while a quest it could finish on the spot sat untouched.
//
// So: anything workable here and now outranks anything needing a trip. Within a class the lower level
// quest goes first, which clears the cheap content a bot is best equipped for, and distance only
// breaks ties.
struct QuestChoiceFacts
{
    QuestPoiReach reach = QuestPoiReach::Local;
    uint32 questLevel = 0;
    float distanceYards = 0.0f;
};

[[nodiscard]] inline bool QuestChoicePreferred(QuestChoiceFacts const& candidate, QuestChoiceFacts const& incumbent,
                                               bool hasIncumbent)
{
    if (!hasIncumbent)
        return true;
    if (candidate.reach != incumbent.reach)
        return candidate.reach == QuestPoiReach::Local;
    if (candidate.questLevel != incumbent.questLevel)
        return candidate.questLevel < incumbent.questLevel;
    return candidate.distanceYards < incumbent.distanceYards;
}

#endif
