#include "global.h"
#include "test/test.h"
#include "event_object_movement.h"
#include "field_player_avatar.h"
#include "sprite.h"
#include "sprite_affine_anims.h"
#include "sprite_effects.h"
#include "event_data.h"
#include "constants/event_objects.h"

extern u32 gOamMatrixAllocBitmap;
extern void AdjustAffineSpriteOffset(struct Sprite *sprite);

static u32 CreateAvatarScaleFixture(u16 graphicsId)
{
    FlagSet(FLAG_PLAYER_OVERWORLD_AFFINE);
    ResetSpriteData();
    const struct ObjectEventGraphicsInfo *graphics = GetObjectEventGraphicsInfo(graphicsId);
    struct SpriteTemplate template = {
        .tileTag = TAG_NONE,
        .paletteTag = TAG_NONE,
        .oam = graphics->oam,
        .anims = gDummySpriteAnimTable,
        .images = graphics->images,
        .affineAnims = graphics->affineAnims != NULL ? graphics->affineAnims : gDummySpriteAffineAnimTable,
        .callback = SpriteCallbackDummy,
    };
    u32 id = CreateSprite(&template, 120, 80, 0);
    EXPECT_LT(id, MAX_SPRITES);
    gPlayerAvatar.objectEventId = 0;
    gPlayerAvatar.spriteId = id;
    gObjectEvents[0].active = TRUE;
    gObjectEvents[0].isPlayer = TRUE;
    gObjectEvents[0].spriteId = id;
    gObjectEvents[0].graphicsId = graphicsId;
    gObjectEvents[0].facingDirection = DIR_SOUTH;
    // Object-event setup uses logical frame dimensions, unlike raw sprites.
    gSprites[id].centerToCornerVecX = -graphics->width / 2;
    gSprites[id].centerToCornerVecY = -graphics->height / 2;
    gSprites[id].subspriteTables = graphics->subspriteTables;
    gSprites[id].subspriteMode = SUBSPRITES_ON;
    gSprites[id].subspriteTableNum = 1;
    return id;
}

static void FinishAvatarScaleFixture(void)
{
    FlagClear(FLAG_PLAYER_OVERWORLD_AFFINE);
    ResetSpriteData();
    memset(&gPlayerAvatar, 0, sizeof(gPlayerAvatar));
    memset(&gObjectEvents[0], 0, sizeof(gObjectEvents[0]));
}

TEST("Player Sprite Scale regular mode preserves movement offsets despite PLAYER_SMOL in every avatar state")
{
    bool32 wasSmol = FlagGet(FLAG_PLAYER_SMOL);
    FlagSet(FLAG_PLAYER_SMOL);
    for (u32 state = PLAYER_AVATAR_STATE_NORMAL; state <= PLAYER_AVATAR_STATE_VSSEEKER; state++)
    {
        for (u32 gender = MALE; gender <= FEMALE; gender++)
        {
            u32 id = CreateAvatarScaleFixture(GetPlayerAvatarGraphicsIdByStateIdAndGender(state, gender));
            FlagClear(FLAG_PLAYER_OVERWORLD_AFFINE);
            struct Sprite *sprite = &gSprites[id];
            sprite->x2 = 3;
            sprite->y2 = -7; // A ledge/bike/Surf-owned transient offset.
            UpdatePlayerOverworldAffine(sprite);
            PlayerStaysShrunk(NULL);
            StartPlayerShrinkAnimation(NULL);
            ApplyPlayerSpriteAffineOrigin(sprite);
            ResetPlayerSprite(NULL);
            // PlayerSetAnimId calls this separate movement-entry hook. The old
            // unconditional (-8, -16) write survived the per-frame mode guard.
            AdjustAffineSpriteOffset(sprite);
            EXPECT_EQ((u32)sprite->oam.affineMode, ST_OAM_AFFINE_OFF);
            EXPECT_EQ(gOamMatrixAllocBitmap, 0);
            EXPECT_EQ(sprite->x2, 3);
            EXPECT_EQ(sprite->y2, -7);
            EXPECT_EQ(FlagGet(FLAG_PLAYER_SMOL), TRUE);
        }
    }
    if (!wasSmol) FlagClear(FLAG_PLAYER_SMOL);
    FinishAvatarScaleFixture();
}

