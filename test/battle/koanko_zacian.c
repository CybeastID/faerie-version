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
#include "battle_interface.h"
#include "battle_anim.h"
#include "battle_reduction.h"
#include "battle_sprite_scale.h"
#include "event_data.h"
#include "battle_util.h"
#include "sprite.h"
#include "malloc.h"
#include "pokemon.h"
#include "party_menu.h"
#include "battle_main.h"
#include "main.h"
#include "reshow_battle_screen.h"
#include "task.h"
#include "window.h"

// Owner canaries and live entry/action/retirement regressions.

static void AllySetUp(void)
{
    gBattleStruct = AllocZeroed(sizeof(*gBattleStruct));
    gBattleTypeFlags = BATTLE_TYPE_DOUBLE; // not LINK, not MULTI, not INGAME_PARTNER
    gBattlersCount = 4;
    gBattlerPartyIndexes[0] = 0;
    gBattlerPartyIndexes[1] = 0;
    gBattlerPartyIndexes[2] = 0;
    gBattlerPartyIndexes[3] = 0;
    gAbsentBattlerFlags = 1u << 3;
    for (u32 i = 0; i < MAX_BATTLERS_COUNT; i++)
    {
        gBattlerPositions[i] = i;
        memset(&gBattleMons[i], 0, sizeof(gBattleMons[i]));
    }
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

TEST("Koanko Zacian party menu orders the six real mons without the ally local index")
{
    AllySetUp();
    TempAllySetState(TEMP_ALLY_ACTIVE);
    gBattlerPartyIndexes[0] = 1;
    BufferBattlePartyCurrentOrderBySide(0, B_FLANK_LEFT);
    EXPECT_EQ(gBattleStruct->battlerPartyOrders[0][0], 0x10);
    EXPECT_EQ(gBattleStruct->battlerPartyOrders[0][1], 0x23);
    EXPECT_EQ(gBattleStruct->battlerPartyOrders[0][2], 0x45);
    EXPECT_EQ(gBattleStruct->battlerPartyOrders[2][0], 0);
    ResetSentPokesToOpponentValue();
    UpdateSentPokesToOpponentValue(2);
    EXPECT_EQ(gSentPokesToOpponent[0], 1u << 1);
    AllyTearDown();
}

TEST("Koanko Zacian keeps the opponent centered with reserves and an empty right position")
{
    AllySetUp();
    u32 savedPlayerCount = gPartiesCount[B_TRAINER_PLAYER];
    u32 savedOpponentCount = gPartiesCount[B_TRAINER_OPPONENT_A];
    gPartiesCount[B_TRAINER_PLAYER] = gPartiesCount[B_TRAINER_OPPONENT_A] = PARTY_SIZE;

    // Ordinary doubles still use two opponent positions.
    EXPECT_EQ(GetBattlerCoordsIndex(1), BATTLE_COORDS_DOUBLES);
    EXPECT_EQ(GetBattlerSpriteCoord(1, BATTLER_COORD_X_2), 200);
    for (u32 state = TEMP_ALLY_ENTERING; state <= TEMP_ALLY_DEPARTING; state++)
    {
        TempAllySetState(state);
        EXPECT_EQ(GetBattlerCoordsIndex(0), BATTLE_COORDS_DOUBLES);
        EXPECT_EQ(GetBattlerCoordsIndex(2), BATTLE_COORDS_DOUBLES);
        EXPECT_EQ(GetBattlerCoordsIndex(1), BATTLE_COORDS_SINGLES);
        EXPECT_EQ(GetBattlerSpriteCoord(1, BATTLER_COORD_X_2), 176);
        EXPECT_EQ(GetBattlerSpriteCoord2(1, BATTLER_COORD_X_2), 176);
    }
    TempAllySetState(TEMP_ALLY_NONE);
    EXPECT_EQ(GetBattlerCoordsIndex(1), BATTLE_COORDS_DOUBLES);
    gPartiesCount[B_TRAINER_PLAYER] = savedPlayerCount;
    gPartiesCount[B_TRAINER_OPPONENT_A] = savedOpponentCount;
    AllyTearDown();
}

TEST("Koanko Zacian cleanup is idempotent and preserves the real owner and Reduction")
{
    AllySetUp();
    TempAllySetState(TEMP_ALLY_ACTIVE);
    AllySetMon(&gParties[B_TRAINER_PLAYER][0], SPECIES_WOBBUFFET, 100);
    AllySetMon(&gParties[B_TRAINER_PARTNER][0], SPECIES_ZACIAN_CROWNED, 100);
    gBattleStruct->reduction.active = TRUE;
    gBattleStruct->reduction.nextLoss = 9;
    gBattleStruct->partyState[B_TRAINER_PLAYER][0].timesGotHit = 3;
    gBattlerAttacker = gBattlerTarget = gEffectBattler = gBattleScripting.battler = TEMP_ALLY_BATTLER;
    TempAllyFinishBattle();
    TempAllyFinishBattle();
    EXPECT_EQ(TempAllyGetState(), TEMP_ALLY_NONE);
    EXPECT_EQ(gBattlersCount, 2);
    EXPECT_EQ(gBattleTypeFlags & BATTLE_TYPE_DOUBLE, 0);
    EXPECT_EQ(gAbsentBattlerFlags & 12, 12);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP), 100);
    EXPECT_EQ((u32)gBattleStruct->partyState[B_TRAINER_PLAYER][0].timesGotHit, 3);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PARTNER][0], MON_DATA_SPECIES), SPECIES_NONE);
    EXPECT_EQ(gBattleStruct->reduction.active, TRUE);
    EXPECT_EQ(gBattleStruct->reduction.nextLoss, 9);
    EXPECT_LT(gBattlerAttacker, gBattlersCount);
    EXPECT_LT(gBattlerTarget, gBattlersCount);
    EXPECT_LT(gEffectBattler, gBattlersCount);
    EXPECT_LT(gBattleScripting.battler, gBattlersCount);
    AllyTearDown();
}

