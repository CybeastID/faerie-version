#include "global.h"
#include "test/battle.h"
#include "battle_ai_util.h"
#include "battle_gfx_sfx_util.h"
#include "battle_gimmick.h"
#include "battle_koanko.h"
#include "battle_entry_scripts.h"
#include "battle_main.h"
#include "battle_script_native.h"
#include "battle_setup.h"
#include "battle_util.h"
#include "gpu_regs.h"
#include "palette.h"
#include "sprite.h"
#include "task.h"
#include "constants/battle_script_commands.h"
#include "constants/trainers.h"

enum
{
    ADAPT_OLDER_LAYER = 1 << 0,
    ADAPT_PERISH = 1 << 1,
    ADAPT_OWN_FIELD = 1 << 2,
    ADAPT_TASK_LIMIT = 1 << 3,
};

static EWRAM_DATA u16 sAdaptTasks = 0;

static void AdaptNoopTask(u8 taskId UNUSED)
{
}

static void FreeAdaptTasks(void)
{
    for (u32 task = 0; task < NUM_TASKS; task++)
        if ((sAdaptTasks & (1u << task)) && gTasks[task].func == AdaptNoopTask)
            DestroyTask(task);
    sAdaptTasks = 0;
}

static void FixtureHp(u16 hp)
{
    gBattleMons[1].hp = hp;
    SetMonData(GetBattlerMon(1), MON_DATA_HP, &hp);
}

// The production staging fixture covers the actual opening turn, full heal and
// saved-layer grant. Here isolate the later break with deliberately damaged
// covered HP: an accidental adaptation heal cannot hide behind a full HP value.
static void BS_AdaptFixture(void)
{
    NATIVE_ARGS(u8 flags);
    gBattleStruct->koankoPhase = KOANKO_CONJURED_TEAM;
    KoankoOnSwitchIn(1);
    EXPECT_EQ(KoankoArceusIsProtected(1), TRUE);
    FixtureHp(37);
    if (cmd->flags & ADAPT_OLDER_LAYER)
    {
        EXPECT_EQ(GrantSavedBreakBar(1), TRUE);
        FixtureHp(29);
    }
    if (cmd->flags & ADAPT_OWN_FIELD)
        EXPECT_EQ(KoankoArceusStartBugSpace(1), TRUE);
    KoankoArceusOnCommandSelection();
    KoankoArceusOnEndTurn();
    if (cmd->flags & ADAPT_OWN_FIELD)
        EXPECT_EQ(KoankoArceusDismissClock(1), TRUE);
    u32 layer = GetBreakBarCount(1);
    EXPECT_EQ(GrantSavedBreakBar(1), TRUE);
    FixtureHp(100);
    EXPECT_EQ(KoankoArceusFinishOpening(1, layer), TRUE);
    gBattleMons[1].statStages[STAT_ATK] = DEFAULT_STAT_STAGE + 2;
    gBattleMons[1].statStages[STAT_SPDEF] = DEFAULT_STAT_STAGE - 1;
    if (cmd->flags & ADAPT_PERISH)
    {
        gBattleMons[1].volatiles.perishSong = TRUE;
        gBattleMons[1].volatiles.perishSongTimer = 0;
    }
    if (cmd->flags & ADAPT_TASK_LIMIT)
    {
        u32 freeTasks = 0;
        for (u32 task = 0; task < NUM_TASKS; task++)
            if (!gTasks[task].isActive) freeTasks++;
        while (freeTasks > 3)
        {
            u32 task = CreateTask(AdaptNoopTask, 254);
            EXPECT_EQ(gTasks[task].func, AdaptNoopTask);
            sAdaptTasks |= 1u << task;
            freeTasks--;
        }
    }
    gBattlescriptCurrInstr = cmd->nextInstr;
}

static const struct __attribute__((packed))
{
    u8 native;
    void (*func)(void);
    u8 flags;
    u8 ret;
} sAdaptFixtures[] =
{
    {B_SCR_OP_CALLNATIVE, BS_AdaptFixture, 0, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_AdaptFixture, ADAPT_OLDER_LAYER, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_AdaptFixture, ADAPT_PERISH, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_AdaptFixture, ADAPT_OWN_FIELD, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_AdaptFixture, ADAPT_TASK_LIMIT, B_SCR_OP_RETURN},
};

