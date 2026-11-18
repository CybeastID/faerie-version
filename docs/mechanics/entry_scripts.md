# Turn 0 and On Entry battle scripts

Implemented on top of `e1ccaf3364` (Break Bar presentation update). User-run
verification: **Entry scripts 16/16 passed** after the final harness-flag
correction; **Break Bar exit HP 4/4 passed** after buffer relocation. The earlier
Trainer Break Bar run passed 11/11. No production encounter is configured.
The user subsequently reports the broader Break Bar, Koanko Zacian and Trainer
Slide suites passed. More Wishes had one failure: an outdated expectation that
Castoria's Revivify would revive another ally after TX's defeat. That test now
asserts the existing caster-level block, and the user reports its focused rerun
passed. The requested automated regression batch is green. mGBA
acceptance remains pending; no gameplay code changed for this test correction.

Both RECORDED and RECORDED_LINK share a test-harness-only exemption: synthetic
non-AI trainer fixtures use both flags, while AI/wild fixtures also use recorded
playback. Real link battles and production replay remain blocked. Initial build
declaration/enum/assertion errors and the missing Speed fixture are corrected.

The new pending-configuration and test-observation buffers now use EWRAM.
Read-only ELF inspection of the allocator-failing build found 388 bytes of these
buffers in IWRAM and only 2,532 bytes between the end of static storage
(`0x0300745c`) and normal stack start (`0x03007e40`). The allocator failure did not
recur in the subsequent focused 4/4 run after relocation. Stack-pressure
involvement remains a diagnosis supported by that result, not a measured stack
collision or a completed broad regression run.

## Configuration and ownership

`include/battle_entry_scripts.h` defines the interface. Runtime scheduling and
trainer generation handoff live in `src/battle_entry_scripts.c`. Author static
trainer defaults in the sparse `src/data/trainer_battle_scripts.h` table. Do not
edit generated trainer data or saved Pokemon structures.

Entry scripts are authored for opposing Trainers and wild Pokémon. Player-side
support is permitted by the underlying API but is not currently a supported
encounter-authoring use case. The owner model and tests may exercise player-side
capabilities without making them supported encounter-authoring patterns.

An On Entry configuration contains a returning battle-script pointer and either
`ENTRY_SCRIPT_ONCE_PER_BATTLE` (the default, zero) or
`ENTRY_SCRIPT_EVERY_ENTRY`. The pointer and saturating execution count belong to
`PartyState[trainer][slot]`. `GetBattlerPartyState` resolves independent owners,
including the temporary ally's partner row. Switching, ability/form changes and
graphics reconstruction retain this state. Regenerating a trainer party clears
the replaced owner's configurations/counts; other owners are unaffected. Reviving
the same mon retains its once-only state.

A trainer configuration contains one optional `turnZeroScript` and optional
entry rules. Source-entry rules use the zero-based source party/pool index, not
the final selected slot. They override exact-species rules, including an explicit
NULL config. Prefer unique selector/value pairs: the first matching source rule
wins, otherwise the last matching species rule wins. Forms match exact species.
Missing difficulty configurations inherit Normal; an explicit
`&gEmptyTrainerBattleScripts` suppresses that fallback. A configured difficulty
replaces the whole Normal configuration. Invalid IDs/difficulties return empty.

All configuration objects, nested rules and scripts must remain valid throughout
the battle. Use file-scope const objects. `INLINE_TRAINER_BATTLE_SCRIPTS` creates
compound literals and is intended for file-scope initializers. For an opening
without entry rules, use a named `TrainerBattleScripts` with only
`.turnZeroScript` instead of an empty variadic rule list.

Trainer parties are generated before `BattleStruct` allocation on the overworld
path. Pending pointers are recorded against the actual `gParties` owner and
consumed once during battle initialization. The party-generation adapter scopes
its rules around the existing Break Bar adapter, including copied party pools.
Raw `CreateNPCTrainerPartyFromTrainer` calls do not inherit a previous encounter's
rules. Bespoke team replacements can use the scoped adapter with their desired
trainer ID, or explicitly assign fresh `PartyState.entryScript` pointers.

