#include "asm.h"
#include "map.h"

#include "player.h"
#include "enemy.h"
#include "projectile.h"
#include "room.h"
#include "structures.h"
#include "vram.h"
#include "effects.h"
#include "object.h"
#include "collision.h"
#include "physics.h"

#include <stddef.h>
#include <stdint.h>

u32 CalcDistance(s32 x, s32 y);
u32 CalcCollisionDirectionOLD(u32 direction, u32 collisions);
u32 GetNextFunction(Entity* entity);
u32 GetNonCollidedSide(Entity* entity);
void sub_080085CC(Entity* entity);
const KeyValuePair* FindEntryForKey(u32 key, const KeyValuePair* keyValuePairList);
void Knockback1(Entity* entity);
void Knockback2(Entity* entity);
extern const u8 gUnk_08007DF4[];
extern const u8 gUnk_0800232E[] __attribute__((weak));
extern const u8 gUnk_08002342[] __attribute__((weak));
extern const KeyValuePair gUnk_080046A4[] __attribute__((weak));
extern const u16 gUnk_080047F6[] __attribute__((weak));
extern const u8 gUnk_080082DC[] __attribute__((weak));
extern const u8 gUnk_0800833C[] __attribute__((weak));
extern const u8 gUnk_0800839C[] __attribute__((weak));
extern const u8 gUnk_080083FC[] __attribute__((weak));
extern const u8 gUnk_0800845C[] __attribute__((weak));
extern const u8 gUnk_080084BC[] __attribute__((weak));
extern const u8 gUnk_0800851C[] __attribute__((weak));
extern u16* gUnk_0800823C[];

static const u8* Port_TableOrFallback(const u8* candidate, const u8* fallback) {
    return (candidate != NULL) ? candidate : fallback;
}

static const u8* Port_GetFuserTable(Entity* entity) {
    if (entity->kind == ENEMY) {
        return gUnk_0800232E;
    }
    if (entity->kind == NPC) {
        return gUnk_08002342;
    }
    return NULL;
}

static u32 Port_PackEntityTypeKey(Entity* entity) {
    return ((u32)entity->id << 16) | ((u32)entity->type << 8) | entity->type2;
}

static u32 Port_GetFuserData(Entity* entity, u32* fuserTextId) {
    static const u32 kEntityTypeMasks[4] = { 0x00ffffffu, 0x00ffff00u, 0x00ff00ffu, 0x00ff0000u };
    const u8* table = Port_GetFuserTable(entity);
    u32 entityKey;

    if (fuserTextId != NULL) {
        *fuserTextId = 0;
    }
    if (table == NULL) {
        return 0;
    }

    entityKey = Port_PackEntityTypeKey(entity);
    table += 6; /* asm starts with `adds r4, #6` before first read */
    for (;;) {
        u32 entryId = table[0];
        u32 entryType = table[1];
        u32 entryType2 = table[2];
        u32 wildcardMaskIndex = 0;
        u32 entryKey;
        u32 mask;

        if (entryId == 0) {
            return 0;
        }
        if (entryType == 0xffu) {
            wildcardMaskIndex += 2;
        }
        if (entryType2 == 0xffu) {
            wildcardMaskIndex += 1;
        }
        if (entryType == 0xffu) {
            entryType = 0;
        }
        if (entryType2 == 0xffu) {
            entryType2 = 0;
        }

        entryKey = (entryId << 16) | (entryType << 8) | entryType2;
        mask = kEntityTypeMasks[wildcardMaskIndex];
        if ((entityKey & mask) == (entryKey & mask)) {
            if (fuserTextId != NULL) {
                *fuserTextId = (u32)(table[4] | (table[5] << 8));
            }
            return table[3];
        }
        table += 6;
    }
}

u32 Port_GetFuserTextId(Entity* entity) {
    u32 textId = 0;
    (void)Port_GetFuserData(entity, &textId);
    return textId;
}

u32 GetFuserId(Entity* entity) {
    return Port_GetFuserData(entity, NULL);
}

static u32 Port_sub_080086D8(Entity* entity, u32 x, u32 y, const u8* table) {
    u32 tile = GetCollisionDataAtWorldCoords(x, y, ((u8*)entity)[0x38]);

    if (gPlayerState.swim_state != 0 && ((u8*)&gPlayerState)[0x12] != 0x18) {
        if (tile < 0x10) {
            tile = 0x0f;
        }
    } else if (tile < 0x10) {
        if ((y & 8u) == 0u) {
            tile >>= 2;
        }
        if ((x & 8u) == 0u) {
            tile >>= 1;
        }
        return tile & 1u;
    }

    if (tile != 0xff) {
        u32 tableIndex = table[tile - 0x10];
        const u16* pattern = gUnk_0800823C[tableIndex];
        u32 yNibble2 = (y & 0x0fu) << 1;
        u32 shift = 0x0fu ^ (x & 0x0fu);
        tile = ((u32)pattern[yNibble2 >> 1]) >> shift;
    }
    return tile & 1u;
}

static u32 Port_CalcCollisionStaticEntityRet(Entity* a, Entity* b) {
    s32 ax, ay, bx, by;
    s32 aw, ah, bw, bh;

    if (((a->collisionLayer & b->collisionLayer) & 3u) == 0u) {
        return 0;
    }

    ax = (s32)(s16)a->x.HALF.HI;
    ay = (s32)(s16)a->y.HALF.HI;
    bx = (s32)(s16)b->x.HALF.HI;
    by = (s32)(s16)b->y.HALF.HI;

    aw = (a->hitbox != NULL) ? a->hitbox->width : 8;
    ah = (a->hitbox != NULL) ? a->hitbox->height : 8;
    bw = (b->hitbox != NULL) ? b->hitbox->width : 8;
    bh = (b->hitbox != NULL) ? b->hitbox->height : 8;

    if ((ax - bx) > (aw + bw) || (bx - ax) > (aw + bw)) {
        return 0;
    }
    if ((ay - by) > (ah + bh) || (by - ay) > (ah + bh)) {
        return 0;
    }
    return 1;
}

/* Direction/collision mask table from asm/src/code_08001A7C.s::gUnk_0800275C */
u16 gUnk_0800275C[32] = {
    0x0006, 0x6006, 0x6006, 0x6006, 0x6006, 0x6006, 0x6006, 0x6000, 0x6060, 0x6060, 0x6060,
    0x6060, 0x6060, 0x6060, 0x0060, 0x0660, 0x0660, 0x0660, 0x0660, 0x0660, 0x0660, 0x0600,
    0x0606, 0x0606, 0x0606, 0x0606, 0x0606, 0x0606, 0x0606, 0x0606, 0x0606, 0x0606,
};

