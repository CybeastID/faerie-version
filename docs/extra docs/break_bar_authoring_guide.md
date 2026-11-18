# Break Bar authoring guide

Source baseline: `mypatch`, `2fb886db5d` (Stage 7 complete), 2026-11-17.
Work branch: `break-bar-presentation-authoring`.

Working-tree update: configured granting was added externally during this task.
The granting section describes that current code, not just the commit baseline.

This guide was drafted before the presentation implementation. Existing APIs and
examples were checked against source. `BREAK_BAR_INTRO_FILLS`,
`TryShowBreakBarIntro`, `AnimateBreakBarIntro`, `BS_AnimateBreakBarIntro`,
`BattleScript_BreakBarIntro`, and `PartyState.breakBarIntroShown` are now implemented
on this branch. Focused tests pass; full regression/build status is recorded below.
No example here automatically assigns bars to a production encounter.
Validation results belong in [the implementation report](break_bar_presentation_implementation.md).

## Choose the smallest encounter extension

Use configuration alone for extra full HP layers. Use a **new battle script plus
configuration** for dialogue, familiar phase effects, animations, or combinations
of existing commands. Writing an original script is the normal extension path.
Reuse a native only after checking its arguments, state assumptions, and script
continuation. Add C when the necessary operation has no safe scripting primitive
(for example, deliberately changing species and recalculating battle stats).
Keep such encounter policy outside upstream engine algorithms.

Break Bars do not automatically confer Koanko, Castoria, Kazuradrop, Reduction,
temporary-ally, or TX behavior. None of their specialized helpers is a generic
boss framework. Do not copy their move-end or formation machinery for a Gym fight.

## Files and configuration ownership

* `include/battle_break_bar.h`: `BreakBarConfig`, flags, colors, runtime API.
* `src/battle_break_bar.c`: depletion, phase dispatch, palettes, saved layers,
  and this branch's introduction animation.
* `include/trainer_break_bars.h`, `src/trainer_break_bars.c`: sparse rules,
  trainer/difficulty selection, party-generation handoff.
* `src/data/trainer_break_bars.h`: profiles and `sTrainerBreakBars`; included
  exactly once by `src/trainer_break_bars.c`.
* `data/battle_scripts_1.s`, `asm/macros/battle_script.inc`: battle scripts and
  real command argument layouts. `include/battle_scripts.h` declares exported
  labels referenced by C.
* `include/constants/battle_string_ids.h`, `src/battle_message.c`: new battle
  string enum entries and `gBattleStringsTable` registrations.
* `asm/macros/event.inc`, `src/scrcmd.c`, `src/script.c`: overworld setters and
  their one-shot context lifetime. Overworld scripts and battle scripts are
  different bytecode languages; do not paste one into the other.

Configs and rule arrays must live for the whole battle. File-scope `static const`
objects, exported `const` objects, and file-scope compound literals qualify.
Never register a pointer to a function's automatic local config.

Runtime counts, pending flags, saved HP pairs, and the introduction-shown bit
belong to `GetBattlerPartyState(battler)`, not the battler slot or saved Pokemon.
Use the accessor: multiple trainers and the temporary ally can own different
party rows on the same side. Switching preserves a member's layers. Fresh party
generation/replacement must initialize its party state. Battle cleanup frees it.
The trainer adapter holds static config pointers until `BattleStruct` exists,
then `TrainerBreakBarsApplyPending` consumes and clears that handoff.

## Trainer defaults, source entries, and event overrides

Only add explicitly configured rows to `sTrainerBreakBars`. A typical row is:

```c
[TRAINER_ROXANNE_1] =
{
    [DIFFICULTY_NORMAL] = INLINE_BREAK_BAR_RULES(
        {TRAINER_BREAK_BAR_SPECIES, SPECIES_NOSEPASS, &gTrainerBreakBarProfiles[0]},
    ),
},
```

`TRAINER_BREAK_BAR_SPECIES` matches an exact species ID, including form.
`TRAINER_BREAK_BAR_ENTRY` matches the **zero-based source party/pool entry**,
not the order in which the AI sends members out. Pool selection/reordering can
move that entry to another generated party slot. Entry rules take precedence
over species rules; an entry with `config = NULL` explicitly disables a match.
Avoid duplicates: the implementation returns the first matching entry rule,
or otherwise the last matching species rule.

The table dimensions are `[TRAINERS_COUNT][DIFFICULTY_COUNT]`. A missing
difficulty inherits Normal. `&gEmptyTrainerBreakBars` explicitly disables that
fallback. A difficulty ruleset replaces the whole Normal ruleset; rules do not
merge. An entirely unspecified trainer gets no configured layers. Profiles
`gTrainerBreakBarProfiles[0]` through `[4]` give one through five extras.
Use `INLINE_BREAK_BAR_RULES(...)` only in file-scope initializers. Named arrays
can instead use `TRAINER_BREAK_BAR_RULES(array)`.

For one event, export a named ruleset from `src/data/trainer_break_bars.h`:

```c
const struct TrainerBreakBarRules gRoxanneEventBars =
    TRAINER_BREAK_BAR_RULES(((const struct TrainerBreakBarRule[])
    {
        {TRAINER_BREAK_BAR_ENTRY, 2, &gTrainerBreakBarProfiles[0]},
    }));
```

In the encounter's existing overworld script, immediately before its existing
`trainerbattle` command, add `settrainerbreakbars TRAINER_ROXANNE_1, gRoxanneEventBars`.
The encounter must use that trainer ID. No C header declaration is needed merely
for the assembler to reference an exported C symbol. The setter reports success
through `VAR_RESULT`, supports two distinct trainers, and replaces a previous
override for the same ID. Matching initial party generation consumes it once.
Stopping/resetting/finishing the script, completing battle setup, and battle
cleanup clear unused overrides. Waiting for asynchronous work preserves them.

Runtime battle scripts can use the existing command:

```asm
    activatebreakbar BS_TARGET, gMyExportedConfig, BattleScript_MyActivationFailed
```

Define/export that config and define the failure label in the caller. This is
scaffolding, not an existing `gMyExportedConfig`. Activation requires a living
battler, one to five extras, and no active extra stack. It does not heal.
Explicit reactivation after depletion is supported; defaults are not reapplied
on switching. For runtime opt-in successive fills, use `showbreakbarfills
BS_TARGET` after successful activation. It requires `BREAK_BAR_INTRO_FILLS` on
the config. The ordinary switch-in hook handles profiles assigned before entry.

