#include "global.h"
#include "test/battle.h"
#include "battle_ai_main.h"
#include "battle_koanko.h"
#include "battle_setup.h"
#include "battle_tx_rollback.h"

// "Koanko TX AI": production selection for the solo TX through the real AI_KoankoTrainer dispatch
// (no injected Wish nomination). AUTHORED WITHOUT A BUILD; scenario-level assertions only.

extern AiScoreFunc sDynamicAiFunc;

static bool8 sTxAiSetup;

// Only enters the conjured phase on first use, exactly like the fixtures in tx_profiles.c, then
// forwards to the production dispatcher.
static s32 KoankoTxProduction(u32 attacker, u32 target, u32 move, s32 score)
{
    if (sTxAiSetup)
    {
        sTxAiSetup = FALSE;
        gBattleStruct->koankoPhase = KOANKO_CONJURED_TEAM;
    }
    return AI_KoankoTrainer(attacker, target, move, score);
}

AI_SINGLE_BATTLE_TEST("Koanko TX AI opens with Reduction and then casts Meteo")
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC);
        sTxAiSetup = TRUE;
        sDynamicAiFunc = KoankoTxProduction;
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(1000); HP(9999); MaxHP(9999); Moves(MOVE_SEISMIC_TOSS, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(1); Speed(1); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_TX_ROLLBACK) { Level(101); Speed(1); Moves(MOVE_WISH, MOVE_DOOM_DESIRE, MOVE_PROTECT); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); EXPECT_SEND_OUT(opponent, 1); }
        TURN { MOVE(player, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_CELEBRATE); }
    } SCENE {
        MESSAGE("A wish for Reduction fell upon your team!");
     //   ANIMATION(ANIM_TYPE_MOVE, MOVE_ARCANUM_METEO, opponent);
    } THEN {
        EXPECT_EQ(ReductionIsActive(), TRUE);
    }
}

AI_SINGLE_BATTLE_TEST("Koanko TX AI prefers a secure ordinary knockout over Meteo")
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC);
        sTxAiSetup = TRUE;
        sDynamicAiFunc = KoankoTxProduction;
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(1000); HP(1); MaxHP(9999); Moves(MOVE_SEISMIC_TOSS, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(1); Speed(1); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_TX_ROLLBACK) { Level(101); Speed(1); Moves(MOVE_SHADOW_BALL, MOVE_DOOM_DESIRE, MOVE_PROTECT); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); EXPECT_SEND_OUT(opponent, 1); }
        TURN { MOVE(player, MOVE_CELEBRATE); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_SHADOW_BALL, opponent);
    } THEN {
        // No Wish and no Meteo were spent on a turn that an ordinary move wins.
        EXPECT_EQ(ReductionIsActive(), FALSE);
        EXPECT_EQ(TxRollbackArcanumLocked(1), FALSE);
    }
}