u32 sub_08000E44(u32 value) {
    if (value != 0 && (s32)value >= 0) {
        return 1;
    }
    return value;
}

u32 sub_08000E62(u32 value) {
    value = (value & 0x55555555u) + ((value >> 1) & 0x55555555u);
    value = (value & 0x33333333u) + ((value >> 2) & 0x33333333u);
    value = (value & 0x0f0f0f0fu) + ((value >> 4) & 0x0f0f0f0fu);
    value = (value & 0x00ff00ffu) + ((value >> 8) & 0x00ff00ffu);
    value = value + (value << 16);
    return value >> 16;
}

void LinearMoveDirectionOLD(Entity* entity, u32 speed, u32 direction) {
    u32 collisions;
    u32 mask;
    u32 sineIndex;

    if ((direction & 0x80u) != 0u) {
        return;
    }

    collisions = entity->collisions;
    if ((direction & 7u) == 0u) {
        u32 adjustedDir = CalcCollisionDirectionOLD(direction, collisions);
        if (adjustedDir != direction) {
            direction = adjustedDir;
            speed = 0x100u;
        }
    }

    mask = gUnk_0800275C[direction & 0x1fu];
    collisions &= mask;
    sineIndex = (direction & 0x1fu) << 4;

    if ((collisions & 0x0000ee00u) == 0u) {
        s16 sx = gSineTable[sineIndex];
        if (sx != 0) {
            entity->x.WORD += ((s32)FixedMul(sx, (s16)speed)) << 8;
        }
    }

    if ((collisions & 0x000000eeu) == 0u) {
        s16 sy = gSineTable[sineIndex + 64u];
        if (sy != 0) {
            entity->y.WORD -= ((s32)FixedMul(sy, (s16)speed)) << 8;
        }
    }
}

u32 GravityUpdate(Entity* entity, u32 gravity) {
    s32 z = entity->z.WORD - entity->zVelocity;
    if (z < 0) {
        entity->z.WORD = z;
        entity->zVelocity -= (s32)gravity;
        return (u32)z;
    }

    entity->z.WORD = 0;
    entity->zVelocity = 0;
    return 0;
}

u32 CheckEntityPickup(Entity* target, Entity* source, u32 radiusX, u32 radiusY) {
    const Hitbox* hb = source->hitbox;
    s32 sourceX, sourceY;
    s32 offsetX, offsetY;
    u32 dx, dy;

    if (hb != NULL) {
        radiusX += hb->width;
        radiusY += hb->height;
        offsetX = hb->offset_x;
        offsetY = hb->offset_y;
    } else {
        offsetX = 0;
        offsetY = 0;
    }

    /* Matches asm's bit2 check on spriteSettings byte: mirror X offset when flipped. */
    if ((*(u8*)&source->spriteSettings & 0x4u) != 0u) {
        offsetX = -offsetX;
    }

    sourceX = (s32)(u16)source->x.HALF.HI + offsetX;
    sourceY = (s32)(u16)source->y.HALF.HI + offsetY;

    if (((target->collisionLayer & source->collisionLayer) & 3u) == 3u) {
        return 0;
    }

    if (radiusX != 0) {
        dx = (u32)(u16)target->x.HALF.HI - (u32)(u16)sourceX + radiusX;
        if ((radiusX << 1) < dx) {
            return 0;
        }
    }

    if (radiusY != 0) {
        dy = (u32)(u16)target->y.HALF.HI - (u32)(u16)sourceY + radiusY;
        if ((radiusY << 1) < dy) {
            return 0;
        }
    }

    return 1;
}

u32 sub_08003FDE(Entity* target, Entity* source, u32 radiusX, u32 radiusY) {
    if (CheckEntityPickup(target, source, radiusX, radiusY)) {
        return sub_0806F58C(target, source);
    }
    return 0;
}

u32 CheckBits(void* src, u32 bit, u32 count) {
    const u8* p = (const u8*)src;
    u32 i;

    for (i = 0; i < count; i++) {
        if (((p[(bit + i) >> 3] >> ((bit + i) & 7u)) & 1u) == 0u) {
            return 0;
        }
    }
    return 1;
}

void SumDropProbabilities(s16* out, const s16* a, const s16* b, const s16* c) {
    s32 i;
    for (i = 15; i >= 0; i--) {
        out[i] = a[i] + b[i] + c[i];
    }
}

u32 SumDropProbabilities2(s16* out, const s16* a, const s16* b, const s16* c) {
    s32 i;
    u32 sum = 0;
    for (i = 15; i >= 0; i--) {
        s32 value = a[i] + b[i] + c[i];
        if (value < 0) {
            value = 0;
        }
        out[i] = (s16)value;
        sum += (u32)value;
    }
    return sum;
}

u32 GetRandomByWeight(const u8* weights) {
    u32 value = (Random() >> 24) & 0xffu;
    u32 index = 0;
    while (1) {
        value -= weights[index];
        if ((s32)value < 0) {
            return index;
        }
        index++;
    }
}

u32 CalcCollisionDirectionOLD(u32 direction, u32 collisions) {
    u32 sector = direction >> 3;

    if (sector == 0) {
        if ((collisions & 0x0000000eu) == 0) {
            return direction;
        }
        sector = 0x08;
        if ((collisions & 0x0000e004u) == 0) {
            return sector;
        }
        sector = 0x18;
        if ((collisions & 0x00000e02u) == 0) {
            return sector;
        }
        return direction;
    }

    if (sector == 1) {
        if ((collisions & 0x0000e000u) == 0) {
            return direction;
        }
        sector = 0;
        if ((collisions & 0x0000200eu) == 0) {
            return sector;
        }
        sector = 0x10;
        if ((collisions & 0x000040e0u) == 0) {
            return sector;
        }
        return direction;
    }

    if (sector == 2) {
        if ((collisions & 0x000000e0u) == 0) {
            return direction;
        }
        sector = 0x08;
        if ((collisions & 0x0000e040u) == 0) {
            return sector;
        }
        sector = 0x18;
        if ((collisions & 0x00000e20u) == 0) {
            return sector;
        }
        return direction;
    }

    if ((collisions & 0x00000e00u) == 0) {
        return direction;
    }
    sector = 0;
    if ((collisions & 0x0000020eu) == 0) {
        return sector;
    }
    sector = 0x10;
    if ((collisions & 0x000004e0u) == 0) {
        return sector;
    }
    return direction;
}

