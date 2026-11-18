# Break Bar entrance presentation implementation

## Documentation-first checkpoint (2026-11-17)

Baseline: clean `mypatch` at `2fb886db5d`, Stage 7 complete. Task branch:
`break-bar-presentation-authoring`. The handoff's `PreS5Cd` restrictions are
superseded by the user's current branch instructions. No merge, commit or push
is authorized. Builds and headless tests are explicitly authorized for this task.
No Computer Use/desktop automation is allowed.

This report and [the authoring guide](break_bar_authoring_guide.md) were created
before any source changes. APIs described as additions below are **planned**, not
yet implemented or validated at this checkpoint. All document deliverables stay
in `docs/extra docs/` as requested; older mechanic references outside that folder
remain historical. In particular, their initial-fade description is superseded
by this task's guide when the implementation is complete.

## Planned behavior and boundaries

Normal activation will select the current palette immediately instead of calling
`RefreshBreakBarPalette(..., TRUE)`. Keep the general palette/fade interface and
existing damage/break semantics. No changes to the established palette constants.

Add `BREAK_BAR_INTRO_FILLS` as a static config flag. A narrow hook at the start of
`DoSwitchInEvents` (ORDER_BY_SPEED) schedules a returning presentation script for
eligible entering owners, before entry effects. Work is serial per battler and
once per party member; explicit activation of a new depleted stack resets the
shown bit. Neither legendary species nor any production profile opts in by default.
Deferred-draw callers retain ownership and take precedence over intro fills.

The frame routine lives in `battle_break_bar.c`, reuses `DrawHealthbarOverlay`,
and overrides only graphics palette selection, never gameplay counts. It fills
one purple-to-top visual layer per extra, intermediate layers full, final layer
at actual current HP/max HP. Three pixels/frame bounds five 48-pixel fills to
85 steps. No animation task allocation or healing/damage controller command.
It waits for controllers; absent/dead/pending owners, invalid graphics, state
changes, menu reshow and graphics teardown cancel safely to authoritative UI.

Transient frame data is module-owned per battler, bound to trainer/party slot and
config/HP/count snapshots. Party-owned `breakBarIntroShown` survives switching
but is not saved. `ReshowBreakBars` cancels transient work; `FreeBreakBarGraphics`
clears it at resource cleanup. Existing saved-fill data is never repurposed.

Break Charm, Endurance and Lunar Dance keep their saved HP pairs and intentional
fill scripts. Koanko stages/forms/temporary ally, TX, Reduction, Castoria and
Kazuradrop policy are out of scope. No substantial battle algorithm is replaced.

## Upstream integration ledger supplement

The existing centralized historical ledger is
[`docs/mechanics/upstream_modifications.md`](../mechanics/upstream_modifications.md).
This task's delta is recorded here to honor the user's requirement that all
document changes live in `docs/extra docs/`.

* `src/battle_switch_in.c:DoSwitchInEvents`: planned narrow scheduling hook before
  speed-order setup. It must retain the current switch-in state while the script
  returns/polls and must not consume entry ability/item events. Regressions:
  configured trainer/wild entry, switch/forced switch, doubles, entry abilities,
  Break Charm/hazards and specialized encounter canaries. Low textual footprint;
  semantic dependency on healthbox readiness and upstream switch-event ordering.
* `include/battle.h:PartyState`: planned battle-only shown bit. No Pokemon save
  layout change. Review upstream state reset/reuse when merging; regression:
  no replay on switching or menu reshow, clean new owner/battle initialization.
* `data/battle_scripts_1.s`, `include/battle_scripts.h`: planned returning
  `BattleScript_BreakBarIntro`, using one polling native. No new bytecode opcode.
  Review continuation conventions if upstream script execution changes.
* `src/battle_break_bar.c`, `include/battle_break_bar.h`: custom module/API changes,
  rather than upstream algorithm modifications. Shared palette override must
  retain normal fade requests and immediate break transitions. Regressions:
  initial true color, visual-only fills, partial fraction, cap, allocation failure,
  stale owners, reshow and teardown. No changes planned to healthbar renderer.

This arrangement adds one shared hook instead of edits across player, opponent,
recorded and partner controllers. Fewer textual conflicts do not prove semantic
compatibility: recheck controller completion, event reentry, ownership and
graphics lifetime during upstream updates.

## Validation plan and status

No builds/tests have run at the documentation-first checkpoint. Intended coverage:
focused Break Bar/trainer/wild/grants/exit suites; presentation tests asserting
unchanged gameplay state; Koanko/Kazuradrop and Trainer Slide canaries; normal ROM
build; broader regression when warranted. Exact WSL commands and test-filter
relink rules are in the guide. Three copyable authoring examples are recipes,
not installed gameplay changes. New form helper in example 3 must be added by
the encounter author; it is not claimed as an implemented engine API.

Manual mGBA checks remain user-owned: first visible palette, count/pacing of
successive fills, final partial fraction, Break Charm overlays, ability/hazard
order, phase form graphics, switching, doubles, menus and battle exit.

## Implementation and results

Implementation and automated validation are complete. The planned APIs,
shown bit, shared switch-event hook and returning script above are present.
Initial activation now requests an immediate palette. Existing plain configured
grant fades and all saved-grant fills retain their behavior. A forward declaration
of `GetHealthbarSprite` also supports the externally added grant caller located
before that helper's definition.

