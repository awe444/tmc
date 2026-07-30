/* Spike 2 tile-diff harness: validates the special-map premise
 * (docs/viewport-expansion-research-plan.md §5) at runtime.
 *
 * Each frame, for every BG layer the map-authoritative predicate accepts,
 * compare the tilemap entries the PPU actually rendered (VRAM screenblock,
 * addressed through hardware scroll-register semantics) against what
 * full-room sampling of gMapData(Bottom|Top)Special at the camera origin
 * would produce. Any persistent mismatch is a layer or mutation path that
 * bypasses the special map — exactly what would break Option E.
 *
 * This file is also the reference implementation of the predicate itself
 * (see mapcheck_layer_authoritative below): Spike 3's PPU mode keys off
 * the same signals.
 *
 * Enabled with --mapcheck (parsed in port_capture.c). Output goes to
 * stderr: per-room-entry lines, mismatch reports, and an exit summary.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "gba/gba.h"
#include "main.h"
#include "map.h"
#include "room.h"
#include "screen.h"
#include "tileMap.h"
#include "vram.h"

#include "port_gba_mem.h"
#include "port_mapsource.h"
#include "cpu/mode1.h"

/* game.h's GAMEMAIN_* enum lives next to heavy includes; the two values
 * the predicate needs are stable engine constants. */
#define MAPCHECK_GAMEMAIN_UPDATE 2  /* GAMEMAIN_UPDATE  (include/game.h) */

#define REG_OFF_BG0CNT 0x08
#define REG_OFF_BG0HOFS 0x10
#define REG_OFF_BG0VOFS 0x12
#define REG_OFF_BG1CNT 0x0A
#define REG_OFF_BG2CNT 0x0C
#define REG_OFF_BG1HOFS 0x14
#define REG_OFF_BG1VOFS 0x16
#define REG_OFF_BG2HOFS 0x18
#define REG_OFF_BG2VOFS 0x1A

static bool sEnabled = false;

/* counters */
static uint32_t sFramesSeen = 0;         /* frames while enabled */
static uint32_t sFramesChecked = 0;      /* frames where >=1 layer compared */
static uint32_t sFramesSkipped[6] = {0}; /* by predicate-reject reason */
static uint32_t sFramesMismatched = 0;
static uint64_t sTilesCompared = 0;
static uint64_t sTilesMismatched = 0;
static uint32_t sPhaseMismatchFrames = 0;
static uint64_t sMismMapEmpty = 0;   /* special map 0, screenblock set */
static uint64_t sMismHwEmpty = 0;    /* special map set, screenblock 0 */
static uint64_t sMismBothSet = 0;    /* both set, different */

static const char* const kSkipReason[6] = {
    "task!=GAME", "substate!=UPDATE", "scroll_flags&1",
    "mid-scroll", "bg-rebound", "layer-off",
};

/* Persistent-mismatch tracking: (layer, mapx, mapy) -> consecutive frames.
 * Transient 1-2 frame diffs are expected (the VBlank upload of gBGxBuffer
 * happens after our present hook, so a same-frame SetTileType shows up
 * here one frame early). A streak >= threshold is a real bypass. */
#define STREAK_SLOTS 256
#define STREAK_REPORT_THRESHOLD 30
typedef struct {
    uint32_t key; /* layer<<28 | my<<14 | mx ; 0 = empty */
    uint32_t streak;
    uint32_t lastFrame;
    uint16_t expect, actual;
    bool reported;
} StreakSlot;
static StreakSlot sStreaks[STREAK_SLOTS];
static uint32_t sPersistentReported = 0;

static uint8_t sLastArea = 0xFF, sLastRoom = 0xFF;

/* --- Spike 2B measurements -------------------------------------------- */
/* Max per-frame camera delta on continuous segments (same room, no
 * transition scroller active) — the vertical number is compared against
 * the 16 px streaming slack a 240-tall window would leave. */
static int sPrevScrollX = -1, sPrevScrollY = -1;
static uint8_t sPrevDeltaArea = 0xFF, sPrevDeltaRoom = 0xFF;
static int sMaxDx = 0, sMaxDy = 0;
/* Delta bands: [0]=1-4 [1]=5-8 [2]=9-16 [3]=17-64 [4]=>64 (teleports). */
static uint32_t sDyBand[5], sDxBand[5];
static int sMaxDyContinuous = 0; /* max dy among deltas <= 64 */

/* OAM entries whose attr0.y sits in [161,239] while enabled: at 160-line
 * height these are sprites partially above the top edge (wrapped
 * negatives); at 240-line height the same encoding renders on-screen at
 * literal y — the concrete severity measure for blocker 3 (8-bit OAM Y). */
