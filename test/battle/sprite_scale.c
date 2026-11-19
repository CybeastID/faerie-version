#include "global.h"
#include "test/battle.h"
#include "battle_anim.h"
#include "battle_bug_space.h"
#include "battle_controllers.h"
#include "battle_entry_scripts.h"
#include "battle_main.h"
#include "battle_reduction.h"
#include "battle_script_native.h"
#include "battle_sprite_scale.h"
#include "battle_util.h"
#include "event_data.h"
#include "main.h"
#include "palette.h"
#include "reshow_battle_screen.h"
#include "sprite.h"
#include "task.h"
#include "malloc.h"
#include "constants/battle_script_commands.h"

extern u32 gOamMatrixAllocBitmap;
extern u32 Test_BattleSpriteScaleBgCopies(u32 battler, bool32 clear);
extern u32 Test_BattleSpriteScaleCloneFrames(u32 battler, bool32 clear);
extern void Test_Battle_ExitCleanupFrame(void);

enum
{
    SCALE_SMOL = 1 << 0,
    SCALE_BUG = 1 << 1,
    SCALE_REDUCTION = 1 << 2,
    SCALE_REAL_REDUCTION = 1 << 3,
    SCALE_RESHOW = 1 << 4,
};

static EWRAM_DATA bool8 sOriginalSmol = FALSE;
static EWRAM_DATA bool8 sScaleReshown = FALSE;
static EWRAM_DATA void (*sScaleController)(enum BattlerId) = NULL;

static void WaitForScaleReshow(enum BattlerId battler)
{
    if (gMain.callback2 == BattleMainCB2)
    {
        sScaleReshown = TRUE;
        gBattlerControllerFuncs[battler] = sScaleController;
        MarkBattleControllerIdleOnLocal(battler);
    }
}

static void Task_ScaleReshow(u8 taskId)
{
    if (gBattleResults.battleTurnCounter >= 1 && gBattleControllerExecFlags == 0)
    {
        sScaleController = gBattlerControllerFuncs[0];
        gBattlerControllerFuncs[0] = WaitForScaleReshow;
        MarkBattlerForControllerExec(0);
        CloseMainBattleScreen();
        ReshowBattleScreenAfterMenu();
        DestroyTask(taskId);
    }
}

static void BS_ScaleFixture(void)
{
    NATIVE_ARGS(u8 flags);
    if (cmd->flags & SCALE_SMOL)
        FlagSet(FLAG_PLAYER_SMOL);
    if (cmd->flags & SCALE_BUG)
    {
        gBattleStruct->bugSpace.active = TRUE;
        gBattleStruct->bugSpace.mode = BUGSPACE_MODE_KOANKO_OPENING;
        gBattleStruct->bugSpace.sourceBattler = 1;
        gBattleStruct->bugSpace.bpThreshold = 80;
        gBattleStruct->bugSpace.bpDecayRate = 0;
    }
    if (cmd->flags & (SCALE_REDUCTION | SCALE_REAL_REDUCTION))
    {
        EXPECT_EQ(ReductionTryActivate(10, B_SIDE_PLAYER), TRUE);
        if (cmd->flags & SCALE_REDUCTION)
        {
            ReductionBeginMutation(0);
            gBattleStruct->reduction.occupant[0].lostLevels = 50;
            ReductionEndMutation(0);
        }
        else
            gBattleStruct->reduction.grace = FALSE;
    }
    if (cmd->flags & SCALE_RESHOW)
    {
        sScaleReshown = FALSE;
        EXPECT_LT(CreateTask(Task_ScaleReshow, 0), NUM_TASKS);
    }
    gBattlescriptCurrInstr = cmd->nextInstr;
}

