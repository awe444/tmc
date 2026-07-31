#include "port_hdma.h"

#include "viewport.h"
#include "port_types.h"
#include "port_gba_mem.h"

#include <cpu/mode1.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HDMA_CHANNELS 4

#define DMA_CNT_DEST_FIXED  0x0040
#define DMA_CNT_DEST_RELOAD 0x0060
#define DMA_CNT_DEST_MASK   0x0060
#define DMA_CNT_SRC_FIXED   0x0100
#define DMA_CNT_32BIT       0x0400

/* Three GBA destination-increment modes per scanline:
 *   FIXED:  no increment during transfer, no reload between transfers
 *   INC:    increment during transfer, no reload between transfers
 *   RELOAD: increment during transfer, *do* reload between transfers
 * The previous implementation conflated FIXED and RELOAD, which made the
 * 8-u16 affine-matrix HBlank-DMA used by the rolling barrel and similar
 * scenes write all 8 values to BG2PA, leaving BG2PB..Y_H untouched. */
typedef enum {
    DEST_INC = 0,
    DEST_FIXED,
    DEST_RELOAD,
} HdmaDestMode;

typedef struct {
    int active;
    const uint8_t* src_orig;
    const uint8_t* src;
    uint8_t* dest_orig;
    uint8_t* dest;
    uint16_t count;     // units per HBlank transfer
    uint8_t  unit;      // 2 or 4 bytes
    uint8_t  src_fixed;
    uint8_t  dest_mode; // HdmaDestMode
} HdmaChannel;

static HdmaChannel s_channels[HDMA_CHANNELS];

/* TMC_HDMA_TRACE=1 — one line per distinct (dest, count) pair registered.
 * Spike 9's registration inventory, taken at runtime rather than by grep:
 * the destination register says which effect it is and `count` says how many
 * halfwords per scanline it consumes, which is what decides whether the
 * table fits its buffer at a taller viewport. */
static void hdma_trace_register(void* dest, uint16_t count, uint16_t cnt_h)
{
    static const void* seen_dest[16];
    static uint16_t seen_count[16];
    static int seen_n = 0;
    int i;

    if (getenv("TMC_HDMA_TRACE") == NULL) {
        return;
    }
    for (i = 0; i < seen_n; ++i) {
        if (seen_dest[i] == dest && seen_count[i] == count) {
            return;
        }
    }
    if (seen_n < (int)(sizeof(seen_dest) / sizeof(seen_dest[0]))) {
        seen_dest[seen_n] = dest;
        seen_count[seen_n] = count;
        seen_n++;
    }
    {
        /* Report the IO offset rather than the host pointer: 0x40 is WIN0H
         * (the circular windows), 0x1C BG3HOFS (light rays, steam, Vaati),
         * 0x0E BG3CNT (the pause map), 0x20 BG2PA (the rolling barrel). */
        const long off = (const uint8_t*)dest - (const uint8_t*)gIoMem;
        fprintf(stderr,
                "[hdma] register io_off=0x%02lX count=%u bytes/line=%u "
                "lines-for-%d=%u cnt_h=0x%04X\n",
                off, (unsigned)count,
                (unsigned)(count * ((cnt_h & DMA_CNT_32BIT) ? 4 : 2)),
                VIEWPORT_HEIGHT,
                (unsigned)(count * ((cnt_h & DMA_CNT_32BIT) ? 4 : 2) * VIEWPORT_HEIGHT),
                (unsigned)cnt_h);
    }
}

void port_hdma_register(int channel, const void* src, void* dest,
                        uint16_t cnt_h, uint16_t count)
{
    HdmaChannel* c;
    uint16_t dm;

    if (channel < 0 || channel >= HDMA_CHANNELS) {
        return;
    }
    hdma_trace_register(dest, count, cnt_h);
    c = &s_channels[channel];
    c->active = 1;
    c->src_orig = c->src = (const uint8_t*)src;
    c->dest_orig = c->dest = (uint8_t*)dest;
    c->count = count ? count : 1;
    c->unit = (cnt_h & DMA_CNT_32BIT) ? 4 : 2;
    c->src_fixed = (cnt_h & DMA_CNT_SRC_FIXED) ? 1 : 0;
    dm = cnt_h & DMA_CNT_DEST_MASK;
    if (dm == DMA_CNT_DEST_FIXED) {
        c->dest_mode = DEST_FIXED;
    } else if (dm == DMA_CNT_DEST_RELOAD) {
        c->dest_mode = DEST_RELOAD;
    } else {
        c->dest_mode = DEST_INC;
    }
}

void port_hdma_unregister(int channel)
{
    if (channel < 0 || channel >= HDMA_CHANNELS) {
        return;
    }
    s_channels[channel].active = 0;
}

int port_hdma_has_active_channels(void)
{
    int ch;

    for (ch = 0; ch < HDMA_CHANNELS; ++ch) {
        if (s_channels[ch].active) {
            return 1;
        }
    }
    return 0;
}

/* IO offset of WIN0H, the only per-scanline window register TMC drives. */
#define HDMA_IO_OFF_WIN0H 0x40

static long hdma_io_offset(const uint8_t* dest)
{
    const long off = dest - (const uint8_t*)gIoMem;
    return (off >= 0 && off < 0x400) ? off : -1;
}

