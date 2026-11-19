#include "global.h"
#include "battle_script_native.h"
#include "battle.h"
#include "battle_break_bar.h"
#include "battle_controllers.h"
#include "battle_interface.h"
#include "battle_script_commands.h"
#include "battle_scripts.h"
#include "graphics.h"
#include "palette.h"
#include "pokemon.h"
#include "sprite.h"
#include "task.h"
#include "constants/rgb.h"

#define BREAK_BAR_FADE_FRAMES 16
// Retained for possible future reuse. Entry/grant presentation must not lerp
// from the ordinary green palette; keep this code compiled out.
#define BREAK_BAR_ENABLE_LEGACY_LERP 0
#define BREAK_BAR_GRANT_PIXELS_PER_FRAME 3
#define BREAK_BAR_INTRO_BASE_FRAMES 12
#define hMain_HealthBarSpriteId data[5]
#define hMain_Battler data[6]

static const u16 sBreakBarBasePalette[] =
    INCBIN_U16("graphics/battle_interface/hpbar_purple.gbapal");
STATIC_ASSERT(ARRAY_COUNT(sBreakBarBasePalette) == 16, BreakBarPaletteSize);

// FGO gauge-marker highlight/body colors, quantized to GBA RGB.
// Only the two green HP-fill entries are replaced; other healthbar colors remain.
static const u16 sBreakBarFillColors[][2] =
{
    [BREAK_BAR_PURPLE] = {RGB(22,  5, 26), RGB(11,  3, 20)},
    [BREAK_BAR_BLUE]   = {RGB( 5, 17, 27), RGB( 3,  9, 27)},
    [BREAK_BAR_GREEN]  = {RGB( 4, 25, 22), RGB( 1, 13, 11)},
    [BREAK_BAR_YELLOW] = {RGB(27, 23,  4), RGB(17, 15,  2)},
    [BREAK_BAR_ORANGE] = {RGB(25, 14, 17), RGB(27, 12, 11)},
};
STATIC_ASSERT(ARRAY_COUNT(sBreakBarFillColors) == BREAK_BAR_MAX_EXTRA_BARS + 1, BreakBarColors);

// Separate slots allow differently colored stacks to coexist in doubles.
static const u16 sBreakBarPaletteTags[MAX_BATTLERS_COUNT] =
{
    TAG_BREAK_BAR_PLAYER_LEFT_PAL,
    TAG_BREAK_BAR_OPPONENT_LEFT_PAL,
    TAG_BREAK_BAR_PLAYER_RIGHT_PAL,
    TAG_BREAK_BAR_OPPONENT_RIGHT_PAL,
};

static u8 sLoadedBreakBarColors[MAX_BATTLERS_COUNT];
static u32 GetHealthbarSprite(u32 battler);

// Presentation state is separate from real HP, saved-layer fills and counts.
static struct BreakBarFillAnimation
{
    struct PartyState *owner;
    const struct BreakBarConfig *config;
    u16 hp, maxHp;
    u8 bars, color, pixels;
    u8 healthbar, baseFrames;
    bool8 active, preparing;
} sBreakBarIntros[MAX_BATTLERS_COUNT], sBreakBarGrantFills[MAX_BATTLERS_COUNT];
static void StartBreakBarFill(u32 battler, struct BreakBarFillAnimation *fill, u32 color);
#if BREAK_BAR_ENABLE_LEGACY_LERP
static void Task_BreakBarPaletteFade(u8 taskId);
static bool32 HasFreeBreakBarFadeTask(void)
{
    for (u32 taskId = 0; taskId < NUM_TASKS; taskId++)
        if (!gTasks[taskId].isActive)
            return TRUE;
    return FALSE;
}
#endif
static bool8 sKazuraStackBreakerActive[MAX_BATTLERS_COUNT];
static u8 sKazuraStackBreakerPixels[MAX_BATTLERS_COUNT];

static const struct BreakBarConfig sSavedBreakBarConfig =
{
    .extraBars = 1,
    .flags = BREAK_BAR_DEFER_INITIAL_DRAW,
};

static struct PartyState *GetBreakBarOwner(u32 battler)
{
    if (gBattleStruct == NULL || battler >= gBattlersCount
     || gBattlerPartyIndexes[battler] >= PARTY_SIZE)
        return NULL;
    return GetBattlerPartyState(battler);
}

u32 GetBreakBarCount(u32 battler)
{
    struct PartyState *owner = GetBreakBarOwner(battler);
    if (owner == NULL || owner->breakBarConfig == NULL)
        return 0;
    return owner->breakBarsRemaining;
}

const u16 *GetBreakBarFillColors(const struct PartyState *owner)
{
    if (owner == NULL || owner->breakBarConfig == NULL)
        return NULL;
    if (owner->breakBarsRemaining != 0)
        return sBreakBarFillColors[owner->breakBarsRemaining];
    if ((owner->breakBarConfig->flags & BREAK_BAR_START_INACTIVE) && owner->breakBarsPeak == 0)
        return NULL;
    return &sBreakBarBasePalette[14];
}

void InitializeWildBreakBar(u32 partySlot, const struct BreakBarConfig *config)
{
    if (gBattleStruct == NULL || partySlot >= PARTY_SIZE || config == NULL
     || config->extraBars == 0 || config->extraBars > BREAK_BAR_MAX_EXTRA_BARS)
        return;
    struct PartyState *state = &gBattleStruct->partyState[B_SIDE_OPPONENT][partySlot];
    state->breakBarConfig = config;
    state->breakBarsRemaining = GetInitialBreakBarCount(config);
}

bool32 IsBreakBarCaptureBlocked(u32 battler)
{
    struct PartyState *owner = GetBreakBarOwner(battler);
    return owner != NULL && (GetBreakBarCount(battler) != 0 || owner->breakBarPending);
}

bool32 IsBreakBarBase(u32 battler)
{
    struct PartyState *owner = GetBreakBarOwner(battler);
    // A START_INACTIVE config has not become a base until one of its bars has been active.
    return owner != NULL && owner->breakBarConfig != NULL
        && owner->breakBarsRemaining == 0
        && !((owner->breakBarConfig->flags & BREAK_BAR_START_INACTIVE) && owner->breakBarsPeak == 0);
}

