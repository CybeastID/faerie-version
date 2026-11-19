#include "global.h"
#include "test/battle.h"
#include "battle_koanko.h"
#include "battle_koanko_smeargle.h"
#include "battle_bug_space.h"
#include "battle_entry_scripts.h"
#include "battle_script_native.h"
#include "battle_reduction.h"
#include "battle_temp_ally.h"
#include "battle_sprite_scale.h"
#include "battle_controllers.h"
#include "battle_setup.h"
#include "data.h"
#include "malloc.h"
#include "item.h"
#include "money.h"
#include "task.h"
#include "trainer_break_bars.h"
#include "constants/battle_script_commands.h"

static s32 ScoreKoankoLeadBoomburst(u32 battler, u16 move);

// Observe the existing production traversal without influencing a decision.
static EWRAM_DATA struct
{
    u32 startingMoney;
    u32 lastMoney;
    u8 payouts;
    u8 transitions;
    u8 phase;
    bool8 sawConjuring;
    bool8 sawAlly;
    bool8 sawOpening;
    bool8 sawCrushOnly;
    bool8 sawAdaptation;
} sAcceptance = {0};

static void ObserveKoankoAcceptance(u8 taskId)
{
    if (gBattleStruct == NULL)
    {
        DestroyTask(taskId);
        return;
    }
    EXPECT_EQ(TRAINER_BATTLE_PARAM.opponentA, TRAINER_KOANKO);
    EXPECT(gBattleTypeFlags & BATTLE_TYPE_TRAINER);
    EXPECT_EQ(CountTotalItemQuantityInBag(ITEM_MAX_REVIVE), 2);
    EXPECT_EQ(CountTotalItemQuantityInBag(ITEM_ETHER), 3);

    u32 money = GetMoney(&gSaveBlock1Ptr->money);
    if (gBattleResults.opponentFaintCounter < 12)
    {
        EXPECT_EQ(gBattleOutcome, 0);
        EXPECT_EQ(money, sAcceptance.startingMoney);
    }
    if (money != sAcceptance.lastMoney)
    {
        EXPECT_GT(money, sAcceptance.lastMoney);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 12);
        sAcceptance.payouts++;
        sAcceptance.lastMoney = money;
    }
    u32 phase = gBattleStruct->koankoPhase;
    if (phase == KOANKO_CONJURING) sAcceptance.sawConjuring = TRUE;
    if (phase == KOANKO_CONJURED_TEAM && sAcceptance.phase != phase) sAcceptance.transitions++;
    sAcceptance.phase = phase;
    if (TempAllyGetState() == TEMP_ALLY_ACTIVE) sAcceptance.sawAlly = TRUE;
    if (gBattleStruct->bugSpace.active)
    {
        if (gBattleStruct->bugSpace.mode == BUGSPACE_MODE_KOANKO_OPENING) sAcceptance.sawOpening = TRUE;
        if (gBattleStruct->bugSpace.mode == BUGSPACE_MODE_KOANKO_CRUSH_ONLY) sAcceptance.sawCrushOnly = TRUE;
    }
    if (gBattleStruct->koankoArceus.phase == KOANKO_ARCEUS_ADAPTED) sAcceptance.sawAdaptation = TRUE;
}

