/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 */

#include "Ai/Base/Actions/CombatMovementPolicy.h"
#include "Ai/Base/Trigger/CombatStuckPolicy.h"
#include "Ai/World/Rpg/FightReportPolicy.h"
#include "Ai/World/Rpg/FlightDestinationPolicy.h"
#include "Ai/World/Rpg/MoveFarStuckPolicy.h"
#include "gtest/gtest.h"

TEST(PlayerbotCombatStuckPolicyTest, AFightIsMeasuredFromItsOwnFirstTickNotFromTheBotsFirstFightEver)
{
    // Bots 977, 1057 and 897, 2026-09-02 07:40: a thirty-second fight ten minutes after an earlier
    // one read as "stuck for over five minutes" and reset the AI every five seconds.
    time_t const firstFight = 1000;
    CombatSpan span;
    for (time_t t = firstFight; t <= firstFight + 30; t += 5)
        span = NoteCombatTick(span, t);
    EXPECT_FALSE(CombatStuckFor(span, firstFight + 30, COMBAT_STUCK_SECONDS));

    time_t const laterFight = firstFight + 600;
    span = NoteCombatTick(span, laterFight);
    EXPECT_EQ(span.since, laterFight);
    for (time_t t = laterFight + 5; t <= laterFight + 30; t += 5)
        span = NoteCombatTick(span, t);
    EXPECT_FALSE(CombatStuckFor(span, laterFight + 30, COMBAT_STUCK_SECONDS));

    // A fight that really does not end is still caught, at five and at fifteen minutes.
    for (time_t t = laterFight + 35; t <= laterFight + 305; t += 5)
        span = NoteCombatTick(span, t);
    EXPECT_TRUE(CombatStuckFor(span, laterFight + 305, COMBAT_STUCK_SECONDS));
    EXPECT_FALSE(CombatStuckFor(span, laterFight + 305, COMBAT_LONG_STUCK_SECONDS));
    EXPECT_TRUE(CombatStuckFor(span, laterFight + 905, COMBAT_LONG_STUCK_SECONDS));
}

TEST(PlayerbotCombatMovementPolicyTest, AFightTakesASoloRandomBotOffItsForcedWalk)
{
    using playerbots::combat::AttackStopsMovement;
    using playerbots::combat::CombatMoveOverridesForced;
    // Hemewmew, 2026-09-01: a forced gathering trip continued through the fight that killed her.
    EXPECT_TRUE(AttackStopsMovement(MovementPriority::MOVEMENT_FORCED, true, false, true));
    EXPECT_TRUE(CombatMoveOverridesForced(MovementPriority::MOVEMENT_COMBAT, MovementPriority::MOVEMENT_FORCED,
                                          true, true));
    // Upstream's rule stays for a mastered bot: a raid mechanic's forced move is not interrupted.
    EXPECT_FALSE(AttackStopsMovement(MovementPriority::MOVEMENT_FORCED, true, false, false));
    EXPECT_FALSE(CombatMoveOverridesForced(MovementPriority::MOVEMENT_COMBAT, MovementPriority::MOVEMENT_FORCED,
                                           true, false));
    // Below the combat band the walk always stops, as before.
    EXPECT_TRUE(AttackStopsMovement(MovementPriority::MOVEMENT_NORMAL, true, false, false));
    // Not moving, or a controlled movement generator, is never touched.
    EXPECT_FALSE(AttackStopsMovement(MovementPriority::MOVEMENT_NORMAL, false, false, true));
    EXPECT_FALSE(AttackStopsMovement(MovementPriority::MOVEMENT_NORMAL, true, true, true));
    // Out of combat a forced walk keeps its rank, and a normal move never outranks it.
    EXPECT_FALSE(CombatMoveOverridesForced(MovementPriority::MOVEMENT_COMBAT, MovementPriority::MOVEMENT_FORCED,
                                           false, true));
    EXPECT_FALSE(CombatMoveOverridesForced(MovementPriority::MOVEMENT_NORMAL, MovementPriority::MOVEMENT_FORCED,
                                           true, true));
}

namespace
{
MoveFarStuckFacts Sample(float displacement, uint32 elapsedMs)
{
    MoveFarStuckFacts facts;
    facts.tracking = true;
    facts.sameMap = true;
    facts.displacementYards = displacement;
    facts.elapsedMs = elapsedMs;
    return facts;
}
}  // namespace

TEST(PlayerbotMoveFarStuckPolicyTest, WithNothingTrackedYetTheCallerTakesTheFirstSample)
{
    MoveFarStuckFacts fresh;
    fresh.tracking = false;
    fresh.elapsedMs = 10 * 60 * 1000;
    EXPECT_EQ(MoveFarStuckVerdict::Resample, EvaluateMoveFarStuck(fresh));
}

TEST(PlayerbotMoveFarStuckPolicyTest, RealMovementRestartsTheWindow)
{
    // Moving further than the reset radius is progress, however long the window had been open.
    EXPECT_EQ(MoveFarStuckVerdict::Resample, EvaluateMoveFarStuck(Sample(5.1f, 0)));
    EXPECT_EQ(MoveFarStuckVerdict::Resample, EvaluateMoveFarStuck(Sample(400.0f, 10 * 60 * 1000)));
}

