#include "global.h"
#include "test/battle.h"
#include "battle_break_bar.h"
#include "battle_ai_main.h"
#include "battle_controllers.h"
#include "battle_interface.h"
#include "battle_script_native.h"
#include "battle_scripts.h"
#include "event_data.h"
#include "graphics.h"
#include "palette.h"
#include "party_menu.h"
#include "script.h"
#include "sprite.h"
#include "task.h"
#include "trainer_break_bars.h"
#include "constants/rgb.h"

static const struct BreakBarConfig sIntroBars[] =
{
    {.extraBars = 1, .flags = BREAK_BAR_INTRO_FILLS},
    {.extraBars = 2, .flags = BREAK_BAR_INTRO_FILLS},
    {.extraBars = 3, .flags = BREAK_BAR_INTRO_FILLS},
    {.extraBars = 4, .flags = BREAK_BAR_INTRO_FILLS},
    {.extraBars = 5, .flags = BREAK_BAR_INTRO_FILLS},
};

static const u16 sIntroColors[][2] =
{
    {0, 0},
    {RGB(22, 5, 26), RGB(11, 3, 20)},
    {RGB(5, 17, 27), RGB(3, 9, 27)},
    {RGB(4, 25, 22), RGB(1, 13, 11)},
    {RGB(27, 23, 4), RGB(17, 15, 2)},
    {RGB(25, 14, 17), RGB(27, 12, 11)},
};

SINGLE_BATTLE_TEST("Break Bar party menu colors follow original party owners only in standard battles")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { BreakBars(&gTrainerBreakBarProfiles[4]); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        struct PartyMenu savedMenu = gPartyMenu;
        u8 savedOrder[sizeof(gBattlePartyCurrentOrder)];
        u32 savedFlags = gBattleTypeFlags;
        bool8 savedInBattle = gMain.inBattle;
        memcpy(savedOrder, gBattlePartyCurrentOrder, sizeof(savedOrder));
        gPartyMenu.menuType = PARTY_MENU_TYPE_IN_BATTLE;
        gMain.inBattle = TRUE;
        gBattlePartyCurrentOrder[0] = 0x20;
        gBattlePartyCurrentOrder[1] = 0x13;
        gBattlePartyCurrentOrder[2] = 0x45;
        struct PartyState *owner = &gBattleStruct->partyState[B_TRAINER_PLAYER][2];
        *owner = *GetBattlerPartyState(0);
        for (u32 doubles = 0; doubles < 2; doubles++)
        {
            gBattleTypeFlags = doubles ? BATTLE_TYPE_DOUBLE : 0;
            for (u32 bars = 0; bars <= BREAK_BAR_MAX_EXTRA_BARS; bars++)
            {
                owner->breakBarsRemaining = bars;
                const u16 *colors = Test_GetPartyMenuBreakBarColors(&gParties[B_TRAINER_PLAYER][0]);
                EXPECT(colors != NULL);
                EXPECT_EQ(colors[0], bars ? sIntroColors[bars][0] : gBattleInterface_BallDisplayPal[14]);
                EXPECT_EQ(colors[1], bars ? sIntroColors[bars][1] : gBattleInterface_BallDisplayPal[15]);
            }
        }
        // A different displayed slot still uses its own owner, including an ordinary bench mon.
        EXPECT_EQ(Test_GetPartyMenuBreakBarColors(&gParties[B_TRAINER_PLAYER][1])[0], sIntroColors[5][0]);
        EXPECT(Test_GetPartyMenuBreakBarColors(&gParties[B_TRAINER_PLAYER][2]) == NULL);
        static const struct BreakBarConfig inactive = {.extraBars = 1, .flags = BREAK_BAR_START_INACTIVE};
        owner->breakBarConfig = &inactive;
        owner->breakBarsRemaining = 0;
        owner->breakBarsPeak = 0;
        EXPECT(Test_GetPartyMenuBreakBarColors(&gParties[B_TRAINER_PLAYER][0]) == NULL);
        owner->breakBarsPeak = 1;
        EXPECT_EQ(Test_GetPartyMenuBreakBarColors(&gParties[B_TRAINER_PLAYER][0])[0], gBattleInterface_BallDisplayPal[14]);
        gMain.inBattle = FALSE;
        EXPECT(Test_GetPartyMenuBreakBarColors(&gParties[B_TRAINER_PLAYER][0]) == NULL);
        gMain.inBattle = TRUE;
        gBattleTypeFlags = BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI;
        EXPECT(Test_GetPartyMenuBreakBarColors(&gParties[B_TRAINER_PLAYER][0]) == NULL);
        gBattleTypeFlags = BATTLE_TYPE_DOUBLE | BATTLE_TYPE_INGAME_PARTNER;
        EXPECT(Test_GetPartyMenuBreakBarColors(&gParties[B_TRAINER_PLAYER][0]) == NULL);
        gPartyMenu = savedMenu;
        gMain.inBattle = savedInBattle;
        gBattleTypeFlags = savedFlags;
        memcpy(gBattlePartyCurrentOrder, savedOrder, sizeof(savedOrder));
    }
}

static u32 IntroPalette(u32 battler)
{
    u32 bar = gSprites[gHealthboxSpriteIds[battler]].data[5];
    return OBJ_PLTT_ID(gSprites[bar].oam.paletteNum);
}

