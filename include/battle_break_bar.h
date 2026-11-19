#ifndef GUARD_BATTLE_BREAK_BAR_H
#define GUARD_BATTLE_BREAK_BAR_H

#include "global.h"
#include "constants/battle_break_bar.h"

// Red is the base HP bar. Extra bars are stacked above it in this order.
enum BreakBarColor
{
    BREAK_BAR_RED,
    BREAK_BAR_PURPLE,
    BREAK_BAR_BLUE,
    BREAK_BAR_GREEN,
    BREAK_BAR_YELLOW,
    BREAK_BAR_ORANGE,
};

#define BREAK_BAR_MAX_EXTRA_BARS 5
#define BREAK_BAR_IGNORE_WHILE_TRANSFORMED (1 << 0)
#define BREAK_BAR_DEFER_INITIAL_DRAW (1 << 1)
// The config is registered on its owner with no active extra bars. The phase table stays attached for
// the whole battle; GrantConfiguredBreakBar() raises the active count one configured phase at a time.
#define BREAK_BAR_START_INACTIVE (1 << 2)
// Visual-only, once-per-owner green base then successive covering fills before switch-in effects.
// Deferred-draw profiles retain ownership of their own presentation.
#define BREAK_BAR_INTRO_FILLS (1 << 3)

// GrantConfiguredBreakBar() flags.
// Cover the current HP/max HP with the new layer (saved like GrantSavedBreakBar) instead of making the
// current HP bar the layer. When a covering layer breaks the covered pair is restored exactly, then the
// layer's configured phase scripts run: restore -> onBreakScript -> afterBreakScript. Unlike a configured
// bar, a NULL onBreakScript means "nothing" here (never a refill, which would heal the covered HP).
#define BREAK_BAR_GRANT_COVER (1 << 0)

struct BreakBarPhase
{
    // Both scripts must return. NULL onBreakScript uses the standard HP refill.
    const u8 *onBreakScript;
    const u8 *afterBreakScript;
};

struct BreakBarConfig
{
    struct BreakBarPhase phases[BREAK_BAR_MAX_EXTRA_BARS];
    u8 extraBars;
    u8 flags;
};

static inline u32 GetInitialBreakBarCount(const struct BreakBarConfig *config)
{
    return (config->flags & BREAK_BAR_START_INACTIVE) ? 0 : config->extraBars;
}

// Registers the current HP bar; it does not heal or change max HP.
// Once all extra bars are broken, callers may explicitly activate a new stack.
// BREAK_BAR_DEFER_INITIAL_DRAW lets the caller provide its own entry animation.
bool32 ActivateBreakBar(u32 battler, const struct BreakBarConfig *config);
void InitializeWildBreakBar(u32 partySlot, const struct BreakBarConfig *config);
u32 GetBreakBarCount(u32 battler);
bool32 IsBreakBarCaptureBlocked(u32 battler);
bool32 IsBreakBarBase(u32 battler);
bool32 CanInterceptBreakBar(u32 battler);
bool32 TryBreakBar(u32 battler, const u8 *returnInstr);
bool32 QueueBreakBar(u32 battler);
s32 ClampBreakBarPassiveDamage(u32 battler, s32 damage);
bool32 GrantSavedBreakBar(u32 battler);
// Raises the active count by one using the owner's own configured phase table (phases[activeCount]).
// Fails unless the owner has a config with an unused phase, is alive and has no pending break.
// The checks GrantConfiguredBreakBar makes, with no effects (lets a script heal first only if the grant can happen).
bool32 CanGrantConfiguredBreakBar(u32 battler);
enum BreakBarGrantResult GetConfiguredBreakBarGrantResult(u32 battler);
bool32 GrantConfiguredBreakBar(u32 battler, u32 flags);
bool32 AnimateConfiguredBreakBar(u32 battler);
bool32 TryResolveBreakBarsAfterMove(void);

void RefreshBreakBarPalette(u32 battler, bool32 animate);
// Highlight/body colors for a party owner's current layer; NULL means ordinary HP colors.
struct PartyState;
const u16 *GetBreakBarFillColors(const struct PartyState *owner);
bool32 DrawBreakBarIntroBase(u32 battler);
bool32 TryShowBreakBarIntro(void);
bool32 AnimateBreakBarIntro(u32 battler);
void BS_AnimateBreakBarIntro(void);
void ReshowBreakBars(void);
void FreeBreakBarGraphics(void);
void BS_RefillBreakBar(void);
void BS_ResolveBreakBars(void);
void BS_StartKazuraStackBreaker(void);
void BS_AnimateKazuraStackBreaker(void);
void BS_ExhaustBreakBars(void);
void BS_TryGrantSavedBreakBar(void);
void BS_GrantConfiguredBreakBar(void);
void BS_CanGrantConfiguredBreakBar(void);
void BS_GetConfiguredBreakBarGrantResult(void);
void BS_AnimateConfiguredBreakBar(void);
void BS_StartBreakBarIntro(void);
void BS_ContinueBreakBarIntro(void);
void BS_FillSavedBreakBar(void);
void BS_AnimateSavedBreakBar(void);
void BS_RestoreSavedBreakBar(void);
void BS_SyncSavedBreakBarHp(void);
void BS_RevealSavedBreakBar(void);

#endif // GUARD_BATTLE_BREAK_BAR_H