TEST(PlayerbotMoveFarStuckPolicyTest, StandingStillBrieflyIsNotYetStuck)
{
    // A bot may legitimately stand still: fighting, looting, waiting on a movement cooldown. The
    // window has to expire before that counts as stuck.
    EXPECT_EQ(MoveFarStuckVerdict::Wait, EvaluateMoveFarStuck(Sample(0.0f, 0)));
    EXPECT_EQ(MoveFarStuckVerdict::Wait, EvaluateMoveFarStuck(Sample(5.0f, 119999)));
}

TEST(PlayerbotMoveFarStuckPolicyTest, StandingStillPastTheWindowIsStuck)
{
    // The defect this exists for: MoveFarTo's own tracker is keyed on the destination and is reset
    // whenever a different subsystem asks for a different one. A bot that quest travel and vendor
    // maintenance both want to move alternates destinations every tick and is never rescued.
    // Measured on the live realm: 1517 quest pathing failures plus 490 vendor pathing failures from
    // one stationary bot over fifty minutes, and zero teleport recoveries.
    EXPECT_EQ(MoveFarStuckVerdict::Rescue, EvaluateMoveFarStuck(Sample(0.0f, 120 * 1000)));
    EXPECT_EQ(MoveFarStuckVerdict::Rescue, EvaluateMoveFarStuck(Sample(4.9f, 50 * 60 * 1000)));
}

TEST(PlayerbotMoveFarStuckPolicyTest, ACrossMapDisplacementIsNotComparable)
{
    // Distance between two maps is meaningless, so a map change can only mean "resample", never
    // "rescue" -- otherwise a bot that took a boat would be teleported back on arrival.
    MoveFarStuckFacts moved = Sample(0.0f, 50 * 60 * 1000);
    moved.sameMap = false;
    EXPECT_EQ(MoveFarStuckVerdict::Resample, EvaluateMoveFarStuck(moved));
}

TEST(PlayerbotMoveFarStuckPolicyTest, TheThresholdsAreConfigurable)
{
    MoveFarStuckFacts patient = Sample(0.0f, 120 * 1000);
    patient.rescueAfterMs = 300 * 1000;
    EXPECT_EQ(MoveFarStuckVerdict::Wait, EvaluateMoveFarStuck(patient));

    // The same three yard drift is "stood still" under the default reset radius and "made progress"
    // under a tighter one, so this pair fails unless the radius is read from the facts.
    EXPECT_EQ(MoveFarStuckVerdict::Rescue, EvaluateMoveFarStuck(Sample(3.0f, 120 * 1000)));

    MoveFarStuckFacts strict = Sample(3.0f, 120 * 1000);
    strict.resetRadius = 1.0f;
    EXPECT_EQ(MoveFarStuckVerdict::Resample, EvaluateMoveFarStuck(strict));
}

TEST(PlayerbotFlightDestinationPolicyTest, ARandomFlightNeverEndsOnAnotherMap)
{
    // Dazedcitizen, 2026-09-05: Stormwind (map 0) to Zul'Aman (node 205, map 530) put her down at
    // Thorium Point in Searing Gorge at level 14.
    EXPECT_FALSE(FlightDestinationOnBotMap(0, 530));
    EXPECT_TRUE(FlightDestinationOnBotMap(0, 0));
    EXPECT_TRUE(FlightDestinationOnBotMap(530, 530));
}

TEST(PlayerbotFightReportPolicyTest, ADeathIsClassifiedByWhetherTheBotEverFoughtBack)
{
    // Washezeka, 2026-09-05: 155 seconds in combat, every action failing, no hit landed.
    FightLedger noContact;
    noContact.startMs = 1000;
    for (int i = 0; i < 10; ++i)
    {
        NoteFightAction(noContact, "reach melee", false);
        NoteFightAction(noContact, "melee", false);
    }
    NoteFightAction(noContact, "melee", false);
    NoteFightDamageTaken(noContact, 400);
    EXPECT_EQ(ClassifyFight(noContact), FightVerdict::NoContact);
    EXPECT_EQ(TopFightFailure(noContact), "melee");
    EXPECT_EQ(noContact.actionsFailed, 21u);
    EXPECT_EQ(noContact.actionsOk, 0u);

    // A fight that was answered but lost three to one is the bot being outdamaged.
    FightLedger outdamaged;
    outdamaged.startMs = 1000;
    NoteFightDamageDealt(outdamaged, 50);
    NoteFightDamageDealt(outdamaged, 50);
    NoteFightDamageTaken(outdamaged, 300);
    NoteFightAction(outdamaged, "melee", true);
    EXPECT_EQ(outdamaged.hits, 2u);
    EXPECT_EQ(ClassifyFight(outdamaged), FightVerdict::Outdamaged);
    EXPECT_EQ(TopFightFailure(outdamaged), "none");

    // Dealt 200, took 250: a fight it could have won.
    FightLedger lost;
    lost.startMs = 1000;
    NoteFightDamageDealt(lost, 200);
    NoteFightDamageTaken(lost, 250);
    EXPECT_EQ(ClassifyFight(lost), FightVerdict::LostExchange);

    // Nothing is counted against a closed ledger: damage outside a fight is not a fight.
    FightLedger closed;
    NoteFightDamageDealt(closed, 100);
    NoteFightAction(closed, "melee", true);
    EXPECT_EQ(closed.hits, 0u);
    EXPECT_EQ(closed.actionsOk, 0u);
    EXPECT_STREQ(FightVerdictName(FightVerdict::NoContact), "nocontact");
}