static void BS_StartKoankoAcceptance(void)
{
    NATIVE_ARGS();
    memset(&sAcceptance, 0, sizeof(sAcceptance));
    SetMoney(&gSaveBlock1Ptr->money, 10000);
    sAcceptance.startingMoney = sAcceptance.lastMoney = GetMoney(&gSaveBlock1Ptr->money);
    sAcceptance.phase = gBattleStruct->koankoPhase;
    u32 taskId = CreateTask(ObserveKoankoAcceptance, 254);
    EXPECT_LT(taskId, NUM_TASKS);
    EXPECT(gTasks[taskId].func == ObserveKoankoAcceptance);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

static const struct __attribute__((packed))
{
    u8 native;
    void (*func)(void);
    u8 ret;
} sKoankoAcceptanceEntry = {B_SCR_OP_CALLNATIVE, BS_StartKoankoAcceptance, B_SCR_OP_RETURN};

// Read the live authored first team. Keep the lifecycle shortcut above/below
// separate from production-profile acceptance; do not force sequence AI here.
static void KoankoProductionOriginal(u32 index)
{
    const struct TrainerMon *mon = &GetTrainerPartyFromId(TRAINER_KOANKO)[index];
    OPPONENT(mon->species) {
        struct Pokemon *pokemon = gBattleTestRunnerState->data.currentMon;
        CreateMonWithIVs(pokemon, mon->species, mon->lvl, mon->nature, OTID_STRUCT_PRESET(0), 31);
        static const u8 fields[] = {MON_DATA_HP_EV, MON_DATA_ATK_EV, MON_DATA_DEF_EV,
            MON_DATA_SPATK_EV, MON_DATA_SPDEF_EV, MON_DATA_SPEED_EV};
        for (u32 i = 0; i < ARRAY_COUNT(fields); i++)
            SetMonData(pokemon, fields[i], &mon->ev[i]);
        CalculateMonStats(pokemon);
        Nature(mon->nature); Ability(mon->ability); Item(mon->heldItem);
        Moves(mon->moves[0], mon->moves[1], mon->moves[2], mon->moves[3]);
        if (mon->shouldUseDynamax)
        {
            DynamaxLevel(mon->dynamaxLevel);
            GigantamaxFactor(mon->gigantamaxFactor);
        }
        Speed(GetMonData(pokemon, MON_DATA_SPEED));
    }
}

AI_SINGLE_BATTLE_TEST("Koanko acceptance production twelve profiles complete one uninterrupted trainer battle")
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        AI_FLAGS(GetTrainerAIFlagsFromId(TRAINER_KOANKO));
        TurnZeroScript(B_TRAINER_OPPONENT_A, (const u8 *)&sKoankoAcceptanceEntry);
        GIVE_PLAYER_ITEM(ITEM_MAX_REVIVE, 2);
        GIVE_PLAYER_ITEM(ITEM_ETHER, 3);
        // Controlled player strength isolates transitions while opponents keep
        // real profiles, production AI and free first-team replacement choices.
        PLAYER(SPECIES_HERACROSS) {
            Level(MAX_LEVEL); Attack(9999); Speed(9999); Defense(9999); SpDefense(9999);
            HP(9999); MaxHP(9999); Ability(ABILITY_SKILL_LINK); Moves(MOVE_ICICLE_SPEAR, MOVE_FEINT);
        }
        for (u32 i = 0; i < PARTY_SIZE; i++)
            KoankoProductionOriginal(i);
    } WHEN {
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); }
        for (u32 i = 1; i < PARTY_SIZE; i++)
            TURN { MOVE(player, MOVE_ICICLE_SPEAR); }
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); }
        TURN { MOVE(player, MOVE_FEINT); }
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); }
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); }
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); }
        SUMMONED_ALLY();
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); }
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); }
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); }
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); }
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); }
    } SCENE {
        MESSAGE("You thought that was all?\nThen watch what I can create!{PAUSE_UNTIL_PRESS}");
        MESSAGE("The opposing Smeargle fainted!");
        MESSAGE("The opposing TX Rollback fainted!");
        MESSAGE("The opposing Calyrex fainted!");
        MESSAGE("The opposing Eternatus fainted!");
        MESSAGE("The opposing Dialga fainted!");
        MESSAGE("Behold, your final trial!");
        MESSAGE("The opposing Arceus fainted!");
    } THEN {
        u32 observer = FindTaskIdByFunc(ObserveKoankoAcceptance);
        EXPECT_LT(observer, NUM_TASKS);
        ObserveKoankoAcceptance(observer); // Include the final bookkeeping frame.
        EXPECT_EQ(sAcceptance.transitions, 1);
        EXPECT(sAcceptance.sawConjuring);
        EXPECT(sAcceptance.sawAlly);
        EXPECT(sAcceptance.sawOpening);
        EXPECT(sAcceptance.sawCrushOnly);
        EXPECT(sAcceptance.sawAdaptation);
        EXPECT_EQ(sAcceptance.payouts, 1);
        EXPECT_GT(GetMoney(&gSaveBlock1Ptr->money), sAcceptance.startingMoney);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 12);
        EXPECT_EQ(gBattleStruct->koankoPhase, KOANKO_CONJURED_TEAM);
        EXPECT_EQ(TRAINER_BATTLE_PARAM.opponentA, TRAINER_KOANKO);
        EXPECT_EQ(gBattleOutcome, B_OUTCOME_WON);
        EXPECT_EQ(gBattlersCount, 2);
        EXPECT_EQ(TempAllyGetState(), TEMP_ALLY_NONE);
        EXPECT_EQ(gBattleStruct->bugSpace.active, FALSE);
        EXPECT_EQ(KoankoArceusGetForm(1), SPECIES_NONE);
        EXPECT_EQ(GetMonData(GetBattlerMon(1), MON_DATA_SPECIES), SPECIES_ARCEUS_NORMAL);
    } AFTER_CLEANUP {
        u32 observer = FindTaskIdByFunc(ObserveKoankoAcceptance);
        if (observer != TASK_NONE) DestroyTask(observer);
        EXPECT_EQ(ReductionIsActive(), FALSE);
        EXPECT_EQ(gBattleStruct->txRollback.combatProfile, TX_PROFILE_NONE);
        EXPECT_EQ(TempAllyGetState(), TEMP_ALLY_NONE);
        EXPECT_EQ(gPartiesCount[B_TRAINER_PARTNER], 0);
        EXPECT_EQ(gBattleStruct->bugSpace.active, FALSE);
        EXPECT_EQ(sAcceptance.payouts, 1);
        EXPECT_EQ(CountTotalItemQuantityInBag(ITEM_MAX_REVIVE), 2);
        EXPECT_EQ(CountTotalItemQuantityInBag(ITEM_ETHER), 3);
    }
}