// Bit n is only meaningful while saved layer n is active; drop the bits of layers that are gone.
static void TrimCoverScripts(struct PartyState *owner)
{
    owner->breakBarCoverScripts &= (1u << owner->breakBarsRemaining) - 1;
}

bool32 CanInterceptBreakBar(u32 battler)
{
    struct PartyState *owner = GetBreakBarOwner(battler);
    return owner != NULL && owner->breakBarConfig != NULL && owner->breakBarsRemaining != 0
        && !(gAbsentBattlerFlags & (1u << battler))
        && !((owner->breakBarConfig->flags & BREAK_BAR_IGNORE_WHILE_TRANSFORMED)
          && gBattleMons[battler].volatiles.transformed);
}

bool32 QueueBreakBar(u32 battler)
{
    if (!CanInterceptBreakBar(battler) || gBattleMons[battler].hp != 0)
        return FALSE;
    GetBattlerPartyState(battler)->breakBarPending = TRUE;
    return TRUE;
}

s32 ClampBreakBarPassiveDamage(u32 battler, s32 damage)
{
    if (IsBreakBarPending(battler))
        return 0; // An empty layer cannot be damaged or healed before its transition.
    if (damage <= 0 || !CanInterceptBreakBar(battler))
        return damage;
    return min(damage, max(0, (s32)gBattleMons[battler].hp - 1));
}

bool32 GrantSavedBreakBar(u32 battler)
{
    struct PartyState *owner = GetBreakBarOwner(battler);
    if (owner == NULL || !IsBattlerAlive(battler) || IsBreakBarPending(battler)
     || owner->breakBarsRemaining >= BREAK_BAR_MAX_EXTRA_BARS)
        return FALSE;
    u32 layer = owner->breakBarsRemaining;
    if (layer == 0)
        owner->breakBarSavedBaseWasRegistered = IsBreakBarBase(battler);
    owner->breakBarSavedHp[layer] = ReductionCanonicalHp(battler);
    owner->breakBarSavedMaxHp[layer] = ReductionCanonicalMaxHp(battler);
    owner->breakBarFillPixels = 0;
    owner->breakBarCoverScripts &= ~(1u << layer); // An ordinary saved layer never runs phase scripts.
    if (owner->breakBarConfig == NULL)
        owner->breakBarConfig = &sSavedBreakBarConfig;
    owner->breakBarsRemaining++;
    owner->breakBarsPeak = max(owner->breakBarsPeak, owner->breakBarsRemaining);
    return TRUE;
}

enum BreakBarGrantResult GetConfiguredBreakBarGrantResult(u32 battler)
{
    struct PartyState *owner = GetBreakBarOwner(battler);
    if (owner == NULL)
        return BREAK_BAR_GRANT_NO_OWNER;
    if (owner->breakBarConfig == NULL)
        return BREAK_BAR_GRANT_NO_CONFIG;
    if (owner->breakBarConfig->extraBars == 0 || owner->breakBarConfig->extraBars > BREAK_BAR_MAX_EXTRA_BARS)
        return BREAK_BAR_GRANT_INVALID_CONFIG;
    if (owner->breakBarPending)
        return BREAK_BAR_GRANT_PENDING;
    if (!IsBattlerAlive(battler))
        return BREAK_BAR_GRANT_NOT_ALIVE;
    if (owner->breakBarsRemaining >= owner->breakBarConfig->extraBars)
        return BREAK_BAR_GRANT_FULL;
    return BREAK_BAR_GRANT_READY;
}

bool32 CanGrantConfiguredBreakBar(u32 battler)
{
    return GetConfiguredBreakBarGrantResult(battler) == BREAK_BAR_GRANT_READY;
}

bool32 GrantConfiguredBreakBar(u32 battler, u32 flags)
{
    enum BreakBarGrantResult result = GetConfiguredBreakBarGrantResult(battler);
    if (result != BREAK_BAR_GRANT_READY)
    {
#ifndef RELEASE
        static const char *const reasons[] =
        {
            [BREAK_BAR_GRANT_NO_OWNER] = "no owner",
            [BREAK_BAR_GRANT_NO_CONFIG] = "no configuration",
            [BREAK_BAR_GRANT_INVALID_CONFIG] = "invalid configuration",
            [BREAK_BAR_GRANT_PENDING] = "pending break",
            [BREAK_BAR_GRANT_NOT_ALIVE] = "dead or absent owner",
            [BREAK_BAR_GRANT_FULL] = "configured capacity full",
        };
        DebugPrintf("BREAK_BAR_GRANT_REJECT battler=%u reason=%s", battler, reasons[result]);
#endif
        return FALSE;
    }
    struct PartyState *owner = GetBreakBarOwner(battler);

    // phases[] is indexed by stack position, so the next configured phase is the next free position.
    // A position already taken (for example by an ordinary saved layer) cannot be granted over.
    u32 layer = owner->breakBarsRemaining;
    if (flags & BREAK_BAR_GRANT_COVER)
    {
        if (layer == 0)
            owner->breakBarSavedBaseWasRegistered = IsBreakBarBase(battler);
        owner->breakBarSavedHp[layer] = ReductionCanonicalHp(battler);
        owner->breakBarSavedMaxHp[layer] = ReductionCanonicalMaxHp(battler);
        owner->breakBarCoverScripts |= 1u << layer;
    }
    owner->breakBarFillPixels = 0;
    owner->breakBarsRemaining++;
    owner->breakBarsPeak = max(owner->breakBarsPeak, owner->breakBarsRemaining);
    sBreakBarGrantFills[battler].active = FALSE;
    // Every grant script waits for a fill. Plain layers use the visual-only
    // fraction; covering layers use the existing explicit HP/saved-fill natives.
    if (!(flags & BREAK_BAR_GRANT_COVER))
    {
        RefreshBreakBarPalette(battler, FALSE);
        if (GetHealthbarSprite(battler) != MAX_SPRITES)
            UpdateHealthboxAttribute(gHealthboxSpriteIds[battler], GetBattlerMon(battler), HEALTHBOX_HEALTH_BAR);
        StartBreakBarFill(battler, &sBreakBarGrantFills[battler], owner->breakBarsRemaining);
    }
    return TRUE;
}

