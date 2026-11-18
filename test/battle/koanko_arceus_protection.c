#include "global.h"
#include "test/battle.h"
#include "battle_koanko.h"
#include "battle_entry_scripts.h"
#include "battle_script_native.h"
#include "battle_setup.h"
#include "task.h"
#include "constants/battle_script_commands.h"
#include "constants/trainers.h"

// Observe real lifecycle boundaries without adding IWRAM scratch or snapshots.
static EWRAM_DATA u8 sPhasesSeen[8] = {0};
static const struct BreakBarConfig sProtectionBar = {.extraBars = 1};

static void ObserveProtection(u8 taskId)
{
    if (gBattleStruct == NULL || gBattleOutcome != 0)
    {
        DestroyTask(taskId);
        return;
    }
    if (gBattleResults.battleTurnCounter < ARRAY_COUNT(sPhasesSeen))
        sPhasesSeen[gBattleResults.battleTurnCounter] |= 1u << gBattleStruct->koankoArceus.phase;
}

static void FinishProtectionFixture(void)
{
    u32 taskId = FindTaskIdByFunc(ObserveProtection);
    if (taskId != TASK_NONE)
        DestroyTask(taskId);
}

static void BS_ProtectionFixture(void)
{
    NATIVE_ARGS(u8 idle);
    gBattleStruct->koankoPhase = KOANKO_CONJURED_TEAM;
    // Initial-entry tests arm after initial send-out using Turn 0. Replacement
    // tests instead exercise the actual early SwitchInClearSetData hook.
    KoankoOnSwitchIn(1);
    if (cmd->idle == 1)
        gBattleMons[1].volatiles.rechargeTimer = 2;
    else if (cmd->idle == 2)
        gBattleMons[1].volatiles.timestop = 3;
    memset(sPhasesSeen, 0, sizeof(sPhasesSeen));
    if (cmd->idle == 3)
    {
        gBattleMons[1].volatiles.perishSong = TRUE;
        gBattleMons[1].volatiles.perishSongTimer = 0;
    }
    u32 taskId = CreateTask(ObserveProtection, 0);
    EXPECT_LT(taskId, NUM_TASKS);
    EXPECT(gTasks[taskId].isActive && gTasks[taskId].func == ObserveProtection);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

static const struct __attribute__((packed))
{
    u8 native;
    void (*func)(void);
    u8 idle;
    u8 ret;
} sProtectionOpenings[] =
{
    {B_SCR_OP_CALLNATIVE, BS_ProtectionFixture, 0, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_ProtectionFixture, 1, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_ProtectionFixture, 2, B_SCR_OP_RETURN},
    {B_SCR_OP_CALLNATIVE, BS_ProtectionFixture, 3, B_SCR_OP_RETURN},
};

static void ProtectionFixture(u32 idle)
{
    gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_KOANKO;
    TurnZeroScript(B_TRAINER_OPPONENT_A, (const u8 *)&sProtectionOpenings[idle]);
}

SINGLE_BATTLE_TEST("Koanko Arceus protection floors direct, OHKO and multihit damage without a faint or bar break")
{
    u32 move;
    bool32 originalBar;
    PARAMETRIZE { move = MOVE_SEISMIC_TOSS; originalBar = FALSE; }
    PARAMETRIZE { move = MOVE_FISSURE; originalBar = FALSE; }
    PARAMETRIZE { move = MOVE_ICICLE_SPEAR; originalBar = FALSE; }
    PARAMETRIZE { move = MOVE_SEISMIC_TOSS; originalBar = TRUE; }
    PARAMETRIZE { move = MOVE_FISSURE; originalBar = TRUE; }
    PARAMETRIZE { move = MOVE_ICICLE_SPEAR; originalBar = TRUE; }
    GIVEN {
        ProtectionFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { Level(MAX_LEVEL); Speed(2); Attack(9999); Ability(ABILITY_SKILL_LINK); Moves(move); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            Level(MAX_LEVEL); Speed(1); HP(50); MaxHP(100); Moves(MOVE_CELEBRATE);
            EntryScript(&gKoankoArceusProtectionEntry);
            if (originalBar) BreakBars(&sProtectionBar);
        }
    } WHEN {
        TURN { MOVE(player, move); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        HP_BAR(opponent, hp: 1);
        NOT MESSAGE("The opposing Arceus fainted!");
    } THEN {
        EXPECT_EQ(opponent->hp, 1);
        EXPECT_EQ(GetMonData(GetBattlerMon(1), MON_DATA_HP), 1);
        EXPECT_EQ(opponent->ability, ABILITY_MULTITYPE);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
        EXPECT_EQ(GetBreakBarCount(1), originalBar ? 1 : 0);
        EXPECT(KoankoArceusOpeningReady(1));
        EXPECT(KoankoArceusIsProtected(1)); // Until a successful saved-layer handoff.
    } AFTER_CLEANUP {
        FinishProtectionFixture();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus protection floors poison, burn and weather at one HP")
{
    u32 source;
    PARAMETRIZE { source = 0; }
    PARAMETRIZE { source = 1; }
    PARAMETRIZE { source = 2; }
    GIVEN {
        ProtectionFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { Moves(source == 2 ? MOVE_SANDSTORM : MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(1); MaxHP(160); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry);
            Status1(source == 0 ? STATUS1_POISON : source == 1 ? STATUS1_BURN : STATUS1_NONE);
        }
    } WHEN {
        TURN { MOVE(player, source == 2 ? MOVE_SANDSTORM : MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(opponent->hp, 1);
        EXPECT_EQ(GetMonData(GetBattlerMon(1), MON_DATA_HP), 1);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
        EXPECT(KoankoArceusOpeningReady(1));
    } AFTER_CLEANUP {
        FinishProtectionFixture();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus protection survives an expiring Perish Song")
{
    GIVEN {
        ProtectionFixture(3);
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(50); MaxHP(100); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry);
            BreakBars(&sProtectionBar);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(opponent->hp, 1);
        EXPECT_EQ(GetMonData(GetBattlerMon(1), MON_DATA_HP), 1);
        EXPECT_EQ((u32)opponent->volatiles.perishSong, FALSE);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
        EXPECT_EQ(GetBreakBarCount(1), 1);
        EXPECT(KoankoArceusOpeningReady(1));
    } AFTER_CLEANUP {
        FinishProtectionFixture();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus protection covers delayed attacks while saved-layer handoff is pending")
{
    GIVEN {
        ProtectionFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { SpAttack(9999); Moves(MOVE_FUTURE_SIGHT, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) { HP(1); MaxHP(100); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry); }
    } WHEN {
        TURN { MOVE(player, MOVE_FUTURE_SIGHT); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } SCENE {
        MESSAGE("The opposing Arceus took the Future Sight attack!");
        NOT MESSAGE("The opposing Arceus fainted!");
    } THEN {
        EXPECT_EQ(opponent->hp, 1);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
        EXPECT(KoankoArceusOpeningReady(1));
    } AFTER_CLEANUP {
        FinishProtectionFixture();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus protection survives Explosion, Memento and Final Gambit without healing")
{
    u32 move;
    PARAMETRIZE { move = MOVE_EXPLOSION; }
    PARAMETRIZE { move = MOVE_MEMENTO; }
    PARAMETRIZE { move = MOVE_FINAL_GAMBIT; }
    GIVEN {
        ProtectionFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { Speed(1); HP(9999); MaxHP(9999); Defense(9999); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            Speed(2); HP(50); MaxHP(100); Moves(move); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(opponent, move); MOVE(player, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(opponent->hp, 1);
        EXPECT_EQ(GetMonData(GetBattlerMon(1), MON_DATA_HP), 1);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
        EXPECT_EQ(GetBreakBarCount(1), 0);
    } AFTER_CLEANUP {
        FinishProtectionFixture();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus protection survives Destiny Bond after a genuine player faint")
{
    GIVEN {
        ProtectionFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { HP(1); Speed(2); Moves(MOVE_DESTINY_BOND); }
        PLAYER(SPECIES_ABRA) { Speed(2); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            Speed(1); HP(50); MaxHP(100); Moves(MOVE_SEISMIC_TOSS); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_DESTINY_BOND); MOVE(opponent, MOVE_SEISMIC_TOSS); SEND_OUT(player, 1); }
    } THEN {
        EXPECT_EQ(opponent->hp, 1);
        EXPECT_EQ(gBattleResults.playerFaintCounter, 1);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
        EXPECT_EQ(player->species, SPECIES_ABRA);
        EXPECT(KoankoArceusIsProtected(1));
    } AFTER_CLEANUP {
        FinishProtectionFixture();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus protection counts a fully selected idle or Time Stopped turn")
{
    u32 idle;
    PARAMETRIZE { idle = 0; }
    PARAMETRIZE { idle = 1; }
    PARAMETRIZE { idle = 2; }
    GIVEN {
        ProtectionFixture(idle);
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) { Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry); }
    } WHEN {
        TURN {
            MOVE(player, MOVE_CELEBRATE);
            if (idle == 0) MOVE(opponent, MOVE_CELEBRATE);
            else SKIP_TURN(opponent);
        }
    } THEN {
        EXPECT(sPhasesSeen[0] & (1u << KOANKO_ARCEUS_SELECTED_TURN));
        EXPECT(KoankoArceusOpeningReady(1));
        EXPECT(KoankoArceusIsProtected(1));
    } AFTER_CLEANUP {
        FinishProtectionFixture();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus protection cannot prevent ordinary player loss")
{
    GIVEN {
        ProtectionFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { Speed(1); HP(1); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            Speed(2); Moves(MOVE_SEISMIC_TOSS); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(opponent, MOVE_SEISMIC_TOSS); MOVE(player, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(gBattleOutcome, B_OUTCOME_LOST);
        EXPECT_EQ(gBattleResults.playerFaintCounter, 1);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
        EXPECT_GT(opponent->hp, 0);
    } AFTER_CLEANUP {
        FinishProtectionFixture();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus protection arms before replacement hazards and waits through a full following turn")
{
    GIVEN {
        ProtectionFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(2); Moves(MOVE_SPIKES, MOVE_SEISMIC_TOSS); }
        OPPONENT(SPECIES_WOBBUFFET) { Speed(1); HP(1); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            Speed(1); HP(1); MaxHP(160); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_SPIKES); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); SEND_OUT(opponent, 1); }
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(opponent->hp, 1);
        EXPECT_EQ(opponent->species, SPECIES_ARCEUS_NORMAL);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 1); // Only the old Wobbuffet.
        EXPECT_EQ(GetBattlerPartyState(1)->entryScriptRuns, 1); // Survived hazards to reach On Entry.
        EXPECT((sPhasesSeen[1] | sPhasesSeen[2]) & (1u << KOANKO_ARCEUS_WAIT_SELECTION));
        EXPECT_EQ(sPhasesSeen[1] & (1u << KOANKO_ARCEUS_OPENING_READY), 0);
        EXPECT(sPhasesSeen[2] & (1u << KOANKO_ARCEUS_SELECTED_TURN));
        EXPECT_EQ(sPhasesSeen[2] & (1u << KOANKO_ARCEUS_OPENING_READY), 0);
        EXPECT(KoankoArceusOpeningReady(1));
    } AFTER_CLEANUP {
        FinishProtectionFixture();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus protection only releases to a full saved layer and retains existing covered HP")
{
    bool32 existing;
    PARAMETRIZE { existing = FALSE; }
    PARAMETRIZE { existing = TRUE; }
    GIVEN {
        ProtectionFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) {
            HP(37); MaxHP(100); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry);
            if (existing) Item(ITEM_BREAK_CHARM);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT(KoankoArceusOpeningReady(1));
        EXPECT_EQ(KoankoArceusFinishOpening(1, 0), FALSE);
        EXPECT_EQ(opponent->hp, existing ? 100 : 37);
        u32 layer = GetBreakBarCount(1);
        EXPECT(GrantSavedBreakBar(1));
        if (!existing) EXPECT_EQ(KoankoArceusFinishOpening(1, layer), FALSE);
        EXPECT(KoankoArceusIsProtected(1));
        // The choreography will wait for the real fill/animation. This fixture
        // verifies the commit gate only, without running a fake healing effect.
        opponent->hp = opponent->maxHP;
        SetMonData(GetBattlerMon(1), MON_DATA_HP, &opponent->hp);
        EXPECT(KoankoArceusFinishOpening(1, layer));
        EXPECT_EQ(KoankoArceusIsProtected(1), FALSE);
        EXPECT_EQ(KoankoArceusClampHpLoss(1, 1000), 1000);
        EXPECT_EQ(gBattleStruct->koankoArceus.savedLayer, layer);
        EXPECT_EQ(GetBattlerPartyState(1)->breakBarSavedHp[0], 37);
        EXPECT_EQ(GetBreakBarCount(1), existing ? 2 : 1);
        EXPECT_EQ(KoankoArceusFinishOpening(1, layer), FALSE); // One handoff.
    } AFTER_CLEANUP {
        FinishProtectionFixture();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus protection stays off without explicit entry configuration")
{
    GIVEN {
        ProtectionFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { Speed(2); Moves(MOVE_SEISMIC_TOSS); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) { Speed(1); HP(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(gBattleOutcome, B_OUTCOME_WON);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 1);
        EXPECT_EQ(gBattleStruct->koankoArceus.phase, KOANKO_ARCEUS_DORMANT);
    } AFTER_CLEANUP {
        FinishProtectionFixture();
    }
}

static void BS_ZeroProtectedArceus(void)
{
    NATIVE_ARGS();
    gBattleMons[1].hp = 0;
    SetMonData(GetBattlerMon(1), MON_DATA_HP, &gBattleMons[1].hp);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

static const struct __attribute__((packed))
{
    u8 native;
    void (*setup)(void);
    u8 idle;
    u8 zero;
    void (*writeZero)(void);
    u8 faint;
    u8 battler;
    bool8 isSpikes;
    const u8 *jump;
    u8 ret;
} sZeroOpening = {B_SCR_OP_CALLNATIVE, BS_ProtectionFixture, 0,
                  B_SCR_OP_CALLNATIVE, BS_ZeroProtectedArceus,
                  B_SCR_OP_TRYFAINTMON, BS_OPPONENT1, FALSE, NULL, B_SCR_OP_RETURN};

SINGLE_BATTLE_TEST("Koanko Arceus protection repairs a direct zero before faint counters, bars or rewards")
{
    GIVEN {
        ProtectionFixture(0);
        TurnZeroScript(B_TRAINER_OPPONENT_A, (const u8 *)&sZeroOpening);
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) { HP(50); MaxHP(100); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(opponent->hp, 1);
        EXPECT_EQ(GetMonData(GetBattlerMon(1), MON_DATA_HP), 1);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
        EXPECT_EQ(gHitMarker & HITMARKER_FAINTED(1), 0);
        EXPECT_EQ(GetBreakBarCount(1), 0);
        EXPECT(KoankoArceusOpeningReady(1));
    } AFTER_CLEANUP {
        FinishProtectionFixture();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus protection validates trainer and species ownership before arming")
{
    bool32 otherTrainer;
    PARAMETRIZE { otherTrainer = TRUE; }
    PARAMETRIZE { otherTrainer = FALSE; }
    GIVEN {
        ProtectionFixture(0);
        if (otherTrainer)
            gBattleTestRunnerState->data.recordedBattle.opponentA = TRAINER_RAZELUXE;
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(otherTrainer ? SPECIES_ARCEUS_NORMAL : SPECIES_WOBBUFFET) {
            HP(50); MaxHP(100); Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry);
        }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(gBattleStruct->koankoArceus.phase, KOANKO_ARCEUS_DORMANT);
        EXPECT_EQ(KoankoArceusClampHpLoss(1, 1000), 1000);
    } AFTER_CLEANUP {
        FinishProtectionFixture();
    }
}

SINGLE_BATTLE_TEST("Koanko Arceus protection does not transfer to another party occupant or rearm")
{
    GIVEN {
        ProtectionFixture(0);
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_ARCEUS_NORMAL) { Moves(MOVE_CELEBRATE); EntryScript(&gKoankoArceusProtectionEntry); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        EXPECT(KoankoArceusIsProtected(1));
        EXPECT_EQ(KoankoArceusIsProtected(0), FALSE);
        EXPECT_EQ(KoankoArceusClampHpLoss(1, -10), -10); // Ordinary healing remains available.
        u32 phase = gBattleStruct->koankoArceus.phase;
        u32 oldAbsent = gAbsentBattlerFlags;
        gAbsentBattlerFlags |= 1u << 1;
        KoankoArceusOnSwitchIn(1); // Transient absence does not change party identity.
        gAbsentBattlerFlags = oldAbsent;
        EXPECT_EQ(gBattleStruct->koankoArceus.phase, phase);
        // Exercise identity retirement without authoring a disallowed voluntary
        // boss switch. Restore the active index before normal fixture cleanup.
        u32 oldSlot = gBattlerPartyIndexes[1];
        gBattlerPartyIndexes[1] = 1;
        EXPECT_EQ(KoankoArceusIsProtected(1), FALSE);
        KoankoArceusOnSwitchIn(1);
        gBattlerPartyIndexes[1] = oldSlot;
        KoankoArceusOnSwitchIn(1);
        EXPECT_EQ(gBattleStruct->koankoArceus.phase, KOANKO_ARCEUS_FINISHED);
        EXPECT_EQ(KoankoArceusIsProtected(1), FALSE);
    } AFTER_CLEANUP {
        FinishProtectionFixture();
    }
}
