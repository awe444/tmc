#ifndef VIEWPORT_H
#define VIEWPORT_H

#include "gba/defines.h"

/* Rendered viewport, and the camera geometry derived from it.
 *
 * The engine was written against a fixed 240x160 screen and spells the
 * derived quantities as literals: 0x78 for half the width, 0xf0 for the
 * width, 0xf8 for the width plus a one-tile margin. Those literals are
 * indistinguishable from unrelated constants by grep, which is why
 * docs/viewport-expansion-research-plan.md treats "camera constants are
 * scattered magic numbers" as its own blocker (§4, blocker 5).
 *
 * Defaults are GBA-native, so a build that overrides nothing behaves
 * exactly as before. The build overrides VIEWPORT_WIDTH alongside the PPU
 * and canvas width.
 */
#ifndef VIEWPORT_WIDTH
#define VIEWPORT_WIDTH DISPLAY_WIDTH
#endif
#ifndef VIEWPORT_HEIGHT
#define VIEWPORT_HEIGHT DISPLAY_HEIGHT
#endif

/* Where the camera holds its target: the viewport centre. */
#define VIEWPORT_HALF_WIDTH (VIEWPORT_WIDTH / 2)
#define VIEWPORT_HALF_HEIGHT (VIEWPORT_HEIGHT / 2)

/* Scroll-region tests allow one tile beyond the visible edge, so an entity
 * straddling the boundary still counts as on-screen (0xf8 = 240 + 8,
 * 0xa8 = 160 + 8). */
#define VIEWPORT_REGION_MARGIN 8
#define VIEWPORT_REGION_WIDTH (VIEWPORT_WIDTH + VIEWPORT_REGION_MARGIN)
#define VIEWPORT_REGION_HEIGHT (VIEWPORT_HEIGHT + VIEWPORT_REGION_MARGIN)

/* BG0 carries the HUD and most UI screens. On hardware it is a 32x32-tile
 * screenblock, which covers 256 px — enough for a 240-wide screen but not
 * for a wider one, where it would wrap and draw the HUD twice.
 *
 * A wider viewport therefore needs a wider BG0 tilemap. Growing the
 * hardware screenblock is not an option: BG0 sits at screenbase 31, so a
 * 64-wide map would run into the OBJ tile region at 0x10000. Instead the
 * buffer becomes a plain wider array and is handed to the PPU as a map
 * source (the same mechanism the world layers use), which has no
 * screenblock and no wrap. At GBA-native width nothing changes — the
 * buffer keeps its 32-tile stride and BG0 stays on the hardware path.
 *
 * Index UI writes with UI_BG0_AT(col, row) rather than a baked linear
 * offset, so the stride is in one place.
 */
#if VIEWPORT_WIDTH > 240
#define UI_BG0_WIDTH_TILES 64
#else
#define UI_BG0_WIDTH_TILES 32
#endif
#define UI_BG0_HEIGHT_TILES 32
#define UI_BG0_ENTRIES (UI_BG0_WIDTH_TILES * UI_BG0_HEIGHT_TILES)
#define UI_BG0_AT(col, row) ((row) * UI_BG0_WIDTH_TILES + (col))

/* Byte size of `rows` whole tilemap rows. Several UI routines clear a band
 * of rows with a literal byte count that silently assumed a 32-entry row;
 * at a wider stride such a literal clears less than a row and leaves the
 * tail of each row stale, which is what garbled the save/erase popups. */
#define UI_BG0_ROW_BYTES(rows) ((rows) * UI_BG0_WIDTH_TILES * 2)

/* Rightmost tile column of the viewport, for edge-anchored UI (D1). At
 * GBA-native width this is column 29, so right-anchored elements keep
 * their original positions. */
#define UI_VIEW_TILE_COLS (VIEWPORT_WIDTH / 8)
#define UI_BG0_RIGHT_COL (UI_VIEW_TILE_COLS - 1)

/* Centering shift for UI surfaces authored against a 240-wide screen.
 * D1 settles the *screens* (title, file select, menus) and the text box as
 * centered, unlike the in-game HUD which is edge-anchored — so these two
 * shifts coexist and must not be confused. Zero at GBA-native width. */
#define UI_CENTER_DX ((VIEWPORT_WIDTH - DISPLAY_WIDTH) / 2)
#define UI_CENTER_TILE_DX (UI_CENTER_DX / 8)

/* Shift applied to right-anchored UI positions (D1: edge-anchored HUD).
 * Zero at GBA-native width, so anchored elements keep their authored
 * coordinates there. */
#define UI_RIGHT_ANCHOR_DX (VIEWPORT_WIDTH - DISPLAY_WIDTH)

/* Horizontal camera limits for a room at (origin_x, width).
 *
 * A room at least as wide as the viewport scrolls between showing its left
 * edge and showing its right edge. A room *narrower* than the viewport
 * cannot scroll at all: it is pinned where it sits centred, leaving equal
 * borders either side (the §6 requirement — 443 of 617 rooms are narrower
 * than 320). That pinned position is left of the room origin, i.e. a
 * negative room-space camera offset, which the PPU map source renders as
 * backdrop beyond the room edges.
 *
 * At GBA-native width the narrow branch is unreachable: 240 is also the
 * narrowest room in the game, so these reduce to the original
 * `origin_x` / `origin_x + width - 240` exactly.
 */
#define VIEWPORT_CAM_MIN_X(origin_x, width)                                                        \
    ((int)(origin_x) - (((int)(width) <= VIEWPORT_WIDTH) ? ((VIEWPORT_WIDTH - (int)(width)) / 2) : 0))
#define VIEWPORT_CAM_MAX_X(origin_x, width)                                                        \
    (((int)(width) <= VIEWPORT_WIDTH) ? VIEWPORT_CAM_MIN_X(origin_x, width)                        \
                                      : ((int)(origin_x) + (int)(width) - VIEWPORT_WIDTH))

#endif // VIEWPORT_H
