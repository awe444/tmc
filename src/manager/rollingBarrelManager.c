/**
 * @file rollingBarrelManager.c
 * @ingroup Managers
 *
 * @brief Rolling barrel in Deepwood Shrine
 */
#include "manager/rollingBarrelManager.h"
#include "area.h"
#include "common.h"
#include "flags.h"
#include "main.h"
#include "physics.h"
#include "room.h"
#include "save.h"
#include "screen.h"
#include "sound.h"
#include "game.h"
#include "asm.h"
#include "fade.h"

extern struct BgAffineDstData gUnk_02017AA0[];
extern struct BgAffineDstData gUnk_02017BA0[];
extern u8 gUpdateVisibleTiles;
extern u32 gUsedPalettes;

void RollingBarrelManager_OnEnterRoom(void);
void sub_08058BC8(RollingBarrelManager*);
void sub_08058CB0(RollingBarrelManager*);
void sub_08058CFC(void);
void sub_08058A04(RollingBarrelManager*);
void sub_080588F8(RollingBarrelManager*);
u32 sub_08058B08(RollingBarrelManager*, u32, u32, const struct_08108228*);
void sub_08058B5C(RollingBarrelManager*, u32);
void RollingBarrelManager_Init(RollingBarrelManager*);
void RollingBarrelManager_Action1(RollingBarrelManager*);
void RollingBarrelManager_Action2(RollingBarrelManager*);

void RollingBarrelManager_Main(RollingBarrelManager* this) {
    static void (*const RollingBarrelManager_Actions[])(RollingBarrelManager*) = {
        RollingBarrelManager_Init,
        RollingBarrelManager_Action1,
        RollingBarrelManager_Action2,
    };
    u32 tmp;
    RollingBarrelManager_Actions[super->action](this);
    sub_08058BC8(this);
    SetVBlankDMA((u16*)&gUnk_02017AA0[gUnk_03003DE4[0] * VIEWPORT_HDMA_HALF_AFFINE], (u16*)REG_ADDR_BG2PA,
                 ((DMA_ENABLE | DMA_START_HBLANK | DMA_16BIT | DMA_REPEAT | DMA_SRC_INC | DMA_DEST_RELOAD) << 16) +
                     0x8);
}
void RollingBarrelManager_Init(RollingBarrelManager* this) {
    super->action = 1;
    this->unk_28 = 0x1234;
    super->timer = CheckLocalFlags(0x15, 0x2) != 0;
    sub_08058CB0(this);
    RegisterTransitionHandler(this, RollingBarrelManager_OnEnterRoom, NULL);
}

void RollingBarrelManager_Action1(RollingBarrelManager* this) {
    sub_08058CFC();
    sub_08058A04(this);
    if (gRoomTransition.transitioningOut) {
        super->action = 2;
    } else {
        sub_080588F8(this);
    }
}

void RollingBarrelManager_Action2(RollingBarrelManager* this) {
}

#define ABS_DIFF_GT(a, b, c) ((signed)(a) - (b) >= 0 ? (a) - (b) > (c) : (b) - (a) > (c))

void sub_080588F8(RollingBarrelManager* this) {
    if (super->subtimer == 0) {
        if (ABS_DIFF_GT(this->unk_28, this->unk_24.HALF.HI, 8)) {
            this->unk_28 = 0x1234;
            switch (this->unk_24.HALF.HI & 0xFFFE) {
                case 0x48:
                case 0xa0:
                case 0xf0:
                    this->unk_28 = this->unk_24.HALF.HI;
                    super->subtimer = 45;
                    SoundReq(SFX_BARREL_ROLL_STOP);
            }
        }
    } else {
        super->subtimer--;
        if (super->subtimer > 0x29) {
            this->unk_24.HALF.HI = this->unk_28 - 2;
        } else {
            this->unk_24.HALF.HI = this->unk_28;
        }
    }
    if (super->timer) {
        s32 tmp = gPlayerEntity.base.y.HALF.HI - gRoomControls.origin_y;
        u32 tmp2;
        tmp2 = (((unsigned)(tmp - 0x50 < 0 ? 0x50 - tmp : tmp - 0x50) >> 3) * 0x3000) + 0x4000;
        if (super->subtimer == 0) {
            if (tmp < 0x49) {
                this->unk_24.WORD -= tmp2;
            } else if (tmp > 0x57) {
                this->unk_24.WORD += tmp2;
            }
            if (ABS_DIFF_GT(this->unk_2c, this->unk_24.WORD, 0x100000)) {
                this->unk_2c = this->unk_24.WORD;
                SoundReq(SFX_BARREL_ROLL);
            }
        }
    }
    if ((this->unk_20 = this->unk_24.HALF.HI) < 0) {
        this->unk_20 = 0x1FF;
        this->unk_24.HALF.HI = 0x1FF;
    } else if (this->unk_20 > 0x1FF) {
        this->unk_20 = 0;
        this->unk_24.HALF.HI = 0;
    }
}