static uint32_t sOamHighYFrames = 0;
static uint32_t sOamHighYEntriesTotal = 0;
static uint32_t sOamHighYEntriesMax = 0;

/* --- Spike 7: off-screen behaviour ---------------------------------------
 * Two things the wider viewport could break, counted rather than eyeballed:
 *
 *  - OAM entries resolving into the *expanded* columns (240..VIEWPORT-1).
 *    Spike 2A established that unused entries are parked *disabled*
 *    (attr0 = 0x2A0), so leakage should be structurally impossible; this
 *    counts enabled sprites landing there, split by whether they are
 *    plausible content or suspiciously parked-looking.
 *  - Entities the engine considers off-screen that are now visible, i.e.
 *    pop-in: an enabled sprite appearing in the expanded columns on the
 *    same frame it first becomes visible.
 */
static uint32_t sExpandedColFrames = 0;
static uint32_t sExpandedColEntries = 0;
static uint32_t sExpandedColMax = 0;
static uint32_t sParkedLookalikes = 0;

static void spike7_sample(void) {
    uint32_t n = 0;
    int i;
    if (gMain.task != TASK_GAME || MODE1_GBA_WIDTH <= 240) {
        return;
    }
    for (i = 0; i < 0x80; i++) {
        uint16_t attr0, attr1;
        int x;
        memcpy(&attr0, (const u8*)&gOAMControls.oam[i] + 0, 2);
        memcpy(&attr1, (const u8*)&gOAMControls.oam[i] + 2, 2);
        if ((attr0 & 0x0300) == 0x0200) {
            continue; /* disabled, including the 0x2A0 parking pattern */
        }
        x = attr1 & 0x1FF;
        if (x >= MODE1_GBA_WIDTH) {
            x -= 512;
        }
        if (x >= 240 && x < MODE1_GBA_WIDTH) {
            n++;
            /* attr0 y == 160 with no other content is the parking y; an
             * enabled entry there is the leakage the old patch feared. */
            if ((attr0 & 0xFF) == 0xA0) {
                sParkedLookalikes++;
            }
        }
    }
    if (n > 0) {
        sExpandedColFrames++;
        sExpandedColEntries += n;
        if (n > sExpandedColMax) sExpandedColMax = n;
    }
}

static void spike2b_sample(void) {
    /* camera deltas: only within one room and outside transition scrolls */
    if (gMain.task == TASK_GAME && gRoomControls.scrollAction < 2 &&
        gRoomControls.area == sPrevDeltaArea && gRoomControls.room == sPrevDeltaRoom &&
        sPrevScrollX >= 0) {
        int dx = gRoomControls.scroll_x - sPrevScrollX;
        int dy = gRoomControls.scroll_y - sPrevScrollY;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        if (dx > sMaxDx) sMaxDx = dx;
        if (dy > sMaxDy) sMaxDy = dy;
        if (dy <= 64 && dy > sMaxDyContinuous) sMaxDyContinuous = dy;
        if (dy > 0) sDyBand[dy <= 4 ? 0 : dy <= 8 ? 1 : dy <= 16 ? 2 : dy <= 64 ? 3 : 4]++;
        if (dx > 0) sDxBand[dx <= 4 ? 0 : dx <= 8 ? 1 : dx <= 16 ? 2 : dx <= 64 ? 3 : 4]++;
    }
    sPrevScrollX = gRoomControls.scroll_x;
    sPrevScrollY = gRoomControls.scroll_y;
    sPrevDeltaArea = gRoomControls.area;
    sPrevDeltaRoom = gRoomControls.room;

    /* OAM high-Y census over the engine-side table for this frame */
    if (gMain.task == TASK_GAME) {
        uint32_t n = 0;
        for (int i = 0; i < 0x80; i++) {
            uint16_t attr0;
            memcpy(&attr0, &gOAMControls.oam[i], 2);
            if ((attr0 & 0x0300) == 0x0200) {
                continue; /* disabled (incl. the 0x2A0 parking pattern) */
            }
            uint32_t y = attr0 & 0xFF;
            if (y > 160 && y < 240) {
                n++;
            }
        }
        if (n > 0) {
            sOamHighYFrames++;
            sOamHighYEntriesTotal += n;
            if (n > sOamHighYEntriesMax) sOamHighYEntriesMax = n;
        }
    }
}