TEST("Koanko Zacian abort cancels an in-progress entry task before freeing its owner")
{
    AllySetUp();
    TempAllySetState(TEMP_ALLY_ENTERING);
    struct BattleSpriteData *saved = gBattleSpritesDataPtr;
    gBattleSpritesDataPtr = AllocZeroed(sizeof(*gBattleSpritesDataPtr));
    gBattleSpritesDataPtr->healthBoxesData = AllocZeroed(sizeof(*gBattleSpritesDataPtr->healthBoxesData) * MAX_BATTLERS_COUNT);
    DoTempAllyZipInAnimation(2);
    EXPECT_EQ(gDoingBattleAnim, TRUE);
    // Match the recorded abort's ordering: graphics disappear before the
    // FreeBattleResources data cleanup. The task must still be cancelled.
    Free(gBattleSpritesDataPtr->healthBoxesData);
    Free(gBattleSpritesDataPtr);
    gBattleSpritesDataPtr = NULL;
    TempAllyFinishBattle();
    EXPECT_EQ(gDoingBattleAnim, FALSE);
    for (u32 taskId = 0; taskId < NUM_TASKS; taskId++)
        EXPECT_EQ(gTasks[taskId].isActive, FALSE);
    EXPECT_EQ(TempAllyGetState(), TEMP_ALLY_NONE);
    gBattleSpritesDataPtr = saved;
    AllyTearDown();
}

extern AiScoreFunc sDynamicAiFunc;
static bool8 sZacianSetup;
static u8 sZacianScenario;
static bool8 sSawSinglesBeforeDialga;
static bool8 sSawAllyPressure;
static bool8 sSawAllyCanaries;
static bool8 sSawReducedAlly;
static bool8 sSawCenteredReshow;
static bool8 sSawRightDash;
static bool8 sSawDashBehindUi;
static bool8 sSawDashOffscreen;
static bool8 sSawPlayerRecentering;
static bool8 sBadRecenterStep;
static bool8 sCenteredBeforeDialga;
static s16 sLastPlayerX;
static BattleControllerFunc sReshowController;

enum
{
    ZACIAN_ARRIVAL,
    ZACIAN_PRESSURE,
    ZACIAN_DEPART,
    ZACIAN_FAINT,
    ZACIAN_PLAYER_LOSS,
    ZACIAN_FAINT_DEPART,
    ZACIAN_RECOIL_DEPART,
    ZACIAN_RESIDUAL_DEPART,
    ZACIAN_RESHOW_DEPART,
    ZACIAN_REDUCTION_DEPART,
};

static bool32 OpponentHasCenteredLayout(void)
{
    return GetBattlerCoordsIndex(1) == BATTLE_COORDS_SINGLES
        && GetBattlerSpriteCoord(1, BATTLER_COORD_X_2) == 176
        && gSprites[gBattlerSpriteIds[1]].x == 176
        && gSprites[gBattlerSpriteIds[1]].y == GetBattlerSpriteDefault_Y(1);
}

static void WaitForZacianReshow(enum BattlerId battler)
{
    if (gMain.callback2 == BattleMainCB2)
    {
        sSawCenteredReshow = OpponentHasCenteredLayout();
        gBattlerControllerFuncs[battler] = sReshowController;
        MarkBattleControllerIdleOnLocal(battler);
    }
}

