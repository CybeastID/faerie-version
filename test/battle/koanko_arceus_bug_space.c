#include "global.h"
#include "test/battle.h"
#include "battle_koanko.h"
#include "battle_entry_scripts.h"
#include "battle_script_native.h"
#include "battle_setup.h"
#include "battle_interface.h"
#include "battle_status_icons.h"
#include "kazuradrop_battle_ui.h"
#include "event_data.h"
#include "sprite.h"
#include "task.h"
#include "constants/battle_script_commands.h"
#include "constants/trainers.h"
#include "constants/flags.h"

static EWRAM_DATA u8 sSavedFlags = 0;
static EWRAM_DATA bool8 sSawSpentExplosionIcon = FALSE;
static const u16 sFixtureFlags[] = {FLAG_KAZURADROP_ENTERED_BATTLE, FLAG_KAZU_LEGENDARY_PUNISH, FLAG_TEMP_1};

static void SaveFixtureFlags(bool32 set)
{
    sSavedFlags = 0;
    for (u32 i = 0; i < ARRAY_COUNT(sFixtureFlags); i++)
    {
        if (FlagGet(sFixtureFlags[i])) sSavedFlags |= 1u << i;
        if (set) FlagSet(sFixtureFlags[i]);
        else FlagClear(sFixtureFlags[i]);
    }
}

static void RestoreFixtureFlags(void)
{
    for (u32 i = 0; i < ARRAY_COUNT(sFixtureFlags); i++)
    {
        if (sSavedFlags & (1u << i)) FlagSet(sFixtureFlags[i]);
        else FlagClear(sFixtureFlags[i]);
    }
}

static void SetFixtureBoosts(void)
{
    gBattleMons[0].statStages[STAT_ATK] = DEFAULT_STAT_STAGE + 6;
    gBattleMons[0].statStages[STAT_DEF] = DEFAULT_STAT_STAGE + 5;
    gBattleMons[0].statStages[STAT_SPATK] = DEFAULT_STAT_STAGE + 1;
    gBattleMons[0].statStages[STAT_SPDEF] = DEFAULT_STAT_STAGE - 1;
    gBattleMons[1].statStages[STAT_ATK] = DEFAULT_STAT_STAGE + 6;
}

static void BS_BugSpaceFixture(void)
{
    NATIVE_ARGS(u8 mode, u16 threshold, u8 tier, bool8 virus);
    gBattleStruct->koankoPhase = KOANKO_CONJURED_TEAM;
    KoankoOnSwitchIn(1);
    EXPECT_EQ(KoankoArceusStartBugSpace(0), FALSE);
    EXPECT(KoankoArceusStartBugSpace(1));
    EXPECT_EQ(KoankoArceusStartBugSpace(1), FALSE);
    EXPECT_EQ(KoankoArceusDismissClock(1), FALSE); // No completed turn yet.
    if (cmd->mode == BUGSPACE_MODE_KOANKO_CRUSH_ONLY)
    {
        // Test mode consumers independently of presentation/scheduling. The
        // protection suite covers actual complete-turn notification timing.
        KoankoArceusOnCommandSelection();
        KoankoArceusOnEndTurn();
        EXPECT(KoankoArceusDismissClock(1));
        EXPECT_EQ(KoankoArceusDismissClock(1), FALSE);
    }
    gBattleStruct->bugSpace.bpThreshold = cmd->threshold;
    gBattleStruct->bugSpace.currentTier = cmd->tier;
    SetFixtureBoosts();
    if (cmd->virus)
    {
        gBattleMons[0].volatiles.meltVirus = TRUE;
        gBattleMons[0].volatiles.meltVirusBy = 1;
    }
    gBattlescriptCurrInstr = cmd->nextInstr;
}