static void AdaptFixture(u32 fixture)
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    TurnZeroScript(B_TRAINER_OPPONENT_A, (const u8 *)&sAdaptFixtures[fixture]);
}

static void ExpectAdapted(enum Type breakType, enum Species species, u32 hp, u32 bars)
{
    EXPECT_EQ(gBattleStruct->koankoArceus.phase, KOANKO_ARCEUS_ADAPTED);
    EXPECT_EQ(gBattleStruct->koankoArceus.breakType, breakType);
    EXPECT_EQ(gBattleMons[1].species, species);
    EXPECT_EQ(GetMonData(GetBattlerMon(1), MON_DATA_SPECIES), species);
    EXPECT_EQ(KoankoArceusGetForm(1), species);
    EXPECT_EQ(gBattleMons[1].types[0], GetSpeciesType(species, 0));
    EXPECT_EQ(gBattleMons[1].types[1], GetSpeciesType(species, 1));
    EXPECT_EQ(gBattleMons[1].types[2], TYPE_MYSTERY);
    EXPECT_EQ(gBattleMons[1].ability, ABILITY_MULTITYPE);
    EXPECT_EQ(gBattleMons[1].hp, hp);
    EXPECT_EQ(GetMonData(GetBattlerMon(1), MON_DATA_HP), hp);
    EXPECT_EQ(gBattleMons[1].maxHP, 100);
    EXPECT_EQ(GetBreakBarCount(1), bars);
    EXPECT_EQ(KoankoArceusIsProtected(1), FALSE);
    EXPECT_EQ(KoankoArceusTryOvercharge(), FALSE);
}

SINGLE_BATTLE_TEST("Koanko Command Seal adaptation restores damaged HP and preserves nonform battle data")
{
    bool32 shiny;
    PARAMETRIZE { shiny = FALSE; }
    PARAMETRIZE { shiny = TRUE; }
    GIVEN {
        FORCE_MOVE_ANIM(TRUE);
        AdaptFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(10); Moves(MOVE_SEISMIC_TOSS); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(100); MaxHP(100); Attack(83); Defense(97); SpAttack(113); SpDefense(127); Speed(1);
            Item(ITEM_POKE_BALL); Shiny(shiny); Moves(MOVE_CELEBRATE, MOVE_JUDGMENT, MOVE_RECOVER);
            EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        HP_BAR(opponent, hp: 0);
        ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_FORM_CHANGE, opponent);
        NONE_OF {
            ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_FORM_CHANGE, opponent);
            MESSAGE("By my Command Seal! Arceus, overcharge! Release your shackles!");
        }
    } THEN {
        ExpectAdapted(TYPE_FIGHTING, SPECIES_ARCEUS_GHOST, 37, 0);
        EXPECT_EQ(gBattleStruct->koankoArceus.breakSource, 0);
        EXPECT_EQ(opponent->attack, 83);
        EXPECT_EQ(opponent->defense, 97);
        EXPECT_EQ(opponent->spAttack, 113);
        EXPECT_EQ(opponent->spDefense, 127);
        EXPECT_EQ(opponent->speed, 1);
        EXPECT_EQ(opponent->statStages[STAT_ATK], DEFAULT_STAT_STAGE + 2);
        EXPECT_EQ(opponent->statStages[STAT_SPDEF], DEFAULT_STAT_STAGE - 1);
        EXPECT_EQ(opponent->item, ITEM_POKE_BALL);
        EXPECT_EQ(opponent->moves[0], MOVE_CELEBRATE);
        EXPECT_EQ(opponent->moves[1], MOVE_JUDGMENT);
        EXPECT_EQ(opponent->pp[0], GetMovePP(MOVE_CELEBRATE) - 1);
        EXPECT_EQ(opponent->pp[1], GetMovePP(MOVE_JUDGMENT));
        EXPECT_EQ(GetActiveGimmick(1), GIMMICK_NONE);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
        // Existing palette/loading APIs must see the actual typed species,
        // including shiny art and a later controller/menu graphics rebuild.
        const u16 *pal = GetMonFrontSpritePal(GetBattlerMon(1));
        EXPECT_EQ(memcmp(&gPlttBufferUnfaded[OBJ_PLTT_ID(1)], pal, PLTT_SIZE_4BPP), 0);
        EXPECT_EQ(GetGpuReg(REG_OFFSET_MOSAIC), 0);
        BattleLoadMonSpriteGfx(GetBattlerMon(1), 1);
        EXPECT_EQ(memcmp(&gPlttBufferUnfaded[OBJ_PLTT_ID(1)], pal, PLTT_SIZE_4BPP), 0);
    } AFTER_CLEANUP {
        FORCE_MOVE_ANIM(FALSE);
    }
}