void sub_08058A04(RollingBarrelManager* this) {
    static const struct_08108228 gUnk_08108228[6] = { { 0x40, 0x2A, 0x1C, 0x10 }, { 0x3C, 0x24, 0x1C, 0x12 },
                                                      { 0x3C, 0x1C, 0x1A, 0x18 }, { 0x38, 0x16, 0x1C, 0x16 },
                                                      { 0x34, 0x16, 0x1E, 0x0E }, { 0x30, 0x16, 0x20, 0x0A } };

    static const struct_08108228 gUnk_08108258[6] = { { 0x30, 0x88, 0x20, 0x0A }, { 0x34, 0x7E, 0x1E, 0x12 },
                                                      { 0x38, 0x78, 0x1A, 0x16 }, { 0x3A, 0x70, 0x1A, 0x18 },
                                                      { 0x3C, 0x64, 0x1E, 0x1E }, { 0x3E, 0x6A, 0x1C, 0x10 } };

    static const struct_08108228 gUnk_08108288[6] = { { 0x98, 0x2A, 0x1C, 0x10 }, { 0x98, 0x24, 0x1C, 0x12 },
                                                      { 0x9C, 0x1C, 0x1A, 0x18 }, { 0x9C, 0x16, 0x1C, 0x16 },
                                                      { 0xA0, 0x16, 0x1E, 0x0E }, { 0xA0, 0x16, 0x20, 0x0A } };

    static const struct_08108228 gUnk_081082B8[6] = { { 0xA0, 0x88, 0x20, 0x0A }, { 0x9E, 0x7E, 0x1E, 0x12 },
                                                      { 0x9C, 0x78, 0x1A, 0x16 }, { 0x9A, 0x70, 0x1A, 0x18 },
                                                      { 0x98, 0x64, 0x1E, 0x1E }, { 0x98, 0x6A, 0x1C, 0x10 } };

    s32 tmp = gPlayerEntity.base.x.HALF.HI - gRoomControls.origin_x;
    s32 tmp2 = gPlayerEntity.base.y.HALF.HI - gRoomControls.origin_y;
    /* The fall is gated on the barrel's angle because the cobweb hole
     * *rotates with the barrel*, while the fall itself snaps the player to a
     * fixed room position (origin + 0x78, 0x50, below). The gate is the only
     * thing tying those two together: inside 0x118..0x124 the drawn hole is at
     * room centre, which is where the player is put.
     *
     * The port used to bypass this (`#ifdef PC_PORT`, CHANGELOG #6) because the
     * 13-unit window out of a 512-unit revolution was hard to land. That made
     * the fall fire at whatever angle the barrel happened to rest at — measured
     * on the maintainer's 2026-08-08 recording, `unk_20 = 0xA4`, a rest angle,
     * with the window open on none of the sampled frames — so the player
     * dropped through a patch of solid barrel while the hole was drawn about
     * 82 degrees away. That is B23, and it was reported the moment B22 made the
     * room playable enough to reach the hole on purpose.
     *
     * Restored to the hardware gate by the maintainer's decision of 2026-08-08.
     * Reachable: the snap targets are 0x48, 0xA0 and 0xF0 (sub_080588F8), and
     * there is none between 0xF0 and the wrap, so a barrel pushed past its
     * 0xF0 rest rolls freely through the window rather than stopping short. */
    bool32 angleOk = (this->unk_20 - 0x118 < 0xDu);
    if (angleOk && CheckGlobalFlag(LV1TARU_OPEN) && (tmp - 0x6d < 0x17u) &&
        (tmp2 - 0x45 < 0x17u) && (gPlayerEntity.base.z.HALF.HI == 0)) {
        gPlayerState.queued_action = PLAYER_FALL;
        gPlayerState.field_0x38 = 0;
        gPlayerEntity.base.x.HALF.HI = gRoomControls.origin_x + 0x78;
        gPlayerEntity.base.y.HALF.HI = gRoomControls.origin_y + 0x50;
        return;
    }
    if (tmp < 0x78) {
        if (tmp2 < 0x50) {
            if (sub_08058B08(this, 0x88, 0xB0, gUnk_08108228)) {
                sub_08058B5C(this, 0);
            }
        } else {
            if (sub_08058B08(this, 0x38, 0x60, gUnk_08108258)) {
                sub_08058B5C(this, 1);
            }
        }
    } else {
        if (tmp2 < 0x50) {
            if (sub_08058B08(this, 0xE0, 0x108, gUnk_08108288)) {
                sub_08058B5C(this, 2);
            }
        } else {
            if (sub_08058B08(this, 0x90, 0xB8, gUnk_081082B8)) {
                sub_08058B5C(this, 3);
            }
        }
    }
}