static void Task_ReshowZacianFormation(u8 taskId)
{
    if (TempAllyGetState() == TEMP_ALLY_ACTIVE && gBattleControllerExecFlags == 0)
    {
        sReshowController = gBattlerControllerFuncs[0];
        gBattlerControllerFuncs[0] = WaitForZacianReshow;
        MarkBattlerForControllerExec(0);
        CloseMainBattleScreen();
        ReshowBattleScreenAfterMenu();
        DestroyTask(taskId);
    }
}

static void Task_ObserveZacianDeparture(u8 taskId)
{
    u32 spriteId = gBattlerSpriteIds[2];
    if (TempAllyGetState() == TEMP_ALLY_DEPARTING && gBattleMons[2].hp != 0
     && spriteId < MAX_SPRITES && gSprites[spriteId].inUse)
    {
        struct Sprite *ally = &gSprites[spriteId];
        sSawRightDash |= ally->x > 90;
        if (ally->x > 170 && ally->x < DISPLAY_WIDTH)
            sSawDashBehindUi |= ally->oam.priority > gSprites[gHealthboxSpriteIds[0]].oam.priority
                && ally->oam.priority > gSprites[gHealthboxSpriteIds[2]].oam.priority;
        sSawDashOffscreen |= ally->x >= DISPLAY_WIDTH + MON_PIC_WIDTH / 2;
    }
    spriteId = gBattlerSpriteIds[0];
    if (sSawAllyCanaries && TempAllyGetState() == TEMP_ALLY_NONE && gBattleMons[0].hp != 0
     && spriteId < MAX_SPRITES && gSprites[spriteId].inUse)
    {
        s16 x = gSprites[spriteId].x;
        sSawPlayerRecentering |= x > 32 && x < 72;
        sBadRecenterStep |= x < sLastPlayerX || x - sLastPlayerX > 2;
        sLastPlayerX = x;
    }
    if (gBattleMons[1].species == SPECIES_DIALGA_ORIGIN)
    {
        sCenteredBeforeDialga = gBattleMons[0].hp == 0
            || (spriteId < MAX_SPRITES && gSprites[spriteId].x == 72);
        DestroyTask(taskId);
    }
}

static s32 ZacianArrivalSeed(u32 attacker, u32 target, u32 move, s32 score)
{
    if (sZacianSetup)
    {
        sZacianSetup = FALSE;
        u32 battler = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);
        gBattleStruct->koankoPhase = KOANKO_CONJURED_TEAM;
        // In the real encounter, the defeated solo TX is still registered.
        gBattleStruct->txRollback.combatProfile = TX_PROFILE_KOANKO_SOLO;
        gBattleStruct->txRollback.ownerTrainer = B_TRAINER_OPPONENT_A;
        gBattleStruct->txRollback.partyIndex = 1;
        gBattleStruct->txRollback.ownerDefeated = TRUE;
        const struct BreakBarConfig *config = ResolveTrainerBreakBar(&gKoankoConjuredBreakBars, gBattleMons[battler].species, 0);
        if (config != NULL)
            ActivateBreakBar(battler, config);
        if (sZacianScenario == ZACIAN_RESHOW_DEPART)
            CreateTask(Task_ReshowZacianFormation, 0);
        if (sZacianScenario == ZACIAN_REDUCTION_DEPART)
            ReductionTryActivate(1, B_SIDE_PLAYER);
        if (sZacianScenario == ZACIAN_DEPART || sZacianScenario == ZACIAN_FAINT_DEPART
         || sZacianScenario == ZACIAN_RECOIL_DEPART || sZacianScenario == ZACIAN_REDUCTION_DEPART
         || sZacianScenario == ZACIAN_RESHOW_DEPART || sZacianScenario == ZACIAN_RESIDUAL_DEPART)
            CreateTask(Task_ObserveZacianDeparture, 1);
    }
    if (TempAllyGetState() != TEMP_ALLY_NONE)
    {
        sSawAllyCanaries = GetBattlerMon(2) == &gParties[B_TRAINER_PARTNER][0]
            && !BattlersShareParty(0, 2) && !CanBattlerSwitch(2)
            && !CanActivateGimmick(2, GIMMICK_TERA)
            && (gAbsentBattlerFlags & (1u << 3));
        if (gBattleMons[2].pp[0] < GetMovePP(MOVE_BEHEMOTH_BLADE))
            sSawAllyPressure = TRUE;
        if (gBattleMons[2].level < 100 && GetMonData(GetBattlerMon(2), MON_DATA_LEVEL) == 100)
            sSawReducedAlly = TRUE;
        if ((sZacianScenario == ZACIAN_DEPART || sZacianScenario == ZACIAN_FAINT_DEPART || sZacianScenario == ZACIAN_RECOIL_DEPART
          || sZacianScenario == ZACIAN_RESHOW_DEPART || sZacianScenario == ZACIAN_REDUCTION_DEPART)
         && gBattleResults.battleTurnCounter == 2)
        {
            u16 hp = 1;
            gBattleMons[1].hp = hp;
            SetMonData(GetBattlerMon(1), MON_DATA_HP, &hp);
            if (sZacianScenario == ZACIAN_RECOIL_DEPART)
            {
                gBattleMons[2].hp = hp;
                SetMonData(GetBattlerMon(2), MON_DATA_HP, &hp);
            }
        }
        if (sZacianScenario == ZACIAN_RESIDUAL_DEPART && gBattleResults.battleTurnCounter == 1)
        {
            u16 hp = 1;
            gBattleMons[1].hp = hp;
            SetMonData(GetBattlerMon(1), MON_DATA_HP, &hp);
        }
    }
    if (gBattleMons[1].species == SPECIES_DIALGA_ORIGIN)
        sSawSinglesBeforeDialga = TempAllyGetState() == TEMP_ALLY_NONE && gBattlersCount == 2 && !IsDoubleBattle();
    if (attacker == 1 && sZacianScenario != ZACIAN_ARRIVAL)
    {
        if (gBattleResults.battleTurnCounter == 0
         || (sZacianScenario != ZACIAN_FAINT && sZacianScenario != ZACIAN_PLAYER_LOSS && sZacianScenario != ZACIAN_FAINT_DEPART)
         || (sZacianScenario == ZACIAN_FAINT_DEPART && gBattleResults.battleTurnCounter > 1))
            return move == MOVE_CELEBRATE ? 200 : 0;
        if (sZacianScenario == ZACIAN_FAINT || sZacianScenario == ZACIAN_FAINT_DEPART)
        {
            gBattleMons[1].spAttack = 3000; // Form recalculation replaces GIVEN's stat override.
            return move == MOVE_FLAMETHROWER && target == 2 ? 200 : 0;
        }
        if (sZacianScenario == ZACIAN_PLAYER_LOSS)
            return move == MOVE_NIGHT_SHADE && target == 0 ? 200 : 0;
    }
    return AI_KoankoTrainer(attacker, target, move, score);
}

