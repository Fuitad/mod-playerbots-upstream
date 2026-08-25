// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#include <cstdlib>
#include <vector>

#include "Bot/Factory/RandomPlayerbotFactionBalance.h"

namespace
{
void Require(bool condition)
{
    if (!condition)
        std::exit(EXIT_FAILURE);
}
}  // namespace

int main()
{
    RandomPlayerbotFactionBalance fresh;
    for (uint32 index = 0; index < 207; ++index)
        fresh.RecordCreated(fresh.ShouldCreateAlliance());
    Require(fresh.AllianceCount() == 104);
    Require(fresh.HordeCount() == 103);

    RandomPlayerbotFactionBalance resumed;
    for (uint32 index = 0; index < 96; ++index)
        resumed.RecordExisting(true);
    for (uint32 index = 0; index < 111; ++index)
        resumed.RecordExisting(false);
    for (uint32 index = 0; index < 15; ++index)
        resumed.RecordCreated(resumed.ShouldCreateAlliance());
    Require(resumed.AllianceCount() == 111);
    Require(resumed.HordeCount() == 111);

    std::vector<uint8> const allianceClasses = {CLASS_WARRIOR, CLASS_PALADIN, CLASS_HUNTER,  CLASS_ROGUE,
                                                CLASS_PRIEST,  CLASS_MAGE,    CLASS_WARLOCK, CLASS_DRUID};
    std::vector<uint8> const hordeClasses = {CLASS_WARRIOR, CLASS_HUNTER, CLASS_ROGUE,   CLASS_PRIEST,
                                             CLASS_SHAMAN,  CLASS_MAGE,   CLASS_WARLOCK, CLASS_DRUID};
    RandomPlayerbotFactionBalance classes;
    for (uint32 index = 0; index < 160; ++index)
    {
        bool const alliance = classes.ShouldCreateAlliance();
        std::vector<uint8> const& available = alliance ? allianceClasses : hordeClasses;
        uint8 const selected = classes.SelectLeastRepresentedClass(alliance, available);
        Require(selected != 0);
        classes.RecordCreated(alliance, selected);
    }
    for (uint8 cls : allianceClasses)
        Require(classes.ClassCount(true, cls) == 10);
    for (uint8 cls : hordeClasses)
        Require(classes.ClassCount(false, cls) == 10);

    Require(RandomPlayerbotRemainingCharacterSlots(0) == 10);
    Require(RandomPlayerbotRemainingCharacterSlots(7) == 3);
    Require(RandomPlayerbotRemainingCharacterSlots(10) == 0);
    Require(RandomPlayerbotRemainingCharacterSlots(11) == 0);
    return EXIT_SUCCESS;
}
