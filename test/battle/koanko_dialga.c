#include "global.h"
#include "test/battle.h"
#include "battle_break_bar.h"
#include "battle_entry_scripts.h"
#include "battle_koanko.h"
#include "battle_script_native.h"
#include "battle_setup.h"
#include "battle_util.h"
#include "constants/battle_script_commands.h"
#include "constants/trainers.h"

// Start at the already-conjured singles boundary. Stage 07's live Zacian
// fixtures separately exercise the preceding formation teardown. No mutable
// static storage is needed here: bytecode/configuration stays in ROM.
static void BS_DialgaFixture(void)
{
    NATIVE_ARGS();
    gBattleStruct->koankoPhase = KOANKO_CONJURED_TEAM;
    KoankoOnSwitchIn(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT));
    gBattlescriptCurrInstr = cmd->nextInstr;
}

static const struct __attribute__((packed))
{
    u8 native;
    void (*func)(void);
    u8 ret;
} sDialgaOpening = {B_SCR_OP_CALLNATIVE, BS_DialgaFixture, B_SCR_OP_RETURN};

static void DialgaFixture(void)
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    TurnZeroScript(B_TRAINER_OPPONENT_A, (const u8 *)&sDialgaOpening);
}

SINGLE_BATTLE_TEST("Koanko Dialga recharges after a genuine KO and permits the player replacement")
{
    u32 move;
    PARAMETRIZE { move = MOVE_ROAR_OF_ETERNITY; }
    PARAMETRIZE { move = MOVE_ROAR_OF_TIME; }
    GIVEN {
        ASSUME(B_SKIP_RECHARGE != GEN_1);
        DialgaFixture();
        PLAYER(SPECIES_WOBBUFFET) { HP(1); Speed(1); Moves(MOVE_CELEBRATE); }
        PLAYER(SPECIES_ABRA) { HP(500); MaxHP(500); Speed(1); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_DIALGA_ORIGIN) { Level(104); Speed(2); Moves(move, MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, move); MOVE(player, MOVE_CELEBRATE); SEND_OUT(player, 1); }
        TURN { SKIP_TURN(opponent); MOVE(player, MOVE_CELEBRATE); }
        TURN { MOVE(opponent, MOVE_CELEBRATE); MOVE(player, MOVE_CELEBRATE); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, move, opponent);
        MESSAGE("Wobbuffet fainted!");
        MESSAGE("The opposing Dialga must recharge!");
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, player);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, opponent);
    } THEN {
        EXPECT_EQ(gBattlersCount, 2);
        EXPECT_EQ(gBattleTypeFlags & BATTLE_TYPE_DOUBLE, 0);
        EXPECT_EQ(gBattlerPartyIndexes[0], 1);
        EXPECT_EQ(player->species, SPECIES_ABRA);
        EXPECT_EQ((u32)player->volatiles.timestop, 0);
        EXPECT_EQ(gBattleResults.playerFaintCounter, 1);
        EXPECT(gBattleStruct->battlerState[1].noOrderChange);
    }
}

// With this near-zero attack, the 150 BP hit does only a few HP. Poison does
// exactly 20 each turn: 21 HP dies on the hit turn, 41 HP on the stopped turn.
// Both cases must replace normally; the volatile belongs to the old occupant.
SINGLE_BATTLE_TEST("Koanko Dialga Time Stop permits residual faint replacement on either turn")
{
    bool32 stoppedTurn;
    PARAMETRIZE { stoppedTurn = FALSE; }
    PARAMETRIZE { stoppedTurn = TRUE; }
    GIVEN {
        DialgaFixture();
        PLAYER(SPECIES_WOBBUFFET) {
            Speed(1); HP(stoppedTurn ? 41 : 21); MaxHP(160); SpDefense(9999);
            Status1(STATUS1_POISON); Moves(MOVE_CELEBRATE);
        }
        PLAYER(SPECIES_ABRA) { Speed(1); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_DIALGA_ORIGIN) { Level(104); Speed(2); SpAttack(1); Moves(MOVE_ROAR_OF_ETERNITY, MOVE_CELEBRATE); }
    } WHEN {
        TURN {
            MOVE(opponent, MOVE_ROAR_OF_ETERNITY);
            MOVE(player, MOVE_CELEBRATE);
            if (!stoppedTurn) SEND_OUT(player, 1);
        }
        TURN {
            SKIP_TURN(opponent);
            if (stoppedTurn) { SKIP_TURN(player); SEND_OUT(player, 1); }
            else MOVE(player, MOVE_CELEBRATE);
        }
        TURN { MOVE(opponent, MOVE_CELEBRATE); MOVE(player, MOVE_CELEBRATE); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_ROAR_OF_ETERNITY, opponent);
        MESSAGE("Wobbuffet was hurt by its poisoning!");
        if (stoppedTurn)
        {
            MESSAGE("The opposing Dialga must recharge!");
            MESSAGE("Wobbuffet was hurt by its poisoning!");
        }
        MESSAGE("Wobbuffet fainted!");
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, player);
    } THEN {
        // Exhausting a recorded fixture ends playback with this synthetic
        // outcome. A live reserve and the next action prove replacement worked.
        EXPECT_EQ(gBattleOutcome, B_OUTCOME_PLAYER_TELEPORTED);
        EXPECT_EQ(gBattlersCount, 2);
        EXPECT_EQ(gBattlerPartyIndexes[0], 1);
        EXPECT_EQ(player->species, SPECIES_ABRA);
        EXPECT_EQ((u32)player->volatiles.timestop, 0);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP), 0);
        EXPECT_EQ(gBattleResults.playerFaintCounter, 1);
    }
}

