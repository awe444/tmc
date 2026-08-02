/* See port_mapsource.h. Kept in C because it reads engine headers, which
 * do not parse as C++ (they use `this` as a parameter name). */
#include "port_mapsource.h"

#include <stdio.h>
#include <stdlib.h>

#include "gba/gba.h"
#include "main.h"
#include "game.h"
#include "map.h"
#include "room.h"
#include "screen.h"
#include "subtask.h"
#include "ui.h"
#include "tileMap.h"
#include "vram.h"

#include "cpu/mode1.h"
#include "viewport.h"

/* Which BG a map layer displays through is *per room*, not fixed: the
 * engine records it in MapLayer.bgSettings, which points at one of
 * gScreen.bg0..bg3 (e.g. beanstalkSubtask.c:151-153 binds bottom->bg2,
 * top->bg1 on the ordinary room-init path, but other paths differ).
 * Deriving the index from that pointer is what makes the binding correct
 * in every room; hardcoding the common case renders the wrong layer's
 * tilemap wherever a room deviates. */
static int mapsource_bg_index(const BgSettings* settings) {
    if (settings == (const BgSettings*)&gScreen.bg0) {
        return 0;
    }
    if (settings == (const BgSettings*)&gScreen.bg1) {
        return 1;
    }
    if (settings == (const BgSettings*)(const void*)&gScreen.bg2) {
        return 2;
    }
    if (settings == (const BgSettings*)(const void*)&gScreen.bg3) {
        return 3;
    }
    return -1;
}

/* The staging buffer each BG index normally streams from; a layer pointed
 * anywhere else is screenblock-shaped data, not a 0x80-stride room map. */
static const void* mapsource_expected_buffer(int bg_index) {
    switch (bg_index) {
        case 0: return &gBG0Buffer;
        case 1: return &gBG1Buffer;
        case 2: return &gBG2Buffer;
        case 3: return &gBG3Buffer;
        default: return NULL;
    }
}

static const void* mapsource_sub_tile_map(int bg_index) {
    switch (bg_index) {
        case 0: return gScreen.bg0.subTileMap;
        case 1: return gScreen.bg1.subTileMap;
        case 2: return gScreen.bg2.subTileMap;
        case 3: return gScreen.bg3.subTileMap;
        default: return NULL;
    }
}

/* Special-map geometry (include/tileMap.h): u16[0x4000] addressed as a
 * 128x128 grid of 8x8 tiles at a 0x80 row stride = 1024x1024 px, which
 * covers the largest room in the game (1024x1008). */
#define MAPSRC_STRIDE 0x80
#define MAPSRC_MAX_TILES 128

enum {
    REASON_BOUND = -1,
    REASON_TASK = 0,
    REASON_SUBSTATE,
    REASON_SCROLL_FLAGS,
    REASON_TRANSITION,
    REASON_REBOUND,
    REASON_NO_LAYER,
    REASON_GEOMETRY,
    REASON_DISABLED,
    REASON_COUNT
};

static const char* const kReasonName[REASON_COUNT] = {
    "task!=GAME", "substate!=UPDATE", "scroll_flags&1", "mid-transition",
    "subTileMap rebound", "layer off", "bad geometry", "disabled",
};

static bool sEnabled = true;
static int sLastReason[2] = { REASON_DISABLED, REASON_DISABLED };
static unsigned sBoundFrames[2];
static unsigned sRejectFrames[2][REASON_COUNT];

void Port_MapSource_SetEnabled(bool enabled) {
    sEnabled = enabled;
}

bool Port_MapSource_Enabled(void) {
    return sEnabled;
}

const char* Port_MapSource_ReasonName(int reason) {
    if (reason == REASON_BOUND) {
        return "bound";
    }
    if (reason < 0 || reason >= REASON_COUNT) {
        return "?";
    }
    return kReasonName[reason];
}

int Port_MapSource_LastReason(int layer) {
    return sLastReason[layer & 1];
}

