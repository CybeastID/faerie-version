#include "global.h"
#include "test/battle.h"
#include "battle_koanko.h"
#include "battle_entry_scripts.h"
#include "battle_interface.h"
#include "battle_script_native.h"
#include "battle_setup.h"
#include "kazuradrop_battle_ui.h"
#include "event_data.h"
#include "decompress.h"
#include "malloc.h"
#include "string_util.h"
#include "task.h"
#include "util.h"
#include "constants/battle_script_commands.h"
#include "constants/trainers.h"

extern void BS_WaitAbilityPopup(void);

static EWRAM_DATA struct
{
    u16 hpBeforeOvercharge;
    u16 monGfxCrc;
    u16 entryGfxCrc;
    u8 overchargeTurn;
    u8 idle;
    u8 oldFlags;
    bool8 idleInstalled;
    bool8 allMultitype;
    bool8 sawSlide;
    bool8 sawHiddenBeforeHeal;
    bool8 checkedEntryGfx;
} sStage = {0};
static const u16 sFlags[] = {FLAG_KAZURADROP_ENTERED_BATTLE, FLAG_KAZU_LEGENDARY_PUNISH, FLAG_TEMP_1};

static void ObserveStaging(u8 taskId)
{
    if (gBattleStruct == NULL || gBattleOutcome != 0)
    {
        DestroyTask(taskId);
        return;
    }
    if (!KoankoArceusOwnsBattler(1))
        return;
    if (!sStage.idleInstalled)
    {
        sStage.idleInstalled = TRUE;
        if (sStage.idle == 1) gBattleMons[1].volatiles.rechargeTimer = 2;
        if (sStage.idle == 2) gBattleMons[1].volatiles.timestop = 3;
    }
    if (gBattleMons[1].ability != ABILITY_MULTITYPE)
        sStage.allMultitype = FALSE;
    if (!sStage.checkedEntryGfx && !gBattleStruct->entryScriptRunning
     && gBattleStruct->bugSpace.active && gBattleStruct->bugSpace.mode == BUGSPACE_MODE_KOANKO_OPENING)
    {
        sStage.checkedEntryGfx = TRUE;
        sStage.entryGfxCrc = CalcCRC16(gMonSpritesGfxPtr->spritesGfx[GetBattlerPosition(1)], MON_PIC_SIZE);
    }
    if (gBattleStruct->koankoArceus.phase == KOANKO_ARCEUS_OVERCHARGING)
    {
        if (sStage.overchargeTurn == 0xFF)
        {
            sStage.overchargeTurn = gBattleResults.battleTurnCounter;
            sStage.hpBeforeOvercharge = gBattleMons[1].hp;
        }
        if (gBattleStruct->bugSpace.mode == BUGSPACE_MODE_KOANKO_CRUSH_ONLY)
        {
            if (IsBugSpacePanelSliding(1)) sStage.sawSlide = TRUE;
            else if (gBattleMons[1].hp < gBattleMons[1].maxHP) sStage.sawHiddenBeforeHeal = TRUE;
        }
    }
}