External work arrived during the task: START_INACTIVE, configured/plain and
configured-cover grants, cover-script mask, the heal-then-cover subroutine and
`test/battle/break_bar_configured.c`. Those edits are preserved. Their existing
documentation edits outside this folder were not authored by this task. The guide
now distinguishes all three grant families, existing presentation, script-owned
grant policy and remaining extensions. It also documents current/max HP changes,
stat stages versus raw stats, moves/PP, lifetime and controller synchronization.

Observed validation:

* Initial pre-presentation `make -j8 check TESTS=Break`: 38/38 passed. The later
  external configured-grant fixture was not part of that original linked ELF.
* Current `make -j8 check TESTS=Break`: **54/54 passed**, including nine configured-
  grant tests and seven new presentation tests (with parameterized variants).
  The first expanded run had one invalid AI-test directive in the new fixture;
  it was corrected to use real forced reentry, then the complete group passed.
* `python3 'docs/extra docs/validate_break_bar_examples.py'`: six C blocks and
  the guide's new battle scripts compile. This is not runtime verification of
  uninstalled example encounters.
* Normal ROM `make -j8`: **passed**, producing `pokeemerald.gba`. Existing missing-
  trailing-newline warnings in three map scripts were observed; those scripts
  were not changed.
* Full preserved baseline: **6,315 total; 5,846 passed; 12 failed; 21 known failing;
  427 TODO; 9 expected failing**.
* Full current snapshot: **6,331 total; 5,862 passed; 12 failed; 21 known failing;
  427 TODO; 9 expected failing**. The same 12 test names failed with the same
  failure conditions. No new failure was observed. All 16 additional tests
  (nine external configured-grant tests, seven presentation tests) passed.
  This is a successful regression comparison, not a completely green full suite.

The 12 baseline failures are: adjacent TV-show preservation in a shiny-wild test;
two existing text-width checks (Castoria name text and Roar of Eternity);
More Wishes revival; two Koanko fixture/continuation assertions; two TX
Restoration message expectations; three AI timing/move-choice checks; and a
Gen 9 wild Thief/Covet inventory assertion. The two Restoration cases fail on
the same missing message in both executables. Detailed names, locations and
assertions are in both full-suite logs. These were compared and left outside
this presentation task's scope.

The source snapshot was held stable during these runs. Logs stay alongside this
report. The original pre-presentation ELF was copied to
`tmp/break-bar-validation/pre-presentation-test.elf` before relinking; SHA-256:
`3b78de74b01e754c5154121d83b7ee025e1ef1c1c0add730a2d47edeb9586ae6`.
Its copy was patched to an empty filter for comparison. The first serial full
baseline attempt was stopped and restarted with eight workers; partial output
was retained temporarily in `tmp/break-bar-validation/baseline-full-partial.log`.
Temporary ELFs, extracted snippets and objects were removed after comparison;
the snippet validator regenerates its own temporary directory when run again.

## Preceding milestone's validated snapshot and acceptance

The preceding milestone on `break-bar-presentation-authoring` was validated
without a commit or push. Its historical source file list and hashes are in
[`break_bar_validation_snapshot.sha256`](break_bar_validation_snapshot.sha256).
It includes the externally added granting code and fixture as well as the initial
presentation changes. The correction below has its own current source manifest.
`git diff --check` passes, and the source hashes matched after the test/build runs.

Important presentation files are `src/battle_break_bar.c`,
`src/battle_switch_in.c`, `include/battle_break_bar.h`, `include/battle.h`,
`include/battle_scripts.h`, `data/battle_scripts_1.s`, and
`test/battle/break_bar_presentation.c`. The upstream delta is the narrow event hook,
battle-only bit and returning script declaration/label described in the ledger
supplement. Gameplay grant/phase policies were preserved rather than refactored.
No production encounter has been opted into fills; authors select that explicitly.

For a repeat of the validated checks, from the repository root in WSL:

```sh
sha256sum -c 'docs/extra docs/break_bar_validation_snapshot.sha256'
python3 'docs/extra docs/validate_break_bar_examples.py'
rm -f pokeemerald-test.elf pokeemerald-test-headless.elf
make -j8 check TESTS=Break
rm -f pokeemerald-test.elf pokeemerald-test-headless.elf
make -j8 check TESTS=
make -j8
git diff --check
```

The full check exits nonzero for the 12 recorded baseline failures. The authoring
guide also lists exact file filters for narrower trainer/wild/grant/exit checks
and explains why filter changes need a link step. Per-frame palette/VRAM tests
verify successive colors, exact final fraction, unchanged battle/party data
(apart from the shown bit), controller waits, deferred/inactive behavior, missing
graphics, menu-style reshow, forced reentry and independent doubles palettes.
The wild entry test also verifies Pressure ordering and the Charm's covered HP.

Remaining acceptance is **manual graphical review**: fill pacing/count, first
visible color, popup/healthbox choreography, form examples when installed, and
menu/battle exit appearance. No desktop automation was used. Example C/scripts
compile, but the example encounters were not installed or runtime-tested.

Grant fills are part of the existing presentation work. The guide no longer
labels them as a missing feature or proposes grant-budget fields. New encounter
grants stay script-owned and rare, reserved for major encounters. Remaining
extensions are explicit heal/grant recipes, optional typed failure feedback with
author/debug diagnostics excluded from release compilation, and script-composed
grant-time HP/stat/form/move effects. No runtime behavior was changed for these
documentation clarifications. Additional handoff requirements will be considered
when the user supplies that later handoff.

