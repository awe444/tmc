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
#include "tileMap.h"
#include "vram.h"

#include "cpu/mode1.h"

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
    if (gMain.substate != GAMEMAIN_UPDATE) {
        return REASON_SUBSTATE;
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
        if (cx < 0 || cy < 0 || tw <= 0 || th <= 0 ||
            tw > MAPSRC_MAX_TILES || th > MAPSRC_MAX_TILES) {
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

void Port_MapSource_Update(void) {
    int layer;
    /* Clear every layer first: which BG a map binds to can change between
     * frames, so a stale binding on a BG this frame's layers no longer use
     * would keep sampling after the engine moved on. */
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
