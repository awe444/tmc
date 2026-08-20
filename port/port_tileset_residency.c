/* See port_tileset_residency.h. B27. */
#include "port_tileset_residency.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gba/gba.h"
#include "common.h"
#include "functions.h"
#include "room.h"
#include "structures.h"

#include "port_capture.h"
#include "port_gba_mem.h"
#include "cpu/mode1.h"

/* The banks the port allocates and the banks the PPU addresses are separate
 * constants in separate repositories, and a mismatch is a silent read of the
 * wrong memory rather than a build failure. Tie them together here, where
 * both headers are in scope. */
typedef char port_vram_agrees_with_ppu[(PORT_VRAM_SHADOW_OFFSET == MODE1_VRAM_SHADOW_OFFSET &&
                                        PORT_VRAM_BANK_STRIDE == MODE1_VRAM_BANK_STRIDE &&
                                        PORT_VRAM_BANKS <= MODE1_VRAM_BANKS &&
                                        PORT_VRAM_TOTAL_SIZE >= MODE1_VRAM_TOTAL_SIZE)
                                           ? 1
                                           : -1];

static int residency_trace(void) {
    static int enabled = -1;
    if (enabled < 0) {
        enabled = (getenv("TMC_TILESET_TRACE") != NULL);
    }
    return enabled;
}

void Port_TilesetResidency_TraceGroups(void) {
    static int enabled = -1;
    static int everyFrame = 0;
    static u8 last[4] = { 0xff, 0xff, 0xff, 0xff };
    static u8 lastArea = 0xff;
    static u8 lastRoom = 0xff;
    int i;

    if (enabled < 0) {
        const char* env = getenv("TMC_TILESET_TRACE");
        enabled = (env != NULL);
        /* =2 adds a line per frame. Locating a pair of frames that share a
         * camera position but not a group needs the positions in between,
         * not just the ones a change happened at. */
        everyFrame = (env != NULL && env[0] == '2');
    }
    if (!enabled) {
        return;
    }
    if (everyFrame) {
        fprintf(stderr, "[frame] %u area 0x%02X room 0x%02X cam %d,%d groups %u,%u,%u\n",
                Port_Capture_Frame(), gRoomControls.area, gRoomControls.room,
                (int)gRoomControls.scroll_x - (int)gRoomControls.origin_x,
                (int)gRoomControls.scroll_y - (int)gRoomControls.origin_y,
                gRoomVars.graphicsGroups[0], gRoomVars.graphicsGroups[1],
                gRoomVars.graphicsGroups[2]);
    }
    if (lastArea != gRoomControls.area || lastRoom != gRoomControls.room) {
        lastArea = gRoomControls.area;
        lastRoom = gRoomControls.room;
        for (i = 0; i < 4; i++) {
            last[i] = 0xff;
        }
        fprintf(stderr, "[groups] frame %u ENTER area 0x%02X room 0x%02X (%ux%u)\n",
                Port_Capture_Frame(), gRoomControls.area, gRoomControls.room,
                gRoomControls.width, gRoomControls.height);
    }
    for (i = 0; i < 4; i++) {
        if (gRoomVars.graphicsGroups[i] == last[i]) {
            continue;
        }
        fprintf(stderr, "[groups] frame %u slot %d: %u -> %u  cam %d,%d\n", Port_Capture_Frame(), i,
                last[i], gRoomVars.graphicsGroups[i],
                (int)gRoomControls.scroll_x - (int)gRoomControls.origin_x,
                (int)gRoomControls.scroll_y - (int)gRoomControls.origin_y);
        last[i] = gRoomVars.graphicsGroups[i];
    }
}

#if VIEWPORT_TILESET_RESIDENCY

/* Hyrule Town declares three; Minish Village declares one. */
#define RESIDENCY_MAX_SLOTS 4
/* The longest authored list is Minish Village's eight. */
#define RESIDENCY_MAX_REGIONS 8
/* Character-address ranges one slot governs, after merging adjacent blocks.
 * Hyrule Town's groups are two 4 KB blocks in different charbase windows;
 * Minish Village's are eight, which merge down to two. */
#define RESIDENCY_MAX_RANGES 4
/* Published entries: one per range, across every slot. */
#define RESIDENCY_MAX_PUBLISHED (RESIDENCY_MAX_SLOTS * RESIDENCY_MAX_RANGES)

typedef char residency_fits_ppu[(RESIDENCY_MAX_PUBLISHED <= MODE1_MAX_CHAR_SLOTS) ? 1 : -1];