static void CopyIntroTiles(u32 battler, u8 *tiles)
{
    u32 bar = gSprites[gHealthboxSpriteIds[battler]].data[5];
    memcpy(tiles, (const void *)(OBJ_VRAM0
        + (gSprites[bar].oam.tileNum + 2) * TILE_SIZE_4BPP), 6 * TILE_SIZE_4BPP);
}

static bool32 RunIntroNative(void (*func)(void))
{
    const struct __attribute__((packed)) { u8 opcode; void (*func)(void); u8 battler; }
        command = {0, func, BS_OPPONENT1};
    const u8 *saved = gBattlescriptCurrInstr;
    gBattlescriptCurrInstr = (const u8 *)&command;
    func();
    bool32 advanced = gBattlescriptCurrInstr == (const u8 *)(&command + 1);
    gBattlescriptCurrInstr = saved;
    return advanced;
}

SINGLE_BATTLE_TEST("Break Bar visual multi-bar fill intro visits each layer and ends at the real HP fraction")
{
    u32 bars, hp;
    PARAMETRIZE { bars = 3; hp = 100; }
    PARAMETRIZE { bars = 5; hp = 37; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { BreakBars(&sIntroBars[bars - 1]); HP(hp); MaxHP(100); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(gBattleTestRunnerState->data.breakBarFirstVisibleMask & 2, 2);
        EXPECT_EQ(gBattleTestRunnerState->data.breakBarFirstVisibleColors[1][0], gBattleInterface_BallDisplayPal[10]);
        EXPECT_EQ(gBattleTestRunnerState->data.breakBarFirstVisibleColors[1][1], gBattleInterface_BallDisplayPal[11]);
        u8 full[6 * TILE_SIZE_4BPP];
        EXPECT_EQ(DrawHealthbarOverlay(1, 0, 1, B_HEALTHBAR_PIXELS), TRUE);
        CopyIntroTiles(1, full);
        EXPECT_EQ(memcmp(gBattleTestRunnerState->data.breakBarFirstVisibleTiles[1], full, sizeof(full)), 0);
        UpdateHealthboxAttribute(gHealthboxSpriteIds[1], GetBattlerMon(1), HEALTHBOX_HEALTH_BAR);
        EXPECT_EQ((u32)GetBattlerPartyState(1)->breakBarIntroShown, TRUE);
        EXPECT_EQ(GetBreakBarCount(1), bars);
        EXPECT_EQ(opponent->hp, hp);
        EXPECT_EQ(opponent->maxHP, 100);
        EXPECT_EQ(AnimateBreakBarIntro(1), FALSE);
        EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 10], sIntroColors[bars][0]);
        EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 11], sIntroColors[bars][1]);
        EXPECT_EQ((u32)gBattleStruct->savedTargetCount, 0);
    }
}

SINGLE_BATTLE_TEST("Break Bar initial healthbox has the correct color on its first visible frame")
{
    u32 bars;
    PARAMETRIZE { bars = 1; }
    PARAMETRIZE { bars = 2; }
    PARAMETRIZE { bars = 3; }
    PARAMETRIZE { bars = 4; }
    PARAMETRIZE { bars = 5; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { BreakBars(&gTrainerBreakBarProfiles[bars - 1]); HP(13); MaxHP(100); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { BreakBars(&gTrainerBreakBarProfiles[bars - 1]); HP(37); MaxHP(100); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(gBattleTestRunnerState->data.breakBarFirstVisibleMask & 3, 3);
        for (u32 battler = 0; battler < 2; battler++)
        {
            EXPECT_EQ(gBattleTestRunnerState->data.breakBarFirstVisibleColors[battler][0], sIntroColors[bars][0]);
            EXPECT_EQ(gBattleTestRunnerState->data.breakBarFirstVisibleColors[battler][1], sIntroColors[bars][1]);
            EXPECT_NE(gBattleTestRunnerState->data.breakBarFirstVisiblePalette[battler], IndexOfSpritePaletteTag(TAG_HEALTHBAR_PAL));
            EXPECT_EQ(GetBreakBarCount(battler), bars);
        }
        EXPECT_EQ(player->hp, 13);
        EXPECT_EQ(opponent->hp, 37);
    }
}

DOUBLE_BATTLE_TEST("Break Bar initial doubles healthboxes preserve each first visible layer color")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { BreakBars(&gTrainerBreakBarProfiles[0]); Moves(MOVE_CELEBRATE); }
        PLAYER(SPECIES_WOBBUFFET) { BreakBars(&gTrainerBreakBarProfiles[4]); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { BreakBars(&gTrainerBreakBarProfiles[1]); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { BreakBars(&gTrainerBreakBarProfiles[3]); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_CELEBRATE); MOVE(playerRight, MOVE_CELEBRATE); MOVE(opponentLeft, MOVE_CELEBRATE); MOVE(opponentRight, MOVE_CELEBRATE); }
    } THEN {
        static const u8 bars[] = {1, 2, 5, 4};
        EXPECT_EQ(gBattleTestRunnerState->data.breakBarFirstVisibleMask, 15);
        for (u32 battler = 0; battler < 4; battler++)
        {
            EXPECT_EQ(gBattleTestRunnerState->data.breakBarFirstVisibleColors[battler][0], sIntroColors[bars[battler]][0]);
            EXPECT_EQ(gBattleTestRunnerState->data.breakBarFirstVisibleColors[battler][1], sIntroColors[bars[battler]][1]);
        }
    }
}

SINGLE_BATTLE_TEST("Break Bar legacy lerp request is compiled out and cannot start a fade")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(ActivateBreakBar(1, &gTrainerBreakBarProfiles[4]), TRUE);
        u32 tasksBefore = 0;
        for (u32 task = 0; task < NUM_TASKS; task++)
            tasksBefore += gTasks[task].isActive;
        GetBattlerPartyState(1)->breakBarsRemaining = 2;
        RefreshBreakBarPalette(1, TRUE); // Kept API argument cannot reactivate disabled lerp code.
        EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 10], sIntroColors[2][0]);
        EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 11], sIntroColors[2][1]);
        u32 tasksAfter = 0;
        for (u32 task = 0; task < NUM_TASKS; task++)
            tasksAfter += gTasks[task].isActive;
        EXPECT_EQ(tasksAfter, tasksBefore);
    }
}