SINGLE_BATTLE_TEST("Koanko acceptance fresh ordinary singles keeps normal rendering and no encounter ownership")
{
    GIVEN {
        FORCE_MOVE_ANIM(TRUE);
        PLAYER(SPECIES_PIKACHU) { Speed(20); Moves(MOVE_TACKLE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(10); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_TACKLE); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_TACKLE, player);
        HP_BAR(opponent);
    } THEN {
        EXPECT_EQ(gBattlersCount, 2);
        EXPECT_EQ(TempAllyGetState(), TEMP_ALLY_NONE);
        EXPECT_EQ(gBattleStruct->bugSpace.active, FALSE);
        EXPECT_EQ(gBattleStruct->koankoArceus.phase, KOANKO_ARCEUS_DORMANT);
        EXPECT_EQ(BattleSpriteScaleCurrent(0), BATTLE_SPRITE_SCALE_ONE);
        EXPECT_EQ(BattleSpriteScaleCurrent(1), BATTLE_SPRITE_SCALE_ONE);
    } AFTER_CLEANUP { FORCE_MOVE_ANIM(FALSE); }
}

DOUBLE_BATTLE_TEST("Koanko acceptance fresh ordinary doubles shares the player party and renders all four mons normally")
{
    GIVEN {
        PLAYER(SPECIES_PIKACHU) { Speed(40); Moves(MOVE_CELEBRATE); }
        PLAYER(SPECIES_MARILL) { Speed(30); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(20); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WYNAUT) { Speed(10); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_CELEBRATE); MOVE(playerRight, MOVE_CELEBRATE); MOVE(opponentLeft, MOVE_CELEBRATE); MOVE(opponentRight, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(gBattlersCount, 4);
        EXPECT_EQ(TempAllyGetState(), TEMP_ALLY_NONE);
        EXPECT_EQ(GetBattlerTrainer(0), B_TRAINER_PLAYER);
        EXPECT_EQ(GetBattlerTrainer(2), B_TRAINER_PLAYER);
        EXPECT_EQ(gBattleStruct->bugSpace.active, FALSE);
        EXPECT_EQ(gBattleStruct->koankoArceus.phase, KOANKO_ARCEUS_DORMANT);
        for (u32 battler = 0; battler < MAX_BATTLERS_COUNT; battler++)
            EXPECT_EQ(BattleSpriteScaleCurrent(battler), BATTLE_SPRITE_SCALE_ONE);
    }
}

TEST("Koanko replacement creates fresh opponents while preserving player, field, inventory and trainer identity")
{
    gBattleStruct = AllocZeroed(sizeof(*gBattleStruct));
    gAiPartyData = AllocZeroed(sizeof(*gAiPartyData));
    gAiLogicData = AllocZeroed(sizeof(*gAiLogicData));
    gBattleHistory = AllocZeroed(sizeof(*gBattleHistory));
    gBattlersCount = 2;
    gBattleStruct->koankoPhase = KOANKO_CONJURING;
    gBattleStruct->givenExpMons[0] = 0x3F;
    gBattleStruct->givenExpMons[1] = 0x15; // Opponent B is independent of this encounter.
    gBattleStruct->gimmick.activated[B_POSITION_OPPONENT_LEFT][GIMMICK_DYNAMAX] = TRUE;
    gBattleStruct->gimmick.activated[B_POSITION_OPPONENT_LEFT][GIMMICK_TERA] = TRUE;
    gBattleStruct->gimmick.activated[B_POSITION_PLAYER_LEFT][GIMMICK_TERA] = TRUE;
    gBattleStruct->partyState[B_SIDE_PLAYER][0].breakBarConfig = &gTrainerBreakBarProfiles[0];
    gBattleStruct->partyState[B_SIDE_PLAYER][0].breakBarsRemaining = 1;
    gAiPartyData->mons[B_SIDE_PLAYER][0].species = SPECIES_PICHU;
    gBattleHistory->trainerItems[0] = ITEM_POTION;
    gBattleHistory->itemsNo = 1;
    gBattleWeather = B_WEATHER_RAIN;
    gSideStatuses[B_SIDE_OPPONENT] = SIDE_STATUS_REFLECT;
    gSideTimers[B_SIDE_OPPONENT].spikesAmount = 2;
    gBattleStruct->futureSight[B_POSITION_OPPONENT_LEFT].counter = 2;
    gBattleResults.opponentFaintCounter = 6;
    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        gBattleStruct->partyState[B_TRAINER_OPPONENT_A][i].isKnockedOff = TRUE;
        gBattleStruct->partyState[B_SIDE_OPPONENT][i].breakBarConfig = &gTrainerBreakBarProfiles[4];
        gBattleStruct->partyState[B_SIDE_OPPONENT][i].breakBarSavedHp[0] = 42;
        gBattleStruct->partyState[B_SIDE_OPPONENT][i].breakBarSavedMaxHp[0] = 100;
        gBattleStruct->partyState[B_SIDE_OPPONENT][i].breakBarsRemaining = 5;
        gBattleStruct->partyState[B_SIDE_OPPONENT][i].breakBarItemUsed = TRUE;
        gAiPartyData->mons[B_SIDE_OPPONENT][i].species = SPECIES_WOBBUFFET;
        gAiPartyData->mons[B_SIDE_OPPONENT][i].isFainted = TRUE;
    }
    ConjureKoankoTeam();
    static const u16 species[] = {SPECIES_SMEARGLE, SPECIES_TX_ROLLBACK, SPECIES_CALYREX_ICE, SPECIES_ETERNATUS, SPECIES_DIALGA_ORIGIN, SPECIES_ARCEUS};
    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_OPPONENT_A][i], MON_DATA_SPECIES), species[i]);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_OPPONENT_A][i], MON_DATA_LEVEL), 100 + i);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_OPPONENT_A][i], MON_DATA_HP), GetMonData(&gParties[B_TRAINER_OPPONENT_A][i], MON_DATA_MAX_HP));
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_OPPONENT_A][i], MON_DATA_TERA_TYPE), i == 2 ? TYPE_FIRE : TYPE_MYSTERY);
        EXPECT_EQ(GetMonData(&gParties[B_TRAINER_OPPONENT_A][i], MON_DATA_DYNAMAX_LEVEL), BLOCK_AI_DYNAMAX);
        EXPECT_EQ((u32)gBattleStruct->partyState[B_TRAINER_OPPONENT_A][i].isKnockedOff, FALSE);
        // Regeneration removes the old five-layer profile, then installs the
        // current production Calyrex/Eternatus phase bars. Those are not stale.
        const struct BreakBarConfig *config = ResolveTrainerBreakBar(&gKoankoConjuredBreakBars, species[i], i);
        EXPECT_EQ(gBattleStruct->partyState[B_SIDE_OPPONENT][i].breakBarConfig, config);
        EXPECT_EQ((u32)gBattleStruct->partyState[B_SIDE_OPPONENT][i].breakBarsRemaining,
                  config == NULL ? 0 : GetInitialBreakBarCount(config));
        EXPECT_EQ(gBattleStruct->partyState[B_SIDE_OPPONENT][i].breakBarSavedMaxHp[0], 0);
        EXPECT_EQ((u32)gBattleStruct->partyState[B_SIDE_OPPONENT][i].breakBarItemUsed, FALSE);
        EXPECT_EQ(gBattleStruct->partyState[B_TRAINER_OPPONENT_A][i].entryScript,
                  i == PARTY_SIZE - 1 ? &gKoankoArceusEntry : NULL);
        EXPECT_EQ((u32)gAiPartyData->mons[B_SIDE_OPPONENT][i].isFainted, FALSE);
    }
    EXPECT_EQ(gBattleStruct->koankoPhase, KOANKO_CONJURED_TEAM);
    EXPECT_EQ(gBattleStruct->givenExpMons[0], 0);
    EXPECT(gBattleStruct->battlerState[B_POSITION_OPPONENT_LEFT].notOnField);
    EXPECT_EQ(gBattleStruct->givenExpMons[1], 0x15);
    EXPECT_EQ(gBattleStruct->gimmick.activated[B_POSITION_OPPONENT_LEFT][GIMMICK_DYNAMAX], TRUE);
    // Only the opponent's Tera-used flag is renewed; the player's Tera usage is untouched.
    EXPECT_EQ(gBattleStruct->gimmick.activated[B_POSITION_OPPONENT_LEFT][GIMMICK_TERA], FALSE);
    EXPECT_EQ(gBattleStruct->gimmick.activated[B_POSITION_PLAYER_LEFT][GIMMICK_TERA], TRUE);
    EXPECT_EQ((u32)gBattleStruct->partyState[B_SIDE_PLAYER][0].breakBarsRemaining, 1);
    EXPECT_EQ(gAiPartyData->mons[B_SIDE_PLAYER][0].species, SPECIES_PICHU);
    EXPECT_EQ(gBattleHistory->trainerItems[0], ITEM_POTION);
    EXPECT_EQ(gBattleHistory->itemsNo, 1);
    EXPECT_EQ((u32)gSideTimers[B_SIDE_OPPONENT].spikesAmount, 2);
    EXPECT_EQ(gBattleWeather, B_WEATHER_RAIN);
    EXPECT_EQ(gSideStatuses[B_SIDE_OPPONENT], SIDE_STATUS_REFLECT);
    EXPECT_EQ((u32)gBattleStruct->futureSight[B_POSITION_OPPONENT_LEFT].counter, 2);
    EXPECT_EQ(gBattleResults.opponentFaintCounter, 6);
    EXPECT_EQ(gPartiesCount[B_TRAINER_OPPONENT_A], 6);
    // Stage 08 uses the existing move and production Dialga kit unchanged.
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_OPPONENT_A][4], MON_DATA_MOVE1), MOVE_ROAR_OF_ETERNITY);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_OPPONENT_A][4], MON_DATA_MOVE2), MOVE_FLASH_CANNON);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_OPPONENT_A][4], MON_DATA_MOVE3), MOVE_EARTH_POWER);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_OPPONENT_A][4], MON_DATA_MOVE4), MOVE_THUNDERBOLT);
    // A second call cannot regenerate a defeated conjured Pokemon.
    u16 hp = 0;
    SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_HP, &hp);
    ConjureKoankoTeam();
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_HP), 0);
    // ...and cannot renew the Tera opportunity a second time once it has been consumed.
    gBattleStruct->gimmick.activated[B_POSITION_OPPONENT_LEFT][GIMMICK_TERA] = TRUE;
    ConjureKoankoTeam();
    EXPECT_EQ(gBattleStruct->gimmick.activated[B_POSITION_OPPONENT_LEFT][GIMMICK_TERA], TRUE);
    Free(gBattleHistory);
    Free(gAiLogicData);
    Free(gAiPartyData);
    Free(gBattleStruct);
    gBattleHistory = NULL;
    gAiLogicData = NULL;
    gAiPartyData = NULL;
    gBattleStruct = NULL;
}

