#include "global.h"
#include "battle.h"
#include "battle_koanko.h"
#include "battle_controllers.h"
#include "battle_entry_scripts.h"
#include "battle_interface.h"
#include "battle_gfx_sfx_util.h"
#include "battle_script_native.h"
#include "battle_scripts.h"
#include "battle_setup.h"
#include "battle_util.h"
#include "kazuradrop_battle_ui.h"
#include "random.h"
#include "sprite.h"
#include "task.h"
#include "constants/battle_anim.h"
#include "constants/battle_script_commands.h"
#include "constants/trainers.h"

STATIC_ASSERT(sizeof(struct KoankoArceusState) == 12, KoankoArceusStateSize);

static const u8 sProtectionEntry[] = {B_SCR_OP_RETURN};
const struct MonEntryScript gKoankoArceusProtectionEntry =
{
    .script = sProtectionEntry,
    .policy = ENTRY_SCRIPT_ONCE_PER_BATTLE,
};

const struct MonEntryScript gKoankoArceusEntry =
{
    .script = BattleScript_KoankoArceusEntry,
    .policy = ENTRY_SCRIPT_ONCE_PER_BATTLE,
};

static bool32 ArceusIdentityMatches(u32 battler)
{
    if (gBattleStruct == NULL || battler >= gBattlersCount)
        return FALSE;
    const struct KoankoArceusState *state = &gBattleStruct->koankoArceus;
    return state->phase != KOANKO_ARCEUS_DORMANT
        && state->phase != KOANKO_ARCEUS_FINISHED
        && state->battler == battler
        && state->trainer == GetBattlerTrainer(battler)
        && state->partyIndex == gBattlerPartyIndexes[battler];
}

static void ClearArceusBugSpace(u32 battler)
{
    if (gBattleStruct->bugSpace.active
     && gBattleStruct->bugSpace.mode != BUGSPACE_MODE_KAZURADROP
     && gBattleStruct->bugSpace.sourceBattler == battler)
    {
        DestroyBugSpacePanel(battler);
        memset(&gBattleStruct->bugSpace, 0, sizeof(gBattleStruct->bugSpace));
    }
}

static void RestoreArceusForm(void)
{
    struct KoankoArceusState *state = &gBattleStruct->koankoArceus;
    if (state->formSpecies == SPECIES_NONE || state->originalSpecies == SPECIES_NONE
     || state->trainer >= MAX_BATTLE_TRAINERS || state->partyIndex >= PARTY_SIZE)
        return;
    struct Pokemon *mon = &gParties[state->trainer][state->partyIndex];
    enum Species originalSpecies = state->originalSpecies;
    // Retire only our override, never a subsequent foreign species mutation.
    if (GetMonData(mon, MON_DATA_SPECIES) == state->formSpecies)
        SetMonData(mon, MON_DATA_SPECIES, &originalSpecies);
    if (state->battler < gBattlersCount
     && GetBattlerTrainer(state->battler) == state->trainer
     && gBattlerPartyIndexes[state->battler] == state->partyIndex
     && gBattleMons[state->battler].species == state->formSpecies)
    {
        gBattleMons[state->battler].species = state->originalSpecies;
        gBattleMons[state->battler].types[0] = GetSpeciesType(state->originalSpecies, 0);
        gBattleMons[state->battler].types[1] = GetSpeciesType(state->originalSpecies, 1);
        gBattleMons[state->battler].types[2] = TYPE_MYSTERY;
    }
    state->formSpecies = SPECIES_NONE;
}

bool32 KoankoArceusOwnsBattler(u32 battler)
{
    return ArceusIdentityMatches(battler) && !(gAbsentBattlerFlags & (1u << battler));
}

