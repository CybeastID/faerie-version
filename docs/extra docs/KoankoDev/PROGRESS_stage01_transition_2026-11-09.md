# Stage 01 transition polish (commit ba3fd15d, local only, NOT pushed, NOT compiled/run)

## Playtest follow-up (2026-11-18; implemented, uncompiled/unrun)

`ConjureKoankoTeam` now leaves the cleared opponent battler state marked
`notOnField`. The party is already regenerated while the fainted battle mon and
old party index remain selected until ordinary replacement; an old index of 5
therefore addresses the future Arceus. Clearing the flag let party-menu return
reconstruct Arceus and display its healthbox before Smeargle's actual send-out.
Keeping it true suppresses that reconstruction; existing `SwitchInClearSetData`
clears it when the real replacement loads. Party order, phase transitions,
scripts, AI and sprite/healthbox allocation routines are unchanged.

`test/battle/koanko_reshow.c` exercises real simultaneous player/opponent faints,
the conjuring transition, full menu close/reshow and ordinary Smeargle send-out.
Both outgoing slot 0 and slot 5 are covered. The replay harness bypasses actual
party selection, so restoration is injected by a test task at the real transition
boundary. It checks absence of reconstructed opponent mon sprites and a hidden
healthbox before entry, then one visible Smeargle and healthbox. The original
state-preservation unit also asserts `notOnField`. User-run focused tests, ROM
build and manual party-menu reproduction are pending; no computer use was used.

## Implemented
1. Presentation: BattleScript_KoankoConjureTeam is now fake-victory sting -> existing two-line reveal slide -> crossfade to mus_vs_kazuradrop -> replacement. Reuses BS_KazuradropFakeVictorySting / BS_KazuradropSendoutMusic (generic bodies). Outcome stays 0; no win/reward code.
2. Music identity (Codex stage 01 left this open): VERIFIED. sound/song_table.inc entry #562 is `song mus_vs_kazuradrop`, and MUS_KAZURADROP_APPEARS == 562 (also END_MUS).
3. Tera: ConjureKoankoTeam clears ONLY gimmick.activated[OPPONENT_LEFT][GIMMICK_TERA]. Player Tera and opponent Dynamax untouched. The function is single-shot (CONJURING -> CONJURED), so no repeat renewal. Does not activate Tera. opponentMonCanTera already rebuilds to Calyrex only (1<<2).
4. AI: AI_FLAG_SEQUENCE_SWITCHING added to aiFlags[OPPONENT_LEFT] at the transition (null-guarded; the existing unit test runs without an AI struct). First-team AI untouched.
5. No Order Change: KoankoOnSwitchIn at the end of SwitchInClearSetData (which clears the flag at its top) sets it for opponent occupants in the CONJURED phase. Faint replacement is unaffected (openpartyscreen only blocks a LIVING occupant).

## Tests (test/battle/koanko.c; all unrun)
- unit test: Tera flags seeded both sides; asserts opp cleared / player kept / Dynamax kept / second call cannot re-clear.
- AI test: asserts HasNoOrderChange and the sequence flag on the conjured lead.
- new: Roar fails to phaze a living conjured Smeargle. Shape is copied from the AI test; the player-side expectations are my best reading.

## Deliberately NOT done (no consumers yet)
Entry-event flags for TX/Arceus lines and the conjured-turn counter. They belong with the TX and Arceus stages; adding unused state now is noise.

## Answers to the Codex plan / PDF
- Stage 02 doubt about the u8 growth count: settled by exhaustive check over every stat value 0..65535. Growth stops within 98 steps from ANY start (starts 0-4 never grow, everything else reaches exactly 65535), and 255 vs 1000 iterations agree. The count can never lose observable history, including after a form change to a different base. My earlier comment was right in effect; the wording "never binds" is now backed by that proof. Transform / Power Trick remain separate, untested questions.
- Script variables (PDF tail): good for state that event scripts outside battle must read; there is already precedent (VAR_KAZURADROP_ENTRY_TURN). For per-battle flags (once-only dialogue, turn counters) I'd keep battle-owned memory: it resets for free on a whiteout-and-retry, a stale save-backed value would silently suppress the dialogue, and tests stay isolated. Bug Space threshold/decay are consulted during move legality; migrating them would touch every test that assigns bugSpace.* directly. Do it as its own audited refactor, not folded into this fight.

## Risks to check when you build
- Reusing Kazuradrop natives inside the test runner (precedent exists, not proven for Koanko's 12-faint test).
- BS names are Kazuradrop-flavoured; rename to generic ones if that bothers you.