static u32 GetHealthbarSprite(u32 battler)
{
    if (battler >= gBattlersCount)
        return MAX_SPRITES;
    u32 healthbox = gHealthboxSpriteIds[battler];
    if (healthbox >= MAX_SPRITES || !gSprites[healthbox].inUse
     || gSprites[healthbox].hMain_Battler != battler)
        return MAX_SPRITES;
    u32 healthbar = gSprites[healthbox].hMain_HealthBarSpriteId;
    if (healthbar >= MAX_SPRITES || !gSprites[healthbar].inUse)
        return MAX_SPRITES;
    return healthbar;
}

static void StopBreakBarFade(u32 battler UNUSED)
{
#if BREAK_BAR_ENABLE_LEGACY_LERP
    for (u32 taskId = 0; taskId < NUM_TASKS; taskId++)
    {
        if (gTasks[taskId].isActive && gTasks[taskId].func == Task_BreakBarPaletteFade
         && gTasks[taskId].data[0] == battler)
            DestroyTask(taskId);
    }
#endif
}

static void BuildBreakBarPalette(u32 color, u16 *palette)
{
    memcpy(palette, sBreakBarBasePalette, sizeof(sBreakBarBasePalette));
    palette[10] = sBreakBarFillColors[color][0];
    palette[11] = sBreakBarFillColors[color][1];
}

static void SetBreakBarPalette(u32 battler, u32 color, bool32 animate UNUSED, const u16 *paletteOverride)
{
    if (battler >= gBattlersCount || color > BREAK_BAR_MAX_EXTRA_BARS)
        return;
    // Ordinary battlers need no palette lookup or task scan on each HP-bar frame.
    if (color == 0 && sLoadedBreakBarColors[battler] == 0)
        return;
    u32 healthbar = GetHealthbarSprite(battler);
    if (healthbar == MAX_SPRITES)
        return;

    u16 tag = sBreakBarPaletteTags[battler];
    u32 slot = IndexOfSpritePaletteTag(tag);
    if (color == 0)
    {
        StopBreakBarFade(battler);
        u32 normalSlot = IndexOfSpritePaletteTag(TAG_HEALTHBAR_PAL);
        if (normalSlot != 0xFF)
            gSprites[healthbar].oam.paletteNum = normalSlot;
        FreeSpritePaletteByTag(tag);
        sLoadedBreakBarColors[battler] = 0;
        return;
    }

    u16 palette[16];
    if (slot == 0xFF || sLoadedBreakBarColors[battler] != color || paletteOverride != NULL)
    {
        StopBreakBarFade(battler);
        if (paletteOverride != NULL)
            memcpy(palette, paletteOverride, sizeof(palette));
        else
            BuildBreakBarPalette(color, palette);
        if (slot == 0xFF)
        {
            const struct SpritePalette spritePalette = {palette, tag};
            slot = LoadSpritePalette(&spritePalette);
            if (slot == 0xFF)
                return;
        }
        else
            LoadPalette(palette, OBJ_PLTT_ID(slot), PLTT_SIZE_4BPP);
        // A composed intro palette must not be mistaken for the ordinary
        // cached palette when completion/cancellation restores real HP.
        sLoadedBreakBarColors[battler] = paletteOverride != NULL ? 0xFF : color;

#if BREAK_BAR_ENABLE_LEGACY_LERP
        // CreateTask returns 0 on exhaustion, so check capacity before using its ID.
        if (animate && HasFreeBreakBarFadeTask())
        {
            LoadPalette(gBattleInterface_BallDisplayPal, OBJ_PLTT_ID(slot), PLTT_SIZE_4BPP);
            u32 taskId = CreateTask(Task_BreakBarPaletteFade, 0);
            gTasks[taskId].data[0] = battler;
            gTasks[taskId].data[1] = color;
        }
#endif
    }
    gSprites[healthbar].oam.paletteNum = slot;
}

static void SetBreakBarIntroPalette(u32 battler, u32 color, const u16 *topColors, const u16 *underlyingColors)
{
    u16 palette[16];
    BuildBreakBarPalette(color, palette);
    if (topColors != NULL)
        memcpy(&palette[10], topColors, 2 * sizeof(u16));
    memcpy(&palette[14], underlyingColors, 2 * sizeof(u16));
    // Load the complete composition at once; never expose a temporary red base.
    SetBreakBarPalette(battler, color, FALSE, palette);
}

static bool32 CanPresentBreakBarIntro(u32 battler)
{
    struct PartyState *owner = GetBreakBarOwner(battler);
    return owner != NULL && owner->breakBarConfig != NULL
        && (owner->breakBarConfig->flags & BREAK_BAR_INTRO_FILLS)
        && !(owner->breakBarConfig->flags & BREAK_BAR_DEFER_INITIAL_DRAW)
        && owner->breakBarsRemaining != 0
        && owner->breakBarsRemaining <= BREAK_BAR_MAX_EXTRA_BARS
        && !owner->breakBarPending && gBattleMons[battler].hp != 0
        && gBattleMons[battler].maxHP != 0
        && !(gAbsentBattlerFlags & (1u << battler));
}

static bool32 ShouldDrawBreakBarIntroBase(u32 battler)
{
    return CanPresentBreakBarIntro(battler)
        && !(GetBreakBarOwner(battler)->breakBarConfig->flags & BREAK_BAR_START_INACTIVE)
        && !GetBreakBarOwner(battler)->breakBarIntroShown;
}

void RefreshBreakBarPalette(u32 battler, bool32 animate)
{
    if (ShouldDrawBreakBarIntroBase(battler))
        SetBreakBarIntroPalette(battler, BREAK_BAR_PURPLE,
            &gBattleInterface_BallDisplayPal[10], &gBattleInterface_BallDisplayPal[10]);
    else
        SetBreakBarPalette(battler, GetBreakBarCount(battler), animate, NULL);
}

bool32 DrawBreakBarIntroBase(u32 battler)
{
    if (!ShouldDrawBreakBarIntroBase(battler))
        return FALSE;
    // Keep ordinary green indices until startup has yielded for VBlank. Both
    // the old shared slot and this private slot map these tiles to green.
    return DrawHealthbarOverlay(battler, 0, 1, B_HEALTHBAR_PIXELS);
}