void KoankoArceusOnSwitchIn(u32 battler)
{
    if (gBattleStruct == NULL || battler >= gBattlersCount)
        return;
    struct KoankoArceusState *state = &gBattleStruct->koankoArceus;
    if (state->phase != KOANKO_ARCEUS_DORMANT)
    {
        if (state->battler == battler && !ArceusIdentityMatches(battler))
        {
            ClearArceusBugSpace(battler);
            RestoreArceusForm();
            state->phase = KOANKO_ARCEUS_FINISHED;
        }
        return; // Never rearm on reentry, revival or a graphics refresh.
    }
    if (!(gBattleTypeFlags & BATTLE_TYPE_TRAINER)
     || (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_FRONTIER | BATTLE_TYPE_TWO_OPPONENTS))
     || TRAINER_BATTLE_PARAM.opponentA != TRAINER_KOANKO
     || gBattleStruct->koankoPhase != KOANKO_CONJURED_TEAM
     || GetBattlerTrainer(battler) != B_TRAINER_OPPONENT_A
     || GET_BASE_SPECIES_ID(gBattleMons[battler].species) != SPECIES_ARCEUS_NORMAL
     || gBattleMons[battler].hp == 0
     || (GetBattlerPartyState(battler)->entryScript != &gKoankoArceusProtectionEntry
      && GetBattlerPartyState(battler)->entryScript != &gKoankoArceusEntry))
        return;
    state->battler = battler;
    state->trainer = GetBattlerTrainer(battler);
    state->partyIndex = gBattlerPartyIndexes[battler];
    state->savedLayer = BREAK_BAR_MAX_EXTRA_BARS; // No tagged saved layer yet.
    state->breakType = TYPE_NONE;
    state->breakSource = MAX_BATTLERS_COUNT;
    state->formSpecies = SPECIES_NONE;
    state->originalSpecies = GetMonData(GetBattlerMon(battler), MON_DATA_SPECIES);
    state->phase = KOANKO_ARCEUS_WAIT_SELECTION;
}

bool32 KoankoArceusIsProtected(u32 battler)
{
    return KoankoArceusOwnsBattler(battler)
        && gBattleStruct->koankoArceus.phase >= KOANKO_ARCEUS_WAIT_SELECTION
        && gBattleStruct->koankoArceus.phase <= KOANKO_ARCEUS_OVERCHARGING;
}

void KoankoArceusOnCommandSelection(void)
{
    if (gBattleStruct != NULL && gBattleOutcome == 0
     && gBattleStruct->koankoArceus.phase == KOANKO_ARCEUS_WAIT_SELECTION
     && KoankoArceusOwnsBattler(gBattleStruct->koankoArceus.battler))
        gBattleStruct->koankoArceus.phase = KOANKO_ARCEUS_SELECTED_TURN;
}

void KoankoArceusOnEndTurn(void)
{
    if (gBattleStruct != NULL && gBattleOutcome == 0
     && gBattleStruct->koankoArceus.phase == KOANKO_ARCEUS_SELECTED_TURN
     && KoankoArceusOwnsBattler(gBattleStruct->koankoArceus.battler))
    {
        // Reserve the next stack position only after all opening-turn grants
        // have finished. An older Break Charm/Wish layer cannot satisfy the
        // later requirement to grant a NEW seal-owned covering layer.
        gBattleStruct->koankoArceus.savedLayer = GetBreakBarCount(gBattleStruct->koankoArceus.battler);
        gBattleStruct->koankoArceus.phase = KOANKO_ARCEUS_OPENING_READY;
    }
}

bool32 KoankoArceusOpeningReady(u32 battler)
{
    return KoankoArceusOwnsBattler(battler)
        && (gBattleStruct->koankoArceus.phase == KOANKO_ARCEUS_OPENING_READY
         || gBattleStruct->koankoArceus.phase == KOANKO_ARCEUS_OVERCHARGING);
}

bool32 KoankoArceusFinishOpening(u32 battler, u32 savedLayer)
{
    if (!KoankoArceusOpeningReady(battler) || savedLayer >= BREAK_BAR_MAX_EXTRA_BARS
     || (gBattleStruct->bugSpace.active && gBattleStruct->bugSpace.sourceBattler == battler
      && gBattleStruct->bugSpace.mode == BUGSPACE_MODE_KOANKO_OPENING)
     || savedLayer != gBattleStruct->koankoArceus.savedLayer
     || GetBreakBarCount(battler) != savedLayer + 1 || IsBreakBarPending(battler)
     || GetBattlerPartyState(battler)->breakBarSavedMaxHp[savedLayer] == 0
     || gBattleMons[battler].hp != gBattleMons[battler].maxHP)
        return FALSE;
    // The caller must dismiss its owned clock and finish grant/fill/animation
    // before this commit. Failure retains the floor; success never heals here.
    gBattleStruct->koankoArceus.savedLayer = savedLayer;
    gBattleStruct->koankoArceus.phase = KOANKO_ARCEUS_COVERED;
    return TRUE;
}