typedef struct {
    u32 gfxIndex;      /* which of gRoomVars.graphicsGroups tracks this slot */
    u32 residentGroup; /* the group in the GBA's own VRAM, so at offset 0    */
    u8 area;
    u8 room;
    bool valid;
    VirtuaPPUMode1CharRegion regions[RESIDENCY_MAX_REGIONS];
    int regionCount;
    u32 lo[RESIDENCY_MAX_RANGES]; /* the character spans this slot governs */
    u32 hi[RESIDENCY_MAX_RANGES];
    int rangeCount;
} ResidencySlot;

static ResidencySlot sSlots[RESIDENCY_MAX_SLOTS];
static int sSlotCount;

/* What is handed to the PPU: one entry per address range, every range of a
 * slot sharing that slot's region list.
 *
 * Rebuilt from the slots on every publish rather than edited in place. A slot
 * that does not apply is then simply absent, instead of present with its
 * answers blanked — which is a distinction with teeth: blanking has to
 * remember every field, and the one it forgot (`fallback_palette_set`)
 * recoloured two Minish Village interiors. Rebuilding also means retiring a
 * slot cannot destroy state only DeclareSlot could put back. */
static VirtuaPPUMode1CharSlot sPublished[RESIDENCY_MAX_PUBLISHED];
static int sPublishedCount;

/* Per-group BG palettes.
 *
 * `sRawPalette` is the palette group's own colours, snapshotted once from
 * the same loader the engine uses. `sFadedPalette` is that run through this
 * frame's fade, which is what the renderer reads — the live palette is faded
 * every VBlank and these have to move with it. Banks outside the group's
 * payload are copied from the live palette so they match exactly. */
#define RESIDENCY_PALETTE_ENTRIES 256
static u16 sRawPalette[PORT_VRAM_BANKS][RESIDENCY_PALETTE_ENTRIES];
static u16 sFadedPalette[PORT_VRAM_BANKS][RESIDENCY_PALETTE_ENTRIES];
static u8 sPaletteValid[PORT_VRAM_BANKS];
/* Which 16-colour banks the group's palette group actually wrote. */
static u32 sPaletteBanks[PORT_VRAM_BANKS];

extern u16 gPaletteBuffer[];
extern u32 gUsedPalettes;
void Port_FadeApply16(const u16* srcPtr, u16* dstPtr, u16 intensity, u8 color);

void Port_TilesetResidency_Reset(void) {
    int i;
    sSlotCount = 0;
    sPublishedCount = 0;
    for (i = 0; i < (int)PORT_VRAM_BANKS; i++) {
        sPaletteValid[i] = 0;
        sPaletteBanks[i] = 0;
    }
    virtuappu_mode1_clear_bg_palette_sets();
}

void Port_TilesetResidency_SetGroupPalette(u32 group, u32 paletteGroupId) {
    u16 saved[RESIDENCY_PALETTE_ENTRIES];
    u32 savedUsed;
    u32 written;

    if (group >= PORT_VRAM_BANKS || sPaletteValid[group]) {
        return;
    }

    /* Snapshot by loading it for real and putting everything back. Going
     * through the engine's own loader is the point: it is what decides where
     * a palette group lands and what the port's asset path substitutes, and
     * a second implementation here would be a second thing to keep right.
     *
     * Which banks it wrote comes from gUsedPalettes, which LoadPalettes sets
     * for exactly the banks it touched — on the asset path too, because that
     * goes through LoadPalettes as well.
     *
     * It must *not* be derived by diffing the result against what was loaded
     * before. That was the first version and it is wrong in a way that hides:
     * the group whose palette is already live diffs to nothing, so its mask
     * comes out empty, every one of its banks then follows the live palette,
     * and its tiles turn whatever colour the current group is. Which is the
     * defect this is meant to fix, reintroduced one level down. */
    memcpy(saved, gPaletteBuffer, sizeof(saved));
    savedUsed = gUsedPalettes;
    gUsedPalettes = 0;
    LoadPaletteGroup(paletteGroupId);
    written = gUsedPalettes;
    memcpy(sRawPalette[group], gPaletteBuffer, sizeof(sRawPalette[group]));
    memcpy(gPaletteBuffer, saved, sizeof(saved));
    gUsedPalettes = savedUsed;

    /* Only the banks this group writes are its own; the rest follow the live
     * palette. Bits above the BG half address OBJ palettes and are not ours. */
    sPaletteBanks[group] = written & 0xFFFFu;
    sPaletteValid[group] = 1;
    virtuappu_mode1_set_bg_palette_set((int)group + 1, sFadedPalette[group]);
    if (residency_trace()) {
        fprintf(stderr, "[tileset] palette group 0x%02X for gfx group %u -> banks 0x%08X\n",
                paletteGroupId, group, sPaletteBanks[group]);
    }
}