bool32 TryShowBreakBarIntro(void)
{
    if (gBattleResources->battleScriptsStack->size
        >= ARRAY_COUNT(gBattleResources->battleScriptsStack->ptr))
        return FALSE;
    for (u32 battler = 0; battler < gBattlersCount; battler++)
    {
        if (gBattleStruct->battlerState[battler].switchIn && CanPresentBreakBarIntro(battler)
         && !GetBreakBarOwner(battler)->breakBarIntroShown)
        {
            gBattleScripting.battler = battler;
            BattleScriptCall(BattleScript_BreakBarIntro);
            return TRUE;
        }
    }
    return FALSE;
}

static void StartBreakBarFill(u32 battler, struct BreakBarFillAnimation *fill, u32 color)
{
    struct PartyState *owner = GetBreakBarOwner(battler);
    StopBreakBarFade(battler);
    fill->owner = owner;
    fill->config = owner->breakBarConfig;
    fill->hp = gBattleMons[battler].hp;
    fill->maxHp = gBattleMons[battler].maxHP;
    fill->bars = owner->breakBarsRemaining;
    fill->color = color;
    fill->pixels = 0;
    fill->baseFrames = fill == &sBreakBarIntros[battler] ? BREAK_BAR_INTRO_BASE_FRAMES : 0;
    fill->preparing = fill == &sBreakBarIntros[battler];
    fill->healthbar = GetHealthbarSprite(battler);
    fill->active = TRUE;
}

static void FinishBreakBarFill(u32 battler, struct BreakBarFillAnimation *fill)
{
    fill->active = FALSE;
    if (fill == &sBreakBarIntros[battler])
    {
        u32 slot = IndexOfSpritePaletteTag(sBreakBarPaletteTags[battler]);
        if (slot != 0xFF)
            LoadPalette(&sBreakBarBasePalette[14], OBJ_PLTT_ID(slot) + 14, 2 * sizeof(u16));
    }
    RefreshBreakBarPalette(battler, FALSE);
    if (GetBreakBarOwner(battler) != NULL && GetHealthbarSprite(battler) != MAX_SPRITES)
        UpdateHealthboxAttribute(gHealthboxSpriteIds[battler], GetBattlerMon(battler), HEALTHBOX_HEALTH_BAR);
}

// TRUE keeps the caller on this frame command; FALSE completes/cancels it.
static bool32 AnimateBreakBarFill(u32 battler, struct BreakBarFillAnimation *fill)
{
    if (!fill->active)
        return FALSE;
    if (battler >= gBattlersCount || gBattleStruct == NULL)
    {
        fill->active = FALSE;
        return FALSE;
    }
    if (gBattleControllerExecFlags)
        return TRUE;

    struct PartyState *owner = GetBreakBarOwner(battler);
    // Never paint a replacement owner, stale healthbox or a changing gameplay bar.
    if (owner != fill->owner
     || owner->breakBarConfig != fill->config
     || owner->breakBarsRemaining != fill->bars
     || owner->breakBarPending || gBattleMons[battler].hp == 0
     || gBattleMons[battler].hp != fill->hp
     || gBattleMons[battler].maxHP != fill->maxHp || fill->maxHp == 0
     || (gAbsentBattlerFlags & (1u << battler))
     || GetHealthbarSprite(battler) == MAX_SPRITES
     || GetHealthbarSprite(battler) != fill->healthbar)
    {
        FinishBreakBarFill(battler, fill);
        return FALSE;
    }

    u32 color = fill->color;
    u32 target = B_HEALTHBAR_PIXELS;
    if (color == fill->bars)
        target = max(1, min((u32)fill->hp * B_HEALTHBAR_PIXELS / fill->maxHp, B_HEALTHBAR_PIXELS));
    u32 pixels = min(fill->pixels, target);
    bool32 intro = fill == &sBreakBarIntros[battler];
    if (fill->preparing)
    {
        u32 oldSlot = OBJ_PLTT_ID(gSprites[fill->healthbar].oam.paletteNum);
        // Explicit reveals may start over a colored bar. Preserve its visible
        // top while staging green in the hidden pair, then yield for VBlank
        // before changing any tile indices or the top color.
        SetBreakBarIntroPalette(battler, color, &gPlttBufferUnfaded[oldSlot + 10],
            &gBattleInterface_BallDisplayPal[10]);
        if (IndexOfSpritePaletteTag(sBreakBarPaletteTags[battler]) == 0xFF)
        {
            FinishBreakBarFill(battler, fill);
            return FALSE;
        }
        fill->preparing = FALSE;
        return TRUE;
    }
    if (intro)
        SetBreakBarIntroPalette(battler, color, NULL, color == BREAK_BAR_PURPLE
            ? &gBattleInterface_BallDisplayPal[10] : sBreakBarFillColors[color - 1]);
    else
        SetBreakBarPalette(battler, color, FALSE, NULL);
    u32 slot = IndexOfSpritePaletteTag(sBreakBarPaletteTags[battler]);
    u32 basePixels = 0;
    if (intro && slot != 0xFF)
    {
        // Keep the preceding layer under this fill. A partial final layer
        // retracts the uncovered edge gradually, ending at authoritative HP.
        basePixels = B_HEALTHBAR_PIXELS;
        if (fill->baseFrames == 0 && color == fill->bars)
            basePixels = target + (B_HEALTHBAR_PIXELS - target) * (target - pixels) / target;
    }
    // Palette exhaustion skips the optional presentation rather than using a wrong color.
    if (slot == 0xFF
     || !DrawHealthbarOverlay(battler, basePixels, B_HEALTHBAR_PIXELS, pixels))
    {
        FinishBreakBarFill(battler, fill);
        return FALSE;
    }
    if (fill->baseFrames != 0)
    {
        fill->baseFrames--;
        return TRUE;
    }
    if (pixels == target)
    {
        if (color == fill->bars)
        {
            FinishBreakBarFill(battler, fill);
            return FALSE;
        }
        // All visible pixels now use the top pair. Prepare the hidden pair
        // before the next frame uses it as the completed underlying layer.
        LoadPalette(sBreakBarFillColors[color], OBJ_PLTT_ID(slot) + 14, 2 * sizeof(u16));
        fill->color++;
        fill->pixels = 0;
    }
    else
        fill->pixels = min(pixels + (intro ? 1 : BREAK_BAR_GRANT_PIXELS_PER_FRAME), target);
    return TRUE;
}

