/**
 * @file port_oam_renderer.c
 * @brief Host-side C port of the single-shot OAM emission helpers
 *        (`arm_DrawDirect` / `arm_sub_080ADA04` / `sub_080B2874`) from
 *        `asm/src/intr.s`.
 *
 * PR A of the title-screen foreground OAM-pipeline plan
 * (see docs/sdl_port.md, "Roadmap 2b.4b runtime flip" notes; the prior
 * silent stubs lived in `ram_silent_stubs.c::ram_DrawDirect` /
 * `ram_sub_080ADA04` and made every `affine.c::DrawDirect()` call a
 * no-op). With these stubs in place, every sprite emitted by a direct
 * (non-entity-loop) caller -- most visibly the title-screen "PRESS
 * START" prompt and the "©" copyright sprite at the bottom of the
 * screen -- never made it into `gOAMControls.oam`, so VBlank's
 * `DmaCopy32` of the OAM buffer to `gPortOam` shipped zeros and the
 * renderer drew nothing. Sword BG2 was unaffected because it is an
 * affine BG layer, not an OAM sprite.
 *
 * What this TU implements
 * -----------------------
 * Two strong (non-weak) symbols that replace the silent stubs:
 *
 *   - `ram_DrawDirect(OAMCommand* cmd, u32 spriteIndex, u32 frameIndex)`
 *     Looks up `gFrameObjLists[spriteIndex]` (a relative byte offset
 *     into the `gFrameObjLists` blob), indexes the resulting per-sprite
 *     frame table by `frameIndex`, and dispatches to the inner
 *     compositor. `frameIndex == 0xFF` is the documented "no draw"
 *     sentinel used by callers like `pauseMenu.c`.
 *
 *   - `ram_sub_080ADA04(OAMCommand* cmd, void* framePtr)`
 *     The compositor proper. Walks the 5-byte-per-sprite array starting
 *     at `framePtr+1` (`framePtr[0]` is the per-frame sprite count),
 *     applies horizontal/vertical flip from the cmd's extended attrs
 *     for non-affine sprites, runs the standard GBA OBJ-bounding-box
 *     clipping, packs and writes one `OamData` (`attr0|attr1` as a u32,
 *     then `attr2` as a u16) per surviving sprite into `gOAMControls.oam`
 *     at the `gOAMControls.updated` cursor, and updates the cursor.
 *     Mirrors the ARM loop body byte-for-byte modulo:
 *
 *       * the `ram_0x80b2be8` size-table lookup is replaced with a
 *         synthesised value computed inline from the sprite's
 *         shape/size (the table on real hardware is just the standard
 *         GBA OBJ width/height plus an affine-double-size centring
 *         offset; see kObjW / kObjH below). Until the actual IWRAM
 *         table is extracted from `baserom.gba`, computing it inline
 *         is both smaller and more transparent than committing a
 *         transcribed copy.
 *
 *       * the longjmp-via-`gOAMControls._0[0x14/0x18]` "OAM full" exit
 *         is replaced with an early `return` -- nothing in the host
 *         build relies on those scratch slots being populated (they
 *         existed only to let the ARM body unwind past
 *         `arm_sub_080ADA04`'s caller frame).
 *
 * Coverage / scope
 * ----------------
 * This is intentionally *just* the single-shot path. The matching
 * per-frame entity OAM compositor (`arm_DrawEntities` and friends)
 * remains a silent no-op in `ram_silent_stubs.c`; that lands in
 * follow-up PR B. Consequently the Zelda logo and the chain sprites
 * (which are spawned via `CreateObject(TITLE_SCREEN_OBJECT, ...)` and
 * walked by the entity loop) are still missing after this PR.
 *
 * The matching ROM build never sees this TU (it is `__PORT__`-only via
 * `TMC_PLATFORM_SHARED_SOURCES`), so no `.gba` output changes.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* The platform headers pull in the same `OAMCommand` / `gOAMControls`
 * declarations the game source uses. We keep this TU header-light so
 * the link-only smoke build can include it without dragging in the
 * full game include graph. */

/* Mirrors `OAMCommand` in include/affine.h; we redeclare locally to
 * avoid pulling in the entire game include graph for this leaf TU. */
typedef struct {
    int16_t x;        /* offset 0 */
    int16_t y;        /* offset 2 */
    uint16_t _4;      /* offset 4 */
    uint16_t _6;      /* offset 6 */
    uint16_t _8;      /* offset 8 */
} PortOamCommand;

