#include "global.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_bug_space.h"
#include "battle_controllers.h"
#include "battle_main.h"
#include "battle_reduction.h"
#include "battle_sprite_scale.h"
#include "battle_util.h"
#include "data.h"
#include "event_data.h"
#include "main.h"
#include "pokemon.h"
#include "sprite.h"

STATIC_ASSERT(sizeof(struct BattleSpriteScale) == 8, BattleSpriteScaleSize);

#define SCALE_STEP 4 // Q8 per rendered frame; 40 frames for 1x -> minimum.
#define BUG_SPACE_START_THRESHOLD 160
#define SCALE_OWNER_VALID 0x80

#if TESTING
static EWRAM_DATA u16 sScaledBgCopies[MAX_BATTLERS_COUNT] = {0};
static EWRAM_DATA u16 sScaledCloneFrames[MAX_BATTLERS_COUNT] = {0};
u32 Test_BattleSpriteScaleBgCopies(u32 battler, bool32 clear)
{
    u32 count = sScaledBgCopies[battler];
    if (clear) sScaledBgCopies[battler] = 0;
    return count;
}
u32 Test_BattleSpriteScaleCloneFrames(u32 battler, bool32 clear)
{
    u32 count = sScaledCloneFrames[battler];
    if (clear) sScaledCloneFrames[battler] = 0;
    return count;
}
#endif

u16 BattleSpriteScaleTarget(u32 battler)
{
    if (gBattleStruct == NULL || battler >= gBattlersCount)
        return BATTLE_SPRITE_SCALE_ONE;

    u32 scale = BATTLE_SPRITE_SCALE_ONE;
    // This flag describes the actual player, not all allies on that side.
    // In particular an independently owned temporary partner is not the player.
    if (GetBattlerTrainer(battler) == B_TRAINER_PLAYER && FlagGet(FLAG_PLAYER_SMOL))
        scale = BATTLE_SPRITE_SCALE_SMOL;

    // Full Bug Space shrinks its opposition as the existing clock decays.
    // Crush-only mode deliberately retains stat crushing without this effect.
    if (BugSpaceHasDoomClock() && gBattleStruct->bugSpace.sourceBattler < gBattlersCount
     && GetBattlerSide(battler) != GetBattlerSide(gBattleStruct->bugSpace.sourceBattler))
    {
        u32 threshold = min(BUG_SPACE_START_THRESHOLD, gBattleStruct->bugSpace.bpThreshold);
        u32 bugScale = BATTLE_SPRITE_SCALE_MIN
            + (BATTLE_SPRITE_SCALE_ONE - BATTLE_SPRITE_SCALE_MIN) * threshold / BUG_SPACE_START_THRESHOLD;
        scale = min(scale, bugScale);
    }

    if (ReductionIsActive() && GetBattlerSide(battler) == gBattleStruct->reduction.affectedSide)
    {
        const struct ReductionOccupant *reduction = &gBattleStruct->reduction.occupant[battler];
        if (reduction->lostLevels != 0 && reduction->partyIndex == gBattlerPartyIndexes[battler])
        {
            // Canonical level and existing accumulated loss remain authoritative
            // even while a nested gameplay mutation temporarily lifts the overlay.
            u32 level = max(1, GetMonData(GetBattlerMon(battler), MON_DATA_LEVEL));
            u32 effective = max(1, (s32)level - reduction->lostLevels);
            u32 reductionScale = effective == 1 ? BATTLE_SPRITE_SCALE_MIN
                : BATTLE_SPRITE_SCALE_MIN
                  + (BATTLE_SPRITE_SCALE_ONE - BATTLE_SPRITE_SCALE_MIN) * effective / level;
            scale = min(scale, reductionScale);
        }
    }
    return scale;
}

u16 BattleSpriteScaleCurrent(u32 battler)
{
    if (gBattleStruct == NULL || battler >= gBattlersCount
     || gBattleStruct->spriteScale[battler].current == 0)
        return BattleSpriteScaleTarget(battler);
    return gBattleStruct->spriteScale[battler].current;
}

void BattleSpriteScaleReset(u32 battler)
{
    if (gBattleStruct != NULL && battler < MAX_BATTLERS_COUNT)
        memset(&gBattleStruct->spriteScale[battler], 0, sizeof(gBattleStruct->spriteScale[battler]));
}

static s16 ScaleMatrixValue(s16 value, u32 scale)
{
    // GBA OBJ matrices are inverse transforms. Shrinking therefore increases
    // their magnitude. Preserve rotation, shear, nonuniform scale and flips.
    s32 result = (s32)value * BATTLE_SPRITE_SCALE_ONE / (s32)scale;
    return max(-32768, min(32767, result));
}

