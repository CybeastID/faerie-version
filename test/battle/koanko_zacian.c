#include "global.h"
#include "test/battle.h"
#include "battle_ai_main.h"
#include "battle_break_bar.h"
#include "trainer_break_bars.h"
#include "battle_controllers.h"
#include "battle_gimmick.h"
#include "battle_koanko.h"
#include "battle_script_commands.h"
#include "battle_temp_ally.h"
#include "malloc.h"
#include "pokemon.h"

// "Koanko Zacian" patch 1: temporary-ally OWNERSHIP only (no entry, AI or departure yet).
// AUTHORED WITHOUT A BUILD. The real player party carries canaries that must never change.

static void AllySetUp(void)
{
    gBattleStruct = AllocZeroed(sizeof(*gBattleStruct));
    gBattleTypeFlags = BATTLE_TYPE_DOUBLE; // not LINK, not MULTI, not INGAME_PARTNER
    gBattlersCount = 4;
    gBattlerPartyIndexes[0] = 0;
    gBattlerPartyIndexes[1] = 0;
    gBattlerPartyIndexes[2] = 0;
    gBattlerPartyIndexes[3] = 0;
    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        ZeroMonData(&gParties[B_TRAINER_PLAYER][i]);
        ZeroMonData(&gParties[B_TRAINER_PARTNER][i]);
    }
}

static void AllySetMon(struct Pokemon *mon, u32 species, u32 hp)
{
    SetMonData(mon, MON_DATA_SPECIES, &species);
    SetMonData(mon, MON_DATA_HP, &hp);
}

static void AllyTearDown(void)
{
    Free(gBattleStruct);
    gBattleStruct = NULL;
    gBattleTypeFlags = 0;
}

TEST("Koanko Zacian ownership routes player-right to the partner row only while active")
{
    AllySetUp();
    AllySetMon(&gParties[B_TRAINER_PLAYER][0], SPECIES_WOBBUFFET, 10);

    // Inactive: ordinary doubles, player-right shares the real party.
    EXPECT_EQ(GetBattlerTrainer(2), B_TRAINER_PLAYER);
    EXPECT_EQ(GetBattlerMon(2), &gParties[B_TRAINER_PLAYER][0]);
    EXPECT_EQ(TrainerHasParty(B_TRAINER_PARTNER), FALSE);

    TempAllySetState(TEMP_ALLY_ACTIVE);
    EXPECT_EQ(GetBattlerTrainer(0), B_TRAINER_PLAYER);
    EXPECT_EQ(GetBattlerTrainer(1), B_TRAINER_OPPONENT_A);
    EXPECT_EQ(GetBattlerTrainer(2), B_TRAINER_PARTNER);
    EXPECT_EQ(GetBattlerTrainer(3), B_TRAINER_OPPONENT_A);
    EXPECT_EQ(GetBattlerMon(0), &gParties[B_TRAINER_PLAYER][0]);
    EXPECT_EQ(GetBattlerMon(2), &gParties[B_TRAINER_PARTNER][0]);
    EXPECT_EQ(GetBattlerParty(2), &gParties[B_TRAINER_PARTNER][0]);
    EXPECT_NE(GetBattlerPartyState(2), GetBattlerPartyState(0));
    EXPECT_EQ(BattlersShareParty(0, 2), FALSE);
    EXPECT_EQ(TrainerHasParty(B_TRAINER_PARTNER), TRUE);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES), SPECIES_WOBBUFFET);
    AllyTearDown();
}

TEST("Koanko Zacian ownership keeps gimmick, party state and faint counters off the player")
{
    AllySetUp();
    TempAllySetState(TEMP_ALLY_ACTIVE);

    SetActiveGimmick(0, GIMMICK_TERA);
    EXPECT_EQ(GetActiveGimmick(0), GIMMICK_TERA);
    EXPECT_EQ(GetActiveGimmick(2), GIMMICK_NONE);
    SetActiveGimmick(2, GIMMICK_DYNAMAX);
    EXPECT_EQ(GetActiveGimmick(0), GIMMICK_TERA);

    GetBattlerPartyState(0)->sentOut = TRUE;
    EXPECT_EQ((u32)GetBattlerPartyState(2)->sentOut, FALSE);

    gBattleStruct->faintCounter[GetBattlerTrainer(2)]++;
    EXPECT_EQ(gBattleStruct->faintCounter[B_TRAINER_PLAYER], 0);
    EXPECT_EQ(gBattleStruct->faintCounter[B_TRAINER_PARTNER], 1);
    AllyTearDown();
}