## Wild and legendary setup

There is no automatic legendary-species or map lookup. Use a named exported
`BreakBarConfig` and the existing overworld commands, in order:

```asm
    setwildbreakbars gLegendaryPhaseBars
    setwildbattle SPECIES_GIRATINA, 70
    dowildbattle
```

`gLegendaryPhaseBars` is defined in example 3 below. `setwildbattle` alone does
not grant bars. `setwildbreakbars` belongs to the initiating `ScriptContext`;
`dowildbattle` arms it. Battle initialization consumes it after allocation and
before first healthbox drawing. It affects the first opponent party member,
including in a double wild encounter; it is not a multi-member ruleset.
Invalid profiles fail with `VAR_RESULT = FALSE`. Canceled scripts and cleanup
clear pending settings. The profile never becomes persistent Pokemon data.
An existing C encounter hook can instead call `ActivateBreakBar` after battler
data is loaded, but must supply its own appropriate scheduling point.

## Layers, HP, color, and entrance style

Red is the registered base. Extras are numbered from bottom to top: purple (1),
blue (2), green/teal (3), yellow/gold (4), orange/coral (5). Five extras therefore
drain orange, gold, green, blue, purple, red. Color describes the layer, not its
HP percentage. An unregistered ordinary Pokemon keeps normal HP-based colors.

Normally `extraBars` counts **additional** layers; total layers are `extraBars + 1`.
With `BREAK_BAR_START_INACTIVE`, it instead specifies configured capacity while
zero extras are initially active; see the granting section.
Activation registers the current HP/max-HP pair as the top layer without healing.
There is no per-layer HP field in `BreakBarConfig`. A NULL `onBreakScript` restores
the next layer to current max HP. For different HP amounts, a custom on-break
script/native must explicitly establish them and synchronize party/controller
data; do not invent `.hp` fields. Saved layers restore their exact covered pair.

Default presentation on this branch is immediate current-layer color at the
actual displayed fraction. The previous green-to-color activation lerp is removed.
Set `.flags = BREAK_BAR_INTRO_FILLS` to opt a selected encounter into successive
fills. There is no species check and no production encounter is opted in here.
The dramatic sequence starts with a full ordinary green base, held briefly,
then covers it with purple through the current top color in ascending order.
Each completed colored layer remains underneath the next fill. Intermediate
fills reach full; the final fill stops at the current HP fraction. For a partial
final layer, the uncovered right edge retracts during that fill so completion
can return to the real fraction without a final length snap. The green base is
presentation only, not a heal or an additional layer.

The corrected pacing is one pixel per frame (about 0.8 seconds per
full 48-pixel layer), with a 12-frame green-base hold. Plain scripted grants
retain their three-pixel step, and saved/covering grants retain their own fill.
Implementation is present; current validation status is recorded in the report.
The startup red-flash correction retains a private palette from the first
visible green frame and stages underlying colors before switching tile indices.
A one-frame preparation yield precedes the hold; it changes no HP or
layer state. See the report for implementation and validation status.
Battlers are presented serially, so a fully opted-in doubles field takes longer.

The shared `DoSwitchInEvents` entry hook schedules returning
`BattleScript_BreakBarIntro` before the usual entry events, including Tera Shift,
Neutralizing Gas, healing wishes, Break Charm, hazards, and general entry abilities.
Healthbox send-out/slide controllers have already completed. The animation waits
for controller flags to clear. No healing controller messages or numeric HP
animation are emitted. Only palette entries and bar tiles change. The once-per-
party-member `breakBarIntroShown` prevents repetition on reshow or reentry.
Explicit runtime reactivation resets it for the new stack.

An encounter script can explicitly request a selected presentation beat:

```asm
BattleScript_MyFlaggedReveal::
    showbreakbarfills BS_TARGET
    return
```

This is a new label to author/export when used by C. `showbreakbarfills` checks
the config flag, waits for controllers, and then fills purple through the current
layer. It can intentionally repeat a previous reveal; it does not reset the
automatic entry policy or mutate HP/layers. Its separate start and polling
natives ensure a menu reshow cancels the command instead of restarting it.
Ordinary layer breaks do not call this command: after the dramatic entry, each
break uses the normal refill of the single bar underneath. Reserve an explicit
reveal command for a separately authored scripted beat, not the normal phase
refill path.

`BREAK_BAR_DEFER_INITIAL_DRAW` wins over `BREAK_BAR_INTRO_FILLS`: the caller owns
its custom presentation. The saved-layer profile uses defer; Break Charm and
Lunar Dance retain their separate intentional fills. On allocation/draw failure,
owner/state changes, reshow, or cleanup, presentation exits without changing
gameplay state. A reshow restores the authoritative bar rather than replaying it.

## Exact phase execution and script completion

`BreakBarConfig.phases[color - 1]` describes the extra layer **being broken**.
With two extras, `[BREAK_BAR_BLUE - 1]` runs first, then purple's entry.

1. A qualifying lethal hit sets HP to zero and queues the layer. A pending owner
   still counts as alive for action flow. Further hits cannot spill into the next
   layer. The action-finished resolver processes the break after the action.
2. `TryBreakBar` validates script/target-stack capacity, saves the original target,
   and binds `gBattlerTarget` (`BS_TARGET`) to the bar owner. It clears pending,
   decrements the remaining count, and changes the palette **before either hook**.
3. It executes `onBreakScript`, or `BattleScript_BreakBarRefill` when NULL.
   At the start of a custom on-break hook HP is zero, the count already describes
   the new layer, and the current max HP is available. An on-break hook **replaces**
   automatic refill; it must leave usable HP itself. The generic script calls
   `BS_RefillBreakBar`, `healthbarupdate BS_TARGET`, then `datahpupdate BS_TARGET,
   ASSURANCE_DOUBLE`. Changing max HP before this call changes the refill amount.
4. `afterBreakScript`, if non-NULL, executes after the on-break script returns.
   With the default refill it sees restored HP. It does not independently heal.