static void streak_note(uint32_t frame, int layer, int mx, int my,
                        uint16_t expect, uint16_t actual) {
    uint32_t key = ((uint32_t)layer << 28) | ((uint32_t)my << 14) | (uint32_t)mx;
    int freeIdx = -1;
    for (int i = 0; i < STREAK_SLOTS; i++) {
        if (sStreaks[i].key == key) {
            if (sStreaks[i].lastFrame == frame - 1) {
                sStreaks[i].streak++;
            } else {
                sStreaks[i].streak = 1;
                sStreaks[i].reported = false;
            }
            sStreaks[i].lastFrame = frame;
            sStreaks[i].expect = expect;
            sStreaks[i].actual = actual;
            if (sStreaks[i].streak == STREAK_REPORT_THRESHOLD && !sStreaks[i].reported) {
                sStreaks[i].reported = true;
                sPersistentReported++;
                fprintf(stderr,
                        "[mapcheck] PERSISTENT MISMATCH frame=%u layer=%d map=(%d,%d) "
                        "special=0x%04X vram=0x%04X area=0x%02X room=0x%02X\n",
                        frame, layer, mx, my, expect, actual, sLastArea, sLastRoom);
            }
            return;
        }
        if (freeIdx < 0 && sStreaks[i].key == 0) {
            freeIdx = i;
        }
    }
    if (freeIdx >= 0) {
        sStreaks[freeIdx].key = key;
        sStreaks[freeIdx].streak = 1;
        sStreaks[freeIdx].lastFrame = frame;
        sStreaks[freeIdx].expect = expect;
        sStreaks[freeIdx].actual = actual;
        sStreaks[freeIdx].reported = false;
    }
}

/* --- the predicate -----------------------------------------------------
 * Layer 0 = bottom (gMapDataBottomSpecial -> gBG1Buffer -> BG1)
 * Layer 1 = top    (gMapDataTopSpecial    -> gBG2Buffer -> BG2)
 * Returns skip-reason index (>=0) if NOT authoritative, -1 if it is. */
static int mapcheck_layer_authoritative(int layer) {
    if (gMain.task != TASK_GAME) {
        return 0;
    }
    if (gMain.substate != MAPCHECK_GAMEMAIN_UPDATE) {
        return 1;
    }
    if (gRoomControls.scroll_flags & 1) {
        return 2;
    }
    /* scrollAction 0/1 are normal camera follow (Scroll0 promotes to 1 on
     * its first frame); >= 2 are the room-to-room transition scrollers
     * (Scroll2/4/5), during which the window blends two rooms' content. */
    if (gRoomControls.scrollAction >= 2) {
        return 3;
    }
    if (layer == 0) {
        if (gScreen.bg1.subTileMap != &gBG1Buffer) {
            return 4;
        }
        if (gMapBottom.bgSettings == NULL) {
            return 5;
        }
    } else {
        if (gScreen.bg2.subTileMap != &gBG2Buffer) {
            return 4;
        }
        if (gMapTop.bgSettings == NULL) {
            return 5;
        }
    }
    return -1;
}

static uint16_t io_read16(int off) {
    return (uint16_t)(gIoMem[off] | (gIoMem[off + 1] << 8));
}

/* Compare one layer; returns mismatches counted (also updates totals).
 *
 * Addressing is derived the way the renderer derives it, from the live BG
 * registers — NOT from a model of what the engine "should" have put in
 * them. An earlier version assumed hofs == cx & 15 and vofs == (cy&15)+8
 * and skipped frames that disagreed; since disagreement is exactly the
 * interesting case, that gate silently excluded the frames most likely to
 * show a bypass. It is gone.
 *
 * Which BG a map layer displays through is per-room, so it comes from
 * MapLayer.bgSettings via Port_MapSource_LayerBgIndex rather than being
 * hardcoded. */
