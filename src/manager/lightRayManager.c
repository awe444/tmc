/**
 * @file lightRayManager.c
 * @ingroup Managers
 *
 * @brief Creates and updates light rays.
 *
 * Light rays at the left of minish woods
 * Light rays in the barrel minish house
 */
#include "manager/lightRayManager.h"
#include "common.h"
#include "main.h"
#include "screen.h"
#include "game.h"
#include "player.h"
#include "physics.h"
#include "viewport.h"
#ifdef PC_PORT
#include <stdio.h>
#include "port_mapsource.h"
#endif

extern void DisableVBlankDMA(void);

void (*const LightRayManager_Actions[])(LightRayManager*);
void (*const gUnk_08107C48[])(LightRayManager*);
const u16 gUnk_08107C1C[];
const u16 gUnk_08107C30[];
const u8 gLightRayManagerGfxGroups[];

extern u16 gUnk_02017AA0[];
extern u16 gUnk_085B4180[];

#define LIGHT_RAY_ACTION_COUNT 4
#define LIGHT_RAY_STATE_COUNT 5
#define LIGHT_RAY_GFX_GROUP_COUNT 8

typedef struct {
    u8 unk0;
    u8 unk1;
    u8 unk2;
    u8 unk3;
    u8 unk4;
} PACKED LightRayManagerProp;

#define ZS(this) ((u8*)&this->speed)

/* How far the camera moves between re-bases of the light shaft's layer, and
 * the vertical twin of B32 in MinishPaths — the same mechanism, the same
 * arithmetic, a different manager.
 *
 * sub_08057450 scrolls BG3 by keeping a fine offset in `yOffset` and
 * re-pointing `subTileMap` a whole 64 px at a time. The block it points into
 * is 32 tiles, so the window it has to cover is `yOffset + screen height` and
 * the fine offset may only range over `256 - height` before the bottom of the
 * screen wraps to the top of the block.
 *
 * At 160 rows that is 96 and the engine's 64 fits. At 240 it is 16, and for
 * 47 of every 64 camera positions the bottom of the shaft showed the top of
 * the block instead — up to 47 rows of it, the ray band breaking and a
 * disconnected fragment appearing beneath the break. Re-basing on a smaller
 * step keeps the whole screen inside the block.
 *
 * The scroll position is unchanged either way: 64*(y/64) + (y&63) and
 * 16*(y/16) + (y&15) are both y. Only how often the map pointer moves
 * differs, and the pointer stays inside gBG3Buffer at both steps — y reaches
 * 192, so the window starts at entry 768 of 2048 and needs 1024. */
#if (256 - VIEWPORT_HEIGHT) < 64
#define LIGHT_RAY_REBASE 16
#else
#define LIGHT_RAY_REBASE 64
#endif
/* Whole tile rows, 32 entries each — gBG3Buffer is u16, not bytes. */
#define LIGHT_RAY_MAP_STEP ((LIGHT_RAY_REBASE / 8) * 32)
static_assert(LIGHT_RAY_REBASE + VIEWPORT_HEIGHT <= 256,
              "the light shaft would wrap its 32-tile screenblock");

void LightRayManager_Main(LightRayManager* this) {
    u8 gfxGroup;
    u8* pbVar2;

    if (super->action >= LIGHT_RAY_ACTION_COUNT) {
#ifdef PC_PORT
        fprintf(stderr,
                "[LIGHT] invalid action=%u type=%u area=%u room=%u prop10=%p unk21=%u\n",
                super->action, super->type, gRoomControls.area, gRoomControls.room, GetCurrentRoomProperty(super->type),
                this->unk_21);
#endif
        super->action = 1;
        return;
    }

    LightRayManager_Actions[super->action](this);
    if (this->unk_21 >= LIGHT_RAY_STATE_COUNT || this->unk_21 >= LIGHT_RAY_GFX_GROUP_COUNT) {
#ifdef PC_PORT
        fprintf(stderr,
                "[LIGHT] invalid state after action action=%u unk21=%u type=%u area=%u room=%u prop10=%p\n",
                super->action, this->unk_21, super->type, gRoomControls.area, gRoomControls.room,
                GetCurrentRoomProperty(super->type));
#endif
        this->unk_21 = 0;
        return;
    }
    gUnk_08107C48[this->unk_21](this);
    gfxGroup = gLightRayManagerGfxGroups[this->unk_21];
    if ((gfxGroup != 0) && (this->gfxGroup != gfxGroup)) {
        this->gfxGroup = gfxGroup;
        LoadGfxGroup(gfxGroup);
    }
}

static void LightRayManager_OnEnterRoom(LightRayManager* this) {
    u8* pbVar1;

    LoadGfxGroup(this->gfxGroup);
    this->unk_34 = NULL;
    pbVar1 = &this->unk_21;
    if (*pbVar1 == 3) {
        gScreen.bg3.updated = 1;
    } else {
        gUnk_08107C48[*pbVar1](this);
    }
}