static void ZacianFixture(u32 scenario)
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    sZacianSetup = TRUE;
    sZacianScenario = scenario;
    sSawSinglesBeforeDialga = sSawAllyPressure = sSawAllyCanaries = FALSE;
    sSawReducedAlly = FALSE;
    sSawCenteredReshow = FALSE;
    sSawRightDash = sSawDashBehindUi = sSawDashOffscreen = FALSE;
    sSawPlayerRecentering = sBadRecenterStep = sCenteredBeforeDialga = FALSE;
    sLastPlayerX = 32;
    sDynamicAiFunc = ZacianArrivalSeed;
}

// One decision: the Eternamax bar breaks. Everything about the arrival is read afterwards.
AI_SINGLE_BATTLE_TEST("Koanko Zacian arrives beside the player right after Eternatus's first bar breaks")
{
    
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC);
        ZacianFixture(ZACIAN_ARRIVAL);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(3000); HP(9999); MaxHP(9999); Moves(MOVE_SEISMIC_TOSS); }
        OPPONENT(SPECIES_ETERNATUS) { Level(103); Speed(2000); HP(1);
            Moves(MOVE_DYNAMAX_CANNON, MOVE_FLAMETHROWER, MOVE_SLUDGE_BOMB, MOVE_CELEBRATE); }
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
        EXPECT_EQ(gBattleMons[2].pp[0], GetMovePP(MOVE_BEHEMOTH_BLADE)); // No arrival-turn attack.
        EXPECT_EQ(GetBattlerMon(2), &gParties[B_TRAINER_PARTNER][0]);
        EXPECT_EQ(gAbsentBattlerFlags & (1u << 2), 0);
        EXPECT(gAbsentBattlerFlags & (1u << 3));
        // The real player party is untouched.
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES), SPECIES_WOBBUFFET);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_SPECIES), SPECIES_NONE);
        // A visually small player does not make the independently owned
        // partner small. Rendering also preserves the asymmetric formation.
        bool32 wasSmol = FlagGet(FLAG_PLAYER_SMOL);
        u32 oldOutcome = gBattleOutcome;
        // The recorded fixture has already teleported out for THEN. Exercise
        // an in-battle flag change rather than the intentional exit-size freeze.
        gBattleOutcome = 0;
        FlagSet(FLAG_PLAYER_SMOL);
        u32 playerScale = BattleSpriteScaleTarget(0);
        u32 allyScale = BattleSpriteScaleTarget(2);
        for (u32 frame = 0; frame < 64; frame++) BattleSpriteScaleBuildOamBuffer();
        u32 renderedPlayerScale = BattleSpriteScaleCurrent(0);
        u32 renderedAllyScale = BattleSpriteScaleCurrent(2);
        if (!wasSmol) FlagClear(FLAG_PLAYER_SMOL);
        gBattleOutcome = oldOutcome;
        EXPECT_EQ(playerScale, BATTLE_SPRITE_SCALE_SMOL);
        EXPECT_EQ(allyScale, BATTLE_SPRITE_SCALE_ONE);
        EXPECT_EQ(renderedPlayerScale, BATTLE_SPRITE_SCALE_SMOL);
        EXPECT_EQ(renderedAllyScale, BATTLE_SPRITE_SCALE_ONE);
        EXPECT(OpponentHasCenteredLayout());
    }
}