5. `BattleScript_BreakBarReturn` restores the original target, then returns to the
   engine's saved continuation. A slower battler can still take its queued action.

This numbered sequence is for ordinary configured layers. A configured covering
grant first restores its covered HP pair, then runs on-break and after-break;
NULL on-break does not refill in that case. See the distinction below.

Both hooks must end in `return`. `call` pushes a continuation; `goto` does not.
Tail-jumping to `BattleScript_BreakBarRefill` is valid because it returns through
the existing phase frame. `end`, `end2`, `end3`, `moveendall`, and ordinary move
effect entry points belong to other engine contexts and can abandon this chain.
Do not use them to finish a phase. The final red-bar faint runs neither hook.

`BS_ATTACKER` remains the original action's attacker; it is not the boss merely
because the boss broke. Bind explicit battlers and preserve any attacker/target,
move, item, or scratch globals that a reused helper requires. Existing
`saveattacker`/`restoreattacker` and `savetarget`/`restoretarget` use bounded engine
stacks; balance them and do not recursively resolve breaks from a phase. Already
pending other owners are handled by the resolver after this returning chain.
Avoid scripts that damage/faint another owner or switch the current owner unless
their pending-break and return ordering has been designed and tested.

Use `waitstate` after explicit controller commands and `waitanimation` after
animations. Natives emitting data must test `gBattleControllerExecFlags`, issue
one request per battler buffer, mark that controller, and resume only when safe.
Never emit HP and max-HP writes into the same pending buffer. `NATIVE_ARGS(...)`
in `include/battle_script_native.h` describes packed arguments after `callnative`;
advance to `cmd->nextInstr` once, or stay on the command while polling. A normal C
function with ordinary parameters is not automatically a callable script native.

## Example 1: Gym Leader ace, configuration only

**Complete drop-in table entry**, added inside `sTrainerBreakBars` in
`src/data/trainer_break_bars.h`, provided that trainer has no existing block:

```c
[TRAINER_ROXANNE_1] =
{
    [DIFFICULTY_NORMAL] = INLINE_BREAK_BAR_RULES(
        {TRAINER_BREAK_BAR_ENTRY, 2, &gTrainerBreakBarProfiles[0]},
    ),
},
```

The checked `src/data/trainers.party` team is Geodude, Geodude, Nosepass: source
entry 2 is her ace. It gets one purple extra and the red base, with ordinary full
refill and no custom effect. Other Roxanne IDs/rematches are independent. No
party-file change, script label, string, native, or event override is required.

## Example 2: new Trainer phase script, dialogue and clear weather

This is a complete alternative to example 1, not a second Roxanne table row.
The first break refills normally, then Roxanne speaks and clears weather. All
operations use existing commands; no new C helper is needed.

In `include/battle_scripts.h` add:

```c
extern const u8 BattleScript_RoxanneClearWeatherPhase[];
```

Inside `sTrainerBreakBars` replace example 1's block with:

```c
[TRAINER_ROXANNE_1] =
{
    [DIFFICULTY_NORMAL] = INLINE_BREAK_BAR_RULES(
        {
            .selector = TRAINER_BREAK_BAR_ENTRY,
            .value = 2,
            .config = &(const struct BreakBarConfig)
            {
                .extraBars = 1,
                .phases[BREAK_BAR_PURPLE - 1].afterBreakScript =
                    BattleScript_RoxanneClearWeatherPhase,
            },
        },
    ),
},
```

Add `STRINGID_ROXANNE_BREAK_PHASE,` immediately before `STRINGID_COUNT` in
`include/constants/battle_string_ids.h`. Add this designated initializer inside
`gBattleStringsTable` in `src/battle_message.c`:

```c
[STRINGID_ROXANNE_BREAK_PHASE] = COMPOUND_STRING("ROXANNE: Let's clear the air!"),
```

Append these labels to `data/battle_scripts_1.s`:

```asm
BattleScript_RoxanneClearWeatherPhase::
    trainerslidein BS_OPPONENT1
    waitstate
    printstring STRINGID_ROXANNE_BREAK_PHASE
    waitmessage B_WAIT_TIME_LONG
    trainerslideout BS_OPPONENT1
    waitstate
    call BattleScript_RoxanneClearWeather
    return

BattleScript_RoxanneClearWeather:
    removeweather
    call BattleScript_TryRevertWeatherform
    return
```

`removeweather` calls `BS_RemoveWeather`/`RemoveAllWeather`, clearing weather and
its duration. The existing returning `BattleScript_TryRevertWeatherform` sorts
battlers and checks their weather forms, displaying any needed changes. This
example deliberately clears all weather, including primal weather; if the boss
should respect an ongoing primal source, add explicit policy/guards instead.
It does not promise every weather-reactive ability will be retriggered; use
`activateweatherchangeabilities` with deliberately selected battlers when needed.
This example targets a normal single-Trainer fight, not an arbitrary two-Trainer
slide. The script's dialogue and new global label require the registrations above.

Trainer Slides also have authored event messages in `src/trainer_slide.c:sTrainerSlides`
and `handletrainerslidemsg` helpers. Those use slide event state and existing
message selection, rather than accepting an arbitrary text pointer. The explicit
slide-in/print/slide-out composition above suits a new phase without inventing a
new slide event. See `test/battle/trainer_slides.c` for compatibility coverage.

## Example 3: legendary phase boss with new form helper

**Complete authoring recipe requiring new C**, not an already implemented
generic species-change API. It gives scripted Giratina two extras and optional
entrance fills; blue's first break changes Altered to Origin Forme and refills,
then purple uses the normal refill. Capture becomes possible only on the red
layer. This recipe intentionally leaves moves, status, stat stages and PP intact.

In `include/battle_scripts.h` add:

```c
extern const u8 BattleScript_GiratinaOriginPhase[];
```

At file scope in `src/data/trainer_break_bars.h`, before the trainer table, add:

```c
const struct BreakBarConfig gLegendaryPhaseBars =
{
    .extraBars = 2,
    .flags = BREAK_BAR_INTRO_FILLS,
    .phases[BREAK_BAR_BLUE - 1].onBreakScript = BattleScript_GiratinaOriginPhase,
};
```

