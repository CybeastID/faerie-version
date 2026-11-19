# Battle sprite scale and player-avatar affine origins

Stage 09.1, 2026-11-18. Current source is on `pre-s9-real`, based on HEAD
`776e5d4e9274c7171129bf2af75e4347f9193eff`. The user explicitly authorized
autonomous implementation/testing and added all other player-avatar origins to
this scope. That authorization has since ended; testing is user-controlled and
Computer Use is revoked. This is presentation only; no new gameplay Reduction system exists.

## Scale sources and tuning

- An actual `B_TRAINER_PLAYER` mon reads `FLAG_PLAYER_SMOL` for a 128/256 base
  scale. Independent partners do not inherit that flag. The renderer never
  writes the flag, stats, HP, level, ownership or activation conditions.
- Full, unsuppressed Bug Space affects the opposing side. Its existing threshold
  maps linearly from 160 -> 256/256 down to 0 -> `BATTLE_SPRITE_SCALE_MIN`/256,
  capped at normal size.
  `BugSpaceHasDoomClock()` excludes crush-only mode without Arceus-specific code.
  Retained boost crushing therefore does not retain the shrinking component.
- Reduction uses its existing `lostLevels`, current occupant identity and the
  canonical level. Scale is `MIN + (256 - MIN) * effectiveLevel / canonicalLevel`;
  level one uses exactly `BATTLE_SPRITE_SCALE_MIN`/256. Nested stat mutations cannot briefly show
  the unreduced level. No rendering loss counter duplicates gameplay state.
- Sources compose by taking the smaller scale, rather than multiplying.
  Removing a source reveals any remaining one. Updates approach the current
  target by four Q8 units per rendered frame.
  A mon already affected before send-out starts at its appropriate size.

These are presentation choices, subject to the recorded mGBA review below.

## Rendering and lifetime

`battle_sprite_scale.c` owns the implementation. `BattleStruct.spriteScale`
contains four eight-byte records: current interpolated scale, trainer/party
identity, and a single-frame OAM transaction. This is 32 extra bytes in the
existing EWRAM battle heap allocation, with no new static rendering buffer.

`BattleMainCB2` calls `BattleSpriteScaleBuildOamBuffer` after `AnimateSprites`.
For each eligible real mon it allocates a temporary affine matrix, composes the
base scale with the animation's signed inverse matrix, and adjusts the visible
grounding offset. `BuildOamBuffer` copies the result to hardware-bound OAM.
The wrapper restores the CPU sprite/matrix binding and coordinates and frees
every temporary matrix before controllers/tasks execute again. Up to four
additional matrices are needed only during this transaction. Exhaustion skips
the visual composition for that frame and retries without corrupting an owner.

Animation matrices, rotations, flips, squash/stretch, callbacks and `y2` remain
owned by the animation engine. The generic OAM algorithm is unchanged for this
battle feature. No new animation task, affine table or Pokemon artwork is needed;
the unused tiny table alone could not preserve a baseline through animations.

Actual runtime image tables identify mons, excluding trainer portraits and UI.
Picture offsets and effective visual/Transform species determine grounding.
Back poses cropped by the text box anchor to the visible battlefield edge, so
shrinking does not bury them behind it. Elevation, formation and temporary
movement remain intact. Substitute keeps its own art; normal fade/reveal and
form reconstruction pick up the correct mon scale afterward.

`SwitchInClearSetData` resets the presentation record when occupants change.
The trainer/party key also prevents transfer to an unrelated occupant. Menu
reconstruction retains the scale in `BattleStruct` and uses the new sprite's
own matrix. Faint animation keeps its last size until removal; replacement
initializes independently. Battle teardown frees the records with the battle
allocation. No temporary matrix survives a completed frame or a battle exit.

Animation background copies now use the same baseline. `DrawBattlerOnBg` and
`AnimTask_TransformMon`/`AnimTask_HideSwapSprite` call `BattleSpriteScaleCopyMonGfx`, which samples 4bpp
pixels directly into the existing animation tile buffer. BG palette blending,
scrolling and mosaic retain their existing owners. There is no additional image
buffer or affine-BG conversion. This corrects the full-size background copies
seen during attacks and Transform. Stat changes use a different OBJ-window mask:
the compositor rebinds matching window/blended copies to the real mon's temporary
matrix and grounding in output OAM only, without changing animation CPU state or
allocating another matrix. An established size also freezes
after a battle outcome, so gameplay cleanup cannot visibly regrow a mon during
the exit fade. These corrections still require user visual review.