static const struct __attribute__((packed))
{
    u8 native;
    void (*func)(void);
    u8 flags;
    u8 ret;
} sScaleFixtures[] =
{
    {B_SCR_OP_CALLNATIVE, BS_ScaleFixture, 0, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_ScaleFixture, SCALE_SMOL, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_ScaleFixture, SCALE_BUG, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_ScaleFixture, SCALE_REDUCTION, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_ScaleFixture, SCALE_BUG | SCALE_REDUCTION, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_ScaleFixture, SCALE_REAL_REDUCTION, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_ScaleFixture, SCALE_SMOL | SCALE_BUG | SCALE_REDUCTION | SCALE_RESHOW, B_SCR_OP_RETURN},
};

static void ScaleFixture(u32 fixture)
{
    for (u32 battler = 0; battler < MAX_BATTLERS_COUNT; battler++)
    {
        Test_BattleSpriteScaleBgCopies(battler, TRUE);
        Test_BattleSpriteScaleCloneFrames(battler, TRUE);
    }
    sOriginalSmol = FlagGet(FLAG_PLAYER_SMOL);
    FlagClear(FLAG_PLAYER_SMOL);
    TurnZeroScript(B_TRAINER_OPPONENT_A, (const u8 *)&sScaleFixtures[fixture]);
}

static void FinishScaleFixture(void)
{
    u32 task = FindTaskIdByFunc(Task_ScaleReshow);
    if (task != TASK_NONE) DestroyTask(task);
    if (sOriginalSmol) FlagSet(FLAG_PLAYER_SMOL);
    else FlagClear(FLAG_PLAYER_SMOL);
    FORCE_MOVE_ANIM(FALSE);
}

static void SettleScale(void)
{
    // The recorded-test harness ends an otherwise ongoing fixture by issuing
    // PLAYER_TELEPORTED. These THEN checks simulate more live battle frames;
    // genuine outcome tests retain WON/LOST and exercise the exit freeze.
    if (gBattleOutcome == B_OUTCOME_PLAYER_TELEPORTED)
        gBattleOutcome = 0;
    for (u32 frame = 0; frame < 64; frame++)
        BattleSpriteScaleBuildOamBuffer();
}

static const struct OamData *FindMonOam(u32 battler)
{
    const struct Sprite *sprite = &gSprites[gBattlerSpriteIds[battler]];
    for (u32 i = 0; i < ARRAY_COUNT(gMain.oamBuffer); i++)
    {
        const struct OamData *oam = &gMain.oamBuffer[i];
        if (oam->affineMode != ST_OAM_AFFINE_ERASE
         && oam->tileNum == sprite->oam.tileNum && oam->paletteNum == sprite->oam.paletteNum
         && oam->shape == sprite->oam.shape && oam->size == sprite->oam.size)
            return oam;
    }
    EXPECT(FALSE);
    return NULL;
}

static s16 RenderedMatrix(u32 battler, u32 component)
{
    const struct OamData *oam = FindMonOam(battler);
    EXPECT_NE(oam, NULL);
    return gMain.oamBuffer[oam->matrixNum * 4 + component].affineParam;
}