Create `src/battle_giratina_phase.c` with the following full helper. The source
wildcard in the Makefile picks up a new C file; no new command opcode or generic
subsystem is needed. This symbol is referenced by assembly, so no public header
is required unless C code will call it.

```c
#include "global.h"
#include "battle.h"
#include "battle_controllers.h"
#include "battle_script_commands.h"
#include "battle_script_native.h"
#include "battle_util.h"
#include "pokemon.h"
#include "constants/species.h"

void BS_GiratinaOriginPhase(void)
{
    NATIVE_ARGS();
    if (gBattleControllerExecFlags)
        return;
    u32 battler = gBattlerTarget;
    struct Pokemon *mon = GetBattlerMon(battler);
    struct PartyState *owner = GetBattlerPartyState(battler);
    gBattleScripting.battler = battler;
    if (gBattleMons[battler].species == SPECIES_GIRATINA
     && !gBattleMons[battler].volatiles.transformed)
    {
        enum Species species = SPECIES_GIRATINA_ORIGIN;
        if (owner->changedSpecies == SPECIES_NONE)
            owner->changedSpecies = gBattleMons[battler].species;
        SetMonData(mon, MON_DATA_SPECIES, &species);
        gBattleMons[battler].species = species;
        RecalcBattlerStats(battler, mon, FALSE);
    }
    gBattlescriptCurrInstr = cmd->nextInstr;
}
```

Append to `data/battle_scripts_1.s`:

```asm
BattleScript_GiratinaOriginPhase::
    callnative BS_GiratinaOriginPhase
    handleformchange BS_SCRIPTING, 0
    waitstate
    playanimation BS_SCRIPTING, B_ANIM_FORM_CHANGE
    waitanimation
    handleformchange BS_SCRIPTING, 1
    call BattleScript_BreakBarRefill
    return
```

In the actual map encounter script, replace the existing wild battle setup with:

```asm
    setwildbreakbars gLegendaryPhaseBars
    setwildbattle SPECIES_GIRATINA, 70
    dowildbattle
```

Keep the surrounding map script's normal lock/release, defeat/capture flags and
postbattle logic. This is a replacement block, not a standalone map event with
invented labels. No new battle text is used, so no string registration is needed.

`RecalcBattlerStats` already recalculates base stats/max HP, copies level/base
stats and ability/types, and brackets its mutation for Reduction. Species must
be synchronized in both party and battle copies first. `changedSpecies` records
the original for the engine's form-restoration path. `handleformchange` case 0
emits species data, the form animation refreshes graphics, and case 1 updates the
healthbox and shadow. Refill follows recalculation/animation and uses the new
max HP. Recalculation can temporarily copy party HP; do not resume ordinary
action processing between that mutation and the explicit refill.

This recipe is scoped to this scripted wild encounter, with no form-changing
held item, Transform, Terastallization or Dynamax. Ordinary form/item rules may
later revert a forced form; capture/end-battle form policy still belongs to the
existing species rules. It does not set an active Mega/Dynamax gimmick, reselect
moves, reset PP, or run switch-in abilities. If a chosen new ability needs an
activation, refill first and then deliberately compose `switchinabilities`/
`abilityonformchange` and their waits. Add focused tests for such interactions.
Do not call `BS_ApplyKoankoBossForm`: it intentionally changes Calyrex/Eternatus
moves and is specialized encounter policy. Kazuradrop transformation also owns
healing, doubled HP, Guts, moves and Bug Space state; it is not reusable here.

## Other ordinary phase effects

Use `curestatus BS_TARGET` and `updatestatusicon BS_TARGET` for existing status
operations; read `BS_CureStatus` before composing status messages. Applying a
status through `seteffectprimary` requires its real attacker/effect-battler and
move-effect arguments and immunity semantics; direct status bit writes omit them.

Stat commands on this branch are `trymovestatchanges`, `trystatchanges`, and
`trybattlerstatchange`, backed by `battle_stat_change.c`. They consume queues;
there is no legacy `statbuffchange`/`setstatchanger` API to paste from an older
Emerald tutorial. A native can queue a change with `SetStatChange` or
`SetStatChange2`, then a new returning script can resolve it with the appropriate
flags. Preserve the interrupted action's queues and choose Contrary, Simple,
Mirror Armor, Opportunist and Mirror Herb behavior deliberately.

`setfieldweather` derives weather from **gCurrentMove** and **gBattlerAttacker**;
The terrain-setting command also depends on the current move. These are not parameterized boss
weather commands. Reusing them requires saving/restoring context and handling
failure, duration, messages, field abilities, forms and seeds. Existing
`removeweather`/`removeterrain` are simpler when clearing the field is intended.
Do not `call BattleScript_Effect...` indiscriminately: move scripts commonly
perform attack cancellation/PP consumption/move-end and terminate rather than return.

`playanimation`/`playmoveanimation` plus `waitanimation`, and `playse` plus
`waitstate`, provide ordinary presentation. `fanfare` and `playfaintcry` have
specific playback purposes; do not treat them as a generic music system. Existing
`playbgm` support must be checked in the actual command set before using that name
(this branch has `BtlController_EmitPlayFanfareOrBGM`, not a `playbgm` macro).
For a music change without a script primitive, reuse an existing compatible
native or add a small encounter helper with a clear restoration policy.

## Changing the active Pokemon's HP, stats, and movepool

All three are legitimate phase effects. They are **gameplay changes made by a
phase script**, separate from the entrance fills, which must never make them.
`BreakBarConfig` does not have HP, stat, or move-list fields. Compose a new
returning script and, where needed, a small encounter native. Decide whether
each change lasts until switching, until battle end, or permanently before coding.
An opponent-only recipe must not accidentally become a permanent mutation when
used on a player Pokemon or on a wild Pokemon that can be captured.

### Current HP and per-phase refill amounts

For a full next layer, keep `onBreakScript` NULL or `call BattleScript_BreakBarRefill`.
For a partially filled next layer, a new on-break native can select the heal
amount, followed by the same existing health/data commands. This is a complete
helper recipe **to add**, not an existing native:

```c
// In a dedicated encounter source file, with global.h, battle.h,
// battle_script_commands.h, battle_script_native.h and battle_util.h included.
void BS_SetPhaseRefillHp(void)
{
    NATIVE_ARGS(u16 hp);
    if (gBattleControllerExecFlags)
        return;
    SetHealAmount(gBattlerTarget,
        max(1, min(cmd->hp, gBattleMons[gBattlerTarget].maxHP)));
    gBattlescriptCurrInstr = cmd->nextInstr;
}
```

