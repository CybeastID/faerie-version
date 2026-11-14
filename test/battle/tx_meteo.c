#include "global.h"
#include "castoria_name_theft.h"
#include "test/battle.h"
#include "battle_ai_main.h"
#include "battle_koanko.h"
#include "battle_setup.h"
#include "battle_tx_rollback.h"
#include "battle_util.h"
#include "malloc.h"
#include "pokemon.h"

// Arcanum is capped at four per Caster: AllMovesUnusable() tracks move slots in a u8 mask.
STATIC_ASSERT(CASTORIA_ARCANUM_COUNT == 4, CastoriaArcanumStaysFour);
STATIC_ASSERT(MAX_MON_MOVES + CASTORIA_ARCANUM_COUNT <= 8, ArcanumSlotsFitU8Mask);
STATIC_ASSERT(MAX_MON_MOVES + TX_ARCANUM_COUNT <= 8, TxSlotsFitU8Mask);

extern AiScoreFunc sDynamicAiFunc;

static void MeteoUnitFixture(u32 profile)
{
    gBattleStruct = AllocZeroed(sizeof(*gBattleStruct));
    gBattlersCount = 2;
    gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    gAbsentBattlerFlags = 0;
    memset(gBattleMons, 0, sizeof(gBattleMons));
    for (u32 b = 0; b < MAX_BATTLERS_COUNT; b++)
    {
        gBattlerPositions[b] = b;
        gBattlerPartyIndexes[b] = b == B_POSITION_OPPONENT_RIGHT ? 5 : 0;
        gBattleMons[b].hp = gBattleMons[b].maxHP = 400;
    }
    gBattleStruct->txRollback.combatProfile = profile;
    gBattleStruct->txRollback.ownerTrainer = B_TRAINER_OPPONENT_A;
    gBattleStruct->txRollback.partyIndex = 0;
    if (profile == TX_PROFILE_CASTORIA_SUMMON)
    {
        gBattleStruct->txRollback.stage = TX_ROLLBACK_ACTIVE;
        gBattleStruct->txRollback.partyIndex = 5;
        gBattlersCount = 4; // The summon's owner position (3) only exists in the expanded formation.
    }
    gBattleResults.battleTurnCounter = 0;
}

TEST("Meteo is one of Castoria's four Arcanum and no slot is duplicated")
{
    u32 meteo = 0;
    for (u32 i = 0; i < CASTORIA_ARCANUM_COUNT; i++)
    {
        u16 move = Test_GetCastoriaArcanumMove(i);
        EXPECT_NE(move, MOVE_NONE);
        if (move == MOVE_ARCANUM_METEO)
            meteo++;
        for (u32 j = i + 1; j < CASTORIA_ARCANUM_COUNT; j++)
            EXPECT_NE(move, Test_GetCastoriaArcanumMove(j));
    }
    EXPECT_EQ(meteo, 1);
    EXPECT_EQ(Test_GetCastoriaArcanumMove(CASTORIA_ARCANUM_COUNT), MOVE_NONE);
}

TEST("Meteo data is native Stellar special 150 BP hitting both opponents")
{
    EXPECT_EQ(GetMoveType(MOVE_ARCANUM_METEO), TYPE_STELLAR);
    EXPECT_EQ(GetMoveCategory(MOVE_ARCANUM_METEO), DAMAGE_CATEGORY_SPECIAL);
    EXPECT_EQ(GetMovePower(MOVE_ARCANUM_METEO), 150);
    EXPECT_EQ(GetMoveAccuracy(MOVE_ARCANUM_METEO), 100);
    EXPECT_EQ(GetMoveTarget(MOVE_ARCANUM_METEO), TARGET_BOTH);
    EXPECT_EQ(GetMovePriority(MOVE_ARCANUM_METEO), 0);
    EXPECT_EQ(GetMoveAdditionalEffectCount(MOVE_ARCANUM_METEO), 0);
}