bool32 AnimateBreakBarIntro(u32 battler)
{
    if (battler >= gBattlersCount)
        return FALSE;
    if (gBattleControllerExecFlags)
        return TRUE;
    if (!sBreakBarIntros[battler].active)
    {
        if (!CanPresentBreakBarIntro(battler) || GetBreakBarOwner(battler)->breakBarIntroShown)
            return FALSE;
        GetBreakBarOwner(battler)->breakBarIntroShown = TRUE;
        StartBreakBarFill(battler, &sBreakBarIntros[battler], BREAK_BAR_PURPLE);
    }
    return AnimateBreakBarFill(battler, &sBreakBarIntros[battler]);
}

void BS_AnimateBreakBarIntro(void)
{
    NATIVE_ARGS();
    if (!AnimateBreakBarIntro(gBattleScripting.battler))
        gBattlescriptCurrInstr = cmd->nextInstr;
}

bool32 AnimateConfiguredBreakBar(u32 battler)
{
    if (battler >= MAX_BATTLERS_COUNT)
        return FALSE;
    return AnimateBreakBarFill(battler, &sBreakBarGrantFills[battler]);
}

#if BREAK_BAR_ENABLE_LEGACY_LERP
static void Task_BreakBarPaletteFade(u8 taskId)
{
    u32 battler = gTasks[taskId].data[0];
    u32 color = gTasks[taskId].data[1];
    u32 frame = gTasks[taskId].data[2];
    if (battler >= gBattlersCount || GetBreakBarCount(battler) != color
     || GetHealthbarSprite(battler) == MAX_SPRITES)
    {
        DestroyTask(taskId);
        return;
    }
    u32 slot = IndexOfSpritePaletteTag(sBreakBarPaletteTags[battler]);
    if (slot == 0xFF)
    {
        DestroyTask(taskId);
        return;
    }

    u16 palette[16];
    BuildBreakBarPalette(color, palette);
    for (u32 i = 0; i < ARRAY_COUNT(palette); i++)
    {
        u32 from = gBattleInterface_BallDisplayPal[i];
        u32 to = palette[i];
        s32 r = from & 31, g = (from >> 5) & 31, b = (from >> 10) & 31;
        r += (((s32)(to & 31) - r) * (s32)frame) / BREAK_BAR_FADE_FRAMES;
        g += (((s32)((to >> 5) & 31) - g) * (s32)frame) / BREAK_BAR_FADE_FRAMES;
        b += (((s32)((to >> 10) & 31) - b) * (s32)frame) / BREAK_BAR_FADE_FRAMES;
        palette[i] = RGB(r, g, b);
    }
    LoadPalette(palette, OBJ_PLTT_ID(slot), PLTT_SIZE_4BPP);
    if (frame == BREAK_BAR_FADE_FRAMES)
        DestroyTask(taskId);
    else
        gTasks[taskId].data[2]++;
}
#endif

bool32 ActivateBreakBar(u32 battler, const struct BreakBarConfig *config)
{
    struct PartyState *owner = GetBreakBarOwner(battler);
    if (owner == NULL || config == NULL || !IsBattlerAlive(battler)
     || config->extraBars == 0 || config->extraBars > BREAK_BAR_MAX_EXTRA_BARS
     || GetBreakBarCount(battler) != 0)
        return FALSE;

    owner->breakBarConfig = config;
    owner->breakBarsRemaining = GetInitialBreakBarCount(config);
    owner->breakBarCoverScripts = 0;
    owner->breakBarIntroShown = FALSE;
    owner->breakBarsPeak = max(owner->breakBarsPeak, owner->breakBarsRemaining);
    if (owner->breakBarsRemaining != 0 && !(config->flags & BREAK_BAR_DEFER_INITIAL_DRAW))
    {
        RefreshBreakBarPalette(battler, FALSE);
        if (GetHealthbarSprite(battler) != MAX_SPRITES)
            UpdateHealthboxAttribute(gHealthboxSpriteIds[battler], GetBattlerMon(battler), HEALTHBOX_HEALTH_BAR);
    }
    return TRUE;
}

bool32 TryBreakBar(u32 battler, const u8 *returnInstr)
{
    struct PartyState *owner = GetBreakBarOwner(battler);
    if (owner == NULL || GetBreakBarCount(battler) == 0
     || gBattleMons[battler].hp != 0 || (gAbsentBattlerFlags & (1u << battler)))
        return FALSE;

    const struct BreakBarConfig *config = owner->breakBarConfig;
    if ((config->flags & BREAK_BAR_IGNORE_WHILE_TRANSFORMED)
     && gBattleMons[battler].volatiles.transformed)
        return FALSE;

    u32 phase = owner->breakBarsRemaining - 1;
    bool32 savedLayer = owner->breakBarSavedMaxHp[phase] != 0;
    // A covering layer granted from the phase table keeps its scripts: restore, then onBreak, then after.
    bool32 coverScripts = savedLayer && (owner->breakBarCoverScripts & (1u << phase));
    const u8 *onScript = config->phases[phase].onBreakScript;
    const u8 *afterScript = (savedLayer && !coverScripts) ? NULL : config->phases[phase].afterBreakScript;
    bool32 pushOnScript = coverScripts && onScript != NULL; // NULL on a covering layer is "nothing", never a refill
    const u8 *adaptScript = KoankoArceusGetBreakScript(battler, phase);
    u32 scriptCount = 2 + (afterScript != NULL) + pushOnScript + (adaptScript != NULL);
    // Preserve the original target while phase scripts operate on the bar owner.
    if (gBattleStruct->savedTargetCount >= ARRAY_COUNT(gBattleStruct->savedBattlerTarget)
     || gBattleResources->battleScriptsStack->size + scriptCount
        > ARRAY_COUNT(gBattleResources->battleScriptsStack->ptr))
        return FALSE;
    SaveBattlerTarget(gBattlerTarget);
    gBattlerTarget = battler;

    if (adaptScript != NULL)
        KoankoArceusCommitBreak(battler);
    owner->breakBarPending = FALSE;
    owner->breakBarsRemaining--;
    TrimCoverScripts(owner);
    RefreshBreakBarPalette(battler, FALSE);
    BattleScriptPush(returnInstr);
    BattleScriptPush(BattleScript_BreakBarReturn);
    if (afterScript != NULL)
        BattleScriptPush(afterScript);
    if (pushOnScript)
        BattleScriptPush(onScript);
    if (adaptScript != NULL)
        BattleScriptPush(adaptScript);
    gBattlescriptCurrInstr = savedLayer ? BattleScript_RestoreSavedBreakBar : onScript;
    if (gBattlescriptCurrInstr == NULL)
        gBattlescriptCurrInstr = BattleScript_BreakBarRefill;
    return TRUE;
}