static s16 GroundingOffset(u32 battler, const struct Sprite *sprite, u32 scale)
{
    enum Species species = GetBattlerVisualSpecies(battler);
    if (gBattleSpritesDataPtr->battlerData[battler].transformSpecies != SPECIES_NONE)
        species = gBattleSpritesDataPtr->battlerData[battler].transformSpecies;
    species = SanitizeSpeciesId(species);
    s32 picOffset = IsOnPlayerSide(battler)
        ? gSpeciesInfo[species].backPicYOffset : gSpeciesInfo[species].frontPicYOffset;
    // Same art-offset convention as SetBattlerSpriteYOffsetFromYScale. Keep
    // the ordinary visible bottom (including a hovering species' elevation).
    s32 halfHeight = max(0, MON_PIC_HEIGHT / 2 - picOffset);
    // Back poses can extend below the text window (notably the +8 entry
    // position). Anchoring to that hidden bottom buries a tiny mon. Preserve
    // the visible field edge instead, without changing any animation's y2.
    if (IsOnPlayerSide(battler))
        halfHeight = max(0, min(halfHeight, DISPLAY_HEIGHT - 48 - sprite->y - sprite->y2));
    return (halfHeight * (BATTLE_SPRITE_SCALE_ONE - scale) + 128) / BATTLE_SPRITE_SCALE_ONE;
}

static bool32 IsMonSprite(u32 battler)
{
    u32 spriteId = gBattlerSpriteIds[battler];
    if (spriteId >= MAX_SPRITES || !gSprites[spriteId].inUse
     || (gAbsentBattlerFlags & (1u << battler)))
        return FALSE;
    // Trainer back sprites share affine tables with mons during the intro.
    // The real battler image table distinguishes them and unrelated UI sprites.
    u32 position = GetBattlerPosition(battler);
    const struct SpriteFrameImage *images = gMonSpritesGfxPtr != NULL
        ? gMonSpritesGfxPtr->templates[position].images : gBattlerSpriteTemplates[position].images;
    return gSprites[spriteId].images == images && gSprites[spriteId].anims != gAnims_Trainer;
}

static void PrepareScale(u32 battler)
{
    struct BattleSpriteScale *state = &gBattleStruct->spriteScale[battler];
    state->renderMatrix = 0xFF;
    if (!IsMonSprite(battler))
        return;

    u32 trainer = GetBattlerTrainer(battler);
    u32 partyIndex = gBattlerPartyIndexes[battler];
    if (trainer >= MAX_BATTLE_TRAINERS || partyIndex >= PARTY_SIZE)
        return;
    u32 owner = SCALE_OWNER_VALID | (trainer << 3) | partyIndex;
    u32 target = BattleSpriteScaleTarget(battler);
    if (state->owner != owner || state->current == 0)
    {
        state->owner = owner;
        state->current = target; // An already-small mon emerges small.
    }
    else if (gBattleMons[battler].hp != 0 && gBattleOutcome == 0)
    {
        if (state->current < target)
            state->current = min(target, state->current + SCALE_STEP);
        else if (state->current > target)
            state->current = max(target, state->current - SCALE_STEP);
    }

    struct Sprite *sprite = &gSprites[gBattlerSpriteIds[battler]];
    if (state->current == BATTLE_SPRITE_SCALE_ONE || sprite->invisible
     || sprite->oam.affineMode == ST_OAM_AFFINE_ERASE
     || gBattleSpritesDataPtr->battlerData[battler].behindSubstitute)
        return;

    u32 matrix = AllocOamMatrix();
    if (matrix == 0xFF)
        return; // Keep the animation intact; try again next rendered frame.

    state->renderMatrix = matrix;
    state->savedMode = sprite->oam.affineMode;
    state->savedMatrix = sprite->oam.matrixNum;
    state->savedY2 = sprite->y2;
    if (sprite->oam.affineMode & ST_OAM_AFFINE_ON_MASK)
    {
        const struct OamMatrix *animation = &gOamMatrices[state->savedMatrix];
        SetOamMatrix(matrix, ScaleMatrixValue(animation->a, state->current),
            ScaleMatrixValue(animation->b, state->current),
            ScaleMatrixValue(animation->c, state->current),
            ScaleMatrixValue(animation->d, state->current));
    }
    else
    {
        s16 x = state->savedMatrix & 8 ? -256 : 256;
        s16 y = state->savedMatrix & 16 ? -256 : 256;
        SetOamMatrix(matrix, ScaleMatrixValue(x, state->current), 0, 0, ScaleMatrixValue(y, state->current));
        sprite->oam.affineMode = ST_OAM_AFFINE_NORMAL;
    }
    sprite->oam.matrixNum = matrix;
    sprite->y2 += GroundingOffset(battler, sprite, state->current);
}