TEST("Player Sprite Scale explicit scripted opt-in restores regular handling and releases its matrix")
{
    bool32 wasSmol = FlagGet(FLAG_PLAYER_SMOL);
    u32 id = CreateAvatarScaleFixture(OBJ_EVENT_GFX_BRENDAN_NORMAL);
    FlagClear(FLAG_PLAYER_OVERWORLD_AFFINE);
    FlagSet(FLAG_PLAYER_SMOL);
    EnablePlayerOverworldAffine(NULL);
    AnimateSprite(&gSprites[id]);
    EXPECT_EQ((u32)gSprites[id].oam.affineMode, ST_OAM_AFFINE_DOUBLE);
    EXPECT_NE(gOamMatrixAllocBitmap, 0);
    DisablePlayerOverworldAffine(NULL);
    EXPECT_EQ((u32)gSprites[id].oam.affineMode, ST_OAM_AFFINE_OFF);
    EXPECT_EQ(gOamMatrixAllocBitmap, 0);
    EXPECT_EQ(gSprites[id].x2, 0);
    EXPECT_EQ(gSprites[id].y2, 0);
    EXPECT_EQ(FlagGet(FLAG_PLAYER_SMOL), TRUE);
    gSprites[id].y2 = -7;
    UpdatePlayerOverworldAffine(&gSprites[id]);
    EXPECT_EQ(gSprites[id].y2, -7);
    // Clearing the flag in an event also retires an active sequence next frame.
    EnablePlayerOverworldAffine(NULL);
    FlagClear(FLAG_PLAYER_OVERWORLD_AFFINE);
    UpdatePlayerOverworldAffine(&gSprites[id]);
    EXPECT_EQ((u32)gSprites[id].oam.affineMode, ST_OAM_AFFINE_OFF);
    EXPECT_EQ(gOamMatrixAllocBitmap, 0);
    if (!wasSmol) FlagClear(FLAG_PLAYER_SMOL);
    FinishAvatarScaleFixture();
}

TEST("Player Sprite Scale all avatar states and genders use the correct affine canvas origin")
{
    for (u32 state = PLAYER_AVATAR_STATE_NORMAL; state <= PLAYER_AVATAR_STATE_VSSEEKER; state++)
    {
        for (u32 gender = MALE; gender <= FEMALE; gender++)
        {
            u32 id = CreateAvatarScaleFixture(GetPlayerAvatarGraphicsIdByStateIdAndGender(state, gender));
            struct Sprite *sprite = &gSprites[id];
            PlayerStaysShrunk(NULL);
            AnimateSprite(sprite);
            EXPECT_EQ((u32)sprite->oam.affineMode, ST_OAM_AFFINE_DOUBLE);
            EXPECT_EQ(sprite->x2, -gOamDimensions[sprite->oam.shape][sprite->oam.size].width / 2);
            EXPECT_EQ(sprite->y2, -gOamDimensions[sprite->oam.shape][sprite->oam.size].height / 2);
            EXPECT_EQ(sprite->x, 120);
            EXPECT_EQ(sprite->y, 80);
            if (state == PLAYER_AVATAR_STATE_NORMAL)
            {
                EXPECT_EQ(sprite->x2, -8); // The user's existing walking correction.
                EXPECT_EQ(sprite->y2, -16);
            }
        }
    }
    FinishAvatarScaleFixture();
}

TEST("Player Sprite Scale decoration and alternate protagonist sprites use their own dimensions")
{
    static const u16 graphics[] = {
        OBJ_EVENT_GFX_BRENDAN_DECORATING, OBJ_EVENT_GFX_MAY_DECORATING,
        OBJ_EVENT_GFX_RED_NORMAL, OBJ_EVENT_GFX_RED_BIKE, OBJ_EVENT_GFX_RED_SURF,
        OBJ_EVENT_GFX_RED_FIELD_MOVE, OBJ_EVENT_GFX_RED_FISH,
        OBJ_EVENT_GFX_GREEN_NORMAL, OBJ_EVENT_GFX_GREEN_BIKE, OBJ_EVENT_GFX_GREEN_SURF,
        OBJ_EVENT_GFX_GREEN_FIELD_MOVE, OBJ_EVENT_GFX_GREEN_FISH,
        OBJ_EVENT_GFX_RED_VS_SEEKER, OBJ_EVENT_GFX_RED_VS_SEEKER_BIKE,
        OBJ_EVENT_GFX_GREEN_VS_SEEKER, OBJ_EVENT_GFX_GREEN_VS_SEEKER_BIKE,
    };
    for (u32 i = 0; i < ARRAY_COUNT(graphics); i++)
    {
        u32 id = CreateAvatarScaleFixture(graphics[i]);
        PlayerStaysShrunk(NULL);
        EXPECT_EQ(gSprites[id].x2, -gOamDimensions[gSprites[id].oam.shape][gSprites[id].oam.size].width / 2);
        EXPECT_EQ(gSprites[id].y2, -gOamDimensions[gSprites[id].oam.shape][gSprites[id].oam.size].height / 2);
    }
    FinishAvatarScaleFixture();
}