static void BS_StageFixture(void)
{
    NATIVE_ARGS(u8 idle);
    gBattleStruct->koankoPhase = KOANKO_CONJURED_TEAM;
    sStage.idle = cmd->idle;
    sStage.idleInstalled = FALSE;
    sStage.overchargeTurn = 0xFF;
    sStage.hpBeforeOvercharge = 0;
    sStage.allMultitype = TRUE;
    sStage.sawSlide = FALSE;
    sStage.sawHiddenBeforeHeal = FALSE;
    sStage.checkedEntryGfx = FALSE;
    sStage.entryGfxCrc = 0;
    // Protection arms during switch-in DATA loading, before incoming graphics
    // replace the previous occupant. Build an independent ROM-art reference
    // in a short-lived EWRAM allocation instead of sampling that early buffer.
    u8 *reference = Alloc(MON_PIC_SIZE * MAX_MON_PIC_FRAMES);
    EXPECT(reference != NULL);
    DecompressDataWithHeaderWram(gSpeciesInfo[SPECIES_ARCEUS_NORMAL].frontPic, reference);
    sStage.monGfxCrc = CalcCRC16(reference, MON_PIC_SIZE);
    Free(reference);
    if (cmd->idle == 3)
    {
        // A field owned by the player must not be stolen or strand Arceus's
        // opening protection when its own field initialization is declined.
        gBattleStruct->bugSpace.active = TRUE;
        gBattleStruct->bugSpace.mode = BUGSPACE_MODE_KAZURADROP;
        gBattleStruct->bugSpace.sourceBattler = 0;
        gBattleStruct->bugSpace.bpThreshold = 200;
        gBattleStruct->bugSpace.bpDecayRate = 0;
        CreateBugSpacePanel(0);
    }
    SetMonData(&gParties[B_TRAINER_OPPONENT_A][1], MON_DATA_NICKNAME, COMPOUND_STRING("Astra"));
    u32 taskId = CreateTask(ObserveStaging, 0);
    EXPECT_LT(taskId, NUM_TASKS);
    EXPECT(gTasks[taskId].isActive && gTasks[taskId].func == ObserveStaging);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

static const struct __attribute__((packed))
{
    u8 native;
    void (*func)(void);
    u8 idle;
    u8 ret;
} sStageOpenings[] =
{
    {B_SCR_OP_CALLNATIVE, BS_StageFixture, 0, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_StageFixture, 1, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_StageFixture, 2, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_StageFixture, 3, B_SCR_OP_RETURN},
};

static void StageFixture(u32 idle)
{
    sStage.oldFlags = 0;
    for (u32 i = 0; i < ARRAY_COUNT(sFlags); i++)
    {
        if (FlagGet(sFlags[i])) sStage.oldFlags |= 1u << i;
        FlagClear(sFlags[i]);
    }
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    TurnZeroScript(B_TRAINER_OPPONENT_A, (const u8 *)&sStageOpenings[idle]);
}

static void FinishStageFixture(void)
{
    u32 taskId = FindTaskIdByFunc(ObserveStaging);
    if (taskId != TASK_NONE) DestroyTask(taskId);
    for (u32 i = 0; i < ARRAY_COUNT(sFlags); i++)
    {
        if (sStage.oldFlags & (1u << i)) FlagSet(sFlags[i]);
        else FlagClear(sFlags[i]);
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus staged opening dismisses the clock, invokes one seal, fully heals and covers the healed HP")
{
    bool32 charm, breakLayer;
    PARAMETRIZE { charm = FALSE; breakLayer = FALSE; }
    PARAMETRIZE { charm = TRUE; breakLayer = FALSE; }
    PARAMETRIZE { charm = FALSE; breakLayer = TRUE; }
    PARAMETRIZE { charm = TRUE; breakLayer = TRUE; }
    GIVEN {
        // Exercise the actual trainer/Seismic Toss/form graphics lifetimes,
        // rather than allowing the headless runner to skip move animations.
        FORCE_MOVE_ANIM(TRUE);
        StageFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(2); Moves(MOVE_SPIKES, MOVE_SEISMIC_TOSS, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(1); HP(1); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            Speed(1); HP(37); MaxHP(100); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusEntry);
            if (charm) Item(ITEM_BREAK_CHARM);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_SPIKES); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); SEND_OUT(opponent, 1); }
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); }
        if (breakLayer) TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        MESSAGE("You remember this one, right?");
        ABILITY_POPUP(opponent, ABILITY_BUG_SPACE, ownerName: COMPOUND_STRING("Alter Ego's"));
        HP_BAR(opponent, hp: 1);
        MESSAGE("Oh, right. That silly thing? We won't be needing that. I'll keep the buff crushing though.");
        MESSAGE("There! Much better! Now then, let's make this trade fair...");
        MESSAGE("By my Command Seal! Arceus, overcharge! Release your shackles!");
        HP_BAR(opponent, hp: 100);
        MESSAGE("Behold, your final trial!");
        if (breakLayer) HP_BAR(opponent, hp: 0);
        NOT MESSAGE("By my Command Seal! Arceus, overcharge! Release your shackles!");
    } THEN {
        EXPECT_EQ(sStage.overchargeTurn, 2); // Arrival remainder, then the first full selected turn.
        EXPECT_EQ(sStage.hpBeforeOvercharge, 1);
        EXPECT_EQ(sStage.allMultitype, TRUE);
        EXPECT_EQ(sStage.checkedEntryGfx, TRUE);
        EXPECT_EQ(sStage.entryGfxCrc, sStage.monGfxCrc);
        EXPECT_EQ(CalcCRC16(gMonSpritesGfxPtr->spritesGfx[GetBattlerPosition(1)], MON_PIC_SIZE), sStage.monGfxCrc);
        EXPECT_EQ(sStage.sawSlide, TRUE);
        EXPECT_EQ(sStage.sawHiddenBeforeHeal, TRUE);
        EXPECT_EQ(gBattleStruct->koankoArceus.phase, breakLayer ? KOANKO_ARCEUS_ADAPTED : KOANKO_ARCEUS_COVERED);
        if (breakLayer) EXPECT_EQ(opponent->species, SPECIES_ARCEUS_GHOST);
        EXPECT_EQ(KoankoArceusIsProtected(1), FALSE);
        EXPECT_EQ(KoankoArceusTryOvercharge(), FALSE);
        EXPECT_EQ(opponent->hp, 100);
        EXPECT_EQ(GetMonData(GetBattlerMon(1), MON_DATA_HP), 100);
        EXPECT_EQ(GetBreakBarCount(1), (charm ? 1 : 0) + (breakLayer ? 0 : 1));
        EXPECT_EQ(gBattleStruct->koankoArceus.savedLayer, charm ? 1 : 0);
        if (!breakLayer) EXPECT_EQ(GetBattlerPartyState(1)->breakBarSavedHp[charm ? 1 : 0], 100);
        // if (charm) EXPECT_EQ(GetBattlerPartyState(1)->breakBarSavedHp[0], 25); // Pre-existing covered HP stays intact.
        EXPECT_EQ(GetBattlerPartyState(1)->entryScriptRuns, 1); // End-turn event is not another entry.
        EXPECT_EQ(gBattleStruct->bugSpace.mode, BUGSPACE_MODE_KOANKO_CRUSH_ONLY);
        EXPECT_EQ(BugSpaceHasDoomClock(), FALSE);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 1); // Only the lead Wobbuffet.
        GetMonData(GetBattlerMon(1), MON_DATA_NICKNAME, gStringVar1);
        EXPECT_EQ(StringCompare(gStringVar1, COMPOUND_STRING("Astra")), 0);
    } AFTER_CLEANUP {
        FORCE_MOVE_ANIM(FALSE);
        FinishStageFixture();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus staged opening counts idle, recharge and Time Stopped turns before overcharging exactly once")
{
    u32 idle;
    PARAMETRIZE { idle = 0; }
    PARAMETRIZE { idle = 1; }
    PARAMETRIZE { idle = 2; }
    GIVEN {
        StageFixture(idle);
        PLAYER(SPECIES_WOBBUFFET) { Speed(2); Moves(MOVE_SEISMIC_TOSS, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(1); HP(1); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            Speed(1); HP(37); MaxHP(100); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); SEND_OUT(opponent, 1); }
        TURN {
            MOVE(player, MOVE_CELEBRATE);
            if (idle == 0) MOVE(opponent, MOVE_CELEBRATE);
            else SKIP_TURN(opponent);
        }
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        MESSAGE("You remember this one, right?");
        ABILITY_POPUP(opponent, ABILITY_BUG_SPACE, ownerName: COMPOUND_STRING("Alter Ego's"));
        MESSAGE("By my Command Seal! Arceus, overcharge! Release your shackles!");
        HP_BAR(opponent, hp: 100);
        MESSAGE("Behold, your final trial!");
        NOT MESSAGE("By my Command Seal! Arceus, overcharge! Release your shackles!");
    } THEN {
        EXPECT_EQ(sStage.overchargeTurn, 1);
        EXPECT_EQ(sStage.hpBeforeOvercharge, 37);
        EXPECT_EQ(sStage.allMultitype, TRUE);
        EXPECT_EQ(sStage.checkedEntryGfx, TRUE);
        EXPECT_EQ(sStage.entryGfxCrc, sStage.monGfxCrc);
        EXPECT_EQ(CalcCRC16(gMonSpritesGfxPtr->spritesGfx[GetBattlerPosition(1)], MON_PIC_SIZE), sStage.monGfxCrc);
        EXPECT_EQ(gBattleStruct->koankoArceus.phase, KOANKO_ARCEUS_COVERED);
        EXPECT_EQ(GetBreakBarCount(1), 1);
        EXPECT_EQ(GetBattlerPartyState(1)->breakBarSavedHp[0], 100);
        EXPECT_EQ(GetBattlerPartyState(1)->entryScriptRuns, 1);
    } AFTER_CLEANUP {
        FinishStageFixture();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus staged foreign Bug Space cannot strand opening protection or lose its owner")
{
    GIVEN {
        StageFixture(3);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(2); Moves(MOVE_SEISMIC_TOSS, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(1); Speed(1); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(37); MaxHP(100); Speed(1); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); SEND_OUT(opponent, 1); }
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        ABILITY_POPUP(opponent, ABILITY_BUG_SPACE, ownerName: COMPOUND_STRING("Alter Ego's"));
        MESSAGE("By my Command Seal! Arceus, overcharge! Release your shackles!");
        HP_BAR(opponent, hp: 100);
        MESSAGE("Behold, your final trial!");
    } THEN {
        EXPECT_EQ(gBattleStruct->koankoArceus.phase, KOANKO_ARCEUS_COVERED);
        EXPECT_EQ(KoankoArceusIsProtected(1), FALSE);
        EXPECT_EQ(GetBreakBarCount(1), 1);
        EXPECT_EQ(gBattleStruct->bugSpace.active, TRUE);
        EXPECT_EQ(gBattleStruct->bugSpace.mode, BUGSPACE_MODE_KAZURADROP);
        EXPECT_EQ(gBattleStruct->bugSpace.sourceBattler, 0);
        EXPECT_EQ(gBattleStruct->bugSpace.bpThreshold, 200);
        EXPECT_EQ(BugSpaceHasDoomClock(), TRUE);
        KoankoArceusFinishBattle();
        EXPECT_EQ(gBattleStruct->bugSpace.active, TRUE);
        EXPECT_EQ(gBattleStruct->bugSpace.sourceBattler, 0);
    } AFTER_CLEANUP {
        FinishStageFixture();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus staged opening cannot overcharge after genuine player loss")
{
    GIVEN {
        StageFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { HP(1); Speed(2); Moves(MOVE_SEISMIC_TOSS, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(1); Speed(1); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(37); MaxHP(100); Speed(3); Moves(MOVE_SEISMIC_TOSS); EntryScript(&gKoankoArceusEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); SEND_OUT(opponent, 1); }
        TURN { MOVE(opponent, MOVE_SEISMIC_TOSS); MOVE(player, MOVE_CELEBRATE); }
    } SCENE {
        ABILITY_POPUP(opponent, ABILITY_BUG_SPACE, ownerName: COMPOUND_STRING("Alter Ego's"));
        NOT MESSAGE("By my Command Seal! Arceus, overcharge! Release your shackles!");
    } THEN {
        EXPECT_EQ(gBattleOutcome, B_OUTCOME_LOST);
        EXPECT_EQ(sStage.overchargeTurn, 0xFF);
        EXPECT_EQ(GetBreakBarCount(1), 0);
        EXPECT_EQ(FlagGet(FLAG_KAZURADROP_ENTERED_BATTLE), FALSE);
        EXPECT_EQ(KoankoArceusTryOvercharge(), FALSE);
    } AFTER_CLEANUP {
        FinishStageFixture();
    }
}

static void BS_NamedPopupFixture(void)
{
    NATIVE_ARGS();
    SetMonData(GetBattlerMon(1), MON_DATA_NICKNAME, COMPOUND_STRING("Oracle"));
    gBattleScripting.abilityPopupOverwrite = ABILITY_PRESSURE;
    CreateAbilityPopUpWithName(1, ABILITY_BUG_SPACE, FALSE, COMPOUND_STRING("Alter Ego's"));
    EXPECT_EQ(gBattleMons[1].ability, ABILITY_MULTITYPE);
    EXPECT_EQ(gBattleScripting.abilityPopupOverwrite, ABILITY_PRESSURE);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

static void BS_OrdinaryPopupFixture(void)
{
    NATIVE_ARGS();
    CreateAbilityPopUp(1, ABILITY_MULTITYPE, FALSE);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

static const struct __attribute__((packed))
{
    u8 named;
    void (*namedFunc)(void);
    u8 waitNamed;
    void (*waitNamedFunc)(void);
    u8 ordinary;
    void (*ordinaryFunc)(void);
    u8 waitOrdinary;
    void (*waitOrdinaryFunc)(void);
    u8 ret;
} sNamedPopupFixture =
{
    B_SCR_OP_CALLNATIVE, BS_NamedPopupFixture,
    B_SCR_OP_CALLNATIVE, BS_WaitAbilityPopup,
    B_SCR_OP_CALLNATIVE, BS_OrdinaryPopupFixture,
    B_SCR_OP_CALLNATIVE, BS_WaitAbilityPopup,
    B_SCR_OP_RETURN,
};

SINGLE_BATTLE_TEST("Koanko Arceus staged popup label is exact and leaves ordinary nickname and popup override behavior intact")
{
    GIVEN {
        TurnZeroScript(B_TRAINER_OPPONENT_A, (const u8 *)&sNamedPopupFixture);
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) { Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        ABILITY_POPUP(opponent, ABILITY_BUG_SPACE, ownerName: COMPOUND_STRING("Alter Ego's"));
        ABILITY_POPUP(opponent, ABILITY_PRESSURE, ownerName: COMPOUND_STRING("Oracle's"));
    } THEN {
        EXPECT_EQ(opponent->ability, ABILITY_MULTITYPE);
        EXPECT_EQ(gBattleScripting.abilityPopupOverwrite, ABILITY_NONE);
    }
}
