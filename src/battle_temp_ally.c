#include "global.h"
#include "battle_script_native.h"
#include "battle.h"
#include "battle_ai_main.h"
#include "battle_ai_record.h"
#include "battle_break_bar.h"
#include "battle_reduction.h"
#include "battle_status_icons.h"
#include "battle_util.h"
#include "battle_controllers.h"
#include "battle_gfx_sfx_util.h"
#include "battle_interface.h"
#include "battle_koanko.h"
#include "battle_anim.h"
#include "battle_main.h"
#include "battle_script_commands.h"
#include "battle_setup.h"
#include "battle_temp_ally.h"
#include "battle_tx_rollback.h"
#include "pokemon.h"
#include "sprite.h"
#include "sound.h"
#include "task.h"
#include "data.h"
#include "constants/songs.h"
#include "constants/trainers.h"
#include "constants/battle_ai.h"
#include "constants/battle_anim.h"

enum TempAllyState TempAllyGetState(void)
{
    return gBattleStruct != NULL ? gBattleStruct->tempAllyState : TEMP_ALLY_NONE;
}

void TempAllySetState(enum TempAllyState state)
{
    if (gBattleStruct != NULL)
        gBattleStruct->tempAllyState = state;
}

bool32 TempAllyOwnsBattler(u32 battler)
{
    return battler == TEMP_ALLY_BATTLER && TempAllyGetState() != TEMP_ALLY_NONE;
}

bool32 TempAllyOwnsTrainer(enum BattleTrainer trainer)
{
    return trainer == B_TRAINER_PARTNER && TempAllyGetState() != TEMP_ALLY_NONE;
}

void TempAllyClearStorage(void)
{
    if (gBattleStruct == NULL)
        return;

    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        ZeroMonData(&gParties[B_TRAINER_PARTNER][i]);
        memset(&gBattleStruct->partyState[B_TRAINER_PARTNER][i], 0, sizeof(gBattleStruct->partyState[B_TRAINER_PARTNER][i]));
        gBattleStruct->gimmick.activeGimmick[B_TRAINER_PARTNER][i] = GIMMICK_NONE;
    }
    gBattleStruct->faintCounter[B_TRAINER_PARTNER] = 0;
    gPartiesCount[B_TRAINER_PARTNER] = 0;
    memset(gBattleStruct->itemLost[B_TRAINER_PARTNER], 0, sizeof(gBattleStruct->itemLost[B_TRAINER_PARTNER]));

    if (gAiPartyData != NULL)
    {
        memset(&gAiPartyData->mons[B_TRAINER_PARTNER], 0, sizeof(gAiPartyData->mons[B_TRAINER_PARTNER]));
        gAiPartyData->count[B_TRAINER_PARTNER] = 0;
    }
}

// Level 100 Zacian-Crowned, spawned directly: the form table only reverts it with Rusted Sword held, so a
// competitive item is fine. 31 IVs, Jolly (personality 13), 252 Atk / 252 Spe / 4 HP, Life Orb.
// Tuning is provisional until approved: level, EVs, nature and item are all set here and nowhere else.
#define ZACIAN_LEVEL 100
#define ZACIAN_PERSONALITY 13 // % NUM_NATURES == NATURE_JOLLY

static void CreateTempAllyZacian(void)
{
    struct Pokemon *mon = &gParties[B_TRAINER_PARTNER][0];
    static const u16 moves[MAX_MON_MOVES] = {MOVE_BEHEMOTH_BLADE, MOVE_SACRED_SWORD, MOVE_PLAY_ROUGH, MOVE_SWORDS_DANCE};
    u8 hpEv = 4, atkEv = 252, speedEv = 252;
    enum Item item = ITEM_LIFE_ORB;

    CreateMonWithIVs(mon, SPECIES_ZACIAN_CROWNED, ZACIAN_LEVEL, ZACIAN_PERSONALITY, OTID_STRUCT_PRESET(0x12345678), 31);
    SetMonData(mon, MON_DATA_HP_EV + STAT_HP, &hpEv);
    SetMonData(mon, MON_DATA_HP_EV + STAT_ATK, &atkEv);
    SetMonData(mon, MON_DATA_HP_EV + STAT_SPEED, &speedEv);
    SetMonData(mon, MON_DATA_HELD_ITEM, &item);
    for (u32 i = 0; i < MAX_MON_MOVES; i++)
        SetMonMoveSlot(mon, moves[i], i);
    CalculateMonStats(mon);
    gBattleStruct->itemLost[B_TRAINER_PARTNER][0].originalItem = item;
    gPartiesCount[B_TRAINER_PARTNER] = CalculatePartyCount(B_TRAINER_PARTNER);
}