AI_SINGLE_BATTLE_TEST("Koanko conjures a second team and victory requires twelve genuine faints")
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        AI_FLAGS(AI_FLAG_SEQUENCE_SWITCHING);
        Test_SetKoankoSmeargleScorer(ScoreKoankoLeadBoomburst);
        // Icicle Spear breaks Focus Sash and can hit Shadow Rider. Feint makes
        // TX's Protect choice irrelevant to this lifecycle fixture. Extra HP
        // keeps the boss's action after a form-bar break from ending the run.
        PLAYER(SPECIES_HERACROSS) {
            Level(MAX_LEVEL); Attack(9999); Speed(9999); Defense(9999); SpDefense(9999);
            HP(9999); MaxHP(9999); Ability(ABILITY_SKILL_LINK); Moves(MOVE_ICICLE_SPEAR, MOVE_FEINT);
        }
        for (u32 i = 0; i < PARTY_SIZE; i++)
            OPPONENT(SPECIES_WOBBUFFET) { HP(1); Speed(50); Moves(MOVE_CELEBRATE); }
    } WHEN {
        for (u32 i = 1; i < PARTY_SIZE; i++)
            TURN { MOVE(player, MOVE_ICICLE_SPEAR); EXPECT_SEND_OUT(opponent, i); }
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); EXPECT_SEND_OUT(opponent, 0); }
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); EXPECT_SEND_OUT(opponent, 1); } // Smeargle, including Sash.
        TURN { MOVE(player, MOVE_FEINT); EXPECT_SEND_OUT(opponent, 2); } // Solo TX.
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); } // Ice Rider's bar -> Shadow Rider, no faint/send-out.
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); EXPECT_SEND_OUT(opponent, 3); }
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); } // Eternatus's bar -> Eternamax and ally entry.
        SUMMONED_ALLY();
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); EXPECT_SEND_OUT(opponent, 4); } // Genuine faint, ally leaves.
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); EXPECT_SEND_OUT(opponent, 5); }
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); } // Arceus's protected full turn, then overcharge.
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); } // Saved layers resolve after all hits, protecting the restored base.
        TURN { MOVE(player, MOVE_ICICLE_SPEAR); } // Genuine Arceus faint after its single adaptation.
    } SCENE {
        for (u32 i = 0; i < PARTY_SIZE; i++)
            MESSAGE("The opposing Wobbuffet fainted!");
        MESSAGE("You thought that was all?\nThen watch what I can create!{PAUSE_UNTIL_PRESS}");
        MESSAGE("The opposing Smeargle fainted!");
        MESSAGE("The opposing TX Rollback fainted!");
        MESSAGE("The opposing Calyrex fainted!");
        MESSAGE("The opposing Eternatus fainted!");
        MESSAGE("The opposing Dialga fainted!");
        MESSAGE("The opposing Arceus fainted!");
        NOT MESSAGE("You thought that was all?\nThen watch what I can create!{PAUSE_UNTIL_PRESS}");
    } THEN {
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 12);
        EXPECT_EQ(gBattleStruct->koankoPhase, KOANKO_CONJURED_TEAM);
        EXPECT_EQ(TRAINER_BATTLE_PARAM.opponentA, TRAINER_KOANKO);
        EXPECT_EQ(gBattleOutcome, B_OUTCOME_WON);
        EXPECT_EQ(gBattlersCount, 2);
        EXPECT_EQ(TempAllyGetState(), TEMP_ALLY_NONE);
    } AFTER_CLEANUP {
        Test_SetKoankoSmeargleScorer(NULL);
    }
}

