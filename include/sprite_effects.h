#ifndef GUARD_SPRITE_EFFECTS_H
#define GUARD_SPRITE_EFFECTS_H

#include "global.h"

struct ScriptContext;
struct Sprite;
struct Subsprite;
struct OamData;

void StartPlayerShrinkAnimation(struct ScriptContext *ctx);
void ResetPlayerSprite(struct ScriptContext *ctx);
void PlayerStaysShrunk(struct ScriptContext *ctx);
void EnablePlayerOverworldAffine(struct ScriptContext *ctx);
void DisablePlayerOverworldAffine(struct ScriptContext *ctx);
void UpdatePlayerOverworldAffine(struct Sprite *sprite);
void ApplyPlayerSpriteAffineOrigin(struct Sprite *sprite);
void ApplyPlayerAffineSubspriteOrigin(const struct Sprite *sprite, const struct Subsprite *part, struct OamData *oam);

#endif