SINGLE_BATTLE_TEST("Break Bar presentation plain grants fill without a fade or any additional gameplay change")
{
    static const struct BreakBarConfig inactive[] =
    {
        {.extraBars = 5, .flags = BREAK_BAR_START_INACTIVE | BREAK_BAR_DEFER_INITIAL_DRAW},
        {.extraBars = 5, .flags = BREAK_BAR_START_INACTIVE | BREAK_BAR_INTRO_FILLS},
    };
    u32 hp, bars;
    bool32 dramatic;
    PARAMETRIZE { hp = 1; bars = 1; dramatic = FALSE; }
    PARAMETRIZE { hp = 37; bars = 2; dramatic = FALSE; }
    PARAMETRIZE { hp = 100; bars = 5; dramatic = FALSE; }
    PARAMETRIZE { hp = 37; bars = 2; dramatic = TRUE; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { HP(hp); MaxHP(100); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(ActivateBreakBar(1, &inactive[dramatic]), TRUE);
        u32 tasksBefore = 0;
        for (u32 task = 0; task < NUM_TASKS; task++)
            tasksBefore += gTasks[task].isActive;
        for (u32 i = 0; i < bars; i++)
            EXPECT_EQ(GrantConfiguredBreakBar(1, 0), TRUE);
        EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 10], sIntroColors[bars][0]);
        struct PartyState before = *GetBattlerPartyState(1);
        struct BattlePokemon monBefore = gBattleMons[1];
        u8 expected[6 * TILE_SIZE_4BPP], actual[6 * TILE_SIZE_4BPP];
        CopyIntroTiles(1, expected);
        EXPECT_EQ(AnimateConfiguredBreakBar(1), TRUE); // First frame is empty in the new color.
        CopyIntroTiles(1, actual);
        EXPECT_NE(memcmp(actual, expected, sizeof(actual)), 0);
        u32 frames = 1;
        bool32 pending;
        do
        {
            pending = AnimateConfiguredBreakBar(1);
            frames++;
            EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 10], sIntroColors[bars][0]);
            EXPECT_EQ(memcmp(GetBattlerPartyState(1), &before, sizeof(before)), 0);
            EXPECT_EQ(memcmp(&gBattleMons[1], &monBefore, sizeof(monBefore)), 0);
            EXPECT_EQ(gBattleControllerExecFlags, 0);
        } while (pending && frames < 30);
        EXPECT_EQ(pending, FALSE);
        EXPECT_LE(frames, 17);
        CopyIntroTiles(1, actual);
        EXPECT_EQ(memcmp(actual, expected, sizeof(actual)), 0);
        EXPECT_EQ(AnimateConfiguredBreakBar(1), FALSE);
        u32 tasksAfter = 0;
        for (u32 task = 0; task < NUM_TASKS; task++)
            tasksAfter += gTasks[task].isActive;
        EXPECT_EQ(tasksAfter, tasksBefore);
        EXPECT_EQ(opponent->hp, hp);
    }
}

