#include "global.h"
#include "test/battle.h"
#include "battle_ai_main.h"
#include "battle_break_bar.h"
#include "battle_scripts.h"
#include "trainer_break_bars.h"

// "Koanko Calyrex" / "Koanko Eternatus": the first original Break Bar transforms the boss into its
// battle form (Stage 06). AUTHORED WITHOUT A BUILD. The bar comes from the production
// gKoankoConjuredBreakBars rules (not a test-only config) and is activated on first AI use, the same
// seeding pattern as break_bar.c. The boss is made faster than the player so its own action for the
// turn is already spent when the bar breaks; a form change must not reinterpret a queued slot.

extern AiScoreFunc sDynamicAiFunc;

static bool8 sFormsSetup;
static u32 sFormsSeedStage;

static s32 KoankoFormsSeed(u32 attacker, u32 target, u32 move, s32 score)
{
    if (sFormsSetup)
    {
        sFormsSetup = FALSE;
        u32 battler = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);
        const struct BreakBarConfig *config = ResolveTrainerBreakBar(&gKoankoConjuredBreakBars, gBattleMons[battler].species, 0);
        if (config != NULL)
            ActivateBreakBar(battler, config);
        if (sFormsSeedStage != 0)
            gBattleMons[battler].statStages[STAT_ATK] = sFormsSeedStage;
    }
    return score;
}

TEST("Koanko Calyrex and Eternatus are the only conjured Break Bar owners")
{
    const struct BreakBarConfig *ice = ResolveTrainerBreakBar(&gKoankoConjuredBreakBars, SPECIES_CALYREX_ICE, 4);
    const struct BreakBarConfig *eternatus = ResolveTrainerBreakBar(&gKoankoConjuredBreakBars, SPECIES_ETERNATUS, 5);

    EXPECT(ice != NULL);
    EXPECT(eternatus != NULL);
    EXPECT_EQ(ice->extraBars, 1);
    EXPECT_EQ(eternatus->extraBars, 1);
    EXPECT(ice->phases[BREAK_BAR_PURPLE - 1].onBreakScript == BattleScript_KoankoBossFormBreak);
    EXPECT(eternatus->phases[BREAK_BAR_PURPLE - 1].onBreakScript == BattleScript_KoankoBossFormBreak);
    // Smeargle, the solo TX, Dialga and Arceus keep no original bar.
    EXPECT(ResolveTrainerBreakBar(&gKoankoConjuredBreakBars, SPECIES_SMEARGLE, 0) == NULL);
    EXPECT(ResolveTrainerBreakBar(&gKoankoConjuredBreakBars, SPECIES_TX_ROLLBACK, 1) == NULL);
    EXPECT(ResolveTrainerBreakBar(&gKoankoConjuredBreakBars, SPECIES_DIALGA_ORIGIN, 6) == NULL);
    EXPECT(ResolveTrainerBreakBar(&gKoankoConjuredBreakBars, SPECIES_ARCEUS, 7) == NULL);
}

AI_SINGLE_BATTLE_TEST("Koanko Calyrex first original bar becomes Shadow Rider without fainting")
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC);
        sFormsSetup = TRUE;
        sFormsSeedStage = 0;
        sDynamicAiFunc = KoankoFormsSeed;
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(1); HP(9999); MaxHP(9999); Moves(MOVE_SEISMIC_TOSS); }
        OPPONENT(SPECIES_CALYREX_ICE) { Level(102); Speed(2000); HP(1); Item(ITEM_LEFTOVERS);
            Moves(MOVE_GLACIAL_LANCE, MOVE_HIGH_HORSEPOWER, MOVE_TERA_BLAST, MOVE_SWORDS_DANCE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); }
    } THEN {
        EXPECT_EQ(opponent->species, SPECIES_CALYREX_SHADOW);
        EXPECT_EQ(opponent->ability, ABILITY_AS_ONE_SHADOW_RIDER);
        EXPECT_EQ(opponent->moves[0], MOVE_ASTRAL_BARRAGE);
        EXPECT_EQ(opponent->moves[1], MOVE_PSYSHOCK);
        EXPECT_EQ(opponent->moves[2], MOVE_AURA_SPHERE);
        EXPECT_EQ(opponent->moves[3], MOVE_NASTY_PLOT);
        EXPECT_EQ(opponent->pp[0], GetMovePP(MOVE_ASTRAL_BARRAGE));
        EXPECT_EQ(opponent->hp, opponent->maxHP);       // shared refill used the new max HP
        EXPECT_EQ(opponent->item, ITEM_LEFTOVERS);
        EXPECT_EQ(GetBreakBarCount(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT)), 0);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
        EXPECT_EQ((u32)GetBattlerPartyState(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT))->changedSpecies, SPECIES_CALYREX_ICE);
        EXPECT_EQ(GetActiveGimmick(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT)), GIMMICK_NONE); // no fake Mega/Dynamax
    }
}

