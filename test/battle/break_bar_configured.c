#include "global.h"
#include "test/battle.h"
#include "battle_ai_main.h"
#include "battle_break_bar.h"
#include "battle_scripts.h"

// "Break Bar configured": a Break Bar phase table exists independently of how many of its bars are active.
// GrantConfiguredBreakBar() raises the active count one configured phase at a time (optionally covering the
// current HP/max HP), and every grant of phases[n] runs phases[n] again when it breaks. AUTHORED WITHOUT A BUILD.

extern AiScoreFunc sDynamicAiFunc;

// Sentinel addresses identify scheduling only; they are never interpreted as bytecode.
static const u8 sOn[BREAK_BAR_MAX_EXTRA_BARS] = {0};
static const u8 sAfter[BREAK_BAR_MAX_EXTRA_BARS] = {0};

#define SENTINEL_PHASES \
    { \
        {&sOn[0], &sAfter[0]}, {&sOn[1], &sAfter[1]}, {&sOn[2], &sAfter[2]}, \
        {&sOn[3], &sAfter[3]}, {&sOn[4], &sAfter[4]}, \
    }

static const struct BreakBarConfig sActive2 = {.extraBars = 2, .phases = SENTINEL_PHASES};
static const struct BreakBarConfig sInactive1 = {.extraBars = 1, .flags = BREAK_BAR_START_INACTIVE, .phases = SENTINEL_PHASES};
static const struct BreakBarConfig sInactive2 = {.extraBars = 2, .flags = BREAK_BAR_START_INACTIVE, .phases = SENTINEL_PHASES};
static const struct BreakBarConfig sInactive5 = {.extraBars = 5, .flags = BREAK_BAR_START_INACTIVE, .phases = SENTINEL_PHASES};
static const struct BreakBarConfig sActive5 = {.extraBars = 5, .phases = SENTINEL_PHASES};

struct BreakInspect
{
    const u8 *entry;
    const u8 *pushed[4];
    u32 pushedCount;
};

// Break the battler's top layer exactly as the engine would schedule it, record what was scheduled, and then
// discard the schedule so the test can keep driving the state (the same approach as break_bar.c).
static bool32 BreakAndInspect(u32 battler, struct BreakInspect *out)
{
    const u8 *savedScript = gBattlescriptCurrInstr;
    u32 savedTarget = gBattlerTarget;
    u32 stackSize = gBattleResources->battleScriptsStack->size;
    u32 targetCount = gBattleStruct->savedTargetCount;

    u32 savedHp = gBattleMons[battler].hp;

    gBattleMons[battler].hp = 0;
    bool32 broke = TryBreakBar(battler, savedScript);
    gBattleMons[battler].hp = savedHp; // The test, not a script, decides what HP the next step starts from.
    out->entry = gBattlescriptCurrInstr;
    out->pushedCount = gBattleResources->battleScriptsStack->size - stackSize;
    for (u32 i = 0; i < out->pushedCount && i < ARRAY_COUNT(out->pushed); i++)
        out->pushed[i] = gBattleResources->battleScriptsStack->ptr[stackSize + i];

    gBattleResources->battleScriptsStack->size = stackSize;
    gBattleStruct->savedTargetCount = targetCount;
    gBattlescriptCurrInstr = savedScript;
    gBattlerTarget = savedTarget;
    return broke;
}

SINGLE_BATTLE_TEST("Break Bar configured: a START_INACTIVE config registers with no active bars and keeps its phases")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { HP(60); MaxHP(100); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        u32 battler = B_POSITION_OPPONENT_LEFT;
        struct PartyState *owner = GetBattlerPartyState(battler);
        struct BreakInspect result;

        EXPECT_EQ(ActivateBreakBar(battler, &sInactive2), TRUE);
        EXPECT(owner->breakBarConfig == &sInactive2);
        EXPECT_EQ(GetBreakBarCount(battler), 0);
        EXPECT_EQ(CanInterceptBreakBar(battler), FALSE);
        EXPECT_EQ(IsBreakBarBase(battler), FALSE); // Never been active: still looks like an ordinary HP bar.
        EXPECT_EQ(BreakAndInspect(battler, &result), FALSE);
        opponent->hp = 60;

        EXPECT_EQ(GrantConfiguredBreakBar(battler, 0), TRUE);
        EXPECT_EQ(GetBreakBarCount(battler), 1);
        EXPECT_EQ(opponent->hp, 60); // A plain grant does not heal; the current HP bar becomes the layer.
        EXPECT_EQ(BreakAndInspect(battler, &result), TRUE);
        EXPECT(result.entry == &sOn[0]);
        EXPECT_EQ(result.pushedCount, 3);
        EXPECT(result.pushed[2] == &sAfter[0]);
        EXPECT(owner->breakBarConfig == &sInactive2); // The break did not consume or replace the config.
        EXPECT(owner->breakBarConfig->phases[0].onBreakScript == &sOn[0]);
        EXPECT_EQ(GetBreakBarCount(battler), 0);
        EXPECT_EQ(IsBreakBarBase(battler), TRUE);     // Now a registered red base, like any exhausted stack.
        opponent->hp = 60;
    }
}