TEST("Meteo TX virtual slots: Wish at 4, Meteo at 5, nothing else, both profiles")
{
    for (u32 profile = TX_PROFILE_CASTORIA_SUMMON; profile <= TX_PROFILE_KOANKO_SOLO; profile++)
    {
        MeteoUnitFixture(profile);
        u32 owner = profile == TX_PROFILE_KOANKO_SOLO ? 1 : 3;
        EXPECT_EQ(TxRollbackHasWishes(owner), TRUE);
        EXPECT_EQ(GetBattlerMoveSlotCount(owner), MAX_MON_MOVES + TX_ARCANUM_COUNT);
        EXPECT_EQ(GetBattlerMoveAtSlot(owner, MAX_MON_MOVES + TX_ARCANUM_SLOT_WISH), MOVE_ARCANUM_WISH);
        EXPECT_EQ(GetBattlerMoveAtSlot(owner, MAX_MON_MOVES + TX_ARCANUM_SLOT_METEO), MOVE_ARCANUM_METEO);
        EXPECT_EQ(GetBattlerMoveAtSlot(owner, MAX_MON_MOVES + TX_ARCANUM_COUNT), MOVE_NONE);
        gChosenMovePos = MAX_MON_MOVES + TX_ARCANUM_SLOT_METEO;
        EXPECT_EQ(IsArcanumAction(owner), TRUE);
        gChosenMovePos = MAX_MON_MOVES + TX_ARCANUM_COUNT;
        EXPECT_EQ(IsArcanumAction(owner), FALSE);
        gChosenMovePos = MAX_MON_MOVES - 1;
        EXPECT_EQ(IsArcanumAction(owner), FALSE);
        FREE_AND_SET_NULL(gBattleStruct);
    }
}

TEST("TX Arcanum cooldown: only a successful Meteo hit arms it, once per action")
{
    MeteoUnitFixture(TX_PROFILE_KOANKO_SOLO);
    gCurrentMove = gChosenMove = MOVE_ARCANUM_METEO;
    gChosenMovePos = MAX_MON_MOVES + TX_ARCANUM_SLOT_METEO;
    gBattleStruct->unableToUseMove = FALSE;

    // Full miss / immunity: no record, no cooldown.
    gBattleStruct->moveResultFlags[0] = MOVE_RESULT_MISSED;
    TxRollbackMeteoRecordTarget(1, 0);
    TxRollbackMeteoFinishAction(1);
    EXPECT_EQ(TxRollbackArcanumLocked(1), FALSE);

    // Partial spread: one miss, one hit. One cooldown, not two.
    gBattleStruct->moveResultFlags[0] = MOVE_RESULT_MISSED;
    gBattleStruct->moveResultFlags[2] = 0;
    TxRollbackMeteoRecordTarget(1, 0);
    TxRollbackMeteoRecordTarget(1, 2);
    TxRollbackMeteoFinishAction(1);
    EXPECT_EQ(TxRollbackArcanumLocked(1), TRUE);
    EXPECT_EQ(gBattleStruct->txRollback.meteoHitLanded, FALSE);

    // Cancelled/unable action never records.
    gBattleStruct->txRollback.arcanumLock = TX_ARCANUM_LOCK_NONE;
    gBattleStruct->unableToUseMove = TRUE;
    gBattleStruct->moveResultFlags[2] = 0;
    TxRollbackMeteoRecordTarget(1, 2);
    TxRollbackMeteoFinishAction(1);
    EXPECT_EQ(TxRollbackArcanumLocked(1), FALSE);

    // A non-Meteo action never records.
    gBattleStruct->unableToUseMove = FALSE;
    gCurrentMove = gChosenMove = MOVE_ARCANUM_WISH;
    gChosenMovePos = MAX_MON_MOVES + TX_ARCANUM_SLOT_WISH;
    TxRollbackMeteoRecordTarget(1, 2);
    TxRollbackMeteoFinishAction(1);
    EXPECT_EQ(TxRollbackArcanumLocked(1), FALSE);
    FREE_AND_SET_NULL(gBattleStruct);
}