```asm
BattleScript_MyPartialRefill::
    callnative BS_SetPhaseRefillHp
    .2byte 50
    healthbarupdate BS_TARGET
    datahpupdate BS_TARGET, ASSURANCE_DOUBLE
    return
```

At on-break entry HP is zero, so this example produces 50 HP, capped at max HP
and floored at 1; it is not an arbitrary set-HP operation at nonzero HP. Declare
the new script in `include/battle_scripts.h` and assign it to the desired
`onBreakScript`. There is no new string. `SetHealAmount` selects existing HP
update state; the standard script commands perform the animation and gameplay
update. Do not set HP and then ask the same commands to apply the heal again.
For an after-break heal, calculate the missing amount from the already refilled
HP, rather than assuming zero. Never use a zero-HP result to emulate a pause:
that can enter faint processing instead of creating a healthy new phase.

Ordinary saved bars are different: `BS_RestoreSavedBreakBar` restores the covered
HP/max-HP pair and bypasses your phase hooks. Configured covers restore first and
then run their hooks, as detailed below. Editing just the current HP does not edit
covered saved HP. Decide explicitly whether a new effect should modify those
snapshots; the standard presentation change never does.

### Max HP and raw stats versus stat stages

Stat **stages** are the ordinary -6 to +6 modifiers, in
`gBattleMons[battler].statStages`. Prefer ordinary queued stat changes for a
vanilla-style boost: queue with `SetStatChange`, resolve through
`trybattlerstatchange`/`trystatchanges`, and let the normal rules handle abilities,
caps, animations and messages. Do not silently overwrite stages if Contrary,
Simple, Mirror Herb or Opportunist should participate. Normal switch-out resets
stages; surviving switching needs a deliberately authored phase policy.

**Raw stats** (`attack`, `defense`, `speed`, `spAttack`, `spDefense`) and `maxHP`
are separate from stages. If the intended change is a real species/form change,
use example 3: synchronize species, call `RecalcBattlerStats`, refresh graphics,
then refill using the recalculated maximum. This existing recalculation also
copies ability/types and reapplies Wishful Thinking, TX and Reduction policy.
It is not a general arbitrary boss-stat multiplier.

For a new multiplier or fixed raw-stat/max-HP value, add a small native and
document its storage policy. The following choices have different lifetimes:

* Writing only `gBattleMons` changes the current active copy; a switch-in or form
  recalculation can replace it from the party copy. It is not switch-persistent.
* Writing the party copy with `SetMonData` and synchronizing `gBattleMons` keeps
  the next controller reload consistent, but stat recalculation may replace
  calculated stats again. It can also affect capture and player persistence.
* A battle-only effect that must survive reloads needs owner-bound state and a
  deliberately placed reapplication seam. `wishfulGrowth` and
  `WishfulThinkingReapplyGrowth` show a specific existing solution. There is no
  generic arbitrary phase-stat override field to claim already exists.

Never repeatedly multiply a previously grown/reduced value on reload. Keep a
clean canonical baseline and reapply once; clamp positive raw stats/max HP to the
u16 range, with max HP at least 1. Bracket canonical battle-stat/HP mutations with
`ReductionBeginMutation`/`ReductionEndMutation` (include `battle_reduction.h`),
unless the reused helper already does that, as `RecalcBattlerStats` does.
Do not use reduced visible numbers as the canonical input or silently borrow
TX/Kazuradrop health-overlay state for an unrelated boss.

On max-HP change, choose a current-HP policy: full refill, preserve percentage,
preserve missing HP, or a fixed phase amount. Clamp HP to the new maximum. At a
break, recalculation before the full/partial refill is usually simplest.
`BS_RestoreSavedBreakBar` and `BS_SyncSavedBreakBarHp` demonstrate serialization
of `REQUEST_MAX_HP_BATTLE` and `REQUEST_HP_BATTLE`: issue max HP, wait, issue HP,
wait, then refresh the full healthbox/numbers. Do not send both requests into one
pending controller buffer. `UpdateHealthboxAttribute` normally reads party HP;
only existing Reduction/TX overlays override that with battle HP. Updating only
`gBattleMons.maxHP` can therefore produce a stale or inconsistent display.
Never use the intro overlay as a substitute for authoritative HP synchronization.

### Replacing moves and choosing PP policy

The existing `KoankoSetBossFormMove` in `src/battle_koanko.c` is a private,
specialized example: it calls `SetMonMoveSlot`, writes the active move/PP, and
grants the replacement move its base PP. It is not an exported generic native.
For an ordinary boss, create an encounter helper with an explicit slot list.
Slots are zero-based; a movepool change does not grant another action.

This copyable helper body illustrates a **new** full-base-PP replacement policy;
put it in the encounter module and call it from its returning phase native.
Include `pokemon.h` and `move.h` in addition to the usual battle headers:

```c
static void ReplacePhaseMove(u32 battler, u32 slot, enum Move move)
{
    if (slot >= MAX_MON_MOVES)
        return;
    struct Pokemon *mon = GetBattlerMon(battler);
    u8 ppBonuses = GetMonData(mon, MON_DATA_PP_BONUSES);
    ppBonuses &= ~(3u << (slot * 2)); // No inherited PP Ups for this new move.
    SetMonData(mon, MON_DATA_PP_BONUSES, &ppBonuses);
    SetMonMoveSlot(mon, move, slot); // Sets the party move and its base PP.
    gBattleMons[battler].ppBonuses = ppBonuses;
    gBattleMons[battler].moves[slot] = move;
    gBattleMons[battler].pp[slot] = GetMovePP(move);
}
```

This changes the party copy too, so the replacement survives switching. For a
player or catchable wild boss, explicitly decide whether the replacement should
survive battle/capture; if not, preserve and restore the original moves/PP/bonuses
at the appropriate lifecycle boundary. If keeping spent PP is intended, use
`CalculatePPWithBonus` and clamp retained PP to the replacement's permitted max
instead of resetting it. Preserve untouched slots and their spent PP. A move
set change alone needs neither a form animation nor recalculation of base stats.

