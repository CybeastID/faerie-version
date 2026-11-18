# Stage 09 — Arceus staged Bug Space and Command Seal Conversion 2

Status: **implemented; normal ROM build and focused/adjacent automated validation passed**
(2026-11-18). User visual acceptance remains pending. Section 16 records the
current implementation, validation evidence and limits. Stage 09.1 is held
until the user verifies Stage 09. Original snapshot: `mypatch`,
2026-11-09; explicit fallback/first-turn defaults remain below.

Current interface audit (2026-11-18): the original design below predates
configurable On Entry scripts and configured covering Break Bars. Section 9
updates those integration recommendations. Section 10 records the first Arceus
implementation milestone. The user reports Stage 08's corrected Dialga,
regeneration and twelve-faint fixtures passing; its palette/manual acceptance
remains open.

## 1. Objective and scope

Deliver the mandatory final boss sequence: fake `Alter Ego's Bug Space` presentation with genuine Multitype; survival through its first complete turn; dismiss Doom Clock while retaining boost crushing; invoke one Command Seal, fully heal HP and grant a full saved layer; on that layer's break adapt actual Arceus form/Judgment to the effective breaking attack type. No extra adaptation heal, copied opponent mechanics, Castoria Command YOU effects, Plate dependency or Kazuradrop regressions. The user's revised overcharge choreography (section 15) supersedes the original damaged-underlying-HP/no-opening-heal plan.

## 2. Existing code verified

- `src/data/koanko_party.h`: level-105 Arceus already has Judgment/Earth Power/Ice Beam/Recover, Multitype, no Plate. No Arceus encounter state/sequence exists in `battle_koanko.c`.
- `include/battle.h` owns `bugSpace`; `battle_util.c:ABILITY_BUG_SPACE` initializes Kazuradrop-specific flags, threshold160/decay10, a configured bar, UI and legend-punishment script. Calling the real ability path would add wrong mechanics and dialogue.
- `battle_end_turn.c:HandleEndTurnBugSpaceDecay` currently combines threshold decay/tier changes **and** positive-stage crushing. Crushing halves positive stages toward neutral. It can choose `BattleScript_BugSpaceStatDecayFirst`, which invokes Kazura dialogue and FLAG_TEMP_1. `HandleEndTurnBugSpacePassiveOHKO` handles Doom death.
- `battle_script_commands.c:IsMoveBlockedByBugSpace` blocks move BP >= threshold. Thus 254 is not an unconditional disable. Other tier-gated damage/item/faint behavior also reads bugSpace. `battle_util.c` can issue Kazura legend-punishment switch-in dialogue whenever Bug Space is active.
- `kazuradrop_battle_ui.c` offers Create/Destroy/SlideOut/SlideIn panel functions. `ReshowKazuradropBattleUI` recreates the panel whenever bugSpace.active, so a slide-out alone does not keep it hidden after a menu.
- `battle_interface.c:CreateAbilityPopUp` supports `gBattleScripting.abilityPopupOverwrite`; presentation can show Bug Space without writing Arceus's ability.
- `battle_break_bar.c:GrantSavedBreakBar` saves canonical covered HP/maxHP. Fill/animate/restore scripts already exist. `TryBreakBar` deliberately bypasses configured original onBreak/afterBreak scripts for saved layers; Arceus therefore needs a targeted saved-layer event continuation, not an ordinary configured phase callback.
- HP updates have separate move/passive/instant-death paths, and `Cmd_tryfaintmon` queues bars before genuine faint effects. A clamp only in the ordinary damage formula cannot cover this requirement.
- `battle_script_commands.c:Cmd_settypetorandomresistance` reuses `GetTypeModifier`, but uses mutable last-move globals, changes typing only, excludes special types, and has ordinary Conversion 2 legality rules. Copy its chart-driven idea, not the whole command. `battle_util.c:GetTypeModifier` respects inverse matchup configuration.
- `GetBattleMoveType` uses current dynamic type; `SetTypeBeforeUsingMove` resolves move/ability/item overrides. Last-hit globals are updated later and may not describe the saved-layer-breaking event after nested/delayed scripts.
- `battle_main.c:GetDynamicMoveType` currently resolves EFFECT_CHANGE_TYPE_ON_ITEM from held item. Judgment requires a narrow encounter-form override; changing Arceus's types alone will leave no-Plate Judgment Normal.
- `gen_4_families.h:ARCEUS_SPECIES_INFO` defines all **18 ordinary forms**, all with Multitype and shared front/back pixel data, distinct type species and normal/shiny palettes. Seventeen type directories and palette declarations exist in `src/data/graphics/pokemon.h`. There is no Stellar/Divine/Caster/Alter Ego Arceus form. Shared indexed shape is intentional resource reuse; switching the actual species record plus its palette is the real typed form, unlike recoloring SPECIES_ARCEUS_NORMAL.
- `battle_anim_effects_3.c:AnimTask_TransformMon` mosaics the attacker and swaps graphics by a supplied mode. `AnimTask_PolymorphMon` mosaics the target, loads party graphics and updates its healthbox; blindly calling it would reload canonical Normal graphics unless adapted. `battle_anim_scripts.s` has both scripts. Existing Castoria seal presentation is text/charge spending, not evidence of a reusable standalone seal graphic.

## 3. Implementation design and order

### A. Owned state and opening turn

1. Add an owner-bound Arceus lifecycle: NOT_ENTERED, OPENING_PROTECTED, CRUSH_ONLY_SAVED, ADAPTING, ADAPTED, FINISHED. Store first-full-turn eligibility, saved-layer tag/index, one seal-available flag, breaking-damage context and effective form override. Commit state before yielding. Keep this separate from ability/stages and Castoria's charges.
2. At entry, arm the HP floor **before entry hazards/delayed entry damage**, then run the stage 01 Arceus slide. Initialize real Bug Space data explicitly for this owner using a Koanko mode, not the ability initializer: source=Arceus, ordinary opening threshold/decay baseline, Doom panel visible and crushing enabled. Do not set Kazuradrop encounter flags or grant its configured bar/Guts/legend punishment.
3. Preserve the exact requested fake presentation text `Alter Ego's Bug Space`, using abilityPopupOverwrite and the existing presentation pattern. If the ordinary popup owner label would instead identify Arceus, supply a presentation-only label override so the complete requested wording is shown; do not change actual species, typing or ability to achieve the label. Restore popup/label context afterward. Effective ability remains Multitype throughout. Use Koanko-specific returning messages where existing scripts contain Kazura taunts.
4. Define the protected complete turn as the first command-selected full battle turn with Arceus present. If it arrives mid-turn, protect the arrival remainder and the following complete turn. If it arrives at a replacement boundary before selection, that next turn is its first. Item-only/idle/Time Stopped actions still count; scripts/bar processing do not. Keep floor until all end-turn effects for that turn finish.
5. Add one owner-scoped clamp shared by HP preview/commit paths for move, passive and instant-death updates; harden the genuine-faint gate as the last defense for direct zero-HP writes. Clamp to 1 before recording faint/break/reward effects or syncing zero HP. Cover OHKO, Perish Song, Destiny Bond, recoil/self-KO, hazards, poison/weather and delayed attacks. This is protection, not a restore/heal event; never trigger healing benefits.

