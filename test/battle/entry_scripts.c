#include "global.h"
#include "test/battle.h"
#include "battle_entry_scripts.h"
#include "battle_controllers.h"
#include "battle_script_native.h"
#include "battle_scripts.h"
#include "battle_break_bar.h"
#include "constants/songs.h"
#include "script.h"
#include "main.h"
#include "reshow_battle_screen.h"
#include "data.h"
#include "event_data.h"
#include "malloc.h"
#include "trainer_break_bars.h"

struct EntryObservation
{
    u8 trainer;
    u8 slot;
    u8 turn;
    u8 runs;
    u8 attackStage;
    u8 bars;
    u8 pp;
    bool8 opening;
};

// Observation buffers must not shrink the normal engine's IWRAM stack headroom.
static EWRAM_DATA struct EntryObservation sObserved[32] = {0};
static EWRAM_DATA u32 sCount = 0;
static EWRAM_DATA u32 sSynchronized = 0;
static EWRAM_DATA bool8 sReshow = FALSE;
static EWRAM_DATA bool8 sBreakBar = FALSE;
static EWRAM_DATA BattleControllerFunc sReshowController = NULL;

static void WaitForReshow(enum BattlerId battler)
{
    if (gMain.callback2 == BattleMainCB2)
    {
        gBattlerControllerFuncs[battler] = sReshowController;
        gBattleControllerExecFlags &= ~(1u << battler);
    }
}

static void Record(bool32 opening)
{
    u32 battler = gBattleScripting.battler;
    if (sCount < ARRAY_COUNT(sObserved))
    {
        struct EntryObservation *event = &sObserved[sCount++];
        event->trainer = GetBattlerTrainer(battler);
        event->slot = gBattlerPartyIndexes[battler];
        event->turn = gBattleResults.battleTurnCounter;
        event->runs = GetBattlerPartyState(battler)->entryScriptRuns;
        event->attackStage = gBattleMons[GetBattlerAtPosition(B_POSITION_PLAYER_LEFT)].statStages[STAT_ATK];
        event->bars = GetBreakBarCount(battler);
        event->pp = gBattleMons[battler].pp[0];
        event->opening = opening;
    }
    // Force a controller yield; the next callnative must wait for completion.
    BtlController_EmitPlaySE(battler, B_COMM_TO_CONTROLLER, SE_SELECT);
    MarkBattlerForControllerExec(battler);
}

static void BS_RecordEntry(void)
{
    NATIVE_ARGS();
    Record(FALSE);
    if (sBreakBar)
    {
        sBreakBar = FALSE;
        gBattleMons[gBattlerAttacker].hp = 0;
        QueueBreakBar(gBattlerAttacker);
    }
    gBattlescriptCurrInstr = cmd->nextInstr;
}