### Final-fade resource lifetime correction

The user narrowed the remaining SMOL snap to after EXP, during fadeout.
`FreeResetData_ReturnToOvOrDoEvolutions` previously freed `gBattleStruct`,
mon graphics and sprite data on the first active-fade frame. Subsequent render
frames therefore took the ordinary unscaled fallback despite the frozen scale.
The callback now waits for `gPaletteFade.active` to clear before any teardown.
Both evolution and direct-return branches then reach the existing common
cleanup; the direct-return branch must not skip cleanup now that earlier fade
frames no longer perform it. Recorded-playback abort already waits until faded
before freeing resources and needs no change.

No additional state, matrix ownership or IWRAM allocation is introduced. SMOL
retains its flag and scale through the visible fade. Temporary Bug Space and
Reduction sizes also retain the established exit freeze; animated restoration
is not implemented by this correction. A new parameterized regression invokes
the production exit callback through a TESTING-only accessor over four active
fade frames, checks resource identity, rendered matrix, matrix allocation and
flag preservation for ordinary, SMOL and cleaned-up Reduction fixtures.

User-run validation on 2026-11-18: the new assertion initially failed compilation
because `EXPECT_EQ` applies `typeof` to the `gPaletteFade.active` bit-field. It
now uses `EXPECT(gPaletteFade.active)`, and the user reports all focused tests
pass. No agent-run build/test or independent visual confirmation is claimed.
The SMOL victory through EXP/fade and ordinary/run/forfeit visual checks remain
unrecorded. Source files are `src/battle_main.c` and `test/battle/sprite_scale.c`.
The earlier ledger/counts below describe their own historical snapshots.

The user still observes incorrect ledge handling when the overworld player is
both affine and small, and explicitly parked that issue. Regular player position
and ledges were confirmed working. Scripted affine overworld traversal therefore
retains this known limitation; it is not a Stage 10/11 acceptance dependency.

## Player-avatar origin extension

**Revised scope following user visual review:** general overworld affine support
is disabled by default. `FLAG_PLAYER_OVERWORLD_AFFINE` uses unused flag 0x25 and
is independent of `FLAG_PLAYER_SMOL`, which keeps its battle/gameplay meaning.
Brendan's normal OAM and shadow return to the actual upstream 1.17.2 release
defaults (`79dd436920`); the other avatar states already use non-affine OAM.
Regular movement no longer writes `x2/y2` or affine facing matrices. Ledges,
Acro hopping, Fly and Surf/Dive therefore retain their normal movement offsets.
The fast underwater bobbing/entry offsets remain user visual retest cases.

Follow-up user retesting reproduced a normal-position regression through New
Game, Quick Start and Continue. A second legacy origin writer remained in
`field_player_avatar.c:AdjustAffineSpriteOffset`, called by `PlayerSetAnimId`.
It unconditionally installed (-8, -16) even for non-affine movement, formerly
masked by the removed per-frame reset. It now delegates to the same guarded
affine-origin helper and no longer writes OAM coordinates directly. The avatar
regression explicitly exercises this movement-entry hook. This correction is
confirmed by user visual retesting for the player position and ledges. No new
agent-run compilation/test result is claimed; user-controlled testing is active.

Scripted sequences opt in explicitly:

```asm
callnative EnablePlayerOverworldAffine
callnative StartPlayerShrinkAnimation
@ Sequence; wait for movement/animation completion before ending it.
callnative DisablePlayerOverworldAffine
```

Enabling permits the existing effect (and applies an existing SMOL state).
Disabling releases its matrix and removes its origin correction immediately,
without clearing SMOL or changing stats. Clearing the new flag directly also
retires an active effect on the next ordinary player movement callback. Map
authors must disable it on exits from an opted-in sequence/area; it persists
through menus/warps until cleared. This does not claim support for arbitrary
shrunken overworld traversal, shadows, footsteps or field effects.

Within opted-in sequences, the user's 16x32 correction was `(-8, -16)`.
`ApplyPlayerSpriteAffineOrigin` derives the same half-canvas correction from
each actual OAM shape/size, covering bikes, surf, underwater, fishing, watering,
field moves, decorating and alternate protagonists without editing artwork or
retuning the overworld shrink amount. Movement/map coordinates remain unchanged.

Priority-split avatar frames need more than a single offset: each affine piece
has its own doubled canvas. A narrow hook in `AddSubspritesToOamBuffer` calls
`ApplyPlayerAffineSubspriteOrigin` only for the actual player's existing shrink
tables. It transforms each piece's center about the full frame, including east
mirroring, while retaining tile selection and per-piece priorities. Other NPCs,
regular-mode sprites and temporary rotation/shear paths use the existing behavior.