### B. Dismiss clock and cover underlying HP

6. Before the following selection, run the exact dismissal line from the handoff, slide the Doom Clock out, and set persistent owner-specific panel-hidden state. Gate Create/SlideIn/Reshow as well as initial drawing. Keep bugSpace.active for crushing, but explicitly disable threshold blocking, threshold decay/tier transitions, Doom/passive OHKO and Kazura-specific dialogue/legend punishment.
7. Extract the existing crushing operation into a reusable branch/helper, preserving original Kazuradrop call order and messages. Koanko mode uses a plain crushing message/animation without ability misidentification or global FLAG_TEMP_1 ownership. Keep candidates 254/0/NORMAL only as data values if convenient; behavior must be controlled by explicit mode, not sentinel constants.
8. Follow the user's revised choreography: invoke the Command Seal, fully heal Arceus's current HP, then use `GrantSavedBreakBar` and the existing animation to cover that healed HP with exactly one full maxHP layer. Record which saved layer belongs to the seal. Remove the floor after successful handoff, before commands resume. Any pre-existing covered layers retain their saved HP, identities and scripts; the new layer saves the newly healed full HP.

### C. Breaking type, seal and genuine form override

9. At the authoritative HP commit that empties the tagged saved layer, capture this event's resolved damage type/source/validity into owner state. Use actual action damage context after dynamic type resolution; for delayed attacks use their stored resolved type. Do not read gCurrentMove or last-hit globals later. Non-attack instant deaths/residual sources have no attack type; they must not inherit stale context. Multi-hit records the hit that actually empties the layer once.
10. Extend the saved-layer resolution seam to optionally push a Koanko-owned returning continuation **after the shared saved HP restoration and before returning to battle**. Include the extra stack entry in preflight capacity checks. Gate it to the recorded layer/owner and its one unconsumed adaptation effect; the Command Seal was already invoked during overcharge. All other saved bars preserve existing behavior. Do not replay original onBreak scripts, invoke a second seal or add another bar/refill.
11. Commit ADAPTING/consumed adaptation before yielding; do not invoke another seal. Restrict candidates to the existing 18 ordinary Arceus forms; Divine, Alter Ego, Caster and Stellar are excluded. First minimize `GetTypeModifier(capturedBreakType, candidateType)` below 1: immunity, chart-provided quarter resistance, ordinary half resistance. Among equally defensive forms, maximize Judgment's type-chart effectiveness against the **current opposing Pokémon at seal resolution**, not necessarily the battler that broke the bar. Obtain its effective types with `GetBattlerTypes(foe, FALSE, types)`, including Tera. Use tagged uniform RNG only among final equals. The current form can be chosen if it ranks equally best. Do not read last-hit globals or sacrifice defense for offense.
12. For unknown/typeless damage or an empty resistance list (notably native Stellar/Divine under current chart), retain current form/type, consume the one charged adaptation effect, and complete presentation without another heal. Divine is boss-exclusive and unavailable to the player through legitimate gameplay; its case is defensive fallback coverage, not an expected player attack. Never invent a boss-only Arceus form or claim resistance where none exists.
13. Apply stage 06's battle-only form override to the chosen actual type-specific species, update typing/graphics/palette while retaining Multitype, and preserve restored underlying HP/maxHP exactly. Add a narrow GetDynamicMoveType/AI query so owned Arceus Judgment uses the override type independently of Plate; ordinary Arceus/item logic stays unchanged. Protect the override from item/Multitype refresh by rebuilding effective data after canonical loads, not by mutating held items or permanent species.
14. Compose a distortion → timed species/palette swap → reveal sequence for the charged layer's break; the seal invocation/slide already happened during overcharge. Reuse Transform mosaic or parameterized Polymorph visual components, adapted to load the effective form through the owner override. Do not call Transform/Polymorph gameplay commands or copy opponent stats/moves/abilities. Restore mosaic/blending/animation battler context and healthbox/bar visuals even if resource allocation fails.
15. On genuine Arceus defeat or battle exit, remove only its owned Bug Space data/UI, survival floor, tagged saved-layer event, seal/override context and deferred callbacks. Preserve ordinary battle cleanup and avoid setting Kazura game-over flags. Cleanup is safe when interrupted, repeated or after another controller refresh.

Proposed files: Koanko state/helpers/header; `battle.h` Bug Space mode/owner state; `battle_end_turn.c` separated crushing/gates; `battle_script_commands.c` HP/type event and scripted adapters; `battle_break_bar.c` targeted saved-layer continuation; `battle_main.c` dynamic Judgment type; Kazuradrop UI/popup/graphics integration and animation scripts/task adaptation; focused Arceus tests. No new external assets required.

## 4. Dependencies

Stages 01–02 and 06; stage 07 cleanup guarantees singles before the final bosses. Existing saved bars, type chart, Multitype forms, popup override and mosaic tasks. Stage 08 must leave no stale grayscale or disabled-slot state.

## 5. Edge cases and invariants

Exactly one first-full-turn protection window, one dismissal, one saved-layer grant and one seal spend. Pending bars are resolved before faint/reward processing. Stored covered HP returns without extra healing and is unchanged by adapting. Haze/stage changes do not invalidate the form; menus/healthbox updates cannot bring back Doom UI. No Kazura legend punish, dialogue, Guts, move swaps, item restriction or custom game-over leaks into Koanko. Ordinary Kazuradrop remains behaviorally identical.

## 6. Test strategy

New prefixes `Koanko Arceus` and `Koanko Command Seal`; existing `Break Bar`, `Arcanum Wish Endurance`, `Conversion 2`, `Multitype`, `Kazuradrop`, and narrow Polymorph/Transform tests. Use separate focused `make check TESTS='<prefix>'` invocations natively in WSL.

Test the exact `Alter Ego's Bug Space` popup, genuine Multitype and unchanged
ordinary popup context. Cover entry hazards, arrival remainder/first complete
turn, every HP-floor source and genuine loss authority. Verify the user's exact
dismissal/panel/dialogue order, full HP heal then one saved layer, restoration of
that healed HP and preservation of older layers. Check menus while hidden and
continued crushing without Doom tiers/Kazura text. Capture dynamic, called,
delayed and multihit breaking types without stale globals, including untyped
fallback. Enumerate ranked forms/ties and verify one charged adaptation, actual
species/palette/Judgment, refresh/item interactions, unchanged nonform battle
data and interrupted-animation/exit cleanup. Original Kazuradrop retains its
separate regressions and the pre-existing Stack Breaker KNOWN_FAILING teardown
exception; optional shrinking is not an acceptance requirement.

