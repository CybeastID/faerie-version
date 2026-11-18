#include "global.h"
#include "test/battle.h"

// Timeline these tests encode (battle_set_effect.c HandleSetEffectTimestop,
// battle_main.c countdown, battle_move_resolution.c and its Faerie cancelers):
//   T1  Roar of Eternity hits: user rechargeTimer = 2; on a 50% proc target timestop = 3.
//       End of T1: rechargeTimer 2 -> 1, timestop 3 -> 2.
//   T2  User must recharge. A timestopped target's action resolves into nothing (silently).
//       End of T2: rechargeTimer 1 -> 0, timestop 2 -> 1 (colors restored).
//   T3  Both act again. timestop 1 still blocks a re-proc; end of T3 it reaches 0.
// So Time Stop lands exactly on the recharge turn: nobody acts on T2.

ASSUMPTIONS
{
    ASSUME(MoveHasAdditionalEffectSelf(MOVE_ROAR_OF_ETERNITY, MOVE_EFFECT_RECHARGE) == TRUE);
    ASSUME(MoveHasAdditionalEffect(MOVE_ROAR_OF_ETERNITY, MOVE_EFFECT_TIMESTOP) == TRUE);
    ASSUME(GetMovePower(MOVE_ROAR_OF_ETERNITY) == 150);
    ASSUME(GetMoveAccuracy(MOVE_ROAR_OF_ETERNITY) == 90);
}

// Shield Dust nulls every non-self additional effect (SetMoveEffect), so it removes the
// Timestop proc but leaves the self-targeted recharge alone. This isolates recharge.
SINGLE_BATTLE_TEST("Roar of Eternity makes the user unable to attack for exactly one turn")
{
    GIVEN {
        PLAYER(SPECIES_DIALGA_ORIGIN) { Moves(MOVE_ROAR_OF_ETERNITY, MOVE_TACKLE); }
        OPPONENT(SPECIES_WOBBUFFET) { Ability(ABILITY_SHIELD_DUST); MaxHP(500); HP(500); SpDefense(500); }
    } WHEN {
        TURN { MOVE(player, MOVE_ROAR_OF_ETERNITY, hit: TRUE); }
        TURN { SKIP_TURN(player); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_TACKLE); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_ROAR_OF_ETERNITY, player);
        MESSAGE("Dialga must recharge!");
        ANIMATION(ANIM_TYPE_MOVE, MOVE_TACKLE, player);
    }
}

SINGLE_BATTLE_TEST("Roar of Eternity inflicts Time Stop 50% of the time")
{
    PASSES_RANDOMLY(50, 100, RNG_SECONDARY_EFFECT_2);
    GIVEN {
        PLAYER(SPECIES_DIALGA_ORIGIN) { Moves(MOVE_ROAR_OF_ETERNITY, MOVE_TACKLE); }
        OPPONENT(SPECIES_WOBBUFFET) { MaxHP(500); HP(500); SpDefense(500); }
    } WHEN {
        TURN { MOVE(player, MOVE_ROAR_OF_ETERNITY, hit: TRUE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        // On a proc, recharge and Time Stop bypass both action prompts on T2.
        // Playback reaches T3 before it ends, so both T1 and T2 have counted down.
        EXPECT_EQ((u32)opponent->volatiles.timestop, 1);
    }
}

// Both effects on the same hit: recharge still operates while the target is Time Stopped,
// and Time Stop covers exactly the recharge turn. Runs under the 50% proc; the non-proc
// outcome fails the T2 SKIP_TURN(opponent), which is what PASSES_RANDOMLY expects.
SINGLE_BATTLE_TEST("Roar of Eternity recharge and Time Stop resolve on the same turn")
{
    PASSES_RANDOMLY(50, 100, RNG_SECONDARY_EFFECT_2);
    GIVEN {
        PLAYER(SPECIES_DIALGA_ORIGIN) { Moves(MOVE_ROAR_OF_ETERNITY, MOVE_TACKLE); }
        OPPONENT(SPECIES_WOBBUFFET) { MaxHP(500); HP(500); SpDefense(500); }
    } WHEN {
        TURN { MOVE(player, MOVE_ROAR_OF_ETERNITY, hit: TRUE); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { SKIP_TURN(player); SKIP_TURN(opponent); }
        TURN { MOVE(player, MOVE_TACKLE); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_ROAR_OF_ETERNITY, player);
        MESSAGE("Dialga must recharge!");
        ANIMATION(ANIM_TYPE_MOVE, MOVE_TACKLE, player);
    } THEN {
        EXPECT_EQ((u32)opponent->volatiles.timestop, 0);
    }
}

// A forced non-proc must still pay the ordinary recharge cost. Rig only the
// second additional effect; the mandatory self effect is not probabilistic.
SINGLE_BATTLE_TEST("Roar of Eternity still recharges when Time Stop does not proc")
{
    GIVEN {
        PLAYER(SPECIES_DIALGA_ORIGIN) { Moves(MOVE_ROAR_OF_ETERNITY, MOVE_TACKLE); }
        OPPONENT(SPECIES_WOBBUFFET) { MaxHP(500); HP(500); SpDefense(500); }
    } WHEN {
        TURN { MOVE(player, MOVE_ROAR_OF_ETERNITY, WITH_RNG(RNG_SECONDARY_EFFECT_2, FALSE)); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { SKIP_TURN(player); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_TACKLE); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_ROAR_OF_ETERNITY, player);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, opponent);
        MESSAGE("Dialga must recharge!");
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, opponent);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_TACKLE, player);
    } THEN {
        EXPECT_EQ((u32)opponent->volatiles.timestop, 0);
    }
}