`PlayerStaysShrunk`/`StartPlayerShrinkAnimation` allocate a matrix when an avatar
starts in a non-affine state, and reuse an owned one across graphics changes.
`ResetPlayerSprite` restores the active graphics' normal affine policy and frees
an unneeded matrix instead of allocating another. Missing resources/invalid
sprites return safely. Existing flag-setting and field-transition policy remain
with their existing scripts and avatar code. This does not authorize giant map
scaling or introduce a new overworld scale system.

## Kazuradrop dialogue correction

User testing found garbage in legendary punishment and other Kazura dialogue.
`ShowKazuraDialogueBox` handed an asynchronous printer a local 512-byte stack
buffer, which expired on return. Its wrapped string now lives in an EWRAM heap
allocation owned by the open dialogue. Hide/graphics teardown deactivates the
printer, removes its window and frees the text, in that order. Repeated hide is
safe. No shared text-printer algorithm changed. The headless battle-script path
had skipped this renderer; `kazuradrop_dialogue.c` now exercises real printers,
stack reuse, both legendary tiers, ordinary taunts and dismissal.

## Size tuning (unchanged at the user's request)

Battle SMOL uses `BATTLE_SPRITE_SCALE_SMOL` in `battle_sprite_scale.h`: 128/256.
For example 64 gives quarter size. `BATTLE_SPRITE_SCALE_MIN` is the independent
Bug Space/Reduction floor; it does not clamp a smaller SMOL choice.
Overworld `gAffineAnim_ShrunkPlayer` in `sprite_effects.c` uses 0x4C/256.
The shrinking transition currently reaches the same value with -3 for 60 frames.
For a 0x30 endpoint, an example matching transition is -4 for 52 frames.
These are instructions for the author; this correction does not change them.

## Verification before the user's regression report (historical snapshot)

Latest CLI evidence before the subsequent movement-entry correction: battle
scale 20/20; avatar scale 8/8; real Kazura printer 1/1 (four message variants);
Kazuradrop 15 passed/one existing known failure; Transform 14/14; Polymorph 9/9;
Substitute 10/10; ordinary OAM 5/5; sheet resizing 1/1. Koanko was 174/175, with
the added Zacian scaling assertion changing SMOL after the recorded fixture
had already exited. That fixture now temporarily restores an ongoing outcome
for its simulated flag change, preserving the production exit-size freeze.
The last normal link used EWRAM 235,616 B, IWRAM 28,808 B (unchanged), and ROM
27,091,240 B. None of these results certify the later movement-entry or Zacian
fixture corrections; the user must build and run those changes.

- `Battle Sprite Scale`: 15/15 groups pass after the visible-back grounding fix.
- `Player Sprite Scale`: 6/6 groups pass, including both genders/all avatar
  states, alternate protagonists/decorating, bridge priority pieces, mirroring,
  repeated resets, matrix exhaustion and unrelated actors.
- `Koanko Zacian arrives`: 1/1 passes with real independent-ally ownership and
  asymmetric positioning assertions.
- Normal ROM before the final grounding adjustment: IWRAM 28,808 B and linked
  EWRAM 235,608 B, both unchanged; ROM 27,090,304 B (+2,704 B). The battle heap
  record adds 32 B despite unchanged fixed-region totals. Final link and broader
  regressions still need recording.
- mGBA has shown progressive/minimum/restored Wailord, small Diglett and tall
  Exeggutor art. A minimum Exeggutor back screenshot exposed excessive text-box
  clipping; the corrected visible-edge anchor passes automated checks and still
  awaits the direct after-frame comparison. Final tuning/acceptance is pending.

Local evidence and the immutable, unshrunk Stage 09 baseline are in
`.stage09_1_validation/`. Stage 09 visual acceptance uses that separate baseline
as well as affected regressions; shrinking is not a prerequisite for accepting it.

The subsequent user review confirmed small send-outs but reported regular-mode
ledge/Acro/Fly/Surf offsets, fast Dive bobbing, Kazura text corruption, full-size
animation copies and exit growth. Those findings supersede any implied visual
acceptance above. Computer Use is revoked. Current fixes require focused CLI
verification and user visual retesting; the Stage 10/11 acceptance track is held
at this correction boundary. Do not certify the earlier tests as proof of the
reported renderer paths: most inspected the eventual scale after animations.
