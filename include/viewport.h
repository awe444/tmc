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
