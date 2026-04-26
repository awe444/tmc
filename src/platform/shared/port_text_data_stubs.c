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

/* ------------------------------------------------------------------
 * gTranslations — per-language string-table pointer table.
 *
 * Background
 * ----------
 * The ROM build defines `gTranslations[]` in `data/const/text.s` as a
 * 7-entry table of `u32*` pointers, each addressing a packed
 * per-language string table (`translation`, `translationFrench`, ...).
 * The packed table layout consumed by `sub_0805EEB4` in `src/text.c` is:
 *
 *   u32 hiCount;                    // ((textIndex >> 8) bound) << 2
 *   u32 hiOffsets[hiCount];         // byte offset (from table start) to a
 *                                   // sub-table for each high-byte page
 *   per-page sub-table:
 *       u32 loCount;                // ((textIndex & 0xff) bound) << 2
 *       u32 loOffsets[loCount];     // byte offset (from sub-table start)
 *                                   // to the packed string bytes
 *
 * `tools/port/gen_host_assets.py` does not extract these tables, so
 * without an override `gTranslations[]` resolves through the 256-byte
 * zero-filled weak BSS placeholder in `port_unresolved_stubs.c`. Each
 * entry then reads as NULL, and the very first text-box draw — e.g.
 * `CreateDialogBox(5, 0)` after the file-select keyboard "END" confirm,
 * which calls `ShowTextBox(8, ...)` -> `InitToken` -> `sub_0805EEB4`
 * — segfaults on `puVar2[(u8)(textIndex >> 8)]` with `puVar2 == NULL`.
 *
 * This is exactly what the CI smoke test (`.github/workflows/sdl.yml`)
 * traps at `f≈900`; the workflow comment attributes it to the unported
 * `sub_0805138C` save-write path, but the actual crash is upstream of
 * the save flow, in the dialog-box text lookup that `sub_080513A8`
 * issues before any save bytes are written.
 *
 * Fix
 * ---
 * Provide a strong override of `gTranslations[]` whose entries all
 * point at one shared, zero-filled "safe translation buffer". With
 * `*buf == 0`, `sub_0805EEB4` computes `uVar6 = 0`, the bounds check
 * `(textIndex >> 8) >= uVar6` is always true, and control falls through
 * to the `case 1` arm that retargets `puVar2` at the (already
 * non-NULL) `gUnk_08109244` placeholder — i.e. the "missing string"
 * fallback the original code already exercises for out-of-range indices.
 * No text pixels get drawn, but the boot path survives instead of
 * SIGSEGV-ing, matching the existing precedent set by `gUnk_08109248`
 * above and by `gGfxGroups` / `gPaletteGroups` / `gUIElementDefinitions`
 * elsewhere in the port tree.
 *
 * Sizing
 * ------
 * The pointer indexer reads at byte offset `(u8)(textIndex >> 8) * 4`
 * — i.e. up to 256 * 4 = 1 KiB — *before* any bounds check applies, so
 * the buffer must be at least 1 KiB to keep that read in-bounds.
 * 4 KiB gives generous headroom and matches the order-of-magnitude of
 * the sibling `sPortFontGlyphZeroBuffer`.
 *
 * `gTranslations` itself is sized to 16 entries (the ROM table is 7;
 * the extras are slack against any host-side `gSaveHeader->language`
 * value that wanders past the ROM range — purely defensive, since
 * `Port_RunGameLoop` zero-initialises `gSaveHeader` and `language`
 * stays in `[0, 6]` along the boot path).
 */
static uint8_t sPortTranslationZeroBuffer[0x1000] __attribute__((aligned(16)));

u32* gTranslations[16] = {
    (u32*)sPortTranslationZeroBuffer, (u32*)sPortTranslationZeroBuffer, (u32*)sPortTranslationZeroBuffer,
    (u32*)sPortTranslationZeroBuffer, (u32*)sPortTranslationZeroBuffer, (u32*)sPortTranslationZeroBuffer,
    (u32*)sPortTranslationZeroBuffer, (u32*)sPortTranslationZeroBuffer, (u32*)sPortTranslationZeroBuffer,
    (u32*)sPortTranslationZeroBuffer, (u32*)sPortTranslationZeroBuffer, (u32*)sPortTranslationZeroBuffer,
    (u32*)sPortTranslationZeroBuffer, (u32*)sPortTranslationZeroBuffer, (u32*)sPortTranslationZeroBuffer,
    (u32*)sPortTranslationZeroBuffer,
};