SINGLE_BATTLE_TEST("Break Bar presentation explicit successive fills require a flag and cancel on reshow")
{
    static const struct BreakBarConfig unflagged = {.extraBars = 2};
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { HP(37); MaxHP(100); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(ActivateBreakBar(1, &unflagged), TRUE);
        EXPECT_EQ(RunIntroNative(BS_StartBreakBarIntro), TRUE);
        EXPECT_EQ(RunIntroNative(BS_ContinueBreakBarIntro), TRUE); // No presentation without the flag.
        EXPECT_EQ((u32)GetBattlerPartyState(1)->breakBarIntroShown, FALSE);
        GetBattlerPartyState(1)->breakBarsRemaining = 0;
        EXPECT_EQ(ActivateBreakBar(1, &sIntroBars[1]), TRUE);
        GetBattlerPartyState(1)->breakBarIntroShown = TRUE; // Explicit script beats may repeat a shown intro.
        RefreshBreakBarPalette(1, FALSE);
        UpdateHealthboxAttribute(gHealthboxSpriteIds[1], GetBattlerMon(1), HEALTHBOX_HEALTH_BAR);
        u8 before[6 * TILE_SIZE_4BPP], after[6 * TILE_SIZE_4BPP];
        CopyIntroTiles(1, before);
        gBattleControllerExecFlags = 1;
        EXPECT_EQ(RunIntroNative(BS_StartBreakBarIntro), FALSE);
        gBattleControllerExecFlags = 0;
        EXPECT_EQ(RunIntroNative(BS_StartBreakBarIntro), TRUE);
        EXPECT_EQ(RunIntroNative(BS_ContinueBreakBarIntro), FALSE);
        EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 10], sIntroColors[2][0]);
        CopyIntroTiles(1, after);
        EXPECT_EQ(memcmp(before, after, sizeof(before)), 0); // First poll stages green without repainting blue.
        EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 14], gBattleInterface_BallDisplayPal[10]);
        EXPECT_EQ(RunIntroNative(BS_ContinueBreakBarIntro), FALSE);
        EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 10], sIntroColors[1][0]);
        ReshowBreakBars();
        EXPECT_EQ(RunIntroNative(BS_ContinueBreakBarIntro), TRUE); // Cancellation cannot restart the script animation.
        EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 10], sIntroColors[2][0]);
        EXPECT_EQ(GetBreakBarCount(1), 2);
        EXPECT_EQ(opponent->hp, 37);
    }
}

SINGLE_BATTLE_TEST("Break Bar presentation activation uses the true color immediately without a fade task")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { HP(37); MaxHP(100); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        static const struct BreakBarConfig normal = {.extraBars = 2};
        u32 tasks = 0;
        for (u32 i = 0; i < NUM_TASKS; i++)
            tasks += gTasks[i].isActive;
        EXPECT_EQ(ActivateBreakBar(1, &normal), TRUE);
        EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 10], sIntroColors[2][0]);
        EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 11], sIntroColors[2][1]);
        u32 after = 0;
        for (u32 i = 0; i < NUM_TASKS; i++)
            after += gTasks[i].isActive;
        EXPECT_EQ(after, tasks);
        EXPECT_EQ(AnimateBreakBarIntro(1), FALSE); // Normal profiles never opt in.
        EXPECT_EQ(opponent->hp, 37);
    }
}

SINGLE_BATTLE_TEST("Break Bar presentation fills each extra color and restores exact partial HP without gameplay writes")
{
    u32 bars, hp;
    PARAMETRIZE { bars = 1; hp = 1; }
    PARAMETRIZE { bars = 1; hp = 100; }
    PARAMETRIZE { bars = 2; hp = 37; }
    PARAMETRIZE { bars = 3; hp = 100; }
    PARAMETRIZE { bars = 4; hp = 1; }
    PARAMETRIZE { bars = 5; hp = 37; }
    PARAMETRIZE { bars = 5; hp = 100; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { HP(hp); MaxHP(100); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(ActivateBreakBar(1, &sIntroBars[bars - 1]), TRUE);
        struct PartyState *owner = GetBattlerPartyState(1);
        // Sentinels guard saved-grant/Charm state against accidental animation reuse.
        owner->breakBarSavedHp[0] = 13;
        owner->breakBarSavedMaxHp[0] = 200;
        owner->breakBarFillPixels = 7;
        owner->breakBarItemUsed = TRUE;
        struct PartyState before = *owner;
        struct BattlePokemon monBefore = gBattleMons[1];
        u8 expected[6 * TILE_SIZE_4BPP], actual[6 * TILE_SIZE_4BPP];
        owner->breakBarIntroShown = TRUE; // Capture the authoritative final tiles, not the full intro base.
        RefreshBreakBarPalette(1, FALSE);
        UpdateHealthboxAttribute(gHealthboxSpriteIds[1], GetBattlerMon(1), HEALTHBOX_HEALTH_BAR);
        CopyIntroTiles(1, expected);
        owner->breakBarIntroShown = FALSE;
        u32 frames = 0, colors = 0;
        bool32 pending;
        do
        {
            pending = AnimateBreakBarIntro(1);
            frames++;
            EXPECT_EQ(opponent->hp, hp);
            EXPECT_EQ(opponent->maxHP, 100);
            EXPECT_EQ(GetBreakBarCount(1), bars);
            EXPECT_EQ(gBattleControllerExecFlags, 0);
            EXPECT_EQ(memcmp(&gBattleMons[1], &monBefore, sizeof(monBefore)), 0);
            for (u32 color = 1; color <= bars; color++)
                if (gPlttBufferUnfaded[IntroPalette(1) + 10] == sIntroColors[color][0]
                 && gPlttBufferUnfaded[IntroPalette(1) + 11] == sIntroColors[color][1])
                    colors |= 1u << color;
        } while (pending && frames < 270);
        EXPECT_EQ(pending, FALSE);
        u32 finalPixels = max(1, hp * B_HEALTHBAR_PIXELS / 100);
        EXPECT_GE(frames, 12 + (bars - 1) * B_HEALTHBAR_PIXELS + finalPixels);
        EXPECT_LE(frames, 258);
        EXPECT_EQ(colors, (1u << (bars + 1)) - 2);
        EXPECT_EQ((u32)owner->breakBarIntroShown, TRUE);
        before.breakBarIntroShown = TRUE;
        EXPECT_EQ(memcmp(owner, &before, sizeof(before)), 0);
        CopyIntroTiles(1, actual);
        EXPECT_EQ(memcmp(actual, expected, sizeof(actual)), 0);
        EXPECT_EQ(AnimateBreakBarIntro(1), FALSE); // No replay.
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
    }
}

SINGLE_BATTLE_TEST("Break Bar presentation dramatic startup preserves its palette slot and stages green before switching tiles")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { HP(100); MaxHP(100); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        u8 ordinary[6 * TILE_SIZE_4BPP];
        CopyIntroTiles(1, ordinary);
        EXPECT_EQ(ActivateBreakBar(1, &sIntroBars[1]), TRUE);
        u32 palette = IntroPalette(1);
        // The visible base already owns the palette which the dramatic fill uses.
        EXPECT_NE(palette, OBJ_PLTT_ID(IndexOfSpritePaletteTag(TAG_HEALTHBAR_PAL)));
        u8 before[6 * TILE_SIZE_4BPP], after[6 * TILE_SIZE_4BPP];
        CopyIntroTiles(1, before);
        EXPECT_EQ(memcmp(ordinary, before, sizeof(before)), 0); // Old hardware slot must still see green.
        EXPECT_EQ(AnimateBreakBarIntro(1), TRUE);
        EXPECT_EQ(IntroPalette(1), palette);
        CopyIntroTiles(1, after);
        EXPECT_EQ(memcmp(before, after, sizeof(before)), 0); // Prepare before touching visible tiles.
        EXPECT_EQ(gPlttBufferUnfaded[palette + 14], gBattleInterface_BallDisplayPal[10]);
        EXPECT_EQ(gPlttBufferUnfaded[palette + 15], gBattleInterface_BallDisplayPal[11]);
        EXPECT_EQ(AnimateBreakBarIntro(1), TRUE);
        EXPECT_EQ(IntroPalette(1), palette);
        CopyIntroTiles(1, after);
        EXPECT_EQ(DrawHealthbarOverlay(1, 1, 1, 0), TRUE);
        CopyIntroTiles(1, before);
        EXPECT_EQ(memcmp(before, after, sizeof(before)), 0); // Underlying tiles are safe only after staging.
        EXPECT_EQ(opponent->hp, 100);
        EXPECT_EQ(GetBreakBarCount(1), 2);
    }
}