SINGLE_BATTLE_TEST("Battle Sprite Scale normal sprites keep their existing matrix and positioning")
{
    GIVEN {
        ScaleFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        u32 matrices = gOamMatrixAllocBitmap;
        s32 y = gSprites[gBattlerSpriteIds[0]].y2;
        u32 matrix = gSprites[gBattlerSpriteIds[0]].oam.matrixNum;
        BattleSpriteScaleBuildOamBuffer();
        EXPECT_EQ(BattleSpriteScaleCurrent(0), BATTLE_SPRITE_SCALE_ONE);
        EXPECT_EQ(gOamMatrixAllocBitmap, matrices);
        EXPECT_EQ(gSprites[gBattlerSpriteIds[0]].y2, y);
        EXPECT_EQ((u32)FindMonOam(0)->matrixNum, matrix);
        EXPECT_EQ(RenderedMatrix(0, 0), gOamMatrices[matrix].a);
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

SINGLE_BATTLE_TEST("Battle Sprite Scale reads PLAYER_SMOL without changing the flag or gameplay data")
{
    GIVEN {
        ScaleFixture(1);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); HP(100); MaxHP(100); Attack(123); Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(100); HP(100); MaxHP(100); Attack(321); Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        SettleScale();
        EXPECT_EQ(BattleSpriteScaleCurrent(0), BATTLE_SPRITE_SCALE_SMOL);
        EXPECT_EQ(BattleSpriteScaleCurrent(1), BATTLE_SPRITE_SCALE_ONE);
        EXPECT_EQ(RenderedMatrix(0, 0), 512);
        EXPECT_EQ(FlagGet(FLAG_PLAYER_SMOL), TRUE);
        EXPECT_EQ(player->attack, 123);
        EXPECT_EQ(opponent->attack, 321);
        EXPECT_EQ(player->hp, 100);
        EXPECT_EQ(opponent->hp, 100);
        EXPECT_EQ(GetMonData(GetBattlerMon(0), MON_DATA_LEVEL), 100);
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

SINGLE_BATTLE_TEST("Battle Sprite Scale full Bug Space shrinks its opposition and crush-only restores size")
{
    GIVEN {
        ScaleFixture(2);
        PLAYER(SPECIES_WOBBUFFET) { Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        SettleScale();
        EXPECT_EQ(BattleSpriteScaleCurrent(0), 176);
        EXPECT_EQ(BattleSpriteScaleCurrent(1), 256);
        gBattleStruct->bugSpace.mode = BUGSPACE_MODE_KOANKO_CRUSH_ONLY;
        EXPECT_EQ(gBattleStruct->bugSpace.active, TRUE);
        BattleSpriteScaleBuildOamBuffer();
        EXPECT_EQ(BattleSpriteScaleCurrent(0), 180); // Visible transition, not a size jump.
        SettleScale();
        EXPECT_EQ(BattleSpriteScaleCurrent(0), 256);
        EXPECT_EQ(gBattleStruct->bugSpace.bpThreshold, 80);
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

SINGLE_BATTLE_TEST("Battle Sprite Scale follows each genuine Reduction update and reaches a visible level-one floor")
{
    u32 turns;
    PARAMETRIZE { turns = 1; }
    PARAMETRIZE { turns = 2; }
    PARAMETRIZE { turns = 3; }
    PARAMETRIZE { turns = 9; }
    GIVEN {
        ScaleFixture(5);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        for (u32 i = 0; i < turns; i++)
            TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        u32 loss = min(100, 10 * turns + turns * (turns - 1) / 2);
        u32 level = max(1, (s32)100 - loss);
        EXPECT_EQ(player->level, level);
        EXPECT_EQ(GetMonData(GetBattlerMon(0), MON_DATA_LEVEL), 100);
        SettleScale();
        EXPECT_EQ(BattleSpriteScaleCurrent(0), level == 1 ? 96 : 96 + 160 * level / 100);
        EXPECT_GE(BattleSpriteScaleCurrent(0), BATTLE_SPRITE_SCALE_MIN);
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

SINGLE_BATTLE_TEST("Battle Sprite Scale overlaps by stronger shrink and removing either source preserves the other")
{
    GIVEN {
        ScaleFixture(4);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        gBattleStruct->bugSpace.bpThreshold = 40;
        SettleScale();
        EXPECT_EQ(BattleSpriteScaleCurrent(0), 136);
        gBattleStruct->bugSpace.active = FALSE;
        SettleScale();
        EXPECT_EQ(BattleSpriteScaleCurrent(0), 176);
        ReductionFinishBattle();
        SettleScale();
        EXPECT_EQ(BattleSpriteScaleCurrent(0), 256);
        EXPECT_EQ(player->level, 100);
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

SINGLE_BATTLE_TEST("Battle Sprite Scale composes rotation and squash without taking animation matrix ownership")
{
    GIVEN {
        ScaleFixture(1);
        PLAYER(SPECIES_WOBBUFFET) { Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        SettleScale();
        struct Sprite *sprite = &gSprites[gBattlerSpriteIds[0]];
        u32 matrix = sprite->oam.matrixNum;
        struct OamMatrix original = gOamMatrices[matrix];
        s16 y = sprite->y2;
        // A nonuniform, rotated temporary animation transform.
        SetOamMatrix(matrix, 192, -64, 128, 320);
        sprite->y2 = 13;
        u32 bitmap = gOamMatrixAllocBitmap;
        BattleSpriteScaleBuildOamBuffer();
        EXPECT_EQ(RenderedMatrix(0, 0), 384);
        EXPECT_EQ(RenderedMatrix(0, 1), -128);
        EXPECT_EQ(RenderedMatrix(0, 2), 256);
        EXPECT_EQ(RenderedMatrix(0, 3), 640);
        EXPECT_EQ(gOamMatrices[matrix].a, 192);
        EXPECT_EQ(gOamMatrices[matrix].b, -64);
        EXPECT_EQ(sprite->y2, 13);
        EXPECT_EQ(gOamMatrixAllocBitmap, bitmap);
        EXPECT_EQ((u32)sprite->oam.matrixNum, matrix);
        gOamMatrices[matrix] = original;
        sprite->y2 = y;
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

SINGLE_BATTLE_TEST("Battle Sprite Scale real Minimize animation restores the persistent base")
{
    GIVEN {
        ScaleFixture(1);
        FORCE_MOVE_ANIM(TRUE);
        PLAYER(SPECIES_WOBBUFFET) { Speed(10); Moves(MOVE_MINIMIZE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_MINIMIZE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        SettleScale();
        EXPECT_EQ(BattleSpriteScaleCurrent(0), 128);
        EXPECT_EQ(RenderedMatrix(0, 0), 512);
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

SINGLE_BATTLE_TEST("Battle Sprite Scale menu sprite reconstruction preserves the current scale and releases matrices")
{
    GIVEN {
        ScaleFixture(3);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        SettleScale();
        u32 bitmap = gOamMatrixAllocBitmap;
        u32 oldSprite = gBattlerSpriteIds[0];
        FreeOamMatrix(gSprites[oldSprite].oam.matrixNum);
        DestroySprite(&gSprites[oldSprite]);
        CreateBattlerSprite(0); // The actual reshow factory.
        BattleSpriteScaleBuildOamBuffer();
        EXPECT_EQ(BattleSpriteScaleCurrent(0), 176);
        EXPECT_EQ(RenderedMatrix(0, 0), 256 * 256 / 176);
        EXPECT_EQ(__builtin_popcount(gOamMatrixAllocBitmap), __builtin_popcount(bitmap));
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

SINGLE_BATTLE_TEST("Battle Sprite Scale full menu close and reshow preserves overlapping sources")
{
    GIVEN {
        ScaleFixture(6);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(sScaleReshown, TRUE);
        SettleScale();
        EXPECT_EQ(FlagGet(FLAG_PLAYER_SMOL), TRUE);
        EXPECT_EQ(BugSpaceHasDoomClock(), TRUE);
        EXPECT_EQ(ReductionIsActive(), TRUE);
        EXPECT_EQ(BattleSpriteScaleCurrent(0), 128);
        EXPECT_EQ(RenderedMatrix(0, 0), 512);
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

SINGLE_BATTLE_TEST("Battle Sprite Scale ordinary switching and returning follow the current Reduction occupant")
{
    GIVEN {
        ScaleFixture(3);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(10); Moves(MOVE_CELEBRATE); }
        PLAYER(SPECIES_PIKACHU) { Level(100); Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { SWITCH(player, 1); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { SWITCH(player, 0); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        SettleScale();
        EXPECT_EQ(gBattlerPartyIndexes[0], 0);
        EXPECT_EQ(gBattleStruct->reduction.occupant[0].lostLevels, 10);
        EXPECT_EQ(player->level, 90);
        EXPECT_EQ(BattleSpriteScaleCurrent(0), 240);
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

SINGLE_BATTLE_TEST("Battle Sprite Scale faint replacement does not inherit the outgoing occupant's shrink")
{
    GIVEN {
        ScaleFixture(3);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(10); Moves(MOVE_HEALING_WISH); }
        PLAYER(SPECIES_PIKACHU) { Level(100); Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_HEALING_WISH); MOVE(opponent, MOVE_CELEBRATE); SEND_OUT(player, 1); }
    } THEN {
        SettleScale();
        EXPECT_EQ(gBattlerPartyIndexes[0], 1);
        EXPECT_EQ(gBattleStruct->reduction.occupant[0].lostLevels, 0);
        EXPECT_EQ(BattleSpriteScaleCurrent(0), 256);
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

SINGLE_BATTLE_TEST("Battle Sprite Scale actual Transform and Polymorph retain the same owner's base")
{
    bool32 polymorph;
    PARAMETRIZE { polymorph = FALSE; }
    PARAMETRIZE { polymorph = TRUE; }
    GIVEN {
        ScaleFixture(1);
        FORCE_MOVE_ANIM(TRUE);
        PLAYER(SPECIES_DITTO) { Level(100); Speed(10); Moves(MOVE_TRANSFORM, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WAILORD) { Speed(1); Moves(MOVE_POLYMORPH, MOVE_CELEBRATE); }
    } WHEN {
        TURN {
            MOVE(player, polymorph ? MOVE_CELEBRATE : MOVE_TRANSFORM);
            MOVE(opponent, polymorph ? MOVE_POLYMORPH : MOVE_CELEBRATE);
        }
    } THEN {
        SettleScale();
        EXPECT_EQ(BattleSpriteScaleCurrent(0), 128);
        EXPECT_EQ(RenderedMatrix(0, 0), 512);
        EXPECT_EQ(gBattlerPartyIndexes[0], 0);
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

DOUBLE_BATTLE_TEST("Battle Sprite Scale both actual player-owned battlers shrink in ordinary doubles")
{
    GIVEN {
        ScaleFixture(1);
        PLAYER(SPECIES_WOBBUFFET) { Speed(40); Moves(MOVE_CELEBRATE); }
        PLAYER(SPECIES_PIKACHU) { Speed(30); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(20); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_PIKACHU) { Speed(10); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_CELEBRATE); MOVE(playerRight, MOVE_CELEBRATE); MOVE(opponentLeft, MOVE_CELEBRATE); MOVE(opponentRight, MOVE_CELEBRATE); }
    } THEN {
        SettleScale();
        EXPECT_EQ(BattleSpriteScaleCurrent(0), 128);
        EXPECT_EQ(BattleSpriteScaleCurrent(2), 128);
        EXPECT_EQ(BattleSpriteScaleCurrent(1), 256);
        EXPECT_EQ(BattleSpriteScaleCurrent(3), 256);
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

SINGLE_BATTLE_TEST("Battle Sprite Scale matrix and sprite exhaustion preserve original state and recover")
{
    GIVEN {
        ScaleFixture(1);
        PLAYER(SPECIES_WOBBUFFET) { Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        SettleScale();
        u32 bitmap = gOamMatrixAllocBitmap;
        u32 owned = 0;
        u32 matrix;
        while ((matrix = AllocOamMatrix()) != 0xFF)
            owned |= 1u << matrix;
        u32 sprite = gBattlerSpriteIds[0];
        s32 y = gSprites[sprite].y2;
        BattleSpriteScaleBuildOamBuffer();
        EXPECT_EQ(gOamMatrixAllocBitmap, 0xFFFFFFFF);
        EXPECT_EQ(gSprites[sprite].y2, y);
        EXPECT_EQ((u32)FindMonOam(0)->matrixNum, (u32)gSprites[sprite].oam.matrixNum);
        for (matrix = 0; matrix < OAM_MATRIX_COUNT; matrix++)
            if (owned & (1u << matrix)) FreeOamMatrix(matrix);
        EXPECT_EQ(gOamMatrixAllocBitmap, bitmap);
        gBattlerSpriteIds[0] = MAX_SPRITES;
        BattleSpriteScaleBuildOamBuffer();
        gBattlerSpriteIds[0] = sprite;
        BattleSpriteScaleBuildOamBuffer();
        EXPECT_EQ(RenderedMatrix(0, 0), 512);
        EXPECT_EQ(gOamMatrixAllocBitmap, bitmap);
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

SINGLE_BATTLE_TEST("Battle Sprite Scale grounds tall wide small and elevated art using existing picture offsets")
{
    enum Species species;
    u32 battler;
    PARAMETRIZE { species = SPECIES_WAILORD; battler = 0; }
    PARAMETRIZE { species = SPECIES_RAYQUAZA; battler = 1; }
    PARAMETRIZE { species = SPECIES_DIGLETT; battler = 1; }
    PARAMETRIZE { species = SPECIES_EXEGGUTOR; battler = 0; }
    GIVEN {
        ScaleFixture(1);
        PLAYER(species) { Level(100); Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(species) { Level(100); Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        if (battler == 1)
        {
            gBattleStruct->bugSpace.active = TRUE;
            gBattleStruct->bugSpace.mode = BUGSPACE_MODE_KOANKO_OPENING;
            gBattleStruct->bugSpace.sourceBattler = 0;
            gBattleStruct->bugSpace.bpThreshold = 32; // Half scale for the opposing front art.
        }
        SettleScale();
        const struct Sprite *sprite = &gSprites[gBattlerSpriteIds[battler]];
        const struct OamData *oam = FindMonOam(battler);
        s32 top = oam->y;
        if (top >= 160) top -= 256;
        s32 canvasHalfHeight = sprite->oam.affineMode == ST_OAM_AFFINE_DOUBLE ? 64 : 32;
        s32 artHalfHeight = 32 - (battler == 0 ? gSpeciesInfo[species].backPicYOffset : gSpeciesInfo[species].frontPicYOffset);
        if (battler == 0)
            artHalfHeight = max(0, min(artHalfHeight, 112 - sprite->y - sprite->y2));
        s32 oldFootQ8 = (sprite->y + sprite->y2 + artHalfHeight) * 256;
        s32 newFootQ8 = (top + canvasHalfHeight) * 256 + artHalfHeight * 128;
        EXPECT_LE(abs(newFootQ8 - oldFootQ8), 128); // At most half a pixel of rounding.
        EXPECT_EQ(RenderedMatrix(battler, 0), 512);
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

// A separately filterable, on-screen tuning reel. Headless pause commands skip
// delays, while normal mGBA shows each stage long enough for visual inspection.
static void BS_VisualScaleLoss(void)
{
    NATIVE_ARGS(u8 battler, u8 loss);
    if (!ReductionIsActive())
        EXPECT_EQ(ReductionTryActivate(10, GetBattlerSide(cmd->battler)), TRUE);
    ReductionBeginMutation(cmd->battler);
    gBattleStruct->reduction.occupant[cmd->battler].lostLevels = cmd->loss;
    ReductionEndMutation(cmd->battler);
    ReductionRefreshDisplay(cmd->battler);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

struct __attribute__((packed)) VisualScalePhase
{
    u8 native;
    void (*func)(void);
    u8 battler;
    u8 loss;
    u8 pause;
    u16 frames;
};

#define VISUAL_SCALE_PHASE(battler, loss) {B_SCR_OP_CALLNATIVE, BS_VisualScaleLoss, battler, loss, B_SCR_OP_PAUSE, 450}
static const struct __attribute__((packed))
{
    struct VisualScalePhase phases[5];
    u8 ret;
} sVisualScaleScripts[] =
{
    {{VISUAL_SCALE_PHASE(0, 0), VISUAL_SCALE_PHASE(0, 25), VISUAL_SCALE_PHASE(0, 75), VISUAL_SCALE_PHASE(0, 99), VISUAL_SCALE_PHASE(0, 0)}, B_SCR_OP_RETURN},
    {{VISUAL_SCALE_PHASE(1, 0), VISUAL_SCALE_PHASE(1, 25), VISUAL_SCALE_PHASE(1, 75), VISUAL_SCALE_PHASE(1, 99), VISUAL_SCALE_PHASE(1, 0)}, B_SCR_OP_RETURN},
};
#undef VISUAL_SCALE_PHASE

SINGLE_BATTLE_TEST("Battle Sprite Scale visual review normal progressive minimum and restored art")
{
    enum Species species;
    u32 battler;
    PARAMETRIZE { species = SPECIES_WAILORD; battler = 0; }
    PARAMETRIZE { species = SPECIES_RAYQUAZA; battler = 1; }
    PARAMETRIZE { species = SPECIES_DIGLETT; battler = 1; }
    PARAMETRIZE { species = SPECIES_EXEGGUTOR; battler = 0; }
    GIVEN {
        sOriginalSmol = FlagGet(FLAG_PLAYER_SMOL);
        FlagClear(FLAG_PLAYER_SMOL);
        TurnZeroScript(B_TRAINER_OPPONENT_A, (const u8 *)&sVisualScaleScripts[battler]);
        PLAYER(species) { Level(100); Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(species) { Level(100); Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        SettleScale();
        EXPECT_EQ(BattleSpriteScaleCurrent(battler), 256);
        EXPECT_EQ(gBattleMons[battler].level, 100);
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

SINGLE_BATTLE_TEST("Battle Sprite Scale real damage stat and Transform animations scale their background copies")
{
    enum Move playerMove, opponentMove;
    PARAMETRIZE { playerMove = MOVE_CELEBRATE; opponentMove = MOVE_TACKLE; }
    PARAMETRIZE { playerMove = MOVE_CELEBRATE; opponentMove = MOVE_GROWL; }
    PARAMETRIZE { playerMove = MOVE_TRANSFORM; opponentMove = MOVE_CELEBRATE; }
    GIVEN {
        ScaleFixture(1);
        FORCE_MOVE_ANIM(TRUE);
        PLAYER(SPECIES_DITTO) { Level(100); HP(1000); MaxHP(1000); Speed(10); Moves(MOVE_CELEBRATE, MOVE_TRANSFORM); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(100); Speed(1); Moves(MOVE_CELEBRATE, MOVE_TACKLE, MOVE_GROWL); }
    } WHEN {
        TURN { MOVE(player, playerMove); MOVE(opponent, opponentMove); }
    } THEN {
        if (opponentMove == MOVE_GROWL)
            EXPECT_GT(Test_BattleSpriteScaleCloneFrames(0, FALSE), 0);
        else
            EXPECT_GT(Test_BattleSpriteScaleBgCopies(0, FALSE), 0);
        EXPECT_EQ(BattleSpriteScaleCurrent(0), 128);
        // Inspect the actual last rendered frame, without manually rebuilding
        // OAM or settling the renderer after the animation has finished.
        EXPECT_EQ(RenderedMatrix(0, 0), 512);
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

SINGLE_BATTLE_TEST("Battle Sprite Scale background image uses the same half size and grounding without another buffer")
{
    GIVEN {
        ScaleFixture(1);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(100); Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        u8 *source = Alloc(MON_PIC_SIZE * 2);
        EXPECT(source != NULL);
        u8 *dest = source + MON_PIC_SIZE;
        memset(source, 0x11, MON_PIC_SIZE);
        memset(dest, 0xFF, MON_PIC_SIZE);
        BattleSpriteScaleCopyMonGfx(0, dest, source);
        u32 pixels = 0;
        for (u32 i = 0; i < MON_PIC_SIZE; i++)
        {
            EXPECT_LE(dest[i] & 15, 1);
            EXPECT_LE(dest[i] >> 4, 1);
            pixels += (dest[i] & 15) + (dest[i] >> 4);
        }
        EXPECT_EQ(pixels, 32 * 32);
        // An unaffected opponent uses the exact original image.
        BattleSpriteScaleCopyMonGfx(1, dest, source);
        EXPECT_EQ(memcmp(source, dest, MON_PIC_SIZE), 0);
        Free(source);
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

SINGLE_BATTLE_TEST("Battle Sprite Scale exit retains its last size when gameplay Reduction is cleaned up")
{
    GIVEN {
        ScaleFixture(3);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(100); Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        SettleScale();
        EXPECT_EQ(BattleSpriteScaleCurrent(0), 176);
        gBattleOutcome = B_OUTCOME_WON;
        ReductionFinishBattle();
        SettleScale();
        EXPECT_EQ(BattleSpriteScaleCurrent(0), 176);
        EXPECT_EQ(RenderedMatrix(0, 0), 256 * 256 / 176);
        gBattleOutcome = 0;
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

SINGLE_BATTLE_TEST("Battle Sprite Scale keeps live resources and rendered size throughout exit fadeout")
{
    u32 fixture;
    PARAMETRIZE { fixture = 0; } // Ordinary rendering remains valid too.
    PARAMETRIZE { fixture = 1; } // Persistent SMOL.
    PARAMETRIZE { fixture = 3; } // Temporary Reduction, already cleaned up.
    GIVEN {
        ScaleFixture(fixture);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(100); Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        SettleScale();
        u32 scale = BattleSpriteScaleCurrent(0);
        void *battleStruct = gBattleStruct;
        void *spriteData = gBattleSpritesDataPtr;
        void *monGfx = gMonSpritesGfxPtr;
        void (*battleMainFunc)(void) = gBattleMainFunc;
        u32 matrices = gOamMatrixAllocBitmap;
        u8 outcome = gBattleOutcome;
        struct PaletteFadeControl fade = gPaletteFade;

        gBattleOutcome = B_OUTCOME_WON;
        ReductionFinishBattle();
        BeginFastPaletteFade(3);
        for (u32 frame = 0; frame < 4; frame++)
        {
            EXPECT(gPaletteFade.active);
            Test_Battle_ExitCleanupFrame();
            EXPECT(gBattleStruct == battleStruct);
            EXPECT(gBattleSpritesDataPtr == spriteData);
            EXPECT(gMonSpritesGfxPtr == monGfx);
            EXPECT(gBattleMainFunc == battleMainFunc);
            EXPECT_EQ(gBattleMons[0].species, SPECIES_WOBBUFFET);
            AnimateSprites();
            BattleSpriteScaleBuildOamBuffer();
            EXPECT_EQ(BattleSpriteScaleCurrent(0), scale);
            EXPECT_EQ(RenderedMatrix(0, 0), 256 * 256 / scale);
            EXPECT_EQ(gOamMatrixAllocBitmap, matrices);
            EXPECT_EQ(FlagGet(FLAG_PLAYER_SMOL), fixture == 1);
            UpdatePaletteFade();
        }
        gPaletteFade = fade;
        gBattleOutcome = outcome;
    } AFTER_CLEANUP { FinishScaleFixture(); }
}

SINGLE_BATTLE_TEST("Battle Sprite Scale visual tall back grounding review")
{
    GIVEN {
        sOriginalSmol = FlagGet(FLAG_PLAYER_SMOL);
        FlagClear(FLAG_PLAYER_SMOL);
        TurnZeroScript(B_TRAINER_OPPONENT_A, (const u8 *)&sVisualScaleScripts[0]);
        PLAYER(SPECIES_EXEGGUTOR) { Level(100); Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_EXEGGUTOR) { Level(100); Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        SettleScale();
        EXPECT_EQ(BattleSpriteScaleCurrent(0), 256);
    } AFTER_CLEANUP { FinishScaleFixture(); }
}