void sub_080028E0(Entity* entity) {
    int8_t value = *(int8_t*)((u8*)entity + 0x3d);
    if (value != 0) {
        value += (value > 0) ? -1 : 1;
        *(int8_t*)((u8*)entity + 0x3d) = value;
    }
}

void sub_08007DCE(PlayerEntity* this) {
    DoPlayerAction(this);
}

typedef struct {
    u16 tileType;
    u8 fromLayer;
    u8 toLayer;
} PortTransitionTile;

static const PortTransitionTile sPortTransitionTiles[] = {
    { 0x2A, 3, 3 }, { 0x2D, 3, 3 }, { 0x2B, 3, 3 }, { 0x2C, 3, 3 }, { 0x4C, 3, 3 }, { 0x4E, 3, 3 },
    { 0x4D, 3, 3 }, { 0x4F, 3, 3 }, { 0x0A, 2, 1 }, { 0x09, 2, 1 }, { 0x0C, 1, 2 }, { 0x0B, 1, 2 },
    { 0x52, 3, 3 }, { 0x27, 3, 3 }, { 0x26, 3, 3 },
};

typedef struct {
    u16 tileType;
    u16 hazardType;
} PortHazardTile;

static const PortHazardTile sPortHazardTiles[] = {
    { 0x0d, 1 }, /* pit */
    { 0x10, 2 }, /* water */
    { 0x11, 2 }, /* water */
    { 0x5a, 3 }, /* lava */
    { 0x13, 4 }, /* swamp */
};

void CheckOnLayerTransition(Entity* entity) {
    u32 tileType = GetActTileAtEntity(entity);
    u32 i;

    for (i = 0; i < sizeof(sPortTransitionTiles) / sizeof(sPortTransitionTiles[0]); i++) {
        const PortTransitionTile* tile = &sPortTransitionTiles[i];
        if (tile->tileType != tileType) {
            continue;
        }
        if (entity->collisionLayer != tile->fromLayer) {
            entity->collisionLayer = tile->toLayer;
        }
        return;
    }
}

void UpdateCollisionLayer(Entity* entity) {
    CheckOnLayerTransition(entity);
    UpdateSpriteForCollisionLayer(entity);
}

u32 CheckOnScreen(Entity* entity) {
    s32 dx = (s32)(s16)entity->x.HALF.HI - (s32)gRoomControls.scroll_x + 0x3f;
    if ((u32)dx >= 0x16eu) {
        return 0;
    }

    s32 dy =
        (s32)(s16)entity->y.HALF.HI - (s32)gRoomControls.scroll_y + (s32)(s16)entity->z.HALF.HI + 0x3f;
    if ((u32)dy >= 0x11eu) {
        return 0;
    }

    return 1;
}

void SnapToTile(Entity* entity) {
    entity->x.WORD = (entity->x.WORD & ~0x000fffff) + 0x00080000;
    entity->y.WORD = (entity->y.WORD & ~0x000fffff) + 0x00080000;
}

static void Port_CreateDrownLikeFx(Entity* parent, u32 fxType) {
    Entity* fx = CreateObject(OBJECT_1F, fxType, 0);
    if (fx != NULL) {
        fx->x.HALF.HI = parent->x.HALF.HI;
        fx->y.HALF.HI = parent->y.HALF.HI;
        fx->z.HALF.HI = parent->z.HALF.HI;

        if (parent->kind == ENEMY) {
            fx->type2 = 1;
        }
    }
    DeleteEntity(parent);
}

void CreateDrownFx(Entity* parent) {
    Port_CreateDrownLikeFx(parent, FX_WATER_SPLASH);
}

void CreateLavaDrownFx(Entity* parent) {
    Port_CreateDrownLikeFx(parent, FX_LAVA_SPLASH);
}

void CreateSwampDrownFx(Entity* parent) {
    Port_CreateDrownLikeFx(parent, FX_GREEN_SPLASH);
}

void CreatePitFallFx(Entity* parent) {
    Port_CreateDrownLikeFx(parent, FX_FALL_DOWN);
}

u32 sub_080040A2(Entity* entity) {
    u8 spriteSettings = *(u8*)&entity->spriteSettings;
    if ((spriteSettings & 0x2u) != 0u) {
        return 1;
    }
    return CheckOnScreen(entity);
}

u32 sub_0800442E(Entity* entity) {
    switch (GetTileHazardType(entity)) {
        case 1:
            CreatePitFallFx(entity);
            return 1;
        case 2:
            CreateDrownFx(entity);
            return 1;
        case 3:
            CreateLavaDrownFx(entity);
            return 1;
        case 4:
            CreateSwampDrownFx(entity);
            return 1;
        default:
            return 0;
    }
}

u32 sub_0800445C(Entity* entity) {
    if (!PlayerCanBeMoved()) {
        return 0;
    }

    if (!Port_CalcCollisionStaticEntityRet(entity, &gPlayerEntity.base)) {
        return 0;
    }

    if (gPlayerEntity.base.action == 2) {
        gPlayerEntity.base.subAction = 3;
    }

    return 1;
}

void CalcCollisionStaticEntity(Entity* a, Entity* b) {
    (void)Port_CalcCollisionStaticEntityRet(a, b);
}

extern void (*const gEnemyFunctions[])(Entity*);
extern bool32 ProjectileInit(Entity*);
extern void (*gProjectileFunctions[])(Entity*) __attribute__((weak));
void EnemyUpdate(Entity* entity) {
    Enemy* enemy = (Enemy*)entity;

    if (entity->action == 0) {
        if (!EnemyInit(enemy)) {
            DeleteThisEntity();
        }
    }

    if (!EntityDisabled(entity)) {
        sub_080028E0(entity);
        if ((enemy->enemyFlags & 0x10u) == 0u) {
            gEnemyFunctions[entity->id](entity);
            entity->contactFlags &= 0x7fu;
        }
    }

    DrawEntity(entity);
}

static const s8 sPortVelocities1[16] = { 0, -3, 3, -3, 3, 0, 3, 3, 0, 3, -3, 3, -3, 0, -3, -3 };
static const s8 sPortIceVelocities[16] = { 0, -10, 10, -10, 10, 0, 10, 10, 0, 10, -10, 10, -10, 0, -10, -10 };
static const s8 sPortVelocities3[16] = { 0, 6, -6, 0, 0, -6, 6, 0, 19, 18, 18, 16, 16, 17, 17, 19 };