SINGLE_BATTLE_TEST("Koanko conjures after residual damage defeats her final Pokemon")
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(1); Status1(STATUS1_POISON); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); SEND_OUT(opponent, 0); }
    } SCENE {
        MESSAGE("The opposing Wobbuffet fainted!");
        MESSAGE("You thought that was all?\nThen watch what I can create!{PAUSE_UNTIL_PRESS}");
    } THEN {
        EXPECT_EQ(opponent->species, SPECIES_SMEARGLE);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 1);
    }
}

SINGLE_BATTLE_TEST("Koanko conjures after recoil defeats her final Pokemon")
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { HP(500); MaxHP(500); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(1); Moves(MOVE_DOUBLE_EDGE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_DOUBLE_EDGE); SEND_OUT(opponent, 0); }
    } SCENE {
        MESSAGE("The opposing Wobbuffet fainted!");
        MESSAGE("You thought that was all?\nThen watch what I can create!{PAUSE_UNTIL_PRESS}");
    } THEN {
        EXPECT_EQ(opponent->species, SPECIES_SMEARGLE);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 1);
    }
}

SINGLE_BATTLE_TEST("Koanko conjured lead takes existing hazards without inheriting the old Pokemon's status")
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_SPIKES); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(1); Status1(STATUS1_POISON); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SPIKES); MOVE(opponent, MOVE_CELEBRATE); SEND_OUT(opponent, 0); }
    } SCENE {
        MESSAGE("You thought that was all?\nThen watch what I can create!{PAUSE_UNTIL_PRESS}");
        HP_BAR(opponent);
        MESSAGE("The opposing Smeargle was hurt by the spikes!");
    } THEN {
        EXPECT_EQ(opponent->hp, opponent->maxHP - opponent->maxHP / 8);
        EXPECT_EQ(opponent->status1, 0);
    }
}