Turn 0 pointers and done bits belong to `BattleStruct`, separately from the
per-mon configurations. There is one optional opening per independent trainer
owner. Each is consumed once, using the owner's first living active battler as
its script subject; an owner without one is skipped. An ordinary encounter has
one opponent opening. Multiple configured owners run in trainer-index order:
player, opponent A, partner, opponent B.

## Execution order

Initial send-outs, healthboxes and their controller waits finish first. The
existing pre-turn overworld weather, terrain, starting statuses and totem boosts
run next. The shared entry pipeline then performs:

1. Configured Break Bar entrance presentation and speed ordering.
2. Tera Shift, Neutralizing Gas and Unnerve.
3. Per-mon Name Theft, Healing Wish/Lunar Dance, Break Charm, entry hazards,
   general switch-in abilities (including Intimidate), immunity checks and items.
4. Form-change abilities, weather/terrain/ally reactions and Booster Energy.
5. White Herb, Opportunist and Mirror Herb.
6. Configured On Entry scripts, in the established entry speed order, only for
   living battlers whose `battlerState.switchIn` flag is set.
7. Normal clearing of entry flags and Eject Pack processing.

Later switches, replacements, simultaneous entries and existing specialized
arrivals use this same pipeline. A mon that faints to hazards before step 6 does
not run or consume its script. Emergency Exit before that stage can replace the
entrant; the replacement runs through its own normal events. Eject Pack occurs
after the configured payload, so an entrant may run its script before ejecting.
Payload changes do not replay prior abilities, hazards or items automatically.
Weather/terrain changes should call their existing returning reaction helpers.

At battle opening, the existing initial faint/replacement handling, opening
Trainer Slides and Castoria check follow entry processing. Configured Turn 0
scripts run **after those stages**, followed by another ordinary faint pass and
then the first command selection. Thus initial On Entry precedes Turn 0. These
scripts never advance the turn/action cursor or spend PP. A nonzero battle
outcome routes out of the opening state machine before command selection.
For configured openings, a small adapter temporarily presents an exhausted
action queue to Gen 4+ faint replacement handling, then restores the original
cursor. Otherwise a script-caused opening KO could defer replacement until an
ordinary action turn that has not started. This adapter retains its cursor across
replacement callbacks and restores it on a terminal outcome as well.

## Returning script contract

Payloads must terminate with **`return`**. Do not use `end`, `end2`, `end3`,
`finishaction`, `finishturn`, ordinary move-end processing or a jump into a
complete move-effect script. Those paths belong to an ordinary action or a
different callback owner. Returning subroutines may be called normally.

The scheduler starts attacker, target, effect battler and scripting battler at
the subject mon. Choose any other targets explicitly. It starts current/chosen
move at `MOVE_NONE`; effect commands that require a move must set one explicitly.
It preserves the enclosing scripting structure, subject globals, move IDs/slots,
action ID, item/ability message context, communication bytes, damage/result
scratch, entry speed arrays and Trainer Slide message pointer. Real combat
changes (HP, status, forms, stats, weather, terrain, faint bits) remain in effect.
The fixed entry order survives a payload that calls a weather helper which sorts
battlers again.

The two returning wrappers use existing interpreter calls, controller gating and
wait commands. Both reserve two script-stack slots; Turn 0 also reserves one
callback slot. Nested author calls and phase scripts need additional room
in the existing eight-slot script stack. Insufficient wrapper capacity rejects
dispatch without changing context; it is not deferred to another turn. Normal
call-depth discipline still applies inside authored bytecode.

The entry cursor advances, and the once count/done bit commits, before yielding.
Menus, pauses, controller callbacks and Break Bar phases cannot restart the hook.
On return, pending bars are resolved with returning `TryBreakBar` scripts before
context restoration. Real fainting uses the enclosing switch/opening machinery,
as entry hazards do; a damaging payload must use normal HP/controller updates and
`tryfaintmon`, rather than only writing zero to a raw HP field. Use supported form,
status and stat helpers so party data, controllers and battle state agree.