## Grant-animation and diagnostics correction

The current requested correction is to remove the remaining configured-grant
color fade and implement typed failure feedback with release-excluded diagnostics.
Covering grants retain their existing saved-layer fill. No grant budgets,
AI-selectable action, automatic grant-time gameplay callback or AI policy is added.
The granting encounter script owns all grant-time HP/stat/form/move effects and
decides whether and when to grant. The later handoff has not been supplied.

Default initial activation/entry is immediately the current colored bar.
Every scripted grant fills its newly added bar: the plain grant command waits
for a visual-only single-layer fill to actual HP; the configured-cover command
fills the new full-HP layer using the existing saved overlay. Generic saved
grants retain their established item/move-script animation ordering. An explicit
`showbreakbarfills battler` script command presents successive fills only
when the owner's config has `BREAK_BAR_INTRO_FILLS`; deferred drawing still wins.
It may deliberately repeat the presentation at a scripted major-encounter beat,
without changing HP, counts, saved HP or gameplay effects. Automatic flagged
entry presentation remains once per owner; reshow cancels rather than replaying.
The three contracts are separate: immediate initial color, fill on every grant,
and explicitly enabled successive multi-layer entrance presentation. Grant
commands finish animation before subsequent encounter-script gameplay effects.

A non-mutating typed eligibility query distinguishes ready, no owner/config,
invalid config, pending break, dead/absent owner and full configured capacity.
An explicit script query can expose the reason without replacing the existing
boolean/failure-label APIs. Only failed grant attempts emit author diagnostics;
preflight queries stay quiet. Developer diagnostic code/text is guarded by
`#ifndef RELEASE`; runtime eligibility and script branching remain in release.
No diagnostic becomes an automatic player-facing battle message.

This plan was written before the correction source edits. The implementation is
now complete and validated as follows:

* `make -j8 check TESTS=Break`: **58/58 passed**, including four new tests for
  typed eligibility, plain fill state/tiles, explicit flagged reveals and actual
  plain/cover granting-script completion. Parameterized variants include partial
  HP, five layers, deferred registration, controller waits and cancellation.
* Trainer assignment/override fixture: **11/11 passed**; scripted-wild/capture
  fixture: **4/4 passed**. These used independently filtered copies of the same
  linked current test ELF; no source changed between runs.
* Normal `make -j8` and `make -j8 release`: **passed**. The release build emitted
  warnings in unchanged existing code (including LTO type mismatches); it produced
  `pokeemerald-release.gba` successfully.
* Guide validator: six C snippets and five battle-script blocks compile, including
  the real new presentation/query macros. These remain authoring recipes rather
  than installed gameplay encounters.
* `verify_break_bar_release_diagnostics.py`: **passed**; the diagnostic marker is
  present in the debug/test ELF and absent from the release ROM. The reason-string
  table and logging call are explicitly inside `#ifndef RELEASE` in source.
* `git diff --check`: passed. The earlier full-suite comparison above applies to
  the preceding milestone; the full suite was not rerun for this focused correction.

### Current implementation and upstream ledger delta

`src/battle_break_bar.c` owns a shared `BreakBarFillAnimation` frame primitive,
separate per-battler intro/grant state, and both sets' reshow/cleanup cancellation.
The plain grant snapshot records owner/config, HP/max HP, count and healthbar
identity; only palettes/tiles change during its fill. `AnimateConfiguredBreakBar`
and its native finish before the next granting-script effect. Cover grants keep
the existing canonical saved pair and saved-fill natives. C grant functions stay
low-level state operations; complete battle-script macros own presentation.

`asm/macros/battle_script.inc` now appends the proper fill continuation to each
configured-grant macro. `BattleScript_ConfiguredBarHealThenCover` in
`data/battle_scripts_1.s` removes its former separate fill calls to avoid a double
animation. This is a small change to an existing script integration, not a new
engine hook. The same file includes the custom result constants and contains
test-only grant scripts guarded by `#if TESTING`.

`include/constants/battle_break_bar.h` defines typed result values shared by C
and scripts; `include/battle_break_bar.h` declares the query/presentation natives.
`getconfiguredbreakbarresult` explicitly writes the chooser scratch byte. Grant
validation is retained in release; author log strings/calls are excluded. No
AI source, move data, grant budget, new gameplay callback or battle topology was
changed. The script performing the grant owns HP/stat/form/move effects.

The upstream maintenance impact is confined to the existing script file/macros;
the frame logic and diagnostics remain in the custom module. Review appended
macro continuations when upstream changes script/controller completion. Existing
cover-grant callers must not append a second `fillsavedbreakbar`/
`animatesavedbreakbar` after the now-complete cover macro. Registration's defer
flag does not suppress a scripted grant's required fill. No additional upstream
event/controller hook or persistent state field was added in this correction.

### Current handoff

The stable current source is recorded in
[`break_bar_grant_validation_snapshot.sha256`](break_bar_grant_validation_snapshot.sha256).
Keep those files stable during validation. All source edits are uncommitted on
`break-bar-presentation-authoring`; other pre-existing changes are preserved.
Logs for this correction use the `break_bar_grant_...` names alongside this report.
The release exclusion check has its own log and repeatable helper.

