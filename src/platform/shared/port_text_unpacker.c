/**
 * @file port_text_unpacker.c
 * @brief Host-C port of the three text-glyph nibble helpers from
 *        `asm/src/code_08001A7C.s` (UnpackTextNibbles, sub_080026C4,
 *        sub_080026F2).
 *
 * These three THUMB routines are the leaf primitives that every text
 * draw path in the game funnels through:
 *
 *   - `UnpackTextNibbles(src, dst)`
 *       Splits 64 packed bytes (read as 16 unrolled iterations of 4
 *       bytes each) into 128 nibbles. For each input byte the low
 *       nibble is written first, then the high nibble. Output is the
 *       column-major nibble buffer the glyph blitters consume.
 *
 *   - `sub_080026C4(src_nibbles, dst_tile, color_lookup, col)`
 *       Blits one 1-pixel-wide, 16-pixel-tall column of nibbles into a
 *       4bpp GBA-tile-format destination with a 16-entry palette remap.
 *       `col` is a pixel x-coordinate; the function picks the right
 *       8x16 tile-strip (`(col >> 3) << 6`), the right byte within each
 *       tile row (`(col & 6) >> 1`) and the right nibble within that
 *       byte (`col & 1`). When writing the high nibble it switches the
 *       lookup to the second half of `color_lookup` (which holds the
 *       same colours pre-shifted into the upper nibble) and the dest
 *       mask to `0x0F`. The src nibble buffer is read with stride 8
 *       (one column out of the 8 produced by `UnpackTextNibbles`) and
 *       the dest is advanced by 4 bytes per row (= 4bpp tile row size).
 *
 *   - `sub_080026F2(...)`
 *       Same as `sub_080026C4` but skips pixels whose looked-up colour
 *       is 0; that's how glyphs preserve the background under
 *       transparent foreground pixels.
 *
 * Without these the host build aborts the moment any text needs to be
 * drawn (see backtrace in PR description: file-select text trips the
 * trap stub and `Port_AsmStubTrap` calls `abort()`). The save-select
 * "missing foreground elements" the user reported are the file-select
 * labels, which also funnel through these primitives.
 *
 * The C ports below are direct, behaviour-preserving translations of
 * the THUMB sequences in `asm/src/code_08001A7C.s` (lines 875-964).
 * They are kept in their own translation unit so the remaining stubs
 * in `asm_stubs.c` continue to mechanically map 1:1 to the unported
 * `.s` files.
 */
#include "global.h"

void UnpackTextNibbles(void* src, u8* dst);
void sub_080026C4(u8* src_nibbles, u8* dst_tile, u8* color_lookup, u32 col);
void sub_080026F2(u8* src_nibbles, void* dst_tile, u8* color_lookup, u32 col);

void UnpackTextNibbles(void* src, u8* dst) {
    const u8* in = (const u8*)src;
    /* The original asm unrolls 4 bytes per loop iteration over 16
     * iterations (= 64 packed input bytes -> 128 nibbles). Keep the
     * unroll so the generated code matches the asm intent and any
     * future bisection against the GBA build is easy to read. */
    u32 i;
    for (i = 0; i < 16; i++) {
        u8 b0 = in[0];
        u8 b1 = in[1];
        u8 b2 = in[2];
        u8 b3 = in[3];
        dst[0] = (u8)(b0 & 0xF);
        dst[1] = (u8)(b0 >> 4);
        dst[2] = (u8)(b1 & 0xF);
        dst[3] = (u8)(b1 >> 4);
        dst[4] = (u8)(b2 & 0xF);
        dst[5] = (u8)(b2 >> 4);
        dst[6] = (u8)(b3 & 0xF);
        dst[7] = (u8)(b3 >> 4);
        in += 4;
        dst += 8;
    }
}

void sub_080026C4(u8* src_nibbles, u8* dst_tile, u8* color_lookup, u32 col) {
    /* Pick the 8x16 tile-strip and the byte within each tile row. */
    u8* dst = dst_tile + ((col >> 3) << 6) + ((col & 6) >> 1);
    u8 mask = 0xF0;                         /* keep high nibble by default */
    const u8* colors = color_lookup;        /* low-nibble half of LUT */
    u32 i;
    if (col & 1) {
        mask = 0x0F;                        /* keep low nibble */
        colors = color_lookup + 0x10;       /* high-nibble half of LUT */
    }
    for (i = 0; i < 16; i++) {
        u8 nibble = *src_nibbles;
        u8 px = colors[nibble];
        *dst = (u8)((*dst & mask) | px);
        dst += 4;
        src_nibbles += 8;
    }
}

void sub_080026F2(u8* src_nibbles, void* dst_tile, u8* color_lookup, u32 col) {
    u8* dst_base = (u8*)dst_tile;
    u8* dst = dst_base + ((col >> 3) << 6) + ((col & 6) >> 1);
    u8 mask = 0xF0;
    const u8* colors = color_lookup;
    u32 i;
    if (col & 1) {
        mask = 0x0F;
        colors = color_lookup + 0x10;
    }
    for (i = 0; i < 16; i++) {
        u8 nibble = *src_nibbles;
        u8 px = colors[nibble];
        if (px != 0) {
            *dst = (u8)((*dst & mask) | px);
        }
        dst += 4;
        src_nibbles += 8;
    }
}
