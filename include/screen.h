#ifndef SCREEN_H
#define SCREEN_H

#include "fade.h"
#include "global.h"

typedef struct {
    /*0x00*/ u16 displayControl;
    /*0x02*/ u8 filler2[0x2];
    /*0x04*/ u16 unk4;
    /*0x06*/ u16 displayControlMask;
} LcdControls;

typedef struct {
    u16 control;
    u16 xOffset;
    u16 yOffset;
    u16 updated;
    void* subTileMap;
} BgSettings;

typedef struct {
    u16 control;
    s16 xOffset;
    s16 yOffset;
    u16 updated;
    void* subTileMap;
} BgAffSettings;

typedef struct {
    u16 dx;
    u16 dmx;
    u16 dy;
    u16 dmy;
    u16 xPointLeastSig;
    u16 xPointMostSig;
    u16 yPointLeastSig;
    u16 yPointMostSig;
} BgTransformationSettings;

/* Window bounds container.
 *
 * On hardware WIN0H/WIN0V pack two 8-bit edges into one u16, which cannot
 * express an edge past 255 — a hard ceiling once the viewport is wider
 * than that (docs/viewport-expansion-research-plan.md §4, blocker 4). The
 * field is widened to 32 bits with the same two-field layout, so each edge
 * has 16 bits of room.
 *
 * Use WIN_RANGE to build a value and WIN_GET_HIGHER / WIN_GET_LOWER to
 * read the edges back, rather than shifting by 8. The higher field is the
 * one that was the high byte on hardware: X1 (left) for WIN*H, Y1 (top)
 * for WIN*V.
 *
 * Note that call sites keep whatever masking they already did. Several
 * rely on 8-bit wrap-around to produce a deliberately inverted window
 * (left > right), which the PPU renders as a wrap — widening the container
 * must not silently change that, so widening an individual site's
 * coordinate range is a per-site decision for the spike that needs it. */
typedef u32 winreg_t;

#define WIN_RANGE(higher, lower) (((winreg_t)(higher) << 16) | (winreg_t)((lower) & 0xFFFF))
#define WIN_GET_HIGHER(v) ((u32)(v) >> 16)
#define WIN_GET_LOWER(v) ((u32)(v) & 0xFFFF)

/* Viewport extents, for window sites that mean "the whole screen" rather
 * than a literal 240/160. These are what actually exceed the 8-bit
 * register ceiling once the viewport widens; the build overrides them
 * alongside the PPU/canvas width. */
#ifndef WIN_VIEWPORT_WIDTH
#define WIN_VIEWPORT_WIDTH DISPLAY_WIDTH
#endif
#ifndef WIN_VIEWPORT_HEIGHT
#define WIN_VIEWPORT_HEIGHT DISPLAY_HEIGHT
#endif

/* Truncate back to the packed 8-bit hardware layout. Lossy above 255 by
 * definition; the PPU takes the untruncated bounds by another route. */
#define WINREG_TO_GBA(v) ((u16)(((WIN_GET_HIGHER(v) & 0xFF) << 8) | (WIN_GET_LOWER(v) & 0xFF)))

#ifdef PC_PORT
/* port_screen.c — forwards full-width window bounds to the PPU. */
void Port_Screen_CommitWindows(winreg_t win0h, winreg_t win0v, winreg_t win1h, winreg_t win1v);
#endif

typedef struct {
    BgTransformationSettings bg2;
    BgTransformationSettings bg3;
    winreg_t window0HorizontalDimensions;
    winreg_t window1HorizontalDimensions;
    winreg_t window0VerticalDimensions;
    winreg_t window1VerticalDimensions;
    u16 windowInsideControl;
    u16 windowOutsideControl;
    u16 mosaicSize;
    u16 layerFXControl;
    u16 alphaBlend;
    u16 layerBrightness;
} BgControls;

typedef struct {
    bool8 ready;
    bool8 readyBackup;
    u16 unused;
    u16* src;
    u16* dest;
    u32 size;
} VBlankDMA;

typedef struct {
    /*0x00*/ LcdControls lcd;
    /*0x08*/ BgSettings bg0;
    /*0x14*/ BgSettings bg1;
    /*0x20*/ BgAffSettings bg2;
    /*0x2c*/ BgAffSettings bg3;
    /*0x38*/ BgControls controls;
    /*0x6c*/ VBlankDMA vBlankDMA;
    // /*0x6d*/ u8 _6d;
    // /*0x70*/ void* _70;
    // /*0x74*/ u32 _74;
    // /*0x78*/ u32 _78;
} Screen;

#ifndef OAM_COMMAND_DEFINED
#define OAM_COMMAND_DEFINED
typedef struct {
    s16 x;
    s16 y;
    u16 _4;
    u16 _6;
    u16 _8;
} OAMCommand;
#endif

extern Screen gScreen;
extern OAMCommand gOamCmd;

extern void sub_080ADA04(OAMCommand*, void*);

#endif // SCREEN_H