SINGLE_BATTLE_TEST("Roar of Eternity follows Roar of Time recharge rules on miss, Protect and immunity")
{
    u32 move, failure;
    PARAMETRIZE { move = MOVE_ROAR_OF_ETERNITY; failure = 0; }
    PARAMETRIZE { move = MOVE_ROAR_OF_TIME; failure = 0; }
    PARAMETRIZE { move = MOVE_ROAR_OF_ETERNITY; failure = 1; }
    PARAMETRIZE { move = MOVE_ROAR_OF_TIME; failure = 1; }
    PARAMETRIZE { move = MOVE_ROAR_OF_ETERNITY; failure = 2; }
    PARAMETRIZE { move = MOVE_ROAR_OF_TIME; failure = 2; }
    GIVEN {
        PLAYER(SPECIES_DIALGA_ORIGIN) { Speed(2); Moves(move, MOVE_TACKLE); }
        OPPONENT(failure == 2 ? SPECIES_CLEFAIRY : SPECIES_WOBBUFFET) { Speed(1); HP(500); MaxHP(500); Moves(MOVE_PROTECT, MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, move, hit: failure != 0); MOVE(opponent, failure == 1 ? MOVE_PROTECT : MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_TACKLE); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        NOT HP_BAR(opponent);
        NOT MESSAGE("Dialga must recharge!");
        ANIMATION(ANIM_TYPE_MOVE, MOVE_TACKLE, player);
        HP_BAR(opponent);
    } THEN {
        EXPECT_EQ((u32)player->volatiles.rechargeTimer, 0);
        EXPECT_EQ((u32)opponent->volatiles.timestop, 0);
    }
}

SINGLE_BATTLE_TEST("Roar of Eternity secondary blockers preserve ordinary recharge")
{
    u32 blocker;
    PARAMETRIZE { blocker = 0; } // Shield Dust.
    PARAMETRIZE { blocker = 1; } // Covert Cloak.
    PARAMETRIZE { blocker = 2; } // Sheer Force.
    PARAMETRIZE { blocker = 3; } // Substitute survives the hit.
    GIVEN {
        PLAYER(SPECIES_DIALGA_ORIGIN) {
            Speed(1); SpAttack(1); Moves(MOVE_ROAR_OF_ETERNITY, MOVE_TACKLE);
            if (blocker == 2) Ability(ABILITY_SHEER_FORCE);
        }
        OPPONENT(SPECIES_WOBBUFFET) {
            Speed(2); HP(500); MaxHP(500); SpDefense(500); Moves(MOVE_SUBSTITUTE, MOVE_CELEBRATE);
            if (blocker == 0) Ability(ABILITY_SHIELD_DUST);
            if (blocker == 1) Item(ITEM_COVERT_CLOAK);
        }
    } WHEN {
        TURN { MOVE(opponent, blocker == 3 ? MOVE_SUBSTITUTE : MOVE_CELEBRATE); MOVE(player, MOVE_ROAR_OF_ETERNITY); }
        TURN { MOVE(opponent, MOVE_CELEBRATE); SKIP_TURN(player); }
        TURN { MOVE(opponent, MOVE_CELEBRATE); MOVE(player, MOVE_TACKLE); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_ROAR_OF_ETERNITY, player);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, opponent);
        MESSAGE("Dialga must recharge!");
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, opponent);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_TACKLE, player);
    } THEN {
        EXPECT_EQ((u32)opponent->volatiles.timestop, 0);
        if (blocker == 3)
            EXPECT(opponent->volatiles.substitute);
    }
}