void ClampPlayerVelocity(PlayerState* state, u32 byteOffset, s32 value) {
    if (value > 0x180) {
        value = 0x180;
    } else if (value < -0x180) {
        value = -0x180;
    }
    *(u16*)(((u8*)state) + byteOffset) = (u16)(s16)value;
}

void AddPlayerVelocity(Entity* entity, s32 vx, s32 vy) {
    (void)entity;
    ClampPlayerVelocity(&gPlayerState, 0x8c, (s16)gPlayerState.vel_x + vx);
    ClampPlayerVelocity(&gPlayerState, 0x8e, (s16)gPlayerState.vel_y + vy);
}

static void Port_AddAndClampPlayerVel(s16 dx, s16 dy) {
    s32 vx = (s16)gPlayerState.vel_x + dx;
    s32 vy = (s16)gPlayerState.vel_y + dy;

    if (vx > 0x180) {
        vx = 0x180;
    } else if (vx < -0x180) {
        vx = -0x180;
    }

    if (vy > 0x180) {
        vy = 0x180;
    } else if (vy < -0x180) {
        vy = -0x180;
    }

    gPlayerState.vel_x = (u16)(s16)vx;
    gPlayerState.vel_y = (u16)(s16)vy;
}

static void Port_DecayPlayerVelAxis(u16* axis) {
    s32 v = (s16)(*axis);
    s32 step = 3;

    if (gPlayerState.jump_status == 0 && gPlayerState.swim_state == 0) {
        step = 3;
    }

    if (v > 0) {
        v -= step;
        if (v < 0) {
            v = 0;
        }
    } else if (v < 0) {
        v += step;
        if (v > 0) {
            v = 0;
        }
    }

    *axis = (u16)(s16)v;
}

void sub_08008A1A(Entity* entity, PlayerState* state, u32 axisByteOffset) {
    s32 step = 3;
    s32 v = (s16)(*(u16*)(((u8*)state) + axisByteOffset));

    (void)entity;
    if (v > 0) {
        v -= step;
        if (v < 0) {
            v = 0;
        }
    } else if (v < 0) {
        v += step;
        if (v > 0) {
            v = 0;
        }
    }
    *(u16*)(((u8*)state) + axisByteOffset) = (u16)(s16)v;
}

void sub_08008942(Entity* entity) {
    if ((gPlayerState.field_0x7 | gPlayerState.field_0xa) != 0) {
        return;
    }
    entity->direction = gPlayerState.direction;
    if ((entity->direction & 0x80u) != 0u) {
        return;
    }
    UpdateIcePlayerVelocity(entity);
}

void sub_08008926(Entity* entity) {
    sub_08008942(entity);
}

u32 sub_080086D8(u32 x, u32 y, const u8* table) {
    return Port_sub_080086D8(&gPlayerEntity.base, x, y, table);
}

u32 sub_080086B4(u32 x, u32 y, const u8* table) {
    return sub_080086D8(x, y, table);
}

u16* DoTileInteraction(Entity* entity, u32 filter, u32 x, u32 y) {
    const KeyValuePair* match;
    const u16* entry;
    u32 tileType;
    u32 tilePos;
    u32 layer;
    Entity* object;
    u16 replacement;
    MapLayer* mapLayer;

    if (*(u16*)&gRoomControls == 1) {
        return NULL;
    }

    layer = entity->collisionLayer;
    tileType = GetTileTypeAtWorldCoords((s32)x, (s32)y, layer);
    if (gUnk_080046A4 == NULL || gUnk_080047F6 == NULL) {
        return NULL;
    }

    match = FindEntryForKey(tileType, gUnk_080046A4);
    if (match == NULL) {
        return NULL;
    }

    entry = &gUnk_080047F6[match->value << 2];
    if (((entry[0] >> filter) & 1u) == 0u) {
        return NULL;
    }

    {
        const u8* entryBytes = (const u8*)entry;
        u32 objectId = entryBytes[2];
        u32 objectType = entryBytes[3];
        if (objectId != 0xff && filter != 6 && filter != 0xe && filter != 0xa && filter != 0xb &&
            !(filter == 0xd && objectId == 0xf && objectType == 0x17)) {
            u32 type2 = (objectId == 0xf) ? 0x80u : 0u;
            object = CreateObject(objectId, objectType, type2);
            if (object != NULL) {
                if (objectId != 0) {
                    object->x.HALF.HI = (u16)((x & ~0xfu) + 8u);
                    object->y.HALF.HI = (u16)((y & ~0xfu) + 8u);
                } else {
                    object->x.HALF.HI = entity->x.HALF.HI;
                    object->y.HALF.HI = entity->y.HALF.HI;
                    object->z.HALF.HI = entity->z.HALF.HI;
                }
                object->parent = entity;
                object->collisionLayer = entity->collisionLayer;
                UpdateSpriteForCollisionLayer(object);
            }
        }
    }

    tilePos = (((x - gRoomControls.scroll_x) >> 4) & 0x3fu) + ((((y - gRoomControls.scroll_y) >> 4) & 0x3fu) << 6);
    replacement = entry[3];
    if ((replacement & 0x4000u) == 0) {
        sub_0807B7D8(replacement, tilePos, layer);
    } else if (replacement == 0xffffu) {
        RestorePrevTileEntity(tilePos, layer);
    } else {
        mapLayer = GetLayerByIndex(layer);
        mapLayer->mapData[tilePos] = replacement;
    }

    return (u16*)entry;
}

