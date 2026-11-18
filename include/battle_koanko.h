#ifndef GUARD_BATTLE_KOANKO_H
#define GUARD_BATTLE_KOANKO_H

enum KoankoPhase
{
    KOANKO_ORDINARY_TEAM,
    KOANKO_CONJURING,
    KOANKO_CONJURED_TEAM,
};

enum KoankoArceusPhase
{
    KOANKO_ARCEUS_DORMANT,
    KOANKO_ARCEUS_WAIT_SELECTION,
    KOANKO_ARCEUS_SELECTED_TURN,
    KOANKO_ARCEUS_OPENING_READY,
    KOANKO_ARCEUS_OVERCHARGING,
    KOANKO_ARCEUS_COVERED,
    KOANKO_ARCEUS_ADAPT_PENDING,
    KOANKO_ARCEUS_ADAPTING,
    KOANKO_ARCEUS_ADAPTED,
    KOANKO_ARCEUS_FINISHED,
};

// Battle-owned EWRAM, not map-lifetime flags or default IWRAM statics.
// Packing avoids APCS-GNU's ordinary struct tail padding. Do not pass the
// address of a packed species field to APIs expecting an aligned u16 pointer.
struct __attribute__((packed)) KoankoArceusState
{
    u8 phase;
    u8 battler;
    u8 trainer;
    u8 partyIndex;
    u8 savedLayer;
    u8 breakType;
    u8 breakSource;
    u8 savedScriptingBattler;
    u16 formSpecies;
    u16 originalSpecies;
};

struct MonEntryScript;
// No-op opt-in marker for focused protection/hand-off fixtures.
extern const struct MonEntryScript gKoankoArceusProtectionEntry;
// Production staged opening installed when Koanko's second party is created.
extern const struct MonEntryScript gKoankoArceusEntry;
void KoankoArceusOnSwitchIn(u32 battler);
void KoankoArceusOnCommandSelection(void);
void KoankoArceusOnEndTurn(void);
void KoankoArceusOnFaint(u32 battler);
bool32 KoankoArceusOwnsBattler(u32 battler);
bool32 KoankoArceusIsProtected(u32 battler);
bool32 KoankoArceusOpeningReady(u32 battler);
bool32 KoankoArceusFinishOpening(u32 battler, u32 savedLayer);
bool32 KoankoArceusStartBugSpace(u32 battler);
bool32 KoankoArceusDismissClock(u32 battler);
bool32 KoankoArceusTryOvercharge(void);
void BS_KoankoArceusCheckOpening(void);
void BS_KoankoArceusShowBugSpace(void);
void BS_KoankoArceusDismissClock(void);
void BS_KoankoArceusHeal(void);
void BS_KoankoArceusGrantBar(void);
void BS_KoankoArceusFinishOvercharge(void);
void KoankoArceusOnHpUpdate(u32 battler, u32 oldHp, u32 source, enum Type type);
const u8 *KoankoArceusGetBreakScript(u32 battler, u32 layer);
void KoankoArceusCommitBreak(u32 battler);
enum Species KoankoArceusGetForm(u32 battler);
void KoankoArceusFinishBattle(void);
void BS_KoankoArceusAdapt(void);
void BS_KoankoArceusFinishAdapt(void);
s32 KoankoArceusClampHpLoss(u32 battler, s32 damage);
void KoankoArceusClampMoveDamage(u32 battler);
void KoankoArceusEmitHpDrop(u32 battler);
bool32 KoankoArceusRepairFaint(u32 battler);

bool32 TryStartKoankoConjuring(const u8 *returnInstr);
void ConjureKoankoTeam(void);
void BS_ConjureKoankoTeam(void);
void BS_ApplyKoankoBossForm(void);
bool32 KoankoApplyBossForm(u32 battler);
void KoankoOnSwitchIn(u32 battler);
bool32 KoankoIsEternamaxTarget(u32 battler);
s32 AI_KoankoTrainer(u32 battlerAtk, u32 battlerDef, u32 move, s32 score);
void KoankoSetupTrainerAi(void);

// Selection only: rank resistance to the captured breaking attack, then Judgment
// effectiveness against the CURRENT foe's effective types (GetBattlerTypes at
// seal resolution, not the original attacker's types). NULL foeTypes omits the
// offensive tie-break. Final equals use tagged uniform RNG. FALSE preserves the
// current species for an invalid/untyped source or empty resistance set. The
// lifecycle caller separately checks ownership and consumes the seal-charged
// layer once, including when no resisting form exists.
bool32 KoankoChooseArceusResistantForm(enum Type attackType, enum Species currentSpecies,
                                     const enum Type foeTypes[3], enum Species *chosenSpecies);

struct Pokemon;
struct ScriptContext;
void Script_StartKoankoSmeargleDebugBattle(struct ScriptContext *ctx);
bool32 KoankoTryCreateDebugSmeargleParty(struct Pokemon *party, u16 trainerNum, bool32 firstTrainer);

#endif