static void LightRayManager_OnExitRoom(void) {
    gScreen.lcd.displayControl &= ~DISPCNT_BG3_ON;
    gScreen.controls.layerFXControl = 0;
    DisableVBlankDMA();
}

void LightRayManager_Init(LightRayManager* this) {
    super->timer = 0;
    this->gfxGroup = 0;
    this->unk_21 = 0;
    this->unk_22 = 0;
    super->action = 1;
    gScreen.bg3.control = 0x1e04;
    gScreen.lcd.displayControl |= DISPCNT_BG3_ON;
    gScreen.controls.layerFXControl = 0x3648;
    gScreen.controls.alphaBlend = 0x1000;
    RegisterTransitionHandler(this, LightRayManager_OnEnterRoom, LightRayManager_OnExitRoom);
}

void LightRayManager_Action1(LightRayManager* this) {
    LightRayManagerProp* prop = GetCurrentRoomProperty(super->type);
    s32 temp;
    s32 x;
    s32 y;

    if (prop == NULL) {
        return;
    }

    if (prop->unk0 == 0xff)
        return;

    x = gPlayerEntity.base.x.HALF.HI / 16;
    y = gPlayerEntity.base.y.HALF.HI / 16;

    for (; prop->unk0 != 0xff; prop++) {
        if (prop->unk0 != this->unk_21) {
            u32 x2 = (gRoomControls.origin_x / 16) + prop->unk1;
            u32 y2 = (gRoomControls.origin_y / 16) + prop->unk2;

            if (y - y2 < prop->unk4 && x - x2 < prop->unk3) {
                switch (prop->unk0) {
                    case 1:
                        if (this->unk_21 == 2) {
                            super->action = 3;
                            super->timer = 9;
                        }
                        break;
                    case 5:
                        if (this->unk_21 == 6) {
                            super->action = 3;
                            super->timer = 9;
                        }
                        break;
                    case 2:
                        if (this->unk_21 == 0) {
                            super->action = 2;
                            super->timer = 0;
                            this->gfxGroup = 0;
                        }
                        break;
                    case 6:
                        if (this->unk_21 == 0) {
                            super->action = 2;
                            super->timer = 0;
                        }
                        break;
                    case 4:
                        if (this->unk_21 == 0) {
                            super->action = 2;
                            super->timer = 0;
                            this->unk_34 = NULL;
                        }
                        break;
                    case 3:
                        if (this->unk_21 == 4) {
                            super->action = 3;
                            super->timer = 9;
                        }
                        break;
                }

                if (super->action != 1) {
                    super->subtimer = 8;
                    this->unk_21 = prop->unk0;
                    this->unk_22 = 1;
                    return;
                }
            }
        }
    }
}

void LightRayManager_Action2(LightRayManager* this) {
    if (--super->subtimer == 0) {
        super->subtimer = 8;
        gScreen.controls.alphaBlend = gUnk_08107C1C[super->timer++];

        if (super->timer == 10) {
            super->action = 1;
            this->unk_22 = 0;
        }
    }
}

void LightRayManager_Action3(LightRayManager* this) {
    if (--super->subtimer == 0) {
        super->subtimer = 8;
        gScreen.controls.alphaBlend = gUnk_08107C1C[super->timer--];

        if (super->timer == 0xff) {
            super->action = 1;
            this->unk_22 = 0;
            this->unk_21 = 0;
            gScreen.vBlankDMA.ready = FALSE;
        }
    }
}

void sub_0805732C(u32 param_1, u32 param_2) {
    u32 index;
    u16* ptr = &gUnk_02017AA0[gUnk_03003DE4[0] * VIEWPORT_HDMA_HALF_U16];

    for (index = 0; index < VIEWPORT_HEIGHT; ptr++, index++) {
        *ptr = gSineTable[(param_2 + index) & 0xff] * param_1 / 0x100 + gScreen.bg3.xOffset;
    }

    SetVBlankDMA(&gUnk_02017AA0[gUnk_03003DE4[0] * VIEWPORT_HDMA_HALF_U16], (u16*)REG_ADDR_BG3HOFS,
                 ((DMA_ENABLE | DMA_START_HBLANK | DMA_16BIT | DMA_REPEAT | DMA_SRC_INC | DMA_DEST_RELOAD) << 16) +
                     0x1);
}

void nullsub_494() {
}

