/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// PLB-LOCAL UPSTREAM-FILE: this fork changes 5 region(s) of this upstream file.

#ifndef PLAYERBOTS_RANDOMPLAYERBOTFACTORY_H
#define PLAYERBOTS_RANDOMPLAYERBOTFACTORY_H

#include "Common.h"
#include "DBCEnums.h"
// PLB-LOCAL(a072e78abf6c): refactor: extract custom playerbot implementations
#include "SharedDefines.h"
#include <map>
#include <unordered_map>
#include <vector>

class Player;
class WorldSession;

enum ArenaType : uint8;

class RandomPlayerbotFactory
{
public:
    enum class NameRaceAndGender : uint8
    {
        // PLB-LOCAL(a072e78abf6c): refactor: extract custom playerbot implementations
        // Generic categories are global pools shared by every playable race.
        GenericMale = 0,
        GenericFemale,
        GnomeMale,
        GnomeFemale,
        DwarfMale,
        DwarfFemale,
        NightelfMale,
        NightelfFemale,
        DraeneiMale,
        DraeneiFemale,
        OrcMale,
        OrcFemale,
        TrollMale,
        TrollFemale,
        TaurenMale,
        TaurenFemale,
        BloodelfMale,
        BloodelfFemale
    };

    // PLB-LOCAL(a072e78abf6c): refactor: extract custom playerbot implementations
    static constexpr NameRaceAndGender CombineRaceAndGender(uint8 /*race*/, uint8 gender)
    {
        return gender == GENDER_FEMALE ? NameRaceAndGender::GenericFemale : NameRaceAndGender::GenericMale;
    }

    RandomPlayerbotFactory() {};
    virtual ~RandomPlayerbotFactory() {}

    // PLB-LOCAL(a072e78abf6c): refactor: extract custom playerbot implementations
    Player* CreateRandomBot(WorldSession* session, uint8 cls, bool alliance,
                            std::unordered_map<NameRaceAndGender, std::vector<std::string>>& names);
    static void CreateRandomBots();
    static void CreateRandomArenaTeams(ArenaType slot, uint32 count);
    static std::string const CreateRandomGuildName();
    static uint32 CalculateTotalAccountCount();
    static uint32 CalculateAvailableCharsPerAccount();

private:
    static bool IsValidRaceClassCombination(uint8 race, uint8 class_, uint32 expansion);
    // PLB-LOCAL(a072e78abf6c): refactor: extract custom playerbot implementations
    static std::vector<uint8> GetAvailableClasses(bool alliance);
    static std::vector<uint8> GetAvailableRaces(uint8 class_, bool alliance);
    std::string const CreateRandomBotName(NameRaceAndGender raceAndGender);
    static std::string const CreateRandomArenaTeamName();
};

#endif
