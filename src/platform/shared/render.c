/**
 * @file render.c
 * @brief Software rasterizer for the GBA's mode-0 BG layer + OBJ layer.
 *
 * This translation unit implements PR #4 of the SDL-port roadmap (see
 * docs/sdl_port.md). It is intentionally cross-port: it has no SDL,
 * OS, or threading dependency and reads exclusively from the emulated
 * GBA memory regions exposed in `include/platform/port.h`
 * (`gPortIo`, `gPortVram`, `gPortPltt`, `gPortOam`). A future PSP /
 * PS2 / Win32 port can reuse it verbatim and only has to provide its
 * own framebuffer upload path.
 *
 * Scope (per the roadmap):
 *   - BG mode 0 (4 text BGs).
 *     * BGxCNT priority, char base block, screen base block,
 *       16-color (4 bpp) vs 256-color (8 bpp), and screen size
 *       (256x256 / 512x256 / 256x512 / 512x512 with the standard
 *       SC0..SC3 sub-screen wrapping).
 *     * BGxHOFS / BGxVOFS scrolling (text BGs wrap unconditionally).
 *     * Tilemap entries decode tile id (10 bits), hflip, vflip,
 *       palette bank (4-bit only used in 4 bpp mode).
 *   - OBJ layer.
 *     * Regular (non-affine) sprites only -- affine attribute is
 *       respected to the extent of skipping the sprite when the
 *       "double size / disable" bit means "disable" on a non-affine
 *       sprite, but transformed sprites are not yet rendered and
 *       are skipped (PR #5).
 *     * 4 bpp + 8 bpp tile data.
 *     * 1D mapping (DISPCNT_OBJ_1D_MAP) and 2D mapping (32-tile-wide
 *       OBJ char sheet).
 *     * All 12 shape x size combinations (8x8 .. 64x64).
 *     * Per-priority composition: OBJs of priority p sit on top of
 *       BGs of priority p; lower BG number / lower OAM index wins
 *       within an equal-priority tier.
 *
 * Out of scope for PR #4 (deferred to PR #5+):
 *   - Affine BGs (modes 1/2 partial, mode 2).
 *   - Bitmap modes 3/4/5.
 *   - Windows 0/1/OBJ/outside.
 *   - BLDCNT / BLDALPHA / BLDY blending and brightness fade.
 *   - Mosaic.
 *   - The OBJ "semi-transparent" and "OBJ window" modes.
 *
 * The dropped features fall through to "draw as opaque normal" so the
 * frame is at least readable; PR #5 will add the real implementations
 * behind the existing register reads.
 */

#include "platform/port.h"

#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* Local helpers.                                                           */
/* ------------------------------------------------------------------------ */

#define DISP_W PORT_GBA_DISPLAY_WIDTH
#define DISP_H PORT_GBA_DISPLAY_HEIGHT

/* Selected REG_* offsets we need. Duplicated here (rather than pulling in
 * `gba/io_reg.h`) so this TU stays free of GBA-decomp headers and remains
 * trivially reusable from another port. The matching constants in
 * `include/gba/io_reg.h` are validated against these via Port_HeadersSelfCheck
 * in port_headers_check.c. */
#define IO_DISPCNT 0x000
#define IO_BG0CNT 0x008
#define IO_BG0HOFS 0x010
#define IO_BG0VOFS 0x012

/* DISPCNT bits we use. */
#define DISPCNT_MODE_MASK 0x0007
#define DISPCNT_OBJ_1D_MAP 0x0040
#define DISPCNT_FORCED_BLANK 0x0080
#define DISPCNT_BG0_ON 0x0100
#define DISPCNT_OBJ_ON 0x1000

/* BGCNT bits. */
#define BGCNT_PRIORITY_MASK 0x0003
#define BGCNT_CHARBASE_SHIFT 2
#define BGCNT_CHARBASE_MASK 0x000C /* bits 2-3 */
#define BGCNT_256COLOR 0x0080
#define BGCNT_SCREENBASE_MASK 0x1F00 /* bits 8-12 */
#define BGCNT_SCREENBASE_SHIFT 8
#define BGCNT_SCREENSIZE_MASK 0xC000 /* bits 14-15 */
#define BGCNT_SCREENSIZE_SHIFT 14

/* OBJ attribute 0. */
#define OBJ_ATTR0_Y_MASK 0x00FF
#define OBJ_ATTR0_AFFINE 0x0100
#define OBJ_ATTR0_DISABLE 0x0200     /* only meaningful when AFFINE = 0 */
#define OBJ_ATTR0_DOUBLE_SIZE 0x0200 /* meaningful when AFFINE = 1 */
#define OBJ_ATTR0_MODE_MASK 0x0C00   /* 0=normal, 1=semi, 2=window, 3=prohibited */
#define OBJ_ATTR0_MOSAIC 0x1000
#define OBJ_ATTR0_256COLOR 0x2000
#define OBJ_ATTR0_SHAPE_MASK 0xC000
#define OBJ_ATTR0_SHAPE_SHIFT 14