bool32 TempAllyCanEnter(void)
{
    u32 freeSprites = 0;
    for (u32 i = 0; i < MAX_SPRITES; i++)
        freeSprites += !gSprites[i].inUse;

    if (gBattleStruct == NULL
     || TempAllyGetState() != TEMP_ALLY_NONE
     || gBattleStruct->koankoPhase != KOANKO_CONJURED_TEAM
     || TxRollbackSummonFormationActive()
     || (gBattleTypeFlags & (BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_LINK))
     || gBattlersCount != 2
     || freeSprites < 20)
        return FALSE;

    // Same split as TxRollback's creation check: battle tests do not populate the trainer parameters and
    // run as recorded battles, so the encounter identity checks apply to real battles only. The
    // koankoPhase gate above is what ties a test fixture to the encounter.
    if (gTestRunnerEnabled)
        return TRUE;

    return !(gBattleTypeFlags & BATTLE_TYPE_RECORDED)
        && TRAINER_BATTLE_PARAM.opponentA == TRAINER_KOANKO;
}

// Mirrors BS_TxRollbackCreate's live 2 -> 4 expansion, but player-right is the enabled slot and
// opponent-right stays absent. Existing positions, choices and the action queue are untouched.
void BS_TempAllyEnter(void)
{
    enum BattlerId allyBattler = GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT);
    NATIVE_ARGS(const u8 *failInstr);

    if (!TempAllyCanEnter())
    {
        gBattlescriptCurrInstr = cmd->failInstr;
        return;
    }

    TempAllyClearStorage();
    TempAllySetState(TEMP_ALLY_ENTERING);
    CreateTempAllyZacian();
    if (gAiPartyData != NULL)
        gAiPartyData->count[B_TRAINER_PARTNER] = gPartiesCount[B_TRAINER_PARTNER];

    gBattleTypeFlags |= BATTLE_TYPE_DOUBLE;
    gBattlersCount = MAX_BATTLERS_COUNT;
    for (u32 battler = 2; battler < MAX_BATTLERS_COUNT; battler++)
    {
        gBattlerPositions[battler] = battler;
        gBattlerPartyIndexes[battler] = battler == B_POSITION_PLAYER_RIGHT ? 0 : gBattlerPartyIndexes[B_POSITION_OPPONENT_LEFT];
        memset(&gBattleMons[battler], 0, sizeof(gBattleMons[battler]));
        memset(&gProtectStructs[battler], 0, sizeof(gProtectStructs[battler]));
        memset(&gSpecialStatuses[battler], 0, sizeof(gSpecialStatuses[battler]));
        memset(&gBattleStruct->battlerState[battler], 0, sizeof(gBattleStruct->battlerState[battler]));
        gBattleStruct->monToSwitchIntoId[battler] = PARTY_SIZE;
        gBattleStruct->AI_monToSwitchIntoId[battler] = PARTY_SIZE;
        gBattleStruct->chosenMovePositions[battler] = 0;
        gChosenMoveByBattler[battler] = MOVE_NONE;
        gChosenActionByBattler[battler] = B_ACTION_NOTHING_FAINTED;
        gBattlerByTurnOrder[battler] = battler;
        gActionsByTurnOrder[battler] = B_ACTION_NOTHING_FAINTED;
        gBattlerSpriteIds[battler] = MAX_SPRITES;
        gHealthboxSpriteIds[battler] = MAX_SPRITES;
        memset(gBattleStruct->battlerPartyOrders[battler], 0, sizeof(gBattleStruct->battlerPartyOrders[battler]));
        // Ordinary move scoring plus the owner-gated Koanko tactical pass.
        gAiThinkingStruct->aiFlags[battler] = AI_FLAG_CHECK_BAD_MOVE | AI_FLAG_CHECK_VIABILITY | AI_FLAG_TRY_TO_FAINT | AI_FLAG_OMNISCIENT | AI_FLAG_DYNAMIC_FUNC;
        if (battler == B_POSITION_PLAYER_RIGHT)
            SetControllerToPlayerPartner(battler); // AI-driven, never opens a party menu for the player.
        else
            SetControllerToOpponent(battler); // Absent slot never emits commands.
    }
    gAbsentBattlerFlags &= ~(1u << B_POSITION_PLAYER_RIGHT);
    gAbsentBattlerFlags |= 1u << B_POSITION_OPPONENT_RIGHT;
    gBattleScripting.battler = allyBattler;
    BtlController_EmitGetMonData(allyBattler, B_COMM_TO_CONTROLLER, REQUEST_ALL_BATTLE, 1u << 0);
    MarkBattlerForControllerExec(allyBattler);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

