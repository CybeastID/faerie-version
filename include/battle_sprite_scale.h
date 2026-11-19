#ifndef GUARD_BATTLE_SPRITE_SCALE_H
#define GUARD_BATTLE_SPRITE_SCALE_H

#include "global.h"

#define BATTLE_SPRITE_SCALE_ONE 256
#define BATTLE_SPRITE_SCALE_SMOL 128
#define BATTLE_SPRITE_SCALE_MIN 96

// One battle-owned record per battler: interpolated presentation only, never
// another Reduction counter. The remaining fields are a single-frame render
// transaction, restored before animation/controller code runs again.
struct BattleSpriteScale
{
    u16 current;
    u8 owner;
    u8 renderMatrix;
    s16 savedY2;
    u8 savedMode;
    u8 savedMatrix;
};

u16 BattleSpriteScaleTarget(u32 battler);
u16 BattleSpriteScaleCurrent(u32 battler);
void BattleSpriteScaleReset(u32 battler);
void BattleSpriteScaleBuildOamBuffer(void);
// Animation-owned background copies need the same baseline as OBJ output.
void BattleSpriteScaleCopyMonGfx(u32 battler, void *dest, const void *src);

#endif
