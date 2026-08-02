#ifndef PORT_VIEWPORT_H
#define PORT_VIEWPORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The presentation canvas — the surface the port actually shows.
 *
 * The PPU renders MODE1_GBA_WIDTH x MODE1_GBA_HEIGHT (240x160). The canvas
 * is 320x240 (4:3), and each frame the PPU output is composited into its
 * centre over a border fill. Everything downstream of that composite —
 * internal render scale, xBRZ, CRT/LCD filters, fit-rect, present — works
 * on the canvas, not on the raw PPU framebuffer.
 *
 * This is Option B of docs/viewport-expansion-research-plan.md: correct
 * 4:3 aspect with zero engine changes, and the shippable fallback if the
 * expansion work is abandoned.
 *
 * It is also the seam the expansion grows into. Milestone 1 widens the
 * *rendered* region to 320 and Milestone 2 to 240 tall; as that happens
 * PORT_VIEW_CONTENT_* approach PORT_VIEW_* and the borders shrink to
 * nothing on rooms large enough to fill the viewport. Nothing downstream
 * of the composite needs to change — which is the point of introducing
 * the canvas now rather than at the end.
 *
 * D2 (research plan §0) fixed these as build-time constants for
 * Milestone 1; a runtime-configurable viewport is deferred.
 */
#define PORT_VIEW_WIDTH  320
#define PORT_VIEW_HEIGHT 240

/* Size of the region the PPU currently renders into the canvas. Grows to
 * PORT_VIEW_WIDTH in Milestone 1, PORT_VIEW_HEIGHT in Milestone 2. */
#ifndef PORT_VIEW_CONTENT_WIDTH
#define PORT_VIEW_CONTENT_WIDTH  240
#endif
#ifndef PORT_VIEW_CONTENT_HEIGHT
#define PORT_VIEW_CONTENT_HEIGHT 160
#endif

/* Where that region lands: centred, so rooms smaller than the viewport
 * get equal borders on both sides (research plan §6 — 78% of rooms are
 * bordered in at least one axis, so this is the common case, not a
 * fallback path). */
#define PORT_VIEW_CONTENT_X ((PORT_VIEW_WIDTH - PORT_VIEW_CONTENT_WIDTH) / 2)
#define PORT_VIEW_CONTENT_Y ((PORT_VIEW_HEIGHT - PORT_VIEW_CONTENT_HEIGHT) / 2)

/* Fill for canvas the PPU does not render into — the ring outside
 * PORT_VIEW_CONTENT_*, which is empty by construction and has no colour of
 * its own. ABGR8888 to match virtuappu_frame_buffer.
 *
 * This is *not* the colour of the bands around a centred UI screen. Those
 * are inside the rendered region and carry that screen's PPU backdrop, which
 * is what hardware shows outside every layer; D3 was amended at Milestone 1
 * sign-off to accept them. */
#define PORT_VIEW_BORDER_COLOR 0xFF000000u

/* The composed canvas, PORT_VIEW_WIDTH * PORT_VIEW_HEIGHT pixels.
 * Valid after Port_PPU_PresentFrame has composed a frame. */
extern uint32_t* Port_Viewport_Canvas(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_VIEWPORT_H */
