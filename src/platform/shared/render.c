/**
 * @file render.c
 * @brief Software rasterizer for the GBA's BG + OBJ pipeline.
 *
 * This translation unit implements PR #4 + PR #5 of the SDL-port roadmap
 * (see docs/sdl_port.md). It is intentionally cross-port: it has no SDL,
 * OS, or threading dependency and reads exclusively from the emulated
 * GBA memory regions exposed in `include/platform/port.h`
 * (`gPortIo`, `gPortVram`, `gPortPltt`, `gPortOam`). A future PSP /
 * PS2 / Win32 port reuses it verbatim and only has to provide its own
 * framebuffer upload path.
 *
 * Scope (per the roadmap):
 *
 *   PR #4 (already shipped):
 *     - BG mode 0 (4 text BGs) with all four screen-size codes,
 *       per-axis HOFS / VOFS scrolling, 4 bpp + 8 bpp tiles.
 *     - Regular OBJ sprites, 4 bpp + 8 bpp, 1D + 2D tile mapping,
 *       all 12 shape x size combinations, hflip / vflip, attr0 disable
 *       bit, per-priority composition with the standard GBA tie-break
 *       (OBJ on top of BG of equal priority; lower BG number / lower
 *       OAM index wins within a tier).
 *
 *   PR #5 (this file):
 *     - Affine BGs in modes 1 / 2.  BG2 affine in mode 1 (BG0 / BG1
 *       remain text), BG2 + BG3 affine in mode 2.  BGCNT bit 13
 *       (display-area overflow) selects "transparent" vs "wrap"
 *       behaviour outside the map area.  Sampling uses BG2/3 PA, PB,
 *       PC, PD (s8.8) and BG2/3 X, Y (s19.8 reference points loaded
 *       per frame).
 *     - Affine sprites.  The four PA / PB / PC / PD slots interleaved
 *       across an affine group (5-bit index in OAM attr1 bits 9..13)
 *       drive a centred-affine 2x2 transform.  The DOUBLE_SIZE bit
 *       gives the sprite a doubled bounding box on screen so the
 *       rotation has room to fit.
 *     - Windows: WIN0 / WIN1 (rectangles from WIN0H/V + WIN1H/V),
 *       OBJ window (pixels where an OBJ in mode 2 is opaque), and
 *       the "outside" region (everywhere else when any window is
 *       enabled in DISPCNT).  Per-region layer enables come from
 *       WININ / WINOUT and gate both the BG/OBJ visibility and the
 *       blend-effect enable bit.
 *     - Alpha blending (BLDCNT mode 1, BLDALPHA EVA / EVB), brightness
 *       fade up (mode 2, BLDY) and brightness fade down (mode 3, BLDY).
 *       OBJ semi-transparent (attr0 mode 1) acts as a per-pixel
 *       "force alpha blend" override regardless of BLDCNT (the OBJ
 *       layer is treated as 1st target; the layer behind it must be a
 *       2nd target for the blend to fire).
 *     - Mosaic (MOSAIC register, BGCNT bit 6 for BGs, OBJ attr0 bit 12
 *       for OBJs).  BG mosaic snaps both axes; OBJ mosaic is applied
 *       per-priority horizontally and the OBJ rasterizer snaps the
 *       sampled row to the mosaic V grid for affected sprites.
 *
 * Out of scope (still deferred):
 *   - Bitmap modes 3 / 4 / 5 (the renderer falls through to a backdrop
 *     fill so the screen stays in a defined state).
 *   - Mid-scanline raster effects (HBlank palette / scroll changes).
 *     The renderer samples each I/O register exactly once at the start
 *     of every scanline, so the GBA's per-line tricks are visible at
 *     full-frame granularity only.
 *   - The BG2/3 reference-point internal-counter increment that the
 *     real PPU performs every scanline based on PB / PD.  We resample
 *     BG2X / BG2Y / BG3X / BG3Y on every scanline, so a game that
 *     relies on the internal counter latching mid-frame will see a
 *     flat affine instead of a per-line one.  The decomp does not
 *     appear to use that effect.
 */

#include "platform/port.h"

#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* Local helpers and register offsets.                                      */
/* ------------------------------------------------------------------------ */

#define DISP_W PORT_GBA_DISPLAY_WIDTH
#define DISP_H PORT_GBA_DISPLAY_HEIGHT

/* Selected REG_* offsets we need.  Duplicated here (rather than pulling in
 * `gba/io_reg.h`) so this TU stays free of GBA-decomp headers and remains
 * trivially reusable from another port.  The matching constants in
 * `include/gba/io_reg.h` are validated against these via
 * `Port_HeadersSelfCheck` in `port_headers_check.c`. */
#define IO_DISPCNT 0x000
#define IO_BG0CNT 0x008
#define IO_BG2CNT 0x00C
#define IO_BG3CNT 0x00E
#define IO_BG0HOFS 0x010
#define IO_BG0VOFS 0x012
#define IO_BG2PA 0x020
#define IO_BG2PB 0x022
#define IO_BG2PC 0x024
#define IO_BG2PD 0x026
#define IO_BG2X 0x028
#define IO_BG2Y 0x02C
#define IO_BG3PA 0x030
#define IO_BG3PB 0x032
#define IO_BG3PC 0x034
#define IO_BG3PD 0x036
#define IO_BG3X 0x038
#define IO_BG3Y 0x03C
#define IO_WIN0H 0x040
#define IO_WIN1H 0x042
#define IO_WIN0V 0x044
#define IO_WIN1V 0x046
#define IO_WININ 0x048
#define IO_WINOUT 0x04A
#define IO_MOSAIC 0x04C
#define IO_BLDCNT 0x050
#define IO_BLDALPHA 0x052
#define IO_BLDY 0x054

/* DISPCNT bits we use. */
#define DISPCNT_MODE_MASK 0x0007
#define DISPCNT_OBJ_1D_MAP 0x0040
#define DISPCNT_FORCED_BLANK 0x0080
#define DISPCNT_BG0_ON 0x0100
#define DISPCNT_OBJ_ON 0x1000
#define DISPCNT_WIN0_ON 0x2000
#define DISPCNT_WIN1_ON 0x4000
#define DISPCNT_WINOBJ_ON 0x8000

/* BGCNT bits (text + affine). */
#define BGCNT_PRIORITY_MASK 0x0003
#define BGCNT_CHARBASE_SHIFT 2
#define BGCNT_CHARBASE_MASK 0x000C
#define BGCNT_MOSAIC 0x0040
#define BGCNT_256COLOR 0x0080
#define BGCNT_SCREENBASE_MASK 0x1F00
#define BGCNT_SCREENBASE_SHIFT 8
#define BGCNT_AFFINE_WRAP 0x2000
#define BGCNT_SCREENSIZE_MASK 0xC000
#define BGCNT_SCREENSIZE_SHIFT 14

/* OBJ attribute 0. */
#define OBJ_ATTR0_Y_MASK 0x00FF
#define OBJ_ATTR0_AFFINE 0x0100
#define OBJ_ATTR0_DISABLE 0x0200
#define OBJ_ATTR0_DOUBLE_SIZE 0x0200
#define OBJ_ATTR0_MODE_MASK 0x0C00
#define OBJ_ATTR0_MOSAIC 0x1000
#define OBJ_ATTR0_256COLOR 0x2000
#define OBJ_ATTR0_SHAPE_MASK 0xC000
#define OBJ_ATTR0_SHAPE_SHIFT 14

/* OBJ attribute 1. */
#define OBJ_ATTR1_X_MASK 0x01FF
#define OBJ_ATTR1_X_SIGN 0x0100
#define OBJ_ATTR1_AFFINE_GROUP_MASK 0x3E00
#define OBJ_ATTR1_AFFINE_GROUP_SHIFT 9
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
#define VRAM_BG_END 0x10000
#define VRAM_OBJ_BASE 0x10000

/* Layer ids used by the compositor for blending / window enables.
 * The numbering matches BLDCNT / WININ / WINOUT bit layouts. */
#define LAYER_BG0 0
#define LAYER_BG1 1
#define LAYER_BG2 2
#define LAYER_BG3 3
#define LAYER_OBJ 4
#define LAYER_BD 5
#define LAYER_NONE 6