SINGLE_BATTLE_TEST("Break Bar configured: a broken configured bar can be restored and runs the same scripts again")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        u32 battler = B_POSITION_OPPONENT_LEFT;
        struct PartyState *owner = GetBattlerPartyState(battler);
        struct BreakInspect result;

        EXPECT_EQ(ActivateBreakBar(battler, &sActive2), TRUE);
        EXPECT_EQ(GrantConfiguredBreakBar(battler, 0), FALSE); // Both configured bars are already active.

        EXPECT_EQ(BreakAndInspect(battler, &result), TRUE);    // Outer bar: phases[1].
        EXPECT(result.entry == &sOn[1]);
        EXPECT(result.pushed[2] == &sAfter[1]);
        EXPECT_EQ(GetBreakBarCount(battler), 1);

        EXPECT_EQ(GrantConfiguredBreakBar(battler, 0), TRUE);  // The outer bar is restored.
        EXPECT_EQ(GetBreakBarCount(battler), 2);
        EXPECT_EQ(BreakAndInspect(battler, &result), TRUE);    // Same script, second time.
        EXPECT(result.entry == &sOn[1]);
        EXPECT(result.pushed[2] == &sAfter[1]);

        EXPECT_EQ(BreakAndInspect(battler, &result), TRUE);    // Inner bar: phases[0], untouched by the repeats.
        EXPECT(result.entry == &sOn[0]);
        EXPECT(result.pushed[2] == &sAfter[0]);
        EXPECT_EQ(GetBreakBarCount(battler), 0);

        EXPECT_EQ(GrantConfiguredBreakBar(battler, 0), TRUE);  // The inner bar can be restored too and repeats.
        EXPECT_EQ(BreakAndInspect(battler, &result), TRUE);
        EXPECT(result.entry == &sOn[0]);
        EXPECT(owner->breakBarConfig == &sActive2);
    }
}

SINGLE_BATTLE_TEST("Break Bar configured: a covering grant saves the exact HP pair and orders restore before scripts")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { HP(37); MaxHP(100); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        u32 battler = B_POSITION_OPPONENT_LEFT;
        struct PartyState *owner = GetBattlerPartyState(battler);
        struct BreakInspect result;

        EXPECT_EQ(ActivateBreakBar(battler, &sInactive2), TRUE);
        EXPECT_EQ(GrantConfiguredBreakBar(battler, BREAK_BAR_GRANT_COVER), TRUE);
        EXPECT_EQ(GetBreakBarCount(battler), 1);
        EXPECT_EQ(owner->breakBarSavedHp[0], 37);
        EXPECT_EQ(owner->breakBarSavedMaxHp[0], 100);
        EXPECT_EQ((u32)owner->breakBarCoverScripts, 1);
        EXPECT_EQ(opponent->hp, 37); // Covering does not heal; the fill script raises the visible layer.

        // Second covering layer over a different (already filled) pair selects phases[1].
        opponent->hp = 100;
        EXPECT_EQ(GrantConfiguredBreakBar(battler, BREAK_BAR_GRANT_COVER), TRUE);
        EXPECT_EQ(owner->breakBarSavedHp[1], 100);
        EXPECT_EQ((u32)owner->breakBarCoverScripts, 3);

        EXPECT_EQ(BreakAndInspect(battler, &result), TRUE);
        EXPECT(result.entry == BattleScript_RestoreSavedBreakBar);
        EXPECT_EQ(result.pushedCount, 4); // caller, return, after, onBreak: LIFO = restore, onBreak, after, return.
        EXPECT(result.pushed[2] == &sAfter[1]);
        EXPECT(result.pushed[3] == &sOn[1]);
        EXPECT_EQ((u32)owner->breakBarCoverScripts, 1);

        EXPECT_EQ(BreakAndInspect(battler, &result), TRUE);
        EXPECT(result.entry == BattleScript_RestoreSavedBreakBar);
        EXPECT(result.pushed[2] == &sAfter[0]);
        EXPECT(result.pushed[3] == &sOn[0]);
        EXPECT_EQ((u32)owner->breakBarCoverScripts, 0);
    }
}