Payloads must not initiate another switch/entry pass, summon, party regeneration,
or replace the battle callback themselves. The scheduler has one non-reentrant
context, not a queue of arbitrary battle lifecycle operations. It can call
returning effects and presentation helpers, including compatible natives.

Graphics-only cleanup leaves the running context and execution counts intact.
`FreeBattleResources` clears pending configuration, the transient context and
overworld overrides on normal completion and abort, then frees battle-owned
storage. There are no independently scheduled callback tasks to carry into the
next encounter. Configuration is local-encounter only: link, actual recorded
playback and Safari hooks are skipped. The local battle-test harness is allowed
through both recorded flags because it replays synthetic parties with freshly
installed test configurations. Non-AI trainer fixtures use RECORDED_LINK as well
as RECORDED; that flag alone does not mean a real link connection in the harness.
Configured encounters disable battle recording
because replay does not serialize these pointers. Unconfigured encounters retain
the existing recording behavior.

## Example A: trainer opening with dialogue and weather

Add these labels to `data/battle_scripts_1.s` (the existing file already includes
the battle macros/constants). This example uses existing returning Trainer Slide
and weather helpers, never an ordinary move terminal script:

```asm
BattleScript_RoxanneOpening::
    setentrytrainerslidemessage RoxanneOpeningText
    call BattleScript_TrainerASlideMsgRet
    sethword gCurrentMove, MOVE_RAIN_DANCE
    setfieldweather
    call BattleScript_MoveWeatherChangeRet
    return

RoxanneOpeningText:
    .string "Let's see how you handle the rain!$"
```

Declare and configure it in `src/data/trainer_battle_scripts.h`, before/inside the
existing table respectively:

```c
extern const u8 BattleScript_RoxanneOpening[];
static const struct TrainerBattleScripts sRoxanneOpening =
{
    .turnZeroScript = BattleScript_RoxanneOpening,
};

// Add this initializer INSIDE sTrainerBattleScripts, not as a second table:
[TRAINER_ROXANNE_1] =
{
    [DIFFICULTY_NORMAL] = &sRoxanneOpening,
    [DIFFICULTY_HARD] = &gEmptyTrainerBattleScripts,
},
```

This uses opponent A's returning slide helper. An opponent B opening should use
`BattleScript_TrainerBSlideMsgRet`; use the partner helper for that owner. The
normal pre-first-turn slide, if separately configured, still happens earlier.

For an encounter-specific override, define an exported const configuration
(without `static`) and place `settrainerbattlescripts TRAINER_ROXANNE_1, ConfigName`
before the encounter's existing `trainerbattle` command. Two trainer-ID overrides
can coexist; a matching override, even empty, replaces defaults for that initial
party generation. The native reports success in `VAR_RESULT`. Overrides are
consumed by trainer ID and cleared when the initiating script stops or the battle
initializes/ends. They do not arm unrelated later encounters.

## Example B: scripted wild introduction

Add these labels to `data/battle_scripts_1.s`:

```asm
BattleScript_MewOpening::
    setentrytrainerslidemessage MewOpeningText
    printstring STRINGID_TRAINERSLIDE
    waitmessage B_WAIT_TIME_LONG
    playanimation BS_SCRIPTING, B_ANIM_RAIN_CONTINUES
    waitanimation
    sethword gCurrentMove, MOVE_RAIN_DANCE
    setfieldweather
    call BattleScript_MoveWeatherChangeRet
    return

MewOpeningText:
    .string "Mew watches the gathering clouds.$"
```

Define an exported object in `src/data/trainer_battle_scripts.h` before the table:

```c
extern const u8 BattleScript_MewOpening[];
const struct WildBattleScripts gMewIntroScripts =
{
    .turnZeroScript = BattleScript_MewOpening,
};
```