SINGLE_BATTLE_TEST("Koanko Command Seal adaptation captures the actual called move and does not adapt twice during multihit")
{
    bool32 called;
    PARAMETRIZE { called = FALSE; }
    PARAMETRIZE { called = TRUE; }
    GIVEN {
        AdaptFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Attack(500); Speed(10); Moves(MOVE_DOUBLE_KICK, MOVE_METRONOME); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(100); MaxHP(100); Defense(50); Speed(1); Moves(MOVE_CELEBRATE);
            EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN {
            if (called) MOVE(player, MOVE_METRONOME, WITH_RNG(RNG_METRONOME, MOVE_SEISMIC_TOSS));
            else MOVE(player, MOVE_DOUBLE_KICK);
            MOVE(opponent, MOVE_CELEBRATE);
        }
    } SCENE {
        HP_BAR(opponent, hp: 0);
        ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_FORM_CHANGE, opponent);
        NOT ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_FORM_CHANGE, opponent);
    } THEN {
        ExpectAdapted(TYPE_FIGHTING, SPECIES_ARCEUS_GHOST, 37, 0);
    }
}

SINGLE_BATTLE_TEST("Koanko Command Seal adaptation captures dynamic Tera type including no-resistance Stellar fallback")
{
    enum Type tera;
    PARAMETRIZE { tera = TYPE_ELECTRIC; }
    PARAMETRIZE { tera = TYPE_STELLAR; }
    GIVEN {
        AdaptFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { SpAttack(9999); Speed(10); TeraType(tera); Moves(MOVE_TERA_BLAST); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(100); MaxHP(100); Speed(1); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_TERA_BLAST, gimmick: GIMMICK_TERA); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        ExpectAdapted(tera, tera == TYPE_STELLAR ? SPECIES_ARCEUS_NORMAL : SPECIES_ARCEUS_GROUND, 37, 0);
    }
}

SINGLE_BATTLE_TEST("Koanko Command Seal adaptation ranks Weather Ball's resolved type against current effective foe types")
{
    bool32 tera;
    PARAMETRIZE { tera = FALSE; }
    PARAMETRIZE { tera = TRUE; }
    GIVEN {
        AdaptFixture(0);
        PLAYER(SPECIES_VENUSAUR) { SpAttack(9999); Speed(10); TeraType(TYPE_GRASS); Moves(MOVE_SUNNY_DAY, MOVE_WEATHER_BALL); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(100); MaxHP(100); Speed(1); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_SUNNY_DAY); MOVE(opponent, MOVE_CELEBRATE); }
        TURN {
            if (tera) MOVE(player, MOVE_WEATHER_BALL, gimmick: GIMMICK_TERA);
            else MOVE(player, MOVE_WEATHER_BALL);
            MOVE(opponent, MOVE_CELEBRATE);
        }
    } THEN {
        ExpectAdapted(TYPE_FIRE, SPECIES_ARCEUS_FIRE, 37, 0);
    }
}