static void BS_RecordOpening(void)
{
    NATIVE_ARGS();
    Record(TRUE);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

static void BS_RecordSynchronized(void)
{
    NATIVE_ARGS();
    if (gBattleControllerExecFlags == 0)
        sSynchronized++;
    // Deliberately overwrite transient context; ordinary actions must survive.
    gBattlerAttacker = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
    gBattlerTarget = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
    gCurrentMove = gChosenMove = MOVE_SPLASH;
    gBattleCommunication[MULTISTRING_CHOOSER] = 7;
    // Weather/terrain returning helpers may sort the shared speed arrays.
    for (u32 i = 0; i < gBattlersCount / 2; i++)
    {
        enum BattlerId saved = gBattlersBySpeed[i];
        gBattlersBySpeed[i] = gBattlersBySpeed[gBattlersCount - 1 - i];
        gBattlersBySpeed[gBattlersCount - 1 - i] = saved;
    }
    gBattlescriptCurrInstr = cmd->nextInstr;
    if (sReshow)
    {
        sReshow = FALSE;
        sReshowController = gBattlerControllerFuncs[0];
        gBattlerControllerFuncs[0] = WaitForReshow;
        MarkBattlerForControllerExec(0);
        CloseMainBattleScreen();
        ReshowBattleScreenAfterMenu();
    }
}

static const struct __attribute__((packed))
{
    u8 native;
    void (*func)(void);
    u8 pause;
    u16 frames;
    u8 check;
    void (*checkFunc)(void);
    u8 ret;
} sEntryPayload = {B_SCR_OP_CALLNATIVE, BS_RecordEntry, B_SCR_OP_PAUSE, 3,
                  B_SCR_OP_CALLNATIVE, BS_RecordSynchronized, B_SCR_OP_RETURN},
  sOpeningPayload = {B_SCR_OP_CALLNATIVE, BS_RecordOpening, B_SCR_OP_PAUSE, 3,
                    B_SCR_OP_CALLNATIVE, BS_RecordSynchronized, B_SCR_OP_RETURN};

// Exercise a real nested battle-script call as well as the scheduler wrapper.
static const struct __attribute__((packed))
{
    u8 call;
    const void *script;
    u8 ret;
} sNestedEntry = {B_SCR_OP_CALL, &sEntryPayload, B_SCR_OP_RETURN};

static const struct MonEntryScript sOnce = {(const u8 *)&sNestedEntry, ENTRY_SCRIPT_ONCE_PER_BATTLE};
static const struct MonEntryScript sEvery = {(const u8 *)&sNestedEntry, ENTRY_SCRIPT_EVERY_ENTRY};
static const struct BreakBarConfig sBars = {.extraBars = 1};

static void BS_FaintEntrySubject(void)
{
    NATIVE_ARGS();
    Record(FALSE);
    u32 battler = gBattleScripting.battler;
    gBattleMons[battler].hp = 0;
    SetMonData(GetBattlerMon(battler), MON_DATA_HP, &gBattleMons[battler].hp);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

static const struct __attribute__((packed))
{
    u8 native;
    void (*func)(void);
    u8 faint;
    u8 battler;
    bool8 isSpikes;
    const u8 *jumpInstr;
    u8 ret;
} sFaintPayload = {B_SCR_OP_CALLNATIVE, BS_FaintEntrySubject, B_SCR_OP_TRYFAINTMON,
                   BS_SCRIPTING, FALSE, NULL, B_SCR_OP_RETURN};
static const struct MonEntryScript sFaintOnce = {(const u8 *)&sFaintPayload, ENTRY_SCRIPT_ONCE_PER_BATTLE};

static void Fixture(void)
{
    sCount = 0;
    sSynchronized = 0;
    sReshow = FALSE;
    sBreakBar = FALSE;
    memset(sObserved, 0, sizeof(sObserved));
}

SINGLE_BATTLE_TEST("Entry scripts: initial entries precede Turn 0 exactly once without spending an action or PP")
{
    bool32 reshow;
    PARAMETRIZE { reshow = FALSE; }
    PARAMETRIZE { reshow = TRUE; }
    GIVEN {
        Fixture();
        sReshow = reshow;
        TurnZeroScript(B_TRAINER_OPPONENT_A, (const u8 *)&sOpeningPayload);
        PLAYER(SPECIES_WOBBUFFET) { EntryScript(&sOnce); Speed(2); MovesWithPP({MOVE_CELEBRATE, 5}); }
        OPPONENT(SPECIES_WOBBUFFET) { EntryScript(&sOnce); Speed(1); MovesWithPP({MOVE_CELEBRATE, 5}); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(sCount, 3);
        EXPECT_EQ(sSynchronized, 3);
        EXPECT_EQ(sObserved[0].trainer, B_TRAINER_PLAYER);
        EXPECT_EQ(sObserved[1].trainer, B_TRAINER_OPPONENT_A);
        EXPECT_EQ(sObserved[2].opening, TRUE);
        for (u32 i = 0; i < sCount; i++)
        {
            EXPECT_EQ(sObserved[i].turn, 0);
            EXPECT_EQ(sObserved[i].pp, 5);
        }
        EXPECT_EQ(player->pp[0], 3);
        EXPECT_EQ(opponent->pp[0], 3);
        EXPECT_EQ(gBattleStruct->entryScriptRunning, FALSE);
        EXPECT_EQ(gBattleResources->battleCallbackStack->size, 0);
    } AFTER_CLEANUP {
        // The runner's post-restoration hook executes before resource freeing.
        EXPECT_EQ(gBattleStruct->entryScriptRunning, FALSE);
        EXPECT_EQ(gBattleResources->battleCallbackStack->size, 0);
    }
}

SINGLE_BATTLE_TEST("Entry scripts: once policy survives withdrawal and every-entry policy repeats on legitimate entries")
{
    const struct MonEntryScript *config;
    PARAMETRIZE { config = &sOnce; }
    PARAMETRIZE { config = &sEvery; }
    GIVEN {
        Fixture();
        PLAYER(SPECIES_WOBBUFFET) { Ability(ABILITY_TELEPATHY); }
        OPPONENT(SPECIES_WOBBUFFET) { EntryScript(config); Ability(ABILITY_TELEPATHY); }
        OPPONENT(SPECIES_ABRA) { Ability(ABILITY_INNER_FOCUS); }
    } WHEN {
        TURN { SWITCH(opponent, 1); }
        TURN { SWITCH(opponent, 0); }
        TURN { }
    } THEN {
        EXPECT_EQ(sCount, config == &sOnce ? 1 : 2);
        EXPECT_EQ(GetBattlerPartyState(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT))->entryScriptRuns, sCount);
        if (config == &sEvery)
        {
            EXPECT_EQ(sObserved[1].slot, 0);
            EXPECT_EQ(sObserved[1].runs, 2);
            EXPECT_EQ(sObserved[1].turn, 1);
        }
    }
}

SINGLE_BATTLE_TEST("Entry scripts: faint replacements have their own once-only state")
{
    GIVEN {
        Fixture();
        PLAYER(SPECIES_WOBBUFFET) { Speed(2); }
        OPPONENT(SPECIES_WOBBUFFET) { EntryScript(&sOnce); HP(1); Speed(1); }
        OPPONENT(SPECIES_ABRA) { EntryScript(&sOnce); Speed(1); }
    } WHEN {
        TURN { MOVE(player, MOVE_SCRATCH); SEND_OUT(opponent, 1); }
        TURN { }
    } THEN {
        EXPECT_EQ(sCount, 2);
        EXPECT_EQ(sObserved[0].slot, 0);
        EXPECT_EQ(sObserved[1].slot, 1);
        EXPECT_EQ(sObserved[0].runs, 1);
        EXPECT_EQ(sObserved[1].runs, 1);
    }
}

AI_SINGLE_BATTLE_TEST("Entry scripts: a scripted opening KO replaces the mon before the first actionable menu")
{
    bool32 opening;
    PARAMETRIZE { opening = FALSE; }
    PARAMETRIZE { opening = TRUE; }
    GIVEN {
        Fixture();
        if (opening)
            TurnZeroScript(B_TRAINER_OPPONENT_A, (const u8 *)&sFaintPayload);
        PLAYER(SPECIES_WOBBUFFET) { Ability(ABILITY_TELEPATHY); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { if (!opening) EntryScript(&sFaintOnce); }
        OPPONENT(SPECIES_ABRA) { EntryScript(&sOnce); MovesWithPP({MOVE_CELEBRATE, 5}); }
    } WHEN {
        TURN {
            // This replacement precedes all actions, so do not let the usual
            // EXPECT_SEND_OUT helper insert an implicit preceding AI move.
            gBattleTestRunnerState->data.actionBattlers |= 1u << (opponent - gBattleMons);
            EXPECT_SEND_OUT(opponent, 1);
            MOVE(player, MOVE_CELEBRATE);
            EXPECT_MOVE(opponent, MOVE_CELEBRATE);
        }
    } THEN {
        EXPECT_EQ(sCount, 2);
        EXPECT_EQ(sObserved[0].slot, 0);
        EXPECT_EQ(sObserved[1].slot, 1);
        EXPECT_EQ(sObserved[1].turn, 0);
        EXPECT_EQ(opponent->species, SPECIES_ABRA);
        EXPECT_EQ(opponent->pp[0], 4);
        EXPECT_EQ(gBattleStruct->openingScriptFaintPass, FALSE);
        EXPECT_EQ(gBattleStruct->entryScriptRunning, FALSE);
    }
}

SINGLE_BATTLE_TEST("Entry scripts: initial ability and Break Charm effects finish before the entry payload")
{
    GIVEN {
        Fixture();
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_ARBOK) { Ability(ABILITY_INTIMIDATE); Item(ITEM_BREAK_CHARM); EntryScript(&sOnce); }
    } WHEN {
        TURN { }
    } THEN {
        EXPECT_EQ(sCount, 1);
        EXPECT_EQ(sObserved[0].attackStage, DEFAULT_STAT_STAGE - 1);
        EXPECT_EQ(sObserved[0].bars, 1);
    }
}

WILD_BATTLE_TEST("Entry scripts: wild participants run entry and Turn 0 scripts without becoming trainer Pokemon")
{
    GIVEN {
        Fixture();
        TurnZeroScript(B_TRAINER_OPPONENT_A, (const u8 *)&sOpeningPayload);
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_MEW) { EntryScript(&sOnce); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(sCount, 2);
        EXPECT_EQ(sObserved[0].opening, FALSE);
        EXPECT_EQ(sObserved[1].opening, TRUE);
        EXPECT_EQ(gBattleTypeFlags & BATTLE_TYPE_TRAINER, 0);
    }
}

MULTI_BATTLE_TEST("Entry scripts: identical party slots in four independent owners do not share one-shot state")
{
    GIVEN {
        Fixture();
        PLAYER(SPECIES_WOBBUFFET) { EntryScript(&sOnce); Speed(4); }
        OPPONENT_A(SPECIES_WOBBUFFET) { EntryScript(&sOnce); Speed(3); }
        PARTNER(SPECIES_WOBBUFFET) { EntryScript(&sOnce); Speed(2); }
        OPPONENT_B(SPECIES_WOBBUFFET) { EntryScript(&sOnce); Speed(1); }
    } WHEN {
        TURN { }
    } THEN {
        EXPECT_EQ(sCount, 4);
        for (u32 i = 0; i < MAX_BATTLE_TRAINERS; i++)
        {
            EXPECT_EQ(sObserved[i].trainer, i);
            EXPECT_EQ(sObserved[i].slot, 0);
            EXPECT_EQ(gBattleStruct->partyState[i][0].entryScriptRuns, 1);
        }
    }
}

SINGLE_BATTLE_TEST("Entry scripts: unconfigured trainer battles retain normal PP without script dispatch")
{
    GIVEN {
        Fixture();
        PLAYER(SPECIES_WOBBUFFET) { MovesWithPP({MOVE_CELEBRATE, 5}); }
        OPPONENT(SPECIES_WOBBUFFET) { MovesWithPP({MOVE_CELEBRATE, 5}); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(sCount, 0);
        EXPECT_EQ(player->pp[0], 4);
        EXPECT_EQ(opponent->pp[0], 4);
        EXPECT_EQ(gBattleStruct->entryScriptRunning, FALSE);
    }
}

WILD_BATTLE_TEST("Entry scripts: unconfigured wild battles retain normal opening behavior")
{
    GIVEN {
        Fixture();
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { }
    } THEN {
        EXPECT_EQ(sCount, 0);
        EXPECT(GetBattlerPartyState(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT))->entryScript == NULL);
    }
}

SINGLE_BATTLE_TEST("Entry scripts: source-entry rules override exact-species defaults including explicit disable")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { }
    } THEN {
        const struct TrainerBattleScripts *config = INLINE_TRAINER_BATTLE_SCRIPTS(NULL,
            {ENTRY_SCRIPT_SPECIES, SPECIES_WOBBUFFET, &sEvery},
            {ENTRY_SCRIPT_SOURCE_ENTRY, 3, &sOnce},
            {ENTRY_SCRIPT_SOURCE_ENTRY, 4, NULL});
        EXPECT(ResolveMonEntryScript(config, SPECIES_WOBBUFFET, 0) == &sEvery);
        EXPECT(ResolveMonEntryScript(config, SPECIES_WOBBUFFET, 3) == &sOnce);
        EXPECT(ResolveMonEntryScript(config, SPECIES_WOBBUFFET, 4) == NULL);
        EXPECT(ResolveMonEntryScript(config, SPECIES_ABRA, 0) == NULL);
        EXPECT(GetTrainerBattleScripts(0xFFFF, DIFFICULTY_NORMAL) == &gEmptyTrainerBattleScripts);
    }
}