/* Per-frame tally for TMC_HDMA_TRACE: how many lines the window channel
 * actually drove, and the range of right edges among them. Reported from the
 * VBlank reset. Without this "the gate still passes" cannot distinguish
 * "no scene exercised it" from "the publish never ran". */
static int s_win_lines_this_frame = 0;
static int s_win_right_max = 0;
static int s_win_right_min = 0x7FFF;

/* TMC_HDMA_NOWIN=1 restores the pre-fix behaviour — the DMA still writes
 * WIN0H, nothing forwards it, and the whole-frame committed bounds win. The
 * A/B that shows which frames the per-scanline window was silently losing. */
static int win_publish_disabled(void) {
    static int off = -1;
    if (off < 0) {
        off = getenv("TMC_HDMA_NOWIN") != NULL;
    }
    return off;
}

/* Push the edges this line's transfer just landed in WIN0H through to the
 * raster.
 *
 * Necessary because a host-supplied window overrides the packed registers for
 * the whole frame (virtuappu_mode1_set_window_h_bounds explains why), so
 * without this the DMA writes go into a register nothing reads and every
 * circular window collapses to whatever whole-frame bounds were last
 * committed. Prefers the untruncated side channel where its recorded key
 * matches the byte the DMA wrote — that is what carries an edge past the
 * hardware table's 240 ceiling. */
static void hdma_publish_window_line(int line)
{
    const uint16_t win0h = (uint16_t)(gIoMem[HDMA_IO_OFF_WIN0H] |
                                      (gIoMem[HDMA_IO_OFF_WIN0H + 1] << 8));
    int left = (int)(win0h >> 8);
    int right = (int)(win0h & 0xFF);

    if (line >= 0 && line < VIEWPORT_HEIGHT) {
        if (gWin0hExtLeftKey[line] >= 0 && gWin0hExtLeftKey[line] == left) {
            left = gWin0hExtLeft[line];
        }
        if (gWin0hExtRightKey[line] >= 0 && gWin0hExtRightKey[line] == right) {
            right = gWin0hExtRight[line];
        }
    }
    s_win_lines_this_frame++;
    if (right > s_win_right_max) {
        s_win_right_max = right;
    }
    if (right < s_win_right_min) {
        s_win_right_min = right;
    }
    virtuappu_mode1_set_window_h_bounds(0, left, right);
}

void port_hdma_step_line(int line)
{
    int ch;

    for (ch = 0; ch < HDMA_CHANNELS; ++ch) {
        HdmaChannel* c = &s_channels[ch];
        uint8_t* d;
        uint16_t i;

        if (!c->active) {
            continue;
        }
        d = c->dest;
        for (i = 0; i < c->count; ++i) {
            memcpy(d, c->src, c->unit);
            if (!c->src_fixed) {
                c->src += c->unit;
            }
            /* DEST_FIXED never advances within a transfer; INC and RELOAD do. */
            if (c->dest_mode != DEST_FIXED) {
                d += c->unit;
            }
        }
        /* Between scanlines: RELOAD rewinds to dest_orig; INC keeps the
         * advanced pointer; FIXED stayed put anyway. */
        c->dest = (c->dest_mode == DEST_RELOAD) ? c->dest_orig : d;

        if (hdma_io_offset(c->dest_orig) == HDMA_IO_OFF_WIN0H && !win_publish_disabled()) {
            hdma_publish_window_line(line);
        }
    }
}

void port_hdma_vblank_reset(void)
{
    int ch;
    static uint32_t frame = 0;

    frame++;
    if (s_win_lines_this_frame > 0 && getenv("TMC_HDMA_TRACE") != NULL) {
        /* A right edge that varies between lines is a genuinely per-scanline
         * window — the circular iris. A frame where every line agrees is a
         * whole-frame window the static commit would have produced anyway. */
        fprintf(stderr,
                "[hdma] frame=%u win0h lines=%d right=%d..%d dispcnt_win=%s%s%s winin=%04X winout=%04X%s\n",
                frame, s_win_lines_this_frame, s_win_right_min, s_win_right_max,
                (gIoMem[1] & 0x20) ? "W0" : "--",
                (gIoMem[1] & 0x40) ? "W1" : "--",
                (gIoMem[1] & 0x80) ? "OW" : "--",
                (unsigned)(gIoMem[0x48] | (gIoMem[0x49] << 8)),
                (unsigned)(gIoMem[0x4A] | (gIoMem[0x4B] << 8)),
                s_win_right_min != s_win_right_max ? "  PER-LINE" : "");
    }
    s_win_lines_this_frame = 0;
    s_win_right_max = 0;
    s_win_right_min = 0x7FFF;

    /*
     * TMC re-arms its HBlank DMA via SetVBlankDMA each frame, so registers
     * are typically refreshed during VBlank. If a channel happens to outlive
     * the frame, rewind src/dest so the same per-scanline table replays.
     */
    for (ch = 0; ch < HDMA_CHANNELS; ++ch) {
        HdmaChannel* c = &s_channels[ch];
        if (!c->active) {
            continue;
        }
        c->src = c->src_orig;
        c->dest = c->dest_orig;
    }
}