## 7. Completion criteria

The whole staged sequence is observable at correct selection boundaries, survives unusual HP sources and refreshes, and implements genuine typed Arceus/Judgment adaptation with one seal and no extra heal/bar. Shared Kazuradrop behavior and ordinary Arceus/Conversion 2 remain unchanged. Visual distortion is composed from existing resources and cleans up completely.

## 8. Open questions and discrepancies

- **Default for approval:** first complete turn includes the arrival remainder plus one full selected turn when entry was mid-turn; fallback retains current form and spends the seal when no valid resisting form exists.
- **Opening data:** the historical proposal was threshold160/decay10. Current Koanko source uses threshold90/decay10 (section 16); ordinary Kazuradrop retains its own initialization. The displayed clock is followed by explicit crush-only mode.
- **Assets clarified:** genuine typed species share base sprite pixels by design; their type/palette records exist. A demand for distinct new silhouettes would require new artwork and is not assumed.
- **Confirmed matchup clarification:** native Stellar TX has no weaknesses or resistances against player-accessible attack types. TYPE_DIVINE intentionally deals 2× to Stellar, but Divine is boss-exclusive and unavailable through legitimate player gameplay. Preserve the interaction and access restriction; this is not a discrepancy or an open design decision.
- **Seal presentation:** a returning Koanko slide plus existing distortion is sufficient. An additional reusable seal glyph was not established by the inspected code and must not be invented as an existing asset.

## 9. Current interfaces and memory restraint (2026-11-18)

Use `battle_entry_scripts.c` for the returning Arceus introduction/presentation
payload, rather than adding a second custom entry scheduler. On Entry runs after
hazards, abilities and items; it cannot arm the required pre-hazard survival floor.
The existing early `KoankoOnSwitchIn` integration is a candidate for that narrow
owner-bound initialization. A separate completed-turn boundary still owns clock
dismissal/layer grant. Turn 0 runs once at battle opening and cannot schedule the
late-arriving Arceus phase or its first complete turn.

`ConjureKoankoTeam` currently regenerates the team with the Break Bar adapter and
clears the opponent's PartyState. Attach fresh per-mon entry configuration during
that regeneration, using the scoped script adapter or explicit static
`PartyState.entryScript` assignment. Merely configuring the initial trainer table
does not configure the conjured Arceus. Returning payloads must follow the existing
entry contract and cannot regenerate parties or replace the battle callback.

`GrantConfiguredBreakBar(..., BREAK_BAR_GRANT_COVER)` now preserves covered HP and
retains configured phase scripts. `TryBreakBar` orders restoration, onBreak,
afterBreak, then return, and already preflights the required script-stack slots.
Prefer this existing path for the tagged seal layer when its phase/configuration
can preserve any existing layer identities. A NULL covering onBreak does not
refill. This can replace the proposed bespoke saved-layer continuation in steps
8/10; ordinary `GrantSavedBreakBar` layers must continue to bypass phase scripts.
Resolved breaking-hit type capture is still required at the damage event; phase
scripts do not make later last-hit/global reads authoritative. Section 15
supersedes the original opening-heal prohibition: the user now explicitly
requires a full HP heal before the new saved layer. Older
covered layers still retain their exact saved HP.

These interfaces reduce new textual overlap with upstream entry/bar execution.
Semantic compatibility still requires tests of pre-hazard protection, the first
complete turn, existing layers and reentrant script/controller yields. Crush-only
Bug Space/UI ownership is added in section 13. Exact popup wording with real
Multitype, actual typed forms/Judgment and complete encounter cleanup remain pending.

**IWRAM restraint:** prefer compact state inside the battle-owned EWRAM allocation;
keep static configuration/type maps/bytecode const in ROM. Any necessary mutable
static scratch or observation buffer must be explicitly placed in EWRAM. Avoid
large automatic snapshots that consume the IWRAM stack. Do not mark new code or
data for IWRAM without a demonstrated requirement. Source placement review is not
a linked memory-size measurement; a later authorized build must report actual
headroom.

## 10. Form-selection core milestone (2026-11-18)

Implemented in `src/battle_koanko_arceus.c`, declared in `battle_koanko.h`:
`KoankoChooseArceusResistantForm(attackType, currentSpecies, foeTypes, chosenSpecies)`.
It returns TRUE with an actual Arceus species that ranks best defensively, then
offensively, with a uniform draw among final equals. `attackType` is the captured
breaking attack's type; `foeTypes` describes the current opposing Pokémon at seal
resolution. NULL omits the offensive comparison. The explicit ROM mapping
includes exactly the 18 ordinary forms and obtains matchups from the existing
chart/inverse helper. The current form can remain if it ranks equally best.

The function returns FALSE and preserves `currentSpecies` for typeless/mystery or
out-of-range attack types, a non-Arceus current species, or an empty candidate
set. Under the current ordinary chart, Stellar and Divine have empty eligible
sets. Inverse battles use the actual inverse chart instead of hardcoded special
type exceptions. A valid call counts best-ranked candidates, draws one rank with the appended
`RNG_KOANKO_ARCEUS_FORM` tag, then finds that rank with another bounded scan. No
rejection sampling or candidate array is needed; fallback does not draw RNG.

This is **selection only**, with no encounter registration, state ownership,
initialization/cleanup hooks, ability/typing/graphics/HP mutation, healing, or seal
spending. The later lifecycle must supply the captured breaking event's type and
the current foe's effective types at seal resolution,
check the actual encounter owner, commit its spent seal before yielding on both
success and fallback, and apply/persist the chosen form/Judgment type. It must
not infer the event from last-hit globals. No partially implemented boss sequence
has been enabled in production.

Memory: the mapping is const in ROM; the selector uses scalar locals and no
mutable static storage, dynamic allocation, IWRAM annotation or automatic
candidate buffer. This source review is not a linked memory-headroom measurement.
The selector itself needs no `BattleStruct` fields; the later opening-protection
milestone in section 12 adds its own compact state alongside its consumers.

`test/battle/koanko_arceus.c` adds the `Koanko Command Seal` prefix for this
milestone. An independent oracle enumerates `GetSpeciesFormTable(Arceus)`, rather
than duplicating the production map. It checks every type/tagged rank produces
every equally best-ranked actual form once, genuine Multitype form records, fallback for
all current forms, current-form eligibility, non-Arceus rejection, and inverse
matchups without changing a real battler's species/ability/HP. Tagged-rank
enumeration and the single `RandomUniform` call establish the equal-choice
construction; no empirical distribution run is claimed.