void sub_080085CC(Entity* entity) {
    const u8* table = Port_TableOrFallback(gUnk_080083FC, gUnk_080082DC);
    s32 x = (s16)entity->x.HALF.HI - (s32)gRoomControls.scroll_x;
    s32 y = (s16)entity->y.HALF.HI - (s32)gRoomControls.scroll_y;
    s32 baseX;
    s32 baseY;
    s32 sx;
    s32 sy;
    s32 step;
    u32 collisions = 0;
    u32 i;
    u8* hitbox;
    u8* playerStateBytes = (u8*)&gPlayerState;

    if (gPlayerState.swim_state != 0) {
        if ((gPlayerState.flags & 0x80u) != 0u) {
            table = Port_TableOrFallback(gUnk_0800839C, table);
        }
    } else {
        table = Port_TableOrFallback(gUnk_0800845C, table);
        if (gPlayerState.jump_status == 0 && (gPlayerState.flags & 0x01000000u) == 0u) {
            table = Port_TableOrFallback(gUnk_0800833C, table);
            if ((gPlayerState.flags & 0x80u) == 0u) {
                table = Port_TableOrFallback(gUnk_080084BC, table);
                if (gPlayerState.gustJarState == 0 && gPlayerState.heldObject == 0) {
                    table = Port_TableOrFallback(gUnk_0800851C, table);
                    if (playerStateBytes[0xaa] == 0) {
                        table = Port_TableOrFallback(gUnk_080082DC, table);
                    }
                }
            }
        }
    }

    hitbox = (u8*)entity->hitbox;
    baseX = x + (s8)hitbox[0];
    baseY = y + (s8)hitbox[1];
    sx = baseX;
    sy = baseY;

    x = baseX + hitbox[2];
    step = hitbox[3];
    for (i = 0; i < 2; i++) {
        collisions <<= 2;
        y += step;
        collisions |= sub_080086D8((u32)x, (u32)y, table);
        collisions <<= 1;
        collisions |= sub_080086D8((u32)x, (u32)(sy - step), table);
        collisions <<= 1;
        if (i == 0) {
            x -= hitbox[2] * 2;
            y = sy;
        }
    }

    x = sx;
    y = baseY + hitbox[5];
    step = hitbox[4];
    for (i = 0; i < 2; i++) {
        collisions <<= 2;
        x += step;
        collisions |= sub_080086D8((u32)x, (u32)y, table);
        collisions <<= 1;
        collisions |= sub_080086D8((u32)(sx - step), (u32)y, table);
        collisions <<= 1;
        if (i == 0) {
            y -= hitbox[5] * 2;
            x = sx;
        }
    }

    entity->collisions = (u16)collisions;
}

void sub_080085B0(Entity* entity) {
    sub_080085CC(entity);
}

u16* DoTileInteractionOffset(Entity* entity, u32 filter, s32 xOffset, s32 yOffset) {
    u32 x = (u16)entity->x.HALF.HI + (s32)xOffset;
    u32 y = (u16)entity->y.HALF.HI + (s32)yOffset;
    return DoTileInteraction(entity, filter, x, y);
}

u32* DoTileInteractionHere(Entity* entity, u32 filter) {
    return (u32*)DoTileInteraction(entity, filter, (u16)entity->x.HALF.HI, (u16)entity->y.HALF.HI);
}

s32 DoItemTileInteraction(Entity* entity, u32 filter, ItemBehavior* itemBehavior) {
    u32 animationOffset = entity->animationState & 6u;
    Entity* interaction = (Entity*)DoTileInteractionOffset(entity, filter, (s8)gUnk_08007DF4[animationOffset],
                                                           (s8)gUnk_08007DF4[animationOffset + 1]);
    if (interaction != NULL) {
        u8* itemBytes = (u8*)itemBehavior;
        itemBytes[3] = ((u8*)interaction)[2];
        itemBytes[7] = ((u8*)interaction)[3];
        itemBytes[8] = ((u8*)interaction)[5];
    }
    return interaction != NULL;
}

void sub_0800857C(Entity* entity) {
    if ((((u8*)entity)[0xb] & 0x80u) == 0u && (gPlayerState.jump_status & 0x80u) == 0u) {
        sub_080085CC(entity);
    }
    LinearMoveDirectionOLD(entity, entity->speed, entity->direction);
}

void sub_08008AA0(Entity* entity) {
    u8* playerStateBytes = (u8*)&gPlayerState;
    u32 direction;
    (void)entity;

    if (playerStateBytes[0x12] == 1) {
        return;
    }
    direction = playerStateBytes[0xd];
    if (direction == 0xffu) {
        return;
    }
    direction <<= 3;
    gPlayerState.vel_x = (u16)gSineTable[direction];
    gPlayerState.vel_y = (u16)-gSineTable[direction + 0x40];
}

void sub_08008AC6(Entity* entity) {
    u8* playerStateBytes = (u8*)&gPlayerState;
    u32 respawnSide;

    if ((playerStateBytes[0x26] & 0x0fu) != 0u) {
        return;
    }
    if ((gPlayerState.flags & 0x20u) != 0u) {
        return;
    }

    respawnSide = GetNonCollidedSide(entity);
    if (respawnSide == 0u) {
        return;
    }

    *(u8*)(((u8*)entity) + 0x3d) = 0xe2;
    RespawnPlayer();
}

void UpdateIcePlayerVelocity(Entity* entity) {
    u32 baseDir = ((u32)entity->animationState >> 1u) << 3u;
    u32 dirMode = baseDir;
    const s8* table = sPortIceVelocities;

    entity->direction = (u8)baseDir;
    if ((baseDir & 0x80u) != 0u) {
        return;
    }

    if (gPlayerState.heldObject == 1 || gPlayerState.heldObject == 2) {
        ResetPlayerVelocity();
        return;
    }

    if (gPlayerState.jump_status != 0) {
        u32 heading = ((u32)entity->animationState >> 1u) << 1u;
        u32 mix = ((baseDir >> 2u) - heading + 2u) & 7u;
        if (mix >= 4u) {
            table = sPortVelocities3 + heading;
        } else {
            table = sPortVelocities1;
        }
    }

    {
        u32 idx = (dirMode >> 2u) << 1u;
        Port_AddAndClampPlayerVel(table[idx], table[idx + 1]);
    }

    if ((s16)gPlayerState.vel_x != 0) {
        u32 moveDir = ((s16)gPlayerState.vel_x > 0) ? 8u : 0x18u;
        LinearMoveDirectionOLD(entity, (u16)((s16)gPlayerState.vel_x > 0 ? (s16)gPlayerState.vel_x : -(s16)gPlayerState.vel_x),
                               moveDir);
        sub_0807A5B8(moveDir);
    }

    if ((s16)gPlayerState.vel_y != 0) {
        u32 moveDir = ((s16)gPlayerState.vel_y > 0) ? 0x10u : 0u;
        LinearMoveDirectionOLD(entity, (u16)((s16)gPlayerState.vel_y > 0 ? (s16)gPlayerState.vel_y : -(s16)gPlayerState.vel_y),
                               moveDir);
        sub_0807A5B8(moveDir);
    }

    if (gPlayerState.jump_status == 0) {
        Port_DecayPlayerVelAxis(&gPlayerState.vel_x);
        Port_DecayPlayerVelAxis(&gPlayerState.vel_y);
    }
}