AI_SINGLE_BATTLE_TEST("Koanko Zacian independently attacks on the next turn and spends its own PP")
{
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC);
        ZacianFixture(ZACIAN_PRESSURE);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(3000); HP(9999); MaxHP(9999); MovesWithPP({MOVE_SEISMIC_TOSS, 5}, {MOVE_CELEBRATE, 5}); }
        PLAYER(SPECIES_ABRA) { Speed(1); Item(ITEM_LEFTOVERS); MovesWithPP({MOVE_TELEPORT, 3}); }
        OPPONENT(SPECIES_ETERNATUS) { Level(103); Speed(2000); HP(1); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_DIALGA_ORIGIN) { Speed(2000); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); }
        SUMMONED_ALLY();
        TURN { MOVE(player, MOVE_CELEBRATE); EXPECT_MOVE(&gBattleMons[2], MOVE_BEHEMOTH_BLADE, target: opponent); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_BEHEMOTH_BLADE, &gBattleMons[2]);
        HP_BAR(opponent);
    } THEN {
        EXPECT(sSawAllyCanaries);
        EXPECT(OpponentHasCenteredLayout());
        EXPECT_EQ(gBattleMons[2].pp[0], GetMovePP(MOVE_BEHEMOTH_BLADE) - 2); // Eternamax's Pressure.
        EXPECT_EQ(GetMonData(GetBattlerMon(2), MON_DATA_PP1), GetMovePP(MOVE_BEHEMOTH_BLADE) - 2);
        EXPECT_EQ(player->pp[0], 3);
        EXPECT_EQ(player->pp[1], 4);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_SPECIES), SPECIES_ABRA);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_HELD_ITEM), ITEM_LEFTOVERS);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_PP1), 3);
        EXPECT_EQ(TxRollbackGetSlotPolicy(3), TX_SLOT_DISABLED);
        EXPECT_EQ(GetActiveGimmick(2), GIMMICK_NONE);
    } AFTER_CLEANUP {
        EXPECT_EQ(TempAllyGetState(), TEMP_ALLY_NONE);
        EXPECT_EQ(gBattlersCount, 2);
        EXPECT_EQ(gPartiesCount[B_TRAINER_PARTNER], 0);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PARTNER][0], MON_DATA_SPECIES), SPECIES_NONE);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_SPECIES), SPECIES_ABRA);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_HELD_ITEM), ITEM_LEFTOVERS);
    }
}

AI_SINGLE_BATTLE_TEST("Koanko Zacian remains independent while the real player replaces a fainted mon")
{
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC);
        ZacianFixture(ZACIAN_PRESSURE);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(3000); HP(9999); MaxHP(9999); Moves(MOVE_SEISMIC_TOSS, MOVE_MEMENTO); }
        PLAYER(SPECIES_ABRA) { Speed(1); Item(ITEM_LEFTOVERS); MovesWithPP({MOVE_CELEBRATE, 5}); }
        OPPONENT(SPECIES_ETERNATUS) { Level(103); Speed(2000); HP(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); }
        SUMMONED_ALLY();
        TURN { MOVE(player, MOVE_MEMENTO); SEND_OUT(player, 1); }
    } THEN {
        EXPECT_EQ(gBattlerPartyIndexes[0], 1);
        EXPECT_EQ(player->species, SPECIES_ABRA);
        EXPECT_EQ(TempAllyGetState(), TEMP_ALLY_ACTIVE);
        EXPECT_EQ(gBattlerPartyIndexes[2], 0);
        EXPECT_EQ(GetBattlerMon(2), &gParties[B_TRAINER_PARTNER][0]);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP), 0);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_PP1), 5);
        EXPECT_EQ(gAbsentBattlerFlags & (1u << 3), 1u << 3);
    }
}