// Runs after switchindataupdate has completed SwitchInClearSetData for the ally.
void BS_TempAllyInitialize(void)
{
    NATIVE_ARGS();
    u32 battler = B_POSITION_PLAYER_RIGHT;

    BattleLoadTempAllyHealthboxGfx(); // Must precede the healthbox sprites that use these tile tags.
    gHealthboxSpriteIds[battler] = CreateBattlerHealthboxSprites(battler);
    InitBattlerHealthboxCoords(battler);
    UpdateHealthboxAttribute(gHealthboxSpriteIds[battler], GetBattlerMon(battler), HEALTHBOX_ALL);
    // The player's left battler now uses doubles coordinates. Its singles-template healthbox cannot be
    // re-framed in place, so rebuild it; the opposing battler keeps its singles layout (one opposing party mon).
    RebuildBattlerHealthbox(GetBattlerAtPosition(B_POSITION_PLAYER_LEFT));
    for (u32 other = 0; other < B_POSITION_PLAYER_RIGHT; other++)
    {
        if (other != B_POSITION_PLAYER_LEFT)
        {
            InitBattlerHealthboxCoords(other);
            UpdateHealthboxAttribute(gHealthboxSpriteIds[other], GetBattlerMon(other), HEALTHBOX_ALL);
        }
        if (IsBattlerSpritePresent(other))
            gSprites[gBattlerSpriteIds[other]].y = GetBattlerSpriteDefault_Y(other);
    }
    gBattlescriptCurrInstr = cmd->nextInstr;
}

// Assembly-only native, repeated every frame like BS_TxRollbackSlideCaster: advance the player's existing
// battler two pixels per frame to its doubles X before the ally arrives, keeping its animation state.
void BS_TempAllySlideBattlers(void)
{
    NATIVE_ARGS();
    u32 battler = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
    u32 spriteId = gBattlerSpriteIds[battler];

    if (IsBattlerSpritePresent(battler) && spriteId < MAX_SPRITES && gSprites[spriteId].inUse)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        s16 targetX = GetBattlerSpriteCoord(battler, BATTLER_COORD_X_2);

        if (sprite->x != targetX)
        {
            sprite->x = sprite->x < targetX ? min(sprite->x + 2, targetX) : max(sprite->x - 2, targetX);
            return; // Re-run this command next frame.
        }
    }
    gBattlescriptCurrInstr = cmd->nextInstr;
}

