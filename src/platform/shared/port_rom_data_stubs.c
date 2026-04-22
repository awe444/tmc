/**
 * @file port_rom_data_stubs.c
 * @brief Host-side stand-ins for the ROM-resident gfx-table data symbols.
 *
 * Sub-step 2b.4b (runtime flip) of the SDL-port roadmap; see
 * docs/sdl_port.md.
 *
 * The real definitions of `gGfxGroups[]` and `gGlobalGfxAndPalettes[]`
 * live in `data/gfx/gfx_groups.s` and `data/gfx/gfx_and_palettes.s` --
 * the latter is ~12 MiB of raw `.gbapal` / `.4bpp` / `.bin` asset blobs
 * that are only available once the base ROM has been extracted by the
 * `tools/asset_processor` pipeline driven by the ROM build's `Makefile`.
 * The SDL host build cannot pull those in (and is intentionally
 * decoupled from the ROM build per `docs/sdl_port.md`'s "Coexistence
 * with the GBA ROM build" section), so the previous
 * `port_unresolved_stubs.c` only kept the link succeeding by emitting
 * 256-byte weak BSS placeholders for both symbols.
 *
 * That placeholder is fine for `gGlobalGfxAndPalettes` (no early-boot
 * code path indexes it before crashing for other reasons) but it is
 * *not* fine for `gGfxGroups[]`: `src/common.c::LoadGfxGroup(group)`
 * starts with
 *
 *     const GfxItem* gfxItem = gGfxGroups[group];
 *     u32 ctrl = gfxItem->unk0.bytes.unk3;   // <-- NULL deref
 *
 * so as soon as `TitleTask -> HandleNintendoCapcomLogos` calls
 * `LoadGfxGroup(16)` to start the Nintendo logo, AgbMain SIGSEGVs.
 *
 * This file replaces the weak stubs with **strong** host definitions:
 *
 * 1. `gGfxGroups[]` is a 133-entry array (index 0 + groups 1..132 from
 *    `data/gfx/gfx_groups.s`) where every entry points to a single
 *    shared `PortGfxItem` whose control byte is `0x0D`. `LoadGfxGroup`
 *    handles `case 0x0D: return;` as the very first dispatch arm, so
 *    the function returns immediately without ever dereferencing
 *    `gGlobalGfxAndPalettes`, calling `LZ77UnComp{Vram,Wram}`, or
 *    issuing a DMA into the (still real) emulated VRAM. The behaviour
 *    is "no graphics get loaded" -- which matches the rest of PR
 *    #2b.4b's scaffolding (silent m4a, EEPROM-only save, etc.) and
 *    keeps every BG/OBJ tile at the host's reset value (zero) so the
 *    rasterizer's cleared backdrop is what shows on screen.
 *
 * 2. `gGlobalGfxAndPalettes[]` becomes a small zero-filled buffer.
 *    The early-boot code path no longer indexes it (because of the
 *    `0x0D` short-circuit above), but a few unrelated subtasks
 *    (`backgroundAnimations.c`, `manager/holeManager.c`, ...) still
 *    take the address `&gGlobalGfxAndPalettes[offset]`. The host stub
 *    array is sized so those address-of expressions are always inside
 *    a known mapped region instead of trapping later, even though the
 *    pointed-at bytes are zero. The ROM build is unaffected (its
 *    real `data/gfx/gfx_and_palettes.s` definition is the strong
 *    one and this file is `__PORT__`-only).
 *
 * Once a real ROM-asset-loading path is added in a later PR the
 * strong definitions here can be removed; the symbol type / signature
 * exactly mirrors the public `extern` declarations in `src/common.c`,
 * `src/beanstalkSubtask.c`, etc. so no caller change is required.
 */

#ifdef __PORT__

#include <stdint.h>
#include <stddef.h>

/* Mirror of the file-local `GfxItem` struct in `src/common.c`. The C
 * type is purely compile-side: at link time only the symbol name and
 * raw byte layout matter, and that layout (4 bytes union, 4 bytes
 * dest, 4 bytes unk8 = 12 bytes) is fixed by the ROM `gfx_raw` macro.
 * Using a port-local mirror avoids leaking the type out of common.c. */
typedef struct {
    union {
        int32_t raw;
        struct {
            uint8_t filler0[3];
            uint8_t unk3;
        } bytes;
    } unk0;
    uint32_t dest;
    uint32_t unk8;
} PortGfxItem;

/* Shared terminator. `unk3 == 0x0D` is the LoadGfxGroup early-return
 * code; `dest` and `unk8` are unused on that path. */
static const PortGfxItem sPortGfxGroupTerminator = {
    /* unk0.bytes.unk3 = 0x0D, low three bytes irrelevant. */
    .unk0 = { .bytes = { { 0, 0, 0 }, 0x0D } },
    .dest = 0,
    .unk8 = 0,
};

/* Helper macros to spell out 133 identical pointer initializers
 * portably (no GNU `[lo ... hi] =` designated-range extension, so
 * MSVC stays happy). 128 + 4 + 1 = 133 entries. */
#define PORT_TERM_8                                                          \
    &sPortGfxGroupTerminator, &sPortGfxGroupTerminator,                      \
        &sPortGfxGroupTerminator, &sPortGfxGroupTerminator,                  \
        &sPortGfxGroupTerminator, &sPortGfxGroupTerminator,                  \
        &sPortGfxGroupTerminator, &sPortGfxGroupTerminator