Repeat from the repository root in WSL:

```sh
sha256sum -c 'docs/extra docs/break_bar_grant_validation_snapshot.sha256'
rm -f pokeemerald-test.elf pokeemerald-test-headless.elf
make -j8 check TESTS=Break
make -j8
make -j8 release
python3 'docs/extra docs/verify_break_bar_release_diagnostics.py'
python3 'docs/extra docs/validate_break_bar_examples.py'
git diff --check
```

Manual mGBA acceptance still remains: first visible color, each grant's fill,
explicit flagged successive fills, covered-layer colors, pacing and menu return.
Automated checks verify state, palette/VRAM, synchronization and script returns,
but no desktop automation or manual graphical acceptance was performed.

## First-visible-frame correction: documentation-first checkpoint

The user observed green HP bars snapping to Break Bar colors in the visual test
ROM, while TX entered with its correct bar. Source inspection found that the
main `test/battle/break_bar.c` fixture calls `ActivateBreakBar` from its dynamic
AI score function. That occurs after the healthbox is visible. Removing the
fade therefore exposed the fixture's late registration instead of fixing it.
TX registers before creating its healthbox; real trainer/wild defaults are also
registered before graphics. Tests that intentionally activate/grant a bar later
are different from an encounter authored to start with bars.

Planned correction: a test-only `BreakBars(config)` Pokemon setup field stores
static config pointers by trainer/party member. A `#if TESTING` initialization
hook applies those pointers after battle-state allocation and before healthbox
creation. The main entry-stack fixtures will use that field; their AI callback
will record counts only. A test-only observer will capture the first visible
bar's palette/color, and a regression will assert correct first-frame colors
for one through five extras. No production ability, hazard or grant timing will
move earlier. Kazuradrop's ability ownership and TX's established initialization
remain unchanged.

The green-to-color task implementation will be retained but compiled out, as
requested by the user for possible future reuse. The existing
`RefreshBreakBarPalette` calling signature stays compatible, but refreshes are
always immediate. Damage/break palette ordering and intentional fill sequences
are unchanged. The new engine seam is strictly test-only, in battle setup;
fixture pointers and observations remain in test-runner data, not saved or
recorded Pokemon. Recheck this hook when upstream changes setup allocation or
graphics ordering. Source changes and validation are pending at this checkpoint.

### Implemented correction and validation

The checkpoint above was written before the source edits. The correction is now
implemented. `BreakBars(config)` stores static per-member fixture configs in
`BattleTestData`; `TestRunner_Battle_InitializeBreakBars` applies them from a
`#if TESTING` hook in `CB2_InitBattle` after party-state allocation and before
healthbox creation. The main entry-stack tests no longer call activation from
AI scoring. Their AI callback only samples remaining counts.

A test-only task waits for controller-assigned positions/party indexes, then
captures each configured initial healthbox's palette on its first visible frame.
The new assertions cover all five colors, both sides, partial HP and all four
doubles positions. Fixture pointers and observations are cleared with normal
test-runner data; they are not recorded/saved Pokemon fields. The observer
destroys itself after the initial boxes are seen. Runtime activation/grant tests
still deliberately register their effects later, as their contracts require.

The full legacy lerp implementation remains in `battle_break_bar.c`, guarded by
the local `BREAK_BAR_ENABLE_LEGACY_LERP` constant set to zero. Its declarations,
allocation branch, task body and task scan are compiled out. The old refresh
argument remains source-compatible, but even an explicit TRUE request cannot
enable the disabled code. The test ELF's symbol table was checked: the legacy
task/helper are absent. Intentional grant and flagged successive fills remain.

Observed results on this snapshot:

* `make -j8 check TESTS=Break`: **61/61 passed**.
* Both first-visible tests with `gTestRunnerHeadless = FALSE`: **2/2 passed**,
  including all five single-battle parameterizations and independent doubles
  colors. These ran the graphical intro paths in the non-GUI emulator runner;
  this is still automated validation, not manual aesthetic acceptance.
* Trainer fixture **11/11 passed**; wild/capture fixture **4/4 passed**.
* Normal ROM and release ROM builds passed. Existing release warnings were
  observed in unchanged code. Release diagnostic exclusion still passes.
* The full suite was not rerun for this focused correction. Its earlier
  baseline failures remain recorded above, without claiming new full-suite results.

### First-visible handoff and integration ledger update

Important new integration files: `include/test/battle.h` (fixture field and
observations), `include/test_runner.h` (test-only declaration),
`test/test_runner_battle.c` (initialization and observer), and
`src/battle_main.c:CB2_InitBattle` (four-line `#if TESTING` setup hook).
`test/battle/break_bar.c` migrates the initial-stack fixtures;
`test/battle/break_bar_presentation.c` adds the first-frame and disabled-lerp
regressions. The runtime module only disables the retained old lerp machinery.
There is no new production graphics hook or changed gameplay registration timing.

Upstream maintenance: the one added setup hook exists only in test builds.
Review allocation and healthbox/controller ordering if upstream moves battle
initialization. Test-data layout changes do not change recorded/save formats.
This is preferable to moving an ability's real activation ahead of hazards or
adding species checks to healthbox drawing, which would change gameplay policy.