void ProjectileUpdate(Entity* entity) {
    if (entity->action == 0) {
        if (!ProjectileInit(entity)) {
            DeleteThisEntity();
        }
    }

    if (!EntityDisabled(entity)) {
        sub_080028E0(entity);
        if (gProjectileFunctions != NULL) {
            gProjectileFunctions[entity->id](entity);
        } else {
            DeleteThisEntity();
        }
        entity->contactFlags &= 0x7fu;
    }

    DrawEntity(entity);
}

void sub_080044AE(Entity* entity, u32 speed, u32 direction) {
    if (entity == &gPlayerEntity.base) {
        sub_08079E58((s32)speed, direction);
        return;
    }

    CalculateEntityTileCollisions(entity, direction, 2);
    ProcessMovementInternal(entity, (s32)speed, (s32)direction, 2);
}

u32 BounceUpdate(Entity* entity, u32 acceleration) {
    s32 z = entity->z.WORD;
    s32 velocity = entity->zVelocity;
    z -= velocity;
    if (z < 0) {
        entity->z.WORD = z;
        entity->zVelocity = velocity - (s32)acceleration;
        return 2;
    }

    entity->z.WORD = 1;
    velocity = (velocity - (s32)acceleration);
    velocity = (s32)((u32)(-velocity) >> 1);
    velocity += (s32)((u32)velocity >> 2);
    if (((u32)velocity >> 12) < 0x0c) {
        velocity = 0;
        entity->zVelocity = velocity;
        return 0;
    }

    entity->zVelocity = velocity;
    return 1;
}

void sub_08004542(Entity* entity) {
    uint8_t* sr = (uint8_t*)&entity->spriteRendering;
    uint8_t* so = (uint8_t*)&entity->spriteOrientation;
    entity->collisionLayer = 2;
    *so = (uint8_t)((*so & ~0xc0u) | 0x40u);
    *sr = (uint8_t)((*sr & ~0xc0u) | 0x40u);
}

void sub_0800451C(Entity* entity) {
    u32 actTile = GetActTileAtEntity(entity);
    switch (actTile) {
        case 0x0b:
        case 0x0c:
            sub_08004542(entity);
            break;
        case 0x09:
        case 0x0a:
            ResetCollisionLayer(entity);
            break;
        case 0x26:
        case 0x27: {
            uint8_t* sr = (uint8_t*)&entity->spriteRendering;
            uint8_t* so = (uint8_t*)&entity->spriteOrientation;
            entity->collisionLayer = 3;
            *so = (uint8_t)((*so & ~0xc0u) | 0x40u);
            *sr = (uint8_t)((*sr & ~0xc0u) | 0x40u);
            break;
        }
        default:
            break;
    }
}

void GenericKnockback(Entity* entity) {
    Knockback1(entity);
}

void GenericKnockback2(Entity* entity) {
    Knockback2(entity);
}

extern const KeyValuePair gMapActTileToSurfaceType[];
u32 GetNonCollidedSide(Entity* entity) {
    u32 collisions = entity->collisions;
    u32 side = 4;
    u32 mask = 0x0e;

    while (side != 0) {
        if ((collisions & mask) == 0) {
            return side;
        }
        collisions >>= 4;
        side--;
    }

    return 1;
}

u32 CheckNEastTile(Entity* entity) {
    u32 tileType = GetActTileRelativeToEntity(entity, 0, 0);
    const KeyValuePair* entry;

    if ((tileType & 0x4000u) != 0u) {
        return 0;
    }

    entry = FindEntryForKey(tileType, gMapActTileToSurfaceType);
    if (entry == NULL) {
        return 0;
    }
    return entry->value == 1;
}

u32 PlayerCheckNEastTile(void) {
    return CheckNEastTile(&gPlayerEntity.base);
}

void sub_08001318(Entity* entity) {
    if ((s16)entity->z.HALF.HI < 0) {
        entity->direction = 0xff;
    }
    GenericKnockback(entity);
}

u32 sub_0800132C(Entity* a, Entity* b) {
    if ((a->collisionLayer & b->collisionLayer) == 0) {
        return 0xff;
    }
    if ((u32)((u16)a->x.HALF.HI - (u16)b->x.HALF.HI + 8) >= 0x11u) {
        return 0xff;
    }
    if ((u32)((u16)a->y.HALF.HI - (u16)b->y.HALF.HI + 8) >= 0x11u) {
        return 0xff;
    }
    return GetFacingDirection(a, b);
}

void sub_08001214(Entity* entity) {
    if ((entity->gustJarState & 1u) == 0u) {
        entity->gustJarState = 1;
        entity->timer = ((entity->frame & 0x80u) != 0u) ? 0x20u : 1u;
    }

    entity->timer--;
    if (entity->timer == 0) {
        CreatePitFallFx(entity);
        return;
    }

    UpdateAnimationVariableFrames(entity, 4);
}

static void (*const sPortEnemyHazardFns[])(Entity*) = {
    NULL,
    sub_08001214,
    CreateDrownFx,
    CreateLavaDrownFx,
    CreateSwampDrownFx,
};

void sub_08001290(Entity* entity, u32 index) {
    if (index != 0 && index < (sizeof(sPortEnemyHazardFns) / sizeof(sPortEnemyHazardFns[0])) &&
        sPortEnemyHazardFns[index] != NULL) {
        sPortEnemyHazardFns[index](entity);
    }
}

s32 sub_080012DC(Entity* entity) {
    u32 hazard;

    if ((entity->gustJarState >> 3) != 0u) {
        return 0;
    }

    hazard = GetTileHazardType(entity);
    if (hazard == 4u) {
        return 0;
    }

    if (hazard == 0u) {
        if ((entity->gustJarState & 1u) != 0u) {
            entity->flags |= 0x80u;
        }
        return 0;
    }

    if (hazard != 1u) {
        entity->timer = 1;
        entity->gustJarState |= 1u;
    }

    return (s32)hazard;
}

void EnemyFunctionHandler(Entity* entity, EntityActionArray funcs) {
    u32 index;
    EntityAction* fn;

    if (sub_080012DC(entity) != 0) {
        index = entity->gustJarState >> 5;
        fn = sPortEnemyHazardFns[index];
    } else {
        index = GetNextFunction(entity);
        fn = funcs[index];
    }
    fn(entity);
}

