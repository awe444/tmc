#pragma once
#include "port_types.h"
#include "viewport.h"
#include <stdbool.h>

/* Both of an area's alternative tilesets resident at once (B27).
 *
 * See VIEWPORT_TILESET_RESIDENCY in viewport.h for why this exists. The
 * division of labour: the tileset manager owns the tables and says which
 * groups pair up, this owns the shadow bank and the PPU binding, and
 * libs/ViruaPPU does the per-tile selection during the raster.
 *
 * Every entry point compiles to a no-op at GBA-native size, so the shipping
 * build behaves exactly as it always has. Callers in `src/` still wrap their
 * calls in `#if VIEWPORT_TILESET_RESIDENCY` — not for the no-op, but because
 * a GBA build has no `port/` include path at all.
 */

/* Room entry: forget the previous room's slots. Every slot must be declared
 * again afterwards, because the resident group is decided per entry. */
void Port_TilesetResidency_Reset(void);

/* Declare one tileset slot: a pair of gfx groups that load to the same two
 * VRAM blocks, and the engine region table saying which of them a tile
 * belongs to.
 *
 * `gfxIndex` indexes gRoomVars.graphicsGroups, which keeps tracking the
 * engine's own camera-based choice and supplies the group for tiles in the
 * authored gaps between regions. `residentGroup` is whatever the engine has
 * just loaded into the GBA's own VRAM; the bytes at `shadowSrc1/2` are
 * copied into the shadow bank now, at the same offsets within it as
 * `dest1/2` have within VRAM.
 *
 * `regions` is the engine's own table — {group, x, y, w, h} in room pixels,
 * terminated by 0xff — and is not copied, so it must outlive the room. */
void Port_TilesetResidency_AddSlot(u32 gfxIndex, const u16* regions, u32 residentGroup,
                                   u32 shadowGroup, const void* shadowSrc1, void* dest1,
                                   const void* shadowSrc2, void* dest2, u32 size);

/* Hand the declared slots to a layer the port has just bound a map source
 * to. Called per frame, because the gap fallback follows the camera. */
void Port_TilesetResidency_PublishForBg(int bg);