/* The predicate. Each clause corresponds to an exclusion class established
 * statically in Spike 2 and confirmed over 7.6M runtime tile comparisons:
 *
 *   task/substate  - menu, title, file-select and subtask scratch uses
 *                    (fileselect save struct, kinstone array, pause-menu
 *                    dungeon bitmap, Gyorg collision bitmap)
 *   scroll_flags&1 - "degraded" rooms whose map came from the 0xffff
 *                    sentinel path: built 512x512 by sub_0807C5F4 and not
 *                    updated by the tile mutators
 *   scrollAction   - room-to-room transition scrollers blend two rooms
 *   subTileMap     - background managers that point a BG at the arrays as
 *                    screenblock-shaped data (bigGoron, minish paths,
 *                    minish rafters) rather than as a 0x80-stride map
 *   bgSettings     - layer detached entirely (e.g. weatherChangeManager
 *                    fog phase)
 */
static bool mapsource_is_ui_screen(void);

static int mapsource_reason(int layer) {
    if (!sEnabled) {
        return REASON_DISABLED;
    }
    {   /* TMC_MAPSRC_LAYERS=0|1|2(both) — bisection aid */
        const char* only = getenv("TMC_MAPSRC_LAYERS");
        if (only != NULL) {
            int want = only[0] - '0';
            if (want != 2 && want != layer) {
                return REASON_DISABLED;
            }
        }
    }
    if (gMain.task != TASK_GAME) {
        return REASON_TASK;
    }
    /* Ordinary gameplay, and also the subtasks that are *views of the
     * world* — cutscenes and world events. Those call UpdateScrollVram
     * (subtaskAuxCutscene.c:85, subtaskWorldEvent.c:57), so the special
     * maps are live and full-room during them, and they must fill the
     * viewport rather than be centred: leaving them on the screenblock
     * path made the opening cutscenes wrap and repeat past column 240.
     * Menu subtasks are excluded — that is where the arrays get
     * repurposed as scratch (Spike 2 §5.1). */
    if (gMain.substate != GAMEMAIN_UPDATE) {
#if VIEWPORT_WIDTH > DISPLAY_WIDTH
        /* Only a wider viewport needs this. At GBA-native size the
         * screenblock path already covers the whole screen, and it is the
         * behaviour verified bit-identical to the pre-expansion build, so
         * do not change what the shipping build renders. */
        if (gMain.substate != GAMEMAIN_SUBTASK || mapsource_is_ui_screen()) {
            return REASON_SUBSTATE;
        }
#else
        return REASON_SUBSTATE;
#endif
    }
    if (gRoomControls.scroll_flags & 1) {
        return REASON_SCROLL_FLAGS;
    }
    if (gRoomControls.scrollAction >= 2) {
        return REASON_TRANSITION;
    }
    {
        const BgSettings* settings =
            (layer == 0) ? gMapBottom.bgSettings : gMapTop.bgSettings;
        int bg;
        if (settings == NULL) {
            return REASON_NO_LAYER;
        }
        bg = mapsource_bg_index(settings);
        if (bg < 0) {
            return REASON_NO_LAYER;
        }
        if (mapsource_sub_tile_map(bg) != mapsource_expected_buffer(bg)) {
            return REASON_REBOUND;
        }
    }
    /* Camera origin must be inside the room and the room must fit the
     * special-map grid; otherwise sampling would read outside the data. */
    {
        int cx = (int)gRoomControls.scroll_x - (int)gRoomControls.origin_x;
        int cy = (int)gRoomControls.scroll_y - (int)gRoomControls.origin_y;
        int tw = gRoomControls.width >> 3;
        int th = gRoomControls.height >> 3;
        /* cx/cy may be negative: a room narrower (or shorter) than the
         * viewport is centred, which puts screen column 0 outside the room.
         * The PPU renders those columns as backdrop, so only the room
         * extent needs to be sane. */
        (void)cx;
        (void)cy;
        if (tw <= 0 || th <= 0 || tw > MAPSRC_MAX_TILES || th > MAPSRC_MAX_TILES) {
            return REASON_GEOMETRY;
        }
    }
    return REASON_BOUND;
}