AI_SINGLE_BATTLE_TEST("Koanko Zacian chooses a legal attack after Behemoth Blade is disabled")
{
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC);
        ZacianFixture(ZACIAN_PRESSURE);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(3000); HP(9999); MaxHP(9999); Moves(MOVE_SEISMIC_TOSS, MOVE_DISABLE, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ETERNATUS) { Level(103); Speed(2000); HP(1); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_DIALGA_ORIGIN) { Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); }
        SUMMONED_ALLY();
        TURN { MOVE(player, MOVE_CELEBRATE); EXPECT_MOVE(&gBattleMons[2], MOVE_BEHEMOTH_BLADE, target: opponent); }
        TURN { MOVE(player, MOVE_DISABLE, target: &gBattleMons[2]); }
        TURN { MOVE(player, MOVE_CELEBRATE); EXPECT_MOVE(&gBattleMons[2], MOVE_PLAY_ROUGH, target: opponent); }
    } THEN {
        EXPECT_NE(gBattleMons[2].pp[2], GetMovePP(MOVE_PLAY_ROUGH));
        EXPECT_NE(gBattleMons[2].pp[0], 0);
    }
}

AI_SINGLE_BATTLE_TEST("Koanko Zacian departs and restores singles before Dialga enters")
{
    u32 scenario;
    PARAMETRIZE { scenario = ZACIAN_DEPART; }
    PARAMETRIZE { scenario = ZACIAN_FAINT_DEPART; }
    PARAMETRIZE { scenario = ZACIAN_RECOIL_DEPART; }
    PARAMETRIZE { scenario = ZACIAN_REDUCTION_DEPART; }
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC | AI_FLAG_SEQUENCE_SWITCHING);
        ZacianFixture(scenario);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(3000); HP(9999); MaxHP(9999); Moves(MOVE_SEISMIC_TOSS, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ETERNATUS) { Level(103); Speed(2000); HP(1); Moves(MOVE_CELEBRATE, MOVE_FLAMETHROWER); }
        OPPONENT(SPECIES_DIALGA_ORIGIN) { Speed(2000); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); }
        SUMMONED_ALLY();
        TURN { MOVE(player, MOVE_CELEBRATE); }
        TURN { MOVE(player, scenario == ZACIAN_RECOIL_DEPART ? MOVE_CELEBRATE : MOVE_SEISMIC_TOSS); EXPECT_SEND_OUT(opponent, 1); }
        TURN { MOVE(player, MOVE_CELEBRATE); }
    } THEN {
        if (scenario != ZACIAN_FAINT_DEPART)
            EXPECT(sSawAllyPressure);
        if (scenario == ZACIAN_DEPART || scenario == ZACIAN_REDUCTION_DEPART)
        {
            EXPECT(sSawRightDash);
            EXPECT(sSawDashBehindUi);
            EXPECT(sSawDashOffscreen);
        }
        else
            EXPECT_EQ(sSawRightDash, FALSE);
        EXPECT(sSawPlayerRecentering);
        EXPECT_EQ(sBadRecenterStep, FALSE);
        EXPECT(sCenteredBeforeDialga);
        EXPECT(sSawSinglesBeforeDialga);
        EXPECT_EQ(gBattlersCount, 2);
        EXPECT_EQ(IsDoubleBattle(), FALSE);
        EXPECT_EQ(opponent->species, SPECIES_DIALGA_ORIGIN);
        EXPECT_EQ(TempAllyGetState(), TEMP_ALLY_NONE);
        EXPECT_EQ(gAbsentBattlerFlags & 12, 12);
        EXPECT_EQ(gBattlerSpriteIds[2], MAX_SPRITES);
        EXPECT_EQ(gHealthboxSpriteIds[2], MAX_SPRITES);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PARTNER][0], MON_DATA_SPECIES), SPECIES_NONE);
        EXPECT_EQ(gPartiesCount[B_TRAINER_PARTNER], 0);
        EXPECT_EQ(GetBattlerCoordsIndex(0), BATTLE_COORDS_SINGLES);
        if (scenario == ZACIAN_REDUCTION_DEPART)
        {
            EXPECT(sSawReducedAlly);
            EXPECT_EQ(ReductionIsActive(), TRUE);
            EXPECT_GT(gBattleStruct->reduction.nextLoss, 1);
            EXPECT_EQ(gBattleStruct->reduction.occupant[2].lostLevels, 0);
            EXPECT_EQ(GetMonData(GetBattlerMon(0), MON_DATA_LEVEL), 100);
            EXPECT_LT(player->level, 100);
        }
    }
}