SINGLE_BATTLE_TEST("Entry scripts: regeneration clears outgoing party one-shot state without clearing other owners")
{
    GIVEN {
        Fixture();
        PLAYER(SPECIES_WOBBUFFET) { EntryScript(&sOnce); }
        OPPONENT(SPECIES_WOBBUFFET) { EntryScript(&sOnce); BreakBars(&sBars); }
    } WHEN {
        TURN { }
    } THEN {
        EXPECT_EQ(sCount, 2);
        BattleEntryScriptsBeginParty(gParties[B_TRAINER_OPPONENT_A]);
        EXPECT(GetBattlerPartyState(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT))->entryScript == NULL);
        EXPECT_EQ(GetBattlerPartyState(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT))->entryScriptRuns, 0);
        EXPECT_EQ(GetBattlerPartyState(GetBattlerAtPosition(B_POSITION_PLAYER_LEFT))->entryScriptRuns, 1);
        EXPECT_EQ(GetBreakBarCount(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT)), 1);
    }
}

SINGLE_BATTLE_TEST("Entry scripts: a full script stack rejects scheduling without changing context or one-shot state")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { }
    } THEN {
        enum BattlerId battler = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);
        struct PartyState *state = GetBattlerPartyState(battler);
        u32 stackSize = gBattleResources->battleScriptsStack->size;
        bool32 switchIn = gBattleStruct->battlerState[battler].switchIn;
        const u8 *cursor = gBattlescriptCurrInstr;
        state->entryScript = &sOnce;
        gBattleStruct->battlerState[battler].switchIn = TRUE;
        gBattleResources->battleScriptsStack->size = ARRAY_COUNT(gBattleResources->battleScriptsStack->ptr);
        EXPECT_EQ(TryRunMonEntryScript(battler), FALSE);
        EXPECT_EQ(state->entryScriptRuns, 0);
        EXPECT_EQ(gBattleStruct->entryScriptRunning, FALSE);
        EXPECT(gBattlescriptCurrInstr == cursor);
        gBattleResources->battleScriptsStack->size = stackSize;
        gBattleStruct->battlerState[battler].switchIn = switchIn;
    }
}

