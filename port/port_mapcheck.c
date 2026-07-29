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

/* game.h's GAMEMAIN_* enum lives next to heavy includes; the two values
 * the predicate needs are stable engine constants. */
#define MAPCHECK_GAMEMAIN_UPDATE 2  /* GAMEMAIN_UPDATE  (include/game.h) */

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

/* Compare one layer; returns mismatches counted (also updates totals). */
static uint32_t mapcheck_compare_layer(uint32_t frame, int layer) {
    /* Measured display binding (self-located via the DIAG candidate scan,
     * 600/600 tile agreement): gMapDataBottomSpecial renders through the
     * BG2 register set, gMapDataTopSpecial through BG1 — BG1's higher
     * priority puts the canopy/top layer above the ground layer. NOTE:
     * this is the opposite of what port_linked_stubs.c's UpdateScrollVram
     * comment claims; trust the registers, not the comment. */
    const u16* special = (layer == 0) ? gMapDataBottomSpecial : gMapDataTopSpecial;
    uint16_t cnt = io_read16(layer == 0 ? REG_OFF_BG2CNT : REG_OFF_BG1CNT);
    uint16_t hofs = io_read16(layer == 0 ? REG_OFF_BG2HOFS : REG_OFF_BG1HOFS) & 0x1FF;
    uint16_t vofs = io_read16(layer == 0 ? REG_OFF_BG2VOFS : REG_OFF_BG1VOFS) & 0x1FF;
    const uint16_t* block = (const uint16_t*)(gVram + (((cnt >> 8) & 0x1F) * 0x800u));

    int cx = (int)(gRoomControls.scroll_x - gRoomControls.origin_x);
    int cy = (int)(gRoomControls.scroll_y - gRoomControls.origin_y);
    if (cx < 0 || cy < 0) {
        return 0;
    }

    /* The BG window is a *sliding* buffer, not a wrapping one: the
     * incremental streamers memmove content and the engine keeps
     * hofs = cx & 15 and vofs = (cy & 15) + 8 (a constant one-tile
     * vertical bias). Screen tile (sx,sy) is therefore at buffer cell
     * ((vofs>>3)+sy, (hofs>>3)+sx) with no wrap. Gate on the fine-scroll
     * bits agreeing so transition frames don't produce garbage counts. */
    if (((hofs ^ (uint32_t)cx) & 7) != 0 || ((vofs ^ (uint32_t)cy) & 7) != 0) {
        sPhaseMismatchFrames++;
        return 0;
    }

    int roomTilesW = gRoomControls.width >> 3;
    int roomTilesH = gRoomControls.height >> 3;
    int tx0 = cx >> 3;
    int ty0 = cy >> 3;

    uint32_t mism = 0;
    for (int sy = 0; sy < 20; sy++) {
        int my = ty0 + sy;
        if (my >= roomTilesH || my >= 128) {
            break;
        }
        int vrow = ((vofs >> 3) + sy) & 0x1F;
        for (int sx = 0; sx < 30; sx++) {
            int mx = tx0 + sx;
            if (mx >= roomTilesW || mx >= 128) {
                break;
            }
            int vcol = ((hofs >> 3) + sx) & 0x1F;
            uint16_t expect = special[my * 0x80 + mx];
            uint16_t actual = block[vrow * 32 + vcol];
            sTilesCompared++;
            if (expect != actual) {
                mism++;
                sTilesMismatched++;
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
            "[mapcheck] tile mutations: SetTileType=%u SetTileByIndex=%u "
            "RestorePrevTileEntity=%u\n",
            gPort_TileMutationCount[0], gPort_TileMutationCount[1],
            gPort_TileMutationCount[2]);
    for (int i = 0; i < 6; i++) {
        if (sFramesSkipped[i]) {
            fprintf(stderr, "[mapcheck]   skipped (%s): %u frames\n",
                    kSkipReason[i], sFramesSkipped[i]);
        }
    }
}