Current stable source hashes are in
[`break_bar_first_visible_validation_snapshot.sha256`](break_bar_first_visible_validation_snapshot.sha256).
The branch remains `break-bar-presentation-authoring`, with no commit/push.
The pre-existing `.gitignore` change and other work are preserved.

The root `pokeemerald-test.elf` has been rebuilt as an interactive artifact with
`TESTS='Break Bar initial'` and normal graphical intro paths. SHA-256:
`66a1dadbc1678f3e4201ea387a74c026acb00eb661d827656abb0d90bcdf2def`.
Open this exact artifact in mGBA for the remaining user visual check. Rebuild it
from WSL with:

```sh
rm -f pokeemerald-test.elf
make -j8 pokeemerald-test.elf TESTS='Break Bar initial'
```

For the broader focused regression, relink using `make -j8 check TESTS=Break` as
described above. Keep the source manifest stable during testing. The targeted
artifact avoids confusing intentional later-activation unit tests with entrance
acceptance. First visible color and pacing still await the user's mGBA review;
no desktop automation was used.

## Heal/grant and successive-intro visual verification checkpoint

The user has now accepted the first visible frame in the interactive ELF above.
That ELF was intentionally filtered to `Break Bar initial`; it did not include
the scripted heal/grant or successive-fill demonstrations. Those two sequences
remain visually unverified.

Before further source edits, the next verification fixture is specified here:

* A test-only returning phase script first refills a broken owner to 37/100 HP,
  then calls the production `BattleScript_ConfiguredBarHealThenCover` helper.
  This exposes its real ordering: heal 37 to 100, save that healed pair, grant
  the configured covering layer, and wait for its fill before the next effect.
  A following break must restore the healed 100/100 pair. Grant timing belongs
  to this battle script; neither an AI choice nor a grant move is introduced.
* Initial configurations with `BREAK_BAR_INTRO_FILLS`, installed before graphics
  through `BreakBars(config)`, demonstrate three full layers and five layers
  ending at 37/100 HP. The final count/HP and intro completion are asserted.
* A returning phase script explicitly invokes `showbreakbarfills` for a flagged
  owner, exercising the author's replay trigger independently of entry.

These cases will share the `Break Bar visual` test-name prefix for one
interactive ELF. Only test fixtures/scripts are planned; no production engine
integration point or gameplay policy needs to change. Existing fill-state
ownership, controller waits and cleanup remain as documented above. Automated
state/script checks and graphical-engine runs will be recorded separately from
the user's remaining visual acceptance.

### Heal/grant and successive-intro verification results

The documentation-first checkpoint for these fixtures is implemented. New cases
in `test/battle/break_bar_presentation.c` and returning scripts in the existing
`#if TESTING` block of `data/battle_scripts_1.s` cover:

* `Break Bar visual scripted heal then covering grant fills and later restores
  the healed HP`: an ordinary configured layer breaks; its script refills to
  37/100, then calls the actual production heal-then-cover helper. Scene checks
  require the heal animation and 37-to-100 HP update. A native marker after the
  helper requires one granted layer, a saved 100/100 pair and a completed 48-pixel
  fill. A second break restores 100/100 without fainting or granting again.
* `Break Bar visual multi-bar fill intro visits each layer and ends at the real
  HP fraction`: pre-graphics flagged configs exercise three full layers and
  five layers ending at 37/100. Final colors, HP/count, completion and target
  stack cleanup are asserted. Existing frame-by-frame tile tests also remain
  in the broader focused regression set.
* `Break Bar visual script trigger replays two remaining fills after the
  three-layer intro`: entry presents three layers, then a break refills the
  remaining bar and its returning phase script explicitly calls
  `showbreakbarfills`, presenting purple then blue before its next native.

The one-shot heal/grant gate and captured observations are test-file static
state, reset in `GIVEN` before each run. The phase script checks the gate again
after restoration and returns. No AI hook selects or schedules this grant.
Configs are static and installed with the existing `BreakBars(config)` setup;
fill ownership/cleanup remains in the production presentation module.

Actual validation:

* `make -j8 check TESTS='Break Bar visual'`: **3/3 named tests passed** (the intro
  test has two parameterizations).
* The same interactive ELF, with `gTestRunnerHeadless = FALSE`, ran through the
  non-GUI emulator runner: **3/3 passed** with graphical battle paths enabled.
* An isolated copy patched to `TESTS=Break` and headless execution:
  **64/64 passed**. The root interactive artifact was not changed by this run.
* `git diff --check` passed. No production logic changed in this checkpoint;
  normal/release builds and the full unrelated test suite were not rerun.

Logs are `break_bar_visual_tests.log`,
`break_bar_visual_graphics_enabled_tests.log` and
`break_bar_visual_regression_tests.log` alongside this report. The stable source
manifest is `break_bar_visual_validation_snapshot.sha256`. No new production
upstream integration point was added: only test fixtures and the existing
test-only script block changed, keeping the engine merge-maintenance footprint
unchanged. No commit/push was made.

The current root `pokeemerald-test.elf` is interactive and filtered to
`Break Bar visual`. SHA-256:
`19c54f528d4f54c2c4dd8c782aa8324307751e2f199b2ad68cce69b9de3ff6c5`.
From WSL, reproduce the exact visual selection with:

```sh
sha256sum -c 'docs/extra docs/break_bar_visual_validation_snapshot.sha256'
rm -f pokeemerald-test.elf
make -j8 pokeemerald-test.elf TESTS='Break Bar visual'
```