Use it from an overworld event:

```asm
    setwildbattlescripts gMewIntroScripts
    setwildbattle SPECIES_MEW, 50, ITEM_NONE
    dowildbattle
    waitstate
```

`dowildbattle` arms the handoff. A random encounter before that command cannot
consume an unarmed config. The first opponent party member receives `onEntry`
when present; the opening uses the same ordinary wild participant. Double-wild
encounters currently expose configuration for opponent slot 0 only. An optional
wild `.onEntry = {ScriptLabel, ENTRY_SCRIPT_ONCE_PER_BATTLE}` uses the same entry
contract as trainers. No trainer flag, reserved actor or special action is added.

## Example C: repeatable mon entry

Add the scripts to `data/battle_scripts_1.s`:

```asm
BattleScript_AliceRepeatEntry::
    jumpifentryscriptreentry BS_SCRIPTING, BattleScript_AliceReentry
    setentrytrainerslidemessage AliceFirstEntryText
    goto BattleScript_AliceEntryPrint
BattleScript_AliceReentry:
    setentrytrainerslidemessage AliceReentryText
BattleScript_AliceEntryPrint:
    printstring STRINGID_TRAINERSLIDE
    waitmessage B_WAIT_TIME_LONG
    return
AliceFirstEntryText:
    .string "A familiar presence enters the field.$"
AliceReentryText:
    .string "That presence returns once more.$"
```

Add a declaration before the sparse table and the initializer inside it:

```c
extern const u8 BattleScript_AliceRepeatEntry[];

[TRAINER_ALICE] =
{
    [DIFFICULTY_NORMAL] = INLINE_TRAINER_BATTLE_SCRIPTS(NULL,
        {ENTRY_SCRIPT_SOURCE_ENTRY, 1, &(const struct MonEntryScript)
            {BattleScript_AliceRepeatEntry, ENTRY_SCRIPT_EVERY_ENTRY}},
    ),
},
```

The second source party/pool entry gets this behavior, including after withdrawal
and reentry. Change the policy to `ENTRY_SCRIPT_ONCE_PER_BATTLE` for a one-time
introduction. `jumpifentryscriptreentry` checks whether that mon has already
dispatched more than once; counts saturate at 255. The state resets on genuine
party regeneration, not forms, switches or revival. Custom returning natives may
read `GetBattlerPartyState(battler)->entryScriptRuns` for further battle-local
variation. These counts describe scripted executions, not all historical entries.

An exact initial-deployment versus later-first-entry mode is not implemented:
replacement and Eject Pack events can happen before the first menu, so battle
turn zero alone is not a reliable initial-deployment discriminator. First versus
repeat execution is supported without adding another entry-lifecycle hook.

## TX ambush audit and compatibility

TX registers its static default through `TxRollbackProfile.ambushScript`; the
battle-owned profile pointer can also be overridden. `BS_TxRollbackFinishEntry`
transitions the summon lifecycle, and `HandleAction_ActionFinished` calls
`TxRollbackTryAmbush` after pending bars and before advancing the action cursor.
The lifecycle commits AMBUSH_RUNNING before dispatch. The ambush wrapper uses
`BattleScriptPushCursorAndCallback`, calls the returning profile payload and
finishes with `end3`. Its finish native supplies a separate continuation callback
for execute-style bar/faint scripts, then restores action context. Outcome or a
genuine pre-dispatch faint cancels the one-shot event.

That lifecycle depends on the summoned right-enemy position, Castoria's pending
action and asymmetric formation. It is not an external generic registration API.
The new scheduler reuses the underlying interpreter call/callback primitives and
the returning-payload convention, not TX's action cursor, summon stages or
specialized faint continuation. No TX source or existing payload is changed.
Break Bar grants remain independent configuration/effects. Koanko regeneration
already clears PartyState; temporary allies already use independent owners.

## Validation and mGBA acceptance

Run from this repository in WSL, keeping the whole current working tree stable
(base `e1ccaf3364` plus this uncommitted feature):

