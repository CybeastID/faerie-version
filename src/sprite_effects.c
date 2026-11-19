#include "global.h"
#include "event_object_movement.h"
#include "field_player_avatar.h"
#include "sprite.h"
#include "sprite_affine_anims.h"  // Your custom affine header
#include "sprite_effects.h"
#include "script.h"
#include "event_data.h"

const union AffineAnimCmd gAffineAnim_Shrinking[] = {
    AFFINEANIMCMD_FRAME(0x0100, 0x0100, 0, 0),   // Start at 1x scale (256/256)
    AFFINEANIMCMD_FRAME(-0x0003, -0x0003, 0, 60), // Reach 0x4C/256, matching the persistent table below.
    AFFINEANIMCMD_FRAME(0x0000, 0x0000, 0, 8),    // Pause for 8 frames
    AFFINEANIMCMD_END
};

const union AffineAnimCmd *const gAffineAnims_Shrinking[] = {
    gAffineAnim_Shrinking,
};

const union AffineAnimCmd gAffineAnim_ShrunkPlayer[] = {
    AFFINEANIMCMD_FRAME(0x004C, 0x004C, 0, 0), // Set to small scale instantly (76/256).
    AFFINEANIMCMD_END
};
const union AffineAnimCmd *const gAffineAnims_ShrunkPlayer[] = {
    gAffineAnim_ShrunkPlayer,
};

const union AffineAnimCmd gAffineAnim_Reset[] = {
    AFFINEANIMCMD_FRAME(0x0100, 0x0100, 0, 0), // Reset to 1x scale instantly
    AFFINEANIMCMD_END
};

const union AffineAnimCmd *const gAffineAnims_Reset[] = {
    gAffineAnim_Reset,
};

const union AffineAnimCmd gAffineAnim_EnemyMonGiant[] = {
    AFFINEANIMCMD_FRAME(0x200, 0x200, 0, 0),  // 2x scale instant
    AFFINEANIMCMD_END,
};

const union AffineAnimCmd *const gAffineAnims_EnemyMonGiant[] = {
    gAffineAnim_EnemyMonGiant,
};


const union AffineAnimCmd gAffineAnim_PlayerMonTiny[] = {
    AFFINEANIMCMD_FRAME(0x80, 0x80, 0, 0),  // 0.5x scale instant
    AFFINEANIMCMD_END,
};
const union AffineAnimCmd *const gAffineAnims_PlayerMonTiny[] = {
    gAffineAnim_PlayerMonTiny,
};


static void ApplyPlayerFacingAffineFlip(struct Sprite *sprite)
{
    u8 dir = gObjectEvents[gPlayerAvatar.objectEventId].facingDirection;
    u8 hFlip = (dir == DIR_EAST || dir == DIR_NORTHEAST || dir == DIR_SOUTHEAST);

    SetSpriteOamFlipBits(sprite, hFlip, sprite->vFlip);
    RefreshSpriteAffineMatrix(sprite);
}

void ApplyPlayerSpriteAffineOrigin(struct Sprite *sprite)
{
    if (FlagGet(FLAG_PLAYER_OVERWORLD_AFFINE)
     && sprite->oam.affineMode == ST_OAM_AFFINE_DOUBLE && sprite->oam.shape < 3)
    {
        // Preserve the existing walking/running (-8, -16) correction while
        // deriving the proper origin for bikes, surfing, fishing and every
        // other avatar frame size. These are canvas offsets, not map coords.
        sprite->x2 = -gOamDimensions[sprite->oam.shape][sprite->oam.size].width / 2;
        sprite->y2 = -gOamDimensions[sprite->oam.shape][sprite->oam.size].height / 2;
    }
}

void ApplyPlayerAffineSubspriteOrigin(const struct Sprite *sprite, const struct Subsprite *part, struct OamData *oam)
{
    // Ordinary sprites and unrelated affine actors retain the upstream path.
    // Only the existing player-shrink animations use this shared-origin fix.
    if (!FlagGet(FLAG_PLAYER_OVERWORLD_AFFINE)
     || (sprite->affineAnims != gAffineAnims_Shrinking && sprite->affineAnims != gAffineAnims_ShrunkPlayer)
     || sprite->oam.affineMode != ST_OAM_AFFINE_DOUBLE || sprite->oam.shape >= 3 || part->shape >= 3
     || gPlayerAvatar.objectEventId >= OBJECT_EVENTS_COUNT
     || gObjectEvents[gPlayerAvatar.objectEventId].spriteId >= MAX_SPRITES
     || sprite != &gSprites[gObjectEvents[gPlayerAvatar.objectEventId].spriteId])
        return;
    const struct OamMatrix *matrix = &gOamMatrices[sprite->oam.matrixNum];
    if (matrix->a == 0 || matrix->d == 0 || matrix->b != 0 || matrix->c != 0)
        return; // A different temporary field animation owns rotation/shear.
    s32 width = gOamDimensions[part->shape][part->size].width;
    s32 height = gOamDimensions[part->shape][part->size].height;
    s32 frameWidth = gOamDimensions[sprite->oam.shape][sprite->oam.size].width;
    s32 frameHeight = gOamDimensions[sprite->oam.shape][sprite->oam.size].height;
    s32 centerX = sprite->oam.x - sprite->centerToCornerVecX + frameWidth / 2;
    s32 centerY = sprite->oam.y - sprite->centerToCornerVecY + frameHeight / 2;
    // Each priority piece has its own doubled affine canvas. Transform its
    // center about the full frame's origin, then account for that canvas.
    // Negative a/d also move pieces correctly when the avatar faces east.
    oam->x = centerX + (part->x + width / 2) * 256 / matrix->a - width;
    oam->y = centerY + (part->y + height / 2) * 256 / matrix->d - height;
}