TEST("Koanko Zacian ownership leaves loss authority with the real player party")
{
    AllySetUp();
    AllySetMon(&gParties[B_TRAINER_PLAYER][0], SPECIES_WOBBUFFET, 0);
    AllySetMon(&gParties[B_TRAINER_PARTNER][0], SPECIES_ZACIAN_CROWNED, 100);
    TempAllySetState(TEMP_ALLY_ACTIVE);

    EXPECT_EQ(BattleSideHasTwoTrainers(B_SIDE_PLAYER), FALSE);
    // The real party is exhausted; a surviving ally cannot sustain the encounter.
    EXPECT_EQ(NoAliveMonsForBattlerSide(0), TRUE);
    AllyTearDown();
}

TEST("Koanko Zacian ownership clears only the partner row")
{
    AllySetUp();
    AllySetMon(&gParties[B_TRAINER_PLAYER][0], SPECIES_WOBBUFFET, 10);
    AllySetMon(&gParties[B_TRAINER_PARTNER][0], SPECIES_ZACIAN_CROWNED, 100);
    TempAllySetState(TEMP_ALLY_ACTIVE);
    GetBattlerPartyState(2)->sentOut = TRUE;
    SetActiveGimmick(2, GIMMICK_DYNAMAX);
    gBattleStruct->partyState[B_TRAINER_PLAYER][0].timesGotHit = 3;

    struct AiPartyData *saved = gAiPartyData;
    gAiPartyData = NULL;
    TempAllyClearStorage();
    TempAllySetState(TEMP_ALLY_NONE);
    gAiPartyData = saved;

    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PARTNER][0], MON_DATA_SPECIES), SPECIES_NONE);
    EXPECT_EQ((u32)gBattleStruct->partyState[B_TRAINER_PARTNER][0].sentOut, FALSE);
    EXPECT_EQ(gBattleStruct->gimmick.activeGimmick[B_TRAINER_PARTNER][0], GIMMICK_NONE);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES), SPECIES_WOBBUFFET);
    EXPECT_EQ((u32)gBattleStruct->partyState[B_TRAINER_PLAYER][0].timesGotHit, 3);
    EXPECT_EQ(GetBattlerTrainer(2), B_TRAINER_PLAYER);
    AllyTearDown();
}

extern AiScoreFunc sDynamicAiFunc;
static bool8 sZacianSetup;

static s32 ZacianArrivalSeed(u32 attacker, u32 target, u32 move, s32 score)
{
    if (sZacianSetup)
    {
        sZacianSetup = FALSE;
        u32 battler = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);
        gBattleStruct->koankoPhase = KOANKO_CONJURED_TEAM;
        const struct BreakBarConfig *config = ResolveTrainerBreakBar(&gKoankoConjuredBreakBars, gBattleMons[battler].species, 0);
        if (config != NULL)
            ActivateBreakBar(battler, config);
    }
    return score;
}

// One decision: the Eternamax bar breaks. Everything about the arrival is read afterwards.
AI_SINGLE_BATTLE_TEST("Koanko Zacian arrives beside the player right after Eternatus's first bar breaks")
{
    
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC);
        sZacianSetup = TRUE;
        sDynamicAiFunc = ZacianArrivalSeed;
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(1); HP(9999); MaxHP(9999); Moves(MOVE_SEISMIC_TOSS); }
        OPPONENT(SPECIES_ETERNATUS) { Level(103); Speed(2000); HP(1);
            Moves(MOVE_DYNAMAX_CANNON, MOVE_FLAMETHROWER, MOVE_SLUDGE_BOMB, MOVE_RECOVER); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); }
    } THEN {
        EXPECT_EQ(opponent->species, SPECIES_ETERNATUS_ETERNAMAX);
        EXPECT_EQ(TempAllyGetState(), TEMP_ALLY_ACTIVE);
        EXPECT_EQ(gBattlersCount, MAX_BATTLERS_COUNT);
        EXPECT_EQ(gBattleMons[2].species, SPECIES_ZACIAN_CROWNED);
        EXPECT_EQ(gBattleMons[2].level, 100);
        EXPECT_EQ(gBattleMons[2].item, ITEM_LIFE_ORB);
        EXPECT_EQ(gBattleMons[2].ability, ABILITY_INTREPID_SWORD);
        EXPECT_EQ(gBattleMons[2].moves[0], MOVE_BEHEMOTH_BLADE);
        EXPECT_EQ(gBattleMons[2].hp, gBattleMons[2].maxHP);
        EXPECT_EQ(GetBattlerMon(2), &gParties[B_TRAINER_PARTNER][0]);
        EXPECT_EQ(gAbsentBattlerFlags & (1u << 2), 0);
        EXPECT(gAbsentBattlerFlags & (1u << 3));
        // The real player party is untouched.
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES), SPECIES_WOBBUFFET);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_SPECIES), SPECIES_NONE);
    }
}