#define PORT_TERM_16 PORT_TERM_8, PORT_TERM_8
#define PORT_TERM_128                                                        \
    PORT_TERM_16, PORT_TERM_16, PORT_TERM_16, PORT_TERM_16, PORT_TERM_16,    \
        PORT_TERM_16, PORT_TERM_16, PORT_TERM_16

/* Strong host definition (replaces the weak BSS stub in
 * port_unresolved_stubs.c). The size matches the ROM table:
 * `gGfxGroups[]` lists `gGfxGroup_0` (a deliberate NULL slot) plus
 * groups 1..132, so 133 pointer slots in total. */
const PortGfxItem* const gGfxGroups[133] = {
    PORT_TERM_128,
    &sPortGfxGroupTerminator,
    &sPortGfxGroupTerminator,
    &sPortGfxGroupTerminator,
    &sPortGfxGroupTerminator,
    &sPortGfxGroupTerminator,
};

/* Strong host definition for the asset blob. 4 KiB is large enough
 * to absorb the few stray address-of dereferences in unrelated
 * subtasks without bloating the binary; the bytes themselves are
 * always zero. The symbol is intentionally non-`const` here even
 * though the public declaration is `const u8 gGlobalGfxAndPalettes[]`
 * -- C linkage only matches the name, and writing into this region
 * is never legal under either the ROM or the host contract anyway. */
const uint8_t gGlobalGfxAndPalettes[4096];

/* ----------------------------------------------------------------------- *
 * gPaletteGroups[] -- analogue of gGfxGroups[] for `LoadPaletteGroup`.
 *
 * Mirror of the file-local PaletteGroup struct in src/common.c:
 *
 *     typedef struct {
 *         u16 paletteId;
 *         u8  destPaletteNum;
 *         u8  numPalettes;
 *     } PaletteGroup;
 *
 * Unlike LoadGfxGroup, LoadPaletteGroup has no early-return code; it
 * always invokes LoadPalettes() and then loops while the high bit of
 * `numPalettes` is set. To make the host stub a safe no-op:
 *
 *   - paletteId      = 0  -> &gGlobalGfxAndPalettes[0] is in-bounds.
 *   - destPaletteNum = 0  -> the DmaCopy32 lands at gPaletteBuffer[0].
 *   - numPalettes    = 1  -> 32-byte copy of zeros into one palette.
 *                            (high bit clear, so the loop exits.)
 *
 * Net effect: every LoadPaletteGroup(g) call zeros palette 0 once.
 * Combined with LoadGfxGroup's 0x0D short-circuit, the boot path
 * advances past `HandleNintendoCapcomLogos` without touching any of
 * the still-unported asset blobs in `data/gfx/`.
 * ----------------------------------------------------------------------- */

typedef struct {
    uint16_t paletteId;
    uint8_t destPaletteNum;
    uint8_t numPalettes;
} PortPaletteGroup;

/* Layout-compatibility guards: any future change to either the host
 * mirror or the file-local `PaletteGroup` in `src/common.c` that
 * desyncs the two will fail at compile time instead of at runtime. */
_Static_assert(sizeof(PortPaletteGroup) == 4,
               "PortPaletteGroup must match PaletteGroup (u16 + u8 + u8 = 4 bytes)");
_Static_assert(offsetof(PortPaletteGroup, paletteId) == 0,
               "PortPaletteGroup.paletteId must be at offset 0");
_Static_assert(offsetof(PortPaletteGroup, destPaletteNum) == 2,
               "PortPaletteGroup.destPaletteNum must be at offset 2");
_Static_assert(offsetof(PortPaletteGroup, numPalettes) == 3,
               "PortPaletteGroup.numPalettes must be at offset 3");

static const PortPaletteGroup sPortPaletteGroupTerminator = {
    .paletteId = 0,
    .destPaletteNum = 0,
    .numPalettes = 1, /* high bit clear -> loop exits after first iteration */
};

/* gPaletteGroups[] in `data/gfx/palette_groups.s` is 208 pointer slots
 * for the USA build (index 0 + groups 1..207); the EU build drops
 * group 207 to 207 slots. Sizing for the larger of the two is harmless
 * for the smaller build because no in-range index is ever NULL. */
#define PORT_PALG_8                                                          \
    &sPortPaletteGroupTerminator, &sPortPaletteGroupTerminator,              \
        &sPortPaletteGroupTerminator, &sPortPaletteGroupTerminator,          \
        &sPortPaletteGroupTerminator, &sPortPaletteGroupTerminator,          \
        &sPortPaletteGroupTerminator, &sPortPaletteGroupTerminator
#define PORT_PALG_16 PORT_PALG_8, PORT_PALG_8
#define PORT_PALG_64 PORT_PALG_16, PORT_PALG_16, PORT_PALG_16, PORT_PALG_16
#define PORT_PALG_192 PORT_PALG_64, PORT_PALG_64, PORT_PALG_64

const PortPaletteGroup* const gPaletteGroups[208] = {
    PORT_PALG_192,
    PORT_PALG_8, PORT_PALG_8,    /* 16  -> 208 */
};

#endif /* __PORT__ */
