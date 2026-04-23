/**
 * @file port_text_data_stubs.c
 * @brief Strong host stub for `gUnk_08109248[]`, the font-glyph-pointer
 *        table that `src/text.c` indexes through `sub_0805F25C`.
 *
 * Background
 * ----------
 * The ROM build defines `gUnk_08109248[]` in `data/const/text.s` as a
 * 9-entry table of pointers to the per-script font-glyph blobs
 * (`gUnk_08692F60`, `gUnk_086978E0`, etc., each ~6 KiB of packed 4bpp
 * glyph rows). The host build does not extract those blobs from the
 * baserom (`tools/port/gen_host_assets.py` only emits `gGfxGroups[]` /
 * `gGlobalGfxAndPalettes[]`), so without an override `gUnk_08109248[]`
 * resolves through the 256-byte zero-filled weak BSS placeholder in
 * `port_unresolved_stubs.c`. Every entry then reads as NULL.
 *
 * That bites the file-select boot path the moment any text needs to be
 * drawn (see issue backtrace: `UnpackTextNibbles(src=0x0, …)` from
 * `sub_0805F820 -> sub_0805F25C`). `sub_0805F25C` returns
 * `gUnk_08109248[uVar1] + param_1 * 0x10`, i.e. NULL plus a small
 * offset, and `sub_0805F820` calls `UnpackTextNibbles(NULL, …)` which
 * derefs the source pointer in its first iteration.
 *
 * Fix
 * ---
 * Provide a **strong** definition of `gUnk_08109248[9]` whose entries
 * all point to one shared, zero-filled "safe glyph buffer". The buffer
 * is sized to absorb the maximum offset the indexer can compute:
 *
 *   - cases 0-4 of `sub_0805F25C`: `param_1` is masked to `u8`
 *     (`& 0xff`), so offset = `0xFF * 0x10 = 0xFF0` bytes.
 *   - cases 5-8: `param_1 <<= 1` first, so offset = `0x1FE * 0x10 =
 *     0x1FE0` bytes.
 *
 * `UnpackTextNibbles` then reads 64 (`0x40`) more bytes past that
 * offset, so the worst-case access is `0x1FE0 + 0x40 = 0x2020` bytes.
 * 16 KiB (0x4000) gives comfortable headroom.
 *
 * With every glyph byte zero, `UnpackTextNibbles` produces 128 zero
 * nibbles per glyph; the downstream blitters (`sub_080026C4` /
 * `sub_080026F2` in `port_text_unpacker.c`) then either mask the
 * destination nibble to background or skip the write entirely
 * (transparent path). Net effect: no text pixels are drawn, but the
 * game survives the call instead of SIGSEGV-ing. Replacing the zero
 * buffer with real font data is a follow-up roadmap item, separate
 * from getting the boot path past the file-select screen.
 *
 * The strong definition here overrides the weak placeholder by
 * standard linker rules; it is harmless under `TMC_LINK_GAME_SOURCES`
 * either way (with `OFF`, no caller exists; with `ON`, the override is
 * what unblocks boot). With `-DTMC_BASEROM=…` set, the generated
 * `port_rom_assets.c` does not define `gUnk_08109248`, so this file is
 * still the only strong source for that symbol.
 */

#ifdef __PORT__

#include <stdint.h>

#include "global.h"

/* 16 KiB of zeros, 16-byte aligned. Worst-case access computed in the
 * file header is `0x2020` bytes; 16 KiB (`0x4000`) provides generous
 * headroom beyond that and rounds to a power of two for clean
 * alignment. Not declared `const` because the public
 * `gUnk_08109248[]` type is `u32*` (not `const u32*`); the SDL boot
 * path only reads from it, but matching the declared mutability avoids
 * a cast that strips `const`. The buffer still ends up in `.bss`
 * because it is zero-initialized. */
static uint8_t sPortFontGlyphZeroBuffer[0x4000] __attribute__((aligned(16)));

/* Strong override of the weak `port_unresolved_stubs.c` placeholder.
 * `extern u32* gUnk_08109248[]` in `src/text.c` — i.e. each entry is a
 * pointer the indexer dereferences as if it pointed at packed glyph
 * bytes. All 9 entries alias one shared zero buffer; the indexer in
 * `sub_0805F25C` only ever picks indices 0..8 (switch on
 * `(param_1 >> 8) & 0xF`, with the default falling through to no
 * extra adjustment, leaving `uVar1 == 0`). A 9-entry array is enough
 * for the documented switch arms; we deliberately do not size to the
 * 64 entries of the BSS placeholder because no caller indexes past
 * 8 and a smaller initializer is easier to audit. */
u32* gUnk_08109248[9] = {
    (u32*)sPortFontGlyphZeroBuffer, (u32*)sPortFontGlyphZeroBuffer, (u32*)sPortFontGlyphZeroBuffer,
    (u32*)sPortFontGlyphZeroBuffer, (u32*)sPortFontGlyphZeroBuffer, (u32*)sPortFontGlyphZeroBuffer,
    (u32*)sPortFontGlyphZeroBuffer, (u32*)sPortFontGlyphZeroBuffer, (u32*)sPortFontGlyphZeroBuffer,
};

#endif /* __PORT__ */