static const struct WildBattleScripts sWildConfig =
{
    .turnZeroScript = (const u8 *)&sOpeningPayload,
    .onEntry = {(const u8 *)&sNestedEntry, ENTRY_SCRIPT_ONCE_PER_BATTLE},
};

static void SetWildConfig(struct ScriptContext *ctx, bool32 arm)
{
    const struct WildBattleScripts *args = &sWildConfig;
    InitScriptContext(ctx, NULL, NULL);
    ctx->scriptPtr = (const u8 *)&args;
    Script_SetWildBattleScripts(ctx);
    ctx->scriptPtr = NULL;
    if (arm)
        Script_ArmWildBattleScripts(ctx);
    ScriptContext_ContinueScript(ctx);
}

WILD_BATTLE_TEST("Entry scripts: overworld wild configuration is consumed only when explicitly armed")
{
    bool32 arm;
    PARAMETRIZE { arm = FALSE; }
    PARAMETRIZE { arm = TRUE; }
    GIVEN {
        struct ScriptContext ctx;
        Fixture();
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_MEW);
        SetWildConfig(&ctx, arm);
    } WHEN {
        TURN { }
    } THEN {
        EXPECT_EQ(sCount, arm ? 2 : 0);
        EXPECT(Script_TakeWildBattleScripts() == NULL);
        ScriptContext_Init();
    }
}