u32 GetTileHazardType(Entity* entity) {
    u32 i;
    u32 actTile;

    if (entity->action == 0) {
        return 0;
    }

    UpdateCollisionLayer(entity);
    if ((s16)entity->z.HALF.HI < 0) {
        return 0;
    }

    actTile = GetActTileAtEntity(entity);
    if (actTile == 0) {
        return 0;
    }

    for (i = 0; i < sizeof(sPortHazardTiles) / sizeof(sPortHazardTiles[0]); i++) {
        if (sPortHazardTiles[i].tileType == actTile) {
            return sPortHazardTiles[i].hazardType;
        }
    }

    return 0;
}

void GenericConfused(Entity* entity) {
    static const s8 kConfusedOffsets[4] = { 0, 1, 0, -1 };

    entity->confusedTime--;
    if (entity->confusedTime < 0x3c) {
        entity->spriteOffsetX = (u8)kConfusedOffsets[entity->confusedTime & 3u];
        if (entity->confusedTime == 0) {
            Entity* fx = ((Enemy*)entity)->child;
            if (fx != NULL && fx->kind == OBJECT && fx->id == 0x0f && fx->type == 0x1c) {
                EnemyDetachFX(entity);
            }
        }
    }

    GravityUpdate(entity, 0x1800);
}

u32 GetNextFunction(Entity* entity) {
    u32 gustJarState = entity->gustJarState;
    if ((gustJarState & 0x4u) == 0u && ((entity->contactFlags >> 7) == 0u)) {
        return 1;
    }

    if (entity->knockbackDuration != 0) {
        return 2;
    }

    if (entity->health == 0) {
        if ((entity->action | entity->subAction) != 0) {
            return 3;
        }
        return 0;
    }

    if ((gustJarState & 0x4u) != 0u) {
        return 5;
    }

    if (entity->confusedTime != 0) {
        return 4;
    }

    return 0;
}

u32 GetNextScriptCommandHalfword(u16* script) {
    return script[0];
}

u32 GetNextScriptCommandHalfwordAfterCommandMetadata(u16* script) {
    return script[1];
}

u32 GetNextScriptCommandWord(u16* script) {
    return ((u32)script[0]) | ((u32)script[1] << 16);
}

u32 GetNextScriptCommandWordAfterCommandMetadata(u16* script) {
    return ((u32)script[1]) | ((u32)script[2] << 16);
}

u32 FindValueForKey(u32 key, const KeyValuePair* keyValuePairList) {
    const KeyValuePair* p = keyValuePairList;
    if (p == NULL) {
        return 0;
    }
    while (p->key != 0) {
        if (p->key == key) {
            return p->value;
        }
        p++;
    }
    return 0;
}

const KeyValuePair* FindEntryForKey(u32 key, const KeyValuePair* keyValuePairList) {
    const KeyValuePair* p = keyValuePairList;
    if (p == NULL) {
        return NULL;
    }
    while (p->key != 0) {
        if (p->key == key) {
            return p;
        }
        p++;
    }
    return NULL;
}

u32 CheckRectOnScreen(s32 roomX, s32 roomY, u32 width, u32 height) {
    s32 cameraRoomX = gRoomControls.scroll_x - gRoomControls.origin_x;
    s32 cameraRoomY = gRoomControls.scroll_y - gRoomControls.origin_y;
    s32 left = roomX;
    s32 top = roomY;
    s32 right = left + (s32)width;
    s32 bottom = top + (s32)height;

    if (right <= cameraRoomX || left >= cameraRoomX + 240) {
        return 0;
    }
    if (bottom <= cameraRoomY || top >= cameraRoomY + 160) {
        return 0;
    }
    return 1;
}

u32 CalculateDirectionTo(u32 x1, u32 y1, u32 x2, u32 y2) {
    s32 dx = (s32)x2 - (s32)x1;
    s32 dy = (s32)y2 - (s32)y1;
    return CalculateDirectionFromOffsets(dx, dy);
}

u32 GetFacingDirection(Entity* from, Entity* to) {
    return CalculateDirectionTo((u32)(u16)from->x.HALF.HI, (u32)(u16)from->y.HALF.HI, (u32)(u16)to->x.HALF.HI,
                                (u32)(u16)to->y.HALF.HI);
}

u32 CheckPlayerInRegion(u32 centerX, u32 centerY, u32 radiusX, u32 radiusY) {
    u32 playerRoomX = (u32)((u16)gPlayerEntity.base.x.HALF.HI - (u16)gRoomControls.origin_x);
    u32 playerRoomY = (u32)((u16)gPlayerEntity.base.y.HALF.HI - (u16)gRoomControls.origin_y);
    u32 dx = centerX - (playerRoomX - radiusX);
    u32 dy = centerY - (playerRoomY - radiusY);
    if (dx >= (radiusX << 1)) {
        return 0;
    }
    if (dy >= (radiusY << 1)) {
        return 0;
    }
    return 1;
}

void sub_08004596(Entity* entity, u32 direction) {
    u32 current = entity->direction;
    if (current < 0x20) {
        s32 diff = (s32)direction - (s32)current;
        if (diff != 0) {
            u32 step = ((u32)diff & 0x1f) < 0x10 ? 1u : (u32)-1;
            direction = current + step;
        }
    }
    entity->direction = (u8)(direction & 0x1f);
}

u32 sub_080045B4(Entity* entity, u32 x, u32 y) {
    return CalculateDirectionTo((u32)(u16)entity->x.HALF.HI, (u32)(u16)entity->y.HALF.HI, x, y);
}

void ResetCollisionLayer(Entity* entity) {
    uint8_t* sr = (uint8_t*)&entity->spriteRendering;
    uint8_t* so = (uint8_t*)&entity->spriteOrientation;
    entity->collisionLayer = 1;
    *sr = (uint8_t)((*sr & ~0xc0u) | 0x80u);
    *so = (uint8_t)((*so & ~0xc0u) | 0x80u);
}