SINGLE_BATTLE_TEST("Break Bar presentation dramatic intro holds full green then slowly covers each completed layer")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { HP(100); MaxHP(100); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(ActivateBreakBar(1, &sIntroBars[1]), TRUE);
        struct PartyState before = *GetBattlerPartyState(1);
        struct BattlePokemon monBefore = gBattleMons[1];
        u8 green[6 * TILE_SIZE_4BPP], actual[6 * TILE_SIZE_4BPP], expected[6 * TILE_SIZE_4BPP];
        EXPECT_EQ(AnimateBreakBarIntro(1), TRUE); // Palette staging yields before the hold touches tiles.
        for (u32 frame = 0; frame < 12; frame++)
        {
            EXPECT_EQ(AnimateBreakBarIntro(1), TRUE);
            EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 14], gBattleInterface_BallDisplayPal[10]);
            EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 15], gBattleInterface_BallDisplayPal[11]);
            CopyIntroTiles(1, actual);
            if (frame == 0)
            {
                memcpy(green, actual, sizeof(green));
                EXPECT_EQ(DrawHealthbarOverlay(1, 1, 1, 0), TRUE);
                CopyIntroTiles(1, expected);
                EXPECT_EQ(memcmp(actual, expected, sizeof(actual)), 0);
            }
            EXPECT_EQ(memcmp(actual, green, sizeof(green)), 0);
        }
        for (u32 color = 1; color <= 2; color++)
            for (u32 pixel = 0; pixel <= B_HEALTHBAR_PIXELS; pixel++)
            {
                bool32 last = color == 2 && pixel == B_HEALTHBAR_PIXELS;
                EXPECT_EQ(AnimateBreakBarIntro(1), !last);
                EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 10], sIntroColors[color][0]);
                EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 11], sIntroColors[color][1]);
                if (!last)
                {
                    const u16 *underlying = pixel == B_HEALTHBAR_PIXELS ? sIntroColors[color]
                        : color == 1 ? &gBattleInterface_BallDisplayPal[10] : sIntroColors[1];
                    EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 14], underlying[0]);
                    EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 15], underlying[1]);
                }
                CopyIntroTiles(1, actual);
                EXPECT_EQ(DrawHealthbarOverlay(1, 1, 1, pixel), TRUE);
                CopyIntroTiles(1, expected);
                EXPECT_EQ(memcmp(actual, expected, sizeof(actual)), 0);
                EXPECT_EQ(memcmp(&gBattleMons[1], &monBefore, sizeof(monBefore)), 0);
            }
        before.breakBarIntroShown = TRUE;
        EXPECT_EQ(memcmp(GetBattlerPartyState(1), &before, sizeof(before)), 0);
        EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 14], gBattleInterface_BallDisplayPal[14]);
        EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 15], gBattleInterface_BallDisplayPal[15]);
    }
}

