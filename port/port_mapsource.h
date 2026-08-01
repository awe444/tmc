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
