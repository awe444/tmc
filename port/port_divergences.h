#ifndef PORT_DIVERGENCES_H
#define PORT_DIVERGENCES_H

/* Deliberate departures from what the GBA does.
 *
 * Everything here makes the port render something the real game does not. Each
 * one exists because the expanded viewport shows world the authored data never
 * expected anyone to see, so "match hardware" and "look right" stop being the
 * same instruction. They are gated to the expanded viewport, so the shipping
 * 240x160 build stays byte-faithful.
 *
 * Every entry must be recorded in docs/hardware-divergences.md before it is
 * added here. That document is the list; this file is the code. */

#include "global.h"

/* D-1 — beanstalk rooms' ground palette.
 *
 * LoadRoomTileSet mirrors BG palette 3 into OBJ palette 5 so sprites can wear
 * the room's terrain colours. Area 13's tileset leaves BG palette 3 as an
 * all-0x7C1F placeholder, because the only sprite that uses it — the beanstalk
 * base — sits below the GBA's 160 rows and is never on screen. At 240 rows it
 * is, and it draws solid magenta.
 *
 * Call after LoadRoomTileSet's mirror. Loads the ground palette of the
 * overworld room each beanstalk grows out of; no-op outside area 13 and at
 * 240x160. */
void Port_Divergence_BeanstalkGroundPalette(void);

#endif /* PORT_DIVERGENCES_H */