SINGLE_BATTLE_TEST("Koanko conjures after simultaneous faints when the player has a reserve")
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        PLAYER(SPECIES_VOLTORB) { Attack(9999); Moves(MOVE_EXPLOSION); }
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_EXPLOSION); SEND_OUT(player, 1); SEND_OUT(opponent, 0); }
    } SCENE {
        MESSAGE("You thought that was all?\nThen watch what I can create!{PAUSE_UNTIL_PRESS}");
    } THEN {
        EXPECT_EQ(opponent->species, SPECIES_SMEARGLE);
        EXPECT_EQ(player->species, SPECIES_WOBBUFFET);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 1);
    }
}

SINGLE_BATTLE_TEST("Koanko does not conjure when simultaneous faints exhaust the player")
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        PLAYER(SPECIES_VOLTORB) { Attack(9999); Moves(MOVE_EXPLOSION); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_EXPLOSION); }
    } SCENE {
        NOT MESSAGE("You thought that was all?\nThen watch what I can create!{PAUSE_UNTIL_PRESS}");
    } THEN {
        EXPECT_EQ(gBattleStruct->koankoPhase, KOANKO_ORDINARY_TEAM);
    }
}

SINGLE_BATTLE_TEST("Koanko conjures after Destiny Bond with a surviving player reserve")
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { HP(1); Speed(100); Moves(MOVE_DESTINY_BOND); }
        PLAYER(SPECIES_WOBBUFFET) { Speed(100); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(50); Moves(MOVE_SCRATCH); }
    } WHEN {
        TURN { MOVE(player, MOVE_DESTINY_BOND); MOVE(opponent, MOVE_SCRATCH); SEND_OUT(player, 1); SEND_OUT(opponent, 0); }
    } SCENE {
        MESSAGE("Wobbuffet took its attacker down with it!");
        MESSAGE("You thought that was all?\nThen watch what I can create!{PAUSE_UNTIL_PRESS}");
    } THEN {
        EXPECT_EQ(gBattleStruct->koankoPhase, KOANKO_CONJURED_TEAM);
        EXPECT_EQ(opponent->species, SPECIES_SMEARGLE);
    }
}