TEST("Player Sprite Scale priority pieces share an origin and mirror as a complete avatar")
{
    static const u16 graphics[] = {OBJ_EVENT_GFX_BRENDAN_NORMAL, OBJ_EVENT_GFX_MAY_MACH_BIKE};
    for (u32 i = 0; i < ARRAY_COUNT(graphics); i++)
    {
        u32 id = CreateAvatarScaleFixture(graphics[i]);
        struct Sprite *sprite = &gSprites[id];
        PlayerStaysShrunk(NULL);
        AnimateSprite(sprite);
        // A priority-split table, as used near bridges/foreground tiles.
        sprite->subspriteTableNum = 3;
        const struct SubspriteTable *table = &sprite->subspriteTables[3];
        EXPECT_GT(table->subspriteCount, 1);
        for (u32 east = FALSE; east <= TRUE; east++)
        {
            SetOamMatrix(sprite->oam.matrixNum, east ? -512 : 512, 0, 0, 512);
            // The ordinary BuildOamBuffer coordinate calculation.
            sprite->oam.x = sprite->x + sprite->x2 + sprite->centerToCornerVecX;
            sprite->oam.y = sprite->y + sprite->y2 + sprite->centerToCornerVecY;
            for (u32 part = 0; part < table->subspriteCount; part++)
            {
                const struct Subsprite *piece = &table->subsprites[part];
                struct OamData output = sprite->oam;
                output.shape = piece->shape;
                output.size = piece->size;
                output.priority = piece->priority;
                ApplyPlayerAffineSubspriteOrigin(sprite, piece, &output);
                s32 halfX = piece->x + gOamDimensions[piece->shape][piece->size].width / 2;
                s32 halfY = piece->y + gOamDimensions[piece->shape][piece->size].height / 2;
                s32 centerX = output.x + gOamDimensions[piece->shape][piece->size].width;
                s32 centerY = output.y + gOamDimensions[piece->shape][piece->size].height;
                EXPECT_EQ(centerX, 120 + (east ? -halfX : halfX) / 2);
                EXPECT_EQ(centerY, 80 + halfY / 2);
                EXPECT_EQ((u32)output.priority, (u32)piece->priority);
            }
        }
    }
    FinishAvatarScaleFixture();
}

TEST("Player Sprite Scale repeated graphics changes and resets reuse rather than leak affine matrices")
{
    u32 id = CreateAvatarScaleFixture(OBJ_EVENT_GFX_BRENDAN_NORMAL);
    for (u32 i = 0; i < 30; i++)
    {
        u32 state = i % (PLAYER_AVATAR_STATE_VSSEEKER + 1);
        u32 graphicsId = GetPlayerAvatarGraphicsIdByStateIdAndGender(state, MALE);
        ObjectEventSetGraphicsId(&gObjectEvents[0], graphicsId);
        PlayerStaysShrunk(NULL);
        AnimateSprite(&gSprites[id]);
        u32 bitmap = gOamMatrixAllocBitmap;
        EXPECT_NE(bitmap, 0);
        EXPECT_EQ(bitmap & (bitmap - 1), 0); // Exactly one owned matrix.
        PlayerStaysShrunk(NULL);
        EXPECT_EQ(gOamMatrixAllocBitmap, bitmap);
        ResetPlayerSprite(NULL);
        AnimateSprite(&gSprites[id]);
        if (!(GetObjectEventGraphicsInfo(graphicsId)->oam->affineMode & ST_OAM_AFFINE_ON_MASK))
            EXPECT_EQ(gOamMatrixAllocBitmap, 0);
    }
    FinishAvatarScaleFixture();
}

TEST("Player Sprite Scale matrix exhaustion leaves an ordinary avatar intact and retries safely")
{
    u32 id = CreateAvatarScaleFixture(OBJ_EVENT_GFX_BRENDAN_MACH_BIKE);
    u32 owned = 0;
    u32 matrix;
    while ((matrix = AllocOamMatrix()) != 0xFF)
        owned |= 1u << matrix;
    PlayerStaysShrunk(NULL);
    EXPECT_EQ((u32)gSprites[id].oam.affineMode, ST_OAM_AFFINE_OFF);
    EXPECT_EQ(gOamMatrixAllocBitmap, 0xFFFFFFFF);
    for (matrix = 0; matrix < OAM_MATRIX_COUNT; matrix++)
        if (owned & (1u << matrix)) FreeOamMatrix(matrix);
    PlayerStaysShrunk(NULL);
    EXPECT_EQ((u32)gSprites[id].oam.affineMode, ST_OAM_AFFINE_DOUBLE);
    EXPECT_EQ(gSprites[id].x2, -16);
    EXPECT_EQ(gSprites[id].y2, -16);
    ResetPlayerSprite(NULL);
    EXPECT_EQ(gOamMatrixAllocBitmap, 0);
    FinishAvatarScaleFixture();
}

TEST("Player Sprite Scale origin hook does not change NPCs or ordinary non-shrink affine sprites")
{
    u32 id = CreateAvatarScaleFixture(OBJ_EVENT_GFX_BRENDAN_NORMAL);
    struct Sprite *sprite = &gSprites[id];
    struct Subsprite part = {.x = -8, .y = -16, .shape = SPRITE_SHAPE(16x32), .size = SPRITE_SIZE(16x32)};
    struct OamData output = {.x = 33, .y = 44};
    ApplyPlayerAffineSubspriteOrigin(sprite, &part, &output);
    EXPECT_EQ((u32)output.x, 33);
    EXPECT_EQ((u32)output.y, 44);
    PlayerStaysShrunk(NULL);
    gObjectEvents[0].spriteId = MAX_SPRITES;
    ApplyPlayerAffineSubspriteOrigin(sprite, &part, &output);
    EXPECT_EQ((u32)output.x, 33);
    EXPECT_EQ((u32)output.y, 44);
    FinishAvatarScaleFixture();
}