Upstream impact: an appended RNG enum entry is the only upstream-maintained
extension. No engine algorithm or existing behavior changed; type matching stays
in the dedicated Koanko module and reuses the current chart helper. Later capture,
HP-floor, popup/UI and Judgment integration still need their own ledger entries
and semantic regression coverage.

**Initial validation: user-reported green after the fixture macro correction.**
The initial selector passed before the new ranking request; that pass does not
validate the ranked revision below. `test/battle.h` overrides `PARAMETRIZE` with a battle-runner macro
that refers to its fixture argument `i`. Both parameterized function `TEST`
cases now use `PARAMETRIZE_LABEL`, which retains the function-runner variant.
The second loop's local `i` would otherwise hide the same wrong-runner access.
That macro correction changed tests only; selector code and memory placement were unchanged.
Codex has not executed builds or tests. The ranked revision is pending validation.
Keep the current engine and
`src/battle_koanko_arceus.c`, `include/battle_koanko.h`, `include/random.h`, and
`test/battle/koanko_arceus.c` stable for:

```sh
make check TESTS='Koanko Command Seal'
```

This validates the selector only. The full Stage 09 sequence, actual break-type
capture, first-turn floor, Doom Clock/crushing, adaptation animation, Judgment
override and cleanup remain unimplemented and unverified.

## 11. User-defined defensive/offensive ranking (2026-11-18)

The user replaced the initial uniform-across-resistances policy with this order:

1. Immunity to the captured bar-breaking attack.
2. Quarter resistance if the active type chart provides it for a candidate.
3. Ordinary half resistance.
4. Highest Judgment type effectiveness against the **current opposing Pokémon**.
5. Tagged uniform randomness among final equals.

Implementation compares incoming fixed-point multipliers numerically, then
outgoing multipliers, rather than hardcoding matchups or resistance lists. The
existing 18 forms are monotypes; no dual-type form is invented to manufacture a
quarter-resistance option. Ordinary Conversion 2's `GetTypeModifier` approach is
reused, while its last-move lookup, current-type exclusion, retry loop and
typing-only mutation are not suitable for this encounter's distinct contract.

The offensive comparison multiplies the active chart's modifiers across distinct
current effective foe types. Duplicate mono/Tera slots count once; a distinct
third type counts; immunity remains zero. This compares **type matchups**, not
full damage rolls or ability/item/weather modifiers. The lifecycle must call
`GetBattlerTypes` at seal resolution, after any opponent change, and keep this
input separate from the previously captured breaking attack type. No other
ordinary battle engine algorithm has been modified to support the selector.

Regression fixtures now enumerate the breaking-type/current-foe-type matrix and
all final tagged choices, explicitly rejecting Divine/Alter Ego/Caster/Stellar
outputs. Hand-calculated cases cover immunity overriding offense, 4x over 2x,
least-resisted offense when nothing is super-effective, offensive immunities,
duplicate/third types and final ties. Battle fixtures supply the current switched
foe's types, ordinary Tera types, Stellar's retained native defenses, and inverse
matchups. These test the selector contract, not the still-unimplemented seal
damage-capture/lifecycle pipeline.

Memory and scratch: no new mutable statics, persistent fields, heap allocation,
IWRAM placement or candidate array. The existing const mapping remains in ROM;
ranking uses scalar locals. Three-element type arrays in fixtures/callers match
the existing `GetBattlerTypes` API. Spare flags/vars were considered: temporary
flags/vars have map lifetime, and existing Kazura/TX code already claims some of
them (`FLAG_TEMP_1`, `VAR_TEMP_0`, `TX_STAT_VARS`). This selection operation needs
no global scratch, so none are claimed. Later battle ownership/timing state must
retain battle-scoped lifetime instead of depending on a map reset.

**Validation: the user reports the ranked revision green.** No counts were
supplied; Codex has not executed builds or tests. Reuse the command in section 10
to reproduce the selector suite. Opening protection is the next separate
milestone below; the selector pass does not validate its new engine hooks.

## 12. Opening-protection foundation (2026-11-18)

Implemented; **user reports the requested suites passing**, excluding out-of-scope
Max/Tera Raid TODOs. User compilation initially caught the
five-byte size assertion failing under APCS-GNU struct padding. The byte-only
state is now explicitly packed, retaining the assertion; the subsequent user run
verified this correction. Production party generation does not assign
`gKoankoArceusProtectionEntry`, so the incomplete final boss sequence remains
disabled. Focused fixtures explicitly install that static returning entry config.
This marker currently runs only `return`; the actual dialogue, fake Bug Space
popup, Doom UI/crushing and saved-layer choreography will replace/extend it in
the later complete sequence. Do not enable the marker in a real encounter yet.

### Owned state, timing and handoff

`KoankoArceusState` in `battle_koanko.h` is explicitly packed to five bytes:
phase, battler ID, trainer owner, party slot and saved-layer index. It lives inside the existing
zero-allocated `BattleStruct`; `AllocateBattleResources` uses the EWRAM `gHeap`.
Actual whole-struct growth includes alignment and awaits a linked-size check.
There are no new IWRAM statics, candidate buffers, large snapshots or separately
allocated runtime objects. The fixture's eight-byte observation buffer is
explicitly `EWRAM_DATA`. No map flag/var is claimed: this state needs battle
lifetime and independent party ownership, rather than map-load reset semantics.

`KoankoArceusOnSwitchIn`, reached through the existing `KoankoOnSwitchIn` hook at
the end of `SwitchInClearSetData`, arms before entry hazards. It requires Koanko,
the conjured phase, the opponent A owner, an actual Arceus form, living HP and the
explicit entry marker. Ordinary trainer/wild/other-Arceus encounters are inactive.
Link/Frontier/two-opponent modes are excluded; synthetic local test playback is
allowed as with the existing Koanko fixtures. Owner identity is battler + trainer
+ party slot, not species/ability/palette. Reentry and graphics reconstruction do
not rearm; a replacement occupying the old slot retires the old ownership.

`HandleTurnActionSelectionState` advances WAIT_SELECTION to SELECTED_TURN before
choice processing. `EndTurnEvents` advances it to OPENING_READY only after every
`DoEndTurnEffects` handler has finished, before the next selection. A mid-turn
arrival therefore keeps WAIT_SELECTION through the arrival remainder and then
protects the next complete selected turn. Items/idle/recharge/Time Stop count as
selected turns; scripts and phase callbacks alone do not advance the phase.