For the focused headless regression instead, force relinking for its filter:

```sh
rm -f pokeemerald-test.elf pokeemerald-test-headless.elf
make -j8 check TESTS=Break
```

Keep the manifest's source files stable during validation. The user has accepted
the earlier first-frame color result. Heal/grant animation and successive-fill
pacing still await the user's visual review of this new artifact; automated
graphical-path passes are not manual visual acceptance.

## Dramatic-fill correction: documentation-first checkpoint

The user requests a full ordinary green base at the start of the opted-in
dramatic presentation, then successive colored layers covering it at a
deliberate pace. The previous implementation instead drew each layer over
empty tiles at three pixels per frame. This checkpoint supersedes that visual
recipe; ordinary immediate-color entrance remains the default.

Planned implementation:

* Before an eligible automatic intro has been shown, its healthbox displays a
  full ordinary green bar from its first visible frame. No HP, max HP, layer
  count, saved HP pair or numeric HP value changes. Deferred-draw profiles
  retain control of their graphics.
* The intro holds that base for 12 frames, then fills one pixel per frame,
  approximately 0.8 seconds for each full layer at GBA refresh speed. The
  preceding layer's colors occupy the overlay renderer's underlying-color pair;
  purple therefore covers green, blue covers purple, and so on. A partial top
  layer's uncovered right edge retracts as it fills, ending at actual HP without
  a final length snap. Plain configured grants retain their three-pixel step;
  saved/cover fills and the disabled color lerp remain unchanged.
* Add a narrow `DrawBreakBarIntroBase` guard to the existing HP rendering seam
  in `battle_interface.c:MoveBattleBarGraphically`. It replaces initial tiles
  only for an eligible unshown flagged intro. Palette selection, eligibility,
  timing and compositing stay in the dedicated Break Bar module. This hook is
  needed because palette selection alone cannot make a partial starting HP
  bar appear full. Upstream merge review must preserve its position after
  palette refresh and before ordinary tile copying; tests cover unflagged
  first-frame colors and partial-HP flagged entry. No core bar algorithm changes.
* Extend the existing test-only first-visible observer with a full-green tile
  observation. Frame tests must verify the green hold, layered compositing,
  slower timing, exact final redraw, cancellation and unchanged gameplay state.
  Graphical-path runs, focused regressions, normal/release builds and a new
  interactive `Break Bar visual` ELF will be recorded after implementation.

Transient hold/timing data remains module-owned per battler, cleared by the
existing reshow/teardown paths. The party-owned shown bit retains its lifetime.
Explicit `showbreakbarfills` uses the same green-base hold and layered animation.
Source changes and new validation are pending at this checkpoint.

### Implemented dramatic fill and validation

The checkpoint above is implemented. The automatic flagged entrance displays a
full ordinary green base before graphics become visible; the test observer
captures its palette and tiles, including the 37/100 starting-HP case. The intro
then holds green for 12 controller-idle frames. Each colored layer covers the
previous one at one pixel per frame, approximately 0.8 seconds per full layer.
Five full layers take 257 animation calls including the hold and endpoints.
The partial final layer retracts its uncovered right edge during the fill;
completion restores the exact authoritative tiles and normal palette entries.
Real/numeric HP, max HP, layer counts and saved/cover state are unchanged.

`BreakBarFillAnimation.baseFrames` belongs to the existing module-owned transient
state. Start initializes it only for intros; controller waits do not consume it.
The existing owner/config/HP/count/graphics guards still cancel stale fills.
Finish restores the underlying-color palette pair, and reshow/teardown clear
the state. Plain grants still use three pixels per frame; saved/cover grants
still use their separate fill natives. `START_INACTIVE` profiles do not pre-draw
a full initial stack, while an explicit `showbreakbarfills` beat still receives
the green hold. Deferred-draw profiles continue to opt out.

Upstream integration ledger delta: `src/battle_interface.c`,
`MoveBattleBarGraphically`, adds a two-line `DrawBreakBarIntroBase` guard after
palette refresh and before ordinary HP tile copying. This narrow graphics hook
is necessary to show a full green base when actual HP is partial. All selection,
eligibility, pacing and compositing live in `src/battle_break_bar.c`; declaration
is in `include/battle_break_bar.h`. Merge review must preserve call ordering and
valid healthbox graphics. No healthbar arithmetic, damage/heal algorithm, grant
eligibility or battle-script opcode was changed. Tests cover both the default
immediate-color route and flagged entry, controller waits, reshow, partial HP,
saved grants and independent colored layers in doubles.

`include/test/battle.h` and `test/test_runner_battle.c` extend the existing
first-visible observation with six bar tiles per battler. The observer only
copies them; it does not redraw or mutate the observed frame. The new frame
test in `test/battle/break_bar_presentation.c` checks the full green hold and
each one-pixel covering step, previous-layer colors, final palette cleanup and
unchanged gameplay state. The first test run was 64/65: the existing doubles
test expected private palettes before any intro began. It now asserts that
ordinary green bases share their normal palette, then that each active fill
has an independent private palette. No runtime fix was needed for that failure.

Actual results after the fixture correction:

* `make -j8 check TESTS=Break`: **65/65 passed**.
* All **65/65 passed** again with `gTestRunnerHeadless = FALSE` through the
  non-GUI emulator runner, including the first-visible full-green tile checks.