/* `gOAMControls` is currently a weak `char[0x920]` from
 * `port_unresolved_stubs.c` and is also referenced by
 * `src/interrupts.c` through the real `OAMControls` struct in
 * `include/vram.h`. Both views agree on the byte layout:
 *   offset 0x03   -> `updated` (u8 cursor into `oam[]`)
 *   offset 0x20   -> `oam[0]` (each entry 8 bytes; 128 total) */
extern uint8_t gOAMControls[];

/* `gFrameObjLists` is the relative-offset blob loaded from
 * `data/gfx/gFrameObjLists.bin` (see `assets/assets.json`). Each
 * `u32` slot stores a *byte* offset into the same blob; resolving
 * it gives a pointer either to a per-frame table (for the
 * `gFrameObjLists[spriteIndex]` step) or to the raw frame data (for
 * the per-frame inner step). */
extern uint32_t gFrameObjLists[];

/* Standard GBA OBJ pixel sizes, indexed [shape][size]. This is the
 * data the ROM's `ram_0x80b2be8` table encodes once per (affine, double)
 * mode. */
static const uint8_t kObjW[3][4] = {
    { 8, 16, 32, 64 }, /* shape 0: square         */
    { 16, 32, 32, 64 }, /* shape 1: wide           */
    { 8,  8, 16, 32 }, /* shape 2: tall           */
};
static const uint8_t kObjH[3][4] = {
    { 8, 16, 32, 64 }, /* shape 0: square         */
    { 8,  8, 16, 32 }, /* shape 1: wide           */
    { 16, 32, 32, 64 }, /* shape 2: tall           */
};

/* The compositor body. `framePtr[0]` is the per-frame sprite count;
 * each subsequent 5-byte block is one OBJ (x_off, y_off, packed
 * shape/size/flip/palette-override flags, attr2 lo, attr2 hi). */