static const struct __attribute__((packed))
{
    u8 native;
    void (*func)(void);
    u8 mode;
    u16 threshold;
    u8 tier;
    bool8 virus;
    u8 ret;
} sBugSpaceFixtures[] =
{
    {B_SCR_OP_CALLNATIVE, BS_BugSpaceFixture, BUGSPACE_MODE_KOANKO_OPENING, 160, BUGSPACE_TIER_NORMAL, FALSE, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_BugSpaceFixture, BUGSPACE_MODE_KOANKO_CRUSH_ONLY, 0, BUGSPACE_TIER_PASSIVE_OHKO, FALSE, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_BugSpaceFixture, BUGSPACE_MODE_KOANKO_CRUSH_ONLY, 254, BUGSPACE_TIER_OHKO, FALSE, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_BugSpaceFixture, BUGSPACE_MODE_KOANKO_CRUSH_ONLY, 90, BUGSPACE_TIER_MINIMIZE, FALSE, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_BugSpaceFixture, BUGSPACE_MODE_KOANKO_OPENING, 80, BUGSPACE_TIER_NORMAL, FALSE, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_BugSpaceFixture, BUGSPACE_MODE_KOANKO_CRUSH_ONLY, 90, BUGSPACE_TIER_MINIMIZE, TRUE, B_SCR_OP_RETURN},
};

static void BugSpaceFixture(u32 fixture, bool32 staleFlags)
{
    SaveFixtureFlags(staleFlags);
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    TurnZeroScript(B_TRAINER_OPPONENT_A, (const u8 *)&sBugSpaceFixtures[fixture]);
}

static u32 CountPanelSprites(bool32 visibleOnly)
{
    u32 count = 0;
    u32 panelTiles = GetSpriteTileStartByTag(TAG_KAZU_PANEL);
    u32 numberTiles = GetSpriteTileStartByTag(TAG_KAZU_NUMBER);
    for (u32 i = 0; i < MAX_SPRITES; i++)
    {
        if (!gSprites[i].inUse)
            continue;
        if ((gSprites[i].oam.tileNum == panelTiles || gSprites[i].oam.tileNum == numberTiles)
         && (!visibleOnly || !gSprites[i].invisible))
            count++;
    }
    return count;
}