/* Layer-enable bit value for the colour-special-effects flag.
 * Both WININ and WINOUT use bit 5 of each byte for "special effect
 * enable in this region", which matches LAYER_BD. */
#define LAYER_BIT(id) ((uint8_t)(1u << (id)))

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
     * to be safe with -fstrict-aliasing builds. */
    uint16_t v;
    memcpy(&v, &gPortIo[off & 0x3FE], sizeof(v));
    return v;
}

static inline uint32_t io_read32(uint32_t off) {
    uint32_t v;
    memcpy(&v, &gPortIo[off & 0x3FC], sizeof(v));
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

/* Sign-extend an N-bit field stored in the low bits of `v`. */
static inline int32_t sign_extend(uint32_t v, int bits) {
    uint32_t mask = (uint32_t)1u << (bits - 1);
    return (int32_t)((v ^ mask) - mask);
}

/* ------------------------------------------------------------------------ */
/* BG scanline output (shared between text + affine BGs).                   */
/* ------------------------------------------------------------------------ */

/* One palette index per pixel (0 = transparent), plus the resolved BGR555
 * colour for non-transparent pixels.  Storing both lets the compositor
 * blend without re-reading the palette. */
typedef struct {
    uint8_t opaque[DISP_W];
    uint16_t colors[DISP_W];
    int priority;
    int active;
} BgScanline;

/* ------------------------------------------------------------------------ */
/* Text BG scanline rendering (modes 0 / 1).                                */
/* ------------------------------------------------------------------------ */

static void text_bg_dimensions_tiles(uint16_t bgcnt, int* w_tiles_out, int* h_tiles_out) {
    int code = (bgcnt & BGCNT_SCREENSIZE_MASK) >> BGCNT_SCREENSIZE_SHIFT;
    int w_tiles = (code & 1) ? 64 : 32;
    int h_tiles = (code & 2) ? 64 : 32;
    *w_tiles_out = w_tiles;
    *h_tiles_out = h_tiles;
}

static uint32_t text_tilemap_addr(uint16_t bgcnt, int tile_x, int tile_y, int w_tiles, int h_tiles) {
    int screen_base = (bgcnt & BGCNT_SCREENBASE_MASK) >> BGCNT_SCREENBASE_SHIFT;
    uint32_t base = (uint32_t)screen_base * 0x800;

    int sub_x = tile_x / 32;
    int sub_y = tile_y / 32;
    int local_x = tile_x % 32;
    int local_y = tile_y % 32;

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

static uint8_t bg_sample_tile_pixel(uint32_t char_base_addr, int tile_id, int px, int py, int is_8bpp) {
    if (is_8bpp) {
        uint32_t off = char_base_addr + (uint32_t)tile_id * 64 + (uint32_t)(py * 8 + px);
        if (off >= VRAM_BG_END) {
            return 0;
        }
        return gPortVram[off];
    } else {
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
    uint32_t char_base_addr = (uint32_t)char_base * 0x4000;
    int is_8bpp = (bgcnt & BGCNT_256COLOR) != 0;

    out->priority = bgcnt & BGCNT_PRIORITY_MASK;
    out->active = 1;

    int vy = ((int)y + (int)vofs) % map_h_px;
    if (vy < 0) {
        vy += map_h_px;
    }
    int tile_y = vy / 8;
    int py = vy & 7;

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
/* Affine BG scanline rendering (modes 1 / 2).                              */
/* ------------------------------------------------------------------------ */

/* Affine BG screen-size code -> map size in pixels.  Each tile is 8 px;
 * each tilemap entry is 1 byte (an 8 bpp tile id, no flip / palette
 * bank). */
static int affine_bg_size_px(uint16_t bgcnt) {
    int code = (bgcnt & BGCNT_SCREENSIZE_MASK) >> BGCNT_SCREENSIZE_SHIFT;
    static const int kSizes[4] = { 128, 256, 512, 1024 };
    return kSizes[code];
}

/* Read the BG2X / BG2Y / BG3X / BG3Y reference point as an s27.8
 * fixed-point integer.  Hardware exposes a 28-bit field; we sign-extend
 * the top bit into the host int32_t. */
static int32_t affine_ref_point(uint32_t off) {
    uint32_t v = io_read32(off);
    return sign_extend(v & 0x0FFFFFFFu, 28);
}

static void render_affine_bg_scanline(int bg_index, int y, BgScanline* out) {
    uint32_t cnt_off = (bg_index == 2) ? IO_BG2CNT : IO_BG3CNT;
    uint32_t pa_off = (bg_index == 2) ? IO_BG2PA : IO_BG3PA;
    uint32_t x_off = (bg_index == 2) ? IO_BG2X : IO_BG3X;
    uint32_t y_off = (bg_index == 2) ? IO_BG2Y : IO_BG3Y;

    uint16_t bgcnt = io_read16(cnt_off);
    int32_t pa = sign_extend((uint32_t)io_read16(pa_off + 0), 16);
    int32_t pb = sign_extend((uint32_t)io_read16(pa_off + 2), 16);
    int32_t pc = sign_extend((uint32_t)io_read16(pa_off + 4), 16);
    int32_t pd = sign_extend((uint32_t)io_read16(pa_off + 6), 16);
    int32_t ref_x = affine_ref_point(x_off);
    int32_t ref_y = affine_ref_point(y_off);

    int char_base = (bgcnt & BGCNT_CHARBASE_MASK) >> BGCNT_CHARBASE_SHIFT;
    uint32_t char_base_addr = (uint32_t)char_base * 0x4000;
    int screen_base = (bgcnt & BGCNT_SCREENBASE_MASK) >> BGCNT_SCREENBASE_SHIFT;
    uint32_t map_base_addr = (uint32_t)screen_base * 0x800;
    int wrap = (bgcnt & BGCNT_AFFINE_WRAP) != 0;
    int size_px = affine_bg_size_px(bgcnt);
    int size_tiles = size_px / 8;

    out->priority = bgcnt & BGCNT_PRIORITY_MASK;
    out->active = 1;

    /* The PPU's per-scanline reference is `(ref + PB*y, ref + PD*y)`
     * because the X/Y registers encode the *frame's* reference, and the
     * internal counter advances by (PB, PD) per scanline.  We resample
     * the BG2X/Y register every scanline (see "Out of scope" in the
     * file header), so we use those values directly without a separate
     * y-step. */
    int32_t cur_x = ref_x + pb * y;
    int32_t cur_y = ref_y + pd * y;

    for (int x = 0; x < DISP_W; ++x) {
        int32_t tex_x_fp = cur_x + pa * x;
        int32_t tex_y_fp = cur_y + pc * x;
        int32_t tex_x = tex_x_fp >> 8;
        int32_t tex_y = tex_y_fp >> 8;

        if (wrap) {
            tex_x &= (size_px - 1);
            tex_y &= (size_px - 1);
        } else if (tex_x < 0 || tex_x >= size_px || tex_y < 0 || tex_y >= size_px) {
            out->opaque[x] = 0;
            continue;
        }

        int tile_x = (int)(tex_x >> 3);
        int tile_y = (int)(tex_y >> 3);
        int px = (int)(tex_x & 7);
        int py = (int)(tex_y & 7);

        uint32_t entry_addr = map_base_addr + (uint32_t)(tile_y * size_tiles + tile_x);
        if (entry_addr >= VRAM_BG_END) {
            out->opaque[x] = 0;
            continue;
        }
        int tile_id = gPortVram[entry_addr];

        uint32_t pix_off = char_base_addr + (uint32_t)tile_id * 64 + (uint32_t)(py * 8 + px);
        if (pix_off >= VRAM_BG_END) {
            out->opaque[x] = 0;
            continue;
        }
        uint8_t pix = gPortVram[pix_off];
        if (pix == 0) {
            out->opaque[x] = 0;
            continue;
        }
        out->colors[x] = pltt_read16(pix);
        out->opaque[x] = 1;
    }
}

/* ------------------------------------------------------------------------ */
/* OBJ scanline rendering (regular + affine).                               */
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

/* Per-priority OBJ output for one scanline plus the auxiliary masks used
 * by the compositor / window logic.  `semi[p][x]` flags pixels whose
 * sprite was in attr0 mode 1 (semi-transparent) so the compositor can
 * force an alpha blend.  `obj_window` accumulates pixels covered by an
 * opaque OBJ in attr0 mode 2 (OBJ window source). */
typedef struct {
    uint8_t opaque[4][DISP_W];
    uint16_t colors[4][DISP_W];
    uint8_t semi[4][DISP_W];
    uint8_t obj_window[DISP_W];
} ObjScanline;

/* Sample one pixel from an OBJ tile sheet, given the row/col within the
 * sprite and the OBJ tile-mapping mode.  Returns the raw 4 bpp / 8 bpp
 * palette index (0 = transparent). */
static uint8_t obj_sample_pixel(int row_in_sprite, int col_in_sprite, int sprite_w_tiles, int base_tile, int is_8bpp,
                                int obj_1d_mapping) {
    int row_tile = row_in_sprite / 8;
    int row_within_tile = row_in_sprite & 7;
    int col_tile = col_in_sprite / 8;
    int col_within_tile = col_in_sprite & 7;

    int eff_base_tile = is_8bpp ? (base_tile & ~1) : base_tile;

    int row_tile_step;
    if (obj_1d_mapping) {
        row_tile_step = row_tile * sprite_w_tiles * (is_8bpp ? 2 : 1);
    } else {
        row_tile_step = row_tile * 32;
    }

    int tile_offset_in_units;
    if (is_8bpp) {
        tile_offset_in_units = row_tile_step + col_tile * 2;
    } else {
        tile_offset_in_units = row_tile_step + col_tile;
    }
    int tile_id = (eff_base_tile + tile_offset_in_units) & 0x3FF;
    uint32_t tile_addr = (uint32_t)VRAM_OBJ_BASE + (uint32_t)tile_id * 32;

    if (is_8bpp) {
        uint32_t off = tile_addr + (uint32_t)(row_within_tile * 8 + col_within_tile);
        if (off >= sizeof(gPortVram)) {
            return 0;
        }
        return gPortVram[off];
    } else {
        uint32_t off = tile_addr + (uint32_t)(row_within_tile * 4 + (col_within_tile >> 1));
        if (off >= sizeof(gPortVram)) {
            return 0;
        }
        uint8_t b = gPortVram[off];
        return (col_within_tile & 1) ? (b >> 4) : (b & 0x0F);
    }
}

/* Read PA / PB / PC / PD for the given affine group from OAM.  Each
 * group's four parameters are interleaved in the high half of four
 * consecutive 8-byte OAM entries: group N occupies OAM[N*32 + 6..7],
 * OAM[N*32 + 14..15], OAM[N*32 + 22..23], OAM[N*32 + 30..31]. */
static void obj_read_affine_params(int group, int32_t* pa, int32_t* pb, int32_t* pc, int32_t* pd) {
    uint32_t base = (uint32_t)(group & 0x1F) * 32;
    uint16_t ra, rb, rc, rd;
    memcpy(&ra, &gPortOam[base + 6], 2);
    memcpy(&rb, &gPortOam[base + 14], 2);
    memcpy(&rc, &gPortOam[base + 22], 2);
    memcpy(&rd, &gPortOam[base + 30], 2);
    *pa = sign_extend(ra, 16);
    *pb = sign_extend(rb, 16);
    *pc = sign_extend(rc, 16);
    *pd = sign_extend(rd, 16);
}

/* Decode the MOSAIC register: returns horizontal / vertical mosaic
 * sizes for BGs and for OBJs.  Each is in [1, 16]. */
static void mosaic_sizes(int* bg_h, int* bg_v, int* obj_h, int* obj_v) {
    uint16_t m = io_read16(IO_MOSAIC);
    *bg_h = ((m >> 0) & 0xF) + 1;
    *bg_v = ((m >> 4) & 0xF) + 1;
    *obj_h = ((m >> 8) & 0xF) + 1;
    *obj_v = ((m >> 12) & 0xF) + 1;
}

static void render_obj_scanline(int y, int obj_1d_mapping, ObjScanline* out) {
    int bg_h, bg_v, obj_h, obj_v;
    mosaic_sizes(&bg_h, &bg_v, &obj_h, &obj_v);
    (void)bg_h;
    (void)bg_v;

    /* OAM holds 128 sprites x 8 bytes each. */
    for (int idx = 0; idx < 128; ++idx) {
        uint32_t base = (uint32_t)idx * 8;
        uint16_t a0, a1, a2;
        memcpy(&a0, &gPortOam[base + 0], 2);
        memcpy(&a1, &gPortOam[base + 2], 2);
        memcpy(&a2, &gPortOam[base + 4], 2);

        int affine = (a0 & OBJ_ATTR0_AFFINE) != 0;
        int double_size = (a0 & OBJ_ATTR0_DOUBLE_SIZE) != 0;
        if (!affine && (a0 & OBJ_ATTR0_DISABLE)) {
            continue;
        }
        int mode = (a0 & OBJ_ATTR0_MODE_MASK) >> 10;
        if (mode == 3) {
            continue; /* prohibited */
        }
        int is_obj_window_source = (mode == 2);

        int shape = (a0 & OBJ_ATTR0_SHAPE_MASK) >> OBJ_ATTR0_SHAPE_SHIFT;
        int size = (a1 & OBJ_ATTR1_SIZE_MASK) >> OBJ_ATTR1_SIZE_SHIFT;
        int sw = kObjDimensions[shape * 4 + size][0];
        int sh = kObjDimensions[shape * 4 + size][1];
        if (sw == 0) {
            continue;
        }

        /* Bounding box on screen.  For affine sprites with the
         * DOUBLE_SIZE bit set, the sprite occupies a 2*sw x 2*sh box
         * centred on (sx + sw, sy + sh); the texture sampling still
         * uses sw x sh internally. */
        int box_w = (affine && double_size) ? sw * 2 : sw;
        int box_h = (affine && double_size) ? sh * 2 : sh;

        int sy = a0 & OBJ_ATTR0_Y_MASK;
        int line_in_box = (y - sy) & 0xFF;
        if (line_in_box >= box_h) {
            continue;
        }

        int sx = a1 & OBJ_ATTR1_X_MASK;
        if (sx & OBJ_ATTR1_X_SIGN) {
            sx -= 0x200;
        }
        if (sx + box_w <= 0 || sx >= DISP_W) {
            continue;
        }

        int is_8bpp = (a0 & OBJ_ATTR0_256COLOR) != 0;
        int prio = (a2 & OBJ_ATTR2_PRIORITY_MASK) >> OBJ_ATTR2_PRIORITY_SHIFT;
        int pal_bank = (a2 & OBJ_ATTR2_PALETTE_MASK) >> OBJ_ATTR2_PALETTE_SHIFT;
        int base_tile = a2 & OBJ_ATTR2_TILE_MASK;
        int sprite_w_tiles = sw / 8;
        int mosaic_on = (a0 & OBJ_ATTR0_MOSAIC) != 0;

        if (affine) {
            int group = (a1 & OBJ_ATTR1_AFFINE_GROUP_MASK) >> OBJ_ATTR1_AFFINE_GROUP_SHIFT;
            int32_t pa, pb, pc, pd;
            obj_read_affine_params(group, &pa, &pb, &pc, &pd);

            int half_box_w = box_w / 2;
            int half_box_h = box_h / 2;
            int half_sw = sw / 2;
            int half_sh = sh / 2;
            int dy = line_in_box - half_box_h;
            if (mosaic_on) {
                /* Snap the screen y to the mosaic V grid.  Because the
                 * sprite is sampled relative to display coordinates
                 * here, we adjust dy so that all pixels in the same
                 * mosaic V block share an identical sprite-y coord. */
                int snapped_y = (y / obj_v) * obj_v;
                dy = (snapped_y - sy - half_box_h);
            }

            for (int col = 0; col < box_w; ++col) {
                int screen_x = sx + col;
                if (screen_x < 0 || screen_x >= DISP_W) {
                    continue;
                }
                int dx = col - half_box_w;
                if (mosaic_on) {
                    int snapped_x = (screen_x / obj_h) * obj_h;
                    dx = snapped_x - sx - half_box_w;
                }

                /* tex = P * (dx, dy) + (sw/2, sh/2) */
                int32_t tex_x_fp = pa * dx + pb * dy;
                int32_t tex_y_fp = pc * dx + pd * dy;
                int tex_x = (int)(tex_x_fp >> 8) + half_sw;
                int tex_y = (int)(tex_y_fp >> 8) + half_sh;
                if (tex_x < 0 || tex_x >= sw || tex_y < 0 || tex_y >= sh) {
                    continue;
                }

                uint8_t pix = obj_sample_pixel(tex_y, tex_x, sprite_w_tiles, base_tile, is_8bpp, obj_1d_mapping);
                if (pix == 0) {
                    continue;
                }
                if (is_obj_window_source) {
                    /* OBJ-window sprites contribute to the OBJ-window
                     * mask only — they do not paint colour. */
                    out->obj_window[screen_x] = 1;
                    continue;
                }
                /* GBA: for equal OBJ priority, higher OAM index draws on top. */
                int pal_index = is_8bpp ? pix : (pal_bank * 16 + pix);
                out->colors[prio][screen_x] = pltt_read16(256 + pal_index);
                out->opaque[prio][screen_x] = 1;
                out->semi[prio][screen_x] = (mode == 1);
            }
        } else {
            int hflip = (a1 & OBJ_ATTR1_HFLIP) != 0;
            int vflip = (a1 & OBJ_ATTR1_VFLIP) != 0;

            int row_in_sprite = vflip ? (sh - 1 - line_in_box) : line_in_box;
            if (mosaic_on) {
                int snapped_y = (y / obj_v) * obj_v;
                int snapped_line = (snapped_y - sy) & 0xFF;
                if (snapped_line >= sh) {
                    snapped_line = sh - 1;
                }
                row_in_sprite = vflip ? (sh - 1 - snapped_line) : snapped_line;
            }

            for (int col = 0; col < box_w; ++col) {
                int screen_x = sx + col;
                if (screen_x < 0 || screen_x >= DISP_W) {
                    continue;
                }
                int eff_col = col;
                if (mosaic_on) {
                    int snapped_x = (screen_x / obj_h) * obj_h;
                    eff_col = snapped_x - sx;
                    if (eff_col < 0)
                        eff_col = 0;
                    if (eff_col >= sw)
                        eff_col = sw - 1;
                }
                int col_in_sprite = hflip ? (sw - 1 - eff_col) : eff_col;

                uint8_t pix =
                    obj_sample_pixel(row_in_sprite, col_in_sprite, sprite_w_tiles, base_tile, is_8bpp, obj_1d_mapping);
                if (pix == 0) {
                    continue;
                }
                if (is_obj_window_source) {
                    out->obj_window[screen_x] = 1;
                    continue;
                }
                int pal_index = is_8bpp ? pix : (pal_bank * 16 + pix);
                out->colors[prio][screen_x] = pltt_read16(256 + pal_index);
                out->opaque[prio][screen_x] = 1;
                out->semi[prio][screen_x] = (mode == 1);
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Mosaic post-pass for BGs and OBJs.                                        */
/* ------------------------------------------------------------------------ */

/* Snap pixels in [0, DISP_W) to the start of each H-mosaic block: every
 * pixel within block N (covering [N*Mh, N*Mh + Mh)) takes the value at
 * its block's leftmost on-screen pixel. */
static void apply_h_mosaic_bg(BgScanline* s, int mh) {
    if (mh <= 1)
        return;
    for (int x = 0; x < DISP_W; ++x) {
        int anchor = (x / mh) * mh;
        if (anchor != x) {
            s->opaque[x] = s->opaque[anchor];
            s->colors[x] = s->colors[anchor];
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Window mask computation.                                                  */
/* ------------------------------------------------------------------------ */

/* For each x along this scanline, the per-pixel layer-enable mask
 * selected by the highest-priority window that covers it
 * (WIN0 > WIN1 > OBJ window > outside). */
typedef struct {
    uint8_t enable[DISP_W];
    int any_window_enabled; /* 1 iff DISPCNT enables WIN0/WIN1/OBJ window */
} WindowMask;

/* Decode WININ / WINOUT byte into a layer-enable mask.  The byte's low
 * 6 bits map directly onto BG0/BG1/BG2/BG3/OBJ/EFFECT (= LAYER_BD). */
static uint8_t window_byte_to_mask(uint8_t b) {
    return (uint8_t)(b & 0x3F);
}

/* Test whether the scanline `y` falls inside WINx's vertical span,
 * honouring the GBA quirks: y2 <= y1 means the window wraps, covering
 * [y1, DISP_H) || [0, y2), and y1 == y2 therefore means full height.
 * Endpoints beyond the display height clamp to DISP_H. */
static int win_v_in_range(uint16_t winv, int y) {
    int y1 = (winv >> 8) & 0xFF;
    int y2 = winv & 0xFF;

    if (y1 > DISP_H) {
        y1 = DISP_H;
    }
    if (y2 > DISP_H) {
        y2 = DISP_H;
    }

    if (y2 > y1) {
        return (y >= y1) && (y < y2);
    }
    return (y >= y1) || (y < y2);
}

static void win_h_range(uint16_t winh, int* x1_out, int* x2_out) {
    int x1 = (winh >> 8) & 0xFF;
    int x2 = winh & 0xFF;

    if (x1 > DISP_W) {
        x1 = DISP_W;
    }
    if (x2 > DISP_W) {
        x2 = DISP_W;
    }

    /* Preserve x2 <= x1 so callers can distinguish wrapped windows:
     * [x1, DISP_W) || [0, x2). */
    *x1_out = x1;
    *x2_out = x2;
}

static int win_h_in_range(int x, int x1, int x2) {
    if (x2 <= x1) {
        return (x >= x1) || (x < x2);
    }
    return (x >= x1) && (x < x2);
}
static void compute_window_mask(int y, uint16_t dispcnt, const ObjScanline* obj, WindowMask* out) {
    int win0_on = (dispcnt & DISPCNT_WIN0_ON) != 0;
    int win1_on = (dispcnt & DISPCNT_WIN1_ON) != 0;
    int winobj_on = (dispcnt & DISPCNT_WINOBJ_ON) != 0;
    out->any_window_enabled = (win0_on || win1_on || winobj_on);
    if (!out->any_window_enabled) {
        for (int x = 0; x < DISP_W; ++x) {
            out->enable[x] = 0x3F; /* everything visible everywhere */
        }
        return;
    }

    uint16_t winin = io_read16(IO_WININ);
    uint16_t winout = io_read16(IO_WINOUT);
    uint8_t mask_in0 = window_byte_to_mask(winin & 0xFF);
    uint8_t mask_in1 = window_byte_to_mask((winin >> 8) & 0xFF);
    uint8_t mask_outside = window_byte_to_mask(winout & 0xFF);
    uint8_t mask_objwin = window_byte_to_mask((winout >> 8) & 0xFF);

    int win0_h_x1 = 0, win0_h_x2 = 0;
    int win1_h_x1 = 0, win1_h_x2 = 0;
    int win0_v_in = 0, win1_v_in = 0;
    if (win0_on) {
        win0_v_in = win_v_in_range(io_read16(IO_WIN0V), y);
        if (win0_v_in) {
            win_h_range(io_read16(IO_WIN0H), &win0_h_x1, &win0_h_x2);
        }
    }
    if (win1_on) {
        win1_v_in = win_v_in_range(io_read16(IO_WIN1V), y);
        if (win1_v_in) {
            win_h_range(io_read16(IO_WIN1H), &win1_h_x1, &win1_h_x2);
        }
    }

    for (int x = 0; x < DISP_W; ++x) {
        if (win0_on && win0_v_in && win_h_in_range(x, win0_h_x1, win0_h_x2)) {
            out->enable[x] = mask_in0;
        } else if (win1_on && win1_v_in && win_h_in_range(x, win1_h_x1, win1_h_x2)) {
            out->enable[x] = mask_in1;
        } else if (winobj_on && obj->obj_window[x]) {
            out->enable[x] = mask_objwin;
        } else {
            out->enable[x] = mask_outside;
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Per-pixel composition + blending.                                        */
/* ------------------------------------------------------------------------ */

static int bg_is_text_in_mode(int mode, int bg_index) {
    switch (mode) {
        case 0:
            return 1;
        case 1:
            return bg_index <= 1; /* BG0/BG1 text */
        case 2:
            return 0;
        default:
            return 0;
    }
}

static int bg_is_affine_in_mode(int mode, int bg_index) {
    switch (mode) {
        case 1:
            return bg_index == 2;
        case 2:
            return bg_index == 2 || bg_index == 3;
        default:
            return 0;
    }
}

/* Blend two BGR555 colours using EVA/EVB in 0..16.  The arithmetic
 * matches the GBA: per-channel `min(31, (top*EVA + bot*EVB) >> 4)`. */
static uint16_t blend_alpha(uint16_t top, uint16_t bot, int eva, int evb) {
    if (eva > 16)
        eva = 16;
    if (evb > 16)
        evb = 16;
    int rt = top & 0x1F, gt = (top >> 5) & 0x1F, bt = (top >> 10) & 0x1F;
    int rb = bot & 0x1F, gb = (bot >> 5) & 0x1F, bb = (bot >> 10) & 0x1F;
    int r = (rt * eva + rb * evb) >> 4;
    int g = (gt * eva + gb * evb) >> 4;
    int b = (bt * eva + bb * evb) >> 4;
    if (r > 31)
        r = 31;
    if (g > 31)
        g = 31;
    if (b > 31)
        b = 31;
    return (uint16_t)(r | (g << 5) | (b << 10));
}

/* Brightness fade: `top + (max - top) * EVY / 16` for fade-to-white,
 * `top - top * EVY / 16` for fade-to-black, per channel. */
static uint16_t blend_brighter(uint16_t top, int evy) {
    if (evy > 16)
        evy = 16;
    int r = top & 0x1F, g = (top >> 5) & 0x1F, b = (top >> 10) & 0x1F;
    r = r + ((31 - r) * evy >> 4);
    g = g + ((31 - g) * evy >> 4);
    b = b + ((31 - b) * evy >> 4);
    return (uint16_t)(r | (g << 5) | (b << 10));
}
static uint16_t blend_darker(uint16_t top, int evy) {
    if (evy > 16)
        evy = 16;
    int r = top & 0x1F, g = (top >> 5) & 0x1F, b = (top >> 10) & 0x1F;
    r = r - (r * evy >> 4);
    g = g - (g * evy >> 4);
    b = b - (b * evy >> 4);
    return (uint16_t)(r | (g << 5) | (b << 10));
}

/* Find the top opaque pixel for column `x` according to GBA priority:
 * priority 0 first, within a priority OBJs sit on top of BGs of equal
 * priority, lower BG index wins among text BGs of equal priority.  The
 * `enable_mask` gates the search (window-enable bits per layer).  When
 * `start_layer` != LAYER_NONE the search is restricted to layers that
 * sit *under* `start_layer` (used to find the 2nd-target pixel for
 * alpha blending).  Returns LAYER_NONE if nothing opaque is found
 * (caller falls back to backdrop). */
static int find_top_layer(const BgScanline bg[4], const ObjScanline* obj, int x, uint8_t enable_mask, int start_layer,
                          int start_priority, uint16_t* color_out, int* prio_out, int* semi_out) {
    /* Walk by priority, then by per-priority order: OBJ first (since
     * OBJ sits on top of BG of equal priority), then BG0..BG3.  When
     * `start_layer` is LAYER_NONE the whole table is in scope; otherwise
     * we skip everything strictly above `(start_priority, start_layer)`
     * in the same composition order. */
    static const int kBgOrder[4] = { LAYER_BG0, LAYER_BG1, LAYER_BG2, LAYER_BG3 };
    int passed_start = (start_layer == LAYER_NONE);
    for (int prio = 0; prio < 4; ++prio) {
        /* OBJ at this priority. */
        int layer = LAYER_OBJ;
        if (!passed_start) {
            if (prio == start_priority && layer == start_layer) {
                passed_start = 1;
                /* fall through; this slot itself is the start, skip it */
                goto skip_obj;
            }
        } else if (enable_mask & LAYER_BIT(layer)) {
            if (obj->opaque[prio][x]) {
                *color_out = obj->colors[prio][x];
                *prio_out = prio;
                if (semi_out)
                    *semi_out = obj->semi[prio][x];
                return LAYER_OBJ;
            }
        }
    skip_obj:
        for (int b = 0; b < 4; ++b) {
            layer = kBgOrder[b];
            if (!passed_start) {
                if (prio == start_priority && layer == start_layer) {
                    passed_start = 1;
                    continue;
                }
                continue;
            }
            if (!bg[layer].active)
                continue;
            if (bg[layer].priority != prio)
                continue;
            if (!(enable_mask & LAYER_BIT(layer)))
                continue;
            if (bg[layer].opaque[x]) {
                *color_out = bg[layer].colors[x];
                *prio_out = prio;
                if (semi_out)
                    *semi_out = 0;
                return layer;
            }
        }
    }
    return LAYER_NONE;
}

static void composite_scanline(uint32_t* out_row, int y, uint16_t dispcnt) {
    uint16_t backdrop = pltt_read16(0);
    uint32_t backdrop_argb = bgr555_to_argb8888(backdrop);

    int mode = dispcnt & DISPCNT_MODE_MASK;
    if (mode > 2) {
        /* Bitmap modes still deferred; fill with backdrop. */
        for (int x = 0; x < DISP_W; ++x) {
            out_row[x] = backdrop_argb;
        }
        return;
    }

    int bg_h_mosaic, bg_v_mosaic, obj_h_mosaic, obj_v_mosaic;
    mosaic_sizes(&bg_h_mosaic, &bg_v_mosaic, &obj_h_mosaic, &obj_v_mosaic);
    (void)obj_h_mosaic;
    (void)obj_v_mosaic;

    /* Render each BG at its (possibly mosaic-snapped) scanline. */
    BgScanline bg[4];
    memset(bg, 0, sizeof(bg));
    for (int b = 0; b < 4; ++b) {
        if (!(dispcnt & (DISPCNT_BG0_ON << b)))
            continue;
        uint16_t bgcnt = io_read16(IO_BG0CNT + b * 2);
        int mosaic_on = (bgcnt & BGCNT_MOSAIC) != 0;
        int sample_y = y;
        if (mosaic_on && bg_v_mosaic > 1) {
            sample_y = (y / bg_v_mosaic) * bg_v_mosaic;
        }
        if (bg_is_text_in_mode(mode, b)) {
            render_text_bg_scanline(b, sample_y, &bg[b]);
        } else if (bg_is_affine_in_mode(mode, b)) {
            render_affine_bg_scanline(b, sample_y, &bg[b]);
        } else {
            continue;
        }
        if (mosaic_on && bg_h_mosaic > 1) {
            apply_h_mosaic_bg(&bg[b], bg_h_mosaic);
        }
    }

    ObjScanline obj;
    memset(&obj, 0, sizeof(obj));
    if (dispcnt & DISPCNT_OBJ_ON) {
        int one_d = (dispcnt & DISPCNT_OBJ_1D_MAP) != 0;
        render_obj_scanline(y, one_d, &obj);
    }

    WindowMask winmask;
    compute_window_mask(y, dispcnt, &obj, &winmask);

    uint16_t bldcnt = io_read16(IO_BLDCNT);
    uint16_t bldalpha = io_read16(IO_BLDALPHA);
    uint16_t bldy_reg = io_read16(IO_BLDY);
    int blend_mode = (bldcnt >> 6) & 0x3;
    uint8_t first_target = (uint8_t)(bldcnt & 0x3F);
    uint8_t second_target = (uint8_t)((bldcnt >> 8) & 0x3F);
    int eva = bldalpha & 0x1F;
    int evb = (bldalpha >> 8) & 0x1F;
    int evy = bldy_reg & 0x1F;

    for (int x = 0; x < DISP_W; ++x) {
        uint8_t enable = winmask.enable[x];
        int blend_allowed = (enable & LAYER_BIT(LAYER_BD)) != 0; /* "effects enabled" bit */

        uint16_t top_color = backdrop;
        int top_prio = 4;
        int top_semi = 0;
        int top_layer = find_top_layer(bg, &obj, x, enable, LAYER_NONE, 0, &top_color, &top_prio, &top_semi);
        if (top_layer == LAYER_NONE) {
            top_layer = LAYER_BD;
            top_color = backdrop;
            top_prio = 4;
            top_semi = 0;
        }

        uint16_t result = top_color;

        /* OBJ semi-transparent forces alpha blend if a 2nd-target pixel
         * is below.  Otherwise consult BLDCNT. */
        int do_alpha = 0;
        int do_brighter = 0;
        int do_darker = 0;
        if (top_layer == LAYER_OBJ && top_semi && blend_allowed) {
            do_alpha = 1;
        } else if (blend_allowed && (first_target & LAYER_BIT(top_layer))) {
            if (blend_mode == 1)
                do_alpha = 1;
            else if (blend_mode == 2)
                do_brighter = 1;
            else if (blend_mode == 3)
                do_darker = 1;
        }

        if (do_alpha) {
            /* Find the 2nd-target pixel directly below the top pixel. */
            uint16_t bot_color = backdrop;
            int bot_prio = 4;
            int bot_layer = find_top_layer(bg, &obj, x, enable, top_layer, top_prio, &bot_color, &bot_prio, NULL);
            if (bot_layer == LAYER_NONE) {
                bot_layer = LAYER_BD;
                bot_color = backdrop;
            }
            if (second_target & LAYER_BIT(bot_layer)) {
                result = blend_alpha(top_color, bot_color, eva, evb);
            }
            /* If the candidate 2nd layer wasn't a 2nd target, OBJ semi
             * still falls back to opaque (no fade applies because the
             * OBJ was never 1st target through BLDCNT mode 2/3). */
        } else if (do_brighter) {
            result = blend_brighter(top_color, evy);
        } else if (do_darker) {
            result = blend_darker(top_color, evy);
        }

        out_row[x] = bgr555_to_argb8888(result);
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
        for (int i = 0; i < DISP_W * DISP_H; ++i) {
            framebuffer[i] = 0xFFFFFFFFu;
        }
        return;
    }

    for (int y = 0; y < DISP_H; ++y) {
        composite_scanline(framebuffer + (uint32_t)y * DISP_W, y, dispcnt);
    }

#ifdef __PORT__
    /* Host-only visibility fallback: when unresolved room/map assets produce
     * an all-black frame, tint it so users can distinguish "running with
     * missing data" from a perceived hard hang. Keep self-check semantics
     * intact by only enabling this once the game loop has advanced. */
    if (Port_GetFrameCount() > 0) {
        int any_non_black = 0;
        for (int i = 0; i < DISP_W * DISP_H; ++i) {
            if (framebuffer[i] != 0xFF000000u) {
                any_non_black = 1;
                break;
            }
        }
        if (!any_non_black) {
            for (int i = 0; i < DISP_W * DISP_H; ++i) {
                framebuffer[i] = 0xFF00A000u;
            }
        }
    }
#endif
}

/* ------------------------------------------------------------------------ */
/* Headless self-check (PRs #4 + #5).                                       */
/* ------------------------------------------------------------------------ */

/* The self-check programs the host arrays directly, so save/restore
 * everything it might touch.  Sized for the biggest region we reach
 * (a 32x32 BG screen + a 256x256 affine BG map + char data + OAM). */
typedef struct {
    uint8_t io[0x60];
    uint8_t pltt[PORT_PLTT_SIZE];
    uint8_t vram_lo[0x4000];  /* BG char + screen bases used here */
    uint8_t vram_obj[0x4000]; /* OBJ tile sheet */
    uint8_t oam[PORT_OAM_SIZE];
} RendererSnapshot;

static void snapshot_save(RendererSnapshot* s) {
    memcpy(s->io, &gPortIo[0], sizeof(s->io));
    memcpy(s->pltt, &gPortPltt[0], sizeof(s->pltt));
    memcpy(s->vram_lo, &gPortVram[0], sizeof(s->vram_lo));
    memcpy(s->vram_obj, &gPortVram[VRAM_OBJ_BASE], sizeof(s->vram_obj));
    memcpy(s->oam, &gPortOam[0], sizeof(s->oam));
}
static void snapshot_restore(const RendererSnapshot* s) {
    memcpy(&gPortIo[0], s->io, sizeof(s->io));
    memcpy(&gPortPltt[0], s->pltt, sizeof(s->pltt));
    memcpy(&gPortVram[0], s->vram_lo, sizeof(s->vram_lo));
    memcpy(&gPortVram[VRAM_OBJ_BASE], s->vram_obj, sizeof(s->vram_obj));
    memcpy(&gPortOam[0], s->oam, sizeof(s->oam));
}

static void io_write16(uint32_t off, uint16_t v) {
    memcpy(&gPortIo[off & 0x3FE], &v, sizeof(v));
}
static void io_write32(uint32_t off, uint32_t v) {
    memcpy(&gPortIo[off & 0x3FC], &v, sizeof(v));
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

/* Reset every IO register and OAM the renderer reads to a sensible
 * "off" baseline so a previous step's leftovers can't pollute the next
 * one. */
static void self_check_reset(void) {
    memset(&gPortIo[0], 0, 0x60);
    /* Disable every sprite. */
    for (int i = 0; i < 128; ++i) {
        oam_write16((uint32_t)i * 8, OBJ_ATTR0_DISABLE);
        oam_write16((uint32_t)i * 8 + 2, 0);
        oam_write16((uint32_t)i * 8 + 4, 0);
    }
}

int Port_RendererSelfCheck(void) {
    RendererSnapshot snap;
    snapshot_save(&snap);

    static uint32_t fb[DISP_W * DISP_H];
    int ok = 1;

    /* ---------- 1. Forced blank yields a fully-white frame. -------- */
    self_check_reset();
    io_write16(IO_DISPCNT, DISPCNT_FORCED_BLANK);
    Port_RenderFrame(fb);
    ok = ok && (fb[0] == 0xFFFFFFFFu) && (fb[DISP_W * DISP_H - 1] == 0xFFFFFFFFu);

    /* ---------- 2. Backdrop colour. -------------------------------- */
    self_check_reset();
    pltt_write16(0, 0x001F); /* pure red */
    Port_RenderFrame(fb);
    ok = ok && (fb[123] == 0xFFFF0000u);

    /* ---------- 3. BG0 mode-0 4 bpp single-tile coverage. ---------- */
    self_check_reset();
    pltt_write16(0, 0x0000);
    pltt_write16(1, 0x03E0); /* green */
    for (int i = 0; i < 32; ++i) {
        gPortVram[i] = 0x11;
    }
    /* Clear the screen base region we use (32*32 entries = 2 KiB). */
    memset(&gPortVram[0x800], 0, 0x800);
    /* tilemap entry 0 -> tile 0 / pal bank 0 / no flip. */
    vram_write16(0x800, 0x0000);
    io_write16(IO_BG0CNT, (1 << BGCNT_SCREENBASE_SHIFT));
    io_write16(IO_DISPCNT, DISPCNT_BG0_ON);
    Port_RenderFrame(fb);
    uint32_t green_argb = bgr555_to_argb8888(0x03E0);
    ok = ok && (fb[0] == green_argb);
    ok = ok && (fb[DISP_W * DISP_H - 1] == green_argb);

    /* ---------- 4. Tilemap entry hflip / vflip / pal bank. -------- */
    self_check_reset();
    pltt_write16(0, 0x0000);
    pltt_write16(1, 0x03E0);
    pltt_write16(2, 0x7C00);      /* blue */
    pltt_write16(16 + 1, 0x001F); /* pal bank 1 idx 1 = red */
    memset(&gPortVram[0], 0x11, 32);
    gPortVram[31] = (2 << 4) | 1; /* (7,7) -> palette idx 2 */
    memset(&gPortVram[0x800], 0, 0x800);
    vram_write16(0x800, 0x0000);
    io_write16(IO_BG0CNT, (1 << BGCNT_SCREENBASE_SHIFT));
    io_write16(IO_DISPCNT, DISPCNT_BG0_ON);
    Port_RenderFrame(fb);
    uint32_t blue_argb = bgr555_to_argb8888(0x7C00);
    ok = ok && (fb[7 + 7 * DISP_W] == blue_argb);
    vram_write16(0x800, 0x0400); /* hflip */
    Port_RenderFrame(fb);
    ok = ok && (fb[0 + 7 * DISP_W] == blue_argb);
    vram_write16(0x800, 0x0800); /* vflip */
    Port_RenderFrame(fb);
    ok = ok && (fb[7 + 0 * DISP_W] == blue_argb);
    vram_write16(0x800, 0x1000); /* pal bank 1 */
    Port_RenderFrame(fb);
    uint32_t red_argb = bgr555_to_argb8888(0x001F);
    ok = ok && (fb[0] == red_argb);

    /* ---------- 5. BG0 scroll (HOFS = 8 swaps tile 0 / tile 1). --- */
    self_check_reset();
    pltt_write16(0, 0x0000);
    pltt_write16(1, 0x03E0);
    pltt_write16(3, 0x03FF);          /* yellow */
    memset(&gPortVram[0], 0x11, 32);  /* tile 0 = idx 1 */
    memset(&gPortVram[32], 0x33, 32); /* tile 1 = idx 3 */
    memset(&gPortVram[0x800], 0, 0x800);
    vram_write16(0x800 + 0, 0x0000);
    vram_write16(0x800 + 2, 0x0001);
    io_write16(IO_BG0CNT, (1 << BGCNT_SCREENBASE_SHIFT));
    io_write16(IO_BG0HOFS, 8);
    io_write16(IO_DISPCNT, DISPCNT_BG0_ON);
    Port_RenderFrame(fb);
    uint32_t yellow_argb = bgr555_to_argb8888(0x03FF);
    ok = ok && (fb[0] == yellow_argb);

    /* ---------- 6. OBJ rendering and priority composition. -------- */
    self_check_reset();
    pltt_write16(0, 0x0000);
    pltt_write16(1, 0x03E0);
    pltt_write16(256 + 1, 0x7FFF); /* OBJ pal idx 1 = white */
    memset(&gPortVram[VRAM_OBJ_BASE], 0x11, 32);
    /* OAM 0: 8x8 sprite at (0,0) using OBJ tile 0, priority 0. */
    oam_write16(0, 0x0000);
    oam_write16(2, 0x0000);
    oam_write16(4, 0x0000);
    /* BG0: same backdrop config; show that OBJ wins at equal priority. */
    memset(&gPortVram[0], 0x11, 32);
    memset(&gPortVram[0x800], 0, 0x800);
    vram_write16(0x800, 0x0000);
    io_write16(IO_BG0CNT, (1 << BGCNT_SCREENBASE_SHIFT));
    io_write16(IO_DISPCNT, DISPCNT_BG0_ON | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    Port_RenderFrame(fb);
    uint32_t white_argb = bgr555_to_argb8888(0x7FFF);
    ok = ok && (fb[0] == white_argb);

    /* OBJ disable bit hides the sprite. */
    oam_write16(0, OBJ_ATTR0_DISABLE);
    Port_RenderFrame(fb);
    ok = ok && (fb[0] == green_argb);
    oam_write16(0, 0);

    /* OBJ moved to priority 3 -> BG0 (priority 0) wins. */
    oam_write16(4, 0x0C00);
    Port_RenderFrame(fb);
    ok = ok && (fb[0] == green_argb);

    /* Same OBJ priority: higher OAM index draws on top (GBA behaviour). */
    oam_write16(4, 0x0000);
    pltt_write16(256 + 1, 0x001F); /* OBJ pal 0 idx 1 = red */
    pltt_write16(256 + 16 + 1, 0x7FFF); /* OBJ pal 1 idx 1 = white */
    memset(&gPortVram[VRAM_OBJ_BASE], 0x11, 32); /* tile 0: all palette idx 1 */
    oam_write16(0, 0x0000);
    oam_write16(2, 0x0000);
    oam_write16(4, 0x0000); /* pal 0 */
    oam_write16(80 + 0, 0x0000);
    oam_write16(80 + 2, 0x0000);
    oam_write16(80 + 4, 0x1000); /* pal 1, same tile, same prio, higher OAM idx */
    io_write16(IO_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    Port_RenderFrame(fb);
    ok = ok && (fb[0] == white_argb);

    /* ---------- 7. Affine BG (mode 1, BG2). ----------------------- */
    /* Build a 16x16-tile (128x128 px) affine map where tile 0 covers
     * the first cell only and contains palette idx 1 (green).  Using
     * the identity affine transform with reference (0,0), the (0,0)
     * pixel should sample tile 0 / palette idx 1.  Outside the map
     * area we test both `wrap` and `transparent`. */
    self_check_reset();
    pltt_write16(0, 0x0000); /* black backdrop */
    pltt_write16(1, 0x03E0); /* green */
    /* Affine BG2: char base 0, screen base at offset 0x800 (= screen
     * base block 1).  16x16 tilemap.  Tile 0 = solid palette idx 1. */
    memset(&gPortVram[0], 1, 64);      /* tile 0: every byte = 1 */
    memset(&gPortVram[64], 0, 64);     /* tile 1: all transparent */
    memset(&gPortVram[0x800], 0, 256); /* clear 16x16 map: tile 0 everywhere */
    /* PA = PD = 0x100 (1.0), PB = PC = 0; ref (0,0). */
    io_write16(IO_BG2PA, 0x0100);
    io_write16(IO_BG2PB, 0x0000);
    io_write16(IO_BG2PC, 0x0000);
    io_write16(IO_BG2PD, 0x0100);
    io_write32(IO_BG2X, 0);
    io_write32(IO_BG2Y, 0);
    /* BGCNT: priority 0, char base 0, screen base block 1, size 0
     * (128x128), wrap off. */
    io_write16(IO_BG2CNT, (1 << BGCNT_SCREENBASE_SHIFT));
    io_write16(IO_DISPCNT, 1 | (1 << 10)); /* mode 1, BG2 on */
    Port_RenderFrame(fb);
    /* Inside the 128x128 map: green.  Outside (>=128, no wrap): backdrop. */
    ok = ok && (fb[0 + 0 * DISP_W] == green_argb);
    ok = ok && (fb[127 + 0 * DISP_W] == green_argb);
    ok = ok && (fb[128 + 0 * DISP_W] == 0xFF000000u); /* outside, transparent */
    /* Enable wrap -> the column at x=128 wraps back to tile 0 / green. */
    io_write16(IO_BG2CNT, (1 << BGCNT_SCREENBASE_SHIFT) | BGCNT_AFFINE_WRAP);
    Port_RenderFrame(fb);
    ok = ok && (fb[128 + 0 * DISP_W] == green_argb);

    /* ---------- 8. Window 0 masking. ------------------------------ */
    /* WIN0 covers the rectangle [16, 32) x [4, 8); inside it BG0 is
     * enabled, outside everything is disabled. */
    self_check_reset();
    pltt_write16(0, 0x0000); /* black backdrop */
    pltt_write16(1, 0x03E0); /* green */
    memset(&gPortVram[0], 0x11, 32);
    memset(&gPortVram[0x800], 0, 0x800);
    vram_write16(0x800, 0x0000);
    io_write16(IO_BG0CNT, (1 << BGCNT_SCREENBASE_SHIFT));
    io_write16(IO_WIN0H, (16 << 8) | 32); /* x in [16, 32) */
    io_write16(IO_WIN0V, (4 << 8) | 8);   /* y in [4, 8) */
    io_write16(IO_WININ, 0x0001);         /* inside WIN0: only BG0 */
    io_write16(IO_WINOUT, 0x0000);        /* outside: nothing */
    io_write16(IO_DISPCNT, DISPCNT_BG0_ON | DISPCNT_WIN0_ON);
    Port_RenderFrame(fb);
    /* (16, 4) is inside the window -> green.  (0, 0) is outside ->
     * backdrop (black). */
    ok = ok && (fb[16 + 4 * DISP_W] == green_argb);
    ok = ok && (fb[0 + 0 * DISP_W] == 0xFF000000u);
    ok = ok && (fb[31 + 7 * DISP_W] == green_argb);  /* still inside */
    ok = ok && (fb[32 + 4 * DISP_W] == 0xFF000000u); /* just outside */

    /* ---------- 9. Alpha blend (mode 1).  BG0 over backdrop. ------ */
    /* BG0 is 1st target with green; backdrop is 2nd target with red.
     * EVA = 8, EVB = 8 -> per-channel `(top*8 + bot*8) >> 4` =
     * (top + bot) / 2. */
    self_check_reset();
    pltt_write16(0, 0x001F); /* red backdrop */
    pltt_write16(1, 0x03E0); /* green BG */
    memset(&gPortVram[0], 0x11, 32);
    memset(&gPortVram[0x800], 0, 0x800);
    vram_write16(0x800, 0x0000);
    io_write16(IO_BG0CNT, (1 << BGCNT_SCREENBASE_SHIFT));
    io_write16(IO_BLDCNT, 0x0001 | (0x20 << 8) | (1 << 6)); /* 1st=BG0, 2nd=BD, mode=1 */
    io_write16(IO_BLDALPHA, (8 << 8) | 8);
    io_write16(IO_DISPCNT, DISPCNT_BG0_ON);
    Port_RenderFrame(fb);
    uint16_t expect_blend = blend_alpha(0x03E0, 0x001F, 8, 8);
    ok = ok && (fb[0] == bgr555_to_argb8888(expect_blend));

    /* ---------- 10. Brightness fade (mode 2 / mode 3, BLDY). ------ */
    /* Mode 2 (brighter), EVY = 16 -> top fades to white.
     * Mode 3 (darker), EVY = 16 -> top fades to black. */
    io_write16(IO_BLDCNT, 0x0001 | (2 << 6)); /* 1st=BG0, mode=2 brighter */
    io_write16(IO_BLDY, 16);
    Port_RenderFrame(fb);
    ok = ok && (fb[0] == 0xFFFFFFFFu);
    io_write16(IO_BLDCNT, 0x0001 | (3 << 6)); /* mode=3 darker */
    Port_RenderFrame(fb);
    ok = ok && (fb[0] == 0xFF000000u);

    /* ---------- 11. Mosaic (BG horizontal). ----------------------- */
    /* BG mosaic 4 horizontally: pixels in columns [0..3] all sample
     * the value at x = 0; columns [8..11] sample x = 8.  Set tile 0
     * to a column-varying pattern by writing distinct palette indices
     * per pixel of row 0 of tile 0. */
    self_check_reset();
    pltt_write16(0, 0x0000);
    pltt_write16(1, 0x001F); /* red */
    pltt_write16(2, 0x03E0); /* green */
    /* Tile 0 row 0 (4 bpp): bytes 0..3 = (idx 1, idx 1), (idx 1, idx 2),
     * (idx 2, idx 1), (idx 2, idx 2).  -> px(0..7) = 1,1,1,2,2,1,2,2.
     * Easier: pixel x within row 0 = (x & 4) ? 2 : 1, so we expect a
     * step from idx 1 to idx 2 at x=4. */
    memset(&gPortVram[0], 0, 32);
    /* row 0: bytes 0..3.  pix(0)=low(b0), pix(1)=high(b0), pix(2)=low(b1)... */
    gPortVram[0] = 0x11; /* pix 0,1 = idx 1 */
    gPortVram[1] = 0x11; /* pix 2,3 = idx 1 */
    gPortVram[2] = 0x22; /* pix 4,5 = idx 2 */
    gPortVram[3] = 0x22; /* pix 6,7 = idx 2 */
    /* Make remaining rows match row 0 so nothing else interferes. */
    for (int r = 1; r < 8; ++r) {
        gPortVram[r * 4 + 0] = 0x11;
        gPortVram[r * 4 + 1] = 0x11;
        gPortVram[r * 4 + 2] = 0x22;
        gPortVram[r * 4 + 3] = 0x22;
    }
    memset(&gPortVram[0x800], 0, 0x800);
    vram_write16(0x800, 0x0000);
    io_write16(IO_BG0CNT, (1 << BGCNT_SCREENBASE_SHIFT) | BGCNT_MOSAIC);
    io_write16(IO_MOSAIC, (4 - 1)); /* H=4, V=1 (BG); OBJ 1x1 */
    io_write16(IO_BLDCNT, 0);
    io_write16(IO_DISPCNT, DISPCNT_BG0_ON);
    Port_RenderFrame(fb);
    /* Without mosaic px(3) = idx 1, px(4) = idx 2.  With H=4 mosaic,
     * px(3) is anchored to px(0) = idx 1, and px(4) anchors to px(4)
     * itself = idx 2. */
    uint32_t color_idx1 = bgr555_to_argb8888(0x001F);
    uint32_t color_idx2 = bgr555_to_argb8888(0x03E0);
    ok = ok && (fb[3] == color_idx1);
    ok = ok && (fb[4] == color_idx2);
    /* x=5 anchors to x=4 -> idx 2 (without mosaic it would be idx 1). */
    ok = ok && (fb[5] == color_idx2);

    /* ---------- 12. OBJ semi-transparent forces alpha blend. ------ */
    /* BG0 = green (2nd target), OBJ = white in mode-1 (semi).  Even
     * though BLDCNT mode is 0, the OBJ semi-transparent attribute
     * forces an alpha blend using EVA / EVB. */
    self_check_reset();
    pltt_write16(0, 0x0000);
    pltt_write16(1, 0x03E0);       /* BG green */
    pltt_write16(256 + 1, 0x7FFF); /* OBJ white */
    memset(&gPortVram[VRAM_OBJ_BASE], 0x11, 32);
    /* OBJ 0: 8x8 at (0,0), mode = 1 (semi-transparent). */
    oam_write16(0, 0x0400); /* mode bits = 1 */
    oam_write16(2, 0x0000);
    oam_write16(4, 0x0000);
    /* BG0 covers the area in green. */
    memset(&gPortVram[0], 0x11, 32);
    memset(&gPortVram[0x800], 0, 0x800);
    vram_write16(0x800, 0x0000);
    io_write16(IO_BG0CNT, (1 << BGCNT_SCREENBASE_SHIFT));
    io_write16(IO_BLDCNT, (0x01 << 8)); /* 2nd target = BG0 only; mode=0 */
    io_write16(IO_BLDALPHA, (8 << 8) | 8);
    io_write16(IO_DISPCNT, DISPCNT_BG0_ON | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    Port_RenderFrame(fb);
    uint16_t expect_obj_blend = blend_alpha(0x7FFF, 0x03E0, 8, 8);
    ok = ok && (fb[0] == bgr555_to_argb8888(expect_obj_blend));

    /* ---------- 13. Affine OBJ (identity transform). -------------- */
    /* An affine sprite with PA=PD=0x100, PB=PC=0 should render
     * identically to a regular sprite of the same size and tile. */
    self_check_reset();
    pltt_write16(0, 0x0000);
    pltt_write16(256 + 1, 0x7C00); /* OBJ blue */
    memset(&gPortVram[VRAM_OBJ_BASE], 0x11, 32);
    /* Affine group 0: PA=0x100, PB=0, PC=0, PD=0x100. */
    oam_write16(6, 0x0100);
    oam_write16(14, 0x0000);
    oam_write16(22, 0x0000);
    oam_write16(30, 0x0100);
    /* OBJ 0: 8x8 affine sprite at (0,0), group 0, no double-size. */
    oam_write16(0, OBJ_ATTR0_AFFINE); /* affine, mode=0, 4bpp, shape=0 */
    oam_write16(2, 0x0000);           /* x=0, group 0, size 0 */
    oam_write16(4, 0x0000);           /* tile 0, prio 0 */
    io_write16(IO_BLDCNT, 0);
    io_write16(IO_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    Port_RenderFrame(fb);
    uint32_t blue2 = bgr555_to_argb8888(0x7C00);
    ok = ok && (fb[0] == blue2);
    ok = ok && (fb[7 + 7 * DISP_W] == blue2);
    ok = ok && (fb[8 + 0 * DISP_W] == 0xFF000000u); /* outside sprite -> backdrop */

    snapshot_restore(&snap);
    return ok ? 0 : 1;
}