* Trainer configuration **11/11 passed**; wild configuration/capture
  **4/4 passed**.
* Normal and release ROM builds passed. Previously observed release warnings
  remain in unchanged code. The diagnostic marker remains present in the test
  ELF and absent from the release ROM.
* The final interactive `Break Bar visual` artifact passed **3/3 named tests**
  with graphical paths enabled. The intro case includes both three full layers
  and five layers ending at partial HP.
* `git diff --check` and the seventeen-file source manifest passed. The full
  unrelated suite was not rerun for this correction.

Logs use the `break_bar_dramatic_` prefix beside this report. The initial failed
fixture expectation is retained in `break_bar_dramatic_initial_tests.log`;
the corrected run is `break_bar_dramatic_tests.log`. The current stable source
snapshot is `break_bar_dramatic_validation_snapshot.sha256`. No commit/push.

The root `pokeemerald-test.elf` is interactive, filtered to `Break Bar visual`.
SHA-256:
`2e622cc9bf141063bd8e88bf462f50baa6958ccc22e3288e8e6eb39913f20b12`.
Reproduce from WSL, keeping the manifest's source files stable:

```sh
sha256sum -c 'docs/extra docs/break_bar_dramatic_validation_snapshot.sha256'
rm -f pokeemerald-test.elf pokeemerald-test-headless.elf
make -j8 check TESTS=Break
rm -f pokeemerald-test.elf
make -j8 pokeemerald-test.elf TESTS='Break Bar visual'
```

This artifact retains the heal-then-cover demonstration alongside the corrected
dramatic entry and explicit replay. Automated graphics-path checks verify tiles,
timing and state; the new pace and covering effect still await the user's visual
acceptance. The earlier accepted first-frame result applies to unflagged entry.

## Ordinary breaks after dramatic entry: documentation-first correction

The user observed the all-layer presentation repeating after a layer broke.
The visual fixture added here explicitly installed `BattleScript_TestBreakBarReplay`
as its outer layer's `afterBreakScript`; that script called `showbreakbarfills`.
This was an inappropriate demonstration of the requested normal break behavior.
The production break resolver does not reset the party-owned intro-shown bit
or automatically replay a completed intro.

Before editing source, the correction is specified as follows: remove the
replay-only test script and the visual config which attaches it to a break.
Replace that visual case with a three-layer flagged entry using ordinary NULL
phase hooks, then break every layer in three turns. Scene checks require one
normal refill from zero to full after each break; the end state must be a live
full red base, no remaining extras, no pending intro and a preserved shown bit.
The separate explicit-command API tests remain; no ordinary break should invoke
that command. The heal/grant visual fixture stays in the selection.

Only test fixtures and the existing `#if TESTING` script block need changes.
There is no new production integration point or upstream maintenance impact.
The default resolver and refill script remain authoritative, and their
target-stack cleanup must still pass. Validation and an updated interactive
`Break Bar visual` artifact are pending at this checkpoint.

### Implemented normal-break visual correction

Removed `BattleScript_TestBreakBarReplay` from the test-only script block in
`data/battle_scripts_1.s` and removed its phase-bound config from
`test/battle/break_bar_presentation.c`. The replacement visual case uses the
ordinary three-layer intro profile with NULL phase hooks. Three Seismic Tosses
each break one layer and invoke the existing single-bar refill. Scene checks
require three zero-to-100 refills; final assertions require a live 100/100 red
base, zero extras, the ordinary palette, a preserved intro-shown bit, no pending
intro, no faint and a balanced target stack. Explicit-command coverage remains
in the separate API tests, without an automatic replay in this demonstration.

Validation on this snapshot:

* `make -j8 check TESTS=Break`: **65/65 passed**.
* The same focused set with graphical battle paths enabled in the non-GUI
  runner: **65/65 passed**.
* The exact final interactive `Break Bar visual` ELF: **3/3 passed** with
  graphical paths enabled, including all three normal breaks and the retained
  heal/cover demonstration.
* `git diff --check` and the source manifest passed. Normal/release builds and
  unrelated tests were not rerun: this correction changes test fixtures only,
  with no production engine or integration change. Previous build results above
  remain historical.

Logs use the `break_bar_normal_break_` prefix alongside this report. Stable source
hashes are in `break_bar_normal_break_validation_snapshot.sha256`. The root
interactive `pokeemerald-test.elf` remains filtered to `Break Bar visual`.
SHA-256:
`885a075afa3c2cb9c2a2d41d98ca3b8f62b4463f27f758aef03ec7e04dac6d5e`.

Reproduce from WSL with the manifest's source files held stable:

```sh
sha256sum -c 'docs/extra docs/break_bar_normal_break_validation_snapshot.sha256'
rm -f pokeemerald-test.elf pokeemerald-test-headless.elf
make -j8 check TESTS=Break
rm -f pokeemerald-test.elf
make -j8 pokeemerald-test.elf TESTS='Break Bar visual'
```

User visual acceptance of this corrected sequence remains pending. No
commit/push was made; the working branch and unrelated modifications are
preserved.

## Dramatic startup red flash: documentation-first checkpoint

