#include "asm.h"
#include "map.h"

#include "player.h"
#include "room.h"
#include "structures.h"
#include "vram.h"

#include <stddef.h>
#include <stdint.h>

u32 CalcDistance(s32 x, s32 y);

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