SINGLE_BATTLE_TEST("Entry scripts: pending Break Bars resolve through returning scripts before command selection")
{
    GIVEN {
        Fixture();
        sBreakBar = TRUE;
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { EntryScript(&sOnce); BreakBars(&sBars); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(sCount, 1);
        EXPECT_EQ(GetBreakBarCount(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT)), 0);
        EXPECT_EQ(IsBreakBarPending(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT)), FALSE);
        EXPECT_EQ(opponent->hp, opponent->maxHP);
        EXPECT_EQ(gBattleStruct->entryScriptRunning, FALSE);
        EXPECT_EQ((u32)gBattleStruct->savedTargetCount, 0);
    }
}

static void SetTrainerConfig(struct ScriptContext *ctx, u16 trainer, const struct TrainerBattleScripts *config)
{
    const struct __attribute__((packed))
    {
        u16 trainer;
        const struct TrainerBattleScripts *config;
    } args = {trainer, config};
    ctx->scriptPtr = (const u8 *)&args;
    Script_SetTrainerBattleScripts(ctx);
    ctx->scriptPtr = NULL;
}

TEST("Entry scripts: trainer overrides consume by identity and script cancellation drops pending configuration")
{
    struct ScriptContext ctx;
    InitScriptContext(&ctx, NULL, NULL);
    SetTrainerConfig(&ctx, TRAINER_ROXANNE_1, &gEmptyTrainerBattleScripts);
    EXPECT_EQ(gSpecialVar_Result, TRUE);
    ScriptContext_ContinueScript(&ctx);
    EXPECT(Script_TakeTrainerBattleScripts(TRAINER_ALICE) == NULL);
    EXPECT(Script_TakeTrainerBattleScripts(TRAINER_ROXANNE_1) == &gEmptyTrainerBattleScripts);
    EXPECT(Script_TakeTrainerBattleScripts(TRAINER_ROXANNE_1) == NULL);
    SetWildConfig(&ctx, TRUE);
    StopScript(&ctx);
    ScriptContext_ContinueScript(&ctx);
    EXPECT(Script_TakeWildBattleScripts() == NULL);
    ScriptContext_Init();
}

