#ifndef PORT_MAPSOURCE_H
#define PORT_MAPSOURCE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Option E (docs/viewport-expansion-research-plan.md §7): bind the engine's
 * full-room special maps to the PPU as BG map sources, so world layers can
 * be drawn at any viewport size instead of being capped by the 32x32
 * screenblock window.
 *
 * Only layers the *map-authoritative predicate* accepts are bound. The
 * predicate is the Spike 2 finding in executable form: the special maps
 * are a live full-room tilemap during ordinary gameplay, but the engine
 * also reuses those arrays as menu scratch, boss-fight bitmaps and
 * screenblock-shaped overlay data (§5.1). Every one of those states flips
 * at least one signal the predicate reads, so excluded layers fall back to
 * the untouched screenblock path automatically.
 *
 * Layer ids are the engine's map layers, not BG indices:
 *   0 = bottom (gMapDataBottomSpecial, displayed through BG2)
 *   1 = top    (gMapDataTopSpecial,    displayed through BG1)
 */

/* Called once per frame from the PPU driver, before rendering. */
void Port_MapSource_Update(void);

/* The predicate. layer: 0 = bottom, 1 = top. */
bool Port_MapSource_LayerAuthoritative(int layer);

/* BG index (0-3) the given map layer currently displays through, or -1.
 * Derived from MapLayer.bgSettings — it is per-room, not fixed. */
int Port_MapSource_LayerBgIndex(int layer);

/* Reason code for the most recent rejection of `layer` (diagnostics; see
 * Port_MapSource_ReasonName). Valid after Port_MapSource_LayerAuthoritative
 * or Port_MapSource_Update. */
int Port_MapSource_LastReason(int layer);
const char* Port_MapSource_ReasonName(int reason);

/* --no-map-sampling disables binding entirely (everything renders through
 * the screenblock path). The A/B switch behind Spike 3's equivalence
 * check; also the escape hatch if a binding ever misbehaves. */
void Port_MapSource_SetEnabled(bool enabled);
bool Port_MapSource_Enabled(void);

/* True while UI content is being centred (i.e. outside gameplay at a wider
 * viewport). The canvas compositor uses it to paint the side bands the
 * border colour instead of letting each screen's backdrop palette show
 * through — D3 asks for solid black borders, and a backdrop is whatever
 * palette entry 0 happens to be (pale yellow on the title screen). */
bool Port_MapSource_UiCentered(void);

/* The world-view twin of the above: a room drawn from an affine layer is a
 * 240x160-authored surface too, so it is centred as well (Deepwood's rolling
 * barrel). Kept separate from UiCentered because everything *else* about the
 * two differs — see mapsource_bind_ui. */
bool Port_MapSource_AffineCentered(void);

/* A BG3 world overlay declaring that it is anchored to the *screen* rather
 * than to the world, and to which edge of it (B21).
 *
 * BG3 in a world view is normally left unclipped, because these overlays are
 * tiled patterns locked to the world — hole parallax, cloud shadows, weather,
 * steam, POW all set bg3.xOffset from scroll_x — and letting the screenblock
 * wrap past 256 px is exactly what covers a wider viewport with more of the
 * same pattern. See the note in mapsource_bind_ui().
 *
 * Minish Woods' light shaft is neither tiled nor world-locked: its xOffset is
 * a constant, so the band sits against the right edge of the *authored 240-px
 * screen*, and the 256-px map is blank for the whole span to its left. Wrapped
 * past 256 the columns beyond the GBA's screen show that blank end, which is
 * the whole of B21 — the shaft stops 80 px short at 320 wide.
 *
 * An overlay that says so is clipped to the authored width instead and pinned
 * to the declared edge, which puts the band where the artwork means it to be
 * and leaves no wrap to show. Nothing else about the layer changes.
 *
 * A declaration lasts as long as the *overlay* does — until BG3 goes off or
 * the room changes — not as long as the declaring handler keeps being called.
 * Those are different, and the difference is visible: a light-ray fade-out
 * dispatches to a null handler on its first frame while eighty frames of fade
 * remain, and a text box suspends the managers entirely. Tying the clip to the
 * tick made the band jump on both. Silence therefore means "unchanged"; a
 * state that wants the unclipped behaviour back declares
 * PORT_BG3_ANCHOR_NONE. No-op at GBA-native width. */
enum {
    PORT_BG3_ANCHOR_NONE = 0,
    PORT_BG3_ANCHOR_RIGHT = 1
};
void Port_MapSource_DeclareBg3ScreenAnchor(int anchor);

/* A world-view layer carrying a tiled, screen-space overlay instead of the
 * room's authored map.
 *
 * The uniform rule for a layer with no map source is "it is reading a 32-tile
 * screenblock, which covers 256 px and wraps, so clip it to the authored width
 * and centre it" — right for a room map caught mid-transition, where repeating
 * the content would be wrong. It is exactly wrong for a repeating pattern,
 * where the wrap is what covers a wider viewport with more of the same. BG3
 * is exempted wholesale for that reason; this says the same thing about a
 * layer the engine has temporarily repurposed.
 *
 * Mt Crenel's weather manager takes BG1 away from the room's top map layer and
 * fills it with a rain sheet from gfx groups 0x2B-0x2E. With the map layer off
 * the map source is refused, the fallback clip caught it, and the rain covered
 * the centred 240 columns with bare border either side.
 *
 * Lasts until the room changes rather than until the declaring handler stops
 * ticking — B35's lesson. Handing the layer back needs no undeclaration: it
 * regains a map source, and the clip only ever applies without one. */
void Port_MapSource_DeclareTiledOverlay(int bg_index);

/* TMC_BLEND_TRACE=2 diagnostic: report a palette group as it loads. */
void Port_TracePaletteGroup(unsigned int group);
void Port_TracePaletteDrop(const char* where);

/* Tile columns the text box should shift itself by. Zero on UI screens,
 * where the whole BG0 layer is already shifted (see the note in the .c). */
int Port_MapSource_MessageTileShift(void);
int Port_MapSource_MessageTileShiftY(void);

/* Frames in which each layer was bound / rejected, for run summaries. */
void Port_MapSource_Report(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_MAPSOURCE_H */