SINGLE_BATTLE_TEST("Koanko Command Seal delayed adaptation captures impact type after unrelated actions and uses the current foe")
{
    enum Move move;
    enum Ability ability;
    bool32 leave;
    enum Type type;
    enum Species form;
    PARAMETRIZE { move = MOVE_FUTURE_SIGHT; ability = ABILITY_BLAZE; leave = FALSE; type = TYPE_PSYCHIC; form = SPECIES_ARCEUS_DARK; }
    PARAMETRIZE { move = MOVE_FUTURE_SIGHT; ability = ABILITY_NORMALIZE; leave = FALSE; type = TYPE_NORMAL; form = SPECIES_ARCEUS_GHOST; }
    PARAMETRIZE { move = MOVE_FUTURE_SIGHT; ability = ABILITY_NORMALIZE; leave = TRUE; type = TYPE_PSYCHIC; form = SPECIES_ARCEUS_DARK; }
    PARAMETRIZE { move = MOVE_DOOM_DESIRE; ability = ABILITY_NORMALIZE; leave = TRUE; type = TYPE_STEEL; form = SPECIES_ARCEUS_FIRE; }
    GIVEN {
        WITH_CONFIG(B_UPDATED_MOVE_TYPES, GEN_9);
        AdaptFixture(0);
        PLAYER(SPECIES_CHARIZARD) { SpAttack(9999); Speed(10); Ability(ability); Moves(move, MOVE_CELEBRATE, MOVE_ELECTRIC_TERRAIN); }
        PLAYER(SPECIES_VENUSAUR) { Speed(10); Moves(MOVE_ELECTRIC_TERRAIN); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(100); MaxHP(100); Speed(1); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(player, move); MOVE(opponent, MOVE_CELEBRATE); }
        TURN {
            if (leave) SWITCH(player, 1);
            else MOVE(player, MOVE_CELEBRATE);
            MOVE(opponent, MOVE_CELEBRATE);
        }
        TURN { MOVE(player, MOVE_ELECTRIC_TERRAIN); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        HP_BAR(opponent, hp: 0);
        ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_FORM_CHANGE, opponent);
    } THEN {
        EXPECT_EQ(gBattleStruct->futureSight[1].resolvedType, type);
        ExpectAdapted(type, form, 37, 0);
    }
}

SINGLE_BATTLE_TEST("Koanko Command Seal adaptation consumes an untyped Perish Song break without using stale move type")
{
    GIVEN {
        AdaptFixture(2);
        PLAYER(SPECIES_WOBBUFFET) { Speed(10); Moves(MOVE_ELECTRIC_TERRAIN); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(100); MaxHP(100); Speed(1); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_ELECTRIC_TERRAIN); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        ExpectAdapted(TYPE_NONE, SPECIES_ARCEUS_NORMAL, 37, 0);
        EXPECT_EQ(gBattleStruct->koankoArceus.breakSource, MAX_BATTLERS_COUNT);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
    }
}

SINGLE_BATTLE_TEST("Koanko Command Seal adaptation leaves older saved layers intact and never adapts on their later break")
{
    GIVEN {
        AdaptFixture(1);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(10); Moves(MOVE_SEISMIC_TOSS, MOVE_NIGHT_SHADE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(100); MaxHP(100); Speed(1); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_NIGHT_SHADE); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        HP_BAR(opponent, hp: 0);
        ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_FORM_CHANGE, opponent);
        HP_BAR(opponent, hp: 0);
        NOT ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_FORM_CHANGE, opponent);
    } THEN {
        ExpectAdapted(TYPE_FIGHTING, SPECIES_ARCEUS_GHOST, 37, 0);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
    }
}

SINGLE_BATTLE_TEST("Koanko Command Seal Judgment runtime and AI typing use the charged form independently of Plate")
{
    GIVEN {
        AdaptFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); HP(1000); MaxHP(1000); Speed(10); Moves(MOVE_SEISMIC_TOSS, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(100); MaxHP(100); Speed(1); Moves(MOVE_CELEBRATE, MOVE_JUDGMENT); EntryScript(&gKoankoArceusProtectionEntry);
        }
        OPPONENT(SPECIES_ARCEUS_NORMAL) { Speed(1); Item(ITEM_FLAME_PLATE); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_JUDGMENT); }
    } SCENE {
        ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_FORM_CHANGE, opponent);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_JUDGMENT, opponent);
        MESSAGE("It's super effective!");
    } THEN {
        ExpectAdapted(TYPE_FIGHTING, SPECIES_ARCEUS_GHOST, 37, 0);
        u16 plate = ITEM_FIST_PLATE;
        opponent->item = plate;
        SetMonData(GetBattlerMon(1), MON_DATA_HELD_ITEM, &plate);
        EXPECT_EQ(GetDynamicMoveType(GetBattlerMon(1), MOVE_JUDGMENT, 1, ABILITY_MULTITYPE, HOLD_EFFECT_PLATE, MON_IN_BATTLE), TYPE_GHOST);
        EXPECT_EQ(GetDynamicMoveType(&gParties[B_TRAINER_OPPONENT_A][1], MOVE_JUDGMENT, 1, ABILITY_MULTITYPE, HOLD_EFFECT_PLATE, MON_OUTSIDE_BATTLE), TYPE_FIRE);
        EXPECT_EQ(TryBattleFormChange(1, FORM_CHANGE_ITEM_HOLD, ABILITY_MULTITYPE), FALSE);
        EXPECT_EQ(opponent->species, SPECIES_ARCEUS_GHOST);
        uq4_12_t effectiveness;
        struct SimulatedDamage damage = AI_CalcDamageSaveBattlers(MOVE_JUDGMENT, 1, 0, &effectiveness, GIMMICK_NONE, GIMMICK_NONE);
        EXPECT_EQ(effectiveness, UQ_4_12(2.0));
        EXPECT_GT(damage.minimum, 0);
        EXPECT_EQ(opponent->item, ITEM_FIST_PLATE);
        EXPECT_EQ(opponent->hp, 37);
    }
}