void sub_080573AC(LightRayManager* this) {
    s32 sin, frameCount;
#if defined(PC_PORT) && UI_CENTER_DX > 0
    /* The other light state, and the opposite kind of layer: this one takes
     * its xOffset from the camera below, so it is world-locked and wants the
     * unclipped BG3 rule. Said explicitly because an anchor persists until
     * something replaces it — going quiet would leave a previous state-4
     * declaration in force over this overlay. */
    Port_MapSource_DeclareBg3ScreenAnchor(PORT_BG3_ANCHOR_NONE);
#endif
    gRoomControls.bg3OffsetX.WORD -= 0x2000;
    gRoomControls.bg3OffsetY.WORD -= 0x1000;
    gScreen.bg3.xOffset = ((gRoomControls.scroll_x - gRoomControls.origin_x) >> 1) + gRoomControls.bg3OffsetX.HALF.HI;
    gScreen.bg3.yOffset = ((gRoomControls.scroll_y - gRoomControls.origin_y) >> 1) + gRoomControls.bg3OffsetY.HALF.HI;
    sin = gSineTable[(gRoomTransition.frameCount & 0xff) + 0x40];
    sub_0805732C((sin >> 5) + 0x10, gRoomTransition.frameCount);
    if (this->unk_22 == 0) {
        if ((gRoomTransition.frameCount & 0x1f) == 0) {
            this->unk_24 = (this->unk_24 + 1) & 7;
            gScreen.controls.alphaBlend = gUnk_08107C30[this->unk_24];
        }
    }
}

void sub_08057450(LightRayManager* this) {
    s32 y;
    gScreen.bg3.xOffset = 0x10;
#if defined(PC_PORT) && UI_CENTER_DX > 0
    /* B21. That constant is the whole point: this shaft is anchored to the
     * screen, not to the world, and the band it draws ends at the right edge
     * of the 240-px screen it was authored for. The port leaves a world view's
     * BG3 unclipped so the tiled, world-locked overlays can wrap their
     * screenblock across a wider viewport — which for this layer wraps its
     * blank left end into the columns past 239 instead. Saying so here lets
     * the renderer pin the band to the viewport's right edge.
     *
     * The port holds this until BG3 goes off or the room changes, which is
     * deliberate and not an oversight: this handler stops being dispatched
     * long before the overlay ends. LightRayManager_Action1 sets `unk_21` to
     * the *trigger* type when a fade-out starts, so the table dispatches to
     * nullsub_494 from the first frame of a fade that runs eighty more, and a
     * text box suspends the managers outright. Both were reported as the band
     * jumping left.
     *
     * Compiled out entirely at GBA-native width, where the band already ends
     * at the screen's right edge and there is nothing to pin: this file's
     * generated code is then byte-identical to the engine's. */
    Port_MapSource_DeclareBg3ScreenAnchor(PORT_BG3_ANCHOR_RIGHT);
#endif
    y = gRoomControls.scroll_y;
    y -= gRoomControls.origin_y;
    y >>= 2;

    gScreen.bg3.yOffset = y & (LIGHT_RAY_REBASE - 1);
    gScreen.bg3.subTileMap = &gBG3Buffer[(y / LIGHT_RAY_REBASE) * LIGHT_RAY_MAP_STEP];
    if (this->unk_34 != gScreen.bg3.subTileMap) {
        this->unk_34 = gScreen.bg3.subTileMap;
        gScreen.bg3.updated = 1;
    }

    if (this->unk_22 == 0 && (gRoomTransition.frameCount & 0x1f) == 0) {
        this->unk_24 = (this->unk_24 + 1) & 7;
        gScreen.controls.alphaBlend = gUnk_08107C30[this->unk_24];
    }

    if ((gRoomTransition.frameCount & 0x7) == 0) {
        u32 index;
        u16* ptr = &gUnk_085B4180[(this->unk_23 << 4)];

        for (index = 0, ptr = ptr + 7; index <= 3; index++) {
            SetColor(index + 0x87, *(ptr + index));
            SetColor(index + 0x8c, *(ptr + index + 5));
        }

        this->unk_23 = (this->unk_23 + 1) & 3;
    }
}

const u16 gUnk_08107C1C[] = {
    0x1000, 0xF01, 0xE02, 0xD03, 0xC04, 0xB05, 0xA06, 0x907, 0x908, 0x909,
};

const u16 gUnk_08107C30[] = {
    0x909, 0xA08, 0xB07, 0xC06, 0xD05, 0xC06, 0xB07, 0xA08,
};

const u8 gLightRayManagerGfxGroups[] = {
    0x25, 0x00, 0x24, 0x00, 0x25, 0x00, 0x00, 0x00,
};

void (*const gUnk_08107C48[])(LightRayManager*) = {
    nullsub_494, nullsub_494, sub_080573AC, nullsub_494, sub_08057450,
};

void (*const LightRayManager_Actions[])(LightRayManager*) = {
    LightRayManager_Init,
    LightRayManager_Action1,
    LightRayManager_Action2,
    LightRayManager_Action3,
};