bool32 KoankoArceusStartBugSpace(u32 battler)
{
    if (!KoankoArceusIsProtected(battler) || gBattleStruct->bugSpace.active)
        return FALSE;
    // Explicit encounter initialization: no borrowed ability, Kazura flags,
    // configured bar, Guts, move swaps or legendary punishment.
    memset(&gBattleStruct->bugSpace, 0, sizeof(gBattleStruct->bugSpace));
    gBattleStruct->bugSpace.mode = BUGSPACE_MODE_KOANKO_OPENING;
    gBattleStruct->bugSpace.sourceBattler = battler;
    gBattleStruct->bugSpace.bpThreshold = 90;
    gBattleStruct->bugSpace.bpDecayRate = 10;
    gBattleStruct->bugSpace.active = TRUE;
    CreateBugSpacePanel(battler);
    return TRUE;
}

bool32 KoankoArceusDismissClock(u32 battler)
{
    if (!KoankoArceusOpeningReady(battler) || !gBattleStruct->bugSpace.active
     || gBattleStruct->bugSpace.sourceBattler != battler
     || gBattleStruct->bugSpace.mode != BUGSPACE_MODE_KOANKO_OPENING)
        return FALSE;
    // Commit persistent hiding before any presentation yields. All clock/tier
    // consumers consult mode; threshold/tier values are not disable sentinels.
    gBattleStruct->bugSpace.mode = BUGSPACE_MODE_KOANKO_CRUSH_ONLY;
    TriggerBugSpacePanelTransformSlide(battler);
    return TRUE;
}

bool32 KoankoArceusTryOvercharge(void)
{
    if (gBattleStruct == NULL || gBattleOutcome != 0
     || gBattleStruct->koankoArceus.phase != KOANKO_ARCEUS_OPENING_READY)
        return FALSE;
    u32 battler = gBattleStruct->koankoArceus.battler;
    if (!KoankoArceusOwnsBattler(battler)
     || GetBattlerPartyState(battler)->entryScript != &gKoankoArceusEntry
     || GetBreakBarCount(battler) != gBattleStruct->koankoArceus.savedLayer
     || GetBreakBarCount(battler) >= BREAK_BAR_MAX_EXTRA_BARS || IsBreakBarPending(battler)
     // Runner wrapper + payload, with room for an ordinary healing continuation.
     || gBattleResources->battleScriptsStack->size + 3 > ARRAY_COUNT(gBattleResources->battleScriptsStack->ptr))
        return FALSE;
    if (!TryRunBattleEventScript(battler, BattleScript_KoankoArceusOvercharge))
        return FALSE;
    // Commit before the returning script yields; never schedule a second seal.
    gBattleStruct->koankoArceus.phase = KOANKO_ARCEUS_OVERCHARGING;
    return TRUE;
}

void BS_KoankoArceusCheckOpening(void)
{
    NATIVE_ARGS(const u8 *failInstr);
    u32 battler = gBattleScripting.battler;
    gBattlescriptCurrInstr = KoankoArceusIsProtected(battler)
        && GetBattlerPartyState(battler)->entryScript == &gKoankoArceusEntry
        ? cmd->nextInstr : cmd->failInstr;
}

void BS_KoankoArceusShowBugSpace(void)
{
    NATIVE_ARGS();
    u32 battler = gBattleScripting.battler;
    if (KoankoArceusIsProtected(battler))
    {
        // A foreign active field keeps its owner and data. Presentation and
        // the finite protection lifecycle must still make forward progress.
        KoankoArceusStartBugSpace(battler);
        CreateAbilityPopUpWithName(battler, ABILITY_BUG_SPACE, IsDoubleBattle(), COMPOUND_STRING("Alter Ego's"));
    }
    gBattlescriptCurrInstr = cmd->nextInstr;
}

