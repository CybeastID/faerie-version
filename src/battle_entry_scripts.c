#include "global.h"
#include "battle.h"
#include "battle_controllers.h"
#include "battle_entry_scripts.h"
#include "battle_script_native.h"
#include "battle_scripts.h"
#include "battle_setup.h"
#include "pokemon.h"
#include "difficulty.h"
#include "script.h"
#include "test_runner.h"
#include "constants/trainers.h"

const struct TrainerBattleScripts gEmptyTrainerBattleScripts = {0};
#include "data/trainer_battle_scripts.h"

// Trainer generation precedes BattleStruct allocation on the overworld path.
// These buffers do not need IWRAM; keep room for the engine's normal stack.
static EWRAM_DATA const struct MonEntryScript *sPendingEntries[MAX_BATTLE_TRAINERS][PARTY_SIZE] = {0};
static EWRAM_DATA const u8 *sPendingOpenings[MAX_BATTLE_TRAINERS] = {0};
static EWRAM_DATA const struct TrainerBattleScripts *sGenerationScripts = NULL;

// Only one returning configured script may own the interpreter at a time.
// Graphics teardown deliberately does not touch this context.
static EWRAM_DATA struct
{
    struct BattleScripting scripting;
    const u8 *script;
    const u8 *trainerSlideMsg;
    u32 hitMarker;
    u16 currentMove;
    u16 chosenMove;
    enum Item lastItem;
    enum Ability lastAbility;
    u8 communication[BATTLE_COMMUNICATION_ENTRIES_COUNT];
    s16 moveDamage[MAX_BATTLERS_COUNT];
    s16 passiveHpUpdate[MAX_BATTLERS_COUNT];
    u32 moveResultFlags[MAX_BATTLERS_COUNT];
    enum BattlerId bySpeed[MAX_BATTLERS_COUNT];
    enum BattlerId byRawSpeed[MAX_BATTLERS_COUNT];
    u8 attacker;
    u8 target;
    u8 effectBattler;
    u8 abilityBattler;
    u8 faintedBattler;
    u8 currentMovePos;
    u8 chosenMovePos;
    u8 actionFunc;
    bool8 unableToUseMove;
} sContext = {0};

static u32 TrainerForParty(struct Pokemon *party)
{
    for (u32 trainer = 0; trainer < MAX_BATTLE_TRAINERS; trainer++)
        if (party == gParties[trainer])
            return trainer;
    return MAX_BATTLE_TRAINERS;
}

const struct TrainerBattleScripts *GetTrainerBattleScripts(u16 trainerId, enum DifficultyLevel difficulty)
{
    if (trainerId == TRAINER_NONE || trainerId >= TRAINERS_COUNT || (u32)difficulty >= DIFFICULTY_COUNT)
        return &gEmptyTrainerBattleScripts;
    return sTrainerBattleScripts[trainerId][difficulty]
        ?: sTrainerBattleScripts[trainerId][DIFFICULTY_NORMAL] ?: &gEmptyTrainerBattleScripts;
}

const struct MonEntryScript *ResolveMonEntryScript(const struct TrainerBattleScripts *config, u32 species, u32 sourceEntry)
{
    const struct MonEntryScript *speciesConfig = NULL;
    if (config == NULL || config->entries == NULL)
        return NULL;
    for (u32 i = 0; i < config->count; i++)
    {
        const struct MonEntryScriptRule *rule = &config->entries[i];
        if (rule->selector == ENTRY_SCRIPT_SOURCE_ENTRY && rule->value == sourceEntry)
            return rule->config;
        if (rule->selector == ENTRY_SCRIPT_SPECIES && rule->value == species)
            speciesConfig = rule->config;
    }
    return speciesConfig;
}

void BattleEntryScriptsBeginParty(struct Pokemon *party)
{
    u32 trainer = TrainerForParty(party);
    if (trainer >= MAX_BATTLE_TRAINERS)
        return;
    memset(sPendingEntries[trainer], 0, sizeof(sPendingEntries[trainer]));
    sPendingOpenings[trainer] = sGenerationScripts == NULL ? NULL : sGenerationScripts->turnZeroScript;
    if (gBattleStruct != NULL)
    {
        for (u32 slot = 0; slot < PARTY_SIZE; slot++)
        {
            struct PartyState *state = &gBattleStruct->partyState[trainer][slot];
            state->entryScript = NULL;
            state->entryScriptRuns = 0;
        }
        gBattleStruct->turnZeroScripts[trainer] = sPendingOpenings[trainer];
    }
}