u32 sub_08058B08(RollingBarrelManager* this, u32 unk1, u32 unk2, const struct_08108228* unk3) {
    u32 tmp = this->unk_20;
    if (tmp < unk1 || tmp > unk2) {
        return 0;
    } else {
        u32 tmp2, tmp3;
        tmp -= unk1;
        tmp >>= 3;
        unk3 += tmp;
        tmp2 = (gPlayerEntity.base.x.HALF.HI - gRoomControls.origin_x - unk3->unk_0);
        tmp3 = (gPlayerEntity.base.y.HALF.HI - gRoomControls.origin_y - unk3->unk_2);
        return ((tmp2 < unk3->unk_4) && (tmp3 < unk3->unk_6));
    }
}

void sub_08058B5C(RollingBarrelManager* this, u32 unk1) {
    static const u16 gUnk_081082E8[0xC] = { 0xB8, 0x80, 0x0, 0xB8, 0x110, 0x2, 0x118, 0x80, 0x2, 0x118, 0x110, 0x0 };
    gRoomTransition.transitioningOut = 1;
    gRoomTransition.type = TRANSITION_DEFAULT;
    gRoomTransition.player_status.spawn_type = PL_SPAWN_STEP_IN;
    gRoomTransition.player_status.area_next = gRoomControls.area;
    gRoomTransition.player_status.room_next = 6;
    gRoomTransition.player_status.start_anim = unk1 & 1 ? 4 : 0;
    gRoomTransition.player_status.start_pos_x = gUnk_081082E8[unk1 * 3];
    gRoomTransition.player_status.start_pos_y = gUnk_081082E8[unk1 * 3 + 1];
    gSave.dws_barrel_state = gUnk_081082E8[unk1 * 3 + 2];
    SoundReq(SFX_STAIRS);
}

void sub_08058BC8(RollingBarrelManager* this) {
    struct BgAffineDstData* tmp = &gUnk_02017AA0[gUnk_03003DE4[0] * VIEWPORT_HDMA_HALF_AFFINE];
    struct BgAffineSrcData tmp2;
    s32 tmp3;
    tmp2.texX = 0x10000;
    tmp2.scrX = 0x78;
    tmp2.scrY = 0x80;
    tmp2.alpha = 0;
    tmp2.sy = 0x100;
    tmp2.sx = 0x100;
    tmp3 = 0;
    do {
        /* Entry `tmp3` is replayed on *screen* scanline tmp3, but everything
         * it computes is a position on the barrel, and the barrel is a
         * 240x160-authored surface centred in the viewport (see the clip in
         * port_mapsource.c, which is what puts it there and what makes the
         * PPU sample this matrix at `line - UI_CENTER_DY`). So the row that
         * matters here is the one inside those 160 authored rows.
         *
         * The 0xA0 below says so: it maps the screen's 160 scanlines onto a
         * quarter period of the sine that gives the staves their curve. Left
         * indexed by the screen line at a taller viewport it runs to 191
         * instead of 127 — off the end of the quarter period and into an
         * unrelated stretch of the table — and the barrel's curvature inverts
         * over the bottom rows.
         *
         * Rows outside the band are clipped away and their entries never
         * reach the PPU, but they are still written: the table is the full
         * VIEWPORT_HEIGHT and a half-filled one feeds the DMA whatever is
         * behind it. Clamped rather than left to wrap so no index leaves the
         * range the 240x160 build uses.
         *
         * At GBA-native height UI_CENTER_DY is 0 and `row` is `tmp3`, so this
         * is the original loop exactly. */
        s32 row = tmp3 - UI_CENTER_DY;
        u32 indx;
        if (row < 0) {
            row = 0;
        } else if (row >= DISPLAY_HEIGHT) {
            row = DISPLAY_HEIGHT - 1;
        }
        indx = ((row << 7) / 0xA0) & 0xFF;
        tmp2.sx = 0x100 + ((gSineTable[indx] * 3) >> 2);
        tmp2.sy =
            0x100 - ((gSineTable[indx * 2] * 2) >>
                     5); // yes, it makes no sense to multiply first and then shift right, but it's matching this way
        tmp2.texY = (this->unk_20 + row) << 8;
        BgAffineSet(&tmp2, tmp, 1);
        tmp++;
    } while (++tmp3 < (s32)VIEWPORT_HEIGHT);
    tmp = &gUnk_02017BA0[gUnk_03003DE4[0] * VIEWPORT_HDMA_HALF_AFFINE];
    gScreen.controls.bg2.dx = tmp->pa;
    gScreen.controls.bg2.dmx = tmp->pb;
    gScreen.controls.bg2.dy = tmp->pc;
    gScreen.controls.bg2.dmy = tmp->pd;
    gScreen.controls.bg2.xPointLeastSig = ((union SplitWord*)&tmp->dx)->HALF.LO;
    gScreen.controls.bg2.xPointMostSig = ((union SplitWord*)&tmp->dx)->HALF.HI;
    gScreen.controls.bg2.yPointLeastSig = ((union SplitWord*)&tmp->dy)->HALF.LO;
    gScreen.controls.bg2.yPointMostSig = ((union SplitWord*)&tmp->dy)->HALF.HI;
}

