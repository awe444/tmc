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
        default:
            /* AUXCUTSCENE, PORTALCUTSCENE, WORLDEVENT, FASTTRAVEL: world. */
            return false;
    }
}

/* B1: the text box centres itself per-window (message.c) because it shares
 * BG0 with the edge-anchored HUD during gameplay. On a UI screen the whole
 * BG0 layer is already shifted, so applying the per-window shift as well
 * moves the box twice — which is what clipped the "Saving file..." and
 * "Erasing file..." popups on the file-select and pause-menu screens. */
int Port_MapSource_MessageTileShift(void) {
#if UI_CENTER_TILE_DX > 0
    return mapsource_is_ui_screen() ? 0 : UI_CENTER_TILE_DX;
#else
    return 0;
#endif
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
#if UI_CENTER_DX > 0
    bool ui_screen = mapsource_is_ui_screen();
    VirtuaPPUMode1BgClip clip;
    int bg;

    virtuappu_mode1_clear_bg_clips();
    sUiCentered = ui_screen;

    /* BG0 always gets its map source: the buffer is wider than a hardware
     * screenblock, so the VRAM copy the screenblock path reads is the wrong
     * shape. Only its origin depends on whether this is a UI screen. */
    {
        VirtuaPPUMode1MapSource src;
        src.map = gBG0Buffer;
        src.stride = UI_BG0_WIDTH_TILES;
        src.width_tiles = UI_BG0_WIDTH_TILES;
        src.height_tiles = UI_BG0_HEIGHT_TILES;
        src.origin_x = (int)gScreen.bg0.xOffset - (ui_screen ? UI_CENTER_DX : 0);
        src.origin_y = (int)gScreen.bg0.yOffset;
        virtuappu_mode1_set_map_source(0, &src);
    }

    /* Sprites travel with the layers they sit on. On a UI screen everything
     * is centred, so the sprites are too. In a world view they stay put and
     * are instead confined to the room's on-screen span below. */
    virtuappu_mode1_set_obj_offset(ui_screen ? UI_CENTER_DX : 0, 0);

    clip.offset_x = UI_CENTER_DX;
    clip.content_width = DISPLAY_WIDTH;
    for (bg = 1; bg < 4; bg++) {
        if (!virtuappu_mode1_has_map_source(bg)) {
            virtuappu_mode1_set_bg_clip(bg, &clip);
        }
    }

    if (ui_screen) {
        virtuappu_mode1_set_obj_clip(UI_CENTER_DX, UI_CENTER_DX + DISPLAY_WIDTH);
        return;
    }

    /* World view: confine sprites to the room's on-screen span so the
     * border stays border. A room narrower than the viewport is centred, so
     * the columns either side are outside the room — hardware never had
     * such columns, and an entity standing there would not have been drawn
     * (the stray Zelda sprite in the left border). During a room-to-room
     * scroll the world layers have fallen back to the screenblock path and
     * are clipped to native width above, so the sprites match that. */
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
    mapsource_bind_ui();
    mapsource_trace_reject();
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
        int mn = VIEWPORT_CAM_MIN_X(0, gRoomControls.width);
        int mx = VIEWPORT_CAM_MAX_X(0, gRoomControls.width);
        int narrow = ((int)gRoomControls.width <= VIEWPORT_WIDTH);
        fprintf(stderr,
                "[camtrace] area=0x%02X room=0x%02X w=%u viewport=%d cam=%d range=[%d,%d] %s%s\n",
                gRoomControls.area, gRoomControls.room, gRoomControls.width,
                VIEWPORT_WIDTH, cx, mn, mx,
                narrow ? "NARROW(centred)" : "scrollable",
                (cx < mn || cx > mx) ? "  ** OUT OF RANGE **" : "");
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