void BattleEntryScriptsRecordMon(struct Pokemon *party, u32 slot, u32 species, u32 sourceEntry)
{
    u32 trainer = TrainerForParty(party);
    if (trainer >= MAX_BATTLE_TRAINERS || slot >= PARTY_SIZE)
        return;
    const struct MonEntryScript *config = ResolveMonEntryScript(sGenerationScripts, species, sourceEntry);
    sPendingEntries[trainer][slot] = config;
    if (gBattleStruct != NULL)
        gBattleStruct->partyState[trainer][slot].entryScript = config;
}

u8 CreateNPCTrainerPartyWithScripts(struct Pokemon *party, const struct Trainer *trainer, u16 trainerId, const struct TrainerBreakBarRules *bars)
{
    const struct TrainerBattleScripts *saved = sGenerationScripts;
    sGenerationScripts = Script_TakeTrainerBattleScripts(trainerId);
    if (sGenerationScripts == NULL && trainerId < TRAINERS_COUNT)
        sGenerationScripts = GetTrainerBattleScripts(trainerId, GetTrainerDifficultyLevel(trainerId));
    u8 count = CreateNPCTrainerPartyWithBreakBars(party, trainer, TRUE, gBattleTypeFlags, bars);
    sGenerationScripts = saved;
    return count;
}

void BattleEntryScriptsApplyPending(void)
{
    if (gBattleStruct != NULL && (gBattleTypeFlags & BATTLE_TYPE_TRAINER))
        for (u32 trainer = 0; trainer < MAX_BATTLE_TRAINERS; trainer++)
        {
            gBattleStruct->turnZeroScripts[trainer] = sPendingOpenings[trainer];
            for (u32 slot = 0; slot < PARTY_SIZE; slot++)
                gBattleStruct->partyState[trainer][slot].entryScript = sPendingEntries[trainer][slot];
        }
    memset(sPendingEntries, 0, sizeof(sPendingEntries));
    memset(sPendingOpenings, 0, sizeof(sPendingOpenings));
}

void BattleEntryScriptsInitializeWild(const struct WildBattleScripts *config)
{
    if (config == NULL || (gBattleTypeFlags & BATTLE_TYPE_TRAINER))
        return;
    gBattleStruct->turnZeroScripts[B_TRAINER_OPPONENT_A] = config->turnZeroScript;
    gBattleStruct->partyState[B_TRAINER_OPPONENT_A][0].entryScript = &config->onEntry;
}

void BattleEntryScriptsClear(void)
{
    memset(sPendingEntries, 0, sizeof(sPendingEntries));
    memset(sPendingOpenings, 0, sizeof(sPendingOpenings));
    memset(&sContext, 0, sizeof(sContext));
    sGenerationScripts = NULL;
}