TEST("TX Arcanum cooldown: the casting turn keeps it, the next active turn clears it once")
{
    MeteoUnitFixture(TX_PROFILE_KOANKO_SOLO);
    gBattleResults.battleTurnCounter = 3;
    TxRollbackArcanumOnAttackSuccess(1);
    EXPECT_EQ(TxRollbackArcanumLocked(1), TRUE);

    // End of the casting turn: counter has not advanced yet.
    gBattleStruct->eventState.endTurnBattler = 1;
    EXPECT_EQ(TxRollbackArcanumEndTurn(1), FALSE);
    EXPECT_EQ(TxRollbackArcanumLocked(1), TRUE);

    // Owner absent on the next turn: the lock waits for its next active turn.
    gBattleResults.battleTurnCounter = 4;
    gAbsentBattlerFlags = 1u << 1;
    gBattleStruct->eventState.endTurnBattler = 1;
    TxRollbackArcanumEndTurn(1);
    EXPECT_EQ(TxRollbackArcanumLocked(1), TRUE);

    // Present again: exactly this turn's end-of-turn pass consumes it.
    gAbsentBattlerFlags = 0;
    gBattleStruct->eventState.endTurnBattler = 1;
    TxRollbackArcanumEndTurn(1);
    EXPECT_EQ(TxRollbackArcanumLocked(1), FALSE);
    FREE_AND_SET_NULL(gBattleStruct);
}

TEST("TX Arcanum cooldown clears on genuine owner defeat and never affects other battlers")
{
    MeteoUnitFixture(TX_PROFILE_KOANKO_SOLO);
    TxRollbackArcanumOnAttackSuccess(1);
    EXPECT_EQ(TxRollbackArcanumLocked(0), FALSE); // Not the owner.
    gBattleMons[1].hp = 0;
    TxRollbackOnFaint(1);
    EXPECT_EQ(gBattleStruct->txRollback.arcanumLock, TX_ARCANUM_LOCK_NONE);
    FREE_AND_SET_NULL(gBattleStruct);
}

static bool8 sMeteoSetup;
static u32 sMeteoScoredMask; // bit n = Meteo was offered while battleTurnCounter == n

// Mechanism fixture: real Koanko identity/entry routing; the scorer always nominates Meteo when
// the engine offers it. The engine only offers usable slots, so a missing entry on a turn means
// the lockout removed it from selection.
static s32 MeteoAlwaysScore(u32 attacker, u32 target, u32 move, s32 score)
{
    if (sMeteoSetup)
    {
        sMeteoSetup = FALSE;
        gBattleStruct->koankoPhase = KOANKO_CONJURED_TEAM;
    }
    if (!BattleAI_IsScoringArcanum())
        return move == (TxRollbackHasWishes(attacker) ? MOVE_PROTECT : MOVE_CELEBRATE) ? 100 : 0;
    if (move == MOVE_ARCANUM_METEO && TxRollbackHasWishes(attacker))
    {
        if (gBattleResults.battleTurnCounter < 32)
            sMeteoScoredMask |= 1u << gBattleResults.battleTurnCounter;
        return 250;
    }
    return 0;
}

AI_SINGLE_BATTLE_TEST("TX Arcanum cooldown: Meteo is unavailable for exactly the next active turn")
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        // Battle tests re-enter the whole function for each phase: anything outside GIVEN would be
        // re-run (and wipe the recorded mask) before THEN. Reset the recorder inside GIVEN only.
        sMeteoScoredMask = 0;
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC);
        sMeteoSetup = TRUE;
        sDynamicAiFunc = MeteoAlwaysScore;
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(1000); HP(9999); MaxHP(9999); Moves(MOVE_SEISMIC_TOSS, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(1); Speed(1); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_TX_ROLLBACK) { Level(101); Speed(1); Moves(MOVE_WISH, MOVE_DOOM_DESIRE, MOVE_PROTECT); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); EXPECT_SEND_OUT(opponent, 1); }
        TURN { MOVE(player, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_CELEBRATE); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_ARCANUM_METEO, opponent);
    } THEN {
        // Do not hard-code absolute counter values: find the first turn Meteo was offered, then
        // require offered / withheld (locked) / offered again on the following turns.
        EXPECT_NE(sMeteoScoredMask, 0);
        u32 first = 0;
        while (first < 31 && !(sMeteoScoredMask & (1u << first)))
            first++;
        EXPECT_EQ((sMeteoScoredMask >> first) & 7, 5);
    }
}