void Port_TilesetResidency_UpdatePalettes(void) {
    u32 group;

    for (group = 0; group < PORT_VRAM_BANKS; group++) {
        int bank;
        if (!sPaletteValid[group]) {
            continue;
        }
        for (bank = 0; bank < RESIDENCY_PALETTE_ENTRIES / 16; bank++) {
            if ((sPaletteBanks[group] & (1u << bank)) == 0) {
                /* Not this group's colours — track the live palette exactly,
                 * fade and all, by copying what the engine just produced. */
                memcpy(&sFadedPalette[group][bank * 16], &gBgPltt[bank * 16], 16 * sizeof(u16));
                continue;
            }
            Port_FadeApply16(&sRawPalette[group][bank * 16], &sFadedPalette[group][bank * 16],
                             gUnk_020354C0[bank].unk2, gUnk_020354C0[bank].unk1);
        }
    }
}

/* The slot already describing this gfx index, or a fresh one. */
static ResidencySlot* residency_slot_for(u32 gfxIndex) {
    int i;

    for (i = 0; i < sSlotCount; i++) {
        if (sSlots[i].gfxIndex == gfxIndex) {
            return &sSlots[i];
        }
    }
    if (sSlotCount >= RESIDENCY_MAX_SLOTS) {
        return NULL;
    }
    memset(&sSlots[sSlotCount], 0, sizeof(sSlots[sSlotCount]));
    sSlots[sSlotCount].gfxIndex = gfxIndex;
    return &sSlots[sSlotCount++];
}

/* Collect the distinct VRAM spans a slot's blocks cover, merging the ones
 * that touch. Minish Village loads eight 4 KB blocks to two runs of four
 * contiguous addresses, so this turns eight into two — fewer ranges is fewer
 * comparisons in the per-tile lookup, and the merge is exact. */
static int residency_collect_ranges(const PortTilesetBlock* blocks, int blockCount, u32* lo,
                                    u32* hi, int maxRanges) {
    int count = 0;
    int i;
    int j;

    for (i = 0; i < blockCount; i++) {
        u32 blo = (u32)((uintptr_t)blocks[i].dest - VRAM);
        u32 bhi = blo + blocks[i].size;
        bool merged = false;

        if (blocks[i].src == NULL || bhi > PORT_VRAM_GBA_SIZE || blocks[i].size == 0) {
            fprintf(stderr, "[tileset] block %d is not in VRAM (0x%X +%u) — slot skipped\n", i, blo,
                    blocks[i].size);
            return -1;
        }
        for (j = 0; j < count; j++) {
            if (blo <= hi[j] && bhi >= lo[j]) {
                if (blo < lo[j]) {
                    lo[j] = blo;
                }
                if (bhi > hi[j]) {
                    hi[j] = bhi;
                }
                merged = true;
                break;
            }
        }
        if (merged) {
            continue;
        }
        if (count >= maxRanges) {
            fprintf(stderr, "[tileset] more than %d character ranges — slot skipped\n", maxRanges);
            return -1;
        }
        lo[count] = blo;
        hi[count] = bhi;
        count++;
    }

    /* One merging pass is not enough when blocks arrive out of order: two
     * ranges can become adjacent only after a third joins them. Repeat until
     * nothing more merges. */
    for (;;) {
        bool any = false;
        for (i = 0; i < count && !any; i++) {
            for (j = i + 1; j < count; j++) {
                if (lo[i] <= hi[j] && hi[i] >= lo[j]) {
                    if (lo[j] < lo[i]) {
                        lo[i] = lo[j];
                    }
                    if (hi[j] > hi[i]) {
                        hi[i] = hi[j];
                    }
                    lo[j] = lo[count - 1];
                    hi[j] = hi[count - 1];
                    count--;
                    any = true;
                    break;
                }
            }
        }
        if (!any) {
            break;
        }
    }
    return count;
}

bool Port_TilesetResidency_SlotDeclared(u32 gfxIndex) {
    const ResidencySlot* slot = residency_slot_for(gfxIndex);
    return slot != NULL && slot->valid && slot->area == gRoomControls.area &&
           slot->room == gRoomControls.room;
}

