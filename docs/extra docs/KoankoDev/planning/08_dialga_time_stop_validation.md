# Stage 08 — Validate Dialga's existing Roar of Eternity and replacement safety

Status: **first behavioral validation milestone passed by user report** (2026-11-18).
The move was already implemented; the original design snapshot was `mypatch`,
2026-11-09. Current source/validation details are in section 9.

## 1. Objective and scope

Retain existing Roar of Eternity, close focused hit/faint/replacement coverage and optionally keep Koanko's Dialga colored during grayscale. Do not recreate the move, change Time Stop duration globally or replace ordinary recharge.

## 2. Existing code verified

- `include/constants/moves.h:MOVE_ROAR_OF_ETERNITY` already exists. `src/data/moves_info.h` defines the required name, Dragon/Special 150 BP, accuracy 90, selected target, 5 PP, Roar of Time animation, mandatory self recharge and 50% MOVE_EFFECT_TIMESTOP. Boss move-call/copy exclusions are present.
- `src/data/koanko_party.h` already gives level-104 Dialga Origin Roar of Eternity/Flash Cannon/Earth Power/Thunderbolt.
- `src/battle_set_effect.c:HandleSetEffectTimestop` applies Time Stop only when the target volatile is zero; sets 3, applies grayscale, and does not refresh the protected cooldown value 1. The shared `SetMoveEffect` dispatcher handles secondary-effect immunity.
- `src/battle_main.c` command selection automatically commits a no-choice action for timestop>1 or recharge. Turn maintenance decrements both timers; colors restore when Time Stop becomes 1. `src/battle_move_resolution.c:CancelerRecharge` and the included `src/battle_faerie_move_resolution.inc:CancelerTimestop` remain separate ordinary cancelers.
- Existing timeline: hit turn ends at Time Stop 2/recharge 1; next turn both skip under a proc; following turn both can act with Time Stop cooldown 1 until end-turn clears it. Preserve this established behavior.
- `Cmd_openpartyscreen` is separate from ordinary action selection and checks genuine replacement/bar/slot policy. A stopped battler is not inherently disqualified from mandatory replacement by this command; runtime edge cases still need coverage.
- `BattleTimestopApplyGreyscale` copies true palettes and grays BG/UI and object banks, exempting a battler found by MOVE_CRACK_ICE. Its current single exempt-slot variable is specific to Kazuradrop. `BattleTimestopRestore` restores all from unfaded buffers.
- `test/battle/move_effect/roar_of_eternity.c` has three repaired cases: one recharge turn (Shield Dust isolates it), 50% tagged proc, and combined recharge/Time Stop recovery. `test/battle/ability/sheer_force.c` includes the move in secondary-effect coverage. User reports targeted tests passed; no fresh run was executed here.

## 3. Implementation design and order

1. Add missing negative-path and genuine-faint replacement tests around the current move; retain existing move definition/timeline and repaired harness assumptions.
2. After stage 07 cleanup, assert Dialga enters in singles. Exercise player-left with Time Stop active, damage/residual faint and a healthy reserve; replacement must be requested through the ordinary faint path before any auto-skipped command path can consume it. Fix only the ordering/gate proven defective, if any.
3. Preserve recharge after a successful hit even when the target is stopped; never use Time Stop as a reason to clear the attacker's timer. On a miss/Protect/immunity, follow ordinary Roar of Time's existing recharge rules, verified with paired fixtures rather than inventing a custom rule.
4. Optional color polish: add a narrow Koanko-Dialga exemption query alongside the existing Crack Ice predicate, building a mask of actual live battler sprite palette banks instead of one hardcoded slot. Gray only non-exempt banks in the same existing function; no new grayscale engine. Reapply the exception after menu/graphics reconstruction only while Time Stop remains active. Keep Kazuradrop's exemption and full restore behavior intact.
5. If tests reveal stale grayscale after the last stopped occupant faints/replaces, restore from the canonical unfaded palette when no stopped effect remains. Do not clear another battler's active Time Stop in doubles. Keep cleanup local to the presentation defect, not a global volatile redesign.

Proposed changes: existing Roar tests, a Koanko Dialga encounter fixture, optional palette exemption helper in `battle_util.c` with Koanko owner query; replacement/refresh ordering only if a test demonstrates a defect. No party/move-data rewrite.