// Deliberate graphics teardown/recreation without opening a menu. Keep this
// stress case separate from the ordinary departure's graphical playthrough.
AI_SINGLE_BATTLE_TEST("Koanko Zacian menu reshow preserves centered Eternamax and departure")
{
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC | AI_FLAG_SEQUENCE_SWITCHING);
        ZacianFixture(ZACIAN_RESHOW_DEPART);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(3000); HP(9999); MaxHP(9999); Moves(MOVE_SEISMIC_TOSS, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ETERNATUS) { Level(103); Speed(2000); HP(1); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_DIALGA_ORIGIN) { Speed(2000); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); }
        SUMMONED_ALLY();
        TURN { MOVE(player, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); EXPECT_SEND_OUT(opponent, 1); }
        TURN { MOVE(player, MOVE_CELEBRATE); }
    } THEN {
        EXPECT(sSawCenteredReshow);
        EXPECT(sSawRightDash);
        EXPECT(sSawDashBehindUi);
        EXPECT(sSawPlayerRecentering);
        EXPECT_EQ(sBadRecenterStep, FALSE);
        EXPECT(sCenteredBeforeDialga);
        EXPECT(sSawAllyPressure);
        EXPECT(sSawSinglesBeforeDialga);
        EXPECT_EQ(opponent->species, SPECIES_DIALGA_ORIGIN);
        EXPECT_EQ(gBattlersCount, 2);
        EXPECT_EQ(IsDoubleBattle(), FALSE);
        EXPECT_EQ(TempAllyGetState(), TEMP_ALLY_NONE);
        EXPECT_EQ(gBattlerSpriteIds[2], MAX_SPRITES);
        EXPECT_EQ(gHealthboxSpriteIds[2], MAX_SPRITES);
    }
}

AI_SINGLE_BATTLE_TEST("Koanko Zacian can faint without replacing it or ending the player's battle")
{
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC);
        ZacianFixture(ZACIAN_FAINT);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(3000); HP(9999); MaxHP(9999); Moves(MOVE_SEISMIC_TOSS, MOVE_CELEBRATE); }
        PLAYER(SPECIES_ABRA) { Speed(1); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ETERNATUS) { Level(103); SpAttack(3000); Speed(2000); HP(1); Moves(MOVE_CELEBRATE, MOVE_FLAMETHROWER); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); }
        SUMMONED_ALLY();
        TURN { MOVE(player, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_CELEBRATE); }
    } SCENE {
        MESSAGE("Zacian fainted!");
    } THEN {
        EXPECT_EQ(TempAllyGetState(), TEMP_ALLY_FAINTED);
        EXPECT_EQ(gBattleMons[2].hp, 0);
        EXPECT_EQ(gAbsentBattlerFlags & 12, 12);
        EXPECT_EQ(NoAliveMonsForBattlerSide(0), FALSE);
        EXPECT_NE(player->hp, 0);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_SPECIES), SPECIES_ABRA);
        EXPECT_EQ(gBattlerPartyIndexes[0], 0);
    }
}

AI_SINGLE_BATTLE_TEST("Koanko Zacian cannot prevent a real player wipe")
{
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC);
        ZacianFixture(ZACIAN_PLAYER_LOSS);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(3000); HP(100); MaxHP(100); Moves(MOVE_SEISMIC_TOSS, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ETERNATUS) { Level(103); Speed(2000); HP(1); Moves(MOVE_CELEBRATE, MOVE_NIGHT_SHADE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); }
        SUMMONED_ALLY();
        TURN { MOVE(player, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(gBattleOutcome, B_OUTCOME_LOST);
        EXPECT_NE(gBattleMons[2].hp, 0);
        EXPECT_EQ(NoAliveMonsForBattlerSide(0), TRUE);
    }
}

AI_SINGLE_BATTLE_TEST("Koanko Zacian simultaneous faints restore singles before both real replacements")
{
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC | AI_FLAG_SEQUENCE_SWITCHING);
        ZacianFixture(ZACIAN_PRESSURE);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Attack(65000); Speed(3000); HP(9999); MaxHP(9999); Moves(MOVE_SEISMIC_TOSS, MOVE_EXPLOSION); }
        PLAYER(SPECIES_ABRA) { Speed(1); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ETERNATUS) { Level(103); Speed(2000); HP(1); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_DIALGA_ORIGIN) { Speed(2000); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); }
        SUMMONED_ALLY();
        TURN { MOVE(player, MOVE_EXPLOSION); SEND_OUT(player, 1); EXPECT_SEND_OUT(opponent, 1); }
        TURN { MOVE(player, MOVE_CELEBRATE); }
    } THEN {
        EXPECT(sSawSinglesBeforeDialga);
        EXPECT_EQ(gBattlersCount, 2);
        EXPECT_EQ(player->species, SPECIES_ABRA);
        EXPECT_EQ(opponent->species, SPECIES_DIALGA_ORIGIN);
        EXPECT_EQ(gAbsentBattlerFlags & 12, 12);
        EXPECT_EQ(TempAllyGetState(), TEMP_ALLY_NONE);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP), 0);
    }
}