// The other attacker can try another proc while Dialga recharges, and again
// during the protected cooldown. Neither hit may extend the original stop.
DOUBLE_BATTLE_TEST("Roar of Eternity cannot refresh active Time Stop or its cooldown")
{
    GIVEN {
        PLAYER(SPECIES_DIALGA_ORIGIN) { Speed(4); SpAttack(1); Moves(MOVE_ROAR_OF_ETERNITY, MOVE_CELEBRATE); }
        PLAYER(SPECIES_DIALGA) { Speed(3); SpAttack(1); Moves(MOVE_ROAR_OF_ETERNITY, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(2); HP(9999); MaxHP(9999); SpDefense(9999); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WYNAUT) { Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_ROAR_OF_ETERNITY, target: opponentLeft); MOVE(playerRight, MOVE_CELEBRATE); MOVE(opponentLeft, MOVE_CELEBRATE); MOVE(opponentRight, MOVE_CELEBRATE); }
        TURN { SKIP_TURN(playerLeft); MOVE(playerRight, MOVE_ROAR_OF_ETERNITY, target: opponentLeft); SKIP_TURN(opponentLeft); MOVE(opponentRight, MOVE_CELEBRATE); }
        TURN { MOVE(playerLeft, MOVE_ROAR_OF_ETERNITY, target: opponentLeft); SKIP_TURN(playerRight); MOVE(opponentLeft, MOVE_CELEBRATE); MOVE(opponentRight, MOVE_CELEBRATE); }
        TURN { SKIP_TURN(playerLeft); MOVE(playerRight, MOVE_CELEBRATE); MOVE(opponentLeft, MOVE_CELEBRATE); MOVE(opponentRight, MOVE_CELEBRATE); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_ROAR_OF_ETERNITY, playerLeft);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_ROAR_OF_ETERNITY, playerRight);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_ROAR_OF_ETERNITY, playerLeft);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, opponentLeft);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, opponentLeft);
    } THEN {
        EXPECT_EQ((u32)opponentLeft->volatiles.timestop, 0);
    }
}

DOUBLE_BATTLE_TEST("Roar of Eternity permits direct damage replacement during the stopped turn")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Speed(2); HP(141); MaxHP(500); SpDefense(9999); Moves(MOVE_CELEBRATE); }
        PLAYER(SPECIES_WYNAUT) { Speed(1); Moves(MOVE_CELEBRATE); }
        PLAYER(SPECIES_ABRA) { Speed(2); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_DIALGA_ORIGIN) { Speed(4); SpAttack(1); Moves(MOVE_ROAR_OF_ETERNITY, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(100); Speed(3); Moves(MOVE_NIGHT_SHADE, MOVE_CELEBRATE); }
    } WHEN {
        TURN {
            MOVE(opponentLeft, MOVE_ROAR_OF_ETERNITY, target: playerLeft);
            MOVE(opponentRight, MOVE_NIGHT_SHADE, target: playerLeft);
            MOVE(playerLeft, MOVE_CELEBRATE); MOVE(playerRight, MOVE_CELEBRATE);
        }
        TURN {
            SKIP_TURN(opponentLeft); MOVE(opponentRight, MOVE_NIGHT_SHADE, target: playerLeft);
            SKIP_TURN(playerLeft); MOVE(playerRight, MOVE_CELEBRATE); SEND_OUT(playerLeft, 2);
        }
        TURN {
            MOVE(opponentLeft, MOVE_CELEBRATE); MOVE(opponentRight, MOVE_CELEBRATE);
            MOVE(playerLeft, MOVE_CELEBRATE); MOVE(playerRight, MOVE_CELEBRATE);
        }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_ROAR_OF_ETERNITY, opponentLeft);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_NIGHT_SHADE, opponentRight);
        MESSAGE("The opposing Dialga must recharge!");
        ANIMATION(ANIM_TYPE_MOVE, MOVE_NIGHT_SHADE, opponentRight);
        MESSAGE("Wobbuffet fainted!");
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, playerLeft);
    } THEN {
        EXPECT_EQ(gBattlerPartyIndexes[0], 2);
        EXPECT_EQ(playerLeft->species, SPECIES_ABRA);
        EXPECT_EQ((u32)playerLeft->volatiles.timestop, 0);
        EXPECT_EQ(gBattleResults.playerFaintCounter, 1);
    }
}
