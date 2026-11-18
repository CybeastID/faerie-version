#ifndef GUARD_BATTLE_ENTRY_SCRIPTS_H
#define GUARD_BATTLE_ENTRY_SCRIPTS_H

#include "constants/battle_entry_scripts.h"
#include "constants/difficulty.h"

struct Pokemon;
struct Trainer;
struct TrainerBreakBarRules;

struct MonEntryScript
{
    const u8 *script; // Returning battle-script subroutine; static lifetime.
    u8 policy;
};

enum EntryScriptSelector
{
    ENTRY_SCRIPT_SPECIES,
    ENTRY_SCRIPT_SOURCE_ENTRY,
};

struct MonEntryScriptRule
{
    enum EntryScriptSelector selector;
    u16 value; // Exact species or zero-based source party/pool entry.
    const struct MonEntryScript *config; // NULL disables the matching entry.
};

struct TrainerBattleScripts
{
    const u8 *turnZeroScript;
    const struct MonEntryScriptRule *entries;
    u16 count;
};

#define ENTRY_SCRIPT_RULES(array) .entries = (array), .count = ARRAY_COUNT(array)
#define INLINE_TRAINER_BATTLE_SCRIPTS(opening, ...) \
    (&(const struct TrainerBattleScripts) { .turnZeroScript = (opening), \
        ENTRY_SCRIPT_RULES(((const struct MonEntryScriptRule[]) { __VA_ARGS__ })) })

struct WildBattleScripts
{
    const u8 *turnZeroScript;
    struct MonEntryScript onEntry;
};

extern const struct TrainerBattleScripts gEmptyTrainerBattleScripts;
const struct TrainerBattleScripts *GetTrainerBattleScripts(u16 trainerId, enum DifficultyLevel difficulty);
const struct MonEntryScript *ResolveMonEntryScript(const struct TrainerBattleScripts *config, u32 species, u32 sourceEntry);
u8 CreateNPCTrainerPartyWithScripts(struct Pokemon *party, const struct Trainer *trainer, u16 trainerId, const struct TrainerBreakBarRules *bars);
void BattleEntryScriptsBeginParty(struct Pokemon *party);
void BattleEntryScriptsRecordMon(struct Pokemon *party, u32 slot, u32 species, u32 sourceEntry);
void BattleEntryScriptsApplyPending(void);
void BattleEntryScriptsInitializeWild(const struct WildBattleScripts *config);
void BattleEntryScriptsClear(void);
bool32 BattleEntryScriptsConfigured(void);
bool32 TryRunMonEntryScript(u32 battler);
bool32 TryRunTurnZeroScript(void);
// Returning event payload; reuses entry context preservation without consuming
// an entry run or creating an opening callback. Caller owns event timing/latch.
bool32 TryRunBattleEventScript(u32 battler, const u8 *script);
bool32 HandleOpeningScriptFaints(void);
void FinishOpeningScriptFaints(void);
void BS_CallConfiguredEntryScript(void);
void BS_FinishConfiguredEntryScript(void);
void BS_JumpIfEntryScriptReentry(void);
void BS_SetEntryTrainerSlideMessage(void);

#endif
