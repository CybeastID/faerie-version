#include "global.h"
#include "test/battle.h"
#include "battle_controllers.h"
#include "battle_entry_scripts.h"
#include "battle_koanko.h"
#include "battle_main.h"
#include "battle_script_native.h"
#include "battle_setup.h"
#include "battle_util2.h"
#include "data.h"
#include "main.h"
#include "reshow_battle_screen.h"
#include "sprite.h"
#include "task.h"
#include "constants/battle_script_commands.h"

static EWRAM_DATA struct
{
    MainCallback savedCallback;
    u8 outgoingSlot;
    bool8 reshown;
} sMenuReturn = {0};

static u32 CountOpponentMonSprites(void)
{
    u32 count = 0;
    const struct SpriteFrameImage *images = gMonSpritesGfxPtr->templates[GetBattlerPosition(1)].images;
    for (u32 i = 0; i < MAX_SPRITES; i++)
        if (gSprites[i].inUse && gSprites[i].images == images && gSprites[i].anims != gAnims_Trainer)
            count++;
    return count;
}

static void WaitForConjuredMenuReturn(void)
{
    if (gMain.callback2 != BattleMainCB2)
        return;
    gMain.callback1 = sMenuReturn.savedCallback;
    sMenuReturn.savedCallback = NULL;
    sMenuReturn.reshown = TRUE;
    // The party has been regenerated, but neither active battler has been
    // replaced yet. A live future party entry is not an on-field opponent.
    EXPECT_EQ(gBattleStruct->koankoPhase, KOANKO_CONJURED_TEAM);
    EXPECT_EQ(gBattlerPartyIndexes[1], sMenuReturn.outgoingSlot);
    EXPECT_EQ(gBattleMons[1].hp, 0);
    EXPECT_EQ(gBattleMons[1].species, SPECIES_WOBBUFFET);
    EXPECT_EQ(GetMonData(GetBattlerMon(1), MON_DATA_SPECIES),
        sMenuReturn.outgoingSlot == 5 ? SPECIES_ARCEUS_NORMAL : SPECIES_SMEARGLE);
    EXPECT(gBattleStruct->battlerState[1].notOnField);
    EXPECT_EQ(CountOpponentMonSprites(), 0);
    EXPECT_LT(gHealthboxSpriteIds[1], MAX_SPRITES);
    EXPECT(gSprites[gHealthboxSpriteIds[1]].invisible);
}

static void Task_ReshowBeforeConjuredSendout(u8 taskId)
{
    if (gBattleStruct == NULL)
    {
        DestroyTask(taskId);
        return;
    }
    if (gBattleStruct->koankoPhase != KOANKO_CONJURED_TEAM || gBattleMons[1].hp != 0)
        return;
    EXPECT_EQ(gBattlerPartyIndexes[1], sMenuReturn.outgoingSlot);
    // Recorded tests bypass the party UI. Exercise its complete close/reshow
    // path at the real simultaneous-replacement boundary, then resume the
    // unchanged controllers and ordinary Smeargle send-out.
    sMenuReturn.savedCallback = gMain.callback1;
    gMain.callback1 = WaitForConjuredMenuReturn;
    CloseMainBattleScreen();
    ReshowBattleScreenAfterMenu();
    DestroyTask(taskId);
}

static void BS_WatchConjuredMenuReturn(void)
{
    NATIVE_ARGS();
    EXPECT_EQ(gBattlerPartyIndexes[1], sMenuReturn.outgoingSlot);
    u32 task = CreateTask(Task_ReshowBeforeConjuredSendout, 254);
    EXPECT_LT(task, NUM_TASKS);
    EXPECT(gTasks[task].func == Task_ReshowBeforeConjuredSendout);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

static const struct __attribute__((packed))
{
    u8 native;
    void (*func)(void);
    u8 ret;
} sWatchConjuredMenuReturn = {B_SCR_OP_CALLNATIVE, BS_WatchConjuredMenuReturn, B_SCR_OP_RETURN};

SINGLE_BATTLE_TEST("Koanko replacement menu return does not render a future opponent before Smeargle sends out")
{
    u32 outgoingSlot;
    PARAMETRIZE { outgoingSlot = 0; }
    PARAMETRIZE { outgoingSlot = 5; }
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    GIVEN {
        memset(&sMenuReturn, 0, sizeof(sMenuReturn));
        sMenuReturn.outgoingSlot = outgoingSlot;
        TurnZeroScript(B_TRAINER_OPPONENT_A, (const u8 *)&sWatchConjuredMenuReturn);
        PLAYER(SPECIES_VOLTORB) { Speed(10); Attack(9999); Moves(MOVE_EXPLOSION); }
        PLAYER(SPECIES_WOBBUFFET) { Speed(5); Moves(MOVE_CELEBRATE); }
        for (u32 slot = 0; slot < PARTY_SIZE; slot++)
            OPPONENT(SPECIES_WOBBUFFET) { Speed(1); HP(slot == outgoingSlot ? 1 : 0); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_EXPLOSION); SEND_OUT(player, 1); SEND_OUT(opponent, 0); }
    } SCENE {
        MESSAGE("You thought that was all?\nThen watch what I can create!{PAUSE_UNTIL_PRESS}");
    } THEN {
        EXPECT(sMenuReturn.reshown);
        EXPECT_EQ(gBattlerPartyIndexes[1], 0);
        EXPECT_EQ(opponent->species, SPECIES_SMEARGLE);
        EXPECT_EQ(GetMonData(GetBattlerMon(1), MON_DATA_SPECIES), SPECIES_SMEARGLE);
        EXPECT_EQ(player->species, SPECIES_WOBBUFFET);
        EXPECT_EQ(CountOpponentMonSprites(), 1);
        EXPECT_LT(gBattlerSpriteIds[1], MAX_SPRITES);
        EXPECT(!gSprites[gBattlerSpriteIds[1]].invisible);
        EXPECT(!gSprites[gHealthboxSpriteIds[1]].invisible);
        EXPECT(!gBattleStruct->battlerState[1].notOnField);
    } AFTER_CLEANUP {
        u32 task = FindTaskIdByFunc(Task_ReshowBeforeConjuredSendout);
        if (task != TASK_NONE) DestroyTask(task);
        if (sMenuReturn.savedCallback != NULL)
        {
            gMain.callback1 = sMenuReturn.savedCallback;
            sMenuReturn.savedCallback = NULL;
        }
    }
}