SINGLE_BATTLE_TEST("Break Bar configured: ordinary saved layers never run phase scripts")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { HP(50); MaxHP(100); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        u32 battler = B_POSITION_OPPONENT_LEFT;
        struct PartyState *owner = GetBattlerPartyState(battler);
        struct BreakInspect result;

        // Above the configured range.
        EXPECT_EQ(ActivateBreakBar(battler, &sActive2), TRUE);
        EXPECT_EQ(GrantSavedBreakBar(battler), TRUE);
        EXPECT_EQ(GetBreakBarCount(battler), 3);
        EXPECT_EQ(BreakAndInspect(battler, &result), TRUE);
        EXPECT(result.entry == BattleScript_RestoreSavedBreakBar);
        EXPECT_EQ(result.pushedCount, 2); // caller + return only
        EXPECT_EQ(BreakAndInspect(battler, &result), TRUE); // Configured layers underneath are unchanged.
        EXPECT(result.entry == &sOn[1]);
        EXPECT(result.pushed[2] == &sAfter[1]);

        // Inside the configured range: phases[0] exists but this layer is an ordinary saved one.
        opponent->hp = 50;
        owner->breakBarsRemaining = 0;
        EXPECT_EQ(ActivateBreakBar(battler, &sInactive2), TRUE);
        EXPECT_EQ(GrantSavedBreakBar(battler), TRUE);
        EXPECT_EQ((u32)owner->breakBarCoverScripts, 0);
        EXPECT_EQ(BreakAndInspect(battler, &result), TRUE);
        EXPECT(result.entry == BattleScript_RestoreSavedBreakBar);
        EXPECT_EQ(result.pushedCount, 2);

        // A configured phase can sit above an ordinary layer that occupies a lower position.
        opponent->hp = 50;
        EXPECT_EQ(GrantSavedBreakBar(battler), TRUE);                // position 0, ordinary
        EXPECT_EQ(GrantConfiguredBreakBar(battler, 0), TRUE);        // position 1, phases[1]
        EXPECT_EQ(GetBreakBarCount(battler), 2);
        EXPECT_EQ(BreakAndInspect(battler, &result), TRUE);
        EXPECT(result.entry == &sOn[1]);
        EXPECT_EQ(BreakAndInspect(battler, &result), TRUE);
        EXPECT(result.entry == BattleScript_RestoreSavedBreakBar);
        EXPECT_EQ(result.pushedCount, 2);
    }
}

SINGLE_BATTLE_TEST("Break Bar configured: grants respect occupied positions, the config, and the five bar cap")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        u32 battler = B_POSITION_OPPONENT_LEFT;
        struct PartyState *owner = GetBattlerPartyState(battler);

        EXPECT_EQ(GrantConfiguredBreakBar(MAX_BATTLERS_COUNT, 0), FALSE);
        EXPECT_EQ(GrantConfiguredBreakBar(battler, 0), FALSE); // No config at all.

        // Active five: no sixth layer of either kind.
        EXPECT_EQ(ActivateBreakBar(battler, &sActive5), TRUE);
        EXPECT_EQ(GrantConfiguredBreakBar(battler, 0), FALSE);
        EXPECT_EQ(GrantSavedBreakBar(battler), FALSE);
        owner->breakBarsRemaining = 0;

        // Inactive five: exactly five grants, in order.
        EXPECT_EQ(ActivateBreakBar(battler, &sInactive5), TRUE);
        for (u32 i = 0; i < BREAK_BAR_MAX_EXTRA_BARS; i++)
            EXPECT_EQ(GrantConfiguredBreakBar(battler, 0), TRUE);
        EXPECT_EQ(GrantConfiguredBreakBar(battler, 0), FALSE);
        EXPECT_EQ(GetBreakBarCount(battler), BREAK_BAR_MAX_EXTRA_BARS);
        owner->breakBarsRemaining = 0;

        // A position taken by an ordinary saved layer cannot be granted over.
        EXPECT_EQ(ActivateBreakBar(battler, &sInactive1), TRUE);
        EXPECT_EQ(GrantSavedBreakBar(battler), TRUE);
        EXPECT_EQ(GrantConfiguredBreakBar(battler, 0), FALSE);
        owner->breakBarsRemaining = 0;

        // A pending (empty) layer cannot be granted over either.
        EXPECT_EQ(ActivateBreakBar(battler, &sInactive2), TRUE);
        owner->breakBarPending = TRUE;
        EXPECT_EQ(GrantConfiguredBreakBar(battler, 0), FALSE);
        owner->breakBarPending = FALSE;
    }
}