SINGLE_BATTLE_TEST("Koanko Command Seal adaptation does not trigger for misses, immunity or Substitute damage")
{
    u32 source;
    PARAMETRIZE { source = 0; }
    PARAMETRIZE { source = 1; }
    PARAMETRIZE { source = 2; }
    GIVEN {
        AdaptFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(10); Moves(MOVE_FISSURE, MOVE_SHADOW_BALL, MOVE_SEISMIC_TOSS); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(100); MaxHP(100); Speed(20); Moves(MOVE_CELEBRATE, MOVE_SUBSTITUTE); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN {
            if (source == 0) MOVE(player, MOVE_FISSURE, hit: FALSE);
            if (source == 1) MOVE(player, MOVE_SHADOW_BALL);
            if (source == 2) MOVE(player, MOVE_SEISMIC_TOSS);
            MOVE(opponent, source == 2 ? MOVE_SUBSTITUTE : MOVE_CELEBRATE);
        }
    } SCENE {
        NOT ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_FORM_CHANGE, opponent);
    } THEN {
        EXPECT_EQ(gBattleStruct->koankoArceus.phase, KOANKO_ARCEUS_COVERED);
        EXPECT_EQ(gBattleStruct->koankoArceus.breakType, TYPE_NONE);
        EXPECT_EQ(opponent->species, SPECIES_ARCEUS_NORMAL);
        EXPECT_EQ(GetBreakBarCount(1), 1);
        EXPECT_EQ(opponent->hp, source == 2 ? 75 : 100);
    }
}

SINGLE_BATTLE_TEST("Koanko Command Seal adaptation permits genuine defeat and removes its owned field and form")
{
    GIVEN {
        AdaptFixture(3);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(10); Moves(MOVE_SEISMIC_TOSS, MOVE_NIGHT_SHADE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(100); MaxHP(100); Speed(1); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_NIGHT_SHADE); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_FORM_CHANGE, opponent);
        MESSAGE("The opposing Arceus fainted!");
    } THEN {
        EXPECT_EQ(gBattleOutcome, B_OUTCOME_WON);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 1);
        EXPECT_EQ(KoankoArceusGetForm(1), SPECIES_NONE);
        EXPECT_EQ(GetMonData(GetBattlerMon(1), MON_DATA_SPECIES), SPECIES_ARCEUS_NORMAL);
        EXPECT_EQ(gBattleStruct->bugSpace.active, FALSE);
    }
}

SINGLE_BATTLE_TEST("Koanko Command Seal cleanup is repeatable and preserves a foreign field and restored HP")
{
    GIVEN {
        AdaptFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(10); Moves(MOVE_SEISMIC_TOSS); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(100); MaxHP(100); Speed(1); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        ExpectAdapted(TYPE_FIGHTING, SPECIES_ARCEUS_GHOST, 37, 0);
        gBattleStruct->bugSpace.active = TRUE;
        gBattleStruct->bugSpace.mode = BUGSPACE_MODE_KAZURADROP;
        gBattleStruct->bugSpace.sourceBattler = 0;
        gBattleStruct->bugSpace.bpThreshold = 123;
        KoankoArceusFinishBattle();
        KoankoArceusFinishBattle();
        EXPECT_EQ(gBattleStruct->koankoArceus.phase, KOANKO_ARCEUS_DORMANT);
        EXPECT_EQ(opponent->species, SPECIES_ARCEUS_NORMAL);
        EXPECT_EQ(GetMonData(GetBattlerMon(1), MON_DATA_SPECIES), SPECIES_ARCEUS_NORMAL);
        EXPECT_EQ(opponent->hp, 37);
        EXPECT_EQ(gBattleStruct->bugSpace.active, TRUE);
        EXPECT_EQ(gBattleStruct->bugSpace.sourceBattler, 0);
        EXPECT_EQ(gBattleStruct->bugSpace.bpThreshold, 123);
    }
}