static uint32_t mapcheck_compare_layer(uint32_t frame, int layer) {
    const u16* special = (layer == 0) ? gMapDataBottomSpecial : gMapDataTopSpecial;
    int bg = Port_MapSource_LayerBgIndex(layer);
    if (bg < 0) {
        return 0;
    }
    uint16_t cnt = io_read16(REG_OFF_BG0CNT + bg * 2);
    uint16_t hofs = io_read16(REG_OFF_BG0HOFS + bg * 4) & 0x1FF;
    uint16_t vofs = io_read16(REG_OFF_BG0VOFS + bg * 4) & 0x1FF;
    const uint16_t* block = (const uint16_t*)(gVram + (((cnt >> 8) & 0x1F) * 0x800u));
    uint16_t size_flag = (uint16_t)((cnt >> 14) & 3);
    int map_w = (size_flag & 1) ? 64 : 32;
    int map_h = (size_flag & 2) ? 64 : 32;

    int cx = (int)(gRoomControls.scroll_x - gRoomControls.origin_x);
    int cy = (int)(gRoomControls.scroll_y - gRoomControls.origin_y);
    int roomTilesW = gRoomControls.width >> 3;
    int roomTilesH = gRoomControls.height >> 3;
    if (cx < 0 || cy < 0) {
        return 0;
    }

    /* Walk every tile the renderer touches, including the partial tiles at
     * the right/bottom edges that a 30x20 loop misses. */
    uint32_t mism = 0;
    for (int line = 0; line < MODE1_GBA_HEIGHT; line += 8) {
        for (int x = 0; x < MODE1_GBA_WIDTH; x += 8) {
            int my = (cy + line) >> 3;
            int mx = (cx + x) >> 3;
            if (my >= roomTilesH || mx >= roomTilesW || my >= 128 || mx >= 128) {
                continue;
            }
            int r = ((line + vofs) % (map_h * 8)) / 8;
            int c = ((x + hofs) % (map_w * 8)) / 8;
            int bi = (c / 32) + (r / 32) * (map_w / 32);
            uint16_t expect = special[my * 0x80 + mx];
            uint16_t actual = block[bi * 1024 + (r % 32) * 32 + (c % 32)];
            sTilesCompared++;
            if (expect != actual) {
                mism++;
                sTilesMismatched++;
                if (expect == 0 && actual != 0) {
                    sMismMapEmpty++;
                } else if (expect != 0 && actual == 0) {
                    sMismHwEmpty++;
                } else {
                    sMismBothSet++;
                }
                streak_note(frame, layer, mx, my, expect, actual);
            }
        }
    }
    return mism;
}

void Port_MapCheck_Enable(void) {
    sEnabled = true;
    fprintf(stderr, "[mapcheck] enabled\n");
}

void Port_MapCheck_OnFrame(uint32_t frame) {
    if (!sEnabled) {
        return;
    }
    sFramesSeen++;
    spike2b_sample();
    spike7_sample();

    if (gRoomControls.area != sLastArea || gRoomControls.room != sLastRoom) {
        sLastArea = gRoomControls.area;
        sLastRoom = gRoomControls.room;
        fprintf(stderr,
                "[mapcheck] frame=%u enter area=0x%02X room=0x%02X size=%ux%u "
                "scroll_flags=0x%02X bg1=%s bg2=%s\n",
                frame, sLastArea, sLastRoom, gRoomControls.width, gRoomControls.height,
                gRoomControls.scroll_flags,
                gScreen.bg1.subTileMap == &gBG1Buffer ? "std"
                : (gScreen.bg1.subTileMap == NULL ? "null" : "REBOUND"),
                gScreen.bg2.subTileMap == &gBG2Buffer ? "std"
                : (gScreen.bg2.subTileMap == NULL ? "null" : "REBOUND"));
    }

    if (frame == 8400 || frame == 10000) { /* one-shot diagnostics */
        int cx = gRoomControls.scroll_x - gRoomControls.origin_x;
        int cy = gRoomControls.scroll_y - gRoomControls.origin_y;
        fprintf(stderr, "[mapcheck] DIAG frame=%u cx=%d cy=%d bg1ctl=0x%04X bg2ctl=0x%04X "
                        "bg1off=(%d,%d) bg2off=(%d,%d) io_hofs1=0x%04X io_vofs1=0x%04X\n",
                frame, cx, cy, gScreen.bg1.control, gScreen.bg2.control,
                gScreen.bg1.xOffset, gScreen.bg1.yOffset,
                gScreen.bg2.xOffset, gScreen.bg2.yOffset,
                io_read16(REG_OFF_BG1HOFS), io_read16(REG_OFF_BG1VOFS));
        /* For each screenblock and each row phase -2..+2, count how many of
         * the visible-window special-map entries match: self-locates the
         * real block+phase convention. */
        for (int b = 0; b < 32; b++) {
            const uint16_t* blk = (const uint16_t*)(gVram + b * 0x800u);
            for (int ph = -2; ph <= 2; ph++) {
                uint32_t hit = 0, tot = 0;
                for (int sy = 0; sy < 20; sy++) {
                    for (int sx = 0; sx < 30; sx++) {
                        int mx = (cx >> 3) + sx, my = (cy >> 3) + sy;
                        if (mx >= 128 || my >= 128) continue;
                        int vr = (my + ph) & 0x1F, vc = mx & 0x1F;
                        tot++;
                        hit += blk[vr * 32 + vc] == gMapDataBottomSpecial[my * 0x80 + mx];
                    }
                }
                if (tot && hit * 100 >= tot * 60) {
                    fprintf(stderr, "[mapcheck]   candidate BOTTOM: block=%d phase=%+d hits=%u/%u\n",
                            b, ph, hit, tot);
                }
                hit = 0; tot = 0;
                for (int sy = 0; sy < 20; sy++) {
                    for (int sx = 0; sx < 30; sx++) {
                        int mx = (cx >> 3) + sx, my = (cy >> 3) + sy;
                        if (mx >= 128 || my >= 128) continue;
                        int vr = (my + ph) & 0x1F, vc = mx & 0x1F;
                        tot++;
                        hit += blk[vr * 32 + vc] == gMapDataTopSpecial[my * 0x80 + mx];
                    }
                }
                if (tot && hit * 100 >= tot * 60) {
                    fprintf(stderr, "[mapcheck]   candidate TOP:    block=%d phase=%+d hits=%u/%u\n",
                            b, ph, hit, tot);
                }
            }
        }
    }

    bool checkedAny = false;
    uint32_t mism = 0;
    for (int layer = 0; layer < 2; layer++) {
        int reason = mapcheck_layer_authoritative(layer);
        if (reason >= 0) {
            if (layer == 0) {
                sFramesSkipped[reason]++;
            }
            continue;
        }
        checkedAny = true;
        mism += mapcheck_compare_layer(frame, layer);
    }
    if (checkedAny) {
        sFramesChecked++;
        if (mism > 0) {
            sFramesMismatched++;
            if (sFramesMismatched <= 20) {
                fprintf(stderr,
                        "[mapcheck] frame=%u transient: %u mismatched tiles "
                        "(area=0x%02X room=0x%02X)\n",
                        frame, mism, sLastArea, sLastRoom);
            }
        }
    }
}

