/* See port_tileset_residency.h. B27. */
#include "port_tileset_residency.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gba/gba.h"
#include "room.h"

#include "port_capture.h"
#include "port_gba_mem.h"
#include "cpu/mode1.h"

/* The bank the port allocates and the bank the PPU addresses are two
 * constants in two repositories, and a mismatch is a silent read of the wrong
 * memory rather than a build failure. Tie them together here, where both
 * headers are in scope. */
typedef char port_vram_agrees_with_ppu[(PORT_VRAM_SHADOW_OFFSET == MODE1_VRAM_SHADOW_OFFSET &&
                                        PORT_VRAM_TOTAL_SIZE >= MODE1_VRAM_TOTAL_SIZE)
                                           ? 1
                                           : -1];

#if VIEWPORT_TILESET_RESIDENCY

/* Hyrule Town declares three; Minish Village will declare one. */
#define RESIDENCY_MAX_SLOTS 4
/* The longest authored list is Minish Village's eight. */
#define RESIDENCY_MAX_REGIONS 8
/* Each slot governs two character-address ranges — the groups load a block
 * into each of the two BG charbase windows. */
#define RESIDENCY_RANGES_PER_SLOT 2

typedef struct {
    u32 gfxIndex;    /* which of gRoomVars.graphicsGroups tracks this slot */
    u32 shadowGroup; /* the group living in the shadow bank                */
    VirtuaPPUMode1CharRegion regions[RESIDENCY_MAX_REGIONS];
    int regionCount;
} ResidencySlot;

static ResidencySlot sSlots[RESIDENCY_MAX_SLOTS];
static int sSlotCount;

/* What is handed to the PPU: one entry per address range, all of a slot's
 * ranges sharing that slot's region list. */
static VirtuaPPUMode1CharSlot sPublished[RESIDENCY_MAX_SLOTS * RESIDENCY_RANGES_PER_SLOT];
static int sPublishedCount;

/* Which room the declared slots describe, recorded when a slot is declared
 * — that is, from inside the manager's own update, where gRoomControls is
 * settled. Publishing checks it so that leaving the area stops the offsets
 * rather than applying them to whatever room came next. It can only ever
 * suppress a publish, never discard a declaration. */
static u8 sArea = 0xff;
static u8 sRoom = 0xff;

static int residency_trace(void) {
    static int enabled = -1;
    if (enabled < 0) {
        enabled = (getenv("TMC_TILESET_TRACE") != NULL);
    }
    return enabled;
}

void Port_TilesetResidency_Reset(void) {
    sSlotCount = 0;
    sPublishedCount = 0;
    sArea = 0xff;
    sRoom = 0xff;
}

/* The slot already describing this gfx index, or a fresh one. Re-declaring
 * is the normal case, not an error: the manager swaps the resident group as
 * the camera moves, exactly as it always has, and each swap makes the pair
 * — and so which region needs which offset — the other way round. */
static ResidencySlot* residency_slot_for(u32 gfxIndex, int* firstRange) {
    int i;

    for (i = 0; i < sSlotCount; i++) {
        if (sSlots[i].gfxIndex == gfxIndex) {
            *firstRange = i * RESIDENCY_RANGES_PER_SLOT;
            return &sSlots[i];
        }
    }
    if (sSlotCount >= RESIDENCY_MAX_SLOTS) {
        return NULL;
    }
    *firstRange = sSlotCount * RESIDENCY_RANGES_PER_SLOT;
    sSlotCount++;
    if (sPublishedCount < sSlotCount * RESIDENCY_RANGES_PER_SLOT) {
        sPublishedCount = sSlotCount * RESIDENCY_RANGES_PER_SLOT;
    }
    return &sSlots[sSlotCount - 1];
}