/* OBJ attribute 1. */
#define OBJ_ATTR1_X_MASK 0x01FF /* 9-bit signed */
#define OBJ_ATTR1_X_SIGN 0x0100
#define OBJ_ATTR1_HFLIP 0x1000
#define OBJ_ATTR1_VFLIP 0x2000
#define OBJ_ATTR1_SIZE_MASK 0xC000
#define OBJ_ATTR1_SIZE_SHIFT 14

/* OBJ attribute 2. */
#define OBJ_ATTR2_TILE_MASK 0x03FF
#define OBJ_ATTR2_PRIORITY_MASK 0x0C00
#define OBJ_ATTR2_PRIORITY_SHIFT 10
#define OBJ_ATTR2_PALETTE_MASK 0xF000
#define OBJ_ATTR2_PALETTE_SHIFT 12

/* GBA video memory layout (offsets into gPortVram). */
#define VRAM_BG_END 0x10000 /* first 64 KiB of VRAM is BG */
#define VRAM_OBJ_BASE 0x10000

/* PR #2a self-check guarantees these match the macros in
 * include/gba/io_reg.h, so we can hard-code them. */

/* ------------------------------------------------------------------------ */
/* Color conversion: BGR555 (GBA) -> ARGB8888 (host).                       */
/* ------------------------------------------------------------------------ */