SINGLE_BATTLE_TEST("Koanko Command Seal break preflight reserves the adaptation before mutating the layer")
{
    GIVEN {
        AdaptFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(100); MaxHP(100); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        u32 stackSize = gBattleResources->battleScriptsStack->size;
        u32 targetCount = gBattleStruct->savedTargetCount;
        FixtureHp(0);
        KoankoArceusOnHpUpdate(1, 100, 0, TYPE_FIGHTING);
        EXPECT_EQ(QueueBreakBar(1), TRUE);
        gBattleResources->battleScriptsStack->size = ARRAY_COUNT(gBattleResources->battleScriptsStack->ptr) - 2;
        EXPECT_EQ(TryBreakBar(1, gBattlescriptCurrInstr), FALSE);
        EXPECT_EQ(gBattleStruct->koankoArceus.phase, KOANKO_ARCEUS_COVERED);
        EXPECT_EQ(GetBreakBarCount(1), 1);
        EXPECT_EQ(IsBreakBarPending(1), TRUE);
        EXPECT_EQ((u32)gBattleStruct->savedTargetCount, targetCount);
        EXPECT_EQ(KoankoArceusGetBreakScript(1, BREAK_BAR_MAX_EXTRA_BARS), NULL);
        gBattleResources->battleScriptsStack->size = stackSize;
        GetBattlerPartyState(1)->breakBarPending = FALSE;
        FixtureHp(100);
    }
}

SINGLE_BATTLE_TEST("Koanko Command Seal scarce animation tasks still reveal the actual form without corrupting tasks")
{
    GIVEN {
        AdaptFixture(4);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(10); Moves(MOVE_SEISMIC_TOSS); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(100); MaxHP(100); Speed(1); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        NOT ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_FORM_CHANGE, opponent);
    } THEN {
        ExpectAdapted(TYPE_FIGHTING, SPECIES_ARCEUS_GHOST, 37, 0);
        const u16 *pal = GetMonFrontSpritePal(GetBattlerMon(1));
        EXPECT_EQ(memcmp(&gPlttBufferUnfaded[OBJ_PLTT_ID(1)], pal, PLTT_SIZE_4BPP), 0);
        EXPECT_EQ(GetGpuReg(REG_OFFSET_MOSAIC), 0);
        for (u32 task = 0; task < NUM_TASKS; task++)
            if (sAdaptTasks & (1u << task))
                EXPECT_EQ(gTasks[task].func, AdaptNoopTask);
    } AFTER_CLEANUP {
        FreeAdaptTasks();
    }
}

SINGLE_BATTLE_TEST("Koanko Command Seal adaptation retains status and does not cure or heal it")
{
    GIVEN {
        AdaptFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(10); Moves(MOVE_SEISMIC_TOSS); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(100); MaxHP(100); Speed(1); Status1(STATUS1_POISON); Moves(MOVE_CELEBRATE);
            EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        ExpectAdapted(TYPE_FIGHTING, SPECIES_ARCEUS_GHOST, 25, 0);
        EXPECT_EQ(opponent->status1, STATUS1_POISON);
        EXPECT_EQ(GetMonData(GetBattlerMon(1), MON_DATA_STATUS), STATUS1_POISON);
    }
}

SINGLE_BATTLE_TEST("Koanko Command Seal form override leaves ordinary Arceus Judgment item typing unchanged")
{
    u16 item;
    PARAMETRIZE { item = ITEM_NONE; }
    PARAMETRIZE { item = ITEM_FIST_PLATE; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { HP(1000); MaxHP(1000); Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) { Speed(1); Item(item); Moves(MOVE_JUDGMENT); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_JUDGMENT); }
    } THEN {
        EXPECT_EQ(KoankoArceusGetForm(1), SPECIES_NONE);
        SetTypeBeforeUsingMove(MOVE_JUDGMENT, 1, ABILITY_MULTITYPE, item == ITEM_NONE ? HOLD_EFFECT_NONE : HOLD_EFFECT_PLATE);
        EXPECT_EQ(GetBattleMoveType(MOVE_JUDGMENT), item == ITEM_NONE ? TYPE_NORMAL : TYPE_FIGHTING);
    }
}
