/**
 * @file port_rom_data_types.h
 * @brief Shared host/ROM struct layouts for `gGfxGroups[]` and
 *        `gPaletteGroups[]`.
 *
 * `src/common.c` defines `GfxItem` and `PaletteGroup` as file-local
 * typedefs (see the typedef blocks just above its `gPaletteGroups` /
 * `gGfxGroups` extern declarations). The host stand-in TU
 * `src/platform/shared/port_rom_data_stubs.c` needs to *define* those
 * same external arrays, so it has to see a struct definition with the
 * exact same name and layout. Re-declaring file-local types in two
 * places is technically undefined behaviour even when the byte layout
 * happens to match (C requires *compatible types* across translation
 * units that name the same external object), and it trips up
 * LTO / whole-program checkers.
 *
 * To stay safe, this header provides the canonical typedefs that both
 * the stub TU and (optionally) `src/common.c` can include. The
 * `_Static_assert`s here lock the layout in -- any future drift in
 * either copy will fail at compile time rather than at runtime.
 *
 * This header is intentionally `__PORT__`-only: the ROM build keeps
 * its in-place file-local typedefs in `src/common.c` to avoid any
 * disturbance to the matching toolchain.
 */
#ifndef PORT_ROM_DATA_TYPES_H
#define PORT_ROM_DATA_TYPES_H

#ifdef __PORT__

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t paletteId;
    uint8_t destPaletteNum;
    uint8_t numPalettes;
} PaletteGroup;

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
} GfxItem;

/* Layout-compatibility guards. Any future change to these structs
 * (or to the file-local copies in `src/common.c`) that breaks size
 * or field offsets will fail at compile time instead of at runtime. */
_Static_assert(sizeof(PaletteGroup) == 4, "PaletteGroup must be u16 + u8 + u8 = 4 bytes");
_Static_assert(offsetof(PaletteGroup, paletteId) == 0, "PaletteGroup.paletteId must be at offset 0");
_Static_assert(offsetof(PaletteGroup, destPaletteNum) == 2, "PaletteGroup.destPaletteNum must be at offset 2");
_Static_assert(offsetof(PaletteGroup, numPalettes) == 3, "PaletteGroup.numPalettes must be at offset 3");

_Static_assert(sizeof(GfxItem) == 12, "GfxItem must be 4 + 4 + 4 = 12 bytes");
_Static_assert(offsetof(GfxItem, unk0) == 0, "GfxItem.unk0 must be at offset 0");
_Static_assert(offsetof(GfxItem, dest) == 4, "GfxItem.dest must be at offset 4");
_Static_assert(offsetof(GfxItem, unk8) == 8, "GfxItem.unk8 must be at offset 8");

#endif /* __PORT__ */

#endif /* PORT_ROM_DATA_TYPES_H */
