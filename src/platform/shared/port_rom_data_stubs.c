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
 * strong definitions here can be removed. The struct types
 * (`GfxItem`, `PaletteGroup`) are pulled from
 * `port_rom_data_types.h`, which is the single source of truth shared
 * with `src/common.c`'s extern declarations. The array element types
 * here therefore match the public `extern const GfxItem* gGfxGroups[]`
 * / `extern const PaletteGroup* gPaletteGroups[]` signatures exactly
 * (same struct tag, same qualifier list on the array elements), so
 * the definitions are type-compatible across translation units even
 * under LTO / whole-program checkers.
 */

#ifdef __PORT__

#include <stdint.h>
#include <stddef.h>

#include "port_rom_data_types.h"

/* Shared terminator. `unk3 == 0x0D` is the LoadGfxGroup early-return
 * code; `dest` and `unk8` are unused on that path. */
static const GfxItem sPortGfxGroupTerminator = {
    /* unk0.bytes.unk3 = 0x0D, low three bytes irrelevant. */
    .unk0 = { .bytes = { { 0, 0, 0 }, 0x0D } },
    .dest = 0,
    .unk8 = 0,
};

/* Helper macros to spell out 133 identical pointer initializers
 * portably (no GNU `[lo ... hi] =` designated-range extension).
 * 128 + 4 + 1 = 133 entries. */
#define PORT_TERM_8                                                                                         \
    &sPortGfxGroupTerminator, &sPortGfxGroupTerminator, &sPortGfxGroupTerminator, &sPortGfxGroupTerminator, \
        &sPortGfxGroupTerminator, &sPortGfxGroupTerminator, &sPortGfxGroupTerminator, &sPortGfxGroupTerminator
#define PORT_TERM_16 PORT_TERM_8, PORT_TERM_8
#define PORT_TERM_128 \
    PORT_TERM_16, PORT_TERM_16, PORT_TERM_16, PORT_TERM_16, PORT_TERM_16, PORT_TERM_16, PORT_TERM_16, PORT_TERM_16

/* Strong host definition (replaces the weak BSS stub in
 * port_unresolved_stubs.c). The size matches the ROM table:
 * `gGfxGroups[]` lists `gGfxGroup_0` (a deliberate NULL slot in the
 * ROM) plus groups 1..132, so 133 pointer slots in total. Unlike the
 * ROM table, this host stub initializes every slot, including index
 * 0, to the shared terminator entry -- the early-boot code paths
 * exercised by the SDL host never index slot 0, so leaving it
 * non-NULL is harmless and keeps the initializer uniform. The
 * element type (`const GfxItem*`, no extra trailing `const`) matches
 * `extern const GfxItem* gGfxGroups[]` in `src/common.c` exactly. */
const GfxItem* gGfxGroups[133] = {
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
 * always zero. This host definition is `const`, matching the public
 * declaration `const u8 gGlobalGfxAndPalettes[]`. Declarations and
 * definitions for this symbol must agree in type and qualifiers
 * across translation units; writing into this region is never legal
 * under either the ROM or the host contract anyway. */
const uint8_t gGlobalGfxAndPalettes[4096];

/* ----------------------------------------------------------------------- *
 * gPaletteGroups[] -- analogue of gGfxGroups[] for `LoadPaletteGroup`.
 *
 * Uses the shared `PaletteGroup` typedef from `port_rom_data_types.h`,
 * which mirrors the file-local typedef in `src/common.c`:
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

static const PaletteGroup sPortPaletteGroupTerminator = {
    .paletteId = 0, .destPaletteNum = 0, .numPalettes = 1, /* high bit clear -> loop exits after first iteration */
};

/* gPaletteGroups[] in `data/gfx/palette_groups.s` is 208 pointer slots
 * for the USA build (index 0 + groups 1..207); the EU build drops
 * group 207 to 207 slots. Sizing for the larger of the two is harmless
 * for the smaller build because no in-range index is ever NULL.
 * Element type matches `extern const PaletteGroup* gPaletteGroups[]`
 * in `src/common.c` exactly (no extra trailing `const`). */
#define PORT_PALG_8                                                                               \
    &sPortPaletteGroupTerminator, &sPortPaletteGroupTerminator, &sPortPaletteGroupTerminator,     \
        &sPortPaletteGroupTerminator, &sPortPaletteGroupTerminator, &sPortPaletteGroupTerminator, \
        &sPortPaletteGroupTerminator, &sPortPaletteGroupTerminator