void BS_KoankoArceusDismissClock(void)
{
    NATIVE_ARGS(const u8 *failInstr);
    u32 battler = gBattleScripting.battler;
    if (!KoankoArceusOpeningReady(battler))
    {
        gBattlescriptCurrInstr = cmd->failInstr;
        return;
    }
    // Only our opening clock is ours to dismiss. A foreign field, absent UI,
    // or an already dismissed clock cannot strand the survival floor.
    KoankoArceusDismissClock(battler);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

void BS_KoankoArceusHeal(void)
{
    NATIVE_ARGS();
    u32 battler = gBattleScripting.battler;
    // Revised user choreography explicitly heals the underlying HP before its
    // extra layer is granted. This is an ordinary HP heal, not status curing.
    SetHealAmount(battler, gBattleMons[battler].maxHP);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

void BS_KoankoArceusGrantBar(void)
{
    NATIVE_ARGS(const u8 *failInstr);
    u32 battler = gBattleScripting.battler;
    bool32 ready = KoankoArceusOpeningReady(battler)
        && GetBreakBarCount(battler) == gBattleStruct->koankoArceus.savedLayer;
    gBattlescriptCurrInstr = ready && GrantSavedBreakBar(battler) ? cmd->nextInstr : cmd->failInstr;
}

void BS_KoankoArceusFinishOvercharge(void)
{
    NATIVE_ARGS(const u8 *failInstr);
    gBattlescriptCurrInstr = KoankoArceusFinishOpening(gBattleScripting.battler, gBattleStruct->koankoArceus.savedLayer)
        ? cmd->nextInstr : cmd->failInstr;
}

void KoankoArceusOnHpUpdate(u32 battler, u32 oldHp, u32 source, enum Type type)
{
    if (!KoankoArceusOwnsBattler(battler) || oldHp == 0 || gBattleMons[battler].hp != 0
     || gBattleStruct->koankoArceus.phase != KOANKO_ARCEUS_COVERED
     || GetBreakBarCount(battler) != gBattleStruct->koankoArceus.savedLayer + 1
     || GetBattlerPartyState(battler)->breakBarSavedMaxHp[gBattleStruct->koankoArceus.savedLayer] == 0)
        return;
    gBattleStruct->koankoArceus.breakType = type;
    gBattleStruct->koankoArceus.breakSource = source;
}

const u8 *KoankoArceusGetBreakScript(u32 battler, u32 layer)
{
    if (layer >= BREAK_BAR_MAX_EXTRA_BARS || !KoankoArceusOwnsBattler(battler)
     || gBattleStruct->koankoArceus.phase != KOANKO_ARCEUS_COVERED
     || layer != gBattleStruct->koankoArceus.savedLayer
     || GetBattlerPartyState(battler)->breakBarSavedMaxHp[layer] == 0)
        return NULL;
    return BattleScript_KoankoArceusAdapt;
}

void KoankoArceusCommitBreak(u32 battler)
{
    // Called only after the shared resolver has reserved its complete stack.
    if (KoankoArceusOwnsBattler(battler))
        gBattleStruct->koankoArceus.phase = KOANKO_ARCEUS_ADAPT_PENDING;
}

enum Species KoankoArceusGetForm(u32 battler)
{
    if (!KoankoArceusOwnsBattler(battler)
     || gBattleStruct->koankoArceus.phase != KOANKO_ARCEUS_ADAPTED)
        return SPECIES_NONE;
    return gBattleStruct->koankoArceus.formSpecies;
}

void BS_KoankoArceusAdapt(void)
{
    NATIVE_ARGS(const u8 *failInstr, const u8 *skipAnimationInstr);
    u32 battler = gBattlerTarget; // The shared bar resolver binds its owner.
    if (!KoankoArceusOwnsBattler(battler) || gBattleOutcome != 0
     || gBattleStruct->koankoArceus.phase != KOANKO_ARCEUS_ADAPT_PENDING
     || gBattleMons[battler].hp == 0)
    {
        gBattlescriptCurrInstr = cmd->failInstr;
        return;
    }
    struct KoankoArceusState *state = &gBattleStruct->koankoArceus;
    state->savedScriptingBattler = gBattleScripting.battler;
    state->phase = KOANKO_ARCEUS_ADAPTING;
    enum Type foeTypes[3];
    u32 foe = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
    GetBattlerTypes(foe, FALSE, foeTypes);
    enum Species species = GetMonData(GetBattlerMon(battler), MON_DATA_SPECIES);
    KoankoChooseArceusResistantForm(state->breakType, species, foeTypes, &species);
    state->formSpecies = species;
    // All ordinary Arceus forms have identical base stats. Only species and
    // typing change; no recalc, heal, move/item/ability copy or gimmick activation.
    SetMonData(GetBattlerMon(battler), MON_DATA_SPECIES, &species);
    gBattleMons[battler].species = species;
    gBattleMons[battler].types[0] = GetSpeciesType(species, 0);
    gBattleMons[battler].types[1] = GetSpeciesType(species, 1);
    gBattleMons[battler].types[2] = TYPE_MYSTERY;
    gBattleScripting.battler = battler;
    state->phase = KOANKO_ARCEUS_ADAPTED; // Charged effect is consumed before yielding.
    // The shared mosaic uses a controller waiter, mon-BG tasks, delayed sound,
    // transform and BG cleanup. Avoid the task allocator's ID-0 fallback when
    // resources are scarce; the reveal native still installs the real art.
    u32 freeTasks = 0;
    for (u32 task = 0; task < NUM_TASKS; task++)
        if (!gTasks[task].isActive)
            freeTasks++;
    u32 sprite = gBattlerSpriteIds[battler];
    bool32 canAnimate = freeTasks >= 5 && sprite < MAX_SPRITES && gSprites[sprite].inUse
        && !gBattleSpritesDataPtr->battlerData[battler].behindSubstitute;
    gBattlescriptCurrInstr = canAnimate ? cmd->nextInstr : cmd->skipAnimationInstr;
}

void BS_KoankoArceusFinishAdapt(void)
{
    NATIVE_ARGS();
    u32 battler = gBattleStruct->koankoArceus.battler;
    u32 sprite = battler < gBattlersCount ? gBattlerSpriteIds[battler] : MAX_SPRITES;
    // Also covers a skipped/headless animation. Do not paint a mon over its
    // Substitute; the ordinary substitute fade will reload canonical art.
    if (KoankoArceusOwnsBattler(battler) && sprite < MAX_SPRITES && gSprites[sprite].inUse
     && !gBattleSpritesDataPtr->battlerData[battler].behindSubstitute)
        HandleSpeciesGfxDataChange(battler, battler, SPECIES_GFX_CHANGE_FORM_CHANGE);
    gBattleScripting.battler = gBattleStruct->koankoArceus.savedScriptingBattler;
    gBattlescriptCurrInstr = cmd->nextInstr;
}

void KoankoArceusFinishBattle(void)
{
    if (gBattleStruct == NULL || gBattleStruct->koankoArceus.phase == KOANKO_ARCEUS_DORMANT)
        return;
    ClearArceusBugSpace(gBattleStruct->koankoArceus.battler);
    RestoreArceusForm();
    memset(&gBattleStruct->koankoArceus, 0, sizeof(gBattleStruct->koankoArceus));
}

void KoankoArceusOnFaint(u32 battler)
{
    if (ArceusIdentityMatches(battler) && gBattleMons[battler].hp == 0)
    {
        ClearArceusBugSpace(battler);
        RestoreArceusForm();
        gBattleStruct->koankoArceus.phase = KOANKO_ARCEUS_FINISHED;
    }
}

s32 KoankoArceusClampHpLoss(u32 battler, s32 damage)
{
    if (damage <= 0 || !KoankoArceusIsProtected(battler))
        return damage;
    return min(damage, max(0, (s32)gBattleMons[battler].hp - 1));
}

void KoankoArceusClampMoveDamage(u32 battler)
{
    if (!KoankoArceusIsProtected(battler))
        return;
    s32 damage = gBattleStruct->moveDamage[battler];
    gBattleStruct->moveDamage[battler] = KoankoArceusClampHpLoss(battler, damage);
    if (damage != gBattleStruct->moveDamage[battler])
    {
        gBattleStruct->moveResultFlags[battler] &= ~MOVE_RESULT_ONE_HIT_KO;
        gBattleStruct->trashCrushOhko &= ~(1u << battler);
    }
}

void KoankoArceusEmitHpDrop(u32 battler)
{
    s32 damage = KoankoArceusIsProtected(battler)
        ? KoankoArceusClampHpLoss(battler, gBattleMons[battler].hp) : INSTANT_HP_BAR_DROP;
    if (damage != 0)
    {
        BtlController_EmitHealthBarUpdate(battler, B_COMM_TO_CONTROLLER, damage);
        MarkBattlerForControllerExec(battler);
    }
}

bool32 KoankoArceusRepairFaint(u32 battler)
{
    if (!KoankoArceusIsProtected(battler) || gBattleMons[battler].hp != 0)
        return FALSE;
    // Last defense for a direct-zero writer. No healing command/benefit runs,
    // and neither a layer nor a genuine faint may be recorded for this zero.
    gBattleMons[battler].hp = 1;
    GetBattlerPartyState(battler)->breakBarPending = FALSE;
    gHitMarker &= ~HITMARKER_FAINTED(battler);
    SetMonData(GetBattlerMon(battler), MON_DATA_HP, &gBattleMons[battler].hp);
    gBattleSpritesDataPtr->battleBars[battler].currValue = 1;
    BtlController_EmitHealthBarUpdate(battler, B_COMM_TO_CONTROLLER, INSTANT_HP_BAR_SNAP);
    MarkBattlerForControllerExec(battler);
    return TRUE;
}

// Only genuine, existing forms. Matchups come from GetTypeModifier, including
// inverse battles; there is no separate boss-only resistance chart or form.
static const struct
{
    enum Type type;
    enum Species species;
} sArceusForms[] =
{
    {TYPE_NORMAL, SPECIES_ARCEUS_NORMAL},
    {TYPE_FIGHTING, SPECIES_ARCEUS_FIGHTING},
    {TYPE_FLYING, SPECIES_ARCEUS_FLYING},
    {TYPE_POISON, SPECIES_ARCEUS_POISON},
    {TYPE_GROUND, SPECIES_ARCEUS_GROUND},
    {TYPE_ROCK, SPECIES_ARCEUS_ROCK},
    {TYPE_BUG, SPECIES_ARCEUS_BUG},
    {TYPE_GHOST, SPECIES_ARCEUS_GHOST},
    {TYPE_STEEL, SPECIES_ARCEUS_STEEL},
    {TYPE_FIRE, SPECIES_ARCEUS_FIRE},
    {TYPE_WATER, SPECIES_ARCEUS_WATER},
    {TYPE_GRASS, SPECIES_ARCEUS_GRASS},
    {TYPE_ELECTRIC, SPECIES_ARCEUS_ELECTRIC},
    {TYPE_PSYCHIC, SPECIES_ARCEUS_PSYCHIC},
    {TYPE_ICE, SPECIES_ARCEUS_ICE},
    {TYPE_DRAGON, SPECIES_ARCEUS_DRAGON},
    {TYPE_DARK, SPECIES_ARCEUS_DARK},
    {TYPE_FAIRY, SPECIES_ARCEUS_FAIRY},
};

static uq4_12_t JudgmentMatchup(enum Type attackType, const enum Type foeTypes[3])
{
    uq4_12_t multiplier = UQ_4_12(1.0);
    if (foeTypes == NULL)
        return multiplier;
    for (u32 i = 0; i < 3; i++)
    {
        enum Type type = foeTypes[i];
        if (type == TYPE_NONE || type == TYPE_MYSTERY || type >= NUMBER_OF_MON_TYPES)
            continue;
        if ((i > 0 && type == foeTypes[0]) || (i > 1 && type == foeTypes[1]))
            continue;
        multiplier = uq4_12_multiply(multiplier, GetTypeModifier(attackType, type));
    }
    return multiplier;
}

bool32 KoankoChooseArceusResistantForm(enum Type attackType, enum Species currentSpecies,
                                     const enum Type foeTypes[3], enum Species *chosenSpecies)
{
    if (chosenSpecies == NULL)
        return FALSE;
    *chosenSpecies = currentSpecies;
    if (attackType == TYPE_NONE || attackType == TYPE_MYSTERY || attackType >= NUMBER_OF_MON_TYPES)
        return FALSE;

    u32 count = 0;
    uq4_12_t bestDefense = UQ_4_12(1.0);
    uq4_12_t bestOffense = UQ_4_12(0.0);
    bool32 validCurrent = FALSE;
    for (u32 i = 0; i < ARRAY_COUNT(sArceusForms); i++)
    {
        validCurrent |= sArceusForms[i].species == currentSpecies;
        uq4_12_t defense = GetTypeModifier(attackType, sArceusForms[i].type);
        if (defense >= UQ_4_12(1.0))
            continue;
        uq4_12_t offense = JudgmentMatchup(sArceusForms[i].type, foeTypes);
        // Defense wins absolutely: immunity, then any chart-provided quarter
        // resistance, then ordinary resistance. Offense only breaks equals.
        if (defense < bestDefense || (defense == bestDefense && offense > bestOffense))
        {
            bestDefense = defense;
            bestOffense = offense;
            count = 1;
        }
        else if (defense == bestDefense && offense == bestOffense)
            count++;
    }
    if (!validCurrent || count == 0)
        return FALSE;

    // Two bounded passes avoid both rejection sampling and a mutable candidate
    // buffer (including an automatic array on the IWRAM stack). One uniform
    // rank gives each equally best-ranked form one equally likely outcome.
    u32 rank = RandomUniform(RNG_KOANKO_ARCEUS_FORM, 0, count - 1);
    for (u32 i = 0; i < ARRAY_COUNT(sArceusForms); i++)
    {
        if (GetTypeModifier(attackType, sArceusForms[i].type) != bestDefense
         || JudgmentMatchup(sArceusForms[i].type, foeTypes) != bestOffense)
            continue;
        if (rank == 0)
        {
            *chosenSpecies = sArceusForms[i].species;
            return TRUE;
        }
        rank--;
    }
    return FALSE;
}