void BS_RefillBreakBar(void)
{
    NATIVE_ARGS();
    SetHealAmount(gBattlerTarget, gBattleMons[gBattlerTarget].maxHP);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

void BS_ResolveBreakBars(void)
{
    NATIVE_ARGS();
    if (gBattleControllerExecFlags)
        return;
    for (u32 battler = 0; battler < gBattlersCount; battler++)
        if (IsBreakBarPending(battler) && TryBreakBar(battler, gBattlescriptCurrInstr))
            return;
    gBattlescriptCurrInstr = cmd->nextInstr;
}

void BS_StartKazuraStackBreaker(void)
{
    NATIVE_ARGS();

    u32 battler = gBattlerTarget;
    u32 pixels = B_HEALTHBAR_PIXELS;

    if (battler >= gBattlersCount)
    {
        gBattlescriptCurrInstr = cmd->nextInstr;
        return;
    }

    // The first bar begins at its actual HP percentage.
    if (gBattleMons[battler].maxHP != 0)
    {
        pixels = (u32)gBattleMons[battler].hp
               * B_HEALTHBAR_PIXELS
               / gBattleMons[battler].maxHP;

        if (gBattleMons[battler].hp != 0 && pixels == 0)
            pixels = 1;
    }

    sKazuraStackBreakerPixels[battler] = pixels;
    sKazuraStackBreakerActive[battler] = TRUE;

    gBattlescriptCurrInstr = cmd->nextInstr;
}

void BS_AnimateKazuraStackBreaker(void)
{
    NATIVE_ARGS();

    if (gBattleControllerExecFlags)
        return;

    u32 battler = gBattlerTarget;

    if (battler >= gBattlersCount)
    {
        gBattlescriptCurrInstr = cmd->nextInstr;
        return;
    }

    struct PartyState *owner = GetBreakBarOwner(battler);

    if (!sKazuraStackBreakerActive[battler])
    {
        gBattlescriptCurrInstr = cmd->nextInstr;
        return;
    }

    u32 pixels = sKazuraStackBreakerPixels[battler];
    bool32 hasExtraBars = owner != NULL
                       && owner->breakBarConfig != NULL
                       && owner->breakBarsRemaining != 0;

    // Colored layers use the overlay; the final red layer
    // uses the base bar. Neither operation changes battle HP.
    bool32 drew;

    if (hasExtraBars)
        drew = DrawHealthbarOverlay(battler, 0, 1, pixels);
    else
        drew = DrawHealthbarOverlay(battler, pixels,
                                   B_HEALTHBAR_PIXELS, 0);

    // Only animate numeric HP during the final red bar.
    if (!hasExtraBars && drew)
    {
        u32 displayedHp = (u32)gBattleMons[battler].hp
                        * pixels / B_HEALTHBAR_PIXELS;

        if (gBattleMons[battler].hp != 0 && pixels != 0 && displayedHp == 0)
            displayedHp = 1;

        UpdateHpTextInHealthbox(
            gHealthboxSpriteIds[battler],
            HP_CURRENT,
            displayedHp,
            gBattleMons[battler].maxHP);
    }

    // Do not hang the battle if the healthbox is unavailable.
    if (!drew)
    {
        if (owner != NULL)
        {
            owner->breakBarsRemaining = 0;
            TrimCoverScripts(owner);
            owner->breakBarPending = FALSE;
            owner->breakBarFillPixels = 0;
        }

        sKazuraStackBreakerActive[battler] = FALSE;
        gBattlescriptCurrInstr = cmd->nextInstr;
        return;
    }

    // Drain three pixels per frame.
    if (pixels > 0)
    {
        sKazuraStackBreakerPixels[battler] =
            pixels > 3 ? pixels - 3 : 0;
        return;
    }

    // This layer has been completely destroyed.
    if (hasExtraBars)
    {
        owner->breakBarsRemaining--;
        TrimCoverScripts(owner);
        owner->breakBarPending = FALSE;
        owner->breakBarFillPixels = 0;

        // Change to the next color without invoking phase scripts.
        RefreshBreakBarPalette(battler, FALSE);

        // The newly exposed layer appears immediately at 100%.
        sKazuraStackBreakerPixels[battler] = B_HEALTHBAR_PIXELS;

        return;
    }

    // The red base layer has now been destroyed too.
    sKazuraStackBreakerActive[battler] = FALSE;
    gBattlescriptCurrInstr = cmd->nextInstr;
}

void BS_ExhaustBreakBars(void)
{
    NATIVE_ARGS();

    if (gBattleControllerExecFlags)
        return;

    u32 battler = gBattlerTarget;
    struct PartyState *owner = GetBreakBarOwner(battler);

    if (owner != NULL)
    {
        owner->breakBarPending = FALSE;
        owner->breakBarsRemaining = 0;
        TrimCoverScripts(owner);
        owner->breakBarFillPixels = 0;

        RefreshBreakBarPalette(battler, FALSE);
    }

    gBattlescriptCurrInstr = cmd->nextInstr;
}

bool32 TryResolveBreakBarsAfterMove(void)
{
    for (u32 battler = 0; battler < gBattlersCount; battler++)
    {
        if (IsBreakBarPending(battler))
        {
            BattleScriptExecute(BattleScript_ResolveBreakBarsEnd2);
            return TRUE;
        }
    }
    return FALSE;
}

void BS_CanGrantConfiguredBreakBar(void)
{
    NATIVE_ARGS(u8 battler, const u8 *failInstr);
    u32 battler = GetBattlerForBattleScript(cmd->battler);
    gBattlescriptCurrInstr = CanGrantConfiguredBreakBar(battler) ? cmd->nextInstr : cmd->failInstr;
}

void BS_GrantConfiguredBreakBar(void)
{
    NATIVE_ARGS(u8 battler, u8 flags, const u8 *failInstr);
    if (gBattleControllerExecFlags)
        return;
    u32 battler = GetBattlerForBattleScript(cmd->battler);
    gBattlescriptCurrInstr = GrantConfiguredBreakBar(battler, cmd->flags) ? cmd->nextInstr : cmd->failInstr;
}

void BS_GetConfiguredBreakBarGrantResult(void)
{
    NATIVE_ARGS(u8 battler);
    gBattleCommunication[MULTISTRING_CHOOSER] = GetConfiguredBreakBarGrantResult(GetBattlerForBattleScript(cmd->battler));
    gBattlescriptCurrInstr = cmd->nextInstr;
}

void BS_AnimateConfiguredBreakBar(void)
{
    NATIVE_ARGS(u8 battler);
    if (!AnimateConfiguredBreakBar(GetBattlerForBattleScript(cmd->battler)))
        gBattlescriptCurrInstr = cmd->nextInstr;
}

void BS_StartBreakBarIntro(void)
{
    NATIVE_ARGS(u8 battler);
    if (gBattleControllerExecFlags)
        return;
    u32 battler = GetBattlerForBattleScript(cmd->battler);
    if (battler < MAX_BATTLERS_COUNT)
    {
        sBreakBarIntros[battler].active = FALSE;
        if (CanPresentBreakBarIntro(battler))
        {
            GetBreakBarOwner(battler)->breakBarIntroShown = TRUE;
            StartBreakBarFill(battler, &sBreakBarIntros[battler], BREAK_BAR_PURPLE);
        }
    }
    gBattlescriptCurrInstr = cmd->nextInstr;
}

void BS_ContinueBreakBarIntro(void)
{
    NATIVE_ARGS(u8 battler);
    u32 battler = GetBattlerForBattleScript(cmd->battler);
    if (battler >= MAX_BATTLERS_COUNT || !AnimateBreakBarFill(battler, &sBreakBarIntros[battler]))
        gBattlescriptCurrInstr = cmd->nextInstr;
}

void BS_FillSavedBreakBar(void)
{
    NATIVE_ARGS(u8 battler);
    if (gBattleControllerExecFlags)
        return;
    u32 battler = GetBattlerForBattleScript(cmd->battler);
    gBattleMons[battler].hp = gBattleMons[battler].maxHP;
    BtlController_EmitSetMonData(battler, B_COMM_TO_CONTROLLER, REQUEST_HP_BATTLE,
        1u << gBattlerPartyIndexes[battler], sizeof(gBattleMons[battler].hp), &gBattleMons[battler].hp);
    MarkBattlerForControllerExec(battler);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

void BS_TryGrantSavedBreakBar(void)
{
    NATIVE_ARGS(u8 battler, const u8 *failInstr);
    u32 battler = GetBattlerForBattleScript(cmd->battler);
    gBattlescriptCurrInstr = GrantSavedBreakBar(battler) ? cmd->nextInstr : cmd->failInstr;
}

void BS_AnimateSavedBreakBar(void)
{
    NATIVE_ARGS(u8 battler);
    if (gBattleControllerExecFlags)
        return;

    u32 battler = GetBattlerForBattleScript(cmd->battler);
    struct PartyState *owner = GetBreakBarOwner(battler);

    if (owner == NULL || owner->breakBarsRemaining == 0)
    {
        gBattlescriptCurrInstr = cmd->nextInstr;
        return;
    }

    u32 covered = owner->breakBarsRemaining - 1;
    u32 savedHp = owner->breakBarSavedHp[covered];
    u32 savedMaxHp = owner->breakBarSavedMaxHp[covered];

    RefreshBreakBarPalette(battler, FALSE);

    u32 slot = IndexOfSpritePaletteTag(sBreakBarPaletteTags[battler]);

    if (slot != 0xFF)
    {
        const u16 *coveredColors;
        if (covered != 0)
            coveredColors = sBreakBarFillColors[covered];
        else if (owner->breakBarSavedBaseWasRegistered || savedMaxHp == 0)
            coveredColors = &sBreakBarBasePalette[14];
        else
        {
            // The overlay draws the covered HP with the red fill tile (palette
            // entries 14/15). Give those entries the covered ordinary bar's
            // actual color until the new layer finishes filling.
            u32 pixels = min(savedHp * B_HEALTHBAR_PIXELS / savedMaxHp, B_HEALTHBAR_PIXELS);
            if (savedHp != 0 && pixels == 0)
                pixels = 1;
            u32 colorIndex = pixels > B_HEALTHBAR_PIXELS * 50 / 100 ? 10
                           : pixels > B_HEALTHBAR_PIXELS * 20 / 100 ? 12 : 14;
            coveredColors = &gBattleInterface_BallDisplayPal[colorIndex];
        }
        LoadPalette(coveredColors, OBJ_PLTT_ID(slot) + 14, 2 * sizeof(u16));
    }

    bool32 drew = DrawHealthbarOverlay(
        battler,
        savedHp,
        savedMaxHp,
        owner->breakBarFillPixels);

    // Animate the numeric HP alongside the graphical bar.
    if (drew && IsOnPlayerSide(battler))
    {
        u32 displayedHp =
            savedMaxHp * owner->breakBarFillPixels / B_HEALTHBAR_PIXELS;

        if (savedHp != 0
         && owner->breakBarFillPixels != 0
         && displayedHp == 0)
            displayedHp = 1;

        UpdateHpTextInHealthbox(
            gHealthboxSpriteIds[battler],
            HP_CURRENT,
            displayedHp,
            savedMaxHp);
    }

    if (!drew || owner->breakBarFillPixels == B_HEALTHBAR_PIXELS)
    {
        if (slot != 0xFF)
            LoadPalette(&sBreakBarBasePalette[14],
                OBJ_PLTT_ID(slot) + 14, 2 * sizeof(u16));

        if (drew)
            UpdateHealthboxAttribute(
                gHealthboxSpriteIds[battler],
                GetBattlerMon(battler),
                HEALTHBOX_HEALTH_BAR);

        gBattlescriptCurrInstr = cmd->nextInstr;
        return;
    }

    owner->breakBarFillPixels++;
}

void BS_RestoreSavedBreakBar(void)
{
    NATIVE_ARGS();
    if (gBattleControllerExecFlags)
        return;
    u32 battler = gBattlerTarget;
    struct PartyState *owner = GetBattlerPartyState(battler);
    u32 layer = owner->breakBarsRemaining;
    ReductionBeginMutation(battler);
    gBattleMons[battler].hp = owner->breakBarSavedHp[layer];
    gBattleMons[battler].maxHP = owner->breakBarSavedMaxHp[layer];
    owner->breakBarSavedHp[layer] = owner->breakBarSavedMaxHp[layer] = 0;
    owner->breakBarFillPixels = 0;
    ReductionEndMutation(battler);
    BtlController_EmitSetMonData(battler, B_COMM_TO_CONTROLLER, REQUEST_MAX_HP_BATTLE,
        1u << gBattlerPartyIndexes[battler], sizeof(gBattleMons[battler].maxHP), &gBattleMons[battler].maxHP);
    // BtlController_EmitSetMonData(battler, B_COMM_TO_CONTROLLER, REQUEST_HP_BATTLE,
    //    1u << gBattlerPartyIndexes[battler], sizeof(gBattleMons[battler].hp), &gBattleMons[battler].hp);
    MarkBattlerForControllerExec(battler);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

void BS_SyncSavedBreakBarHp(void)
{
    NATIVE_ARGS(u8 battler);
    if (gBattleControllerExecFlags)
        return;
    u32 battler = GetBattlerForBattleScript(cmd->battler);
    BtlController_EmitSetMonData(battler, B_COMM_TO_CONTROLLER, REQUEST_HP_BATTLE,
        1u << gBattlerPartyIndexes[battler], sizeof(gBattleMons[battler].hp), &gBattleMons[battler].hp);
    MarkBattlerForControllerExec(battler);
    gBattlescriptCurrInstr = cmd->nextInstr;
}

void BS_RevealSavedBreakBar(void)
{
    NATIVE_ARGS(u8 battler);

    if (gBattleControllerExecFlags)
        return;

    u32 battler = GetBattlerForBattleScript(cmd->battler);
    struct PartyState *owner = GetBreakBarOwner(battler);

    if (owner == NULL || GetHealthbarSprite(battler) == MAX_SPRITES)
    {
        gBattlescriptCurrInstr = cmd->nextInstr;
        return;
    }

    u32 hp = gBattleMons[battler].hp;
    u32 maxHp = gBattleMons[battler].maxHP;

    if (maxHp == 0)
    {
        gBattlescriptCurrInstr = cmd->nextInstr;
        return;
    }

    u32 targetPixels = min(
        hp * B_HEALTHBAR_PIXELS / maxHp,
        B_HEALTHBAR_PIXELS);

    if (hp != 0 && targetPixels == 0)
        targetPixels = 1;

    u32 pixels = min(owner->breakBarFillPixels, targetPixels);

    // Draw the revealed layer progressively from empty.
    bool32 drew;
    if (GetBreakBarCount(battler) != 0)
    {
        // Extra Break Bars remaining:
        // Use the currently selected Break Bar color.
        drew = DrawHealthbarOverlay(battler, 0, 1, pixels);
    }
    else
    {
        // Final HP layer:
        // Use the red tiles regardless of HP percentage.
        drew = DrawHealthbarOverlay(
            battler, pixels, B_HEALTHBAR_PIXELS, 0);
    }
    if (drew && IsOnPlayerSide(battler))
    {
        u32 displayedHp = min(
            maxHp * pixels / B_HEALTHBAR_PIXELS,
            hp);

        if (pixels != 0 && hp != 0 && displayedHp == 0)
            displayedHp = 1;

        UpdateHpTextInHealthbox(
            gHealthboxSpriteIds[battler],
            HP_CURRENT,
            displayedHp,
            maxHp);
    }

    if (!drew || pixels >= targetPixels)
    {
        // Final authoritative redraw ensures exact HP,
        // including partial HP and rounding.
        UpdateHealthboxAttribute(
            gHealthboxSpriteIds[battler],
            GetBattlerMon(battler),
            HEALTHBOX_HEALTH_BAR);

        if (IsOnPlayerSide(battler))
            UpdateHpTextInHealthbox(
                gHealthboxSpriteIds[battler],
                HP_BOTH,
                hp,
                maxHp);

        gBattlescriptCurrInstr = cmd->nextInstr;
        return;
    }

    owner->breakBarFillPixels++;
}

void ReshowBreakBars(void)
{
    memset(sBreakBarIntros, 0, sizeof(sBreakBarIntros));
    memset(sBreakBarGrantFills, 0, sizeof(sBreakBarGrantFills));
    for (u32 battler = 0; battler < gBattlersCount; battler++)
    {
        StopBreakBarFade(battler);
        if (GetBreakBarCount(battler) != 0)
            sLoadedBreakBarColors[battler] = 0;
        RefreshBreakBarPalette(battler, FALSE);
        if (GetHealthbarSprite(battler) != MAX_SPRITES)
            UpdateHealthboxAttribute(gHealthboxSpriteIds[battler], GetBattlerMon(battler), HEALTHBOX_HEALTH_BAR);
    }
}

void FreeBreakBarGraphics(void)
{
    memset(sBreakBarIntros, 0, sizeof(sBreakBarIntros));
    memset(sBreakBarGrantFills, 0, sizeof(sBreakBarGrantFills));
    for (u32 battler = 0; battler < MAX_BATTLERS_COUNT; battler++)
    {
        StopBreakBarFade(battler);
        FreeSpritePaletteByTag(sBreakBarPaletteTags[battler]);
        sLoadedBreakBarColors[battler] = 0;
    }
}