#define PORT_PALG_16 PORT_PALG_8, PORT_PALG_8
#define PORT_PALG_64 PORT_PALG_16, PORT_PALG_16, PORT_PALG_16, PORT_PALG_16
#define PORT_PALG_192 PORT_PALG_64, PORT_PALG_64, PORT_PALG_64

const PaletteGroup* gPaletteGroups[208] = {
    PORT_PALG_192, PORT_PALG_8, PORT_PALG_8, /* 16  -> 208 */
};

/* ----------------------------------------------------------------------- *
 * gUIElementDefinitions[] -- per-UI-element-type dispatch table.
 *
 * The real ROM table is defined in the still-unported `src/ui_data.c`
 * (and lives in unported asm/data on disk). Each entry's
 * `updateFunction` is invoked for every "used" UI element on every
 * frame by `UpdateUIElements()`. The default 256-byte zero-filled
 * weak BSS placeholder in `port_unresolved_stubs.c` therefore
 * NULL-derefs the first time the file-select task creates a UI element
 * (`HandleFileScreenEnter -> sub_080A70AC` calls `CreateUIElement`
 * twice with `UIElementType` 5 and 2; on the next frame
 * `UpdateUIElements` calls `gUIElementDefinitions[type].updateFunction`
 * which is NULL, crashing the file-select screen).
 *
 * For the SDL host build we provide a strong table whose
 * `updateFunction` slot is a no-op (`Port_UIElementUpdateNoOp`). The
 * other fields stay zero, which is fine for the host: `unk_4` (read
 * into `element->unk_1a` in `CreateUIElement`) and `buttonElementId`
 * being zero is a safe default; `DrawUIElements` only fires when
 * `element->unk_0_1 == 1`, which is set by helpers (e.g.
 * `sub_0801CB20`) that index `gSpritePtrs` -- gated separately by the
 * port's sprite-data stubs and not exercised by the file-select
 * task's two type=5/type=2 elements.
 *
 * `UIElementDefinition` is a file-local typedef in `src/ui.c`; the
 * declaration there is `extern UIElementDefinition gUIElementDefinitions[]`.
 * Storage layout/size is irrelevant to the linker (the symbol is
 * looked up by name only), but the **stride** on indexed access must
 * match what `src/ui.c` compiled against. We mirror the exact field
 * layout from `src/ui.c` here so a host-side `sizeof(PortUIElementDefinition)`
 * matches what `src/ui.c` sees as `sizeof(UIElementDefinition)` on the
 * same host. UIElementType currently has 11 values (0..10); 16 entries
 * provides headroom. */

/* Forward declaration of the engine's UIElement struct so we can spell
 * the function-pointer signature without pulling in the full ui.h
 * dependency chain. The field layout is the engine's; we only need the
 * pointer type here. */
struct UIElement;

typedef struct {
    uint16_t unk_0;
    uint16_t unk_2;
    uint16_t unk_4;
    uint16_t spriteIndex;
    void (*updateFunction)(struct UIElement*);
    uint8_t buttonElementId;
    uint8_t unk_d;
    uint8_t unk_e;
    uint8_t unk_f;
} PortUIElementDefinition;

static void Port_UIElementUpdateNoOp(struct UIElement* element) {
    (void)element;
    /* The real per-type update functions advance frame timers, swap
     * sprite frames, etc. With no real sprite/animation data wired in
     * yet, doing nothing is the safe behaviour for the SDL port: the
     * UI element stays in its initial state and does not trigger any
     * downstream NULL-deref through gSpritePtrs / gFrameObjLists. */
}

#define PORT_UIDEF_NOOP                                                                                       \
    { .unk_0 = 0, .unk_2 = 0, .unk_4 = 0, .spriteIndex = 0, .updateFunction = Port_UIElementUpdateNoOp,       \
      .buttonElementId = 0, .unk_d = 0, .unk_e = 0, .unk_f = 0 }

PortUIElementDefinition gUIElementDefinitions[16] = {
    PORT_UIDEF_NOOP, PORT_UIDEF_NOOP, PORT_UIDEF_NOOP, PORT_UIDEF_NOOP,
    PORT_UIDEF_NOOP, PORT_UIDEF_NOOP, PORT_UIDEF_NOOP, PORT_UIDEF_NOOP,
    PORT_UIDEF_NOOP, PORT_UIDEF_NOOP, PORT_UIDEF_NOOP, PORT_UIDEF_NOOP,
    PORT_UIDEF_NOOP, PORT_UIDEF_NOOP, PORT_UIDEF_NOOP, PORT_UIDEF_NOOP,
};

#endif /* __PORT__ */