static bool32 StartScript(u32 battler, const u8 *script, bool32 opening)
{
    if (gBattleOutcome != 0 || gBattleStruct->entryScriptRunning || script == NULL
     || !IsBattlerAlive(battler)
     || (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_SAFARI))
     // The harness uses RECORDED for every battle and RECORDED_LINK for
     // non-AI trainer fixtures, even though neither uses a real link/replay.
     || (!gTestRunnerEnabled && (gBattleTypeFlags & (BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK))))
        return FALSE;
    // Reserve the scheduler's complete call chain before changing any context.
    u32 needed = 2;
    if (gBattleResources->battleScriptsStack->size + needed > ARRAY_COUNT(gBattleResources->battleScriptsStack->ptr)
     || (opening && gBattleResources->battleCallbackStack->size >= ARRAY_COUNT(gBattleResources->battleCallbackStack->function)))
        return FALSE;

    sContext.scripting = gBattleScripting;
    sContext.script = script;
    sContext.trainerSlideMsg = gBattleStruct->trainerSlideMsg;
    sContext.hitMarker = gHitMarker;
    sContext.currentMove = gCurrentMove;
    sContext.chosenMove = gChosenMove;
    sContext.lastItem = gLastUsedItem;
    sContext.lastAbility = gLastUsedAbility;
    memcpy(sContext.communication, gBattleCommunication, sizeof(sContext.communication));
    memcpy(sContext.moveDamage, gBattleStruct->moveDamage, sizeof(sContext.moveDamage));
    memcpy(sContext.passiveHpUpdate, gBattleStruct->passiveHpUpdate, sizeof(sContext.passiveHpUpdate));
    memcpy(sContext.moveResultFlags, gBattleStruct->moveResultFlags, sizeof(sContext.moveResultFlags));
    memcpy(sContext.bySpeed, gBattlersBySpeed, sizeof(sContext.bySpeed));
    memcpy(sContext.byRawSpeed, gBattlersByRawSpeed, sizeof(sContext.byRawSpeed));
    sContext.attacker = gBattlerAttacker;
    sContext.target = gBattlerTarget;
    sContext.effectBattler = gEffectBattler;
    sContext.abilityBattler = gBattlerAbility;
    sContext.faintedBattler = gBattlerFainted;
    sContext.currentMovePos = gCurrMovePos;
    sContext.chosenMovePos = gChosenMovePos;
    sContext.actionFunc = gCurrentActionFuncId;
    sContext.unableToUseMove = gBattleStruct->unableToUseMove;
    gBattleStruct->entryScriptRunning = TRUE;
    gBattleStruct->entryScriptHasRun = TRUE;
    gSaveBlock2Ptr->frontier.disableRecordBattle = TRUE;
    gBattlerAttacker = gBattlerTarget = gEffectBattler = gBattleScripting.battler = battler;
    gCurrentMove = gChosenMove = MOVE_NONE;
    memset(gBattleStruct->moveDamage, 0, sizeof(gBattleStruct->moveDamage));
    memset(gBattleStruct->moveResultFlags, 0, sizeof(gBattleStruct->moveResultFlags));
    if (opening)
        BattleScriptPushCursorAndCallback(BattleScript_ConfiguredTurnZero);
    else
        BattleScriptCall(BattleScript_ConfiguredEntry);
    return TRUE;
}

bool32 TryRunBattleEventScript(u32 battler, const u8 *script)
{
    if (gBattleControllerExecFlags || battler >= gBattlersCount)
        return FALSE;
    return StartScript(battler, script, FALSE);
}

bool32 TryRunMonEntryScript(u32 battler)
{
    if (battler >= gBattlersCount || !gBattleStruct->battlerState[battler].switchIn || !IsBattlerAlive(battler))
        return FALSE;
    struct PartyState *state = GetBattlerPartyState(battler);
    const struct MonEntryScript *config = state->entryScript;
    if (config == NULL || config->policy > ENTRY_SCRIPT_EVERY_ENTRY
     || (config->policy == ENTRY_SCRIPT_ONCE_PER_BATTLE && state->entryScriptRuns != 0))
        return FALSE;
    if (!StartScript(battler, config->script, FALSE))
        return FALSE;
    // Commit before the first interpreter tick, so yields/reshows cannot replay.
    if (state->entryScriptRuns != 0xFF)
        state->entryScriptRuns++;
    return TRUE;
}

bool32 TryRunTurnZeroScript(void)
{
    for (u32 trainer = 0; trainer < MAX_BATTLE_TRAINERS; trainer++)
    {
        if (gBattleStruct->turnZeroScriptsDone & (1u << trainer))
            continue;
        gBattleStruct->turnZeroScriptsDone |= 1u << trainer;
        for (u32 battler = 0; battler < gBattlersCount; battler++)
            if (GetBattlerTrainer(battler) == trainer && IsBattlerAlive(battler))
            {
                if (StartScript(battler, gBattleStruct->turnZeroScripts[trainer], TRUE))
                    return TRUE;
                break;
            }
    }
    return FALSE;
}

bool32 BattleEntryScriptsConfigured(void)
{
    for (u32 trainer = 0; trainer < MAX_BATTLE_TRAINERS; trainer++)
    {
        if (gBattleStruct->turnZeroScripts[trainer] != NULL)
            return TRUE;
        for (u32 slot = 0; slot < PARTY_SIZE; slot++)
        {
            const struct MonEntryScript *config = gBattleStruct->partyState[trainer][slot].entryScript;
            if (config != NULL && config->script != NULL)
                return TRUE;
        }
    }
    return FALSE;
}

