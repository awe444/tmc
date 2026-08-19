#ifndef PORT_BORDER_COLOR_H
#define PORT_BORDER_COLOR_H

/*
 * Per-scene border colour.
 *
 * Where a 240x160-authored surface is centred in a larger viewport, the
 * margin around it is the PPU backdrop — BG palette entry 0 — because that
 * is what hardware shows outside every layer (B14). A few scenes look better
 * with a different colour there than the one their own palette happens to
 * carry, so this overrides that entry for them.
 *
 * It changes only the margin, not the scene: in all three cases the backdrop
 * is drawn nowhere inside the centred 240x160 (measured — 0 px), so nothing
 * the GBA would have shown moves.
 *
 * Called from FadeVBlank so the override is re-derived after the fade every
 * frame and carries it, the same reason the B27 shadow palettes are rebuilt
 * there.
 */
void Port_BorderColor_Apply(void);

#endif /* PORT_BORDER_COLOR_H */