SINGLE_BATTLE_TEST("Koanko Dialga Time Stop cannot prevent loss after a residual faint")
{
    GIVEN {
        DialgaFixture();
        PLAYER(SPECIES_WOBBUFFET) {
            Speed(1); HP(41); MaxHP(160); SpDefense(9999);
            Status1(STATUS1_POISON); Moves(MOVE_CELEBRATE);
        }
        OPPONENT(SPECIES_DIALGA_ORIGIN) { Level(104); Speed(2); SpAttack(1); Moves(MOVE_ROAR_OF_ETERNITY); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_ROAR_OF_ETERNITY); MOVE(player, MOVE_CELEBRATE); }
        // Both battlers automatically skip the next selection. Playback reaches
        // the fatal poison tick without consuming another recorded command.
        // An extra all-SKIP TURN would fail the harness's lastActionTurn check.
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_ROAR_OF_ETERNITY, opponent);
        MESSAGE("The opposing Dialga must recharge!");
        MESSAGE("Wobbuffet fainted!");
    } THEN {
        EXPECT_EQ(gBattleOutcome, B_OUTCOME_LOST);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP), 0);
    }
}

SINGLE_BATTLE_TEST("Koanko Dialga recharge survives breaking a saved Break Charm layer")
{
    u32 move;
    bool32 proc;
    PARAMETRIZE { move = MOVE_ROAR_OF_ETERNITY; proc = TRUE; }
    PARAMETRIZE { move = MOVE_ROAR_OF_ETERNITY; proc = FALSE; }
    PARAMETRIZE { move = MOVE_ROAR_OF_TIME; proc = FALSE; }
    GIVEN {
        DialgaFixture();
        PLAYER(SPECIES_WOBBUFFET) { HP(7); MaxHP(10); Speed(1); Item(ITEM_BREAK_CHARM); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_DIALGA_ORIGIN) { Level(104); Speed(2); SpAttack(9999); Moves(move, MOVE_CELEBRATE); }
    } WHEN {
        // Pending bars count as alive while additional effects resolve. Time
        // Stop can therefore land before the covered HP is restored.
        TURN {
            if (move == MOVE_ROAR_OF_ETERNITY)
                MOVE(opponent, move, WITH_RNG(RNG_SECONDARY_EFFECT_2, proc));
            else
                MOVE(opponent, move);
            MOVE(player, MOVE_CELEBRATE);
        }
        TURN {
            SKIP_TURN(opponent);
            if (proc) SKIP_TURN(player);
            else MOVE(player, MOVE_CELEBRATE);
        }
        TURN { MOVE(opponent, MOVE_CELEBRATE); MOVE(player, MOVE_CELEBRATE); }
    } SCENE {
        MESSAGE("Wobbuffet's Break Charm formed an extra HP bar!");
        ANIMATION(ANIM_TYPE_MOVE, move, opponent);
        HP_BAR(player, hp: 0);
        MESSAGE("The opposing Dialga must recharge!");
        if (!proc) ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, player);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, opponent);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, player);
    } THEN {
        EXPECT_EQ(player->hp, 7);
        EXPECT_EQ(GetBreakBarCount(0), 0);
        EXPECT_EQ(gBattleResults.playerFaintCounter, 0);
        EXPECT_EQ((u32)player->volatiles.timestop, 0);
    }
}

SINGLE_BATTLE_TEST("Koanko Dialga simultaneous genuine faints permit both replacements")
{
    GIVEN {
        DialgaFixture();
        PLAYER(SPECIES_WOBBUFFET) { HP(1); Speed(2); Moves(MOVE_DESTINY_BOND); }
        PLAYER(SPECIES_ABRA) { Speed(2); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_DIALGA_ORIGIN) { Level(104); Speed(1); Moves(MOVE_ROAR_OF_ETERNITY); }
        OPPONENT(SPECIES_ARCEUS) { Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_DESTINY_BOND); MOVE(opponent, MOVE_ROAR_OF_ETERNITY); SEND_OUT(player, 1); SEND_OUT(opponent, 1); }
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        MESSAGE("Wobbuffet fainted!");
        MESSAGE("Wobbuffet took its attacker down with it!");
        MESSAGE("The opposing Dialga fainted!");
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, player);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_CELEBRATE, opponent);
    } THEN {
        EXPECT_EQ(gBattlersCount, 2);
        EXPECT_EQ(gBattlerPartyIndexes[0], 1);
        EXPECT_EQ(gBattlerPartyIndexes[1], 1);
        EXPECT_EQ((u32)player->volatiles.timestop, 0);
        EXPECT_EQ((u32)opponent->volatiles.rechargeTimer, 0);
        EXPECT_EQ(gBattleStruct->koankoPhase, KOANKO_CONJURED_TEAM);
    }
}

SINGLE_BATTLE_TEST("Koanko Dialga permits bag actions once Time Stop expires")
{
    GIVEN {
        DialgaFixture();
        PLAYER(SPECIES_WOBBUFFET) { HP(100); MaxHP(500); Speed(1); SpDefense(9999); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_DIALGA_ORIGIN) { Level(104); Speed(2); SpAttack(1); Moves(MOVE_ROAR_OF_ETERNITY, MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_ROAR_OF_ETERNITY); MOVE(player, MOVE_CELEBRATE); }
        TURN { SKIP_TURN(opponent); SKIP_TURN(player); }
        TURN { USE_ITEM(player, ITEM_FULL_RESTORE, partyIndex: 0); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(player->hp, 500);
        EXPECT_EQ((u32)player->volatiles.timestop, 0);
        EXPECT_EQ((u32)opponent->volatiles.rechargeTimer, 0);
    }
}