static s32 ScoreKoankoLeadBoomburst(u32 battler, u16 move)
{
    return move == MOVE_BOOMBURST ? 100 : 0;
}

AI_SINGLE_BATTLE_TEST("Koanko AI sends the conjured lead and selects from its new moves")
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        Test_SetKoankoSmeargleScorer(ScoreKoankoLeadBoomburst);
        PLAYER(SPECIES_WOBBUFFET) { SpDefense(9999); Speed(1000); Moves(MOVE_SEISMIC_TOSS, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(1); Speed(50); Moves(MOVE_SEISMIC_TOSS, MOVE_NIGHT_SHADE, MOVE_SPORE, MOVE_ENDEAVOR); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); EXPECT_SEND_OUT(opponent, 0); }
        // The replacement changes slot 0 at runtime. A named move expectation
        // would try to add Tackle to the original, already full fixture moveset.
        TURN { MOVE(player, MOVE_CELEBRATE); EXPECT_MOVE(opponent, moveSlot: 0); }
    } SCENE {
        MESSAGE("You thought that was all?\nThen watch what I can create!{PAUSE_UNTIL_PRESS}");
        MESSAGE("The opposing Smeargle used Boomburst!");
    } THEN {
        EXPECT_EQ(opponent->species, SPECIES_SMEARGLE);
        EXPECT_EQ(gAiPartyData->count[B_SIDE_OPPONENT], PARTY_SIZE);
        // The living conjured occupant cannot be moved out of sequence, and the AI now follows party order.
        EXPECT(HasNoOrderChange(B_POSITION_OPPONENT_LEFT));
        EXPECT(gAiThinkingStruct->aiFlags[B_POSITION_OPPONENT_LEFT] & AI_FLAG_SEQUENCE_SWITCHING);
        EXPECT_EQ(opponent->moves[0], MOVE_BOOMBURST);
        EXPECT_EQ(opponent->moves[1], MOVE_NONE);
        EXPECT_EQ(opponent->moves[2], MOVE_NONE);
        EXPECT_EQ(opponent->moves[3], MOVE_NONE);
        Test_SetKoankoSmeargleScorer(NULL);
    }
}