OPENING_READY **retains the floor** while the future dismissal/layer script runs.
The end-turn transition records the then-next free layer position. Only
`KoankoArceusFinishOpening` can release protection, and only when a new saved
layer occupies that position, has a saved covered HP/maxHP pair, is not pending
break, and is filled to current maxHP. An older saved layer cannot satisfy this
gate. The caller must finish grant/fill/animation before invoking it; the helper
does not grant, fill, heal or animate. Failed handoffs keep protection; a success
records COVERED and cannot repeat. This milestone has no dismissal scheduler, so
configured fixtures deliberately retain protection while handoff is pending.

Genuine defeat reaches `KoankoArceusOnFaint` from `FaintClearSetData`, retiring the
owner. Normal battle exit frees all state with `BattleStruct`; graphics-only
cleanup leaves it intact. No callback tasks or global scratch belong to the
runtime implementation. The observation task is test-only and explicitly cleaned
up. Ability, moves, typing, maxHP and existing layer identities are untouched.

### HP update integration and ordering

The dedicated module supplies one positive-HP-loss clamp: protected owned HP
cannot fall below 1; zero and negative/healing amounts remain unchanged.

- `battle_move_resolution.c:CancelerMoveAnimation` uses the reachable loss for
  its faint/multihit animation hint. `CancelerHealthBarUpdate` and
  `TryMoveDamageUpdate` clamp preview and commit after Substitute/Disguise/Ice Face
  exclusions. Only a clamped hit's OHKO/Trash & Crush presentation flags clear;
  no bar is queued and no false faint/reward occurs.
- `battle_script_commands.c:Cmd_healthbarupdate` and `Cmd_datahpupdate` apply the
  same floor to passive/delayed/scripted loss, including explicit instant-death
  requests. A protected instant death becomes bounded loss, not a healing event;
  its healing bit is suppressed so it cannot mirror/award healing benefits.
- `Cmd_tryexplosion`, `BS_InstantHpDrop`, Memento's stat-change setup and
  `Cmd_setatkhptozero` use matching self-KO presentation/commit behavior. An
  unprotected actor retains the original instant-drop/zero semantics.
- `Cmd_tryfaintmon` and `MoveEndFaintBlock` repair a protected direct-zero write
  before pending-bar or genuine-faint processing. The last-defense repair sets
  canonical/battle HP to 1 and snaps the healthbar through the existing controller
  path, without invoking healing effects. The enclosing command retries after
  controller completion. Unsupported direct-zero writers that bypass all normal
  faint processing are not a supported encounter-authoring pattern.

These are narrow shared-path hooks; no damage formula, general canceler order,
Break Bar resolver or ordinary recharge/Time Stop duration was rewritten.
Semantic risk is broader than the small patches: controller preview/commit must
agree, immunity/substitute paths must remain ordinary, and explicit self/instant
deaths must remain ordinary for every unconfigured actor. Adjacent regressions
are required before claiming these hooks verified.

### Authored validation and current limits

`test/battle/koanko_arceus_protection.c` adds `Koanko Arceus protection` fixtures
for lethal direct/OHKO/multihit damage; poison/burn/weather; expiring Perish Song;
delayed Future Sight during a pending handoff; Explosion/Memento/Final Gambit;
Destiny Bond with a genuine player replacement; ordinary player loss;
idle/recharge/Time Stop turns;
pre-hazard replacement arming; arrival remainder versus the following complete
turn; saved-layer handoff with and without an existing Break Charm layer; direct
zero repair; trainer/species gating; unconfigured ordinary defeat; and owner
replacement/no rearm. The saved-layer fixture tests the commit gate and covered
HP identity; it does not claim to test the future dismissal/fill animation.

Keep the entire current engine/test checkout stable for these user-run commands:

```sh
make check TESTS='Koanko Arceus protection'
make check TESTS='Koanko Command Seal'
make check TESTS='Break Bar'
make check TESTS='Final Gambit'
make check TESTS='Destiny Bond'
make check TESTS='Perish Song'
make check TESTS='Koanko Dialga'
```

The user reports all completed tests in these requested suites passing; Max/Tera
Raid TODOs remain out of scope. Counts and linked memory headroom were not supplied.
Codex did not execute builds/tests. Full
opening presentation, clock dismissal scheduling, actual saved-layer grant
choreography, breaking-hit type capture, seal/adaptation/Judgment and final
encounter cleanup/palette/manual acceptance remain pending.

## 13. Bug Space modes and persistent clock dismissal (2026-11-18)

**User-verified after four fixture setup/DSL corrections.**
The reserve Abra in the Explosion fixture needed an explicit Speed, and three
scenes needed grouped `NONE_OF` exclusions instead of consecutive `NOT` commands.
These corrections changed only test source; the user subsequently reported all
tests passing in the requested `Koanko Arceus Bug Space` and `Kazuradrop Bug Space`
reruns (counts not supplied). Section 12's user result applies to the preceding
protection snapshot. This milestone adds the mode APIs and field/UI behavior;
the production entry marker still contains only `return` and is not installed
by party generation. No complete staged encounter is claimed.

### State ownership and interfaces

`BugSpaceData.mode` replaces the unused one-byte `thresholdPercent` field.
There were no live readers/writers of that old field. Its location/type stay
unchanged; this adds no state bytes to the EWRAM battle allocation and no save
format change. Mode zero is ordinary Kazuradrop, preserving zero initialization
and existing fixtures that set `active`/tiers directly. The other modes are
Koanko opening and Koanko crush-only. Configuration/bytecode is const in ROM;
new runtime functions use scalar locals and no IWRAM declarations. The new test
file uses one explicitly EWRAM byte to restore its three temporary flag values
and one EWRAM observation byte for the intended Explosion/icon ordering.
No production map flag/var is claimed.

`KoankoArceusStartBugSpace` requires the existing protected owner and refuses to
replace any active field. It explicitly initializes threshold 160/decay 10 and
creates the panel. It never changes Multitype, species, moves, bars, Guts or
Kazuradrop flags/vars. The later returning entry payload must call this API.

`KoankoArceusDismissClock` requires OPENING_READY and the same owned opening
field. It commits crush-only before starting the slide-out, is one-shot and
does not grant a bar, fill HP or release protection. `FinishOpening` now also
refuses handoff while this owner's opening clock remains active. The eventual
caller must schedule the exact dismissal line after the first full turn, wait
for presentation, then grant/fill the saved layer and commit handoff.

Owner retirement through the existing switch-in/faint notifications destroys
only its own panel and clears only a Koanko-mode field with its source battler.
A foreign Kazuradrop field is preserved. Ordinary battle cleanup owns allocation
and UI teardown; graphics-only reshow does not reset the mode.

### Shared behavior and execution order