## 4. Dependencies

Stage 01 identity/progression and stage 07 return-to-singles. Existing Time Stop, recharge, faint replacement and palette buffers. Core isolated move tests are independent of stage 07.

## 5. Edge cases and invariants

Shield Dust/Sheer Force/Substitute/Protect/immunity use ordinary additional-effect rules. A successful proc cannot chain or extend during nonzero volatile state. Time Stop blocks normal command choice/action, not mandatory party replacement or battle-loss cleanup. A fainted occupant's volatile does not transfer to its reserve. Recharge works if Dialga's target faints, survives via a saved Break Bar or is replaced. Optional palette logic never identifies arbitrary Dialga outside this encounter as Koanko's source.

## 6. Test strategy

Implementation commands, run natively in WSL: `make check TESTS='Roar of Eternity'`, `make check TESTS='Koanko Dialga'`, and affected existing `Sheer Force`/recharge test prefixes. Reuse the three original tests; do not revert their repaired expected countdowns.

Add hit/no proc/proc, miss, Protect, Fairy immunity, Substitute and Shield Dust comparisons against ordinary recharge behavior. Test proc followed by residual faint/reserve replacement, direct KO with reserves, no reserves/player loss, simultaneous faint, Break Charm/pending bar, item/menu entry after expiry and no re-proc during cooldown. Test new Dialga after Zacian departure has no right-slot prompt. Palette acceptance requires emulator captures of Dialga in color, player/UI grayscale, menu-return continuity and full restoration, plus Kazuradrop unchanged.

## 7. Completion criteria

Existing custom move remains intact and focused tests prove recharge/proc timing, failure handling and normal mandatory replacements without hangs. Dialga's correct kit/level and singles entry are verified. Optional color polish is either verified or explicitly deferred; it does not block mandatory battle behavior.

## 8. Open questions

- **Confirmed:** no move implementation is missing; preserve current 50% effect and ordinary recharge semantics.
- **Optional default:** include owner-specific color exemption if it remains a small palette-mask change; otherwise defer it. No global Time Stop duration changes are planned.
- Passing user-reported targeted tests establish the baseline, not coverage of untested faint/menu cases; record fresh focused results during implementation.

## 9. First validation milestone (2026-11-18)

Authored against HEAD `4093d02fb7b7` plus the current working tree. **No build,
compilation or test was executed.** This milestone adds coverage before deciding
whether any engine ordering/gate change is necessary; it does not close Stage 08.

- `test/battle/move_effect/roar_of_eternity.c` preserves the original three timing
  fixtures and adds a tagged forced non-proc, paired Roar of Time miss/Protect/Fairy
  immunity cases, Shield Dust/Covert Cloak/Sheer Force/Substitute controls, a
  doubles active-stop/cooldown re-proc attempt, and direct-damage replacement on
  the stopped turn. The doubles fixture uses a second attacker to deliver real
  damage while Dialga recharges.
- `test/battle/koanko_dialga.c` starts at the already-conjured singles boundary,
  using the existing returning Turn 0 interface only to establish encounter phase
  and No Order Change. It tests direct KO/recharge against Roar of Time, poison
  faint/replacement on the hit turn and stopped turn, real player loss, a broken
  saved Break Charm layer with exact underlying HP restored, Destiny Bond/both
  replacements, and Bag use after expiry. These are controlled mechanics
  fixtures, not a full twelve-opponent encounter or production entry-script
  configuration. They do not claim to validate the future Arceus sequence.
- `test/battle/koanko.c` extends the real conjuring fixture to assert Dialga's four
  authored moves; its existing loop already checks species and level 104.
- The existing `Koanko Zacian` departure, residual-faint and simultaneous-faint
  fixtures remain responsible for the live return to singles before Dialga.

No production source, engine integration point, move data, timing or volatile
ownership changed. No new mutable static storage or production battle state was
introduced. New fixture bytecode is `static const` (ROM). The user's IWRAM
restraint also applies to later Stage 09 work: use compact battle-owned EWRAM
state and explicitly placed EWRAM scratch if needed; do not add default mutable
statics, large stack snapshots or IWRAM code without a demonstrated requirement.