Move IDs also live in Disable, Encore, Choice lock, last-used move history,
Mimic/Transform state, queued action selection, and AI caches. A slower boss may
already have selected a move for this turn when its layer breaks. Choose whether
that queued action remains the original move or is deliberately updated; do not
assume replacing a slot safely rewrites `gCurrentMove` or the action queue.
Specify which locks/history persist and reconcile invalidated ones. The helper
above alone does not solve those policies. For battle-copy-only moves, inspect
the existing `Cmd_updatebattlermoves`/`updatebattlermoves` path before reloading:
it copies moves/PP from party data and can overwrite temporary replacements.

For all three kinds of modification, test the first phase transition, a slower
already-selected action, switch out/back, form/stat recalculation, low/partial
HP and PP, saved bars over the phase, Reduction where applicable, final faint,
capture/escape and battle cleanup. A passing visual-fill test says nothing about
the correctness of these separate gameplay mutations.

## Saved bars, fainting, switching and cleanup

`GrantSavedBreakBar` covers the current canonical HP/max-HP pair and adds an
extra full-max-HP layer, capped at five extras. The caller fills/animates it.
Break Charm stays held, grants once per member per battle (also when passed),
and activates before hazards. Its fill is intentional and separate from the
removed initial palette lerp. Lunar Dance first restores HP/status/PP and then
adds a layer; at the cap its restoration still works. Endurance uses the same
saved-layer API. The covered ordinary bar keeps its HP-dependent color during
the overlay; a registered base stays red.

When an ordinary saved layer breaks, it restores its saved pair, skips both
configured phase hooks, and carries no overkill into that pair. Configured layers underneath
still run their original hooks. The arrays, not the visible color alone,
determine whether a layer is saved. Never zero those arrays to simplify UI code.

Ordinary passive damage (weather, poison, recoil, confusion, hazards and similar
item/ability damage) stops at 1 HP while extras are interceptable. A pending layer
cannot be healed/damaged before transition. Direct lethal damage, OHKO effects,
Perish Song, Destiny Bond and deliberate self-KO have separate interception/
resolution paths; inspect their tests rather than generalizing the passive floor.
Delayed/end-turn lethal paths resolve before the next applicable event. The base
layer faints normally with standard EXP, items, abilities, and replacement logic.

Switching and forced switching keep party-owned depletion; returning members do
not regain configured layers or replay the once-per-member intro. New occupants
do not inherit another member's bars. Ordinary HP-fraction updates still use the
standard controller/UI path. Intro fills do not mark a member fainted or grant
an extra action, and must not invoke switch-in abilities themselves.

`ReshowBreakBars` rebuilds palettes and authoritative bar fractions after menus;
`FreeBreakBarGraphics` cancels presentation and frees palette tags at graphics
cleanup. Saved player HP is consolidated by `RestorePlayerBreakBarHp` at battle
exit, with existing form/Reduction handling. Break counts and presentation state
are battle-only; none is added to save data. Capture is blocked while extras or
a pending break remain, including against Master Balls, but returns to ordinary
odds on the base. Rejected standard/Safari balls are not spent.

## Configured grants: current working-tree additions

The current header/source expose `BREAK_BAR_START_INACTIVE`,
`CanGrantConfiguredBreakBar`, `GrantConfiguredBreakBar`, `BREAK_BAR_GRANT_COVER`,
and their native/macros. These are existing external edits preserved by this
presentation task, not proposed APIs. They separate a member's attached phase
table from its current number of active layers.

`GetInitialBreakBarCount` returns zero for START_INACTIVE, otherwise `extraBars`.
Trainer registration, wild registration, and runtime activation all use it.
Before an inactive profile has ever gained a bar, `IsBreakBarBase` remains false:
the ordinary HP-based color remains. Once bars have been active/depleted, the
registered red base applies. `breakBarsPeak` records that history.

There are three distinct grant contracts:

* `GrantSavedBreakBar`: save canonical covered HP/max HP and add an ordinary
  layer; on break restore the pair and skip both phase hooks. Break Charm,
  Lunar Dance, Endurance, TX Restoration and TX Endurance use this family.
* `GrantConfiguredBreakBar(battler, 0)` / `grantconfiguredbreakbar`: raise the
  active count by one; the current HP becomes the newly active configured layer.
  No heal or snapshot. The script command fills that new layer to its actual
  current HP fraction before the next command, without a palette fade.
  On break use the owner's `phases[oldActiveCount]`, with
  normal on-break/automatic-refill and after-break semantics.
* `GrantConfiguredBreakBar(battler, BREAK_BAR_GRANT_COVER)` /
  `grantconfiguredcoverbreakbar`: save the covered pair, raise the count, and
  mark that position in `breakBarCoverScripts`. The script command fills the new
  full-HP layer and animates its saved overlay before the next command.
  On break: **restore covered pair -> onBreakScript -> afterBreakScript
  -> target restoration/continuation**. NULL on-break means do nothing, not full
  refill. An explicit refill in either hook can intentionally heal the restored HP.

Configured grants require a living owner with an attached config, no pending
break, and `activeCount < config->extraBars`. They use the next free stack
position, not a separate queue of unused phase IDs. An ordinary saved bar can
occupy that position and prevent a configured grant. A broken configured position
can be granted again and its scripts run again; there is no inherent once-only
phase limit. `CanGrantConfiguredBreakBar` mirrors eligibility without mutation.
The absolute extra-layer cap remains five. The low-level C grant function does
not change max HP or heal: script commands own animation and, for covers, filling
the new HP layer. Use the complete grant macros for new encounter scripts.
`BREAK_BAR_DEFER_INITIAL_DRAW` still defers registration/intro presentation;
scripted grants always fill. A C integration must explicitly execute the matching
fill continuation rather than granting invisibly. Generic saved grants retain
their established move/item-script fill timing, including Break Charm.

The current ready-made returning `BattleScript_ConfiguredBarHealThenCover`
expects `gBattlerTarget` to be the owner. It checks `canconfiguredbreakbar`
**before healing**, plays `B_ANIM_WISH_HEAL`, performs ordinary full HP update,
waits, then calls `grantconfiguredcoverbreakbar`, whose macro includes the
`fillsavedbreakbar`/`animatesavedbreakbar` commands. Do not append those commands
again after that macro.
It covers the healed full HP pair, not the previous injured pair. It silently
returns when ineligible; callers own any success/failure dialogue. A complete
returning phase composition can therefore say:

```asm
BattleScript_MyGrantBeat::
    call BattleScript_ConfiguredBarHealThenCover
    return
```

Define/export that new script if referenced by C. This snippet has no message;
unconditionally printing a success line after the call would be wrong on failure.
When called from a break hook, the count has already dropped, so a new grant
can replace the layer just broken. The encounter script must deliberately decide
whether and when to call it again; no engine grant budget is needed.

`TrimCoverScripts` discards bits for depleted layers. Ordinary saved grants clear
the bit at their new position so a recycled position cannot inherit configured
hooks. Saved-array restoration, stack-exhaustion paths, switching and fresh
owner cleanup must stay consistent with that bitmask. Configured-cover on-break
hooks start at **restored nonzero HP**, so the zero-start partial-refill recipe
above must not be reused there without adapting its healing policy.

`test/battle/break_bar_configured.c` was authored with the external additions;
its original comment says it was not built. It tests inactive registration,
repeat grants, stack order, cover-vs-ordinary behavior, capacity, cleanup and
real restoration. This task's report separately records actual build/test results.

### Eligibility reasons and developer diagnostics

`GetConfiguredBreakBarGrantResult(battler)` returns `enum BreakBarGrantResult`.
The constants in `include/constants/battle_break_bar.h` are
`BREAK_BAR_GRANT_READY`, `BREAK_BAR_GRANT_NO_OWNER`, `BREAK_BAR_GRANT_NO_CONFIG`,
`BREAK_BAR_GRANT_INVALID_CONFIG`, `BREAK_BAR_GRANT_PENDING`,
`BREAK_BAR_GRANT_NOT_ALIVE` (dead or absent), and `BREAK_BAR_GRANT_FULL`.
`CanGrantConfiguredBreakBar` uses the same checks without mutation or logging.
The existing failure-label commands remain available.

Scripts can explicitly query the reason into `cMULTISTRING_CHOOSER`. Include the
constants header in another battle-script translation unit if needed (the main
`data/battle_scripts_1.s` already includes it). For example, author this returning
script, declaring its label in `include/battle_scripts.h` if C references it:

```asm
BattleScript_MyScriptedGrant::
    getconfiguredbreakbarresult BS_TARGET
    jumpifbyte CMP_NOT_EQUAL, cMULTISTRING_CHOOSER, BREAK_BAR_GRANT_READY, BattleScript_MyScriptedGrantEnd
    grantconfiguredbreakbar BS_TARGET, BattleScript_MyScriptedGrantEnd
    @ The new bar has finished filling here.
    @ Compose any intended HP/stat/form/move effects here using their real helpers.
BattleScript_MyScriptedGrantEnd:
    return
```

The comments themselves add no gameplay effect. The explicit query overwrites
the chooser scratch byte; query before preparing a message that also uses it.
Only an actual failed low-level grant attempt logs
`BREAK_BAR_GRANT_REJECT ... reason=...` through `DebugPrintf`; preflight queries
are quiet. That developer log and reason strings are compiled out under
`RELEASE`. All validation, result values and failure branching remain functional
in release. No rejection automatically prints a player-facing battle message.

The granting battle script owns grant-time effects and their order. No grant
move, AI-selectable action, grant budget, or automatic on-grant callback exists.
New grants by effect remain rare, major-encounter script policy.

### Grant presentation, authoring policy, and remaining extensions

1. **Existing fill presentation.** Saved grants already use
   `fillsavedbreakbar`/`animatesavedbreakbar`; selected introductions already use
   the opt-in successive fills. These belong to the current presentation work.
   Initial entry selects the current color immediately. Every scripted grant
   fills its newly added bar, including plain configured grants. The correction
   also adds an explicit `showbreakbarfills`
   script command, gated by `BREAK_BAR_INTRO_FILLS`, for selected scripted beats.
   Covering grants keep their saved overlay. The correction is implemented;
   automated validation is recorded in the implementation report.
2. **Script-owned encounter grants.** New configured grants are invoked by the
   encounter's scripts. The scripts control whether, when and how often to grant.
   Do not add grant-budget fields or an engine budgeting subsystem. Granting bars
   through new effects should be rare and reserved for major encounters. Preserve
   established Break Charm and other existing grant behavior.
3. **Explicit heal-and-grant variants.** Preserve the existing heal-then-cover
   contract. Add separately named returning recipes for cover-current-HP and
   heal-to-a-selected-fraction-before-cover. Show eligibility failures before
   healing/dialogue, and make the covered pair's timing clear. These usually
   need script composition and at most a small HP-amount native, not engine edits.
4. **Grant feedback with release-excluded diagnostics.** The current correction
   adds a typed eligibility query and explicit script access to the reason.
   It distinguishes no owner/configuration, invalid configuration, full capacity,
   pending/dead owner and success. Author/debug diagnostics
   must be compiled out of release builds, using the existing `RELEASE` build
   define (for example, `#ifndef RELEASE`). Eligibility checks and failure
   branching remain in every build. Any player-facing failure message is a
   deliberate encounter-script choice, not automatic diagnostic chatter. The
   current boolean plus failure label remains sufficient for simple encounters.
5. **Scripted grant-time HP/stat/move changes.** Compose the encounter's HP,
   stat, form or moves/PP helper before or after the grant in a returning battle
   script, using the policies above. Existing phase callbacks execute on break;
   grant-time effects are composed by the granting script. A new callback field
   or automatic grant subsystem is not required.

Do not compose these effects by reactivating the config, changing the real count
to drive an animation, or replaying entry events. START_INACTIVE + INTRO_FILLS
has no entrance fill when zero bars are active. Later grants use their explicit
scripted presentation; they do not automatically rerun this once-per-owner intro.
Prioritize tests for mixed ordinary/configured saved layers, repeats and cap,
Charm ownership, restored partial HP, pending-break rejection, failure-without-
healing, script returns, form/stat/move updates, and menu/battle cleanup.

## Validation workflow

Run from the repository root in WSL. This task has explicit autonomous build/test
authorization; repository default for subsequent tasks remains user-controlled.