`battle_bug_space.c` centralizes `BugSpaceHasDoomClock`, `BugSpaceIsKazuradrop`
and `BugSpaceCrushBoosts`. Crushing was extracted from
`HandleEndTurnBugSpaceDecay`, retaining its iteration, rounding, target selection
and scheduler position. Kazuradrop still decays, queues tier announcements, then
crushes, and still returns early at threshold zero. Its first crushing message
still owns FLAG_TEMP_1 and uses the original ability popup/taunt. Koanko uses
the same stat animation/message through a returning script label after the
popup, without borrowing an ability or claiming that flag.

User-confirmed invariant: Explosion's self-KO deliberately precedes Bug Space's
blocker. Ordinary Kazuradrop responds to the spent attack with the Invincible
symbol and existing taunt; do not move the blocker ahead of the self-KO or save
the exploding player. A focused fixture observes the icon while the user's HP
is already zero, then requires the taunt, one genuine faint and replacement.
Its test-only observer task is cleaned up explicitly. The separate Koanko
blocking fixture uses V-create and makes no claim about Explosion survival.

Opening mode permits ordinary threshold blocking/decay. Its blocked-move script
is plain text, with no Kazura taunt/popup/invincibility icon. Tier announcements,
legendary switch-in punishment, passive stack-breaking death/custom faint and
Hurricane throw-out remain Kazuradrop-specific. The intended one-turn opening
does not approach the low tiers; no new Koanko passive-death sequence is added.

Crush-only keeps `active` true, but skips threshold decay/tier updates and runs
crushing once per turn even if the stored threshold is zero. Explicit mode
checks disable threshold blocking, Bug Space Minimize damage/accuracy bonuses,
crushing-tier forced damage/message, Trash & Crush's tier override and Melt
Virus's tier amplification. A real Minimize volatile and ordinary move effects
retain their original behavior. Threshold/tier values are not disable sentinels.

`CreateBugSpacePanel`, explicit slide-in and menu reshow consult the clock mode.
Dismissal cancels a running number flash before sliding out, preventing its idle
callback from revealing the dismissed panel. Existing slide-out mechanics and
ordinary Kazuradrop transformation slide-in remain unchanged. The dismissed
panel can remain allocated offscreen until UI cleanup; recreating the UI cannot
bring it back while crushing remains active.

### Validation and remaining scope

`koanko_arceus_bug_space.c` authors fixtures for unchanged Multitype/flags and
legendary policy, plain opening blocking/crushing, rounding and ally-side
preservation, stale threshold/tier isolation, ordinary accuracy and Melt Virus
damage, panel slide/flash/recreate persistence, dismissal-before-handoff, owned
versus foreign field cleanup, ordinary player loss, and Kazuradrop's existing
flag/popup/crushing and deliberate Explosion/self-KO/Invincible behavior.
Crush-only fixtures invoke the tested protection
notifications directly to isolate mode consumers; they do not claim complete
encounter scheduling. The panel fixture exercises the UI cleanup/recreate seam,
not an emulator menu/VRAM round trip. The handoff fixture simulates filling HP,
not the future grant/fill animation. Test flag snapshots restore after cleanup.

Keep the current engine/test checkout stable for user-run validation:

```sh
make check TESTS='Koanko Arceus'
make check TESTS='Kazuradrop'
make check TESTS='Minimize'
make check TESTS='Break Bar'
make check TESTS='Koanko Command Seal'
```

Known TODOs remain outside this milestone. The pre-existing Kazuradrop Stack
Breaker fixture declares KNOWN_FAILING for an AnimTask_ShakePlatforms teardown
issue; do not count that as a newly verified or newly introduced behavior.
No build/test or linked memory measurement was executed by Codex. The user ran
validation and reported the four fixture errors above, then reported the focused
reruns passing after corrections. Exact fake
popup wording, opening slide, dismissal scheduler/line, configured saved-layer
grant, breaking-type capture, actual seal/form/Judgment, full cleanup/palette
and emulator acceptance remain pending.

## 14. Optional battler shrinking: feasibility only (2026-11-18)

**Feasible; not implemented or added to Stage 09 acceptance.** The user requested
assessment before implementation. Both a gradual Bug Space shrink and a player
mon emerging already tiny can use existing GBA affine scaling and existing art.
This must remain a visual effect, separate from Minimize's gameplay volatile.

Source evidence:

- The actual flag is `FLAG_PLAYER_SMOL` in `constants/flags.h`. Its live uses
  already cover the overworld avatar, hidden enemy level display and enemy
  stat/HP generation adjustments (`PlayerSmolDifficultyTweak`). A visual feature
  should read it, not set it as a side effect of Bug Space.
- `sprite_effects.c` already defines `gAffineAnims_PlayerMonTiny` and an enemy
  giant table, but neither table has a live consumer. They are not functioning
  battle-size support. The overworld shrink functions target the player avatar,
  not battler sprites.
- `battle_anim_effects_2.c:AnimTask_Minimize` proves gradual scaling of the actual
  mon sprite. It resets the scale when done, so calling that task does not make
  a persistent tiny mon. Its cloned afterimages are unnecessary for this idea.
- `battle_anim_mons.c` supplies `PrepareBattlerSpriteForRotScale`,
  `SetSpriteRotScale`, reset helpers and a Y-offset helper for ground anchoring.
  Battlers already retain an affine matrix index through animation mode changes.
  The matrix array is EWRAM, with a fixed shared pool of 32 entries.
- `pokeball.c` starts EMERGE and NORMAL affine animations; its normal endpoint
  would undo a one-time shrink. Truly emerging tiny needs a scaled endpoint and
  emergence path, not a size change after the ball animation finishes.
- `reshow_battle_screen.c:CreateBattlerSprite` recreates sprites after menus.
  Move animations, sprite/form reloads, Substitute and animation/background
  copies also need review before claiming persistent size through all visuals.

A short shrink animation is a small addition. Persistent tiny idle sprites are
also practical, with compact desired-size/interpolation state in battle-owned
EWRAM and reapplication at creation/reshow/animation completion. Preserving a
smooth size through every move is a moderate integration task: ordinary effects
temporarily own the same affine matrix, offsets and sometimes a BG copy of the
mon. Blindly forcing scale every frame would fight those effects. A supported
base-size composition policy would be preferable to editing each animation.

Merge-maintenance impact: keep size policy in a dedicated visual module with
narrow creation/reshow/animation hooks. A global change to sprite affine/OAM
rendering could centralize composition but affects nonbattle sprites and requires
a broader compatibility audit. The existing artwork needs no new assets or large
pixel buffers; no new IWRAM allocation/code placement is inherently required.
Actual CPU/matrix/headroom and visual quality still need emulator checks.

Before implementation, settle the shrink progression/minimum size, whether it
persists across switching or clock dismissal, and treatment of Substitute,
Transform, temporary allies and background/clone-based animations. An initial
50% idle-size implementation is lower scope than promising continuously tiny
sprites throughout every animation; no visual size or policy is selected here.