```sh
make check TESTS="Entry scripts:"
make check TESTS="Trainer Break Bar"
make check TESTS="Break Bar"
make check TESTS="Arcanum Wish More Wishes"
make check TESTS="Koanko Zacian"
make check TESTS="Trainer Slide"
```

`entry_scripts_validation.sha256` records the feature's source and documentation
snapshot. Check it with `sha256sum -c docs/mechanics/entry_scripts_validation.sha256`
before validation if another session may have changed this checkout. Keep the
other engine sources on the same base revision as well.

The focused allocator/entry suites and the reported broader suites have passed,
including the corrected More Wishes assertion. Its isolated regression command is:

```sh
make check TESTS="Arcanum Wish More Wishes retires its defeated slot"
```

The isolated checks, if needed, are:

```sh
make check TESTS="Break Bar exit HP"
make check TESTS="Entry scripts:"
```

If an allocator assertion returns, capture the first failure and stop the loop.

The focused source is `test/battle/entry_scripts.c`. Authored coverage includes
opening order/PP, once/every reentry, faint replacement ownership, scripted
pre-menu KOs (initial entry and Turn 0), Intimidate/Charm ordering, ordinary
trainer/wild behavior, four independent owners, nested
calls/controller waits, menu reconstruction, bar resolution, stack-capacity
rejection, source-rule resolution, pending trainer/wild handoff and cancellation.
The existing TX/Break Bar/ally/slide suites cover their behavior. Actual results
and remaining validation are recorded at the top of this guide; distinguish
existing baseline failures when rerunning.
Script-caused terminal KOs, live Trainer Slide choreography and temporary ally
entry scripts specifically still need manual/integration acceptance.

For visual acceptance, temporarily author the examples in a disposable working
copy, build using the project's usual ROM command, then in mGBA:

1. Start the trainer encounter: send-outs, bar intro/Charm/ability messages and
   normal slides precede the new introduction; rain starts before the first menu.
2. Play the first turn: each chosen participant acts once and spends only its
   chosen move's PP. Return from party/Bag/summary; no introduction repeats.
3. Have the configured opposing mon leave and reenter twice, for example through
   the trainer's switching logic. Confirm first/repeat messages
   for EVERY_ENTRY, and a single message total for ONCE_PER_BATTLE.
4. Start the wild encounter: text, animation and weather complete; normal wild
   capture/run behavior remains available. Finish/abort and start an unconfigured
   encounter; no queued script or weather configuration carries over.
5. Check multi-owner entries, a hazard faint, a script-caused faint, bars breaking
   during a payload, Castoria/TX summoning and Koanko/Zacian arrival. Confirm the
   original ambush and formation timing and complete normal cleanup.

Do not copy the examples into production merely to enable the engine interface.

## Returning battle events (Stage 09, awaiting validation)

`TryRunBattleEventScript(battler, script)` exposes the same single-owner returning
runner for an already scheduled battle event. It checks controller/owner/stack
eligibility through the existing runner, saves context in the same EWRAM buffer,
and uses the ordinary returning wrapper. It does not increment a mon's entry
counter or install an opening callback. The caller must choose a safe interpreter
boundary and commit its event latch before yielding; nesting while another
configured payload owns the context is rejected. Encounter authoring remains
opposing trainer/wild participants.

Koanko's Arceus staged config uses it after the first completed full
turn, before next selection. The same interpreter resumes ENDTURN_COUNT after
the payload and performs ordinary cleanup once. This adds no context buffer or
IWRAM static. `ConjureKoankoTeam` now installs the config on the regenerated
Arceus. Autonomous validation on 2026-11-18 passed all 172 `Koanko` groups,
including production regeneration and twelve genuine faints, all 16 existing
runner groups and the five staging groups with real move animations enabled
for the core choreography. User visual acceptance remains separate. Rerun with:
`make check TESTS='Koanko Arceus'` and `make check TESTS='Entry scripts:'`.