// NOT RUN when authored. Shaped on the AI test above (same transition setup).
AI_SINGLE_BATTLE_TEST("Koanko living conjured Pokemon cannot be phazed out of sequence")
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Speed(1000); Moves(MOVE_SEISMIC_TOSS, MOVE_ROAR); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(1); Speed(50); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); EXPECT_SEND_OUT(opponent, 0); }
        TURN { MOVE(player, MOVE_ROAR); }
    } THEN {
        // Roar would normally drag in another of the five living conjured Pokemon.
        EXPECT_EQ(opponent->species, SPECIES_SMEARGLE);
        EXPECT_EQ(gBattlerPartyIndexes[B_POSITION_OPPONENT_LEFT], 0);
    }
}


SINGLE_BATTLE_TEST("Koanko Loaded Dice allows 102 and 101 percent damage and retains 85 percent minimum")
{
    s16 damage;
    u32 roll;
    s16 expectedDamage;

    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;

    PARAMETRIZE { roll = 0;  expectedDamage = 173; } // 102%
    PARAMETRIZE { roll = 1;  expectedDamage = 171; } // 101%
    PARAMETRIZE { roll = 17; expectedDamage = 144; } // 85%

    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) {
            Level(100);
            Defense(100);
            HP(1000);
            MaxHP(1000);
            Moves(MOVE_CELEBRATE);
        }
        OPPONENT(SPECIES_WOBBUFFET) {
            Level(100);
            Attack(500);
            Moves(MOVE_SCRATCH);
        }
    } WHEN {
        TURN {
            MOVE(player, MOVE_CELEBRATE);
            MOVE(opponent, MOVE_SCRATCH,
                 WITH_RNG(RNG_DAMAGE_MODIFIER, roll),
                 criticalHit: FALSE);
        }
    } SCENE {
        HP_BAR(player, captureDamage: &damage);
    } THEN {
        EXPECT_EQ(expectedDamage, damage);
    }
}

SINGLE_BATTLE_TEST("Koanko Loaded Dice does not affect ordinary trainers")
{
    s16 damage;
    u32 roll;
    s16 expectedDamage;

    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_ROXANNE_1;

    PARAMETRIZE { roll = 0;  expectedDamage = 170; } // 100%
    PARAMETRIZE { roll = 15; expectedDamage = 144; } // 85%

    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) {
            Level(100);
            Defense(100);
            HP(1000);
            MaxHP(1000);
            Moves(MOVE_CELEBRATE);
        }
        OPPONENT(SPECIES_WOBBUFFET) {
            Level(100);
            Attack(500);
            Moves(MOVE_SCRATCH);
        }
    } WHEN {
        TURN {
            MOVE(player, MOVE_CELEBRATE);
            MOVE(opponent, MOVE_SCRATCH,
                 WITH_RNG(RNG_DAMAGE_MODIFIER, roll),
                 criticalHit: FALSE);
        }
    } SCENE {
        HP_BAR(player, captureDamage: &damage);
    } THEN {
        EXPECT_EQ(expectedDamage, damage);
    }
}

SINGLE_BATTLE_TEST("Koanko Loaded Dice does not improve the player's damage")
{
    s16 damage;
    u32 roll;
    s16 expectedDamage;

    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;

    PARAMETRIZE { roll = 0;  expectedDamage = 170; } // 100%
    PARAMETRIZE { roll = 15; expectedDamage = 144; } // 85%

    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) {
            Level(100);
            Attack(500);
            Moves(MOVE_SCRATCH);
        }
        OPPONENT(SPECIES_WOBBUFFET) {
            Level(100);
            Defense(100);
            HP(1000);
            MaxHP(1000);
            Moves(MOVE_CELEBRATE);
        }
    } WHEN {
        TURN {
            MOVE(player, MOVE_SCRATCH,
                 WITH_RNG(RNG_DAMAGE_MODIFIER, roll),
                 criticalHit: FALSE);
            MOVE(opponent, MOVE_CELEBRATE);
        }
    } SCENE {
        HP_BAR(opponent, captureDamage: &damage);
    } THEN {
        EXPECT_EQ(expectedDamage, damage);
    }
}