void Port_TilesetResidency_AddSlot(u32 gfxIndex, const u16* regions, u32 residentGroup,
                                   u32 shadowGroup, const void* shadowSrc1, void* dest1,
                                   const void* shadowSrc2, void* dest2, u32 size) {
    ResidencySlot* slot;
    const void* src[RESIDENCY_RANGES_PER_SLOT];
    u32 off[RESIDENCY_RANGES_PER_SLOT];
    const u16* entry;
    int firstRange;
    int i;

    if (residency_trace()) {
        fprintf(stderr,
                "[tileset] frame %u gfx %u: resident group %u, shadow group %u at +0x%X "
                "(cam %d,%d)\n",
                Port_Capture_Frame(), gfxIndex, residentGroup, shadowGroup,
                PORT_VRAM_SHADOW_OFFSET,
                (int)gRoomControls.scroll_x - (int)gRoomControls.origin_x,
                (int)gRoomControls.scroll_y - (int)gRoomControls.origin_y);
    }
    if (regions == NULL) {
        return;
    }

    /* Validate both destinations before touching anything. A slot that is
     * half re-declared would publish one range against the new resident
     * group and one against the old, which renders as a plausible picture
     * with the wrong tiles in it — the hardest kind of wrong to notice. */
    src[0] = shadowSrc1;
    src[1] = shadowSrc2;
    off[0] = (u32)((uintptr_t)dest1 - VRAM);
    off[1] = (u32)((uintptr_t)dest2 - VRAM);
    for (i = 0; i < RESIDENCY_RANGES_PER_SLOT; i++) {
        if (src[i] == NULL || off[i] + size > PORT_VRAM_GBA_SIZE) {
            fprintf(stderr, "[tileset] gfx %u range %d is not in VRAM (0x%X +%u) — slot skipped\n",
                    gfxIndex, i, off[i], size);
            return;
        }
    }

    slot = residency_slot_for(gfxIndex, &firstRange);
    if (slot == NULL) {
        return;
    }

    slot->gfxIndex = gfxIndex;
    slot->shadowGroup = shadowGroup;
    slot->regionCount = 0;

    for (i = 0; i < RESIDENCY_RANGES_PER_SLOT; i++) {
        VirtuaPPUMode1CharSlot* published = &sPublished[firstRange + i];

        /* Straight into the bank, not through gba_write*: those guards exist
         * to keep the *engine* out of it, and this is the port. */
        memcpy(gVram + PORT_VRAM_SHADOW_OFFSET + off[i], src[i], size);

        published->addr_lo = off[i];
        published->addr_hi = off[i] + size;
        published->regions = slot->regions;
        published->count = 0; /* filled in below, once the list is built */
        published->fallback = 0u;
    }

    /* The engine's table is {group, x, y, w, h} in room *pixels*, terminated
     * by 0xff. The PPU tests tile_col/tile_row, so convert once here rather
     * than per tile during the raster. Every rectangle in all five town
     * tables and both Minish ones is 8-aligned, so this is exact; anything
     * that is not would silently lose up to 7 px, so it is checked. */
    for (entry = regions; *entry != 0xff && slot->regionCount < RESIDENCY_MAX_REGIONS; entry += 5) {
        VirtuaPPUMode1CharRegion* region = &slot->regions[slot->regionCount];

        if ((entry[1] | entry[2] | entry[3] | entry[4]) & 7u) {
            fprintf(stderr,
                    "[tileset] region %d of gfx %u is not 8-aligned: %u,%u %ux%u — skipped\n",
                    slot->regionCount, gfxIndex, entry[1], entry[2], entry[3], entry[4]);
            continue;
        }

        region->x0 = entry[1] >> 3;
        region->y0 = entry[2] >> 3;
        region->w = entry[3] >> 3;
        region->h = entry[4] >> 3;
        region->offset = (entry[0] == shadowGroup) ? PORT_VRAM_SHADOW_OFFSET : 0u;
        slot->regionCount++;
    }

    for (i = 0; i < RESIDENCY_RANGES_PER_SLOT; i++) {
        sPublished[firstRange + i].count = slot->regionCount;
    }

    sArea = gRoomControls.area;
    sRoom = gRoomControls.room;

    if (residency_trace()) {
        for (i = 0; i < RESIDENCY_RANGES_PER_SLOT; i++) {
            const VirtuaPPUMode1CharSlot* published = &sPublished[firstRange + i];
            fprintf(stderr, "[tileset]   chars 0x%05X..0x%05X\n", published->addr_lo,
                    published->addr_hi);
        }
        for (i = 0; i < slot->regionCount; i++) {
            const VirtuaPPUMode1CharRegion* region = &slot->regions[i];
            fprintf(stderr,
                    "[tileset]   region %d: tiles x %d..%d y %d..%d "
                    "(px %d..%d, %d..%d) -> %s\n",
                    i, region->x0, region->x0 + region->w - 1, region->y0,
                    region->y0 + region->h - 1, region->x0 * 8, (region->x0 + region->w) * 8 - 1,
                    region->y0 * 8, (region->y0 + region->h) * 8 - 1,
                    region->offset != 0u ? "shadow" : "resident");
        }
    }
}

void Port_TilesetResidency_PublishForBg(int bg) {
    int i;

    if (sPublishedCount == 0) {
        return;
    }
    /* Slots describe one room's VRAM. Leaving the area stops the offsets
     * rather than applying them to the room that followed; the manager
     * declares again on the way back in. */
    if (sArea != gRoomControls.area || sRoom != gRoomControls.room) {
        return;
    }

    /* A tile matching none of a slot's regions is in one of the authored
     * gaps between them, and there is no per-tile answer for it: the gap is
     * exactly where the data declines to say. Give it the group the engine
     * itself loaded, so those tiles keep rendering the way hardware renders
     * them — the manager's own camera-driven swap still runs and
     * gRoomVars.graphicsGroups still tracks it. */
    for (i = 0; i < sPublishedCount; i++) {
        const ResidencySlot* slot = &sSlots[i / RESIDENCY_RANGES_PER_SLOT];
        sPublished[i].fallback =
            (gRoomVars.graphicsGroups[slot->gfxIndex] == slot->shadowGroup) ? PORT_VRAM_SHADOW_OFFSET
                                                                           : 0u;
    }

    virtuappu_mode1_set_char_slots(bg, sPublished, sPublishedCount);
}

#else

void Port_TilesetResidency_Reset(void) {
}

void Port_TilesetResidency_AddSlot(u32 gfxIndex, const u16* regions, u32 residentGroup,
                                   u32 shadowGroup, const void* shadowSrc1, void* dest1,
                                   const void* shadowSrc2, void* dest2, u32 size) {
    (void)gfxIndex;
    (void)regions;
    (void)residentGroup;
    (void)shadowGroup;
    (void)shadowSrc1;
    (void)dest1;
    (void)shadowSrc2;
    (void)dest2;
    (void)size;
}

void Port_TilesetResidency_PublishForBg(int bg) {
    (void)bg;
}

#endif /* VIEWPORT_TILESET_RESIDENCY */