bool Port_MapSource_LayerAuthoritative(int layer) {
    int r = mapsource_reason(layer & 1);
    sLastReason[layer & 1] = r;
    return r == REASON_BOUND;
}

/* UI centring (D1: screens and the text box are centered; the in-game HUD
 * is edge-anchored and handled separately).
 *
 * Content authored for a 240-wide screen is centred by clipping each UI
 * layer to a 240-px span starting UI_CENTER_DX in. A clip is used rather
 * than a map source because title and file select load their tilemaps
 * straight into VRAM screenblocks — a map source would read the staging
 * buffer, which for those screens is stale or empty. The clip works on the
 * layer's normal screenblock fetch, so it is agnostic about where the
 * content came from, and it suppresses the wrap that a plain scroll offset
 * would produce.
 *
 * BG0 during gameplay is deliberately *not* clipped: it carries the
 * edge-anchored HUD, authored out to the viewport edges. The text box also
 * lives on BG0 there and centres itself per-window (message.c). */
static bool sUiCentered = false;
static int sClippedBgMask = 0; /* which BGs the clip rule caught, for the trace */

bool Port_MapSource_UiCentered(void) {
    return sUiCentered;
}

/* Is the screen currently showing a full-screen surface that was *authored*
 * for a 240-wide display, as opposed to a view of the world?
 *
 * `substate != GAMEMAIN_UPDATE` is NOT the right test, which is what an
 * earlier version used. Cutscenes run as subtasks too, but they are views
 * of the world and must fill the viewport like gameplay does; centring them
 * shrinks the cutscene to 240 and shifts its sprites, which is what made
 * the opening cutscenes render narrow and misaligned.
 *
 * So discriminate on the subtask *type* (gUI.lastState, the value handed to
 * MenuFadeIn): menus are 240-authored UI, cutscenes and world events are
 * world views. Title and file select are not subtasks at all — they are
 * separate tasks — and are always UI. */
static bool mapsource_is_ui_screen(void) {
    if (gMain.task != TASK_GAME) {
        return true; /* title, file select, gameover, staffroll */
    }
    if (gMain.substate != GAMEMAIN_SUBTASK) {
        return false; /* ordinary gameplay */
    }
    switch (gUI.lastState) {
        case SUBTASK_PAUSEMENU:
        case SUBTASK_MAPHINT:
        case SUBTASK_KINSTONEMENU:
        case SUBTASK_FIGURINEMENU:
        case SUBTASK_LOCALMAPHINT:
            return true;
        case SUBTASK_AUXCUTSCENE:
            /* A cutscene is a view of the world and must fill the viewport
             * (B3) — except that one of them spends its first half showing
             * something else. The Picori legend's stained-glass cards are a
             * whole 240x160 authored panel, and they run as states 0..10 of
             * the *same* AUXCUTSCENE that later fades into Zelda walking
             * through Hyrule Field. So the subtask cannot decide this; it is
             * one subtask wearing two hats.
             *
             * What separates them is mechanical rather than a guess about
             * which cutscene is playing: a story panel has no world behind
             * it, and says so by detaching both map layers
             * (cutscene.c:230-231, gMapBottom/gMapTop.bgSettings = NULL).
             * With no layer bound there is no world view to fill, and what is
             * on screen is a 240-authored surface like any menu. SetBGDefaults
             * rebinds them when the cutscene switches to its world half, which
             * is what puts the hat back.
             *
             * This is only about the *vertical* shift. The panels already took
             * the horizontal one via the no-map-source clip rule, which is why
             * B2 and B9 are about them. */
            return gMapBottom.bgSettings == NULL && gMapTop.bgSettings == NULL;
        default:
            /* PORTALCUTSCENE, WORLDEVENT, FASTTRAVEL: world. */
            return false;
    }
}

