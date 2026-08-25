/**
 * @file port_divergences.c
 *
 * Deliberate departures from hardware. See port_divergences.h and
 * docs/hardware-divergences.md.
 */
#include "port_divergences.h"

#include "area.h"
#include "common.h"
#include "gfx.h"
#include "roomid.h"
#include "room.h"
#include "viewport.h"

#if VIEWPORT_HEIGHT > DISPLAY_HEIGHT

/* OBJ palette 5, which LoadRoomTileSet mirrors BG palette 3 into.
 * LoadPalettes numbers OBJ palettes from 16, as LoadObjPaletteAtIndex does. */
#define BEANSTALK_GROUND_SLOT (16 + 5)

/* Each beanstalk's ground palette is the BG palette 3 of the overworld room it
 * grows out of — read out of those rooms and matched against the ROM's palette
 * table, rather than picked by eye:
 *
 *   beanstalk        source room            BG palette 3
 *   Mt Crenel        area  6 room 0         gPalette_550
 *   Lake Hylia       area 11 room 1         gPalette_381
 *   Ruins            area  5 room 1         gPalette_537
 *   Eastern Hills    area  3 room 3         gPalette_446
 *   Western Woods    area  3 room 0         gPalette_446  (same palette)
 *
 * The source rooms are gUnk_080B4410's five entries, which is where the
 * beanstalk subtask itself gets them. Indexed here by area-13 room: 0-4 are
 * the tops and 16-20 the climbs, in the same order. */
static u16 BeanstalkGroundPalette(u32 room) {
    switch (room) {
        case ROOM_BEANSTALKS_CRENEL:
        case ROOM_BEANSTALKS_CRENEL + 16:
            return 550;
        case ROOM_BEANSTALKS_LAKE_HYLIA:
        case ROOM_BEANSTALKS_LAKE_HYLIA + 16:
            return 381;
        case ROOM_BEANSTALKS_RUINS:
        case ROOM_BEANSTALKS_RUINS + 16:
            return 537;
        case ROOM_BEANSTALKS_EASTERN_HILLS:
        case ROOM_BEANSTALKS_EASTERN_HILLS + 16:
        case ROOM_BEANSTALKS_WESTERN_WOODS:
        case ROOM_BEANSTALKS_WESTERN_WOODS + 16:
            return 446;
        default:
            return 0;
    }
}

void Port_Divergence_BeanstalkGroundPalette(void) {
    u16 palette;

    if (gRoomControls.area != AREA_BEANSTALKS) {
        return;
    }
    palette = BeanstalkGroundPalette(gRoomControls.room);
    if (palette == 0) {
        return;
    }
    /* Palette N is at N*32 in gGlobalGfxAndPalettes — the same arithmetic
     * LoadPaletteGroup's hardware path uses. */
    LoadPalettes(&gGlobalGfxAndPalettes[palette * 32], BEANSTALK_GROUND_SLOT, 1);
}

#else

void Port_Divergence_BeanstalkGroundPalette(void) {
}

#endif /* VIEWPORT_HEIGHT > DISPLAY_HEIGHT */