SINGLE_BATTLE_TEST("Break Bar presentation waits for controllers and cancels on reshow or a changed layer")
{
    bool32 reshow;
    PARAMETRIZE { reshow = TRUE; }
    PARAMETRIZE { reshow = FALSE; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { HP(37); MaxHP(100); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(ActivateBreakBar(1, &sIntroBars[4]), TRUE);
        struct PartyState *owner = GetBattlerPartyState(1);
        gBattleControllerExecFlags = 1;
        EXPECT_EQ(AnimateBreakBarIntro(1), TRUE);
        EXPECT_EQ((u32)owner->breakBarIntroShown, FALSE);
        gBattleControllerExecFlags = 0;
        EXPECT_EQ(AnimateBreakBarIntro(1), TRUE);
        EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 14], gBattleInterface_BallDisplayPal[10]);
        if (reshow)
            ReshowBreakBars();
        else
            owner->breakBarsRemaining = 2;
        EXPECT_EQ(AnimateBreakBarIntro(1), FALSE);
        EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 10], sIntroColors[reshow ? 5 : 2][0]);
        EXPECT_EQ(opponent->hp, 37);
        EXPECT_EQ((u32)owner->breakBarPending, FALSE);
    }
}

SINGLE_BATTLE_TEST("Break Bar presentation defers custom drawings and skips inactive or missing graphics")
{
    static const struct BreakBarConfig deferred =
        {.extraBars = 2, .flags = BREAK_BAR_INTRO_FILLS | BREAK_BAR_DEFER_INITIAL_DRAW};
    static const struct BreakBarConfig inactive =
        {.extraBars = 2, .flags = BREAK_BAR_INTRO_FILLS | BREAK_BAR_START_INACTIVE};
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(ActivateBreakBar(1, &inactive), TRUE);
        EXPECT_EQ(AnimateBreakBarIntro(1), FALSE);
        EXPECT_EQ(GetBreakBarCount(1), 0);
        EXPECT_EQ(ActivateBreakBar(1, &deferred), TRUE);
        EXPECT_EQ(AnimateBreakBarIntro(1), FALSE);
        GetBattlerPartyState(1)->breakBarsRemaining = 0;
        EXPECT_EQ(ActivateBreakBar(1, &sIntroBars[1]), TRUE);
        u32 healthbox = gHealthboxSpriteIds[1];
        gHealthboxSpriteIds[1] = MAX_SPRITES;
        EXPECT_EQ(AnimateBreakBarIntro(1), FALSE);
        gHealthboxSpriteIds[1] = healthbox;
        ReshowBreakBars();
        EXPECT_EQ(GetBreakBarCount(1), 2);
        EXPECT_EQ(AnimateBreakBarIntro(MAX_BATTLERS_COUNT), FALSE);
    }
}

static void SetIntroWildOverride(struct ScriptContext *ctx, const struct BreakBarConfig *config)
{
    const struct __attribute__((packed)) { const struct BreakBarConfig *config; } args = {config};
    InitScriptContext(ctx, NULL, NULL);
    ctx->scriptPtr = (const u8 *)&args;
    Script_SetWildBreakBars(ctx);
    ctx->scriptPtr = NULL;
    Script_ArmWildBreakBars(ctx);
    ScriptContext_ContinueScript(ctx);
}

WILD_BATTLE_TEST("Break Bar presentation scripted wild entry returns before Pressure and leaves Charm's saved fill intact")
{
    GIVEN {
        struct ScriptContext ctx;
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(37); MaxHP(100); Ability(ABILITY_PRESSURE); Item(ITEM_BREAK_CHARM); Moves(MOVE_CELEBRATE); }
        SetIntroWildOverride(&ctx, &sIntroBars[1]);
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        MESSAGE("The wild Wobbuffet's Break Charm formed an extra HP bar!");
        ABILITY_POPUP(opponent, ABILITY_PRESSURE);
        MESSAGE("The wild Wobbuffet is exerting its pressure!");
    } THEN {
        struct PartyState *owner = GetBattlerPartyState(1);
        EXPECT_EQ((u32)owner->breakBarIntroShown, TRUE);
        EXPECT_EQ(GetBreakBarCount(1), 3);
        EXPECT_EQ(owner->breakBarSavedHp[2], 37);
        EXPECT_EQ(owner->breakBarSavedMaxHp[2], 100);
        EXPECT_EQ(opponent->hp, 100);
        EXPECT_EQ((u32)owner->breakBarItemUsed, TRUE);
        EXPECT_EQ((u32)gBattleStruct->savedTargetCount, 0);
        ScriptContext_Init();
    }
}

extern AiScoreFunc sDynamicAiFunc;
static bool8 sAssignTrainerIntro;
static const struct TrainerBreakBarRules *const sTrainerIntroRules = INLINE_BREAK_BAR_RULES(
    {TRAINER_BREAK_BAR_ENTRY, 0, &sIntroBars[1]},
);

static s32 AssignTrainerIntro(u32 attacker, u32 target, u32 move, s32 score)
{
    if (sAssignTrainerIntro)
    {
        InitializeTrainerBreakBar(0, sTrainerIntroRules, SPECIES_WOBBUFFET, 0);
        sAssignTrainerIntro = FALSE;
    }
    return score;
}