void Port_MapCheck_Report(void) {
    extern u32 gPort_TileMutationCount[3]; /* src/playerUtils.c tap */
    if (!sEnabled) {
        return;
    }
    fprintf(stderr,
            "[mapcheck] summary: frames seen=%u checked=%u mismatch-frames=%u "
            "tiles=%llu mismatched=%llu persistent=%u phase-skips=%u\n",
            sFramesSeen, sFramesChecked, sFramesMismatched,
            (unsigned long long)sTilesCompared, (unsigned long long)sTilesMismatched,
            sPersistentReported, sPhaseMismatchFrames);
    fprintf(stderr,
            "[mapcheck] mismatch classes: map-empty/hw-set=%llu map-set/hw-empty=%llu "
            "both-set-differ=%llu\n",
            (unsigned long long)sMismMapEmpty, (unsigned long long)sMismHwEmpty,
            (unsigned long long)sMismBothSet);
    fprintf(stderr,
            "[mapcheck] tile mutations: SetTileType=%u SetTileByIndex=%u "
            "RestorePrevTileEntity=%u\n",
            gPort_TileMutationCount[0], gPort_TileMutationCount[1],
            gPort_TileMutationCount[2]);
    fprintf(stderr,
            "[mapcheck] spike2b: max |dscroll| x=%d y=%d; max continuous dy=%d; "
            "dy bands 1-4/5-8/9-16/17-64/>64 = %u/%u/%u/%u/%u; "
            "dx bands = %u/%u/%u/%u/%u\n",
            sMaxDx, sMaxDy, sMaxDyContinuous,
            sDyBand[0], sDyBand[1], sDyBand[2], sDyBand[3], sDyBand[4],
            sDxBand[0], sDxBand[1], sDxBand[2], sDxBand[3], sDxBand[4]);
    fprintf(stderr,
            "[mapcheck] spike7: OAM in expanded cols 240..%d: frames=%u entries=%u "
            "max-simultaneous=%u parked-lookalikes=%u\n",
            MODE1_GBA_WIDTH - 1, sExpandedColFrames, sExpandedColEntries,
            sExpandedColMax, sParkedLookalikes);
    fprintf(stderr,
            "[mapcheck] spike2b: OAM high-Y (enabled, y in 161..239): frames=%u "
            "entries-total=%u max-simultaneous=%u\n",
            sOamHighYFrames, sOamHighYEntriesTotal, sOamHighYEntriesMax);
    for (int i = 0; i < 6; i++) {
        if (sFramesSkipped[i]) {
            fprintf(stderr, "[mapcheck]   skipped (%s): %u frames\n",
                    kSkipReason[i], sFramesSkipped[i]);
        }
    }
}