SINGLE_BATTLE_TEST("Koanko Arceus Bug Space retains Multitype and leaves Kazuradrop flags and legendary policy alone")
{
    bool32 staleFlags;
    PARAMETRIZE { staleFlags = FALSE; }
    PARAMETRIZE { staleFlags = TRUE; }
    GIVEN {
        BugSpaceFixture(0, staleFlags);
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        PLAYER(SPECIES_XERNEAS) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) { Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry); }
    } WHEN {
        TURN { SWITCH(player, 1); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        NONE_OF {
            MESSAGE("Kazuradrop's parameters are rapidly increasing!");
            ABILITY_POPUP(opponent, ABILITY_BUG_SPACE);
        }
    } THEN {
        EXPECT_EQ(opponent->ability, ABILITY_MULTITYPE);
        EXPECT_EQ(opponent->species, SPECIES_ARCEUS_NORMAL);
        EXPECT_EQ(GetBreakBarCount(1), 0);
        EXPECT_EQ(gBattleStruct->bugSpace.moveSwapActive, 0);
        EXPECT_EQ(gBattleStruct->bugSpace.gutsActive, 0);
        EXPECT_EQ(gBattleStruct->bugSpace.bpThreshold, 150);
        EXPECT_EQ(FlagGet(FLAG_KAZURADROP_ENTERED_BATTLE), staleFlags);
        EXPECT_EQ(FlagGet(FLAG_KAZU_LEGENDARY_PUNISH), staleFlags);
        EXPECT_EQ(FlagGet(FLAG_TEMP_1), staleFlags);
        EXPECT_EQ(opponent->statStages[STAT_ATK], DEFAULT_STAT_STAGE + 6);
        EXPECT_EQ(opponent->statStages[STAT_DEF], DEFAULT_STAT_STAGE);
    } AFTER_CLEANUP {
        RestoreFixtureFlags();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus Bug Space opening decays and crushes boosts without Kazuradrop dialogue or an ability popup")
{
    u32 fixture;
    PARAMETRIZE { fixture = 0; }
    PARAMETRIZE { fixture = 4; }
    GIVEN {
        BugSpaceFixture(fixture, FALSE);
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) { Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        NONE_OF {
            ABILITY_POPUP(opponent, ABILITY_MULTITYPE);
            ABILITY_POPUP(opponent, ABILITY_BUG_SPACE);
        }
        MESSAGE("Wobbuffet's raised stats were lowered!");
    } THEN {
        EXPECT_EQ(player->statStages[STAT_ATK], DEFAULT_STAT_STAGE + 3);
        EXPECT_EQ(player->statStages[STAT_DEF], DEFAULT_STAT_STAGE + 2);
        EXPECT_EQ(player->statStages[STAT_SPATK], DEFAULT_STAT_STAGE);
        EXPECT_EQ(player->statStages[STAT_SPDEF], DEFAULT_STAT_STAGE - 1);
        EXPECT_EQ(opponent->statStages[STAT_ATK], DEFAULT_STAT_STAGE + 6);
        EXPECT_EQ(gBattleStruct->bugSpace.bpThreshold, fixture == 0 ? 150 : 70);
        EXPECT_EQ(FlagGet(FLAG_TEMP_1), FALSE);
        EXPECT(BugSpaceHasDoomClock());
        EXPECT_EQ(BugSpaceIsKazuradrop(), FALSE);
    } AFTER_CLEANUP {
        RestoreFixtureFlags();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus Bug Space opening blocks powerful attacks without Kazuradrop dialogue")
{
    GIVEN {
        BugSpaceFixture(0, FALSE);
        PLAYER(SPECIES_WOBBUFFET) { Speed(2); Moves(MOVE_V_CREATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) { Speed(1); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry); }
    } WHEN {
        TURN { MOVE(player, MOVE_V_CREATE, WITH_RNG(RNG_ACCURACY, TRUE)); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        NONE_OF {
            ABILITY_POPUP(opponent, ABILITY_MULTITYPE);
            ABILITY_POPUP(opponent, ABILITY_BUG_SPACE);
            HP_BAR(opponent);
        }
        MESSAGE("You can't use that move anymore!");
    } THEN {
        EXPECT_GT(player->hp, 0);
        EXPECT_EQ(gBattleStruct->bugSpace.invincibleActive, 0);
        EXPECT_EQ(gBattleResults.playerFaintCounter, 0);
    } AFTER_CLEANUP {
        RestoreFixtureFlags();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus Bug Space crush-only ignores stale thresholds and tiers while continuing once-per-turn crushing")
{
    u32 fixture;
    PARAMETRIZE { fixture = 1; }
    PARAMETRIZE { fixture = 2; }
    PARAMETRIZE { fixture = 3; }
    GIVEN {
        BugSpaceFixture(fixture, FALSE);
        PLAYER(SPECIES_WOBBUFFET) { HP(9999); MaxHP(9999); Attack(1); Moves(MOVE_V_CREATE, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(9999); MaxHP(9999); Defense(9999); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_V_CREATE, WITH_RNG(RNG_ACCURACY, TRUE)); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        HP_BAR(opponent);
        MESSAGE("Wobbuffet's raised stats were lowered!");
        MESSAGE("Wobbuffet's raised stats were lowered!");
    } THEN {
        EXPECT_EQ(player->statStages[STAT_ATK], DEFAULT_STAT_STAGE + 1);
        EXPECT_EQ(player->statStages[STAT_SPATK], DEFAULT_STAT_STAGE);
        EXPECT_EQ(opponent->statStages[STAT_ATK], DEFAULT_STAT_STAGE + 6);
        EXPECT_EQ(gBattleStruct->bugSpace.bpThreshold, sBugSpaceFixtures[fixture].threshold);
        EXPECT_EQ(gBattleStruct->bugSpace.currentTier, sBugSpaceFixtures[fixture].tier);
        EXPECT_EQ(gBattleStruct->bugSpace.active, TRUE);
        EXPECT_EQ(BugSpaceHasDoomClock(), FALSE);
        EXPECT_EQ(gBattleResults.playerFaintCounter, 0);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
        EXPECT_EQ(FlagGet(FLAG_TEMP_1), FALSE);
    } AFTER_CLEANUP {
        RestoreFixtureFlags();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus Bug Space crush-only disables stale crushing-tier instant damage")
{
    GIVEN {
        BugSpaceFixture(2, FALSE);
        PLAYER(SPECIES_WOBBUFFET) { HP(9999); MaxHP(9999); Defense(9999); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            Attack(1); Moves(MOVE_BODY_SLAM); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_BODY_SLAM); }
    } SCENE {
        HP_BAR(player);
        NOT MESSAGE("It's a one-hit KO!");
    } THEN {
        EXPECT_GT(player->hp, 0);
        EXPECT_EQ(gBattleResults.playerFaintCounter, 0);
    } AFTER_CLEANUP {
        RestoreFixtureFlags();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus Bug Space crush-only does not grant Minimize-tier perfect accuracy")
{
    GIVEN {
        BugSpaceFixture(3, FALSE);
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) { Moves(MOVE_DRAGON_RUSH); EntryScript(&gKoankoArceusProtectionEntry); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_DRAGON_RUSH, WITH_RNG(RNG_ACCURACY, FALSE)); }
    } SCENE {
        NOT HP_BAR(player);
        MESSAGE("Wobbuffet avoided the attack!");
    } AFTER_CLEANUP {
        RestoreFixtureFlags();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus Bug Space crush-only leaves Melt Virus at its ordinary one-eighth damage")
{
    GIVEN {
        BugSpaceFixture(5, FALSE);
        PLAYER(SPECIES_WOBBUFFET) { HP(160); MaxHP(160); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(100); MaxHP(100); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(player->hp, 140);
        EXPECT_EQ(opponent->hp, 100);
    } AFTER_CLEANUP {
        RestoreFixtureFlags();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus Bug Space dismissed panel stays hidden through slide-in requests and UI rebuild")
{
    GIVEN {
        BugSpaceFixture(0, FALSE);
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) { Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(CountPanelSprites(FALSE), 2);
        TriggerBugSpaceNumberFlash(1);
        EXPECT(KoankoArceusDismissClock(1));
        for (u32 frame = 0; frame < 40; frame++) AnimateSprites();
        EXPECT_EQ(CountPanelSprites(TRUE), 0); // Flash cannot bring the panel back.
        TriggerBugSpacePanelSlideIn(1);
        CreateBugSpacePanel(1);
        for (u32 frame = 0; frame < 40; frame++) AnimateSprites();
        EXPECT_EQ(CountPanelSprites(TRUE), 0);
        // Simulate the UI cleanup/recreate seam used by menu return. This is
        // not an emulator-level menu/VRAM acceptance test.
        FreeKazuradropBattleUI();
        InitKazuradropBattleUI();
        ReshowKazuradropBattleUI();
        EXPECT_EQ(CountPanelSprites(FALSE), 0);
        EXPECT_EQ(gBattleStruct->bugSpace.active, TRUE);
        EXPECT(KoankoArceusIsProtected(1)); // Dismissal alone does not cover HP.
    } AFTER_CLEANUP {
        RestoreFixtureFlags();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus Bug Space clock dismissal must precede a completed saved-layer handoff")
{
    GIVEN {
        BugSpaceFixture(0, FALSE);
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(37); MaxHP(100); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT(GrantSavedBreakBar(1));
        opponent->hp = opponent->maxHP;
        SetMonData(GetBattlerMon(1), MON_DATA_HP, &opponent->hp);
        EXPECT_EQ(KoankoArceusFinishOpening(1, 0), FALSE);
        EXPECT(KoankoArceusIsProtected(1));
        EXPECT(KoankoArceusDismissClock(1));
        EXPECT(KoankoArceusFinishOpening(1, 0));
        EXPECT_EQ(KoankoArceusIsProtected(1), FALSE);
        EXPECT_EQ(GetBattlerPartyState(1)->breakBarSavedHp[0], 37);
        EXPECT_EQ(opponent->ability, ABILITY_MULTITYPE);
        EXPECT_EQ(gBattleStruct->bugSpace.mode, BUGSPACE_MODE_KOANKO_CRUSH_ONLY);
    } AFTER_CLEANUP {
        RestoreFixtureFlags();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus Bug Space owner retirement clears only its own field and panel")
{
    bool32 foreign;
    PARAMETRIZE { foreign = FALSE; }
    PARAMETRIZE { foreign = TRUE; }
    GIVEN {
        BugSpaceFixture(0, FALSE);
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) { Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        if (foreign) gBattleStruct->bugSpace.mode = BUGSPACE_MODE_KAZURADROP;
        EXPECT_EQ(KoankoArceusStartBugSpace(1), FALSE); // Never replace an active field.
        u32 oldSlot = gBattlerPartyIndexes[1];
        gBattlerPartyIndexes[1] = oldSlot + 1;
        KoankoArceusOnSwitchIn(1);
        gBattlerPartyIndexes[1] = oldSlot;
        EXPECT_EQ(gBattleStruct->koankoArceus.phase, KOANKO_ARCEUS_FINISHED);
        EXPECT_EQ(gBattleStruct->bugSpace.active, foreign);
        EXPECT_EQ(CountPanelSprites(FALSE), foreign ? 2 : 0);
        KoankoArceusOnSwitchIn(1); // Repeated cleanup remains harmless.
        EXPECT_EQ(gBattleStruct->bugSpace.active, foreign);
    } AFTER_CLEANUP {
        RestoreFixtureFlags();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus Bug Space preserves ordinary player loss without Kazuradrop game-over flags")
{
    GIVEN {
        BugSpaceFixture(1, FALSE);
        PLAYER(SPECIES_WOBBUFFET) { HP(1); Speed(1); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            Speed(2); Moves(MOVE_SEISMIC_TOSS); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(opponent, MOVE_SEISMIC_TOSS); MOVE(player, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(gBattleOutcome, B_OUTCOME_LOST);
        EXPECT_EQ(gBattleResults.playerFaintCounter, 1);
        EXPECT_EQ(FlagGet(FLAG_KAZURADROP_ENTERED_BATTLE), FALSE);
        EXPECT_EQ(FlagGet(FLAG_KAZU_LEGENDARY_PUNISH), FALSE);
    } AFTER_CLEANUP {
        RestoreFixtureFlags();
    }
}

static void BS_KazuraCrushingFixture(void)
{
    NATIVE_ARGS();
    SetFixtureBoosts();
    gBattlescriptCurrInstr = cmd->nextInstr;
}

static const struct __attribute__((packed))
{
    u8 native;
    void (*func)(void);
    u8 ret;
} sKazuraCrushingFixture = {B_SCR_OP_CALLNATIVE, BS_KazuraCrushingFixture, B_SCR_OP_RETURN};

static void ObserveBlockedExplosion(u8 taskId)
{
    if (gBattleStruct == NULL || gBattleOutcome != 0)
    {
        DestroyTask(taskId);
        return;
    }
    u32 icon = Test_GetBattleStatusIconSpriteId(1, BATTLE_STATUS_ICON_INVINCIBLE);
    if (gBattleMons[0].hp == 0 && gBattleMons[1].hp != 0
     && icon < MAX_SPRITES && gSprites[icon].inUse)
        sSawSpentExplosionIcon = TRUE;
}

static void BS_WatchBlockedExplosion(void)
{
    NATIVE_ARGS();
    sSawSpentExplosionIcon = FALSE;
    u32 taskId = CreateTask(ObserveBlockedExplosion, 0);
    EXPECT_LT(taskId, NUM_TASKS);
    EXPECT(gTasks[taskId].isActive && gTasks[taskId].func == ObserveBlockedExplosion);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

static const struct __attribute__((packed))
{
    u8 native;
    void (*func)(void);
    u8 ret;
} sBlockedExplosionFixture = {B_SCR_OP_CALLNATIVE, BS_WatchBlockedExplosion, B_SCR_OP_RETURN};

SINGLE_BATTLE_TEST("Kazuradrop Bug Space intentionally lets Explosion self-KO before flashing Invincible and taunting")
{
    GIVEN {
        SaveFixtureFlags(FALSE);
        TurnZeroScript(B_TRAINER_OPPONENT_A, (const u8 *)&sBlockedExplosionFixture);
        PLAYER(SPECIES_WOBBUFFET) { Speed(2); Moves(MOVE_EXPLOSION); }
        PLAYER(SPECIES_ABRA) { Speed(2); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_KAZURADROP) {
            HP(100); MaxHP(100); Speed(1); Ability(ABILITY_BUG_SPACE); Moves(MOVE_CELEBRATE);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_EXPLOSION); MOVE(opponent, MOVE_CELEBRATE); SEND_OUT(player, 1); }
    } SCENE {
        NOT HP_BAR(opponent);
        MESSAGE("You can't use that move anymore!");
        MESSAGE("“Too powerful for my world? Don't worry. You'll shrink to fit soon enough.”");
        MESSAGE("Wobbuffet fainted!");
    } THEN {
        EXPECT(sSawSpentExplosionIcon);
        EXPECT_EQ(opponent->hp, 100);
        EXPECT_EQ(gBattleResults.playerFaintCounter, 1);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
        EXPECT_EQ(player->species, SPECIES_ABRA);
    } AFTER_CLEANUP {
        u32 taskId = FindTaskIdByFunc(ObserveBlockedExplosion);
        if (taskId != TASK_NONE) DestroyTask(taskId);
        RestoreFixtureFlags();
    }
}

SINGLE_BATTLE_TEST("Kazuradrop Bug Space crushing retains its first-use flag, popup and per-turn rounding")
{
    GIVEN {
        SaveFixtureFlags(FALSE);
        TurnZeroScript(B_TRAINER_OPPONENT_A, (const u8 *)&sKazuraCrushingFixture);
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_KAZURADROP) { Ability(ABILITY_BUG_SPACE); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        ABILITY_POPUP(opponent, ABILITY_BUG_SPACE);
        MESSAGE("Wobbuffet's raised stats were lowered!");
        MESSAGE("Wobbuffet's raised stats were lowered!");
    } THEN {
        EXPECT(BugSpaceIsKazuradrop());
        EXPECT(BugSpaceHasDoomClock());
        EXPECT_EQ(gBattleStruct->bugSpace.bpThreshold, 140);
        EXPECT_EQ(player->statStages[STAT_ATK], DEFAULT_STAT_STAGE + 1);
        EXPECT_EQ(player->statStages[STAT_DEF], DEFAULT_STAT_STAGE + 1);
        EXPECT_EQ(player->statStages[STAT_SPATK], DEFAULT_STAT_STAGE);
        EXPECT_EQ(player->statStages[STAT_SPDEF], DEFAULT_STAT_STAGE - 1);
        EXPECT_EQ(opponent->statStages[STAT_ATK], DEFAULT_STAT_STAGE + 6);
        EXPECT(FlagGet(FLAG_TEMP_1));
    } AFTER_CLEANUP {
        RestoreFixtureFlags();
    }
}