AI_SINGLE_BATTLE_TEST("Koanko Calyrex form change keeps stat stages")
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC);
        sFormsSetup = TRUE;
        sFormsSeedStage = DEFAULT_STAT_STAGE + 2;
        sDynamicAiFunc = KoankoFormsSeed;
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(1); HP(9999); MaxHP(9999); Moves(MOVE_SEISMIC_TOSS); }
        OPPONENT(SPECIES_CALYREX_ICE) { Level(102); Speed(2000); HP(1);
            Moves(MOVE_GLACIAL_LANCE, MOVE_HIGH_HORSEPOWER, MOVE_TERA_BLAST, MOVE_SWORDS_DANCE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); }
    } THEN {
        EXPECT_EQ(opponent->species, SPECIES_CALYREX_SHADOW);
        EXPECT_EQ(opponent->statStages[STAT_ATK], DEFAULT_STAT_STAGE + 2);
    }
}

AI_SINGLE_BATTLE_TEST("Koanko Eternatus first original bar becomes genuine Eternamax with Eternabeam")
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC);
        sFormsSetup = TRUE;
        sFormsSeedStage = 0;
        sDynamicAiFunc = KoankoFormsSeed;
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(1); HP(9999); MaxHP(9999); Moves(MOVE_SEISMIC_TOSS); }
        OPPONENT(SPECIES_ETERNATUS) { Level(103); Speed(2000); HP(1);
            Moves(MOVE_DYNAMAX_CANNON, MOVE_FLAMETHROWER, MOVE_SLUDGE_BOMB, MOVE_RECOVER); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); }
    } THEN {
        EXPECT_EQ(opponent->species, SPECIES_ETERNATUS_ETERNAMAX);
        EXPECT_EQ(opponent->ability, ABILITY_PRESSURE);
        EXPECT_EQ(opponent->moves[0], MOVE_ETERNABEAM);
        EXPECT_EQ(opponent->moves[1], MOVE_FLAMETHROWER);
        EXPECT_EQ(opponent->moves[2], MOVE_SLUDGE_BOMB);
        EXPECT_EQ(opponent->moves[3], MOVE_RECOVER);
        EXPECT_EQ(opponent->hp, opponent->maxHP);
        EXPECT_EQ(GetBreakBarCount(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT)), 0);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
        EXPECT_EQ(GetActiveGimmick(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT)), GIMMICK_NONE);
    }
}

// The move in the slot at resolution is what executes, so a Calyrex action already queued under the
// Ice Rider kit resolves as the Shadow Rider kit's move in that slot. Whichever slot the AI chose,
// the move it ends up using must come from the new kit.
AI_SINGLE_BATTLE_TEST("Koanko Calyrex queued action resolves from the new kit after the bar breaks")
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC);
        sFormsSetup = TRUE;
        sFormsSeedStage = 0;
        sDynamicAiFunc = KoankoFormsSeed;
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(2000); HP(9999); MaxHP(9999); Moves(MOVE_SEISMIC_TOSS); }
        OPPONENT(SPECIES_CALYREX_ICE) { Level(102); Speed(1); HP(1);
            Moves(MOVE_GLACIAL_LANCE, MOVE_HIGH_HORSEPOWER, MOVE_TERA_BLAST, MOVE_SWORDS_DANCE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); }
    } THEN {
        u32 used = gLastMoves[GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT)];
        EXPECT_EQ(opponent->species, SPECIES_CALYREX_SHADOW);
        EXPECT(used == MOVE_ASTRAL_BARRAGE || used == MOVE_PSYSHOCK
            || used == MOVE_AURA_SPHERE || used == MOVE_NASTY_PLOT);
    }
}