AI_SINGLE_BATTLE_TEST("Koanko Zacian residual final faint restores singles before Dialga replacement")
{
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC | AI_FLAG_SEQUENCE_SWITCHING);
        ZacianFixture(ZACIAN_RESIDUAL_DEPART);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(3000); HP(9999); MaxHP(9999); Moves(MOVE_SEISMIC_TOSS, MOVE_SPORE, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ETERNATUS) { Level(103); Speed(2000); HP(1); Item(ITEM_FLAME_ORB); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_DIALGA_ORIGIN) { Speed(2000); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); }
        SUMMONED_ALLY();
        TURN { MOVE(player, MOVE_SPORE, target: &gBattleMons[2]); EXPECT_SEND_OUT(opponent, 1); }
        TURN { MOVE(player, MOVE_CELEBRATE); }
    } SCENE {
        MESSAGE("The opposing Eternatus was hurt by its burn!");
        MESSAGE("The opposing Eternatus fainted!");
    } THEN {
        EXPECT(sSawSinglesBeforeDialga);
        EXPECT(sSawPlayerRecentering);
        EXPECT_EQ(sBadRecenterStep, FALSE);
        EXPECT(sCenteredBeforeDialga);
        EXPECT_EQ(gBattlersCount, 2);
        EXPECT_EQ(TempAllyGetState(), TEMP_ALLY_NONE);
        EXPECT_EQ(opponent->species, SPECIES_DIALGA_ORIGIN);
        EXPECT_EQ(gAbsentBattlerFlags & 12, 12);
    }
}

static s32 EternamaxDamageSeed(u32 attacker, u32 target, u32 move, s32 score)
{
    gBattleStruct->koankoPhase = KOANKO_CONJURED_TEAM;
    return score;
}

AI_SINGLE_BATTLE_TEST("Koanko Zacian anti-Eternamax power is shared by damage and AI without Dynamax", s16 damage; u32 estimate)
{
    bool32 koanko;
    PARAMETRIZE { koanko = FALSE; }
    PARAMETRIZE { koanko = TRUE; }
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC | AI_FLAG_OMNISCIENT);
        gBattleTestRunnerState->data.recordedBattle.opponentA = koanko ? TRAINER_KOANKO : TRAINER_RAZELUXE;
        sDynamicAiFunc = EternamaxDamageSeed;
        PLAYER(SPECIES_ZACIAN_CROWNED) { Level(100); Speed(3000); Moves(MOVE_BEHEMOTH_BLADE, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ETERNATUS_ETERNAMAX) { Level(103); Speed(2000); HP(9999); MaxHP(9999); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_BEHEMOTH_BLADE); }
    } SCENE {
        HP_BAR(opponent, captureDamage: &results[i].damage);
    } THEN {
        results[i].estimate = gAiLogicData->simulatedDmg[0][1][0].median;
        EXPECT_EQ(GetActiveGimmick(1), GIMMICK_NONE);
        EXPECT_EQ(KoankoIsEternamaxTarget(1), koanko);
    } FINALLY {
        EXPECT_MUL_EQ(results[0].damage, UQ_4_12(2.0), results[1].damage);
        EXPECT_MUL_EQ(results[0].estimate, UQ_4_12(2.0), results[1].estimate);
    }
}

TEST("Koanko Zacian solo TX faint deactivates its owner without summon retirement or losing Reduction")
{
    AllySetUp();
    gBattleStruct->txRollback.combatProfile = TX_PROFILE_KOANKO_SOLO;
    gBattleStruct->txRollback.ownerTrainer = B_TRAINER_OPPONENT_A;
    gBattleStruct->txRollback.partyIndex = 0;
    gBattleStruct->reduction.active = TRUE;
    gBattleStruct->reduction.nextLoss = 7;
    gBattleMons[1].hp = 0;
    TxRollbackOnFaint(1);
    EXPECT_EQ(gBattleStruct->txRollback.combatProfile, TX_PROFILE_KOANKO_SOLO);
    EXPECT_EQ(gBattleStruct->txRollback.ownerDefeated, TRUE);
    EXPECT_EQ(gBattleStruct->txRollback.stage, TX_ROLLBACK_NONE);
    EXPECT_EQ(TxRollbackSummonFormationActive(), FALSE);
    EXPECT_EQ(TxRollbackCanPopulateSlot(1), TRUE);
    EXPECT_EQ(gBattleStruct->reduction.active, TRUE);
    EXPECT_EQ(gBattleStruct->reduction.nextLoss, 7);
    AllyTearDown();
}