void BS_TempAllyFinishEntry(void)
{
    NATIVE_ARGS();
    // The ally joins ordinary AI next turn: it was never in the already-chosen action queue.
    gBattleStruct->battlerState[B_POSITION_PLAYER_RIGHT].noOrderChange = TRUE;
    if (TempAllyGetState() == TEMP_ALLY_ENTERING)
        TempAllySetState(TEMP_ALLY_ACTIVE);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

// Ball-less entrance: the ally zips in from the left screen edge, fast at first and easing into place.
// Send-out contract (see kazuradrop_entrance.c): gDoingBattleAnim and ballAnimActive stay TRUE while it
// runs, the sprite ends visible in BATTLER_AFFINE_NORMAL, the controller's wait-sprite ends at
// SpriteCallbackDummy, and both flags are cleared at the end. No ball graphics were loaded for the ally,
// so none are freed.
#define TEMP_ALLY_ZIP_START_X   (-40)
#define TEMP_ALLY_ZIP_MIN_STEP  3
#define TEMP_ALLY_ZIP_MAX_STEP  14
#define TEMP_ALLY_ZIP_HOLD      12
#define TEMP_ALLY_DASH_END_X    (DISPLAY_WIDTH + MON_PIC_WIDTH)
#define TEMP_ALLY_DASH_STEP     16
#define TEMP_ALLY_RECENTER_STEP 2

static void Task_TempAllyZipIn(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    u32 battler = data[0];
    struct Sprite *mon = &gSprites[gBattlerSpriteIds[battler]];

    switch (data[2])
    {
    case 0:
        data[3] = mon->x; // target X, already set at creation
        mon->x = TEMP_ALLY_ZIP_START_X;
        mon->invisible = FALSE;
        StartSpriteAffineAnim(mon, BATTLER_AFFINE_NORMAL);
        PlaySE(SE_M_SWIFT);
        data[2] = 1;
        break;
    case 1:
    {
        s32 step = (data[3] - mon->x) / 4;

        step = max(TEMP_ALLY_ZIP_MIN_STEP, min(step, TEMP_ALLY_ZIP_MAX_STEP));
        mon->x += step;
        if (mon->x >= data[3])
        {
            mon->x = data[3];
            PlayCry_ByMode(GetBattlerVisualSpecies(battler), -25, CRY_MODE_NORMAL);
            data[1] = 0;
            data[2] = 2;
        }
        break;
    }
    case 2:
        if (++data[1] >= TEMP_ALLY_ZIP_HOLD)
        {
            gSprites[gBattleControllerData[battler]].callback = SpriteCallbackDummy;
            gDoingBattleAnim = FALSE;
            gBattleSpritesDataPtr->healthBoxesData[battler].ballAnimActive = FALSE;
            DestroyTask(taskId);
        }
        break;
    }
}

u8 DoTempAllyZipInAnimation(u32 battler)
{
    u8 taskId;

    gDoingBattleAnim = TRUE;
    gBattleSpritesDataPtr->healthBoxesData[battler].ballAnimActive = TRUE;
    taskId = CreateTask(Task_TempAllyZipIn, 5);
    gTasks[taskId].data[0] = battler;
    gTasks[taskId].data[1] = 0;
    gTasks[taskId].data[2] = 0;
    return 0;
}

// The existing encounter-slot seam is shared with TX. Ownership and lifetimes
// remain separate: only these right positions are reserved for this encounter.
enum TxRollbackSlotPolicy TempAllyGetSlotPolicy(u32 battler)
{
    if (TempAllyGetState() == TEMP_ALLY_NONE)
        return TX_SLOT_NORMAL;
    if (battler == B_POSITION_OPPONENT_RIGHT)
        return TX_SLOT_DISABLED;
    if (battler == TEMP_ALLY_BATTLER)
        return TempAllyGetState() == TEMP_ALLY_FAINTED ? TX_SLOT_RETIRED : TX_SLOT_RESERVED;
    return TX_SLOT_NORMAL;
}

void TempAllyOnFaint(u32 battler)
{
    if (TempAllyOwnsBattler(battler) && gBattleMons[battler].hp == 0 && !IsBreakBarPending(battler))
        TempAllySetState(TEMP_ALLY_FAINTED);
}

bool32 TempAllyTryDeparture(void)
{
    extern const u8 BattleScript_KoankoZacianDepart[];
    if (TempAllyGetState() == TEMP_ALLY_NONE || TempAllyGetState() == TEMP_ALLY_DEPARTING
     || gCurrentTurnActionNumber < gBattlersCount || gBattleControllerExecFlags != 0
     || gBattleMons[B_POSITION_OPPONENT_LEFT].species != SPECIES_ETERNATUS_ETERNAMAX
     || gBattleMons[B_POSITION_OPPONENT_LEFT].hp != 0
     || IsBreakBarPending(B_POSITION_OPPONENT_LEFT) || GetBreakBarCount(B_POSITION_OPPONENT_LEFT) != 0)
        return FALSE;
    TempAllySetState(TEMP_ALLY_DEPARTING);
    BattleScriptExecute(BattleScript_KoankoZacianDepart);
    return TRUE;
}

// Idempotent data cleanup also used by abort/recorded cleanup. The global
// battle graphics teardown owns sprites on those exits; live departure removes
// its graphics first. Restore Reduction while the partner owner still exists.
void TempAllyFinishBattle(void)
{
    if (TempAllyGetState() == TEMP_ALLY_NONE)
        return;
    // Recorded aborts may release graphics before FreeBattleResources reaches
    // this hook. Never leave the entrance task pointing at freed sprite data.
    u32 taskId = FindTaskIdByFunc(Task_TempAllyZipIn);
    if (taskId != TASK_NONE)
    {
        DestroyTask(taskId);
        gDoingBattleAnim = FALSE;
        if (gBattleSpritesDataPtr != NULL)
            gBattleSpritesDataPtr->healthBoxesData[TEMP_ALLY_BATTLER].ballAnimActive = FALSE;
    }
    ReductionResetOccupant(TEMP_ALLY_BATTLER);
    TempAllyClearStorage();
    for (u32 battler = 2; battler < MAX_BATTLERS_COUNT; battler++)
    {
        memset(&gBattleMons[battler], 0, sizeof(gBattleMons[battler]));
        memset(&gBattleStruct->battlerState[battler], 0, sizeof(gBattleStruct->battlerState[battler]));
        memset(&gProtectStructs[battler], 0, sizeof(gProtectStructs[battler]));
        memset(&gSpecialStatuses[battler], 0, sizeof(gSpecialStatuses[battler]));
        memset(&gBattleStruct->wish[battler], 0, sizeof(gBattleStruct->wish[battler]));
        memset(&gBattleStruct->futureSight[battler], 0, sizeof(gBattleStruct->futureSight[battler]));
        gBattleStruct->gimmick.toActivate &= ~(1u << battler);
        gBattleStruct->gimmick.usableGimmick[battler] = GIMMICK_NONE;
        memset(gBattleStruct->gimmick.activated[battler], 0, sizeof(gBattleStruct->gimmick.activated[battler]));
        gChosenActionByBattler[battler] = gActionsByTurnOrder[battler] = B_ACTION_NOTHING_FAINTED;
        gChosenMoveByBattler[battler] = MOVE_NONE;
        gBattlerByTurnOrder[battler] = gBattlersBySpeed[battler] = battler;
        gBattleStruct->monToSwitchIntoId[battler] = gBattleStruct->AI_monToSwitchIntoId[battler] = PARTY_SIZE;
        gBattleStruct->moveTarget[battler] = B_POSITION_OPPONENT_LEFT;
        gBattleStruct->dynamax.dynamaxTurns[battler] = 0;
        gBattleStruct->dynamax.baseMoves[battler] = MOVE_NONE;
        gBattleStruct->zmove.healReplacement &= ~(1u << battler);
        gBattleStruct->zmove.possibleZMoves[battler] = 0;
        gBattleStruct->zmove.baseMoves[battler] = MOVE_NONE;
        gBattlerBattleController[battler] = BATTLE_CONTROLLER_NONE;
        MarkBattleControllerIdleOnLocal(battler);
        gBattlerControllerFuncs[battler] = NULL;
        if (gAiBattleData != NULL)
        {
            gAiBattleData->aiUsingGimmick &= ~(1u << battler);
            gAiBattleData->chosenMoveIndex[battler] = 0;
            gAiBattleData->chosenTarget[battler] = B_POSITION_OPPONENT_LEFT;
            memset(gAiBattleData->finalScore[battler], 0, sizeof(gAiBattleData->finalScore[battler]));
        }
        if (gBattleHistory != NULL)
        {
            ClearBattlerMoveHistory(battler);
            ClearBattlerAbilityHistory(battler);
            ClearBattlerItemEffectHistory(battler);
            gBattleHistory->heldItems[battler] = ITEM_NONE;
        }
        if (gAiThinkingStruct != NULL)
            gAiThinkingStruct->aiFlags[battler] = 0;
        if (gAiLogicData != NULL)
        {
            gAiLogicData->battlerMovesScored &= ~(1u << battler);
            gAiLogicData->shouldSwitch &= ~(1u << battler);
            gAiLogicData->mostSuitableMonId[battler] = PARTY_SIZE;
            gAiLogicData->monToSwitchInId[battler] = PARTY_SIZE;
        }
    }
    // No left action remains at this barrier. Do not carry a right-side turn
    // index into singles end-turn processing or the next replacement iteration.
    for (u32 battler = 0; battler < 2; battler++)
    {
        gBattlerByTurnOrder[battler] = gBattlersBySpeed[battler] = battler;
        gActionsByTurnOrder[battler] = B_ACTION_FINISHED;
        if (gBattleStruct->moveTarget[battler] >= 2)
            gBattleStruct->moveTarget[battler] = GetOppositeBattler(battler);
    }
    gHitMarker &= ~(HITMARKER_FAINTED(2) | HITMARKER_FAINTED(3));
    gAbsentBattlerFlags |= (1u << 2) | (1u << 3);
    gBattleTypeFlags &= ~BATTLE_TYPE_DOUBLE;
    gBattlersCount = 2;
    if (gAiLogicData != NULL)
        gAiLogicData->reverseBattlerLogicOrder = FALSE;
    gCurrentTurnActionNumber = min(gCurrentTurnActionNumber, 2);
    gBattleStruct->eventState.endTurnBattler = min(gBattleStruct->eventState.endTurnBattler, 2);
    if (gBattlerAttacker >= 2)
        gBattlerAttacker = B_BATTLER_0;
    if (gBattlerTarget >= 2)
        gBattlerTarget = B_BATTLER_1;
    if (gEffectBattler >= 2)
        gEffectBattler = B_BATTLER_0;
    if (gBattleScripting.battler >= 2)
        gBattleScripting.battler = B_BATTLER_0;
    TempAllySetState(TEMP_ALLY_NONE);
}

void BS_TempAllyDepart(void)
{
    NATIVE_ARGS();
    u32 battler = TEMP_ALLY_BATTLER;
    u32 spriteId = gBattlerSpriteIds[battler];
    if (gBattleControllerExecFlags != 0 || IsAnyAbilityPopUpActive())
        return;
    // A living ally leaves ball-less, just as it arrived. A fainted sprite has
    // already been destroyed by its controller, so never reuse its old id.
    if (gBattleMons[battler].hp != 0 && spriteId < MAX_SPRITES && gSprites[spriteId].inUse)
    {
        if (gSprites[spriteId].x < TEMP_ALLY_DASH_END_X)
        {
            // Healthboxes use priority 1. Keep the dash beneath their panels
            // while crossing the right side, without changing any UI sprites.
            gSprites[spriteId].oam.priority = 2;
            gSprites[spriteId].x += TEMP_ALLY_DASH_STEP;
            return;
        }
        FreeSpriteOamMatrix(&gSprites[spriteId]);
        DestroySprite(&gSprites[spriteId]);
    }
    gBattlerSpriteIds[battler] = MAX_SPRITES;
    if (TempAllyGetState() != TEMP_ALLY_NONE)
    {
        for (u32 icon = 0; icon < BATTLE_STATUS_ICON_COUNT; icon++)
            DestroyBattleStatusIcon(battler, icon);
        DestroyBattlerHealthbox(battler);
        FreeSpriteTilesByTag(TAG_HEALTHBOX_PLAYER2_TILE);
        FreeSpriteTilesByTag(TAG_HEALTHBAR_PLAYER2_TILE);
        TempAllyFinishBattle();
        BattleRestoreSinglesHealthboxGfx();
        for (u32 other = 0; other < 2; other++)
            RebuildBattlerHealthbox(other);
    }
    // Ownership/layout cleanup happens once. This script command remains at
    // the replacement barrier while the surviving player slides to singles;
    // a fainted player's replacement will instead spawn centered normally.
    for (u32 other = 0; other < 2; other++)
    {
        spriteId = gBattlerSpriteIds[other];
        if (gBattleMons[other].hp != 0 && IsBattlerSpritePresent(other) && spriteId < MAX_SPRITES && gSprites[spriteId].inUse)
        {
            s16 targetX = GetBattlerSpriteCoord(other, BATTLER_COORD_X_2);
            if (gSprites[spriteId].x != targetX)
            {
                gSprites[spriteId].x = gSprites[spriteId].x < targetX
                    ? min(gSprites[spriteId].x + TEMP_ALLY_RECENTER_STEP, targetX)
                    : max(gSprites[spriteId].x - TEMP_ALLY_RECENTER_STEP, targetX);
                return;
            }
            gSprites[spriteId].y = GetBattlerSpriteDefault_Y(other);
        }
    }
    gBattlescriptCurrInstr = cmd->nextInstr;
}