static inline uint32_t bgr555_to_argb8888(uint16_t bgr) {
    /* GBA palette format is 0bbbbb gggggrrrrr. Expand each 5-bit channel
     * to 8 bits with the standard "shift up 3, OR top 3" replication so
     * 0x1F maps to 0xFF rather than 0xF8. */
    uint32_t r5 = (bgr >> 0) & 0x1F;
    uint32_t g5 = (bgr >> 5) & 0x1F;
    uint32_t b5 = (bgr >> 10) & 0x1F;
    uint32_t r = (r5 << 3) | (r5 >> 2);
    uint32_t g = (g5 << 3) | (g5 >> 2);
    uint32_t b = (b5 << 3) | (b5 >> 2);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static inline uint16_t io_read16(uint32_t off) {
    /* All host-supported targets are little-endian (see docs/sdl_port.md
     * "Risks"), so a direct unaligned 16-bit read is fine. Use memcpy
     * to be safe with -fstrict-aliasing-flipped builds and any future
     * platform that disables -fno-strict-aliasing for this TU. */
    uint16_t v;
    memcpy(&v, &gPortIo[off & 0x3FE], sizeof(v));
    return v;
}

static inline uint16_t pltt_read16(uint32_t index) {
    /* 256 BG entries followed by 256 OBJ entries, each 2 bytes. */
    uint16_t v;
    memcpy(&v, &gPortPltt[(index & 0x1FF) * 2], sizeof(v));
    return v;
}

static inline uint16_t vram_read16(uint32_t off) {
    uint16_t v;
    if (off + 2 > sizeof(gPortVram)) {
        return 0;
    }
    memcpy(&v, &gPortVram[off], sizeof(v));
    return v;
}

/* ------------------------------------------------------------------------ */
/* BG mode-0 (text) scanline rendering.                                     */
/* ------------------------------------------------------------------------ */

/* Per-scanline output for a single BG: one palette index per pixel,
 * 0 = transparent (or the BG palette's own entry 0, which the GBA also
 * treats as transparent for non-backdrop BG layers). The companion
 * `colors[]` array holds the resolved BGR555 value for non-transparent
 * pixels so the compositor doesn't have to re-read the palette per
 * pixel. */
typedef struct {
    uint8_t opaque[DISP_W];  /* 1 = drawn, 0 = transparent */
    uint16_t colors[DISP_W]; /* BGR555 when opaque[x] != 0 */
    int priority;            /* 0..3 from BGCNT */
    int active;              /* 1 if this BG produced any output this frame */
} BgScanline;

/* Map the BGCNT screen-size code (0..3) to per-axis size in tiles
 * (each tile is 8 px). The four sub-screens (SC0..SC3) inside a 512-px
 * map are stored as consecutive 32x32-tile (2 KiB) blocks. */
static void text_bg_dimensions_tiles(uint16_t bgcnt, int* w_tiles_out, int* h_tiles_out) {
    int code = (bgcnt & BGCNT_SCREENSIZE_MASK) >> BGCNT_SCREENSIZE_SHIFT;
    /* 0: 32x32 (256x256 px), 1: 64x32, 2: 32x64, 3: 64x64. */
    int w_tiles = (code & 1) ? 64 : 32;
    int h_tiles = (code & 2) ? 64 : 32;
    *w_tiles_out = w_tiles;
    *h_tiles_out = h_tiles;
}

/* Resolve a tilemap entry's address inside VRAM, given the BG's screen
 * base (in 2 KiB blocks) and per-axis tile coordinates inside the
 * BG's full tile-map. Handles the GBA's SC0..SC3 sub-screen layout for
 * 512-px-wide / 512-px-tall maps. */
static uint32_t text_tilemap_addr(uint16_t bgcnt, int tile_x, int tile_y, int w_tiles, int h_tiles) {
    int screen_base = (bgcnt & BGCNT_SCREENBASE_MASK) >> BGCNT_SCREENBASE_SHIFT;
    uint32_t base = (uint32_t)screen_base * 0x800; /* 2 KiB per screen-base block */

    /* Each sub-screen is 32x32 tiles (= 2 KiB of 16-bit entries). For
     * wider/taller maps, walk to the right sub-screen first. */
    int sub_x = tile_x / 32;
    int sub_y = tile_y / 32;
    int local_x = tile_x % 32;
    int local_y = tile_y % 32;

    /* Sub-screen ordering inside a 64x64 map:
     *   SC0 SC1
     *   SC2 SC3
     * For 64x32 only SC0+SC1 are used (in that order). For 32x64,
     * SC0+SC1 are stacked (SC0 on top, SC1 below). */
    int sub_index;
    if (w_tiles == 64 && h_tiles == 64) {
        sub_index = sub_y * 2 + sub_x;
    } else if (w_tiles == 64) {
        sub_index = sub_x;
    } else if (h_tiles == 64) {
        sub_index = sub_y;
    } else {
        sub_index = 0;
    }

    return base + (uint32_t)sub_index * 0x800 + (uint32_t)(local_y * 32 + local_x) * 2;
}

/* Sample one pixel from a BG tile. `char_base_addr` is the BG character
 * data base (offset into VRAM); `tile_id` is the tilemap entry's tile
 * index; `(px,py)` are 0..7 within the tile, post-flip. Returns 0 for
 * transparent (palette entry 0) or the BG palette index 1..255. The
 * caller must combine with the palette bank for 4 bpp tiles. */
static uint8_t bg_sample_tile_pixel(uint32_t char_base_addr, int tile_id, int px, int py, int is_8bpp) {
    if (is_8bpp) {
        /* Each 8 bpp tile is 64 bytes. */
        uint32_t off = char_base_addr + (uint32_t)tile_id * 64 + (uint32_t)(py * 8 + px);
        if (off >= VRAM_BG_END) {
            return 0;
        }
        return gPortVram[off];
    } else {
        /* Each 4 bpp tile is 32 bytes; two pixels per byte (low nibble
         * is the lower x). */
        uint32_t off = char_base_addr + (uint32_t)tile_id * 32 + (uint32_t)(py * 4 + (px >> 1));
        if (off >= VRAM_BG_END) {
            return 0;
        }
        uint8_t b = gPortVram[off];
        return (px & 1) ? (b >> 4) : (b & 0x0F);
    }
}

static void render_text_bg_scanline(int bg_index, int y, BgScanline* out) {
    uint16_t bgcnt = io_read16(IO_BG0CNT + bg_index * 2);
    uint16_t hofs = io_read16(IO_BG0HOFS + bg_index * 4) & 0x1FF;
    uint16_t vofs = io_read16(IO_BG0VOFS + bg_index * 4) & 0x1FF;

    int w_tiles, h_tiles;
    text_bg_dimensions_tiles(bgcnt, &w_tiles, &h_tiles);
    int map_w_px = w_tiles * 8;
    int map_h_px = h_tiles * 8;

    int char_base = (bgcnt & BGCNT_CHARBASE_MASK) >> BGCNT_CHARBASE_SHIFT;
    uint32_t char_base_addr = (uint32_t)char_base * 0x4000; /* 16 KiB blocks */
    int is_8bpp = (bgcnt & BGCNT_256COLOR) != 0;

    out->priority = bgcnt & BGCNT_PRIORITY_MASK;
    out->active = 1;

    int vy = ((int)y + (int)vofs) % map_h_px;
    if (vy < 0) {
        vy += map_h_px;
    }
    int tile_y = vy / 8;
    int py = vy & 7;

    /* Precompute the start x in the virtual map; iterate one pixel at a
     * time. A tighter implementation would step through whole tiles, but
     * the GBA framebuffer is 240 px wide so this is already <40k inner
     * iterations per frame at 60 Hz. */
    int vx_start = (int)hofs;
    for (int x = 0; x < DISP_W; ++x) {
        int vx = (vx_start + x) % map_w_px;
        if (vx < 0) {
            vx += map_w_px;
        }
        int tile_x = vx / 8;
        int px = vx & 7;

        uint32_t entry_addr = text_tilemap_addr(bgcnt, tile_x, tile_y, w_tiles, h_tiles);
        uint16_t entry = vram_read16(entry_addr);
        int tile_id = entry & 0x3FF;
        int hflip = (entry & 0x0400) != 0;
        int vflip = (entry & 0x0800) != 0;
        int pal_bank = (entry >> 12) & 0xF;

        int sample_x = hflip ? (7 - px) : px;
        int sample_y = vflip ? (7 - py) : py;
        uint8_t pix = bg_sample_tile_pixel(char_base_addr, tile_id, sample_x, sample_y, is_8bpp);
        if (pix == 0) {
            out->opaque[x] = 0;
            continue;
        }
        int pal_index = is_8bpp ? pix : (pal_bank * 16 + pix);
        out->colors[x] = pltt_read16(pal_index);
        out->opaque[x] = 1;
    }
}

/* ------------------------------------------------------------------------ */
/* OBJ layer scanline rendering.                                            */
/* ------------------------------------------------------------------------ */

/* Sprite size table indexed by (shape * 4 + size). Shape 3 is "prohibited"
 * on real hardware; we map it to all-zero so such sprites render as
 * disabled. Each entry is { width, height } in pixels. */
static const uint8_t kObjDimensions[16][2] = {
    /* shape 0 (square) */
    { 8, 8 },
    { 16, 16 },
    { 32, 32 },
    { 64, 64 },
    /* shape 1 (wide) */
    { 16, 8 },
    { 32, 8 },
    { 32, 16 },
    { 64, 32 },
    /* shape 2 (tall) */
    { 8, 16 },
    { 8, 32 },
    { 16, 32 },
    { 32, 64 },
    /* shape 3 (prohibited) */
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
};

/* OBJ output for a single scanline split by priority. We iterate OAM in
 * forward order and write the first opaque pixel per (x, priority); a
 * lower OAM index "wins" within the same priority because once a pixel
 * is filled the later sprite cannot overwrite it. The compositor draws
 * higher-numbered priorities first so OBJs of priority 0 end up on top. */
typedef struct {
    uint8_t opaque[4][DISP_W];
    uint16_t colors[4][DISP_W];
} ObjScanline;

static void render_obj_scanline(int y, int obj_1d_mapping, ObjScanline* out) {
    /* OAM holds 128 sprites x 8 bytes each. The last 2 bytes of every
     * 8-byte OBJ entry are interleaved OBJ rotation/scaling parameters
     * which we don't use here. */
    for (int idx = 0; idx < 128; ++idx) {
        uint32_t base = (uint32_t)idx * 8;
        uint16_t a0, a1, a2;
        memcpy(&a0, &gPortOam[base + 0], 2);
        memcpy(&a1, &gPortOam[base + 2], 2);
        memcpy(&a2, &gPortOam[base + 4], 2);

        int affine = (a0 & OBJ_ATTR0_AFFINE) != 0;
        if (!affine && (a0 & OBJ_ATTR0_DISABLE)) {
            continue;
        }
        /* PR #4 does not yet rasterize affine sprites. Skip them so they
         * don't leak garbage onto the screen; PR #5 covers the affine
         * pipeline. */
        if (affine) {
            continue;
        }
        /* Mode 3 ("prohibited") + OBJ window mode: skip until PR #5
         * implements windows. Semi-transparent (mode 1) renders as
         * normal opaque for now. */
        int mode = (a0 & OBJ_ATTR0_MODE_MASK) >> 10;
        if (mode == 2 || mode == 3) {
            continue;
        }

        int shape = (a0 & OBJ_ATTR0_SHAPE_MASK) >> OBJ_ATTR0_SHAPE_SHIFT;
        int size = (a1 & OBJ_ATTR1_SIZE_MASK) >> OBJ_ATTR1_SIZE_SHIFT;
        int sw = kObjDimensions[shape * 4 + size][0];
        int sh = kObjDimensions[shape * 4 + size][1];
        if (sw == 0) {
            continue;
        }

        /* Y is 8-bit unsigned but the visible window wraps at 256 so a
         * sprite at Y=240 overlaps the top of the screen. */
        int sy = a0 & OBJ_ATTR0_Y_MASK;
        int line_in_sprite = (y - sy) & 0xFF;
        if (line_in_sprite >= sh) {
            continue;
        }

        /* X is 9-bit signed. */
        int sx = a1 & OBJ_ATTR1_X_MASK;
        if (sx & OBJ_ATTR1_X_SIGN) {
            sx -= 0x200;
        }
        if (sx + sw <= 0 || sx >= DISP_W) {
            continue;
        }

        int hflip = (a1 & OBJ_ATTR1_HFLIP) != 0;
        int vflip = (a1 & OBJ_ATTR1_VFLIP) != 0;
        int is_8bpp = (a0 & OBJ_ATTR0_256COLOR) != 0;
        int prio = (a2 & OBJ_ATTR2_PRIORITY_MASK) >> OBJ_ATTR2_PRIORITY_SHIFT;
        int pal_bank = (a2 & OBJ_ATTR2_PALETTE_MASK) >> OBJ_ATTR2_PALETTE_SHIFT;
        int base_tile = a2 & OBJ_ATTR2_TILE_MASK;

        int row_in_sprite = vflip ? (sh - 1 - line_in_sprite) : line_in_sprite;
        int row_tile = row_in_sprite / 8;
        int row_within_tile = row_in_sprite & 7;
        int sprite_w_tiles = sw / 8;

        /* OBJ tile addressing. Tile numbers are always in 32-byte units;
         * for 8 bpp sprites the low bit of the tile number is ignored
         * (real hardware) so we mask it to even. */
        int eff_base_tile = is_8bpp ? (base_tile & ~1) : base_tile;

        /* Step (in 32-byte tile units) from the top-left tile of the
         * sprite to the start of `row_tile`. */
        int row_tile_step;
        if (obj_1d_mapping) {
            /* 1D: tiles laid out linearly. For 8 bpp the per-row step
             * doubles because each "logical tile" occupies two 32-byte
             * tile slots. */
            row_tile_step = row_tile * sprite_w_tiles * (is_8bpp ? 2 : 1);
        } else {
            /* 2D: OBJ char sheet is 32 tiles wide, regardless of bpp. */
            row_tile_step = row_tile * 32;
        }

        for (int col = 0; col < sw; ++col) {
            int screen_x = sx + col;
            if (screen_x < 0 || screen_x >= DISP_W) {
                continue;
            }
            if (out->opaque[prio][screen_x]) {
                /* A lower-OAM-index sprite already won this pixel at
                 * this priority. */
                continue;
            }
            int col_in_sprite = hflip ? (sw - 1 - col) : col;
            int col_tile = col_in_sprite / 8;
            int col_within_tile = col_in_sprite & 7;

            int tile_offset_in_units;
            if (is_8bpp) {
                /* Each visible 8x8 cell occupies two 32-byte slots, so
                 * stepping one cell to the right is +2 tile units. */
                tile_offset_in_units = row_tile_step + col_tile * 2;
            } else {
                tile_offset_in_units = row_tile_step + col_tile;
            }
            int tile_id = (eff_base_tile + tile_offset_in_units) & 0x3FF;
            uint32_t tile_addr = (uint32_t)VRAM_OBJ_BASE + (uint32_t)tile_id * 32;
            uint8_t pix;
            if (is_8bpp) {
                uint32_t off = tile_addr + (uint32_t)(row_within_tile * 8 + col_within_tile);
                if (off >= sizeof(gPortVram)) {
                    pix = 0;
                } else {
                    pix = gPortVram[off];
                }
            } else {
                uint32_t off = tile_addr + (uint32_t)(row_within_tile * 4 + (col_within_tile >> 1));
                if (off >= sizeof(gPortVram)) {
                    pix = 0;
                } else {
                    uint8_t b = gPortVram[off];
                    pix = (col_within_tile & 1) ? (b >> 4) : (b & 0x0F);
                }
            }
            if (pix == 0) {
                continue;
            }
            int pal_index = is_8bpp ? pix : (pal_bank * 16 + pix);
            /* OBJ palette starts at gPortPltt[0x200] = pltt_read16(256). */
            out->colors[prio][screen_x] = pltt_read16(256 + pal_index);
            out->opaque[prio][screen_x] = 1;
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Per-scanline composition.                                                */
/* ------------------------------------------------------------------------ */

/* Mode-0 has all four BGs as text BGs. Modes 1/2 mix in affine BGs that
 * PR #4 does not yet implement; for those we still render any text BGs
 * that mode allows so the screen isn't completely empty, but the affine
 * BGs are ignored. */
static int bg_is_text_in_mode(int mode, int bg_index) {
    switch (mode) {
        case 0:
            return 1; /* all four BGs are text */
        case 1:
            return bg_index <= 1; /* BG0/BG1 text, BG2 affine, BG3 off */
        case 2:
            return 0; /* BG0/BG1 off, BG2/BG3 affine */
        default:
            return 0; /* bitmap modes */
    }
}

static void composite_scanline(uint32_t* out_row, int y, uint16_t dispcnt) {
    uint16_t backdrop = pltt_read16(0);
    uint32_t backdrop_argb = bgr555_to_argb8888(backdrop);

    int mode = dispcnt & DISPCNT_MODE_MASK;

    /* Bitmap modes (3/4/5) and any "no BGs in this mode" path: just fill
     * with backdrop until PR #5. The OBJ layer is still composited so a
     * test image with only sprites remains visible. */

    BgScanline bg[4] = { 0 };
    for (int b = 0; b < 4; ++b) {
        if ((dispcnt & (DISPCNT_BG0_ON << b)) && bg_is_text_in_mode(mode, b)) {
            render_text_bg_scanline(b, y, &bg[b]);
        }
    }

    ObjScanline obj = { { { 0 } } };
    if (dispcnt & DISPCNT_OBJ_ON) {
        int one_d = (dispcnt & DISPCNT_OBJ_1D_MAP) != 0;
        render_obj_scanline(y, one_d, &obj);
    }

    /* Back-to-front composite: backdrop, then BG/OBJ pairs from
     * priority 3 down to 0, with BGs of equal priority drawn highest
     * BG number first (so BG0 ends up on top), and OBJs of that same
     * priority painted afterwards (so OBJ sits on top of BG of equal
     * priority). */
    for (int x = 0; x < DISP_W; ++x) {
        uint32_t pixel = backdrop_argb;
        for (int prio = 3; prio >= 0; --prio) {
            for (int b = 3; b >= 0; --b) {
                if (bg[b].active && bg[b].priority == prio && bg[b].opaque[x]) {
                    pixel = bgr555_to_argb8888(bg[b].colors[x]);
                }
            }
            if (obj.opaque[prio][x]) {
                pixel = bgr555_to_argb8888(obj.colors[prio][x]);
            }
        }
        out_row[x] = pixel;
    }
}

/* ------------------------------------------------------------------------ */
/* Public entry point.                                                      */
/* ------------------------------------------------------------------------ */

void Port_RenderFrame(uint32_t* framebuffer) {
    if (framebuffer == NULL) {
        return;
    }

    uint16_t dispcnt = io_read16(IO_DISPCNT);

    if (dispcnt & DISPCNT_FORCED_BLANK) {
        /* Real hardware drives the display white during forced blank. */
        for (int i = 0; i < DISP_W * DISP_H; ++i) {
            framebuffer[i] = 0xFFFFFFFFu;
        }
        return;
    }

    for (int y = 0; y < DISP_H; ++y) {
        composite_scanline(framebuffer + (uint32_t)y * DISP_W, y, dispcnt);
    }
}

/* ------------------------------------------------------------------------ */
/* Headless self-check (PR #4).                                             */
/* ------------------------------------------------------------------------ */

/* Tiny RAII-ish helper: snapshot a region of one of the host arrays so
 * the self-check can scribble over it and restore the original bytes
 * before returning. Sized for the biggest region we touch (a 32x32-tile
 * BG screen + tile char data + a few sprites = under 8 KiB). */
typedef struct {
    uint8_t io[64];
    uint8_t pltt[16 * 2 + 16 * 2]; /* a few BG + OBJ palette entries */
    uint8_t bg_chr[64];            /* 2 x 32-byte 4 bpp tiles */
    uint8_t bg_map[8];             /* a few tilemap entries */
    uint8_t obj_chr[64];           /* 2 x 32-byte 4 bpp tiles */
    uint8_t oam[1024];             /* full OAM (128 sprites x 8 bytes) */
} RendererSnapshot;

static void snapshot_save(RendererSnapshot* s) {
    memcpy(s->io, &gPortIo[0], sizeof(s->io));
    memcpy(s->pltt, &gPortPltt[0], 16 * 2);
    memcpy(s->pltt + 16 * 2, &gPortPltt[0x200], 16 * 2);
    memcpy(s->bg_chr, &gPortVram[0], sizeof(s->bg_chr));
    memcpy(s->bg_map, &gPortVram[0x800], sizeof(s->bg_map));
    memcpy(s->obj_chr, &gPortVram[VRAM_OBJ_BASE], sizeof(s->obj_chr));
    memcpy(s->oam, &gPortOam[0], sizeof(s->oam));
}
static void snapshot_restore(const RendererSnapshot* s) {
    memcpy(&gPortIo[0], s->io, sizeof(s->io));
    memcpy(&gPortPltt[0], s->pltt, 16 * 2);
    memcpy(&gPortPltt[0x200], s->pltt + 16 * 2, 16 * 2);
    memcpy(&gPortVram[0], s->bg_chr, sizeof(s->bg_chr));
    memcpy(&gPortVram[0x800], s->bg_map, sizeof(s->bg_map));
    memcpy(&gPortVram[VRAM_OBJ_BASE], s->obj_chr, sizeof(s->obj_chr));
    memcpy(&gPortOam[0], s->oam, sizeof(s->oam));
}

static void io_write16(uint32_t off, uint16_t v) {
    memcpy(&gPortIo[off & 0x3FE], &v, sizeof(v));
}
static void pltt_write16(uint32_t index, uint16_t v) {
    memcpy(&gPortPltt[(index & 0x1FF) * 2], &v, sizeof(v));
}
static void vram_write16(uint32_t off, uint16_t v) {
    memcpy(&gPortVram[off], &v, sizeof(v));
}
static void oam_write16(uint32_t off, uint16_t v) {
    memcpy(&gPortOam[off], &v, sizeof(v));
}

int Port_RendererSelfCheck(void) {
    RendererSnapshot snap;
    snapshot_save(&snap);

    /* 1. Forced blank should yield a fully-white frame. */
    static uint32_t fb[DISP_W * DISP_H];
    io_write16(IO_DISPCNT, DISPCNT_FORCED_BLANK);
    Port_RenderFrame(fb);
    int ok = (fb[0] == 0xFFFFFFFFu && fb[DISP_W * DISP_H - 1] == 0xFFFFFFFFu);

    /* 2. Backdrop colour: clear DISPCNT (mode 0, no BGs/OBJs enabled),
     *    write a known BGR555 to PLTT[0], expect every pixel to match. */
    io_write16(IO_DISPCNT, 0); /* mode 0, all layers off */
    pltt_write16(0, 0x7FFF);   /* white in BGR555 */
    Port_RenderFrame(fb);
    ok = ok && (fb[0] == 0xFFFFFFFFu);
    pltt_write16(0, 0x001F); /* pure red */
    Port_RenderFrame(fb);
    ok = ok && (fb[123] == 0xFFFF0000u);

    /* 3. BG0 mode-0 4 bpp: place a single-tile, single-entry tilemap
     *    showing palette index 1 across the entire tile. The whole
     *    screen should sample that one palette entry. */
    pltt_write16(0, 0x0000); /* black backdrop */
    pltt_write16(1, 0x03E0); /* pure green (BGR555: g = 0x1F << 5) */
    /* tile 0 in char base 0: every byte = 0x11 (low and high nibble = 1) */
    for (int i = 0; i < 32; ++i) {
        gPortVram[i] = 0x11;
    }
    /* tilemap entry 0 in screen base 1 (offset 0x800): tile id 0,
     * no flip, palette bank 0. */
    vram_write16(0x800, 0x0000);
    /* BG0CNT: priority 0, char base 0, screen base 1, 16-color, 32x32. */
    io_write16(IO_BG0CNT, (1 << BGCNT_SCREENBASE_SHIFT));
    io_write16(IO_BG0HOFS, 0);
    io_write16(IO_BG0VOFS, 0);
    /* DISPCNT: mode 0, BG0 on. */
    io_write16(IO_DISPCNT, DISPCNT_BG0_ON);
    Port_RenderFrame(fb);
    /* Tile 0 only covers the first 8x8 of the tilemap. The remaining
     * tilemap entries inside screen base 1 are 0 too (we cleared the
     * snapshot region), so they all reference tile 0 / palette bank 0
     * / palette index 1. Therefore every pixel of the screen reads
     * green. */
    uint32_t green_argb = bgr555_to_argb8888(0x03E0);
    ok = ok && (fb[0] == green_argb);
    ok = ok && (fb[DISP_W / 2 + (DISP_H / 2) * DISP_W] == green_argb);
    ok = ok && (fb[DISP_W * DISP_H - 1] == green_argb);

    /* 4. Tilemap entry hflip + vflip: change tile 0 so the bottom-right
     *    pixel is index 2 instead of 1, then test that hflip moves it
     *    to bottom-left and vflip to top-right. */
    pltt_write16(2, 0x7C00); /* pure blue (BGR555: b = 0x1F << 10) */
    /* Tile 0: clear, then set pixel (7,7) to palette index 2. In 4 bpp
     * tile layout: row y * 4 + (x>>1) byte; if x odd → high nibble. */
    memset(&gPortVram[0], 0x11, 32);
    /* (7,7): byte = 7 * 4 + 3 = 31; x is odd, so high nibble. Set high
     * nibble to 2, low nibble keeps existing index 1. */
    gPortVram[31] = (2 << 4) | 1;

    /* No flip: pixel (7,7) of tile 0 → screen pixel (7,7) is blue. */
    vram_write16(0x800, 0x0000);
    Port_RenderFrame(fb);
    uint32_t blue_argb = bgr555_to_argb8888(0x7C00);
    ok = ok && (fb[7 + 7 * DISP_W] == blue_argb);
    ok = ok && (fb[0] == green_argb);

    /* hflip only: tile 0's pixel (7,7) maps to screen (0, 7). */
    vram_write16(0x800, 0x0400);
    Port_RenderFrame(fb);
    ok = ok && (fb[0 + 7 * DISP_W] == blue_argb);
    ok = ok && (fb[7 + 7 * DISP_W] == green_argb);

    /* vflip only: tile 0's pixel (7,7) maps to screen (7, 0). */
    vram_write16(0x800, 0x0800);
    Port_RenderFrame(fb);
    ok = ok && (fb[7 + 0 * DISP_W] == blue_argb);

    /* Palette bank: select palette bank 1 in the tilemap entry, swap
     * palette[16+1] with a different colour, expect every "background"
     * pixel of tile 0 to use the new colour. */
    pltt_write16(16 + 1, 0x001F); /* red */
    vram_write16(0x800, 0x1000);  /* pal bank 1, no flip */
    Port_RenderFrame(fb);
    uint32_t red_argb = bgr555_to_argb8888(0x001F);
    ok = ok && (fb[0] == red_argb);
    /* And the high-nibble pixel (7,7) of tile 0 used palette index 2.
     * Switch the high nibble to 0 (= transparent in the GBA sense; only
     * sample==0 is transparent, palette colour 0 is just black) and
     * verify the pixel exposes the backdrop. */
    pltt_write16(0, 0x1234);
    /* (7,7) byte = 31; high nibble = sample for x=7. Set high=0, low=1. */
    gPortVram[31] = (0 << 4) | 1;
    Port_RenderFrame(fb);
    ok = ok && (fb[7 + 7 * DISP_W] == bgr555_to_argb8888(0x1234));

    /* 5. Scroll: with hofs=1, the screen pixel previously at (1,0) now
     *    appears at (0,0). Easiest verification: with hofs=8 the entire
     *    leftmost tile shifts off-screen but since every tilemap entry
     *    references tile 0 the visible pattern is unchanged. So we
     *    instead test that after writing tile 1's row 0 to a distinct
     *    pattern and using that tile in entry (1, 0), an hofs of 8
     *    moves it to the leftmost column. */
    pltt_write16(0, 0x0000);
    /* Tile 1 (offset 32): every pixel = palette index 3. */
    pltt_write16(3, 0x03FF); /* yellow-ish */
    memset(&gPortVram[32], 0x33, 32);
    vram_write16(0x800 + 0 * 2, 0x0000); /* (0,0) -> tile 0 */
    vram_write16(0x800 + 1 * 2, 0x0001); /* (1,0) -> tile 1 */
    io_write16(IO_BG0HOFS, 8);
    Port_RenderFrame(fb);
    uint32_t yellow_argb = bgr555_to_argb8888(0x03FF);
    ok = ok && (fb[0] == yellow_argb); /* tile 1 is now at column 0 */
    io_write16(IO_BG0HOFS, 0);

    /* 6. OBJ rendering: place an 8x8 4 bpp sprite that paints palette
     *    index 1 at (0,0). It should sit on top of the BG. First disable
     *    every other sprite -- a default-zero OAM entry would otherwise
     *    render as "8x8 sprite at (0,0) using OBJ tile 0" and shadow
     *    the test sprite. */
    for (int i = 1; i < 128; ++i) {
        oam_write16(i * 8, OBJ_ATTR0_DISABLE);
    }
    pltt_write16(256 + 1, 0x7FFF); /* OBJ palette[1] = white */
    /* OBJ tile 0 at OBJ char base = VRAM_OBJ_BASE. */
    memset(&gPortVram[VRAM_OBJ_BASE], 0x11, 32);
    /* OAM 0: y=0, x=0, size 8x8, 4 bpp, normal mode. */
    oam_write16(0, 0x0000); /* y=0, no affine, mode normal, shape 0 (square) */
    oam_write16(2, 0x0000); /* x=0, no flip, size 0 (=> 8x8 with shape 0) */
    oam_write16(4, 0x0000); /* tile 0, priority 0, palette bank 0 */
    /* Disable BG0 so it doesn't shadow the test, leave only OBJ on. */
    io_write16(IO_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    Port_RenderFrame(fb);
    uint32_t white_argb = bgr555_to_argb8888(0x7FFF);
    ok = ok && (fb[0] == white_argb);
    ok = ok && (fb[7 + 7 * DISP_W] == white_argb);
    /* Outside the sprite -> backdrop. */
    ok = ok && (fb[8 + 0 * DISP_W] == bgr555_to_argb8888(0x0000));

    /* OBJ disabled via attr0 disable bit: should disappear. */
    oam_write16(0, OBJ_ATTR0_DISABLE);
    Port_RenderFrame(fb);
    ok = ok && (fb[0] != white_argb);
    oam_write16(0, 0); /* re-enable */

    /* OBJ priority composition: place BG0 over the sprite area. With the
     * sprite at priority 0 and BG0 at priority 0 too, the OBJ wins. With
     * the sprite moved to priority 3 and BG0 at priority 0, the BG wins. */
    pltt_write16(1, 0x03E0); /* BG palette[1] = green */
    memset(&gPortVram[0], 0x11, 32);
    vram_write16(0x800, 0x0000);
    io_write16(IO_BG0CNT, (1 << BGCNT_SCREENBASE_SHIFT)); /* prio 0 */
    io_write16(IO_DISPCNT, DISPCNT_BG0_ON | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    Port_RenderFrame(fb);
    ok = ok && (fb[0] == white_argb); /* OBJ on top */

    oam_write16(4, 0x0C00); /* sprite priority = 3 */
    Port_RenderFrame(fb);
    ok = ok && (fb[0] == green_argb); /* BG wins now */

    snapshot_restore(&snap);
    return ok ? 0 : 1;
}