void sub_08058CB0(RollingBarrelManager* this) {
    static const u16 gUnk_08108300[4] = { 0xA4, 0x4C, 0xF4, 0x9C };
    u32 tmp = gPlayerEntity.base.x.HALF.HI - gRoomControls.origin_x;
    u32 tmp2 = gPlayerEntity.base.y.HALF.HI - gRoomControls.origin_y;
    u32 tmp3;
    if (tmp < 0x78) {
        tmp3 = 1;
        if (tmp2 < 0x50) {
            tmp3 = 0;
        }
    } else {
        tmp3 = 3;
        if (tmp2 < 0x50) {
            tmp3 = 2;
        }
    }
    this->unk_24.HALF.HI = this->unk_20 = gUnk_08108300[tmp3];
}

/* Hold the player on the barrel's midline, which is where the roll speed,
 * the quadrant split and the door and cobweb-hole hit tests are all measured
 * from (sub_080588F8, sub_08058A04, sub_08058CB0 — every one of them against
 * origin_y + 0x50).
 *
 * The engine spelled the anchor `scroll_y`, and on hardware that is the same
 * number: the room is exactly 240x160, so the camera has nowhere to go and
 * sits on the room origin. Above GBA-native height it is not — a 160-row room
 * in a 240-row viewport is centred, which puts the camera 40 px *above* the
 * origin (VIEWPORT_CAM_MIN_Y) — so the pin held the player at room y 40 while
 * everything he can interact with is 40 px below him. Measured on Link's
 * sprite: room y 27 at 320x240 against 63 at 240x160.
 *
 * That is the whole of the "doors do not line up with the walkable area"
 * report. The doors are where they always were; the player was not.
 *
 * Anchored to the room, which is what the barrel actually is. At 240x160
 * scroll_y == origin_y here by construction, so the shipping build does not
 * move — verified by capturing this room at that size either side of the
 * change. */
void sub_08058CFC(void) {
    u32 tmp = gPlayerEntity.base.y.HALF.HI - gRoomControls.origin_y;
    if (tmp < 0x4C) {
        sub_080044AE(&gPlayerEntity.base, 0xC0, 0x10);
    }
    if (tmp > 0x54) {
        sub_080044AE(&gPlayerEntity.base, 0xC0, 0);
    }
}

void RollingBarrelManager_OnEnterRoom(void) {
    u16 tmp;
    u32 tmp2;
    LoadPaletteGroup(0x28);
    MemCopy(gPaletteBuffer + 3 * 16, gPaletteBuffer + 21 * 16, 16 * 2);
    USE_PALETTE(21);
    LoadGfxGroup(0x16);
    tmp = gScreen.lcd.displayControl;
    tmp2 = 0;
    gScreen.lcd.displayControl |= DISPCNT_MODE_1;
    gScreen.bg2.control = 0xBC82;
    gScreen.bg1.control = 0x5E86;
    gScreen.bg1.xOffset = 0;
    gScreen.bg1.yOffset = tmp2;
    gScreen.controls.layerFXControl = 0x3456;
    gScreen.controls.alphaBlend = 0x909;
    gArea.bgm = gArea.queued_bgm;
    gUpdateVisibleTiles = 0;
    if (CheckGlobalFlag(LV1TARU_OPEN)) {
        LoadGfxGroup(0x4A);
    }
}