The user reports green, then a red frame, then green immediately before the
dramatic fill. The current startup displays ordinary green tiles using the
shared palette, then switches the sprite to a private palette while writing
underlying tiles which use entries 14/15. Those tile writes go directly to VRAM;
the sprite's palette slot reaches hardware through VBlank OAM upload. The old
shared palette maps 14/15 to red. Existing tests observed the completed software
palette/tiles together and did not cover that handoff.

Before source edits, the correction is specified here: allocate the intro's
private palette while its initial healthbox is still being prepared. Both fill
color pairs initially hold ordinary green; initial tiles retain the ordinary
green indices, which are safe even before the new slot reaches hardware.
Startup retains that palette slot, stages the underlying
green while preserving currently displayed top colors, then yields once before
drawing the green hold. Build complete temporary palettes before loading them,
so no intermediate red underlying pair is exposed. At a completed colored
layer, stage its colors into the now-hidden underlying pair before the next
frame begins covering it with the next color. This also addresses the analogous
palette/tile handoff between layers.

Add a regression for stable palette ownership and startup staging before fixing
the runtime, then update existing timing/compositing checks for the preparation
frame. One pixel per frame and the 12-frame green hold remain; the startup yield
adds one call (258 maximum for five full layers). Default immediate-color entry,
grants, normal breaks after entry, explicit command gating, cancellation and the
compiled-out legacy lerp retain their contracts.

Implementation stays in `battle_break_bar.c`, using the existing renderer hook;
no additional upstream integration or generic graphics algorithm change is
planned. Transient preparation state belongs to the existing per-battler fill
snapshot and is cleared by existing reshow/teardown. Validation and the updated
interactive ELF remain pending at this checkpoint.

### Implemented palette staging and validation

The new startup regression failed against the previous implementation: the
initial base still used the shared ordinary palette, which startup replaced.
It now passes with a private palette assigned during initial bar setup and
retained across startup. Both fill pairs initially contain ordinary green.
Initial tiles still use ordinary green indices, so a late visible activation
also stays green while the old hardware palette selection is in effect.

`SetBreakBarIntroPalette` builds a complete palette locally before loading it;
there is no intermediate load with a red underlying pair. The first intro poll
preserves the existing top colors and tiles, stages underlying green, and
yields. Only the following poll switches to underlying tiles for the green
hold. Explicit reveals over an already colored bar use the same staged handoff.
When a layer becomes fully covered, its colors are copied into the now-hidden
underlying pair before the next frame starts the next colored fill.

All runtime edits are in `src/battle_break_bar.c`. Composed palettes invalidate
the ordinary color cache with `0xFF`, ensuring finish/cancellation reloads the
authoritative palette. `BreakBarFillAnimation.preparing` uses the existing
transient state's lifecycle; reshow and teardown clear it. The same one private
slot per battler is allocated earlier, without adding another slot. Existing
upstream graphics/switch hooks, save/party layouts, grant behavior and normal
single-bar refills after breaks are unchanged. Merge-maintenance impact is
confined to this dedicated module's implementation behind the existing hooks.

`test/battle/break_bar_presentation.c` now checks private ownership before
startup, unchanged tile indices while the old hardware slot may still be
selected, preparation before switching to underlying tiles, explicit reveal
staging over a blue bar, and the next layer's underlying pair being ready at
the preceding layer's completion. Existing tests retain HP/count/state and
palette-cleanup assertions. The green hold is still 12 frames, each full layer
still advances one pixel per frame, and five full layers now take 258 calls
including the preparation yield.

Actual results:

* The new regression failed on the preceding source, as captured in
  `break_bar_startup_flash_reproduction.log`.
* `make -j8 check TESTS=Break`: **66/66 passed** after the correction.
* All **66/66 passed** with graphical battle paths enabled in the non-GUI
  emulator runner.
* Trainer configuration **11/11 passed**; wild configuration/capture
  **4/4 passed**.
* Normal and release ROM builds passed. Previously observed warnings remain
  in unchanged release code. Grant diagnostic exclusion passed.
* The final interactive `Break Bar visual` ELF passed **3/3 named cases** with
  graphical paths enabled, retaining the heal/cover sequence and three normal
  breaks after the dramatic entrance.
* `git diff --check` and the seventeen-file source manifest passed. The full
  unrelated suite was not rerun for this localized correction.

Logs use the `break_bar_startup_flash_` prefix alongside this report; stable
source hashes are in `break_bar_startup_flash_validation_snapshot.sha256`.
The root interactive `pokeemerald-test.elf` is filtered to `Break Bar visual`.
SHA-256:
`40d9b3ca102527d583a39d1ce90c0e4cae59178e25d0d812af1c816d4f86aca6`.

Reproduce from WSL, keeping the source manifest stable:

```sh
sha256sum -c 'docs/extra docs/break_bar_startup_flash_validation_snapshot.sha256'
rm -f pokeemerald-test.elf pokeemerald-test-headless.elf
make -j8 check TESTS=Break
make -j8
make -j8 release
python3 'docs/extra docs/verify_break_bar_release_diagnostics.py'
rm -f pokeemerald-test.elf
make -j8 pokeemerald-test.elf TESTS='Break Bar visual'
```

Automated checks cover palette ownership, preparation order, tile indices and
graphical execution paths. On 2026-11-17, the user visually accepted the corrected
startup transition in this exact artifact ("there we go"), confirming resolution
of the reported green/red/green flash. No desktop automation, commit or push
was performed.