/* B1: the text box centres itself per-window (message.c) because it shares
 * BG0 with the edge-anchored HUD during gameplay. On a UI screen the whole
 * BG0 layer is already shifted, so applying the per-window shift as well
 * moves the box twice — which is what clipped the "Saving file..." and
 * "Erasing file..." popups on the file-select and pause-menu screens. */
int Port_MapSource_MessageTileShift(void) {
    /* Always zero now. BG0 is a 32-column map, so shifting a ~28-column
     * text box by 5 columns would overflow the row; the whole layer is
     * centred instead, which carries the box with it. */
    return 0;
}

/* The vertical sibling, and unlike the x one it is *not* always zero.
 *
 * A world view's BG0 is not shifted vertically — it carries the HUD, which
 * stays at the top — so the text box has to take UI_CENTER_DY on its own to
 * keep its authored position inside the centred frame. A UI screen's BG0 *is*
 * shifted by exactly that, so a popup on one already travels with the layer
 * and adding this on top would move it twice. That is the same double-shift that B1
 * clipped the "Saving file..." and "Erasing file..." popups with, one axis
 * over: at 240 rows the second shift would push a popup past the 160-row
 * content span and the clip would delete it outright. */
int Port_MapSource_MessageTileShiftY(void) {
    return sUiCentered ? 0 : UI_TEXTBOX_TILE_DY;
}

/* Decide, per layer, how it can legitimately fill a wider viewport.
 *
 * There are only two ways a text BG can:
 *   1. It is bound to a full-room map source (the world layers during
 *      gameplay and cutscenes, and BG0 whose buffer we widened). Those
 *      render the whole viewport.
 *   2. It is not — in which case it is reading a 32-tile VRAM screenblock,
 *      which covers 256 px and *wraps*. Such a layer cannot fill 320 no
 *      matter what state the game is in: stretching it just repeats its
 *      content, which is what duplicated the stained-glass artwork and the
 *      pause-menu frame.
 *
 * So the rule is mechanical rather than a guess about game state: a layer
 * without a map source is clipped to DISPLAY_WIDTH and centred. That covers
 * every 240-authored surface (title, file select, menus, legend artwork)
 * and every transient screenblock fallback (room-to-room scrolling) with no
 * need to classify subtasks correctly for *rendering* — classification is
 * only still needed to decide whether the world layers may be map-sourced
 * at all, and whether sprites should travel with a shifted layer.
 */