void Port_TilesetResidency_DeclareSlot(u32 gfxIndex, const u16* regions, u32 residentGroup,
                                       const PortTilesetBlock* blocks, int blockCount) {
    ResidencySlot* slot;
    u32 lo[RESIDENCY_MAX_RANGES];
    u32 hi[RESIDENCY_MAX_RANGES];
    const u16* entry;
    int rangeCount;
    int i;

    if (regions == NULL || blocks == NULL || blockCount <= 0 ||
        (residentGroup >= PORT_VRAM_BANKS && residentGroup != PORT_TILESET_NO_RESIDENT)) {
        return;
    }
    slot = residency_slot_for(gfxIndex);
    if (slot == NULL) {
        return;
    }
    /* The answer depends on the room and on which group is resident, and on
     * nothing else, so those two are the whole cache key. Re-declaring is the
     * normal case — the manager swaps the resident group as the camera moves,
     * exactly as it always has, and each swap makes a different group the one
     * that reads the GBA's own VRAM. */
    if (slot->valid && slot->residentGroup == residentGroup && slot->area == gRoomControls.area &&
        slot->room == gRoomControls.room) {
        return;
    }

    rangeCount = residency_collect_ranges(blocks, blockCount, lo, hi, RESIDENCY_MAX_RANGES);
    if (rangeCount <= 0) {
        slot->valid = false;
        return;
    }

    /* Every group except the resident one goes to its bank. The resident one
     * is left reading the GBA's own VRAM, so a character write the manager
     * does not know about — Hyrule Town's second oracle house — is still
     * seen for the group it was written against. With no resident group at
     * all, every group is copied and VRAM stops being read for these
     * addresses. */
    for (i = 0; i < blockCount; i++) {
        u32 off;
        if (blocks[i].group == residentGroup) {
            continue;
        }
        if (blocks[i].group >= PORT_VRAM_BANKS) {
            continue;
        }
        off = (u32)((uintptr_t)blocks[i].dest - VRAM);
        /* Straight into the bank, not through gba_write*: those guards exist
         * to keep the *engine* out of it, and this is the port. */
        memcpy(gVram + PORT_VRAM_BANK_OFFSET(blocks[i].group) + off, blocks[i].src,
               blocks[i].size);
    }

    /* The engine's table is {group, x, y, w, h} in room *pixels*, terminated
     * by 0xff. The PPU tests tile_col/tile_row, so convert once here rather
     * than per tile during the raster. Every rectangle in all five town
     * tables and both Minish ones is 8-aligned, so this is exact; anything
     * that is not would silently lose up to 7 px, so it is checked. */
    slot->regionCount = 0;
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
        region->offset = (entry[0] == residentGroup) ? 0u : PORT_VRAM_BANK_OFFSET(entry[0]);
        /* A group that swaps the palette too needs its own; one that does
         * not — every group in Hyrule Town — keeps the hardware palette,
         * which is what set 0 means. */
        region->palette_set =
            (entry[0] < PORT_VRAM_BANKS && sPaletteValid[entry[0]]) ? (int)entry[0] + 1 : 0;
        slot->regionCount++;
    }

    for (i = 0; i < rangeCount; i++) {
        slot->lo[i] = lo[i];
        slot->hi[i] = hi[i];
    }
    slot->rangeCount = rangeCount;

    slot->residentGroup = residentGroup;
    slot->area = gRoomControls.area;
    slot->room = gRoomControls.room;
    slot->valid = true;

    if (residency_trace()) {
        fprintf(stderr, "[tileset] frame %u gfx %u: resident group %u (cam %d,%d)\n",
                Port_Capture_Frame(), gfxIndex, residentGroup,
                (int)gRoomControls.scroll_x - (int)gRoomControls.origin_x,
                (int)gRoomControls.scroll_y - (int)gRoomControls.origin_y);
        for (i = 0; i < rangeCount; i++) {
            fprintf(stderr, "[tileset]   chars 0x%05X..0x%05X\n", lo[i], hi[i]);
        }
        for (i = 0; i < slot->regionCount; i++) {
            const VirtuaPPUMode1CharRegion* region = &slot->regions[i];
            fprintf(stderr,
                    "[tileset]   region %d: tiles x %d..%d y %d..%d (px %d..%d, %d..%d) -> %s\n", i,
                    region->x0, region->x0 + region->w - 1, region->y0,
                    region->y0 + region->h - 1, region->x0 * 8, (region->x0 + region->w) * 8 - 1,
                    region->y0 * 8, (region->y0 + region->h) * 8 - 1,
                    region->offset != 0u ? "bank" : "VRAM");
        }
    }
}