static struct Sprite *GetPlayerScaleSprite(void)
{
    if (gPlayerAvatar.objectEventId >= OBJECT_EVENTS_COUNT)
        return NULL;
    u32 spriteId = gObjectEvents[gPlayerAvatar.objectEventId].spriteId;
    return spriteId < MAX_SPRITES && gSprites[spriteId].inUse ? &gSprites[spriteId] : NULL;
}

static bool32 PreparePlayerScaleSprite(struct Sprite *sprite)
{
    if (sprite == NULL || !FlagGet(FLAG_PLAYER_OVERWORLD_AFFINE))
        return FALSE;
    if (!(sprite->oam.affineMode & ST_OAM_AFFINE_ON_MASK))
    {
        u32 matrix = AllocOamMatrix();
        if (matrix == 0xFF)
            return FALSE;
        sprite->oam.matrixNum = matrix;
    }
    sprite->oam.affineMode = ST_OAM_AFFINE_DOUBLE;
    sprite->affineAnimPaused = FALSE;
    ApplyPlayerSpriteAffineOrigin(sprite);
    return TRUE;
}

void PlayerStaysShrunk(struct ScriptContext *ctx)
{
    (void)ctx;
    struct Sprite *sprite = GetPlayerScaleSprite();
    if (!PreparePlayerScaleSprite(sprite))
        return;

    sprite->affineAnims = gAffineAnims_ShrunkPlayer;
    StartSpriteAffineAnim(sprite, 0);
    ApplyPlayerFacingAffineFlip(sprite);
}

void StartPlayerShrinkAnimation(struct ScriptContext *ctx)
{
    (void)ctx;
    struct Sprite *sprite = GetPlayerScaleSprite();
    if (!PreparePlayerScaleSprite(sprite))
        return;

    sprite->affineAnims = gAffineAnims_Shrinking;
    StartSpriteAffineAnim(sprite, 0);
    ApplyPlayerFacingAffineFlip(sprite);
}

void ResetPlayerSprite(struct ScriptContext *ctx)
{
    struct Sprite *sprite = GetPlayerScaleSprite();
    if (sprite != NULL && (sprite->affineAnims == gAffineAnims_Shrinking || sprite->affineAnims == gAffineAnims_ShrunkPlayer))
    {
        const struct ObjectEventGraphicsInfo *graphics = GetObjectEventGraphicsInfo(gObjectEvents[gPlayerAvatar.objectEventId].graphicsId);
        if (graphics->oam->affineMode & ST_OAM_AFFINE_ON_MASK)
        {
            if (PreparePlayerScaleSprite(sprite))
            {
                sprite->oam.affineMode = graphics->oam->affineMode;
                sprite->affineAnims = graphics->affineAnims != NULL ? graphics->affineAnims : gDummySpriteAffineAnimTable;
                StartSpriteAffineAnim(sprite, 0);
            }
        }
        else
        {
            if (sprite->oam.affineMode & ST_OAM_AFFINE_ON_MASK)
                FreeOamMatrix(sprite->oam.matrixNum);
            sprite->oam.affineMode = ST_OAM_AFFINE_OFF;
            sprite->affineAnims = gDummySpriteAffineAnimTable;
        }
        // Regular overworld sprites use logical frame origins and own x2/y2
        // for ledges, bikes, Surf/Dive bobbing and Fly. Remove our correction
        // once, then leave those offsets entirely to the movement engine.
        sprite->x2 = 0;
        sprite->y2 = 0;
        ApplyPlayerFacingAffineFlip(sprite);
    }
    if (ctx != NULL)
        ScriptContext_ContinueScript(ctx);
}

void UpdatePlayerOverworldAffine(struct Sprite *sprite)
{
    if (sprite != GetPlayerScaleSprite())
        return;
    bool32 owned = sprite->affineAnims == gAffineAnims_Shrinking
               || sprite->affineAnims == gAffineAnims_ShrunkPlayer;
    if (!FlagGet(FLAG_PLAYER_OVERWORLD_AFFINE))
    {
        if (owned) ResetPlayerSprite(NULL);
        return;
    }
    if (owned)
    {
        ApplyPlayerSpriteAffineOrigin(sprite);
        ApplyPlayerFacingAffineFlip(sprite);
    }
}

void EnablePlayerOverworldAffine(struct ScriptContext *ctx)
{
    (void)ctx;
    FlagSet(FLAG_PLAYER_OVERWORLD_AFFINE);
    if (FlagGet(FLAG_PLAYER_SMOL)) PlayerStaysShrunk(NULL);
}

void DisablePlayerOverworldAffine(struct ScriptContext *ctx)
{
    (void)ctx;
    FlagClear(FLAG_PLAYER_OVERWORLD_AFFINE);
    ResetPlayerSprite(NULL);
}