static void mapsource_bind_ui(void) {
#if UI_CENTER_DX > 0 || UI_CENTER_DY > 0
    bool ui_screen = mapsource_is_ui_screen();
    VirtuaPPUMode1BgClip clip;
    int bg;

    virtuappu_mode1_clear_bg_clips();
    sUiCentered = ui_screen;

    /* One rule, applied uniformly: a layer with no map source is reading a
     * 32-tile screenblock. It covers 256 px and wraps, so it cannot fill a
     * wider viewport — stretching it only repeats its content. Clip it to
     * the authored width and centre it.
     *
     * BG0 is now such a layer (its buffer stayed 32 wide), so it needs no
     * special case: the HUD, the text box and every UI screen are all
     * carried by the same clip, which is what keeps their authored
     * relationship to each other intact. */
    clip.offset_x = UI_CENTER_DX;
    clip.content_width = DISPLAY_WIDTH;
    /* Vertically the rule is *not* uniform, and this is the one place the two
     * axes differ. A whole authored screen is centred, so it takes the shift.
     * A world view's BG0 carries the HUD, which is anchored to the top of the
     * screen — and the top of a taller screen is still the top — so it keeps
     * offset_y = 0 and is allowed the full frame. See UI_CENTER_DY. */
    clip.offset_y = ui_screen ? UI_CENTER_DY : 0;
    clip.content_height = ui_screen ? DISPLAY_HEIGHT : MODE1_GBA_HEIGHT;
    sClippedBgMask = 0;
    for (bg = 0; bg < 4; bg++) {
        /* BG3 during a world view is a *gameplay overlay* — hole parallax,
         * cloud shadows, light/dark, weather, steam, POW — not a 240-authored
         * surface. Two things go wrong if the rule catches it.
         *
         * It loses the overlay in the border columns, and, worse, the clip
         * also shifts the layer by UI_CENTER_DX. Most of these overlays are
         * world-locked (bg3.xOffset = scroll_x + k, e.g. holeManager.c:299,
         * powBackgroundManager.c:32), and at a wider viewport scroll_x is
         * already 40px further left, so the layer aligns with the world by
         * itself. Adding the shift on top misaligns it — visible in the
         * middle of the screen, not just the borders.
         *
         * Leaving it unclipped lets it wrap its 32-tile screenblock past
         * 256px, which for these tiled patterns is exactly what covers the
         * viewport: measured pixel-identical to the 240 build through the
         * centre 240 columns, where clipping it differed by 6197 px. On a UI
         * screen BG3 *is* authored content and still takes the clip. */
        if (bg == 3 && !ui_screen) {
            continue;
        }
        if (!virtuappu_mode1_has_map_source(bg)) {
            virtuappu_mode1_set_bg_clip(bg, &clip);
            sClippedBgMask |= 1 << bg;
        }
    }

    /* On a UI screen everything on show is centred, sprites included. In a
     * world view the world sprites must stay where the engine put them —
     * the HUD's own sprites are shifted at their source instead
     * (UI_HUD_SPRITE_DX in ui.c), so they travel with the HUD layer without
     * dragging Link along with them. */
    virtuappu_mode1_set_obj_offset(ui_screen ? UI_CENTER_DX : 0,
                                   ui_screen ? UI_CENTER_DY : 0);

    if (ui_screen) {
        virtuappu_mode1_set_obj_clip(UI_CENTER_DX, UI_CENTER_DX + DISPLAY_WIDTH);
        virtuappu_mode1_set_obj_clip_v(UI_CENTER_DY, UI_CENTER_DY + DISPLAY_HEIGHT);
        return;
    }
    virtuappu_mode1_set_obj_clip_v(0, MODE1_GBA_HEIGHT);

    /* World view: confine sprites to the room's on-screen span so the border
     * stays border. A room narrower than the viewport is centred, so the
     * columns either side are outside the room and hardware would never
     * have drawn an entity standing there. When the world layers have fallen
     * back to the screenblock path (mid-transition) they are clipped to the
     * authored width above, so the sprites match that instead. */
    {
        int span_left = 0;
        int span_right = MODE1_GBA_WIDTH;
        if (gRoomControls.scrollAction >= 2 || !virtuappu_mode1_has_map_source(2)) {
            span_left = UI_CENTER_DX;
            span_right = UI_CENTER_DX + DISPLAY_WIDTH;
        } else if ((int)gRoomControls.width < MODE1_GBA_WIDTH) {
            int cx = (int)gRoomControls.scroll_x - (int)gRoomControls.origin_x;
            span_left = -cx;
            span_right = span_left + (int)gRoomControls.width;
            if (span_left < 0) span_left = 0;
            if (span_right > MODE1_GBA_WIDTH) span_right = MODE1_GBA_WIDTH;
        }
        virtuappu_mode1_set_obj_clip(span_left, span_right);
    }
#endif
}

/* TMC_LAYER_TRACE=1: which BG indices have a map source and which the clip
 * rule caught, with DISPCNT and all four BGxCNT. The question this answers is
 * "which layer is that artwork actually on, and did the rule reach it" — so it
 * must run after mapsource_bind_ui(), not before. */
