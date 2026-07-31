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

/* BG0 carries the HUD, the text box and most UI screens.
 *
 * It stays a 32x32-tile map at every viewport size — the hardware shape.
 * Widening it was tried and abandoned: the row stride is baked into far
 * more places than the buffer's own accessors (the shared text renderer's
 * two-tile-tall glyph writer, per-line advances, and several byte-count
 * clears), and each one is a silent corruption rather than a compile error.
 * Three rounds of playtesting kept finding more.
 *
 * Keeping the hardware stride means BG0 cannot place a tile past x=255, so
 * a right-edge-anchored HUD is not achievable on this layer. D1 is
 * therefore realised as *centered*: the whole 240-wide layer is shifted to
 * the middle of a wider viewport, so the HUD, the text box and every UI
 * screen keep their authored relationship to each other and only their
 * position on the frame changes. Sprites belonging to the HUD are shifted
 * by the same amount at their source (gHUD.buttonX) so they travel with it.
 *
 * The accessors below are kept even though they now reduce to the original
 * literals: they document intent, and they are how a future height
 * expansion or a second attempt at widening would be expressed.
 */
#define UI_BG0_WIDTH_TILES 32
#define UI_BG0_HEIGHT_TILES 32
#define UI_BG0_ENTRIES (UI_BG0_WIDTH_TILES * UI_BG0_HEIGHT_TILES)
#define UI_BG0_AT(col, row) ((row) * UI_BG0_WIDTH_TILES + (col))

/* Byte size of `rows` whole tilemap rows. */
#define UI_BG0_ROW_BYTES(rows) ((rows) * UI_BG0_WIDTH_TILES * 2)

/* Rightmost tile column of the authored 240-wide layout. */
#define UI_BG0_RIGHT_COL ((DISPLAY_WIDTH / 8) - 1)

/* Centering shift for UI surfaces authored against a 240-wide screen.
 * D1 was reversed (see docs/viewport-bug-tracker.md): the in-game HUD is
 * *also* centered now, not edge-anchored, because a 32-column BG0 cannot
 * place a tile past x=255. There is therefore only one shift, and every
 * 240-authored surface takes it. Zero at GBA-native width. */
#define UI_CENTER_DX ((VIEWPORT_WIDTH - DISPLAY_WIDTH) / 2)
#define UI_CENTER_TILE_DX (UI_CENTER_DX / 8)

/* HUD sprites are positioned in engine coordinates and must move with the
 * centred BG0 layer they sit on, so they take the same shift. World sprites
 * must NOT — that is why this is applied at each HUD source site rather than
 * as a global OBJ offset. Every UI element that sets a screen x needs it:
 * buttons via gHUD.buttonX, the heart overlay and the Ezlo nag directly
 * (ui.c); item and text elements inherit x from the button element and so
 * must not add it again. Zero at GBA-native width. */
#define UI_HUD_SPRITE_DX UI_CENTER_DX

/* Per-scanline (HBlank DMA) tables.
 *
 * Nine sites register a table that the DMA replays one entry per rendered
 * line: the four circular WIN0H windows (common.c), three BG3HOFS scrollers
 * (light rays, steam, Vaati's arrival), the pause map's per-line BG3CNT, and
 * the rolling barrel's per-line BG2 affine matrix. Each fills exactly one
 * entry per line, so every fill loop is bounded by the line count and every
 * table is VIEWPORT_HEIGHT entries long — a table sized for 160 lines feeds
 * the DMA whatever follows it in memory for the remaining 80.
 *
 * The tables are double-buffered in gUnk_02017AA0, and the half stride is
 * set by the widest consumer: the barrel writes a whole 8-halfword affine
 * matrix per line, where the others write a single halfword. At GBA-native
 * height a half is 160 * 16 = 0xA00 bytes, which is what the engine spells
 * literally, and the pair is the 0x1400 the array has always been.
 */
#define VIEWPORT_HDMA_UNITS_PER_LINE 8 /* halfwords; the affine-matrix case */
#define VIEWPORT_HDMA_HALF_BYTES (VIEWPORT_HEIGHT * VIEWPORT_HDMA_UNITS_PER_LINE * 2)
#define VIEWPORT_HDMA_HALF_U16 (VIEWPORT_HDMA_HALF_BYTES / 2)
#define VIEWPORT_HDMA_HALF_AFFINE (VIEWPORT_HDMA_HALF_BYTES / 16)
#define VIEWPORT_HDMA_BYTES (VIEWPORT_HDMA_HALF_BYTES * 2)

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