## 15. Revised opening and overcharge choreography (2026-11-18)

**Historical staging milestone; see section 16 for current validation.** The user reported trainer pixels
replacing Arceus after the new slide, visible during Seismic Toss. Both returning
payloads now run `RESTORE_BATTLER_SLIDE_CONTROL` after slide-out/controller wait.
Trainer portraits reuse the mon graphics backing buffer; this existing command
reloads the living battler/partner graphics and shadow callbacks. The staging
observer checks graphics after entry, then after overcharge/the optional
subsequent Seismic Toss, against a checksum of Arceus's ROM artwork. Its first
version incorrectly sampled the shared buffer as soon as protection armed:
that happens during data loading, before the incoming mon's graphics replace
the outgoing Wobbuffet. The user reported the two staging functions failing
that new observation (first variants); two other functions passed. The corrected
reference is decompressed once at Turn 0 into a temporary 4 KiB EWRAM heap block,
checksummed and immediately freed. Only checksum values remain in EWRAM; there
is no new IWRAM array. Separate observation/CRC assertions improve diagnostics.
Observer flags now use explicit equality assertions so another failure reports
the actual flag value separately from the expected and observed artwork CRCs.
The source fix is unverified; emulator reproduction must be retried.
Earlier user compilation found the
shared item-popup caller missing the new optional owner-name argument. It now
passes NULL for ordinary nickname formatting; both helper callers were checked.
The subsequent emulator report reaches the staged sequence; a complete automated
pass has not been reported. Earlier selector/protection/mode results apply to
their prior snapshots. The user supplied this exact replacement sequence:

1. "Oh, right. That silly thing? We wont be needing that. I'll keep the buff crushing though."
2. End that textbox, animate the clock panel offscreen, then:
   "There! Much better~! Now then, let's make this trade fair..."
3. End that textbox, then:
   "By my Command Seal! Arceus, overcharge! Release your shackles!"
4. Fully heal HP and grant one Break Bar, then:
   "Behold, your final trial!"

This explicit user revision supersedes the original prohibition on healing the
damaged underlying HP. The new saved layer covers newly healed full HP. Any
older saved layer retains its own exact HP/maxHP. Adaptation later consumes the
effect charged by this invocation, without a second seal invocation or extra
heal/refill. HP healing here does not cure status. Subsequent local text edits
use `won't` and `There! Much better!`; the graphics fix preserves them. Embedded
newline/scroll/page controls only lay out the words.

### Entry presentation and state

`gKoankoArceusEntry` is a distinct, returning once-per-battle opt-in config.
The prior `gKoankoArceusProtectionEntry` remains a no-op marker for focused API
fixtures. Early owner arming accepts both; only the staged config automatically
queues overcharge. At this milestone production party generation installed
neither; section 16 records the subsequently completed production wiring.

The staged entry payload in `data/battle_scripts_koanko_arceus.inc` checks the
protected owner, slides Koanko in for "You remember this one, right?", returns
him offscreen, explicitly starts the opening field and displays the fake popup.
`CreateAbilityPopUpWithName` renders the exact owner label `Alter Ego's` and
ability `Bug Space`. The API takes presentation arguments directly and leaves
`abilityPopupOverwrite`, real ability, nickname and species untouched. Ordinary
`CreateAbilityPopUp` still resolves the legacy overwrite and real/Illusion
nickname, then delegates to the same renderer. The existing string buffer is
reused; no persistent popup override pointer or new IWRAM storage is introduced.

### Completed-turn event and heal/bar ordering

After the existing `DoEndTurnEffects` pass finishes, `EndTurnEvents` calls
`KoankoArceusOnEndTurn` and then `KoankoArceusTryOvercharge`. Only a live owned
staged entry at OPENING_READY, with its opening field and available saved-layer
position, qualifies. Controller/script capacity is checked before scheduling.
The phase becomes OVERCHARGING before execution yields, preventing duplicate
dialogue/heals/grants. This enum value adds no state bytes: the packed owner
record remains five bytes in the existing EWRAM battle allocation.

`TryRunBattleEventScript` reuses the returning entry runner and its existing
EWRAM context. It does not increment entry runs or add an opening callback.
The interpreter returns to the same completed end-turn command, whose effect
cursor is already ENDTURN_COUNT; cleanup, timer countdown and next selection
then finish normally. Real loss blocks event start. Action/PP, popup/trainer-slide
and damage/presentation context restore through the runner.

The overcharge payload uses ordinary returning trainer slide/text commands.
It clears the first text before `KoankoArceusDismissClock` commits crush-only
and starts the panel slide. `BS_WaitBugSpacePanelSlide` waits for the real sprite
state (missing panel counts as complete), then the fair-trade and seal texts run
with separate clears. `BS_KoankoArceusHeal` prepares ordinary HP healing;
healthbar/datahp commands and their controller wait finish before
`BS_KoankoArceusGrantBar` calls `GrantSavedBreakBar`. Existing healing callback
semantics remain in use; Multitype is not replaced with Wishful Thinking.
The saved-layer animation finishes before `FinishOpening` releases protection.
Only successful handoff prints the final-trial line. Preflight/grant/handoff
rejection retains the existing fail-closed floor; it cannot add repeated bars.
No new runtime observation task, allocator or Break Bar resolver hook is added.

### Validation

`test/battle/koanko_arceus_staging.c` authors replacement-entry/hazard tests,
damaged HP then full healing, existing Break Charm identity, exact dialogue and
popup label, one invocation/grant, next-hit restoration of healed underlying HP,
idle/recharge/Time Stop timing, genuine player loss and ordinary popup context.
Its compact observation record is explicitly EWRAM and its test task/flag
snapshot is cleaned up. The test runner's optional
`ABILITY_POPUP(..., ownerName: COMPOUND_STRING("Alter Ego's"))` asserts the actual
formatted owner label even headless; older popup checks retain their matching.

Keep the current engine/test checkout stable for user validation:

```sh
make check TESTS='Koanko Arceus'
make check TESTS='Entry scripts:'
make check TESTS='Kazuradrop'
make check TESTS='Break Bar'
```

Codex ran no build, preprocessing or tests. Live trainer slides, textbox layout,
panel timing and graphic resource failure behavior still need emulator review.
Actual memory headroom is unmeasured. Capturing the breaking attack, one-time
charged adaptation, actual form/palette/Judgment, production configuration and
complete encounter acceptance remain pending. Optional sprite shrinking remains
outside this implementation.

## 16. Completed implementation and validation handoff (2026-11-18)

**Implemented and automated-verified; awaiting the user's visual acceptance.**
The user enabled autonomous mode for completing Stage 09, including builds and
test execution, then explicitly held Stage 09.1 until their own verification.
Earlier user-controlled-testing statements describe earlier snapshots only.
No Stage 09.1 shrinking work was begun.