static void mapsource_trace_layers(void) {
    static int en = -1;
    static int last = -1;
    int mapsrc = 0, key, b;
    if (en < 0) en = (getenv("TMC_LAYER_TRACE") != NULL);
    if (!en) return;
    for (b = 0; b < 4; b++) {
        if (virtuappu_mode1_has_map_source(b)) mapsrc |= 1 << b;
    }
    key = mapsrc | (sClippedBgMask << 4);
    if (key == last) return;
    last = key;
    fprintf(stderr, "[layers] task=%d sub=%d ui=%d area=0x%02X room=0x%02X "
                    "mapsrc_mask=0x%X clip_mask=0x%X dispcnt=0x%04X "
                    "bg0ctl=0x%04X bg1ctl=0x%04X bg2ctl=0x%04X bg3ctl=0x%04X\n",
            gMain.task, gMain.substate, gUI.lastState,
            gRoomControls.area, gRoomControls.room, mapsrc, sClippedBgMask,
            gScreen.lcd.displayControl, gScreen.bg0.control, gScreen.bg1.control,
            gScreen.bg2.control, gScreen.bg3.control);
}

/* TMC_BG3_TRACE=1: log every time BG3 is switched on or off during gameplay.
 *
 * BG3 carries the screen-fixed gameplay overlays (hole parallax, light/dark,
 * weather, steam, POW). It is off in ordinary rooms, and it never has a map
 * source — so the "no map source ⇒ clip" rule always catches it, which is
 * fine while it is off and is exactly what needs checking when it is not.
 * This is the sweep hook for that (docs/viewport-bug-tracker.md). */
static void mapsource_trace_bg3(void) {
    static int en = -1;
    static int lastOn = -1;
    static u8 lastArea = 0xFF, lastRoom = 0xFF;
    static unsigned frame = 0, onFrames = 0;
    int on;
    if (en < 0) en = (getenv("TMC_BG3_TRACE") != NULL);
    if (!en) return;
    /* Port_MapSource_Update runs once per VBlank, the same cadence as the
     * capture frame counter, so this lines up with --dump frame numbers. */
    frame++;
    on = (gScreen.lcd.displayControl & 0x0800) != 0;
    if (on) {
        onFrames++;
    }
    if (on == lastOn && gRoomControls.area == lastArea && gRoomControls.room == lastRoom) {
        return;
    }
    if (on || lastOn == 1) {
        fprintf(stderr, "[bg3] f=%-6u onFrames=%-5u %s task=%d sub=%d area=0x%02X room=0x%02X "
                        "bg3ctl=0x%04X ofs=(%d,%d) roomw=%u clipped=%d\n",
                frame, onFrames,
                on ? "ON " : "off", gMain.task, gMain.substate,
                gRoomControls.area, gRoomControls.room, gScreen.bg3.control,
                (int)gScreen.bg3.xOffset, (int)gScreen.bg3.yOffset,
                gRoomControls.width, (sClippedBgMask >> 3) & 1);
    }
    lastOn = on;
    lastArea = gRoomControls.area;
    lastRoom = gRoomControls.room;
}

/* TMC_REJECT_TRACE=1: why each world layer was refused a map source. */
static void mapsource_trace_reject(void) {
    static int en = -1;
    static int lastR0 = -99, lastR1 = -99;
    if (en < 0) en = (getenv("TMC_REJECT_TRACE") != NULL);
    if (!en) return;
    if (sLastReason[0] != lastR0 || sLastReason[1] != lastR1) {
        lastR0 = sLastReason[0]; lastR1 = sLastReason[1];
        fprintf(stderr, "[reject] task=%d substate=%d uiState=%d area=0x%02X room=0x%02X "
                        "w=%u sf=0x%02X sa=%u -> bottom=%s top=%s\n",
                gMain.task, gMain.substate, gUI.lastState,
                gRoomControls.area, gRoomControls.room, gRoomControls.width,
                gRoomControls.scroll_flags, gRoomControls.scrollAction,
                Port_MapSource_ReasonName(sLastReason[0]),
                Port_MapSource_ReasonName(sLastReason[1]));
    }
}

