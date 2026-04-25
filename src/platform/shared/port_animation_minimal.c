#ifdef __PORT__

#include "entity.h"

/*
 * Minimal host-side animation shims to keep CI smoke paths alive until the
 * full asm/src/code_08003FC4.s animation block is ported.
 */

void InitializeAnimation(Entity* entity, u32 animIndex) {
    entity->animIndex = animIndex;
    entity->frameIndex = 0;
    entity->frame = 0;
    entity->frameSpriteSettings = 0;
    entity->spriteOffsetX = 0;
    entity->spriteOffsetY = 0;
}

void InitAnimationForceUpdate(Entity* entity, u32 animIndex) {
    InitializeAnimation(entity, animIndex);
    entity->lastFrameIndex = 0xff;
}

void UpdateAnimationVariableFrames(Entity* entity, u32 step) {
    (void)entity;
    (void)step;
}

void GetNextFrame(Entity* entity) {
    UpdateAnimationVariableFrames(entity, 1);
}

void UpdateAnimationSingleFrame(Entity* entity) {
    GetNextFrame(entity);
}

#endif /* __PORT__ */