void FinishOpeningScriptFaints(void)
{
    if (gBattleStruct->openingScriptFaintPass)
    {
        gCurrentTurnActionNumber = gBattleStruct->openingScriptActionCursor;
        gBattleStruct->openingScriptFaintPass = FALSE;
    }
}

bool32 HandleOpeningScriptFaints(void)
{
    if (!gBattleStruct->entryScriptHasRun)
        return HandleFaintedMonActions();
    if (!gBattleStruct->openingScriptFaintPass)
    {
        gBattleStruct->openingScriptActionCursor = gCurrentTurnActionNumber;
        gBattleStruct->openingScriptFaintPass = TRUE;
    }
    // There are no chosen actions before the first menu. Gen 4+ replacement
    // deferral must not mistake this for an unfinished ordinary action queue.
    gCurrentTurnActionNumber = gBattlersCount;
    if (HandleFaintedMonActions())
        return TRUE;
    FinishOpeningScriptFaints();
    return FALSE;
}

void BS_JumpIfEntryScriptReentry(void)
{
    NATIVE_ARGS(u8 battler, const u8 *jumpInstr);
    u32 battler = GetBattlerForBattleScript(cmd->battler);
    gBattlescriptCurrInstr = GetBattlerPartyState(battler)->entryScriptRuns > 1 ? cmd->jumpInstr : cmd->nextInstr;
}

void BS_SetEntryTrainerSlideMessage(void)
{
    NATIVE_ARGS(const u8 *message);
    gBattleStruct->trainerSlideMsg = cmd->message;
    gBattlescriptCurrInstr = cmd->nextInstr;
}

void BS_CallConfiguredEntryScript(void)
{
    NATIVE_ARGS();
    gBattlescriptCurrInstr = cmd->nextInstr;
    BattleScriptCall(sContext.script);
}

void BS_FinishConfiguredEntryScript(void)
{
    NATIVE_ARGS();
    if (gBattleControllerExecFlags)
        return;
    // Resolve returning bar scripts without a new callback or action/faint pass.
    // The enclosing switch/turn-opening scheduler owns faint replacements.
    if (gBattleOutcome == 0)
        for (u32 battler = 0; battler < gBattlersCount; battler++)
            if (IsBreakBarPending(battler) && TryBreakBar(battler, gBattlescriptCurrInstr))
                return;
    gBattleScripting = sContext.scripting;
    gBattlerAttacker = sContext.attacker;
    gBattlerTarget = sContext.target;
    gEffectBattler = sContext.effectBattler;
    gBattlerAbility = sContext.abilityBattler;
    gBattlerFainted = sContext.faintedBattler;
    gCurrentMove = sContext.currentMove;
    gChosenMove = sContext.chosenMove;
    gLastUsedItem = sContext.lastItem;
    gLastUsedAbility = sContext.lastAbility;
    gCurrMovePos = sContext.currentMovePos;
    gChosenMovePos = sContext.chosenMovePos;
    gCurrentActionFuncId = sContext.actionFunc;
    gBattleStruct->unableToUseMove = sContext.unableToUseMove;
    gBattleStruct->trainerSlideMsg = sContext.trainerSlideMsg;
    memcpy(gBattleCommunication, sContext.communication, sizeof(sContext.communication));
    memcpy(gBattleStruct->moveDamage, sContext.moveDamage, sizeof(sContext.moveDamage));
    memcpy(gBattleStruct->passiveHpUpdate, sContext.passiveHpUpdate, sizeof(sContext.passiveHpUpdate));
    memcpy(gBattleStruct->moveResultFlags, sContext.moveResultFlags, sizeof(sContext.moveResultFlags));
    memcpy(gBattlersBySpeed, sContext.bySpeed, sizeof(sContext.bySpeed));
    memcpy(gBattlersByRawSpeed, sContext.byRawSpeed, sizeof(sContext.byRawSpeed));
    // Keep real faint/damage flags; restore only transient presentation state.
    gHitMarker = (gHitMarker & ~HITMARKER_DISABLE_ANIMATION) | (sContext.hitMarker & HITMARKER_DISABLE_ANIMATION);
    gBattleStruct->entryScriptRunning = FALSE;
    sContext.script = NULL;
    gBattlescriptCurrInstr = cmd->nextInstr;
}