void Port_MapSource_Update(void) {
    int layer;
    /* Clear every layer first: which BG a map binds to can change between
     * frames, so a stale binding on a BG this frame's layers no longer use
     * would keep sampling after the engine moved on. */
    { void Port_MapSource_CamTrace(void); Port_MapSource_CamTrace(); }
    virtuappu_mode1_clear_map_sources();
    for (layer = 0; layer < 2; layer++) {
        int reason = mapsource_reason(layer);
        int bg;
        sLastReason[layer] = reason;
        if (reason != REASON_BOUND) {
            sRejectFrames[layer][reason]++;
            continue;
        }
        bg = mapsource_bg_index((layer == 0) ? gMapBottom.bgSettings
                                             : gMapTop.bgSettings);
        sBoundFrames[layer]++;
        if (getenv("TMC_MAPSRC_DIAG") != NULL && sBoundFrames[layer] % 400 == 1) {
            const u16* m = (layer == 0) ? gMapDataBottomSpecial : gMapDataTopSpecial;
            int cx = (int)gRoomControls.scroll_x - (int)gRoomControls.origin_x;
            int cy = (int)gRoomControls.scroll_y - (int)gRoomControls.origin_y;
            int bgcnt = (int)((layer == 0) ? gScreen.bg2.control : gScreen.bg1.control);
            int sb = (bgcnt >> 8) & 0x1F;
            const u16* blk = (const u16*)(gVram + sb * 0x800);
            int hofs = 0, vofs = 0, agree = 0, tot = 0, sx, sy;
            hofs = (layer == 0) ? gScreen.bg2.xOffset : gScreen.bg1.xOffset;
            vofs = (layer == 0) ? gScreen.bg2.yOffset : gScreen.bg1.yOffset;
            for (sy = 0; sy < 20; sy++) {
                for (sx = 0; sx < 30; sx++) {
                    int mx = (cx >> 3) + sx, my = (cy >> 3) + sy;
                    int vr = (((vofs >> 3) + sy) & 0x1F), vc = (((hofs >> 3) + sx) & 0x1F);
                    tot++;
                    if (blk[vr * 32 + vc] == m[my * MAPSRC_STRIDE + mx]) agree++;
                }
            }
            fprintf(stderr, "[mapsrc-diag] area=0x%02X room=0x%02X layer=%d bg=%d "
                            "room=%ux%u cam=(%d,%d) ofs=(%d,%d) sb=%d agree=%d/%d\n",
                    gRoomControls.area, gRoomControls.room, layer, bg,
                    gRoomControls.width, gRoomControls.height, cx, cy, hofs, vofs, sb,
                    agree, tot);
        }
        {
            /* origin = camera position in room pixels. This is exactly the
             * world pixel the hardware path resolves for screen column 0:
             * the engine keeps hofs = cx & 15 and vofs = (cy & 15) + 8 over
             * a *sliding* (non-wrapping) window, and the two addressings
             * reduce to the same map entry. That identity is what makes the
             * map-sampled output bit-identical at GBA-native size. */
            VirtuaPPUMode1MapSource src;
            src.map = (layer == 0) ? gMapDataBottomSpecial : gMapDataTopSpecial;
            src.stride = MAPSRC_STRIDE;
            src.width_tiles = gRoomControls.width >> 3;
            src.height_tiles = gRoomControls.height >> 3;
            src.origin_x = (int)gRoomControls.scroll_x - (int)gRoomControls.origin_x;
            src.origin_y = (int)gRoomControls.scroll_y - (int)gRoomControls.origin_y;
            virtuappu_mode1_set_map_source(bg, &src);
        }
    }

    /* The UI clip rule must run *after* the world bindings: it asks which
     * layers have a map source, and a layer bound earlier in this same
     * function would otherwise be seen as unbound and wrongly clipped to the
     * authored width — which silently confined the whole world to 240 and
     * made cutscenes look centred even while the trace said "bound".
     *
     * It must also run *unconditionally*, on every frame. An earlier attempt
     * fixed the ordering by moving this call into Port_MapSource_CamTrace,
     * which is a diagnostic: it returns early unless TMC_CAMTRACE is set and
     * the room has just changed. That did order it after the bindings, but it
     * also meant the clips were never applied in an ordinary run — which is
     * what regressed B2 (legend artwork repeating past x=240) and disarmed
     * the sprite clip and offset with it. */
    mapsource_bind_ui();
    mapsource_trace_reject();
    mapsource_trace_layers();
    mapsource_trace_bg3();
}