AI_SINGLE_BATTLE_TEST("Break Bar presentation trainer-assigned member shows once across forced reentry")
{
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC);
        sDynamicAiFunc = AssignTrainerIntro;
        sAssignTrainerIntro = TRUE;
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_ROAR); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(37); MaxHP(100); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WYNAUT) { Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_ROAR); EXPECT_MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_ROAR); EXPECT_MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_ROAR); EXPECT_MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_ROAR); EXPECT_MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ((u32)GetBattlerPartyState(1)->breakBarIntroShown, TRUE);
        EXPECT_EQ(GetBreakBarCount(1), 2);
        EXPECT_EQ(opponent->hp, 37);
        EXPECT_EQ((u32)gBattleStruct->partyState[B_SIDE_OPPONENT][1].breakBarIntroShown, FALSE);
    }
}

DOUBLE_BATTLE_TEST("Break Bar presentation keeps independent palettes and fills for two owners")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(37); MaxHP(100); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(13); MaxHP(100); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_CELEBRATE); MOVE(playerRight, MOVE_CELEBRATE); MOVE(opponentLeft, MOVE_CELEBRATE); MOVE(opponentRight, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(ActivateBreakBar(1, &sIntroBars[0]), TRUE);
        EXPECT_EQ(ActivateBreakBar(3, &sIntroBars[4]), TRUE);
        u32 leftPalette = IntroPalette(1), rightPalette = IntroPalette(3);
        EXPECT_NE(leftPalette, rightPalette); // Private slots are owned before either visible green base.
        EXPECT_EQ(AnimateBreakBarIntro(1), TRUE);
        EXPECT_EQ(AnimateBreakBarIntro(3), TRUE);
        EXPECT_EQ(IntroPalette(1), leftPalette);
        EXPECT_EQ(IntroPalette(3), rightPalette);
        for (u32 frame = 1; frame < 258; frame++)
        {
            AnimateBreakBarIntro(1);
            AnimateBreakBarIntro(3);
        }
        EXPECT_EQ(AnimateBreakBarIntro(1), FALSE);
        EXPECT_EQ(AnimateBreakBarIntro(3), FALSE);
        EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(1) + 10], sIntroColors[1][0]);
        EXPECT_EQ(gPlttBufferUnfaded[IntroPalette(3) + 10], sIntroColors[5][0]);
        EXPECT_EQ(GetBreakBarCount(1), 1);
        EXPECT_EQ(GetBreakBarCount(3), 5);
        EXPECT_EQ(opponentLeft->hp, 37);
        EXPECT_EQ(opponentRight->hp, 13);
    }
}

extern const u8 BattleScript_TestConfiguredPlainGrant[];
extern const u8 BattleScript_TestConfiguredCoverGrant[];
static const struct BreakBarConfig sScriptPlainGrant =
{
    .extraBars = 2,
    .flags = BREAK_BAR_START_INACTIVE,
    .phases[0].afterBreakScript = BattleScript_TestConfiguredPlainGrant,
};
static const struct BreakBarConfig sScriptCoverGrant =
{
    .extraBars = 2,
    .flags = BREAK_BAR_START_INACTIVE,
    .phases[0].afterBreakScript = BattleScript_TestConfiguredCoverGrant,
};
static const struct BreakBarConfig *sGrantScriptConfig;
static bool8 sSeedGrantScript, sGrantScriptDone;

extern const u8 BattleScript_TestConfiguredHealGrant[];
static const struct BreakBarConfig sVisualHealGrant =
{
    .extraBars = 1,
    .phases[0].onBreakScript = BattleScript_TestConfiguredHealGrant,
};
static u32 sHealGrantScriptCalls, sHealGrantBars, sHealGrantHp;
static u32 sHealGrantSavedHp, sHealGrantSavedMaxHp, sHealGrantFillPixels;