```sh
python3 'docs/extra docs/validate_break_bar_examples.py'
make -j8 check TESTS=test/battle/break_bar.c
make -j8 check TESTS=test/battle/trainer_break_bars.c
make -j8 check TESTS=test/battle/wild_break_bars.c
make -j8 check TESTS=test/battle/break_bar_grants.c
make -j8 check TESTS=test/battle/break_bar_exit_hp.c
make -j8 check TESTS=Break
make -j8 check TESTS=Koanko
make -j8 check TESTS=Kazuradrop
make -j8 check TESTS=test/battle/trainer_slides.c
make -j8
make -j8 release
python3 'docs/extra docs/verify_break_bar_release_diagnostics.py'
```

The example validator extracts the actual guide blocks and compiles six C
snippets plus the new battle scripts with the repository toolchain under
`tmp/break-bar-validation/examples/`. They compiled successfully in this task;
the unused static move-helper warning is expected because that recipe is not
called by a production encounter. This checks syntax and declarations, not an
installed encounter's runtime form/HP/move policy or graphical appearance.

`TESTS` is patched into `gTestRunnerArgv` in the **test ELF link recipe**. Changing
the variable alone is not a reliable way to change an already linked filter.
If make does not relink for a different filter, remove only generated test ELFs:

```sh
rm -f pokeemerald-test.elf pokeemerald-test-headless.elf
make -j8 check TESTS=test/battle/wild_break_bars.c
```

These names follow this checkout's default `ROM_NAME`; adjust if you override it.
For a full regression, deliberately relink with an empty filter:

```sh
rm -f pokeemerald-test.elf pokeemerald-test-headless.elf
make -j8 check TESTS=
```

Unfiltered regressions and justified rebuilds are valid. Do not preserve an old
filter by habit. Avoid an expensive clean rebuild unless dependencies/toolchain
changes or stale generated assets justify it. Keep the source snapshot stable
during each run and distinguish baseline failures from new ones.

`test/battle/break_bar.c` covers direct/multihit/phase dispatch, graphics and
allocation guards; `break_bar_grants.c` covers saved grants and Break Charm;
`break_bar_exit_hp.c` covers cleanup; trainer and wild files cover rules/overrides
and capture. Add actual battle tests for new hooks: successful script return,
field/stats or form state, switch persistence, final faint, and stack/target
restoration. Presentation tests must inspect palette/tiles across frames and
assert HP/count/saved-layer state remains unchanged, including partial HP,
multiple layers, menu reshow, controller waits and invalid graphics.

Manual mGBA acceptance (no desktop automation):

1. Use a temporary local copy of example 1 and confirm purple is correct from
   the first visible healthbox frame, at full and partial HP. Verify 1/2/5 extras.
2. Opt the profile into fills: count purple-to-top fills, check the final fraction,
   check dialogue/ability/hazard order, and confirm no extra heal/item messages.
3. Equip Break Charm alone and over configured bars; its own fill and restored
   covered HP/color must remain. Check entry hazards and reentry grant limits.
4. Break every layer, check phase order and normal final faint; try multihit,
   residual damage, forced switching and switching back at partial HP.
5. Check a scripted legendary with fills and form transition, final-layer capture,
   menu open/close, and a doubles fight with differently colored stacks.
6. Leave/restart battles and confirm no stale bar colors, replayed intros, or UI
   allocations. Review the three superbosses for unchanged specialized behavior.

Examples are authoring recipes, not production encounter edits. Compile and
test an adopted recipe before treating its gameplay or graphical timing as
verified. Headless passes cannot certify visual pacing or aesthetic acceptance.

### Testing the first visible entrance frame

For an encounter intended to start with Break Bars, use the test-only
`BreakBars(config)` field inside its `PLAYER`/`OPPONENT` block in `GIVEN`. Its
static config is applied before healthbox creation. The main
`test/battle/break_bar.c` entry fixtures use this field; their AI scoring callback
records counts rather than activating bars after send-out. Late activation in
AI scoring makes an ordinary green bar visible first and is unsuitable for an
entrance-presentation check.

The first-visible regressions in `test/battle/break_bar_presentation.c` cover all
five extra-layer colors, player/opponent sides, partial HP and independent
doubles palettes. A test-runner task records the actual palette as the healthbox
first becomes visible, along with its tiles. These immediate-color cases use
unflagged profiles. The `Break Bar visual` intro cases instead require a full
ordinary green first frame, including the 37/100 setup, before the colored
layers cover it. `START_INACTIVE` does not pre-display that full base: its
scripted grants keep their own fill; an explicit dramatic reveal still uses
the green-base hold. Tests intentionally calling activation in `THEN` still
exercise runtime API behavior; their earlier barless entrance is intentional.

For a focused interactive mGBA test ELF, run in WSL:

```sh
rm -f pokeemerald-test.elf
make -j8 pokeemerald-test.elf TESTS='Break Bar initial'
```

Open that newly built root `pokeemerald-test.elf` in mGBA. The legacy lerp source
is retained in `src/battle_break_bar.c` behind
`BREAK_BAR_ENABLE_LEGACY_LERP = 0`, and is excluded from compilation. The retained
refresh API argument cannot enable it. This does not change the intentional
fill animations for grants or flagged successive presentations.

### Visual heal/grant and successive-fill fixtures

The interactive verification set uses the `Break Bar visual` prefix in
`test/battle/break_bar_presentation.c`. It shows three full intro layers,
five intro layers ending at 37/100 HP, and the real heal-then-cover helper on a
37/100 owner. The visual break case runs its dramatic intro once, then breaks
all three layers with ordinary single-bar refills. Explicit
replay remains covered by API tests rather than being installed in that case's
phase script.

The heal fixture's returning phase script grants once, then permits a subsequent
break to restore the saved healed pair. This one-shot condition belongs to the
fixture's script, not to engine grant eligibility or AI move selection.

Rebuild the interactive artifact from WSL with:

```sh
rm -f pokeemerald-test.elf
make -j8 pokeemerald-test.elf TESTS='Break Bar visual'
```

The user accepted the first-frame color check and, on 2026-11-17, confirmed the
corrected dramatic startup transition without the reported red flash. See the
implementation report for the exact artifact hash and validation of each case.