u32 Port_TilesetResidency_OffsetFor(u32 charAddr, int tileCol, int tileRow) {
    int i;
    int j;

    for (i = 0; i < sPublishedCount; i++) {
        const VirtuaPPUMode1CharSlot* slot = &sPublished[i];
        if (charAddr < slot->addr_lo || charAddr >= slot->addr_hi) {
            continue;
        }
        for (j = 0; j < slot->count; j++) {
            const VirtuaPPUMode1CharRegion* region = &slot->regions[j];
            if (tileCol >= region->x0 && tileCol < region->x0 + region->w &&
                tileRow >= region->y0 && tileRow < region->y0 + region->h) {
                return region->offset;
            }
        }
        return slot->fallback;
    }
    return 0u;
}

void Port_TilesetResidency_PublishForBg(int bg) {
    static int disabled = -1;
    int i;
    int j;

    /* TMC_TILESET_OFF=1 suppresses the per-tile selection, leaving every
     * layer reading the GBA's own VRAM exactly as it did before B27. It is
     * how a before/after comparison comes from one binary instead of two —
     * the only difference then really is the selection, rather than every
     * other change that happened to land in between. */
    if (disabled < 0) {
        disabled = (getenv("TMC_TILESET_OFF") != NULL);
    }
    if (disabled || sSlotCount == 0) {
        return;
    }

    /* Rebuilt from scratch every publish. A slot that does not apply is left
     * out entirely rather than blanked in place — see sPublished.
     *
     * A tile matching none of a slot's regions is in one of the authored gaps
     * between them, and there is no per-tile answer for it: the gap is
     * exactly where the data declines to say. Give it the group the engine
     * itself loaded, so those tiles keep rendering the way hardware renders
     * them — the manager's own camera-driven selection still runs and
     * gRoomVars.graphicsGroups still tracks it. */
    sPublishedCount = 0;
    for (i = 0; i < sSlotCount; i++) {
        const ResidencySlot* slot = &sSlots[i];
        u32 fallback;
        int fallbackPalette;
        u8 group;

        /* Slots describe one room's VRAM. Leaving the area stops them rather
         * than applying them to the room that followed; the manager declares
         * again on the way back in. */
        if (!slot->valid || slot->area != gRoomControls.area || slot->room != gRoomControls.room) {
            continue;
        }
        group = gRoomVars.graphicsGroups[slot->gfxIndex];
        fallback = (group == slot->residentGroup || group >= PORT_VRAM_BANKS)
                       ? 0u
                       : PORT_VRAM_BANK_OFFSET(group);
        fallbackPalette = (group < PORT_VRAM_BANKS && sPaletteValid[group]) ? (int)group + 1 : 0;
        for (j = 0; j < slot->rangeCount; j++) {
            VirtuaPPUMode1CharSlot* published;
            if (sPublishedCount >= RESIDENCY_MAX_PUBLISHED) {
                break;
            }
            published = &sPublished[sPublishedCount++];
            published->addr_lo = slot->lo[j];
            published->addr_hi = slot->hi[j];
            published->regions = slot->regions;
            published->count = slot->regionCount;
            published->fallback = fallback;
            published->fallback_palette_set = fallbackPalette;
        }
    }

    if (sPublishedCount == 0) {
        return;
    }
    virtuappu_mode1_set_char_slots(bg, sPublished, sPublishedCount);
}

#else

bool Port_TilesetResidency_SlotDeclared(u32 gfxIndex) {
    (void)gfxIndex;
    return false;
}

void Port_TilesetResidency_Reset(void) {
}

void Port_TilesetResidency_DeclareSlot(u32 gfxIndex, const u16* regions, u32 residentGroup,
                                       const PortTilesetBlock* blocks, int blockCount) {
    (void)gfxIndex;
    (void)regions;
    (void)residentGroup;
    (void)blocks;
    (void)blockCount;
}

u32 Port_TilesetResidency_OffsetFor(u32 charAddr, int tileCol, int tileRow) {
    (void)charAddr;
    (void)tileCol;
    (void)tileRow;
    return 0u;
}

void Port_TilesetResidency_SetGroupPalette(u32 group, u32 paletteGroupId) {
    (void)group;
    (void)paletteGroupId;
}

void Port_TilesetResidency_UpdatePalettes(void) {
}

void Port_TilesetResidency_PublishForBg(int bg) {
    (void)bg;
}

#endif /* VIEWPORT_TILESET_RESIDENCY */
