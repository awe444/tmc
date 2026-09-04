#include "global.h"
#include "main.h"
#include "room.h"
#include "screen.h"
#include "structures.h"

#include <string.h>

#ifdef PC_PORT
#include "port_rom.h"
extern u32 gFrameObjLists[50016];
#else
extern u32 gFrameObjLists[];
#endif

extern void ram_DrawEntities(void);
extern void ram_sub_080ADA04(OAMCommand*, void*);
extern void ram_DrawDirect(OAMCommand*, u32, u32);

void* sub_080AD8F0(u32 sprite, u32 frame) {
#ifdef PC_PORT
    const size_t frameObjSize = sizeof(gFrameObjLists);
    const u8* base = (const u8*)gFrameObjLists;
    size_t frameEntryOffset;
    u32 frameTableOffset;
    u32 frameDataOffset;

    if ((size_t)sprite >= (frameObjSize / sizeof(u32))) {
        return NULL;
    }

    frameTableOffset = gFrameObjLists[sprite];
    if ((size_t)frameTableOffset > frameObjSize - sizeof(u32)) {
        return NULL;
    }

    frameEntryOffset = (size_t)frameTableOffset + (size_t)frame * sizeof(u32);
    if (frameEntryOffset > frameObjSize - sizeof(u32)) {
        return NULL;
    }

    frameDataOffset = Port_ReadU32(base + frameEntryOffset);
    if ((size_t)frameDataOffset >= frameObjSize) {
        return NULL;
    }

    return (void*)(base + frameDataOffset);
#else
    u32* temp = &gFrameObjLists[0];
    u32 x = gFrameObjLists[sprite];
    temp = (u32*)((uintptr_t)(((u32*)((uintptr_t)temp + x))[frame]) + (uintptr_t)temp);

    return temp;
#endif
}

void FlushSprites(void) {
    gOAMControls.updated = 0;
}

void CopyOAM(void) {
    u16* d;
    s32 rem;

    if (gMain.pad == 0) {
        gOAMControls.unk[0x20].unk0 = 0;
        gOAMControls.unk[0x48].unk4 = 0;
        gOAMControls.unk[0x71].unk0 = 0;
        gOAMControls.unk[0x99].unk4 = 0;
    } else {
        gMain.pad--;
    }

    rem = 0x80 - gOAMControls.updated;
    if (rem > 0) {
        d = (u16*)&gOAMControls.oam[gOAMControls.updated];
        for (; rem != 0; rem--) {
            *d = 0x2A0;
            d = (u16*)((u8*)d + 8);
        }
    }
    if (gOAMControls.unk[0].unk7) {
        gOAMControls.unk[0].unk7 = 0;
#ifdef PC_PORT
        struct ObjAffineSrcData affineSrc[32];
        s16 keep[32][4];
        u32 i;
        u32 k;

        for (i = 0; i < ARRAY_COUNT(affineSrc); i++) {
            memcpy(&affineSrc[i], &gOAMControls.unk[i].unk0, sizeof(affineSrc[i]));
        }

        /* A slot whose source is still all zero has not been written since
         * Subtask_Init's MemClear(gOAMControls.unk, 0x100), which wipes all 32
         * affine sources when a menu opens. The flag that triggers this
         * recompute is global — ui.c raises it for slot 0 alone — so one UI
         * write recomputes every slot, and a slot nobody has rewritten yet
         * gets a matrix derived from zero scale. That is a degenerate matrix:
         * every screen pixel of the sprite samples one texel, which draws as a
         * flat block over the sprite's whole bounding box.
         *
         * It shows because the port draws the world for several frames after a
         * menu before the entity that owns the sprite has updated once — the
         * lily pad's first SetAffineInfo lands 8 frames after it starts being
         * drawn, so it spent those frames as a solid green square (B62). Two
         * mGBA savestates say hardware never presents that state: slot 2 holds
         * a live matrix both during the menu ([275,20,-20,275]) and at the
         * black frame as the world returns ([285,21,-21,285]).
         *
         * Keep the previous matrix for such a slot rather than deriving a new
         * one from nothing. The owning entity overwrites it the moment it runs,
         * so this only ever covers the gap. */
        for (k = 0; k < ARRAY_COUNT(affineSrc); k++) {
            if (affineSrc[k].xScale == 0 && affineSrc[k].yScale == 0 && affineSrc[k].rotation == 0) {
                keep[k][0] = gOAMControls.oam[k * 4 + 0].affineParam;
                keep[k][1] = gOAMControls.oam[k * 4 + 1].affineParam;
                keep[k][2] = gOAMControls.oam[k * 4 + 2].affineParam;
                keep[k][3] = gOAMControls.oam[k * 4 + 3].affineParam;
            } else {
                keep[k][0] = keep[k][1] = keep[k][2] = keep[k][3] = 0;
            }
        }

        ObjAffineSet(affineSrc, &gOAMControls.oam[0].affineParam, 32, 8);

        for (k = 0; k < ARRAY_COUNT(affineSrc); k++) {
            if (affineSrc[k].xScale == 0 && affineSrc[k].yScale == 0 && affineSrc[k].rotation == 0) {
                gOAMControls.oam[k * 4 + 0].affineParam = keep[k][0];
                gOAMControls.oam[k * 4 + 1].affineParam = keep[k][1];
                gOAMControls.oam[k * 4 + 2].affineParam = keep[k][2];
                gOAMControls.oam[k * 4 + 3].affineParam = keep[k][3];
            }
        }
#else
        ObjAffineSet((struct ObjAffineSrcData*)gOAMControls.unk, &gOAMControls.oam[0].affineParam, 32, 8);
#endif
    }
    gOAMControls.field_0x0 = 1;
}

void DrawEntities(void) {
    void (*fn)(void);

    gOAMControls._0[6] = gRoomTransition.field2f ? 15 : 0;
    gOAMControls._4 = gRoomControls.aff_x + gRoomControls.scroll_x;
    gOAMControls._6 = gRoomControls.aff_y + gRoomControls.scroll_y;
    gOAMControls.field_0x1++;

    fn = &ram_DrawEntities;
    fn();
}

// TODO second parameter is a frame obj entry from gFrameObjLists
void sub_080ADA04(OAMCommand* cmd, void* dst) {
    void (*fn)(OAMCommand*, void*) = ram_sub_080ADA04;
    fn(cmd, dst);
}

void DrawDirect(u32 spriteIndex, u32 frameIndex) {
    void (*fn)(OAMCommand*, u32, u32) = ram_DrawDirect;
    fn(&gOamCmd, spriteIndex, frameIndex);
}