SINGLE_BATTLE_TEST("Break Bar configured: cover bits are cleaned up with their layers and stay on their party member")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { HP(40); MaxHP(100); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_PICHU) { Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        u32 battler = B_POSITION_OPPONENT_LEFT;
        struct PartyState *owner = GetBattlerPartyState(battler);

        EXPECT_EQ(ActivateBreakBar(battler, &sInactive2), TRUE);
        EXPECT_EQ(GrantConfiguredBreakBar(battler, BREAK_BAR_GRANT_COVER), TRUE);
        EXPECT_EQ((u32)owner->breakBarCoverScripts, 1);
        // The bar belongs to its party slot; the reserve has no config and no cover bits.
        EXPECT(gBattleStruct->partyState[B_SIDE_OPPONENT][1].breakBarConfig == NULL);
        EXPECT_EQ((u32)gBattleStruct->partyState[B_SIDE_OPPONENT][1].breakBarCoverScripts, 0);

        // Exhausting the stack (as Kazuradrop's stack breaker script does) leaves no armed bit behind.
        const struct __attribute__((packed)) { u8 opcode; void (*func)(void); } cmd = {0, BS_ExhaustBreakBars};
        const u8 *savedInstr = gBattlescriptCurrInstr;
        u32 savedTarget = gBattlerTarget;
        gBattlerTarget = battler;
        gBattlescriptCurrInstr = (const u8 *)&cmd;
        BS_ExhaustBreakBars();
        gBattlescriptCurrInstr = savedInstr;
        gBattlerTarget = savedTarget;
        EXPECT_EQ(GetBreakBarCount(battler), 0);
        EXPECT_EQ((u32)owner->breakBarCoverScripts, 0);

        // A later ordinary saved grant at the same position must not inherit the old bit.
        EXPECT_EQ(GrantSavedBreakBar(battler), TRUE);
        EXPECT_EQ((u32)owner->breakBarCoverScripts, 0);
    }
}

// End to end through the real break path: the covered pair is restored first, then the configured scripts run.
static const struct BreakBarConfig sCoverNoScripts =
{
    .extraBars = 1, .flags = BREAK_BAR_START_INACTIVE | BREAK_BAR_DEFER_INITIAL_DRAW,
};
static const struct BreakBarConfig sCoverAfterRefill =
{
    .phases[0] = {.onBreakScript = NULL, .afterBreakScript = BattleScript_BreakBarRefill},
    .extraBars = 1, .flags = BREAK_BAR_START_INACTIVE | BREAK_BAR_DEFER_INITIAL_DRAW,
};

static const struct BreakBarConfig *sCoverSeedConfig;
static bool8 sCoverSeedPending;

static s32 CoverSeed(u32 attacker, u32 target, u32 move, s32 score)
{
    if (sCoverSeedPending)
    {
        sCoverSeedPending = FALSE;
        u32 battler = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);
        ActivateBreakBar(battler, sCoverSeedConfig);
        GrantConfiguredBreakBar(battler, BREAK_BAR_GRANT_COVER);
        gBattleMons[battler].hp = gBattleMons[battler].maxHP; // What the fill script does for the visible layer.
    }
    return score;
}

AI_SINGLE_BATTLE_TEST("Break Bar configured: a covering layer with no onBreakScript restores the covered HP without healing")
{
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC);
        sCoverSeedConfig = &sCoverNoScripts;
        sCoverSeedPending = TRUE;
        sDynamicAiFunc = CoverSeed;
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(100); Moves(MOVE_SEISMIC_TOSS); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(37); MaxHP(100); Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); }
    } THEN {
        EXPECT_EQ(opponent->hp, 37);     // NULL onBreakScript on a covering layer is "nothing", never a refill.
        EXPECT_EQ(opponent->maxHP, 100);
        EXPECT_EQ(GetBreakBarCount(B_POSITION_OPPONENT_LEFT), 0);
        EXPECT_EQ((u32)GetBattlerPartyState(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT))->breakBarCoverScripts, 0);
        EXPECT_EQ(gBattleResults.opponentFaintCounter, 0);
    }
}

