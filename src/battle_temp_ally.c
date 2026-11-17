#include "global.h"
#include "battle_script_native.h"
#include "battle.h"
#include "battle_ai_main.h"
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
#include "constants/trainers.h"

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
     || gBattleStruct->txRollback.combatProfile != TX_PROFILE_NONE
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
        // The ally scores like Koanko's side: same AI flags as the opposing trainer's lead battler.
        gAiThinkingStruct->aiFlags[battler] = gAiThinkingStruct->aiFlags[B_POSITION_OPPONENT_LEFT];
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
    // Existing battlers move to their doubles coordinates (their healthbox frames were re-loaded above).
    for (u32 other = 0; other < B_POSITION_PLAYER_RIGHT; other++)
    {
        if (IsBattlerSpritePresent(other))
        {
            gSprites[gBattlerSpriteIds[other]].x = GetBattlerSpriteCoord(other, BATTLER_COORD_X_2);
            gSprites[gBattlerSpriteIds[other]].y = GetBattlerSpriteDefault_Y(other);
        }
        InitBattlerHealthboxCoords(other);
        UpdateHealthboxAttribute(gHealthboxSpriteIds[other], GetBattlerMon(other), HEALTHBOX_ALL);
    }
    gBattlescriptCurrInstr = cmd->nextInstr;
}

void BS_TempAllyFinishEntry(void)
{
    NATIVE_ARGS();
    // The ally joins ordinary AI next turn: it was never in the already-chosen action queue.
    gBattleStruct->battlerState[B_POSITION_PLAYER_RIGHT].noOrderChange = TRUE;
    TempAllySetState(TEMP_ALLY_ACTIVE);
    gBattlescriptCurrInstr = cmd->nextInstr;
}