/* ------------------------------------------------------------------
 * gUnk_081092AC — text-box border / fill ROM-graphics pointer table.
 *
 * Background
 * ----------
 * The ROM build defines `gUnk_081092AC[]` in `data/const/text.s` as a
 * 10-entry table of `u32*` pointers, each addressing a packed 4bpp
 * border-pattern blob (`gUnk_086926A0`, `gUnk_08692780`, ...).
 * `sub_0805F918` in `src/text.c` indexes it as
 * `puVar1 = gUnk_081092AC[idx]`, then loops 3 times calling
 * `UnpackTextNibbles(puVar1, &gUnk_02036A58)` and advancing
 * `puVar1 += 0x40` each iteration — i.e. it consumes 3 * 0x40 = 0xC0
 * bytes from the entry.
 *
 * `tools/port/gen_host_assets.py` does not extract those border blobs,
 * so without an override `gUnk_081092AC[]` resolves through the
 * 256-byte zero-filled weak BSS placeholder in
 * `port_unresolved_stubs.c`. Each entry then reads as NULL, and the
 * file-select save-creation dialog SIGSEGVs at `f≈720` in the
 * `UnpackTextNibbles(src=NULL, ...)` first iteration (this is exactly
 * the next crash hit by the 120 s alternating-A/START scripted-input
 * test after the `sub_0805EEB4` pointer-truncation fix lands).
 *
 * Fix
 * ---
 * Same precedent as `gUnk_08109248` and `gTranslations` above: provide
 * a strong override whose entries all alias one shared, zero-filled
 * "safe border buffer". With every byte zero, `UnpackTextNibbles`
 * produces 128 zero nibbles per call; the downstream blitters then
 * either mask the destination nibble to background or skip the write
 * entirely (transparent path). Net effect: no border pixels are drawn,
 * but the game survives the call instead of SIGSEGV-ing.
 *
 * Sizing
 * ------
 * Worst-case access is 3 * 0x40 = 0xC0 bytes per entry; 1 KiB gives
 * generous headroom and matches the order-of-magnitude of the sibling
 * zero buffers above. The table itself is sized to 16 entries because
 * `sub_0805F918`'s `idx` argument is `font.border_type`, a 4-bit
 * bitfield (`u8 border_type : 4` in `Font` / `include/message.h`),
 * so it can legally take values 0..15 — six positions past the ROM's
 * 10 entries. Aliasing all 16 to the same buffer keeps any stray
 * out-of-range index safe rather than reading whatever lives in
 * adjacent memory.
 */
static uint8_t sPortTextBorderZeroBuffer[0x400] __attribute__((aligned(16)));

void* gUnk_081092AC[16] = {
    (void*)sPortTextBorderZeroBuffer, (void*)sPortTextBorderZeroBuffer, (void*)sPortTextBorderZeroBuffer,
    (void*)sPortTextBorderZeroBuffer, (void*)sPortTextBorderZeroBuffer, (void*)sPortTextBorderZeroBuffer,
    (void*)sPortTextBorderZeroBuffer, (void*)sPortTextBorderZeroBuffer, (void*)sPortTextBorderZeroBuffer,
    (void*)sPortTextBorderZeroBuffer, (void*)sPortTextBorderZeroBuffer, (void*)sPortTextBorderZeroBuffer,
    (void*)sPortTextBorderZeroBuffer, (void*)sPortTextBorderZeroBuffer, (void*)sPortTextBorderZeroBuffer,
    (void*)sPortTextBorderZeroBuffer,
};

/* ------------------------------------------------------------------
 * Remaining text constants still unresolved in `text.c`.
 * Keep all host fallbacks inert/zero so text rendering becomes a
 * no-op instead of dereferencing weak 256-byte placeholders.
 */
static uint8_t sPortTextMacroEmptyString[] = "";
u8* gUnk_08109230[16] = {
    sPortTextMacroEmptyString, sPortTextMacroEmptyString, sPortTextMacroEmptyString, sPortTextMacroEmptyString,
    sPortTextMacroEmptyString, sPortTextMacroEmptyString, sPortTextMacroEmptyString, sPortTextMacroEmptyString,
    sPortTextMacroEmptyString, sPortTextMacroEmptyString, sPortTextMacroEmptyString, sPortTextMacroEmptyString,
    sPortTextMacroEmptyString, sPortTextMacroEmptyString, sPortTextMacroEmptyString, sPortTextMacroEmptyString,
};

u32 gUnk_08109244 = 0;
u32 gUnk_0810926C[256];
u16 gUnk_081092D4 = 0;

typedef struct {
    u8 filler0[12][16];
} PortVStruct;

PortVStruct gUnk_0810942E[16];
u8 gUnk_081094CE[16 * 0xC0];

#endif /* __PORT__ */