AI_SINGLE_BATTLE_TEST("Break Bar configured: a covering layer runs its configured scripts after the covered HP is restored")
{
    GIVEN {
        AI_FLAGS(AI_FLAG_DYNAMIC_FUNC);
        sCoverSeedConfig = &sCoverAfterRefill;
        sCoverSeedPending = TRUE;
        sDynamicAiFunc = CoverSeed;
        PLAYER(SPECIES_WOBBUFFET) { Level(100); Speed(100); Moves(MOVE_SEISMIC_TOSS); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(37); MaxHP(100); Speed(1); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(player, MOVE_SEISMIC_TOSS); }
    } THEN {
        // Restore first (HP 37), then the configured refill runs: it can only reach 100 if it ran after the restore.
        EXPECT_EQ(opponent->hp, 100);
        EXPECT_EQ(opponent->maxHP, 100);
        EXPECT_EQ(GetBreakBarCount(B_POSITION_OPPONENT_LEFT), 0);
    }
}

SINGLE_BATTLE_TEST("Break Bar configured: CanGrantConfiguredBreakBar mirrors the grant checks without any effect")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { HP(37); MaxHP(100); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        u32 battler = B_POSITION_OPPONENT_LEFT;
        struct PartyState *owner = GetBattlerPartyState(battler);

        EXPECT_EQ(CanGrantConfiguredBreakBar(MAX_BATTLERS_COUNT), FALSE);
        EXPECT_EQ(CanGrantConfiguredBreakBar(battler), FALSE); // No config.
        EXPECT_EQ(ActivateBreakBar(battler, &sInactive1), TRUE);
        EXPECT_EQ(CanGrantConfiguredBreakBar(battler), TRUE);
        EXPECT_EQ(GetBreakBarCount(battler), 0);                // The check changed nothing.
        EXPECT_EQ((u32)owner->breakBarCoverScripts, 0);
        EXPECT_EQ(GrantConfiguredBreakBar(battler, BREAK_BAR_GRANT_COVER), TRUE);
        EXPECT_EQ(CanGrantConfiguredBreakBar(battler), FALSE);  // The only configured phase is in use.
    }
}

SINGLE_BATTLE_TEST("Break Bar configured: eligibility reports distinct failure reasons without mutation")
{
    static const struct BreakBarConfig invalid = {.extraBars = BREAK_BAR_MAX_EXTRA_BARS + 1};
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { HP(37); MaxHP(100); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        u32 battler = B_POSITION_OPPONENT_LEFT;
        struct PartyState *owner = GetBattlerPartyState(battler);
        EXPECT_EQ(GetConfiguredBreakBarGrantResult(MAX_BATTLERS_COUNT), BREAK_BAR_GRANT_NO_OWNER);
        EXPECT_EQ(GetConfiguredBreakBarGrantResult(battler), BREAK_BAR_GRANT_NO_CONFIG);
        EXPECT_EQ(ActivateBreakBar(battler, &sInactive2), TRUE);
        struct PartyState before = *owner;
        EXPECT_EQ(GetConfiguredBreakBarGrantResult(battler), BREAK_BAR_GRANT_READY);
        EXPECT_EQ(memcmp(owner, &before, sizeof(before)), 0);
        owner->breakBarPending = TRUE;
        opponent->hp = 0;
        EXPECT_EQ(GetConfiguredBreakBarGrantResult(battler), BREAK_BAR_GRANT_PENDING);
        owner->breakBarPending = FALSE;
        EXPECT_EQ(GetConfiguredBreakBarGrantResult(battler), BREAK_BAR_GRANT_NOT_ALIVE);
        opponent->hp = 37;
        gAbsentBattlerFlags |= 1u << battler;
        EXPECT_EQ(GetConfiguredBreakBarGrantResult(battler), BREAK_BAR_GRANT_NOT_ALIVE);
        gAbsentBattlerFlags &= ~(1u << battler);
        owner->breakBarConfig = &invalid;
        EXPECT_EQ(GetConfiguredBreakBarGrantResult(battler), BREAK_BAR_GRANT_INVALID_CONFIG);
        EXPECT_EQ(GrantConfiguredBreakBar(battler, 0), FALSE);
        EXPECT_EQ(GetBreakBarCount(battler), 0);
        owner->breakBarConfig = &sInactive2;
        owner->breakBarsRemaining = 2;
        EXPECT_EQ(GetConfiguredBreakBarGrantResult(battler), BREAK_BAR_GRANT_FULL);
        EXPECT_EQ(CanGrantConfiguredBreakBar(battler), FALSE);
    }
}