static const struct TrainerMon sPool[] =
{
    {.species = SPECIES_WOBBUFFET, .lvl = 10, .gender = TRAINER_MON_RANDOM_GENDER},
    {.species = SPECIES_PICHU, .lvl = 10, .gender = TRAINER_MON_RANDOM_GENDER},
    {.species = SPECIES_GEODUDE, .lvl = 10, .gender = TRAINER_MON_RANDOM_GENDER},
};
static const struct TrainerBattleScripts *const sPoolConfig = INLINE_TRAINER_BATTLE_SCRIPTS((const u8 *)&sOpeningPayload,
    {ENTRY_SCRIPT_SOURCE_ENTRY, 0, &sOnce},
    {ENTRY_SCRIPT_SOURCE_ENTRY, 1, &sEvery},
    {ENTRY_SCRIPT_SOURCE_ENTRY, 2, NULL});

TEST("Entry scripts: selected pool entries reach the correct independent owner through the pre-allocation handoff")
{
    struct Trainer trainer = {.party = sPool, .partySize = 2, .poolSize = ARRAY_COUNT(sPool)};
    u32 savedFlags = gBattleTypeFlags;
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_TWO_OPPONENTS;
    for (u32 owner = B_TRAINER_OPPONENT_A; owner <= B_TRAINER_OPPONENT_B; owner += BIT_FLANK)
    {
        struct ScriptContext ctx;
        InitScriptContext(&ctx, NULL, NULL);
        SetTrainerConfig(&ctx, TRAINER_ROXANNE_1, sPoolConfig);
        ScriptContext_ContinueScript(&ctx);
        CreateNPCTrainerPartyWithScripts(gParties[owner], &trainer, TRAINER_ROXANNE_1, &gEmptyTrainerBreakBars);
    }
    gBattleStruct = AllocZeroed(sizeof(*gBattleStruct));
    BattleEntryScriptsApplyPending();
    for (u32 owner = B_TRAINER_OPPONENT_A; owner <= B_TRAINER_OPPONENT_B; owner += BIT_FLANK)
    {
        EXPECT(gBattleStruct->turnZeroScripts[owner] == (const u8 *)&sOpeningPayload);
        for (u32 slot = 0; slot < trainer.partySize; slot++)
        {
            u32 species = GetMonData(&gParties[owner][slot], MON_DATA_SPECIES);
            const struct MonEntryScript *expected = species == SPECIES_WOBBUFFET ? &sOnce : species == SPECIES_PICHU ? &sEvery : NULL;
            EXPECT(gBattleStruct->partyState[owner][slot].entryScript == expected);
            EXPECT_EQ(gBattleStruct->partyState[owner][slot].entryScriptRuns, 0);
        }
    }
    EXPECT(gBattleStruct->partyState[B_TRAINER_PLAYER][0].entryScript == NULL);
    EXPECT(gBattleStruct->partyState[B_TRAINER_PARTNER][0].entryScript == NULL);
    Free(gBattleStruct);
    gBattleStruct = NULL;
    gBattleTypeFlags = savedFlags;
    BattleEntryScriptsClear();
    TrainerBreakBarsClearPending();
    ScriptContext_Init();
}