### Source currently present

- `KoankoArceusState` is a packed 12-byte member of the EWRAM `BattleStruct`.
  It now holds the breaking type/source, tagged layer, original/chosen species
  and scripting-battler restoration byte as well as lifecycle identity. Const
  scripts and the ranked form table remain in ROM. `FutureSight.resolvedType`
  adds battle-owned storage. The ROM link reports EWRAM 235,608 / 262,144 bytes,
  IWRAM 28,808 / 32,768 bytes and ROM 27,087,600 / 33,554,432 bytes. The remaining
  linked IWRAM region also serves runtime stack needs; it is not all spare
  persistent capacity. Exact per-member padding/heap high-water is unmeasured.
  The new module contributes no `.data`/`.bss` globals; its code/table are in ROM.
- The zero-HP direct-damage commit in `TryMoveDamageUpdate` records the resolved
  type before queuing the bar. Generic scripted HP updates and self-KO record
  an untyped source. Future Sight/Doom Desire snapshot type at impact, before
  the returning attack script; benched sources explicitly clear stale dynamic
  type before using the existing party-damage rules.
- `TryBreakBar` includes the encounter continuation in stack preflight, then
  consumes the tagged effect. Execution is saved-HP restoration, adaptation,
  existing eligible covering scripts, shared bar return and original caller.
  Ordinary saved layers retain their existing script policy. Resolution is
  after all hits of the move: overkill/remaining hits do not damage revealed HP.
- `BS_KoankoArceusAdapt` selects against the captured incoming type and the
  current player-left battler's effective types. It sets actual party/battle
  species and typing without recalculating stats or healing. The existing
  form-change controller/mosaic animation handles graphics and palette; the
  custom script restores its scripting-battler binding afterward. It preflights
  five free task slots for the controller/BG/sound/mosaic sequence. Scarce tasks,
  a missing sprite or Substitute skip the animation without delaying gameplay.
  The reveal native reloads actual graphics/palette even when an animation is
  skipped; it never paints Arceus over a Substitute. Substitute fade uses the
  now-canonical typed species. No global task allocator/animation algorithm was
  changed and no extra production state byte is needed for this fallback.
- `GetDynamicMoveType` supplies owned Judgment's chosen form type independently
  of Plate. `TryBattleFormChange` protects that form from ordinary refreshes.
  This implementation temporarily changes the opponent's battle party species,
  rather than adding a general graphics override. Owner retirement, genuine
  faint and `FreeBattleResources` restore the original species when the stored
  override still owns it. Foreign species mutations are not overwritten.
- `ConjureKoankoTeam` installs `gKoankoArceusEntry` on the regenerated Arceus.
  Initial trainer-table configuration is insufficient for that replacement
  party. Current opening data is threshold **90**, decay **10**; the earlier
  threshold160 proposal is not the current source.
- A foreign active Bug Space remains owned by its source. Declining field
  initialization no longer prevents the finite opening/overcharge handoff.
  The public clock-dismissal helper still rejects foreign/already-dismissed
  clocks; the staged payload advances without stealing or clearing them.

### Runtime coverage and results

`test/battle/koanko_arceus_adaptation.c` now exercises the real zeroing commit,
bar resolver and returning form payload, using deliberately damaged covered HP
so an accidental extra heal cannot be hidden by full HP. It covers called and
multihit moves, Weather Ball, effective Tera typing, Stellar/untyped fallback,
active and benched delayed sources (including Normalize), current-foe offense,
older layers, unchanged stats/stages/status/item/PP/ability/gimmicks, actual
normal/shiny palette, graphics rebuild, Judgment runtime/AI/reserve/Plate
queries, ordinary Arceus, misses/immunity/Substitute, scarce tasks, stack
preflight, genuine defeat and repeatable owner-only cleanup.

The staging fixture now expects `ADAPTED` after its optional bar break. Its main
four variants force real trainer/Seismic Toss/form animation execution even
under the headless runner, check both shared graphics restores and verify HP,
timing, panel dismissal and dialogue. A foreign-field variant verifies forward
progress without changing the existing owner. The production party assertion
checks entry configuration only on Arceus. The twelve-faint traversal includes
the protected turn, one saved-layer break, then a separate finishing attack;
remaining hits cannot bypass the established saved-HP protection contract.

Executed against the compiled snapshot on 2026-11-18:

- `make -j6 check TESTS='Koanko Command Seal'`: **23/23 passed**.
- `make -j6 check TESTS='Koanko'`: **172/172 passed**, including Arceus staging,
  adaptation, regeneration and all twelve genuine faints.
- `make -j6 check TESTS='Koanko Arceus staged'`: **5/5 passed** after enabling
  actual move animations in the four core choreography variants.
- Equivalent Makefile headless Hydra runner against an immutable ELF snapshot:
  `Koanko Arceus` **30/30**, `Entry scripts:` **16/16**, `Trainer Slide:` **75/75**,
  `Break Bar` **66/66**, `Future Sight` **19/19**, `Conversion 2` **13/13**,
  `Transform` **14/14**, `Polymorph` **9/9**, `Arcanum Wish Endurance` **10/10**.
- `Kazuradrop`: **14 passed, 1 existing KNOWN_FAILING**:
  `kazuradrop_stack_breaker.c:33`, `AnimTask_ShakePlatforms: task not freed`.
  This is the documented pre-existing exception, not a newly accepted failure.
- `Doom Desire` and `Multitype` standalone prefixes select **no runnable tests**;
  no green coverage is claimed for those invocations. Doom Desire and actual
  Multitype/Judgment behavior are covered by the new runtime fixtures above.
- `make -j6`: normal ROM build passed, producing `pokeemerald.gba`. Existing
  unrelated unused-symbol/map-newline warnings remain. `git diff --check`
  passed for the changed Stage 09 files.

The earlier broad run had one twelve-faint fixture failure because it expected
multi-hit overkill to reach saved underlying HP. The final 172/172 run followed
the separate finishing-attack correction. No gameplay rule was changed to make
that fixture pass. Raw execution logs are retained locally in `.stage09-draft/`.

### Stable handoff and remaining acceptance

Current engine/test files and the built ROM are stable for user review. For a
repeat focused check use the first three make commands above; optional adjacent
prefixes are listed in section 6. `.stage09-draft/final-snapshot.sha256` records
the relevant source/test files and the built ROM for this uncommitted snapshot.
Automated execution validates graphics buffers,
palette, animation completion and script ordering; it does not replace the
user's review of textbox layout, distortion appearance and panel timing in the
emulator. Actual runtime heap/stack high-water is not measured.

Implementation stops at Stage 09. The user must verify it before Stage 09.1
starts; persistent shrinking and Wish for Reduction visual work remain untouched.