void BattleSpriteScaleCopyMonGfx(u32 battler, void *dest, const void *src)
{
    u32 scale = BattleSpriteScaleCurrent(battler);
    if (!gMain.inBattle || gBattleStruct == NULL || gBattleSpritesDataPtr == NULL || battler >= gBattlersCount
     || scale == BATTLE_SPRITE_SCALE_ONE
     || gBattleSpritesDataPtr->battlerData[battler].behindSubstitute)
    {
        CpuCopy16(src, dest, MON_PIC_SIZE);
        return;
    }
    // BG1/BG2 are tiled text backgrounds, not affine layers. Sample directly
    // into their existing EWRAM tile buffer; no extra image buffer or IWRAM.
    // Retain normal monbg scrolling, palette fades and Transform's mosaic.
    const u8 *source = src;
    u8 *target = dest;
#if TESTING
    sScaledBgCopies[battler]++;
#endif
    const struct Sprite *sprite = &gSprites[gBattlerSpriteIds[battler]];
    s32 inverse = BATTLE_SPRITE_SCALE_ONE * BATTLE_SPRITE_SCALE_ONE / scale;
    s32 grounding = GroundingOffset(battler, sprite, scale);
    for (u32 y = 0; y < MON_PIC_HEIGHT; y++)
    {
        s32 sy = (32 * 256 + ((s32)y - 32 - grounding) * inverse) >> 8;
        s32 sxQ8 = 32 * 256 - 32 * inverse;
        for (u32 x = 0; x < MON_PIC_WIDTH; x += 2)
        {
            u32 pixels = 0;
            for (u32 half = 0; half < 2; half++, sxQ8 += inverse)
            {
                s32 sx = sxQ8 >> 8;
                if (sx >= 0 && sx < MON_PIC_WIDTH && sy >= 0 && sy < MON_PIC_HEIGHT)
                {
                    u32 offset = ((sy / 8) * 8 + sx / 8) * TILE_SIZE_4BPP + (sy % 8) * 4 + (sx % 8) / 2;
                    pixels |= ((source[offset] >> ((sx & 1) * 4)) & 15) << (half * 4);
                }
            }
            u32 offset = ((y / 8) * 8 + x / 8) * TILE_SIZE_4BPP + (y % 8) * 4 + (x % 8) / 2;
            target[offset] = pixels;
        }
    }
}

void BattleSpriteScaleBuildOamBuffer(void)
{
    if (!gMain.inBattle || gBattleStruct == NULL || gBattleSpritesDataPtr == NULL
     || gBattlersCount > MAX_BATTLERS_COUNT)
    {
        BuildOamBuffer();
        return;
    }

    for (u32 battler = 0; battler < gBattlersCount; battler++)
        PrepareScale(battler);
    BuildOamBuffer();
    // Stat changes use an OBJ-window copy, while several moves use blended
    // copies of the same mon. Those copies share its original matrix, not its
    // transient rendered binding. Rebind matching output only; their CPU state,
    // callbacks and lifetime remain entirely animation-owned. No new matrix.
    for (u32 battler = 0; battler < gBattlersCount; battler++)
    {
        const struct BattleSpriteScale *state = &gBattleStruct->spriteScale[battler];
        if (state->renderMatrix == 0xFF)
            continue;
        const struct Sprite *sprite = &gSprites[gBattlerSpriteIds[battler]];
        for (u32 i = 0; i < ARRAY_COUNT(gMain.oamBuffer); i++)
        {
            struct OamData *oam = &gMain.oamBuffer[i];
            if ((oam->objMode != ST_OAM_OBJ_WINDOW && oam->objMode != ST_OAM_OBJ_BLEND)
             || oam->tileNum != sprite->oam.tileNum || oam->shape != sprite->oam.shape || oam->size != sprite->oam.size
             || oam->affineMode != state->savedMode || oam->matrixNum != state->savedMatrix)
                continue;
            oam->affineMode = sprite->oam.affineMode;
            oam->matrixNum = state->renderMatrix;
            oam->y += sprite->y2 - state->savedY2;
#if TESTING
            sScaledCloneFrames[battler]++;
#endif
        }
    }
    for (u32 battler = 0; battler < gBattlersCount; battler++)
    {
        struct BattleSpriteScale *state = &gBattleStruct->spriteScale[battler];
        if (state->renderMatrix == 0xFF)
            continue;
        struct Sprite *sprite = &gSprites[gBattlerSpriteIds[battler]];
        // OAM has its composed transform now. Restore CPU state before the
        // next animation/controller step; none of them owns the base scale.
        sprite->oam.y -= sprite->y2 - state->savedY2;
        sprite->y2 = state->savedY2;
        sprite->oam.affineMode = state->savedMode;
        sprite->oam.matrixNum = state->savedMatrix;
        FreeOamMatrix(state->renderMatrix);
        state->renderMatrix = 0xFF;
    }
}