void ram_sub_080ADA04(PortOamCommand* cmd, const void* framePtr) {
    if (framePtr == NULL || cmd == NULL) {
        return;
    }
    const uint8_t* frame = (const uint8_t*)framePtr;
    uint8_t count = frame[0];
    if (count == 0) {
        return;
    }

    uint8_t updated = gOAMControls[3];
    if (updated >= 0x80) {
        return;
    }

    const int16_t cx = cmd->x;
    const int16_t cy = cmd->y;
    /* The ARM `ldr r8, [r0, #4]` reads a 32-bit extended-attribute
     * word straddling `_4` and `_6`; reconstruct it portably. */
    const uint32_t ext = (uint32_t)cmd->_4 | ((uint32_t)cmd->_6 << 16);
    const uint16_t pal_tile = cmd->_8;
    const uint32_t aff_bits = ext & 0x300u;

    const uint8_t* sl = frame + 1;
    uint8_t* oam = &gOAMControls[0x20] + (size_t)updated * 8u;

    while (count != 0) {
        --count;

        int32_t s_x = (int8_t)sl[0];
        int32_t s_y = (int8_t)sl[1];
        const uint8_t r3 = sl[2];
        const uint8_t tile_lo = sl[3];
        const uint8_t pal_hi = sl[4];
        sl += 5;

        /* H/V flip is applied to the per-sprite offset for non-affine
         * OBJs only; affine sprites carry rotation in the attrs. */
        if (aff_bits == 0) {
            if (ext & 0x20000000u) {
                s_y = -s_y;
            }
            if (ext & 0x10000000u) {
                s_x = -s_x;
            }
        }

        /* Bounding-box adjustment: affine double-size puts the OAM
         * x/y at the top-left of the *2x* bounding box, so subtract
         * one sprite-size to keep the visible sprite centred on the
         * caller-supplied (x, y). Non-affine and affine non-double
         * keep OAM x/y at the sprite top-left (no adjustment). */
        const uint8_t r3_shape = (r3 >> 6) & 3u;
        const uint8_t r3_size = (r3 >> 4) & 3u;
        const uint8_t w = kObjW[r3_shape][r3_size];
        const uint8_t h = kObjH[r3_shape][r3_size];
        uint8_t x_off_adj, y_off_adj, x_box, y_box;
        if (aff_bits == 0u || aff_bits == 0x100u) {
            x_off_adj = 0;
            y_off_adj = 0;
            x_box = w;
            y_box = h;
        } else {
            x_off_adj = w;
            y_off_adj = h;
            x_box = (uint8_t)(w * 2u);
            y_box = (uint8_t)(h * 2u);
        }

        const int32_t sy = (int32_t)cy + s_y - (int32_t)y_off_adj;
        const int32_t sx = (int32_t)cx + s_x - (int32_t)x_off_adj;

        /* Standard GBA OBJ visibility test: the screen is 240x160
         * and OAM y/x wrap modulo 256/512 respectively. Drop sprites
         * whose bounding box is entirely off any of the four edges
         * to match the ARM clip path. */
        if (sy >= 160) {
            continue;
        }
        if (sy + (int32_t)y_box <= 0) {
            continue;
        }
        if (sx >= 240) {
            continue;
        }
        if (sx + (int32_t)x_box <= 0) {
            continue;
        }

        /* attr0 = (sy & 0xff) | (cmd_ext low 16) | (sprite_shape<<14)
         * attr1 = (sx & 0x1ff) | (cmd_ext high 16) ^ (sprite size/flip
         *         override XOR'd in via r3 bits 2-5 → attr1 bits 12-15)
         * Pack both 16-bit attrs into a single store, matching the ARM
         * `str r0, [ip], #4`. */
        uint32_t attr01 = (uint32_t)(sy & 0xff);
        attr01 |= ((uint32_t)(sx & 0x1ff)) << 16;
        attr01 |= ext;
        attr01 |= ((uint32_t)(r3 & 0xc0u)) << 8;
        attr01 ^= ((uint32_t)(r3 & 0x3cu)) << 26;

        /* attr2 = pal_tile_base + tile_lo, optionally with the
         * caller-supplied palette nibble cleared (when r3 bit 0 set,
         * the per-sprite palette override in pal_hi wins). Then add
         * pal_hi at byte 1 (priority + palette lands in attr2 bits
         * 8-15). The ARM `strh` truncates to 16 bits. */
        uint32_t attr2 = (uint32_t)tile_lo + (uint32_t)pal_tile;
        if (r3 & 1u) {
            attr2 &= ~(uint32_t)0xf000u;
        }
        attr2 += (uint32_t)pal_hi << 8;

        /* The OAM buffer is u16-aligned but not necessarily u32-
         * aligned on the host (it is a `char[]` weak placeholder and
         * `oam` may be at any 8-byte stride). `memcpy` keeps this
         * strict-aliasing-clean. */
        memcpy(oam, &attr01, 4);
        const uint16_t a2 = (uint16_t)attr2;
        memcpy(oam + 4, &a2, 2);
        oam += 8;
        ++updated;

        if (updated >= 0x80) {
            /* OAM full: matches the ARM longjmp exit. The host build
             * has no frame to unwind past, so a plain return suffices.
             */
            gOAMControls[3] = 0x80;
            return;
        }
    }

    gOAMControls[3] = updated;
}

void ram_DrawDirect(PortOamCommand* cmd, uint32_t spriteIndex, uint32_t frameIndex) {
    /* The 0xFF sentinel means "no draw" -- e.g. `pauseMenu.c` uses it
     * when a slot is intentionally empty. The ROM `arm_DrawDirect`
     * checks this and returns immediately. */
    if (frameIndex == 0xff) {
        return;
    }

    /* Two-level relative-offset deref into the `gFrameObjLists` blob:
     *   per_sprite_table = gFrameObjLists_base + gFrameObjLists[spriteIndex]
     *   frame_data       = gFrameObjLists_base + per_sprite_table[frameIndex]
     * Both inner offsets are stored as u32 byte counts relative to
     * `&gFrameObjLists[0]`. Use byte-pointer arithmetic + memcpy so
     * the (potentially u32-misaligned) inner read stays defined. */
    uint8_t* base = (uint8_t*)gFrameObjLists;
    uint32_t per_sprite_off;
    memcpy(&per_sprite_off, base + (size_t)spriteIndex * 4u, sizeof(per_sprite_off));
    uint32_t frame_off;
    memcpy(&frame_off, base + per_sprite_off + (size_t)frameIndex * 4u, sizeof(frame_off));

    const uint8_t* framePtr = base + frame_off;
    ram_sub_080ADA04(cmd, framePtr);
}