void Test_PrepareConfiguredHealGrant(void)
{
    NATIVE_ARGS();
    // The granting script owns its one-shot condition. On the covering layer's
    // later break, keep the restored pair and return without another grant.
    gBattleCommunication[MULTISTRING_CHOOSER] = ++sHealGrantScriptCalls == 1;
    if (sHealGrantScriptCalls == 1)
        SetHealAmount(gBattlerTarget, 37);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

void Test_RecordConfiguredHealGrant(void)
{
    NATIVE_ARGS();
    struct PartyState *owner = GetBattlerPartyState(gBattlerTarget);
    sHealGrantBars = GetBreakBarCount(gBattlerTarget);
    sHealGrantHp = gBattleMons[gBattlerTarget].hp;
    sHealGrantSavedHp = owner->breakBarSavedHp[0];
    sHealGrantSavedMaxHp = owner->breakBarSavedMaxHp[0];
    sHealGrantFillPixels = owner->breakBarFillPixels;
    gBattlescriptCurrInstr = cmd->nextInstr;
}

void Test_RecordConfiguredGrantPresentation(void)
{
    NATIVE_ARGS();
    sGrantScriptDone = TRUE;
    gBattlescriptCurrInstr = cmd->nextInstr;
}

SINGLE_BATTLE_TEST("Break Bar visual scripted heal then covering grant fills and later restores the healed HP")
{
    GIVEN {
        sHealGrantScriptCalls = 0;
        sHealGrantBars = sHealGrantHp = sHealGrantSavedHp = sHealGrantSavedMaxHp = sHealGrantFillPixels = 0;
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(100); Moves(MOVE_SEISMIC_TOSS); }
        OPPONENT(SPECIES_WOBBUFFET) { BreakBars(&sVisualHealGrant); HP(100); MaxHP(100); Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        HP_BAR(opponent, hp: 0);
        HP_BAR(opponent, hp: 37);
        ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_WISH_HEAL, opponent);
        HP_BAR(opponent, hp: 100);
        HP_BAR(opponent, hp: 0);
    } THEN {
        struct PartyState *owner = GetBattlerPartyState(1);
        EXPECT_EQ(sHealGrantScriptCalls, 2);
        EXPECT_EQ(sHealGrantBars, 1);
        EXPECT_EQ(sHealGrantHp, 100);
        EXPECT_EQ(sHealGrantSavedHp, 100);
        EXPECT_EQ(sHealGrantSavedMaxHp, 100);
        EXPECT_EQ(sHealGrantFillPixels, B_HEALTHBAR_PIXELS);
        EXPECT_EQ(GetBreakBarCount(1), 0);
        EXPECT_EQ(opponent->hp, 100);
        EXPECT_EQ(opponent->maxHP, 100);
        EXPECT_EQ(owner->breakBarSavedMaxHp[0], 0);
        EXPECT_EQ((u32)owner->breakBarCoverScripts, 0);
        EXPECT_EQ((u32)gBattleStruct->savedTargetCount, 0);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
    }
}

SINGLE_BATTLE_TEST("Break Bar visual dramatic intro runs once then each break normally refills the underlying bar")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(100); Moves(MOVE_SEISMIC_TOSS); }
        OPPONENT(SPECIES_WOBBUFFET) { BreakBars(&sIntroBars[2]); HP(100); MaxHP(100); Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        HP_BAR(opponent, hp: 0);
        HP_BAR(opponent, hp: 100);
        HP_BAR(opponent, hp: 0);
        HP_BAR(opponent, hp: 100);
        HP_BAR(opponent, hp: 0);
        HP_BAR(opponent, hp: 100);
    } THEN {
        EXPECT_EQ((u32)GetBattlerPartyState(1)->breakBarIntroShown, TRUE);
        EXPECT_EQ(GetBreakBarCount(1), 0);
        EXPECT_EQ(IsBreakBarBase(1), TRUE);
        EXPECT_EQ(opponent->hp, 100);
        EXPECT_EQ(opponent->maxHP, 100);
        EXPECT_EQ(AnimateBreakBarIntro(1), FALSE);
        EXPECT_EQ((u32)gSprites[gSprites[gHealthboxSpriteIds[1]].data[5]].oam.paletteNum,
            IndexOfSpritePaletteTag(TAG_HEALTHBAR_PAL));
        EXPECT_EQ((u32)gBattleStruct->savedTargetCount, 0);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
    }
}

static s32 SeedGrantScript(u32 attacker, u32 target, u32 move, s32 score)
{
    if (sSeedGrantScript)
    {
        sSeedGrantScript = FALSE;
        ActivateBreakBar(1, sGrantScriptConfig);
        GrantConfiguredBreakBar(1, 0);
    }
    return score;
}

AI_SINGLE_BATTLE_TEST("Break Bar presentation granting scripts finish plain and covering fills before their next effect")
{
    bool32 cover;
    PARAMETRIZE { cover = FALSE; }
    PARAMETRIZE { cover = TRUE; }
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC);
        sGrantScriptConfig = cover ? &sScriptCoverGrant : &sScriptPlainGrant;
        sSeedGrantScript = TRUE;
        sGrantScriptDone = FALSE;
        sDynamicAiFunc = SeedGrantScript; // Fixture initialization only; grants are executed by the phase script.
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(100); Moves(MOVE_SEISMIC_TOSS); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(10); MaxHP(10); Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); EXPECT_MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        HP_BAR(opponent, hp: 0);
        HP_BAR(opponent, hp: 10);
    } THEN {
        struct PartyState *owner = GetBattlerPartyState(1);
        EXPECT_EQ(sGrantScriptDone, TRUE);
        EXPECT_EQ(GetBreakBarCount(1), 1);
        EXPECT_EQ(opponent->hp, 10);
        EXPECT_EQ(opponent->maxHP, 10);
        EXPECT_EQ(AnimateConfiguredBreakBar(1), FALSE);
        if (cover)
        {
            EXPECT_EQ(owner->breakBarSavedHp[0], 10);
            EXPECT_EQ(owner->breakBarSavedMaxHp[0], 10);
            EXPECT_EQ(owner->breakBarFillPixels, B_HEALTHBAR_PIXELS);
            EXPECT_EQ((u32)owner->breakBarCoverScripts, 1);
        }
        else
        {
            EXPECT_EQ(owner->breakBarSavedMaxHp[0], 0);
            EXPECT_EQ((u32)owner->breakBarCoverScripts, 0);
        }
        EXPECT_EQ((u32)gBattleStruct->savedTargetCount, 0);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
    }
}
