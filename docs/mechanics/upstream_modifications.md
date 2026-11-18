# Upstream modification ledger

## Koanko Stage 09 entry and revised overcharge (2026-11-18)

Completed adaptation integration: normal ROM build passed; all 23 Command Seal,
172 Koanko, 30 Arceus and 5 real-animation staging groups passed on 2026-11-18.
Earlier pending-implementation/validation notes below describe earlier snapshots;
[Stage 09 section 16](../extra%20docs/KoankoDev/planning/09_arceus_bug_space_and_conversion.md#16-completed-implementation-and-validation-handoff-2026-11-18)
records current coverage, remaining user visual acceptance and the Stage 09.1 hold.

- `battle_move_resolution.c:TryMoveDamageUpdate` and
  `battle_script_commands.c:Cmd_datahpupdate/Cmd_setatkhptozero`: narrow commit
  notifications distinguish the actual zeroing attack type from untyped
  scripted/self-KO damage. Custom ownership/layer checks live in
  `battle_koanko_arceus.c`. Preserve ordering before bar queuing when merging;
  test dynamic/called/multihit/instant-death sources and ordinary bars.
- `battle.h:FutureSight`, `battle_move_resolution.c:SetFutureSightAttack` and
  `battle_end_turn.c:HandleEndTurnFutureSight`: store resolved impact type before
  yielding. The delayed attack setup also clears stale dynamic type when its
  source is benched. This touches shared delayed-attack semantics and needs
  Future Sight/Doom Desire regressions in addition to Arceus capture tests.
  Added storage is battle-owned EWRAM. The normal link reports EWRAM 235,608 B
  and IWRAM 28,808 B. Individual field padding/runtime heap high-water is unmeasured.
- `battle_main.c:GetDynamicMoveType`: narrow owned-Judgment query precedes
  item-driven type selection, including active-mon simulation. Verify no-Plate
  and conflicting-Plate Judgment, reserve simulation and ordinary Arceus.
  Future changes to AI move typing must retain runtime/query parity.
- `battle_util.c:TryBattleFormChange`: owner-specific early guard preserves the
  charged form across ordinary refresh methods, allowing faint/end cleanup.
  Verify Multitype/item refresh and unrelated form changes. Method additions
  upstream need semantic review rather than blindly retaining the guard.
- `battle_util2.c:FreeBattleResources`: one cleanup call before freeing battle
  state restores only the owned party species and removes the owned field/UI.
  Verify genuine faint, interruption, foreign ownership and repeated cleanup.
  Canonical battle-party species supports existing menu/graphics loading; no
  general graphics loader or animation algorithm was modified for adaptation.

The custom `battle_break_bar.c:TryBreakBar` continuation now includes the tagged
adaptation in preflight and pushes it after saved-HP restoration. No original
saved-layer phase scripts are enabled by this hook. Verify stack exhaustion,
older saved/configured layers and exactly one adaptation. Production
`battle_koanko.c:ConjureKoankoTeam` installs the staged entry only in the rebuilt
Arceus party slot. Runtime fixtures in `koanko_arceus_adaptation.c` now exercise
these paths; `koanko.c` verifies regeneration and twelve genuine faints.
Most hooks are small textual changes, but shared delayed damage, form refresh
and simulation introduce semantic maintenance risks requiring the regressions
above. The packed encounter record is now 12 EWRAM bytes; no new IWRAM code or
data is requested.

The dedicated adaptation payload preflights five free task slots before using
the existing form mosaic, preserving the generic task allocator/animation code.
Its reveal native reloads actual graphics/palette after an animation is skipped
or completes, guards missing sprites and leaves Substitute art intact. Scarce
task, real-animation palette/shiny/rebuild and cleanup tests pass. The finite
opening handoff now proceeds without stealing another active Bug Space field;
the strict public dismissal API still rejects foreign owners.

Adjacent results: Entry scripts 16, Trainer Slide 75, Break Bar 66, Future Sight
19, Conversion 2 13, Transform 14, Polymorph 9 and Wish Endurance 10 all passed.
Kazuradrop had 14 passes and its existing marked Stack Breaker task-leak failure.
Standalone Doom Desire/Multitype prefixes selected no runnable tests; their
Arceus-specific behavior is covered by the new runtime tests. Source and built
ROM are stable for user visual review; no Stage 09.1 work was started.

Graphics correction, automated-verified: both Arceus payloads now use the
existing `handletrainerslidemsg ... RESTORE_BATTLER_SLIDE_CONTROL` after slide-out
and controller completion. The trainer loader overwrites the battler's shared
graphics buffer; omitting the restore lets later move animations draw trainer
pixels as the mon. This changes only the dedicated scripts and adds compact
EWRAM checksum observations to staging fixtures; no engine hook or runtime
storage is added. `Koanko Arceus staged` now forces real move animations for its
four core variants, including Seismic Toss after both slides; all five staging
and 75 ordinary slide groups pass. User visual review remains pending. Existing
dialogue/local fixture edits are kept.

Fixture correction after the user reported two staging functions failing their
first variants (two other functions passed): the initial checksum was sampled
when protection armed, which precedes incoming graphic loading. It could record
the outgoing mon. The reference now comes directly from Arceus ROM artwork,
using a temporary 4 KiB EWRAM heap block freed at Turn 0; only checksum scalars
remain. Separate capture/CRC assertions identify future mismatches; observer
flags use equality assertions to report their actual values. Production
scripts and restore commands are unchanged by this fixture correction. The later
bar-break assertion now expects ADAPTED rather than COVERED; the rerun passes.

**Implemented; complete automated/visual validation pending.** User compilation caught the
shared item-popup helper call missing its optional owner-name argument. It now
passes NULL, preserving ordinary nickname behavior. The subsequent emulator
report exposed the missing slide restoration described above.
The user now explicitly requires the dismissal
text/panel slide, fair-trade text, Command Seal invocation, full HP heal then bar,
and final-trial line. This supersedes the earlier no-opening-heal plan; older
saved layers retain their HP. Details and limits:
[opening/overcharge milestone](../extra%20docs/KoankoDev/planning/09_arceus_bug_space_and_conversion.md#15-revised-opening-and-overcharge-choreography-2026-11-18).

- `battle_main.c:EndTurnEvents`: extends the existing Arceus completed-turn seam
  with a returning event check after all end-turn handlers. The helper commits
  its phase before yielding; return resumes the completed cursor before normal
  cleanup/counters/selection. Narrow scheduling hook, no new callback lifecycle.
- `battle_entry_scripts.c:TryRunBattleEventScript` and header: exposes the existing
  custom returning-context runner for an owner-bound event. No second context
  allocation/snapshot, entry count increment or extra opening callback.
- `battle_interface.c:CreateAbilityPopUp`, `CreateAbilityPopUpWithName`,
  `FormatAbilityPopUpOwner`, `PrintBattlerOnAbilityPopUp`: shared rendering with
  an explicit presentation owner label. The ordinary wrapper retains global
  overwrite/nickname/Illusion behavior. The new API bypasses the global override
  without changing it, allowing Alter Ego's Bug Space with genuine Multitype.
  `battle_interface.h` adds its public declaration. No persistent popup state.
- `kazuradrop_battle_ui.c:IsBugSpacePanelSliding`, `BS_WaitBugSpacePanelSlide`
  and header expose a read-only panel completion gate. Existing create/slide/UI
  ownership and cleanup are unchanged.
- `data/battle_scripts_1.s` adds one include for dedicated returning Arceus
  payloads/text. `battle_scripts.h` exports their entry points. The scripts use
  existing heal, controller synchronization, saved-layer animation and trainer
  slide operations. No shared HP algorithm or Break Bar resolver changes.
- `test_runner.h`, `test/battle.h`, `test_runner_battle.c`: optional exact popup
  owner-label matching and recorder argument. Existing fixtures omit the field
  and retain prior matching; the same formatter supplies graphics and headless
  events. This metadata is test-only.

`battle_koanko_arceus.c` owns scheduling eligibility, natives and the opt-in
entry config. OVERCHARGING is a new enum phase within the same packed five-byte
EWRAM record; configuration/text remains ROM and no IWRAM placement is added.
Policy remains outside upstream functions. Rebase review must preserve the
completed-turn yield/cursor, runner stack capacity/context, ordinary popup
overwrite behavior and heal-before-cover controller order. These are semantic
risks even though the scheduler/include edits are narrow. Production remains
unconfigured until charged adaptation/form/Judgment integration is complete.
Regressions: `Koanko Arceus`, `Entry scripts:`, `Kazuradrop`, `Break Bar`.

## Koanko Stage 09 Bug Space modes (2026-11-18)

**User-verified after four fixture setup/DSL corrections.** The user reports
`Koanko Arceus Bug Space` and `Kazuradrop Bug Space` reruns passing; counts were
not supplied. Explicit reserve Speed and grouped `NONE_OF` corrections affect only
`koanko_arceus_bug_space.c`; no engine integration changed for these fixes.
Implementation, lifecycle, limits and validation:
[Bug Space mode milestone](../extra%20docs/KoankoDev/planning/09_arceus_bug_space_and_conversion.md#13-bug-space-modes-and-persistent-clock-dismissal-2026-11-18).
Production entry remains unconfigured while the remaining sequence is incomplete.

- `include/battle.h:BugSpaceData.mode`: repurposes the unused byte
  `thresholdPercent`; no added state bytes, allocator change, save format change
  or new IWRAM declaration. Zero remains ordinary Kazuradrop mode.
- `battle_end_turn.c:HandleEndTurnBugSpaceDecay`: extracts existing positive-stage
  crushing to custom `battle_bug_space.c:BugSpaceCrushBoosts`, keeping its original
  scheduler position/cursor and Kazuradrop decay/tier/message order. Crush-only
  bypasses decay/tier changes while continuing crushing. Tier announcements and
  `HandleEndTurnBugSpacePassiveOHKO` remain Kazuradrop-specific.
- `battle_faerie_move_resolution.inc:IsMoveBlockedByBugSpace`, `CancelerBugSpace`,
  `TrashCrushInstantKnockout`: mode guards and plain Koanko blocked presentation.
  Preserve the intentional Explosion self-KO before the blocker, followed by
  Kazuradrop's Invincible icon and taunt; the canceler order is unchanged.
  `battle_util.c:GetMinimizeModifier`, `GetAdjustedDamage`,
  `CanMoveSkipAccuracyCalc`; `battle_move_resolution.c:CancelerDamageCalc`;
  `battle_script_commands.c:BS_TrashCrushOHKOCheck`; and
  `battle_end_turn.c:HandleEndTurnMeltVirus`: narrow guards exclude dismissed
  clock tier effects. Real Minimize/ordinary move effects retain prior branches.
- `battle_util.c:AbilityBattleEffects`: legendary punishment requires Kazuradrop
  mode. `battle_script_commands.c:Cmd_tryfaintmon` and
  `battle_anim.c:LaunchBattleAnimation`: Kazura passive-faint/throw-out presentation
  requires Kazuradrop mode, preventing its custom loss flow on Koanko's field.
- `kazuradrop_battle_ui.c:CreateBugSpacePanel`, `TriggerBugSpacePanelSlideIn`,
  `ReshowKazuradropBattleUI`: mode gates keep dismissed UI hidden across refreshes.
  `TriggerBugSpacePanelTransformSlide` stops a dismissed panel's pending flash.
- `data/battle_scripts_1.s:BattleScript_KoankoArceusStatDecay` aliases the existing
  animation/message after Kazuradrop's popup; the original entry/popup ordering
  stays intact. `BattleScript_KoankoArceusMoveBlocked` is a plain move-end script.
  New declarations are in `battle_scripts.h`.

The custom owner module initializes/dismisses its field and cleans up through
existing owner notifications; no new entry/end-turn/exit scheduler hook. Shared
policy lives in a dedicated module, reducing repeated custom logic in upstream
functions. The extracted crushing block is the only moved engine behavior;
other changes are mode guards. Semantic rebase review must check all tier
consumers, script call order, scheduler cursors and panel recreation, even where
textual overlap is small. Existing Kazura mode-zero initialization must remain.
Regression prefixes: `Koanko Arceus`, `Kazuradrop`, `Minimize`, `Break Bar`,
`Koanko Command Seal`. Manual menus/presentation and full encounter remain pending.

## Koanko Stage 09 opening protection (2026-11-18)

Implementation and validation contract:
[opening-protection foundation](../extra%20docs/KoankoDev/planning/09_arceus_bug_space_and_conversion.md#12-opening-protection-foundation-2026-11-18).
**User-verified**, excluding out-of-scope Max/Tera Raid TODOs in the requested
regression suites. User compilation initially caught the five-byte state
assertion failing due to APCS-GNU struct padding; explicit packing of the byte-only
record corrected the source layout, and the subsequent user run passed. Production party
data does not install the opt-in
entry marker while the remaining final boss sequence is incomplete.

- `include/battle.h:BattleStruct.koankoArceus`: packed five-byte record in the existing
  EWRAM heap allocation; alignment affects actual struct growth. No allocator
  change, IWRAM static or map flag/var claim. `battle_koanko.h` owns the compact
  identity/phase schema and interfaces; runtime policy stays in the dedicated
  `battle_koanko_arceus.c` module.
- `SwitchInClearSetData`'s existing `KoankoOnSwitchIn` integration now dispatches
  owner/entry-marker arming before hazards. No new core switch-in hook. The
  custom dispatch also retires a stale owner when its position is repopulated.
- `battle_main.c:HandleTurnActionSelectionState`, `EndTurnEvents`: narrow
  selection/completed-turn notifications. They distinguish arrival remainder
  from the first complete turn and retain the floor until saved-layer handoff.
  `FaintClearSetData` notifies genuine owner retirement; normal battle allocation
  cleanup owns the rest, with no new exit/graphics-cleanup hook.
- `battle_move_resolution.c:CancelerMoveAnimation`, `CancelerHealthBarUpdate`,
  `TryMoveDamageUpdate`: owner-scoped reachable-loss animation hint and matched
  preview/commit clamps. Substitute/Disguise/Ice Face retain their prior order.
  `MoveEndFaintBlock` repairs protected raw zero before bar/faint/reward gates.
- `battle_script_commands.c:Cmd_healthbarupdate`, `Cmd_datahpupdate`: matched
  passive/scripted/instant-death loss clamp. Protected instant death becomes
  bounded damage and cannot trigger healing-mirror benefits. Unconfigured
  actors retain baseline handling.
- `Cmd_tryexplosion`, `Cmd_setatkhptozero`, `BS_InstantHpDrop` and
  `battle_move_resolution.c:StatChangeBeforeChange` (Memento) route self-KO
  presentation/commit through the same owner policy. `Cmd_tryfaintmon` provides
  the scripted raw-zero last defense before any bar or genuine faint.

All are narrow hooks, not replacement core algorithms. Keeping policy in the
custom module limits textual merge overlap; semantic review must still cover
every changed preview/commit and script/controller yield when rebasing upstream.
No Break Bar resolver or ordinary Time Stop/recharge behavior changed. The
handoff helper refuses to reuse an older saved layer and leaves its covered HP
identity intact. Regression prefixes: `Koanko Arceus protection`, `Koanko Command
Seal`, `Break Bar`, `Final Gambit`, `Destiny Bond`, `Perish Song`, `Koanko Dialga`.

## Koanko Stage 09 form-selection core (2026-11-18)

- `include/random.h:RNG_KOANKO_ARCEUS_FORM`: appended tag for a uniform finite
  choice among resisting actual Arceus forms. Existing tag values are unchanged.
  This is an enum extension, with no change to RNG algorithms or engine dispatch.
- `src/battle_koanko_arceus.c:KoankoChooseArceusResistantForm` and its declaration
  in `battle_koanko.h` are dedicated custom code, reusing the existing
  `GetTypeModifier` chart/inverse interface. No existing engine function changed.
  No encounter is wired to this selector yet; lifecycle/type capture/form changes
  remain future integration work.

No new mutable static storage, heap allocation, BattleStruct state or IWRAM
placement. The form map is const in ROM and selection uses scalar locals.
Merge impact is confined to appending the RNG tag and custom module/header;
semantic compatibility depends on retaining the real 18 form records and current
type-chart behavior. No independent matchup chart or ordinary Conversion 2 edit.
Regression coverage: `make check TESTS='Koanko Command Seal'` (selector only),
user compilation found a test-macro error, now corrected in both parameterized
function fixtures by using function-runner `PARAMETRIZE_LABEL`.
Selector/engine integration and RAM placement were unchanged by that correction.
The user then reported the initial selector green. The subsequent user-requested
ranking revision was subsequently reported green as well: minimize incoming damage
multiplier, maximize Judgment's multiplier against current effective foe types,
then uniformly choose among equals. Candidate forms remain the existing 18,
excluding Divine/Alter Ego/Caster/Stellar. There is no new engine integration or
mutable scratch/RAM state, and ordinary Conversion 2 is unchanged. Source and
fixture changes are confined to the custom module/header/test. Details and remaining scope:
[Stage 09 milestone](../extra%20docs/KoankoDev/planning/09_arceus_bug_space_and_conversion.md#10-form-selection-core-milestone-2026-11-18).

## Koanko Stage 08 validation fixtures (2026-11-18)

The first Stage 08 milestone adds tests only; **no new or changed upstream engine
integration point** and no new production RAM state. Existing Time Stop/recharge
dispatch, faint replacement and Break Bar behavior remain unchanged. New fixture
bytecode/configuration is const in ROM, with no mutable static observation buffers.

Coverage and user-run commands:
[Stage 08 implementation record](../extra%20docs/KoankoDev/planning/08_dialga_time_stop_validation.md#9-first-validation-milestone-2026-11-18).
The new `Koanko Dialga` fixtures use the existing returning Turn 0 contract to
seed the already-conjured singles boundary; they do not add production encounter
configuration. Codex has not executed builds/tests. Palette polish and visual
cleanup verification remain open.

First user-run follow-up: five fixture failures were reported. Corrected recorded
playback outcome/final no-command-turn expectations and the saved-layer Time Stop
proc case; modernized the older regeneration/twelve-faint fixtures for production
form bars and Zacian. These are test-only corrections with no new engine
integration point or RAM state. The user subsequently reported all three requested
reruns green (Dialga fixtures, regeneration, twelve-faint traversal); no counts
were supplied. See Stage 08 section 10. Palette/manual acceptance remains open.

Stage 09 should reuse On Entry/configured covering-layer interfaces before adding
new hooks. Its remaining cross-source survival/type-capture requirements still
need semantic compatibility tests. The user's memory restraint is explicit:
prefer battle-owned EWRAM/ROM and avoid new IWRAM data/code or large stack snapshots.

## Configurable Turn 0 / On Entry scripts (2026-11-17)

Implementation, ordering, TX audit and authoring/validation details:
[entry scripts](entry_scripts.md). User verification: Entry scripts **16/16**
after the final harness-only RECORDED/RECORDED_LINK exemption; exit HP **4/4**
after moving new static buffers from IWRAM to EWRAM. The earlier trainer assignment
run passed **11/11**. Real link/replay behavior is unchanged. The allocator failure
did not recur in the focused exit-HP run; no allocator algorithm was changed.
The user subsequently reports broader Break Bar, Koanko Zacian and Trainer Slide
passes; More Wishes' only failure was an outdated Revivify expectation. Its test
now asserts the existing Castoria-level revival block; the user reports the
focused rerun passed, completing the requested automated regression batch.
No new or changed engine hook accompanies that test correction. mGBA acceptance
remains pending.

Standing authoring scope: entry scripts are for opposing Trainers and wild
Pokémon. The underlying API permits player-side support, which is not currently
a supported encounter-authoring use case. This is authoring guidance, with no
additional engine gate or integration point.

- `battle_setup.c:CreateNPCTrainerParty` uses the dedicated scoped script adapter
  around the existing Break Bar adapter. `CreateNPCTrainerPartyFromTrainer`
  clears only the generated owner's hook state and records selected source
  indices alongside mon generation. Narrow hooks; preserve original trainer ID
  versus copied party/pool identity and pre-allocation timing on upstream merges.
  Regressions: source rules, owner isolation, pool selection/pending handoff.
- `battle_main.c:CB2_InitBattle/CB2_InitBattleInternal` consume scripted-wild and
  pending-trainer pointers before entry. Configured encounters disable recording
  because replay does not serialize configuration. Narrow initialization hooks;
  audit party-generation/allocation order and replay support if upstream changes.
- `battle_switch_in.c:DoSwitchInEvents` adds one stage after Mirror Herb and
  before clearing entry flags/Eject Pack. `constants/battle_switch_in.h` names the
  stage. Narrow scheduler hook, with a semantic dependency on living entrants,
  established speed ordering and the existing switchIn flag. Regressions:
  initial/repeat/once, Charm/Intimidate ordering, replacement and independent owners.
- `battle_main.c:TryDoEventsBeforeFirstTurn` adds opening scripts after existing
  slides/Castoria check, then an ordinary faint pass. Nonzero outcomes exit via
  the existing end-battle table before command selection. Both opening faint
  phases use a dedicated adapter after a configured hook has run: Gen 4+
  replacement sees an exhausted action queue, then the real cursor is restored.
  This prevents pre-menu KOs from deferring a replacement into turn one.
  `battle_main.h` names the stages. Narrow hooks; preserve first-menu, action/PP
  and faint ordering.
  Regressions: opening once/order/PP, unconfigured openings, controller waits.
- `scrcmd.c:ScrCmd_dowildbattle` arms a ScriptContext-owned wild handoff.
  `script.c:InitScriptContext/StopScript` clear added override pointers;
  `script.h` stores independent trainer overrides and the armed wild config.
  Event macros use existing callnative encoding; no new opcode. Narrow handoff
  hooks; audit initiating-context lifetime and cancellation. Regressions:
  armed/unarmed wild, trainer identity consumption and script stop.
- `battle_util2.c:FreeBattleResources` clears module context/pending configuration
  and ScriptContext overrides. `battle.h:PartyState/BattleStruct` own per-mon
  counts/configs and encounter opening pointers/done/running state. Graphics-only
  teardown is unchanged. Narrow cleanup/storage additions; preserve full-resource
  versus graphics lifetime on upstream merges. Regressions: reshow during payload,
  cleanup, party regeneration, stack rejection and pending bar resolution.

All policy, configuration and execution-context code remains in
`battle_entry_scripts.c`; TX's summon/ambush code is unchanged. This increases
small integration touchpoints in party creation and two event state machines,
without replacing their algorithms. The main compatibility risk is semantic
entry/faint/replay ordering, not the number of changed lines. Run the new
`Entry scripts:` suite plus existing Break Bar, trainer assignment, TX More
Wishes, Trainer Slide and Koanko/Zacian regressions before considering it verified.

## Koanko Stage 07: independent ally and singles restoration (2026-11-17)

Implementation details and exact validation status:
[temporary ally](temporary_ally.md). Stage 07 preserves the previously observed
entry choreography and existing partner-row ownership accessors. New hooks:

- `battle_main.c:FaintClearSetData`: narrow post-Reduction hook marks only the
  temporary actor FAINTED. Normal faint effects/animation and real-party loss
  authority remain. `HandleEndTurn_FinishBattle`, `Test_RestorePlayerBattleStats`
  and `battle_util2.c:FreeBattleResources` invoke idempotent ally cleanup before
  battle ownership storage disappears. Audit cleanup order when upstream moves
  resource or party restoration. Regressions: ally faint, player loss, simultaneous
  KOs, recorded AFTER_CLEANUP and idempotent cleanup.
- `battle_util.c:HandleFaintedMonActions` adds a barrier at WAIT_STATE before
  replacement iteration. The custom module schedules departure only after all
  chosen actions finish and genuine final Eternamax faint is established. Queue,
  count and end-turn cursor restoration live in the custom module. Narrow hook,
  but significant semantic dependency on action/faint/end-turn ordering. Recheck
  this barrier if upstream changes when replacements run. Regressions: departure
  alive/already fainted/Life Orb double KO, Explosion with both real replacements.
- `battle_controller_player_partner.c:PlayerPartnerHandleChooseAction/
  PlayerPartnerHandleChoosePokemon`: narrow independent-owner gates prevent
  switching, bag use or reserve/revival requests. Normal cached move selection
  remains. `battle_ai_switch.c:ShouldSwitch/GetMostSuitableMonToSwitchInto`
  short-circuit this owner before evaluating switches. Regressions: next-turn
  attack, Disable, player replacement, ordinary doubles/multi and trainer control.
- `battle_gimmick.c:CanActivateGimmick/HasTrainerUsedGimmick/
  SetGimmickAsActivated` exclude the ally from activation and cross-owner sharing
  (including Dynamax's broader sharing rule). `battle_util.c:
  IsPartnerMonFromSameTrainer` distinguishes the independent owner without setting
  MULTI/INGAME_PARTNER. Narrow ownership conditions; review new trainer-wide
  abilities/gimmicks for assumptions based only on battle side. Regressions:
  owner canaries, Intrepid Sword, multi/doubles gimmick tests.
- `battle_script_commands.c:Cmd_checkteamslost` counts one reserve-bearing real
  owner per side during this formation, avoiding fake partner capacity. Existing
  replacement/revival paths use the shared position-policy seam. `battle_util.c:
  HasNoMonsToSwitch`, `battle_ai_util.c:CountUsablePartyMons/
  AnyPartyMemberStatused` exclude the temporary ally's local party index and read
  its own status. AI exceptions apply only during this formation; ordinary multi
  reserve-count behavior is preserved. Validate reserves, healing, status and
  simultaneous KOs.
- `party_menu.c:GetPartyLayoutFromBattleType/BufferBattlePartyCurrentOrder/
  BufferBattlePartyOrderBySide` use a single real active slot when the partner has
  another owner or is disabled/retired. This avoids duplicate local index 0 and a
  seventh write into a six-entry ordering buffer. `battle_main.c:SwitchPartyOrder`
  excludes the temporary owner from shared order copies while retaining ordinary
  multi ordering. Regressions: six-slot
  ordering canary, real replacement and ordinary multi/partner control.
- `battle_util.c:ResetSentPokesToOpponentValue/
  OpponentSwitchInResetSentPokesToOpponentValue/UpdateSentPokesToOpponentValue`
  skip the temporary owner in real-player EXP participation masks. Existing
  controller identity keeps it out of EXP recipients. Narrow gates; review if
  upstream adds a separate participation mechanism. Regression: player-left slot
  1 plus independent local slot 0 does not mark real player slot 0 present.
- `battle_interface.c:DestroyBattlerHealthbox/RebuildBattlerHealthbox` extracts
  reusable selective destruction; rebuilding retains visibility/shared UI ids.
  `Task_FreeAbilityPopUpGfx` scans all four allocated positions so shrinking the
  live count cannot leak a right-side popup sheet. `battle_gfx_sfx_util.c:
  BattleRestoreSinglesHealthboxGfx` reallocates singles frames after a possible
  doubles menu reshow; `CreateEnemyShadowSprite` skips DISABLED as well as RETIRED
  positions. Small graphics lifecycle extensions; upstream changes to tags,
  healthbox children, tile sizes or shadow policy require semantic review.
  Graphical acceptance is reserved for the user.
- `battle_interface.c:GetBattlerCoordsIndex` extends the existing disabled-right
  singles-layout rule to opponent-left. Koanko's opponent-right is disabled even
  with a six-member opponent party, so sprite creation, animation targets and
  menu reshow must all retain centered singles coordinates. Narrow shared-policy
  correction; no per-move animation patches or extra reshow hook. Castoria's
  reserved opponent-right and ordinary doubles retain their existing layout.
  Regressions: six-member coordinate canary, live Behemoth Blade with reserves,
  actual recreated sprite position after forced reshow, ordinary trainer slides.
  Review upstream layout policy changes for side topology versus reserve counts.
- `battle_util.c:CalcMoveBasePower` (EFFECT_DYNAMAX_DOUBLE_DMG) adds the
  encounter-gated `KoankoIsEternamaxTarget` predicate. Shared real/AI calculation
  doubles power without fabricating Dynamax state. Narrow rule, approved by the
  user on 2026-11-17. Regression: live damage and cached AI estimates against the
  same genuine Eternamax in Koanko and an unrelated trainer battle.
- `test/test_runner_battle.c`, `include/test/battle.h`: SUMMONED_ALLY describes
  later player-right AI expectations without modifying recorded real parties or
  battle format. It parallels the existing SUMMONED_OPPONENT seam. This is a
  test-only infrastructure extension; validate both summon test groups.

Custom-module changes: `battle_temp_ally.c` owns lifecycle, storage, exit and
cleanup; `battle_koanko_ai.c` dispatches owner-gated pressure/setup scoring.
The departure native now dashes the living ally right behind healthboxes, cleans
the formation once, then remains pending while the surviving player slides to
singles center before replacement resumes. These presentation phases reuse the
same script barrier; they add no upstream hooks or battle-state fields. Live
frame observers cover dash direction, UI priority, fully offscreen removal,
bounded player translation and centering before Dialga; simultaneous faint
coverage retains normal centered replacement instead of moving a stale sprite.
`battle_tx_rollback.c` exposes its existing formation predicate and delegates the
shared slot-policy seam while an ally exists. Solo TX faint state and persistent
Reduction lifetime are unchanged. Keeping transitions in custom modules limits
textual upstream conflicts; semantic risk still spans owner routing, action/faint
ordering, resource lifetimes and future gimmicks, even where no conflict appears.

After graphical review and departure-motion polish, the final focused Zacian
suite passes 20/20, including live per-frame dash/recentering assertions.
Parameterized departure covers living, already fainted, recoil double KO and
continuing Reduction; separate cases cover centered menu reshow, six-member
coordinate policy, residual damage, simultaneous real replacements and aborting
entry after graphics teardown. Cleanup also redirects right-position script
cursors. For the shared coordinate correction, TX profiles pass 7/7 and trainer
slides pass 75/75; More Wishes retains
its prior revival assertion failure (18/19). Ordinary ROM compilation succeeds.
Starting-commit and last full-suite comparisons are
recorded in the implementation note; graphical acceptance is reserved for the
user. The intentionally failing trainer-bar CRASH DUMMY test was removed at the
user's request (the remaining file passes 11/11).

## Resumed migration: native and script APIs

The migrated ROM builds through `resume-rom-09.log` (exit 0), including the
controller/history, AI configuration, multihit and summon-entry repairs.
Actual focused passes include TX profiles 12/12,
TX wishes 23/23, AFTER_CLEANUP 6/6, migration ownership 4/4 and stock trainer
control 21/21 and complete `arcanum.c` 71/71. Remaining Arcanum files pass
10/10 Endurance, 22/22 Debilitation, 18/18 More Wishes, 9/9 Normality and 20/20
Reduction. Break Bars pass 21/21, grants 13/13, exit HP 4/4, trainer bars 11/11
and wild bars 4/4. Ordinary-engine and remaining boss files run serially.
These are partial behavioral results,
not full migration acceptance; exact logs and failures are in Chapter 15.

- Documented post-release fixes: terrain correction/test from upstream
  `be3b089fc121143b7ccf6503ea1385b8abe05b8c` restores terrain-aware AI scoring
  and grounded-target damage checks in `battle_ai_main.c`/`battle_util.c`.
  Pivot/phazing correction from `8b10030b4c42f2b481567bcf43eb46eb64679d93`
  is adapted in `battle_script_commands.c:CountAliveMonsForBattlerSide/
  Cmd_forcerandomswitch`. Counts include each trainer and pending layers;
  reserve enumeration uses owner-local slots and shared-party active exclusion.
  The No Order Change guard is retained. These repairs are necessary for target
  ownership and existing AI semantics, not a general master update. Validate
  partner/multi defeat, pivot, Roar/Red Card and pending-layer cases, plus
  Grassy Terrain/Ring Target and AI terrain removal. This replaces obsolete
  half-party branching rather than adding a competing ownership framework.
- `battle_controllers.c:SetFinalChosenTarget` resolves the live committed move
  for Koanko's sampled Smeargle, because its choose-move buffer precedes the
  slot-0 replacement. Ordinary controller-buffer selection remains unchanged.
  Virtual Arcanum slots still use the existing live lookup. Polymorph snapshot
  rows in `castoria_name_theft.c` use target trainer IDs, matching PartyState;
  their validity/lifetime remains governed by `polymorphed` and first-cast
  original capture. Persistent-player exit restoration uses the player row.
  New `test/battle/migration_owner_apis.c` covers independent owners, pending
  layers, Roar reserves and stale Smeargle targets; all four pass in
  `resume-owner-runtime-03.log`.
- Target temporary flag aliases 5/6 coexist with custom Caster/Kazuradrop names
  at the same existing IDs. This preserves target League lighting compilation
  and the established map-local custom flag lifecycle; no new IDs were assigned.
- `test/test_runner_battle.c` retains summoned-opponent recording before the
  runtime battler exists, target trainer bindings and cleanup callbacks. Its
  creation fixture now uses the target fixed-IV/OT API. Actual harness and four
  focused fixture object builds passed; runtime/stack-guard checks remain pending.
- Makefile includes the existing standalone custom assembly-song sources in
  normal/test object lists, retaining `audio_rules.mk` assembly rules. This is
  the baseline custom music registration lost during Makefile reconciliation.
  Hurricane throw-out cleanup calls target `UnsetBackground`, equivalent to the
  retired Psychic-background helper including both fade waits and arg-7 sentinel.
  Verify the custom tracks and forced-out animation in the emulator.
- `battle_tx_wishes.c:TxScriptContext` stores target u32 move-result flags for
  all battlers. Both scripted phase and mirrored-recovery contexts use this
  width; their entry/restore order is unchanged. The baseline u16 snapshot would
  copy only half of the target array. A new four-slot/high-bit restoration
  regression in `tx_wishes.c` guards this target-interface dependency.
- `src/data.c` retains the production trainer table in TESTING builds too.
  `include/data.h:GetTrainerStructFromId` consults a TESTING-only override for
  target trainer-control fixtures; `test/test_runner_battle.c` owns that sparse
  mock table. Unmocked trainer IDs use the same authored data as the ROM. This
  preserves production Koanko kit/AI tests without duplicate boss definitions
  or suppressing target trainer-name checks. Ordinary builds have no lookup
  branch. Validate stock trainer-control and custom trainer difficulty/AI tests.
- `battle_main.c:CB2_QuitRecordedBattle/HandleEndTurn_FinishBattle` preserve the
  baseline test observation order: THEN inspects live combat state before
  Reduction/TX/form/HP cleanup; the guarded AFTER_CLEANUP callback observes
  restoration afterward, before resource freeing. The target recorded exit's
  cleanup-first dispatch erased TX/Reduction state before THEN. Keep this
  distinction when updating target test infrastructure. Validate the dedicated
  `after_cleanup.c` fixtures, TX/Reduction and natural/recorded battle exits.
- `battle_move_resolution.c:CancelerPowerPoints/CancelerPPDeduction` restores
  baseline PP-array bounds for virtual slots and conjured Smeargle's real-slot
  PP/Pressure exemption. Virtual actions preserve ordinary last-move/Metronome
  state in this stage; submove continuation handling remains target-owned.
  The PP correction compiled in ROM 08 and TX profiles passed 12/12 afterward.
  The history guards below are under full Arcanum validation; see Chapter 15
  for the precise source/test boundaries rather than treating this as acceptance.
- `battle_ai_main.c:ChooseMoveOrAction_Doubles` records final scores under its
  evaluated target `i`, replacing a stale global-target index retained by pinned
  1.17.2. The local defect was exposed by TX's ordinary-versus-virtual tie test.
  This fixes score observation without changing candidate scores or tie policy.
  Preserve the strict existing tie assertion and verify doubles AI regressions.
- Virtual-action history guards are ported in `CancelerAttackstring`,
  `MoveEndUpdateLastMoves`, `MoveEndMirrorMove` and `SetSameMoveTurnValues`:
  Arcanum does not overwrite ordinary used/printed/last move or Metronome state;
  defender hit records and baseline known-move learning still run. The target
  Grudge faint stage bounds its chosen slot before any PP access. Smeargle's
  existing Choice-lock exemption is restored in `CancelerChoiceLock`.
  These are narrow policy hooks in target algorithms, not restored old opcode
  implementations. Validate existing Arcanum history/Grudge/regular-slot
  regressions and Smeargle PP/Choice/forced-action tests before acceptance.
- Recorded player/opponent default-target handlers bound real choose-buffer
  slots and resolve virtual slots with `GetBattlerMoveAtSlot`. This preserves
  ordinary cached selection while preventing virtual-slot out-of-bounds reads.
- `include/config/ai.h` uses median defaults for attacking and attacking-partner
  contexts, preserving the baseline shared AI estimate through target config
  knobs; Risky/Conservative overrides are unchanged. New target-only contexts
  retain target defaults. The direct AI fixture explicitly sets/restores its
  controller role, matching target awareness without exposing hidden moves.
  Merge maintenance is confined to two configuration values and narrow lookup
  hooks; no competing damage simulation or ownership system was introduced.
- `battle_hold_effects.c:ItemBattleEffects` and the Break Charm hold-effect row
  use target `onSwitchIn` activation for opening and later entries. The common
  switch-in interpreter calls the returning Charm script; the removed separate
  first-turn dispatch is retired. Test grant timing before hazards, saved-layer
  numerical animation and later re-entry without duplicate grants.
- Target move-end `TryRedCard/TryEjectButton/TryEjectPack` retain baseline
  No Order Change gates before item consumption or switch queueing. Red Card
  checks the attacker it would move, not the holder. Debilitation passes 22/22
  including these item/ability cases. `MoveEndMultihitMove` uses baseline
  pending-aware liveness instead of raw HP for continuation; pending damage
  remains suppressed and the phase resolver remains after the action. Validate
  full multihit and spread/parental-bond ordering before acceptance.
- The Healing Wish script distinguishes a Lunar Dance user restored by a lost
  layer: resolve the phase, store the recipient wish, then use the existing
  immediate-switch script. A genuine faint retains target deferred replacement.
  This narrow branch preserves the established surviving-user contract without
  forking the target queued-switch machinery. Grants retry 03 passes 13/13;
  the main Break Bar file passes 21/21 with its exact two-hit scene assertion.
- Custom sigil/status-icon/Kazuradrop panel callers use target
  `CreateSpriteUnchecked`, matching their existing sentinel/exhaustion handling.
  Target's checked `CreateSprite` instead aborts during a full doubles intro.
  Normality passes 9/9 after this API port. Visual sigil retry/residency under
  full sprite pressure still needs emulator validation; core allocation was
  not changed and temporary diagnostics were removed.
- The More Wishes script explicitly runs target `switchinevents` after
  `switchineffects` and before finishing entry/ambush. The hazard stage in
  `battle_switch_in.c:FirstEventBlockEvents` exempts the summoned member only
  while `TX_ROLLBACK_ENTERING`; ordinary and Koanko-solo entries retain target
  hazard rules. Abilities/items use the shared target interpreter, preserving
  their order before the ambush. Initializing `switchIn` FALSE did not implement
  this exemption because target entry-marking overwrote it. This is one narrow
  policy gate plus a target-script API call, not duplicate entry processing.
  Validate the More Wishes entry popup/no-HP-bar/White Herb regression and
  ordinary entry/hazard tests. More Wishes retry 04 passes 18/18; the ordinary
  entry/hazard matrix subsequently passed (see Chapter 15 for exact counts).
- `battle_koanko.c:ConjureKoankoTeam` and its debug generator disable target
  SMART_TERA random-type generation only in their temporary trainer descriptors.
  The encounter keeps its decision flag; only explicitly authored Fire Calyrex
  is eligible, preserving the old mask. Target's generator remains shared.
  Actual encounter retry 03 passes 13/13; optional-Tera validation continues.
- `battle_ai_util.c:ShouldTeraFromCalcs` counts Tera-capable reserves using
  target's disabled `TYPE_MYSTERY` sentinel, replacing its incorrect `> 0`
  check (which included disabled entries and excluded Normal). `DecideTerastal`
  retains baseline capture/restoration of no-Tera effectiveness alongside damage
  when deferring. These are narrow corrections in shared target calculations,
  preserving the existing optional Fire Tera policy and cache parity. Existing
  Koanko AI passes 37/37, stock smart-Tera 4/4 and Terastal 44/44;
  this is not a new AI balance policy.
- `battle_ai_main.c:DoAIScoreProcessing` records the separate
  `AI_CompareDamagingMoves` delta as Smeargle diagnostic pass 64. Target moved
  this adjustment outside the per-flag loop. Scoring/selection are unchanged;
  all four exact score-accounting regressions pass in diagnostic retry 02.
- `battle_util.c:AbilityBattleEffects(ABILITYEFFECT_ON_SWITCHIN)` keeps Legendary
  Clause after the player's ordinary entry ability by pushing its returning
  script as that ability's continuation. With no ability script scheduled, it
  calls punishment directly. The once-only flag is committed before yielding;
  target's single visit to this stage replaced baseline retry behavior.
  Kazuradrop retry 02 passes 9/9, including Intimidate ordering/boost tiers.
- `battle_break_bar.c:RefreshBreakBarPalette` checks the target task slots
  locally before allocating a fade, preserving exhaustion protection without
  restoring a generic task API. Task/palette exhaustion and menu lifecycle
  behavior remain runtime validation requirements.

- `battle_move_resolution.c:CancelerInterruptibleMoves/SetFutureSightAttack`
  factors the target Future Sight/Doom Desire record initialization into one
  public scheduling primitive. `battle_tx_wishes.c:BS_TxRollbackSetDoomDesire`
  calls it at the original scripted phase point after explicit eligibility
  checking. This narrow extraction avoids restoring the removed command or
  maintaining duplicate delayed-attack initialization. Test ordinary/delayed
  attacks, owner/slot changes and nested TX phase context restoration. Future
  upstream scheduling metadata changes must update the shared primitive.
- `battle_script_commands.c:BS_TryBugSpaceFaintGulpMissile` bridges the custom
  passive faint presentation to target on-hit form/ability dispatch, filtered
  to Gulp Missile. Already-consumed forms cannot activate twice. The special
  faint script also uses target Dynamax cleanup, Sky Drop release and the
  weather-form loop. Verify faint/recoil/passive ordering and blocked-status
  continuations; this is a narrow dispatch adapter, not the old effect algorithm.
- Castoria Debilitation uses `tryconfusionafterskydrop` before its cancellation
  native, preserving passenger release/fatigue confusion with target bytecode.
  The native then clears the holder link and synchronizes visibility. No core
  cancellation algorithm was restored. Exercise both holder and passenger
  cases, control immunities, animation completion and script-stack restoration.
- `battle_faerie_anim.c:AnimTask_PreloadFaerieParticles` uses target graphics/
  palette bookkeeping from custom animation scripts. Each replaced load retains
  `delay 1`, matching the old wait. There is no new animation opcode or modified
  core loading algorithm. Visual timing, particle residency and teardown need
  emulator verification; assembly does not establish them.
- `battle_koanko.c` uses trainer-A PartyState, AI history, items and fresh
  Pokemon gimmick eligibility. Its trainer resources remain encounter-owned;
  only the documented conjured-team Tera opportunity resets. Target generation
  remains authoritative. Regression tests must prove twelve-opponent progress,
  final-only victory and retained player/trainer resources.

## Migration resolution checkpoint — 2026-11-11

**Historical initial checkpoint; superseded by the resumed results above.**
The [durable handoff](../extra%20docs/KoankoDev/planning/15_upstream_migration_checkpoint.md)
records exact failures and resume commands. All textual conflicts are resolved;
seven core battle objects compiled at that checkpoint. The formerly failing
custom modules and script assembly now compile and the ROM links as recorded
above. Only the focused runtime results listed above are verified.
This section supersedes the older initial-integration dispatch status below.

- `battle_move_resolution.c:ShouldSkipBattlerForDamage/TryMoveDamageUpdate/
  MoveEndFaintBlock`: Break Bar gates in target damage/faint processing queue a
  lost layer before true-faint effects, suppress remaining damage, and retain
  the existing after-action phase resolver. Guts snaps use target controllers.
  These replace retired command-level algorithms. Test multihit/spread,
  simultaneous KO, rewards, Destiny Bond, capture and phase continuations.
- `battle_script_commands.c:Cmd_healthbarupdate/Cmd_datahpupdate`: floor passive
  damage, suppress pending-layer writes and preserve mirrored recovery.
  `BattleStruct.instantDeathHpUpdate` marks explicit scripted deaths; ordinary
  setters clear it and the HP commit consumes it. `trashCrushOhko` resets per
  action. Test exceptions and canonical/controller HP synchronization.
- `battle_faerie_move_resolution.inc` retains custom policy alongside narrow
  target canceler/damage hooks. Trash & Crush rolls after survival adjustment;
  `battle_util.c:GetAdjustedDamage` retains Bug Space's crushing damage before
  survival adjustment. Move-failure and Emergency Exit retain No Order Change.
  Complete pivot/phazing and boss exception regression coverage is pending.
- `battle_set_effect.c:sSetEffectHandlers`: Melt Virus and Timestop use target
  dispatch and common checks. `include/battle_script_native.h` shares packed
  native-argument parsing across custom modules without restoring old opcodes.
- `battle_ai_main.c`: signed selection and virtual Arcanum actions return target
  ChosenAction; ordinary and hypothetical slot-0 scoring share target passes.
  `battle_ai_util.c` uses AiCalcValues and restores simulation scratch. Tactical
  module migration and runtime validation remain incomplete.
- `battle_switch_in.c` adds Name Theft/pre-hazard Break Charm stages and retains
  unconditional Lunar Dance layer grants. TX identity and masked Reduction/TX
  controller normalization use trainer ownership. Test partner/multi, Wish
  identity, canonical HP and continuation behavior before accepting these ports.

Merge maintenance: target generation, effect dispatch, resolution and graphics
lifetime replace old integration sites. The faint/damage changes are substantive
semantic integrations despite narrow hooks. Preserve custom policy separately,
but re-audit ordering during upstream updates; fewer changed lines are not proof
of lower compatibility risk. State lifetime and outstanding risks are documented
in the checkpoint. No tests were weakened or failures suppressed.

Central record for reviewed custom engine integration points. This initial entry covers
Stage 03 Smeargle integration; it is not an exhaustive inventory of older custom hooks.
Implementation status is separate from runtime validation.

## Upstream 1.17.2 migration — initial integration (2026-11-11)

**In progress; runtime behavior unverified.** Baseline `mypatch` commit
`2bfc834d8c` is preserved. An uncommitted merge of exact target
`79dd436920845ebc145577649de624f65298c209` is open only in the isolated
`upgrade-branch` worktree. Current status, recovery evidence, conversion
decisions and exact baseline statistics are in
[Chapter 14, Section 11](../extra%20docs/KoankoDev/planning/14_upstream_upgrade_target_recommendation.md#11-authorized-migration-execution-record--2026-11-11).

- `battle_setup.c:CreateNPCTrainerPartyFromTrainer/CreateNPCTrainerParty`:
  narrow calls retain selected source-entry identities while the new target
  generator owns party creation. `trainer_break_bars.c` records static config
  pointers before BattleStruct exists, consumes them into trainer/local-slot
  PartyState at `battle_main.c:CB2_InitBattle`, and clears them after consumption
  or full teardown. Runtime generation can assign existing PartyState directly.
  The previous custom generator in `battle_main.c` is retired. Test trainer
  overrides, pooled selection, both opponent owners, Koanko replacement and
  consecutive encounters; target preallocation order and generation callbacks
  are semantic compatibility concerns even if future merges are textual clean.
- `battle_util2.c:AllocateBattleGfxResources/FreeBattleGfxResources`: graphics
  initialization/free owns status-icon IDs, Kazuradrop panel/sigil graphics and
  Break Bar palettes/tasks. `FreeBattleResources` retains script/pending-profile
  cleanup. Menu closure follows target graphics-only teardown and leaves custom
  combat state allocated. Test repeated Bag/party/summary reconstruction, saved
  layers, palette/task cleanup and genuine battle exit. These are narrow hooks;
  sprite reset/free ordering remains a maintenance concern.
- `include/battle.h:PartyState/BattleStruct/IsBreakBarPending/IsBattlerAlive`:
  retain custom state using the target trainer-owned array/accessor and pending
  layer survival predicate. TX combat-profile, delayed recipient and controller
  owner conversions are still pending. Multi/partner, simultaneous faint,
  summoning and no-false-reward coverage is required before accepting this port.
- Custom enum hooks now live in target-owned
  `constants/battle_set_effect.h`, `constants/battle_move_resolution.h` and
  `constants/battle_util.h`. Their implementation dispatch is unfinished; the
  old effect/move execution algorithms are not reinstated as compatibility code.
- `battle_ai_switch.c:CanBattlerConsiderSwitch`: carries the existing No Order
  Change rejection from deleted `battle_ai_switch_items.c`. No additional AI
  policy is introduced. The deleted empty Alice placeholder had no behavior.
  Test No Order Change and ordinary voluntary/pivot/phazing switch paths.
- `test/test_runner_battle.c` and `include/test/battle.h`: keep the guarded,
  separately allocated 4096-byte stack while adopting target trainer bindings,
  party sizes, controller reset and inventory initialization. Summoned TX maps
  to opponent A in test declarations; cleanup assertions remain. This avoids
  reintroducing a stack inside the bounded map buffer. Actual memory/link and
  runtime overflow checks on the integrated target remain required.
- Identifier/data headers retain custom entries after target ranges; trainer
  pictures use `gTrainerPicInfo` and regional dex uses target macros. ARM enum
  compilation and boundary checks passed. This is data/interface validation,
  not proof of gameplay, canonical HP correctness or complete numeric auditing.

Merge-maintenance impact: target party ownership, generation, effect dispatch
and graphics lifetime replace older integration locations. Small adapters keep
policy in existing custom modules and avoid carrying obsolete generators or AI
files. The move-resolution and script ports remain the largest semantic risks.
The frozen baseline ROM/full suite passed all battle cases (three accepted text
failures remain); **the integrated target has not built or passed runtime tests**.

## Pre-migration regression fixtures (2026-11-10)

No production integration points changed in this milestone.
`test/test_runner_battle.c:DeclareSummonedOpponent` now declares the existing
summoned TX profile's four ordinary moves so `EXPECT_MOVE` can resolve Wide
Guard before runtime creation. This is a narrow correction to the existing
test helper; keep it aligned with `BS_TxRollbackCreate` on future moveset edits.
`test/battle/arcanum_more_wishes.c:MoreWishesScore` allows the simultaneous-KO
fixture to choose Follow Me instead of blocking its own trigger with Wide
Guard. Other fixtures retain Wide Guard. Regression assertions are preserved,
including an explicit Follow Me selection in the KO turn. No faint/replacement,
controller, Wish-selection or damage algorithm was changed. Merge maintenance
is confined to test code, with no additional production hook or dependency.
The user reports both More Wishes and TX Wishes focused runs passing after
these repairs; exact counts were not supplied for those two runs. The subsequent
12-file isolation loop also passed (saved logs: 137 PASS, 10 TODO, 147 TOTAL).
The other 13 reported battle failures passed in isolation but recurred together
in the full suite. Subsequent diagnostics identify invocation-stack corruption;
see below and the
[repair record and exact commands](tx_rollback.md#pre-migration-regression-repair-2026-11-10-through-2026-11-11).

### Blocked nonvolatile effect continuation (2026-11-10)

- `src/battle_script_commands.c:SetMoveEffect`: when the shared nonvolatile
  eligibility check rejects an effect, advance to the supplied continuation.
  Previously this branch left the cursor at the same command; diagnostic
  Gulp Missile snapshots confirm paralysis blocked by unintended Misty Terrain
  looping with no outstanding controller request. The repair also applies to
  other nonvolatile effects using the same branch, preserving all immunity
  checks, successful status scripts and existing continuation ownership.
- Regression: the existing faint-trigger Gulp Missile fixture plus a new
  explicit Misty Terrain/faint/replacement case in
  `test/battle/ability/gulp_missile.c`; shared status/ability suites and full
  suite remain required. User-run focused log: 11 PASS/11 TOTAL, including the
  new explicit Misty Terrain case. The user subsequently reports the full
  suite green except the deferred text cases after the stack correction.
- Merge impact: two added lines in the existing upstream switch branch; no
  new hook or module. Recheck upstream status rejection/continuation semantics
  during migration, and drop this local fix if upstream already handles it.

### Test invocation stack capacity (2026-11-11)

- `test/test_runner_battle.c:BATTLE_TEST_STACK_SIZE` provides 4096 bytes.
  Direct healthbox calls from test bodies reach Smol decompression, whose
  symbol-delta decoder alone uses a measured 1740-byte local code buffer in
  the existing ELF. Diagnostic guards caught overwrites in three Break Bar
  fixtures; weather changes and corrupted trial/parameter counters show the
  damage escaping the test stack. The corrupted fog value then correctly
  creates Misty Terrain in later battles, causing misleading status/AI failures.
- The initial embedded-stack increase failed the user's compilation because
  the existing `sBackupMapDataSpace` assertion correctly rejected state larger
  than the 20 KiB map buffer. The corrected design removes the embedded stack
  from `include/test/battle.h:BattleTestData` and adds a separate test-only,
  8-byte-aligned `EWRAM_DATA` stack with two guard words (4104 bytes plus possible
  linker padding). Last linked ELF evidence leaves 19468 EWRAM bytes available;
  the added buffer leaves about 15 KiB if other allocations are unchanged.
  IWRAM, production stack, heap size and fixed map-buffer capacity are unchanged.
- `InvokeTestFunction` retains the guard check on every invocation. Three
  explicit resets mirror the previous state/data memset points, preserving
  test-local lifetime across scene/assertion phases. Static storage has no
  allocation/cleanup callbacks. Existing state-size and result bounds remain.
- Weather/state trace logging and the temporary ELF-inspection script are
  removed. No weather reset or decompressor algorithm modification hides the
  overwrite. Existing offending tests provide regression coverage: Break Bar
  dispatch/redraw/dynamic fill and No Order Change healthbox reconstruction,
  followed by the full suite. The user reports that full stack green except
  text tests. Final linked ELF inspection confirms the stack at `0x0203AA08`
  and EWRAM ending at `0x0203C400`, leaving 15360 bytes free. No additional
  tests were run by Codex.
- Merge impact: test infrastructure only, localized macro/field/invocation
  changes. Review upstream test stack ownership, capacity and result-storage
  layout when merging. Production rendering and compression remain unchanged.

The second diagnostic full run reports 4896 total, 3936 passed, 18 known-failing,
779 TODO and 163 unexpected failures. It exposes stack corruption across more
workers after diagnostic layout/scheduling changes; it is not an accepted
baseline. The subsequent separate-stack full-suite run is user-reported green
except the deferred text tests, with zero unexpected battle failures. Exact
final passed/failed/known-failing/TODO/total counts were not supplied; the
diagnostic numbers above must not be presented as final counts.

## Simplified new game (2026-11-10)

- `src/new_game.c:NewGameInitData` / `InitSimplifiedNewGame`: replaces the
  existing mandatory AliceRoom startup warp with an outdoor Littleroot spawn,
  ordinary starter/item creation and immediate introductory-state setup after
  vanilla resets. This is a narrow fresh-save integration, not a second startup
  route. Continue and save structures are unchanged; Alice maps/scripts/trainers
  and all battle mechanics are preserved.
- `data/scripts/new_game.inc:EventScript_InitSimplifiedNewGame`: new immediate
  initialization script beside existing reset scripts. Mirrors post-Pokedex /
  Running Shoes state, gender-specific household visibility and safe respawn;
  bypasses only the opening through the Route 103 rival handoff.
- `data/layouts/LittlerootTown/map.bin`: two sealed north-exit tiles, (10, 1)
  and (11, 1), changed from `0x044E` to `0x3001` so Route 101 is reachable.
  Review these tiles against upstream map edits rather than replacing layouts.

Merge exposure is localized to the existing new-game function, event-script
file and four map bytes. Semantic compatibility requires rechecking upstream
reset order, intro state values, starter/Pokedex registration and heal-location
setup even after a clean textual merge. No overworld or battle algorithm hook
is added. ROM build `make -j4` passed; fresh-save movement/menu/house/wild-battle/
save-reload/Petalburg/Alice checks remain pending because mGBA computer-use
access was rejected. Exact state and validation instructions:
[Simplified new game](new_game.md).

## Wild Break Bar assignment, capture and overlay (2026-11-10)

Implemented in the working tree. The user reports the wild fixture passing
after its opponent-position setup was corrected. ROM build and emulator color
inspection have not been reported. See [Break Bar reference](break_bars.md).

- `src/scrcmd.c:ScrCmd_dowildbattle` and `src/battle_main.c:CB2_InitBattle`:
  narrow hooks arm and consume a script-owned wild profile before healthbox
  creation. `src/script.c` owns the pending pointer and clears it on script
  stop/reset; battle setup and cleanup clear unused state. Review callback and
  battle initialization order on upstream merges. Trainer party generation is
  unchanged.
- `src/battle_script_commands.c:Cmd_handleballthrow`: narrow rejection before
  catch-rate and guaranteed-capture branches. `src/battle_util.c` skips Bag or
  Safari Ball consumption for that rejection. Review both action handlers and
  any new capture paths when upstream changes Ball transactions; a clean textual
  merge does not establish semantic coverage of new paths.
- `src/battle_break_bar.c:BS_AnimateSavedBreakBar`: changes only the temporary
  covered-layer palette selection. The shared `DrawHealthbarOverlay` and new
  layer fill remain unchanged. Review healthbar palette indices and HP color
  thresholds if upstream graphics change.
- `include/battle.h:PartyState`: one battle-only byte records whether the base
  covered by a dynamic layer was already registered. This preserves its red
  presentation even when another layer is inserted after earlier depletion.
  Review PartyState layout and reset semantics when upstream changes battle
  state; there is no save-format change.

The runtime state and predicate remain in the custom Break Bar module; new
event syntax lives in `asm/macros/event.inc`, and the rejection message in
`data/battle_scripts_2.s`/`src/battle_message.c`. Focused regressions:
`test/battle/wild_break_bars.c`, `Break Bar`, `Trainer Break Bar`, `Break Charm`,
capture and healthbox cases, plus normal ROM build and visual inspection.

## Koanko Stage04 TX profiles, health and Reduction (2026-11-10)

Implemented in the working tree; builds and runtime tests are pending user-run
validation. [Behavior/state/codeflow](tx_rollback.md),
[milestone changes and commands](../extra%20docs/KoankoDev/STAGE04_IMPLEMENTATION_AND_CODEFLOW.md).

- `battle_controllers.c:BtlController_EmitSetMonData`: narrow
  `TxRollbackNormalizeRequest` hook after Reduction normalization, converting
  emitted HP/maxHP/full records to the original party maximum. Existing emission
  buffering is retained. Review request layout, masked party identities and
  inverse overlay order when updating controller serialization.
- `battle_controllers.c:BtlController_HandleHealthBarUpdate` and
  `battle_interface.c:UpdateHealthboxAttribute`: extend existing effective-HP
  predicates to the TX overlay. Party maxima deliberately remain canonical, so
  HUD/preview reads require battle HP/maxHP. These touch upstream display paths;
  a clean merge still needs semantic checks of HP-update timing and reloads.
- `battle_util.c:CopyMonLevelAndBaseStatsToBattleMon`: one clean-copy hook after
  Wishful Thinking growth and before Reduction. `battle_script_commands.c:
  Cmd_switchindataupdate` adds the corresponding hook after growth on its clean
  controller copy, before ordinary switch-in cleanup. Incorrect insertion order
  could lose maxHP deterioration or duplicate growth; check clean-copy semantics
  when either upstream function changes.
- `battle_main.c:HandleEndTurn_FinishBattle`: one cleanup call after Reduction
  restoration; refine the existing Dex exemption to summoned ownership, so solo
  combat ownership does not inherit summon-only behavior. Review cleanup order
  and ownership availability if upstream rearranges teardown/Dex processing.
- `include/battle_tx_rollback.h` extends the existing custom portion of battle
  state; no save-format fields are added. The state is still owned by the
  existing `BattleStruct.txRollback` member, with battle lifetime only.
- `data/battle_scripts_1.s`: add returning-to-shared-refill 60/30 wrappers, a TX
  Reduction success script and a Reciprocity abort-return label. Shared Break
  Bar refill/resolution algorithms are unchanged; assembly label availability,
  native argument layout and controller waits need review on upstream changes.

The substantive implementation stays in custom `battle_tx_health.c`,
`battle_tx_rollback.c`, `battle_tx_wishes.c`, `battle_reduction.c` and the existing
`battle_koanko.c:KoankoOnSwitchIn` entry hook. Original Reduction calls use the
idempotent 10+ wrapper; solo calls request 1+. No generic AI algorithm, type chart
or new Break Bar executor is added. This limits new integration surface, but the
shared controller/stat-copy paths remain semantic maintenance hotspots. Overlap
with later upstream edits predicts review work, not a proven textual conflict.

Required regressions: all of `test/battle/tx_profiles.c`, `TX Wishes`,
`Arcanum Wish More Wishes`, `Arcanum Wish Reduction`, `Wishful Thinking`,
`Break Bar`, `No Order Change`, `Koanko`, and adjacent `Arcanum`; normal ROM
build. New mechanism fixtures inject Wish scores and do not establish production
AI selection quality. Stage05 must add that separate evidence.

## Koanko per-mon dynamic tactics follow-up (2026-11-10)

The user explicitly requested an individual conditional block for all twelve
roster members and accepted retaining the preceding shared AI improvements.
Current behavior and test results: [Koanko trainer AI](koanko_trainer_ai.md).

- `src/battle_ai_main.c:BattleAI_SetupFlags`: one narrow registration hook invokes
  `KoankoSetupTrainerAi` only when no explicit dynamic callback is installed.
  This makes the original six reach their dynamic blocks before conjuring.
  Encounter validation and registration are in the custom module. Script/test
  callback precedence and existing cleanup/reset are preserved.
- `include/battle_koanko.h:KoankoSetupTrainerAi`: declaration for that hook; no
  battle/save state layout or ownership storage is added.
- `src/battle_koanko_ai.c:AI_KoankoTrainer`: explicit roster blocks, real cache
  damage/accuracy and observable conditions. Smeargle retains its existing rank
  path without stacking new ordinary-roster bonuses. Current-form handlers do
  not implement missing form scripts, TX virtual profiles or Zacian ownership.

All new mon-specific tuning stays in the custom file. There is no additional
global per-species scorer or replacement switch/item/execution path. Upstream
maintenance risk is concentrated in initialization order and dynamic callback
lifetime/flag derivation; scoring arithmetic remains shared through the existing
API. The prior shared Break Bar, signed-score, cache and Max-effect work is kept.
The `Koanko Tactics` and full `Koanko` prefixes validate the new route, with
ordinary `AI`, `Arcanum` and `TX Wishes` as adjacent controls.

Zacian analysis does not add a runtime hook: Stage07 still needs an explicit
independent owner, AI recognition, player-partner controller/override gating and
resource isolation. The one per-battle callback may then route that owner to a
separate allied helper before the enemy-side guard. Species alone is insufficient.

## Koanko Stage11 AI validation and shared outcome/cache fixes (2026-11-10)

Validation status and per-combatant evidence are maintained in
[Stage11](../extra%20docs/KoankoDev/planning/11_koanko_ai_validation.md#9-implementation-record-2026-11-10).
The user explicitly approved CHECK_VIABILITY and SMART_TERA in the authored
Koanko trainer profile; these use ordinary AI and do not force an opener or Tera.

- `battle_ai_main.c:ChooseMoveOrAction_Singles/Doubles`: signed maximum comparison,
  authoritative real-slot limitation filtering, and signed-invalid target sentinels.
  Arithmetic/target-selection correction, not a new scoring policy. Boundary fixtures
  include scores above 255, zero, negative scores and INT32_MIN. Target validity
  is separate from its signed score; all-unusable/no-target queries stay bounded.
  Ordinary/virtual exact ties
  continue to favor ordinary moves.
- `battle_ai_util.c:CanEndureHit`, `AI_GetDamageOutcome`, `AI_GetReachableDamage`,
  `CanAIFaintTarget`, `CanTargetMoveFaintAi`: share the existing execution interception
  predicate from `battle_break_bar.c:CanInterceptBreakBar`. Original-layer breaks
  and saved-layer reveals are progress, not KOs, including multi-hit overkill.
  No layer scripts run during valuation; no future-form damage is extrapolated.
  This affects shared lethal reasoning, so upstream KO/survival helpers require
  semantic review even when there is no textual conflict. An unimplemented Arceus
  survival floor is deliberately not fabricated here.
- `battle_ai_main.c:CalcBattlerAiMovesData/SetAiLogicDataForTurn`: clear removed or
  newly unusable slots and refresh only DOUBLE_BATTLE topology flags at the normal
  fresh-turn boundary, preserving explicit zero flags. Preserve encounter/sequence/dynamic flags and committed
  actions. No new mid-turn decision loop or ownership model.
- `battle_ai_util.c:AI_CalcDamage`: restore simulation nesting and transient
  type/category/hit-counter/power scratch state, including its healing-ally early
  return. Existing ordinary damage/type/gimmick engine remains authoritative.
- `battle_ai_util.c:DecideTerastal/ShouldTeraFromCalcs`: keep deferred-Tera
  effectiveness consistent with its already-updated damage row; count intended
  living Tera users through `battle_gimmick.c:ShouldTrainerPartyMonUseGimmick`
  rather than treating every stored Tera type as trainer permission. The existing
  battler API delegates to that shared party-index query. Outcome-based offensive
  KO reasoning cannot mistake a layer break for victory. Actual consumption and
  Stage01's one-time opponent reset are unchanged.
- `battle_ai_util.c:ShouldUseWishAromatherapy`: No Order Change limits Wish's
  reserve-healing assumption to its actual occupant. No new TX profile, Wish
  outcome, ability trigger scope or cooldown is introduced.
- `battle_ai_main.c:AI_CheckViability/AI_CalcAdditionalEffectScore`: real cache
  slots remain unchanged, while viability resolves the planned/active Max move's
  effects through `GetMaxMove`. This fixes the demonstrated Alcremie defect where
  Recover was credited as healing although it would execute Max Guard. Finale
  values reachable healing of living active allies, respecting Heal Block and
  pending layers. The fixture observes actual G-Max Finale damage, healing and
  one-time Dynamax use. This does not claim to finish every generic Max bad-move,
  accuracy or activation-timing edge case; adjacent known failures are tracked
  separately from Koanko's required kit.

Trainer-specific Time Stop expected utility remains in `battle_koanko_ai.c`, gated
by Koanko identity, conjured phase, opponent ownership, Dialga species and party
slot. It does not duplicate ordinary recharge scoring or change the proc chance.
The first six receive no species-specific scorer. These shared edits have more
textual overlap with upstream AI than a Koanko-only workaround, but avoid keeping
duplicate selection, survival and Tera-eligibility algorithms in custom modules.

## Koanko trainer Special AI and Smeargle ranking (2026-11-10)

`src/battle_koanko_ai.c:AI_KoankoTrainer` follows the Alice/Razeluxe dynamic trainer
callback interface. Its Smeargle policy reads existing slot-0 damage/accuracy caches,
adds KO/damage utility and execution/drawback costs, and preserves state-aware utility.
Other regular-move owners keep their incoming score. Unimplemented extra-slot policies
return 0; no generic Arcanum/Wish scoring is accidentally enabled.

- `src/battle_ai_main.c:ScriptSetDynamicAiFunc / SetDynamicAiFunc` and
  `include/battle_ai_main.h`: narrow programmatic setter for the existing callback,
  with the script setter delegating to it. Ordinary scoring algorithms unchanged.
- `src/battle_koanko.c:ConjureKoankoTeam`: register Koanko's callback and add
  AI_FLAG_DYNAMIC_FUNC at the phase transition. `data/scripts/debug.inc:Script_3`
  registers the same callback before launching the dedicated debug encounter.
  Existing battle cleanup resets the pointer. Future first-team tactics can register
  the same callback in their encounter script without duplicating the policy.
- `src/battle_ai_main.c:ScoreMoves_Singles / BattleAI_ScoreSlot0Candidate /
  AI_CheckViability / AI_CheckBadMove`: guarded diagnostic calls observe pass
  boundaries, priority, cached metadata and aggregate effect contributions. No extra
  score-function evaluation is added by these hooks. Instrumentation stays in the
  dedicated diagnostic module under its compile-time feature switch.

Maintenance: ranking remains in the custom Koanko module, uses no new BattleStruct
fields, adds no sampler-specific ranking branch or full damage-calculation pass,
and requires semantic review when upstream changes cache layout, callback lifetime,
scoring order, survival or speed helpers. Detailed records remain in EWRAM.
Verified: Koanko 44/44, TX Wishes 22/22, Arcanum 150/150, ordinary AI 310 pass with four declared known
failures and four TODOs; normal ROM build succeeds. Manual gameplay remains pending.
See [Stage 03 note](../extra%20docs/KoankoDev/PROGRESS_stage03_smeargle_2026-11-10.md)
for formula, before/after examples, timings, memory measurements and exact commands.

## Direct Koanko Smeargle debug encounter (2026-11-10)

- `src/battle_main.c:CreateNPCTrainerParty`: narrow call/early return for the one-shot
  Koanko debug party request. It runs after normal AI flag/resource initialization
  and before opening party/controller reads. The custom module creates only the real
  conjured Smeargle at party index 0 and initializes conjured phase, sequence flags
  and No Order Change. Ordinary calls fall through unchanged. Upstream changes to
  initialization ordering or party-factory dispatch require semantic review.
- `src/debug.c:sDebugMenu_Actions_Scripts` and `data/scripts/debug.inc:Debug_EventScript_Script_3`:
  repurpose unused Script 3 as "Koanko Smeargle" and invoke the dedicated native.
  No debug menu engine or battle selection algorithm changes.

Lifecycle and return handling stay in `src/battle_koanko.c`; the existing debug
transition launcher and normal party constructor are reused without modifying them.
The native deliberately avoids generic debug trainer substitution and follower-party
restoration. No new BattleStruct fields. Normal ROM build and focused Koanko suite
pass; manual entry/exit/repeat with/without a follower remains unverified here.
Details: [Stage 03 implementation note](../extra%20docs/KoankoDev/PROGRESS_stage03_smeargle_2026-11-10.md).

## No Order Change icon placement (2026-11-10)

`src/battle_status_icons.c:sIcons / UpdateIconPosition` now place No Order Change
beside and vertically centered on the healthbox, on the player left/opponent right.
A table-driven singles-player Y adjustment follows the actual healthbox layout.
These changes stay within the custom icon module; no upstream-maintained function
or integration hook changed. Existing movement, visibility and lifetime ownership
are preserved. Upstream healthbox artwork/origin changes can still require a semantic
layout adjustment without a text conflict. Existing icon geometry expectations were
updated. The user reports both focused suites (`Battle status icons` and
`Arcanum No Order Change indicator`) passing; in-game visual confirmation is pending.
See [No Order Change](no_order_change.md).

## Koanko Stage 03: Smeargle Any Move (2026-11-10)

Implemented in the working tree; current build and verification results are recorded above. Details and
test commands: [Stage 03 integration note](../extra%20docs/KoankoDev/PROGRESS_stage03_smeargle_2026-11-10.md).

- `src/battle_ai_main.c:BattleAI_ChooseMoveIndex`: narrow owner-gated hook to sample
  fresh choices after setting the ordinary gimmick assumption; temporarily enable
  CHECK_VIABILITY and restore flags on exit. Depends on action selection still running
  before controllers copy the move list and on the forced-action predicate matching
  `HandleTurnActionSelectionState` (recharge, multipleTurns, Time Stop).
- `src/battle_ai_main.c:ChooseMoveOrAction_Singles / ScoreMoves_Singles`: small shared
  scoring extraction, preserving the existing final picker and TX/Arcanum extra-slot
  logic. `BattleAI_ScoreSlot0Candidate` uses these same passes without selecting an
  attack, and saves/restores ProtectStruct scratch flags. This is more than an inserted
  hook, but avoids maintaining a duplicated singles scoring loop. Future upstream
  scoring-order changes must remain in the shared helper; review prediction and any
  new scoring side effects for repeated hypothetical evaluations.
- `src/battle_ai_main.c:BattleAI_PrepareSlot0Move`, declarations in
  `include/battle_ai_main.h`: candidate/winner API writes real slot 0, refreshes
  limitations and clears damage/effectiveness/accuracy before existing cache calculation.
  Narrow API addition. Future cache fields or calculator prerequisites must be audited
  here. Assumes this encounter keeps slots 1-3 empty; not a general multi-slot API.
- `src/battle_util.c:CancelerPPDeduction`: narrow predicate gate on PP spending only,
  covering Pressure without skipping ordinary post-deduction submove handling or
  Metronome-item bookkeeping. Recheck if upstream moves PP expenditure elsewhere.
- `src/battle_util.c:CancelerChoiceLock`: narrow predicate gate on Choice/Gorilla lock
  assignment. Existing limitation checks remain unchanged; immunity prevents new locks.
- `src/battle_script_commands.c:Cmd_trysetencore`: narrow failure-condition addition
  before Encore acquisition. Recheck if another acquisition path is introduced.
- `include/random.h:RandomTag`: appended DRAW/TIE identifiers; existing numeric tags
  preserved. Calls use shared RNG, not isolated streams.

Regression coverage: `test/battle/koanko_smeargle.c` retains 15 supplied cases and adds
two integration cases; existing Koanko lead expectation updated in `test/battle/koanko.c`.
Required broader checks include Koanko/Loaded Dice, TX Wishes, Arcanum, Encore, Choice
and Gorilla Tactics. Direct Choice/Gorilla immunity, forced continuations beyond recharge,
targeting, runtime cost and memory headroom remain validation gaps.

User build follow-up: the test link failed with 1856 bytes of IWRAM overflow. The
5088-byte full-pool observation arrays now use EWRAM_DATA in the Smeargle test file.
This changes test storage placement only; no linker script or upstream engine hook
was changed. The subsequent user run linked successfully; exact headroom is unmeasured.

User results: Smeargle 16/17, Polymorph 9/9, combined Koanko 28/30 passed, including all
three Loaded Dice cases. The remaining recharge and lead fixture errors were corrected
using explicit SKIP_TURN and a slot-0 expectation, respectively. These
fixture corrections introduce no new upstream integration points.
The recharge assertion was subsequently scoped to per-turn evaluations after the
user observed two pool scans in the lifetime counter: playback can calculate the
next fresh turn before discovering record exhaustion. It still requires one scan on
the Hyper Beam turn and none on recharge. The user subsequently reported all requested
reruns passing. The user's s16 score-observation storage change saves another 1696
bytes, leaving 3392 bytes for both EWRAM observation arrays; fixture scores are 0..53.

Maintenance impact: bespoke eligibility, shuffle, owner predicate and test statistics
live in `src/battle_koanko_smeargle.c`. No Koanko state-layout changes, new transition
hooks, custom attack executor, or modifications to Loaded Dice's damage algorithm.
Textual overlap is concentrated in the AI chooser/cache/scoring area and three small
engine conditions. Semantic overlap is broader: upstream changes to scoring mutation,
gimmicks, restrictions, targeting, PP and forced actions need review even without a
merge conflict. The shared helper reduces duplicated maintenance without replacing
the ordinary scoring algorithm.

## Stage 03.1 test-harness follow-up (2026-11-10)

`src/castoria_name_theft.c:CastoriaSelectPolymorphTargetAndBudget` now accepts a
TESTING-only cyclic species-draw override through `Test_SetPolymorphDrawSpecies`,
declared in `include/castoria_name_theft.h`. This is a custom-module test seam, with
no new upstream-maintained integration point and no production sampling change.
The caller owns the array lifetime and clears the pointer/count after use. The
updated Polymorph regression asserts that the sampled winner belongs to its injected
draw set; it has not been run here. Details: [Polymorph implementation note](../extra%20docs/KoankoDev/PROGRESS_stage03_1_polymorph_2026-11-09.md).