/* Spike 5: per-room camera-range report (TMC_CAMTRACE=1). Confirms the
 * horizontal clamp keeps the camera inside its room and that rooms
 * narrower than the viewport sit centred. */
void Port_MapSource_CamTrace(void) {
    static u8 lastArea = 0xFF, lastRoom = 0xFF;
    static int enabled = -1;
    if (enabled < 0) enabled = (getenv("TMC_CAMTRACE") != NULL);
    if (!enabled) return;
    if (gRoomControls.area == lastArea && gRoomControls.room == lastRoom) return;
    if (gMain.task != TASK_GAME || gRoomControls.width == 0) return;
    lastArea = gRoomControls.area; lastRoom = gRoomControls.room;
    {
        int cx = (int)gRoomControls.scroll_x - (int)gRoomControls.origin_x;
        int mnx = VIEWPORT_CAM_MIN_X(0, gRoomControls.width);
        int mxx = VIEWPORT_CAM_MAX_X(0, gRoomControls.width);
        int narrowx = ((int)gRoomControls.width <= VIEWPORT_WIDTH);
        int cy = (int)gRoomControls.scroll_y - (int)gRoomControls.origin_y;
        int mny = VIEWPORT_CAM_MIN_Y(0, gRoomControls.height);
        int mxy = VIEWPORT_CAM_MAX_Y(0, gRoomControls.height);
        int shorty = ((int)gRoomControls.height <= VIEWPORT_HEIGHT);
        fprintf(stderr,
                "[camtrace] area=0x%02X room=0x%02X %ux%u viewport=%dx%d "
                "camx=%d rangex=[%d,%d] %s%s | camy=%d rangey=[%d,%d] %s%s\n",
                gRoomControls.area, gRoomControls.room,
                gRoomControls.width, gRoomControls.height,
                VIEWPORT_WIDTH, VIEWPORT_HEIGHT,
                cx, mnx, mxx, narrowx ? "NARROW(centred)" : "scrollable",
                (cx < mnx || cx > mxx) ? "  ** X OUT OF RANGE **" : "",
                cy, mny, mxy, shorty ? "SHORT(centred)" : "scrollable",
                (cy < mny || cy > mxy) ? "  ** Y OUT OF RANGE **" : "");
    }
}

void Port_MapSource_Report(void) {
    int layer;
    for (layer = 0; layer < 2; layer++) {
        int r;
        fprintf(stderr, "[mapsource] layer %d (%s): bound %u frames",
                layer, layer == 0 ? "bottom/BG2" : "top/BG1", sBoundFrames[layer]);
        for (r = 0; r < REASON_COUNT; r++) {
            if (sRejectFrames[layer][r]) {
                fprintf(stderr, ", %s %u", kReasonName[r], sRejectFrames[layer][r]);
            }
        }
        fprintf(stderr, "\n");
    }
}

/* Temporary Spike 3 audit hook (see mode1.c): compares every map-sampled
 * tile fetch against the screenblock entry the hardware path would use. */
extern int mode1_map_source_audit;
extern unsigned long mode1_map_source_audit_total, mode1_map_source_audit_bad;
void Port_MapSource_AuditEnable(void) { mode1_map_source_audit = 1; }
void Port_MapSource_AuditReport(void) {
    fprintf(stderr, "[mapsrc-audit] fetches=%lu mismatched=%lu\n",
            mode1_map_source_audit_total, mode1_map_source_audit_bad);
}

int Port_MapSource_LayerBgIndex(int layer) {
    const BgSettings* s = ((layer & 1) == 0) ? gMapBottom.bgSettings : gMapTop.bgSettings;
    return (s == NULL) ? -1 : mapsource_bg_index(s);
}