Optional owner-specific color exemption is **deferred**. Existing grayscale
cleanup/menu continuity, multiple stopped battlers and Kazuradrop presentation
still need palette-specific coverage/manual acceptance. A stale palette is a
presentation defect to investigate, not permission to clear another occupant's
Time Stop or alter recharge. No palette correctness is claimed by these new
behavioral fixtures.

Keep the current engine sources and these three test files stable while running:

```sh
make check TESTS='Roar of Eternity'
make check TESTS='Koanko Dialga'
make check TESTS='Koanko replacement creates fresh opponents'
make check TESTS='Recharge moves'
make check TESTS='Sheer Force'
make check TESTS='Koanko Zacian'
```

Record actual results here before marking the milestone verified. If a fixture
fails, distinguish a harness expectation from an engine defect and make any
necessary correction at that boundary. Full encounter/palette acceptance remains
open even if these focused fixtures pass.

## 10. First user-run failures and fixture corrections (2026-11-18)

The user reported five failing fixtures/parameter cases. No complete suite totals
were supplied, so other suites are not newly marked passed. Corrections are
authored, and the user subsequently reported all three requested reruns green:
`Koanko Dialga`, `Koanko replacement creates fresh opponents`, and
`Koanko conjures a second team`. No counts were supplied. Codex has not executed
builds or tests. The new saved-layer proc/non-proc cases are included in that
reported Dialga pass. Earlier broader suites are not inferred from these reruns.

- Residual replacement reached its healthy reserve and completed playback, which
  sets `B_OUTCOME_PLAYER_TELEPORTED` (5) in `RecordedBattle_ReadAction`. Expecting
  zero at THEN was incorrect; the fixture now asserts the synthetic completion
  outcome alongside the reserve/action/faint-counter checks.
- The fatal second turn has both commands auto-skipped. The harness tracks the
  last *recorded command* in `lastActionTurn`, not the last simulated turn. Remove
  the extra all-SKIP final TURN; scene assertions still require Dialga's recharge
  and the actual poison faint/loss during that automatically executed turn.
- A pending layer makes `IsBattlerAlive` true even at zero displayed HP. Therefore
  Roar's secondary can apply while a broken saved layer awaits restoration. The
  Break Charm fixture now explicitly rigs both proc/non-proc cases, skips the
  stopped player's next choice when applicable, and retains Roar of Time as the
  ordinary recharge control. Both paths still require exactly 7 underlying HP,
  no genuine faint, and ordinary recovery after the skipped turn.
- The older regeneration unit fixture incorrectly expected every fresh bar config
  to be NULL. It now expects the production rule's pointer/count for Calyrex and
  Eternatus, while still asserting old saved HP/item-use/fainted state is cleared.
- The older twelve-faint fixture assumed all conjured opponents had one layer
  and used a Fighting attack that cannot hit Shadow Rider. It now uses the AI
  singles harness with explicit sequential replacement expectations, one extra
  bar-breaking turn per form boss, and the supported summoned-ally declaration.
  Skill Link Icicle Spear covers Focus Sash/Shadow Rider; Feint removes TX Protect
  variability. An existing test-only Smeargle scorer limits its choice to
  Boomburst and is reset after cleanup. The max-level player avoids level-up stat
  recalculation replacing the controlled damage/HP overrides. Actual boss data,
  phase scripts and the ally lifecycle are retained, including singles at victory.

All corrections are test/documentation changes. No new engine hook, persistent
state, mutable static buffer or IWRAM placement was introduced. The first three
errors were new fixture assumptions; the last two were stale expectations in
existing fixtures that already conflict with the implemented form bars. No
gameplay regression is established by the supplied failures.

Keep the current engine and both corrected test files (`test/battle/koanko.c`,
`test/battle/koanko_dialga.c`) stable for this rerun:

```sh
make check TESTS='Koanko Dialga'
make check TESTS='Koanko replacement creates fresh opponents'
make check TESTS='Koanko conjures a second team'
```

These three reruns are now user-reported passing. The commands remain recorded
for reproduction. Palette-specific cleanup/menu continuity and manual encounter
acceptance remain open; the optional owner-color exemption remains deferred.