static u32 Port_Sub_080040F2_Impl(Entity* entity, const u8* table, u32 worldX, u32 worldY) {
    MapLayer* mapLayer;
    u32 layer = entity->collisionLayer;
    u32 xRel = ((u32)((u16)worldX - (u16)gRoomControls.origin_x)) & 0x3f0u;
    u32 yRel = ((u32)((u16)worldY - (u16)gRoomControls.origin_y)) & 0x3f0u;
    u32 tilePos = (xRel >> 4u) + (yRel << 2u);
    u32 value;

    if (tilePos >= (MAX_MAP_SIZE * MAX_MAP_SIZE)) {
        return 0;
    }
    mapLayer = GetLayerByIndex(layer);
    value = mapLayer->collisionData[tilePos];

    if (value < 0x10u) {
        if ((yRel >> 4u) & 1u) {
            value >>= 2u;
        }
        if ((xRel >> 4u) & 1u) {
            value >>= 1u;
        }
    } else if (value != 0xffu) {
        u16 mask = (u16)table[(value - 0x10u) * 2u] | (u16)((u16)table[(value - 0x10u) * 2u + 1u] << 8u);
        u32 xNibble = xRel & 0x0fu;
        u32 yNibble = yRel & 0x0fu;
        if (xNibble >= 4u) {
            mask >>= 1u;
            if (xNibble >= 8u) {
                mask >>= 1u;
                if (xNibble >= 12u) {
                    mask >>= 1u;
                }
            }
        }
        if (yNibble >= 4u) {
            mask >>= 4u;
            if (yNibble >= 8u) {
                mask >>= 4u;
                if (yNibble >= 12u) {
                    mask >>= 4u;
                }
            }
        }
        value = mask;
    }

    return value & 1u;
}

u32 sub_080040D8(Entity* entity, const u8* table, u32 worldX, u32 worldY) {
    return Port_Sub_080040F2_Impl(entity, table, worldX, worldY);
}

u32 sub_080040E2(Entity* entity, const u8* table) {
    return Port_Sub_080040F2_Impl(entity, table, (u16)entity->x.HALF.HI, (u16)entity->y.HALF.HI);
}

u32 sub_080040EC(Entity* entity, const u8* table) {
    return Port_Sub_080040F2_Impl(entity, table, (u16)entity->x.HALF.HI, (u16)entity->y.HALF.HI);
}

void sub_0800417E(Entity* entity, u32 collisions) {
    u32 dir = entity->direction;
    if ((collisions & 0x0000ee00u) != 0) {
        dir = 0x20u - dir;
    }
    if ((collisions & 0x000000eeu) != 0) {
        dir = 0x10u - dir;
    }
    entity->direction = (u8)(dir & 0x1fu);
}

u32 sub_080041DC(Entity* entity, u32 x, u32 y) {
    s32 dx = (s32)(u16)entity->x.HALF.HI - (s32)(u16)x;
    s32 dy = (s32)(u16)entity->y.HALF.HI - (s32)(u16)y;
    return CalcDistance(dx, dy);
}

u32 sub_08004212(Entity* entity, u32 direction, u32 tilePos) {
    MapLayer* mapLayer;
    u32 d = direction;
    u32 t = tilePos;
    u32 tileIndex;

    if ((d & 3u) != 0u) {
        t += (d & 4u) ? (u32)-2 : 2u;
    }

    if ((d & 3u) != 2u) {
        d += 1u;
        t += (d & 4u) ? 0x80u : (u32)-0x80;
    }

    t &= 0x1fffu;
    mapLayer = GetLayerByIndex(entity->collisionLayer);
    tileIndex = mapLayer->mapData[t];
    if ((tileIndex & 0x4000u) == 0u) {
        if (tileIndex < TILESET_SIZE) {
            return mapLayer->tileTypes[tileIndex];
        }
        return 0;
    }
    return tileIndex;
}

u32 sub_08004202(Entity* entity, void* outTileType, u32 tilePos) {
    u32 tileType = sub_08004212(entity, entity->animationState, tilePos);
    if (outTileType != NULL) {
        *(u32*)outTileType = tileType;
    }
    return tilePos;
}

void sub_080042D0(Entity* entity, u32 frameIndex, u16 spriteIndex) {
    const SpritePtr* spritePtr;
    SpriteFrame* frames;
    SpriteFrame* frame;
    GfxSlot* slot;
    uint8_t oldCount;
    const void* oldPtr;
    const void* newPtr;

    if (frameIndex == 0xffu) {
        return;
    }

    spritePtr = &gSpritePtrs[spriteIndex];
    frames = spritePtr->frames;
    if (frames == NULL) {
        return;
    }
    frame = &frames[frameIndex];
    if (frame->numTiles == 0) {
        return;
    }

    slot = &gGFXSlots.slots[entity->spriteAnimation[0]];
    if (slot->status < GFX_SLOT_GFX) {
        return;
    }

    oldCount = (uint8_t)slot->paletteIndex;
    slot->paletteIndex = (slot->paletteIndex & 0xff00u) | frame->numTiles;
    oldPtr = slot->palettePointer;
    newPtr = (const uint8_t*)spritePtr->ptr + ((u32)frame->firstTileIndex << 5);
    slot->palettePointer = newPtr;

    if (oldCount != frame->numTiles || oldPtr != newPtr) {
        slot->vramStatus = GFX_VRAM_3;
    }
}

void sub_080042BA(Entity* entity, u32 frameStep) {
    uint8_t frame;
    uint8_t lastFrame;

    UpdateAnimationVariableFrames(entity, frameStep);
    frame = entity->frameIndex;
    lastFrame = entity->lastFrameIndex;
    entity->lastFrameIndex = frame;
    if (frame != lastFrame) {
        sub_080042D0(entity, frame, entity->spriteIndex);
    }
}

u32 sub_0800419C(Entity* a, Entity* b, u32 radiusX, u32 radiusY) {
    if (radiusX != 0) {
        u32 dx = (u32)(u16)a->x.HALF.HI - (u32)(u16)b->x.HALF.HI + radiusX;
        if ((radiusX << 1) < dx) {
            return 0;
        }
    }
    if (radiusY != 0) {
        u32 dy = (u32)(u16)a->y.HALF.HI - (u32)(u16)b->y.HALF.HI + radiusY;
        if ((radiusY << 1) < dy) {
            return 0;
        }
    }
    return 1;
}

u32 EntityInRectRadius(Entity* a, Entity* b, u32 radiusX, u32 radiusY) {
    if (((a->collisionLayer & b->collisionLayer) & 3u) == 0u) {
        return 0;
    }
    return sub_0800419C(a, b, radiusX, radiusY);
}

u32 sub_080041E8(s32 x1, s32 y1, s32 x2, s32 y2) {
    return CalcDistance(x1 - x2, y1 - y2);
}
