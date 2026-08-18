#pragma once
#include "port_types.h"
#include "viewport.h"
#include <stdbool.h>

/* Every alternative tileset resident at once (B27).
 *
 * See VIEWPORT_TILESET_RESIDENCY in viewport.h for why this exists. The
 * division of labour: the tileset manager owns the tables and says which
 * groups are alternatives for which VRAM, this owns the group banks and the
 * PPU binding, and libs/ViruaPPU does the per-tile selection during the
 * raster.
 *
 * Every entry point compiles to a no-op at GBA-native size, so the shipping
 * build behaves exactly as it always has. Callers in `src/` still wrap their
 * calls in `#if VIEWPORT_TILESET_RESIDENCY` — not for the no-op, but because
 * a GBA build has no `port/` include path at all.
 */

/* Diagnostic: report each change to gRoomVars.graphicsGroups with the frame
 * and camera position it happened at. TMC_TILESET_TRACE=1, or =2 to add a
 * line per frame. Works in every area and at both viewport sizes, including
 * areas with no slots declared. */
void Port_TilesetResidency_TraceGroups(void);

/* Room entry: forget the previous room's slots. */
void Port_TilesetResidency_Reset(void);

/* One block of one group's character data: the bytes, and the VRAM address
 * the engine loads them to. Two groups' blocks sharing a `dest` are what
 * makes them alternatives. */
typedef struct {
    u32 group;
    const void* src;
    void* dest;
    u32 size;
} PortTilesetBlock;

/* Pass as `residentGroup` when no group should read the GBA's own VRAM.
 *
 * Every group then reads its own bank, and what is in VRAM stops mattering
 * for these addresses. Use it wherever the manager owns the whole character
 * range — which is what lets Minish Village be correct *during* its
 * eight-frame staged load, where VRAM holds half of the outgoing group and
 * half of the incoming one and is briefly not any group at all.
 *
 * The alternative, naming the resident group, exists for Hyrule Town, where
 * the second oracle house is written into a slot's range behind the
 * manager's back and would be lost if nothing read VRAM. */
#define PORT_TILESET_NO_RESIDENT 0xFFFFFFFFu

/* Declare, or refresh, one tileset slot: a set of alternative gfx groups
 * that load to the same VRAM, and the engine region table saying which of
 * them a tile belongs to.
 *
 * `gfxIndex` indexes gRoomVars.graphicsGroups, which keeps tracking the
 * engine's own camera-based choice and supplies the group for tiles in the
 * authored gaps between regions. `residentGroup` is whatever the engine
 * currently has in the GBA's own VRAM; every *other* group's blocks are
 * copied into that group's bank, and tiles belonging to it read from there.
 * With PORT_TILESET_NO_RESIDENT every group is copied and none reads VRAM.
 *
 * Safe and cheap to call every frame: it returns immediately unless the room
 * or the resident group has changed, and those are the only things the
 * answer depends on.
 *
 * `regions` is the engine's own table — {group, x, y, w, h} in room pixels,
 * terminated by 0xff — and is not copied, so it must outlive the room. */
void Port_TilesetResidency_DeclareSlot(u32 gfxIndex, const u16* regions, u32 residentGroup,
                                       const PortTilesetBlock* blocks, int blockCount);

/* Say that a group also swaps the BG palette, and which palette group it
 * loads. Call before declaring the slot; the raw palette is snapshotted
 * once, and the faded copy the renderer uses is rebuilt every frame.
 *
 * Groups nobody declares a palette for keep the hardware palette, which is
 * every group in Hyrule Town — there, only the character data swaps. */
void Port_TilesetResidency_SetGroupPalette(u32 group, u32 paletteGroupId);

/* Rebuild the shadow BG palettes from this frame's fade. Called from
 * FadeVBlank, after it has written the live palette. */
void Port_TilesetResidency_UpdatePalettes(void);

/* Hand the declared slots to a layer the port has just bound a map source
 * to. Called per frame, because the gap fallback follows the camera. */
void Port_TilesetResidency_PublishForBg(int bg);

/* The offset the renderer would apply to a tile: same lookup, for probes.
 * Returns 0 when no slot governs the address, which is also what an
 * unhandled area returns. */
u32 Port_TilesetResidency_OffsetFor(u32 charAddr, int tileCol, int tileRow);
