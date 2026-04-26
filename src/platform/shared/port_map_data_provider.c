#ifdef __PORT__

#include "global.h"
#include "flags.h"
#include "fade.h"
#include "map.h"
#include "menu.h"
#include "sound.h"
#include "subtask.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef TMC_BASEROM_PATH
#define TMC_BASEROM_PATH ""
#endif

#define PORT_AREA_COUNT 0x99u
#define PORT_MAX_ROOMS_PER_AREA 256u
#define PORT_ROM_BASE_ADDR 0x08000000u

/* USA baserom label addresses from data/map headers comments. */
#if defined(USA) || defined(DEMO_USA)
#define PORT_OFFS_GAREATABLE 0x000D50FCu
#define PORT_OFFS_GAREATILESETS 0x0010246Cu
#define PORT_OFFS_GAREATILES 0x0010309Cu
#define PORT_OFFS_GAREAROOMMAPS 0x00107988u
#define PORT_OFFS_GAREAROOMHEADERS 0x0011E214u
#else
/* Other variants are intentionally left disabled until verified offsets are added. */
#define PORT_OFFS_GAREATABLE 0u
#define PORT_OFFS_GAREATILESETS 0u
#define PORT_OFFS_GAREATILES 0u
#define PORT_OFFS_GAREAROOMMAPS 0u
#define PORT_OFFS_GAREAROOMHEADERS 0u
#endif

extern u8 gMapData[];
extern void** gAreaRoomMaps[];
extern void*** gAreaTable[];
extern void** gAreaTileSets[];
extern void* gAreaTiles[];

static void* sPortRoomMapsByArea[PORT_AREA_COUNT][PORT_MAX_ROOMS_PER_AREA];
static void* sPortTileSetsByArea[PORT_AREA_COUNT][PORT_MAX_ROOMS_PER_AREA];
static void* sPortPropertiesByAreaRoom[PORT_AREA_COUNT][PORT_MAX_ROOMS_PER_AREA][8];
static void** sPortAreaTableByArea[PORT_AREA_COUNT][PORT_MAX_ROOMS_PER_AREA];
static MapDataDefinition sPortSafeTiles[] = {
    { .src = 0u, .dest = NULL, .size = 0u },
};

static uint32_t port_read_u32(const uint8_t* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int port_off_range_ok(uint32_t off, uint32_t size) {
    const uint32_t kMapDataSize = 16u * 1024u * 1024u;
    return off <= kMapDataSize && size <= kMapDataSize - off;
}

static uint8_t* port_rom_ptr_to_host(uint32_t rom_ptr) {
    if (rom_ptr < PORT_ROM_BASE_ADDR) {
        return NULL;
    }
    uint32_t off = rom_ptr - PORT_ROM_BASE_ADDR;
    if (!port_off_range_ok(off, 1u)) {
        return NULL;
    }
    return &gMapData[off];
}

static uint8_t* port_rom_off_to_host(uint32_t rom_off) {
    return &gMapData[rom_off];
}

static uint32_t port_count_rooms(const uint8_t* room_headers_base) {
    uint32_t room = 0;
    uint32_t base_off = (uint32_t)(room_headers_base - gMapData);
    while (room < PORT_MAX_ROOMS_PER_AREA) {
        const uint8_t* hdr = room_headers_base + room * 10u;
        if (!port_off_range_ok(base_off + room * 10u, 2u)) {
            break;
        }
        uint16_t marker = (uint16_t)hdr[0] | ((uint16_t)hdr[1] << 8);
        if (marker == 0xFFFFu) {
            break;
        }
        room++;
    }
    return room;
}

static uint32_t port_read_rom_u32(uint32_t rom_ptr, uint32_t word_index) {
    uint8_t* p = port_rom_ptr_to_host(rom_ptr);
    if (p == NULL) {
        return 0u;
    }
    if (!port_off_range_ok((uint32_t)(p - gMapData) + word_index * 4u, 4u)) {
        return 0u;
    }
    return port_read_u32(p + word_index * 4u);
}

/** Read one little-endian u32 from a host pointer inside `gMapData[]`. */
static uint32_t port_read_u32_host(const uint8_t* p) {
    if (p == NULL) {
        return 0u;
    }
    if (!port_off_range_ok((uint32_t)(p - (const uint8_t*)gMapData), 4u)) {
        return 0u;
    }
    return port_read_u32(p);
}

static uint16_t port_room_header_tile_set_id(const uint8_t* hdr10) {
    return (uint16_t)hdr10[8] | ((uint16_t)hdr10[9] << 8);
}

extern u32 sub_unk3_HouseInteriors2_LinksHouseBedroom(void);
extern void sub_StateChange_HouseInteriors2_LinksHouseBedroom(void);

static void* port_translate_room_callback(uint32_t rom_ptr) {
    /* Room property callback pointers in entity_headers.s are THUMB
     * function addresses (LSB set). Match on the full value first, then
     * on the aligned address for safety. */
    switch (rom_ptr) {
        case 0x0804E7D9u:
            return (void*)sub_unk3_HouseInteriors2_LinksHouseBedroom;
        case 0x0804E7DDu:
            return (void*)sub_StateChange_HouseInteriors2_LinksHouseBedroom;
        default:
            break;
    }
    switch (rom_ptr & ~1u) {
        case 0x0804E7D8u:
            return (void*)sub_unk3_HouseInteriors2_LinksHouseBedroom;
        case 0x0804E7DCu:
            return (void*)sub_StateChange_HouseInteriors2_LinksHouseBedroom;
        default:
            return NULL;
    }
}

static int port_load_baserom_into_mapdata(void) {
    if (TMC_BASEROM_PATH[0] == '\0') {
        return -1;
    }
    FILE* f = fopen(TMC_BASEROM_PATH, "rb");
    if (f == NULL) {
        return -1;
    }
    size_t n = fread(gMapData, 1, 16u * 1024u * 1024u, f);
    int ok = (n == 16u * 1024u * 1024u);
    fclose(f);
    return ok ? 0 : -1;
}

void Port_MapDataInit(void) {
    uint32_t area;
    static int initialised = 0;
    if (initialised) {
        return;
    }
    initialised = 1;

    if (PORT_OFFS_GAREATABLE == 0u) {
        return;
    }
    if (port_load_baserom_into_mapdata() != 0) {
        return;
    }

    for (area = 0; area < PORT_AREA_COUNT; ++area) {
        uint32_t room;
        uint32_t room_count;
        uint32_t headers_ptr = port_read_u32(port_rom_off_to_host(PORT_OFFS_GAREAROOMHEADERS + area * 4u));
        uint32_t maps_ptr = port_read_u32(port_rom_off_to_host(PORT_OFFS_GAREAROOMMAPS + area * 4u));
        uint32_t props_ptr = port_read_u32(port_rom_off_to_host(PORT_OFFS_GAREATABLE + area * 4u));
        uint32_t tile_sets_ptr = port_read_u32(port_rom_off_to_host(PORT_OFFS_GAREATILESETS + area * 4u));
        uint32_t tiles_ptr = port_read_u32(port_rom_off_to_host(PORT_OFFS_GAREATILES + area * 4u));

        gAreaRoomHeaders[area] = (void**)port_rom_ptr_to_host(headers_ptr);
        if (tiles_ptr != 0u) {
            gAreaTiles[area] = (void*)port_rom_ptr_to_host(tiles_ptr);
        } else {
            gAreaTiles[area] = (void*)sPortSafeTiles;
        }

        if (gAreaRoomHeaders[area] == NULL) {
            gAreaRoomMaps[area] = NULL;
            gAreaTileSets[area] = NULL;
            gAreaTable[area] = NULL;
            continue;
        }

        memset(sPortRoomMapsByArea[area], 0, sizeof(sPortRoomMapsByArea[area]));
        memset(sPortTileSetsByArea[area], 0, sizeof(sPortTileSetsByArea[area]));

        room_count = port_count_rooms((const uint8_t*)gAreaRoomHeaders[area]);
        for (room = 0; room < room_count; ++room) {
            if (props_ptr != 0u) {
                uint32_t prop_list_ptr = port_read_rom_u32(props_ptr, room);
                uint8_t* prop_list_host = port_rom_ptr_to_host(prop_list_ptr);
                uint32_t k;
                if (prop_list_host == NULL) {
                    sPortAreaTableByArea[area][room] = NULL;
                    continue;
                }
                for (k = 0; k < 4u; ++k) {
                    uint32_t p = port_read_u32(prop_list_host + k * 4u);
                    sPortPropertiesByAreaRoom[area][room][k] = (void*)port_rom_ptr_to_host(p);
                }
                /* Property slots 4..7 are callback function pointers in ROM
                 * text space. Bridge only the known decompiled callbacks to
                 * host function pointers; leave the rest NULL. */
                for (k = 4u; k < 8u; ++k) {
                    uint32_t p = port_read_u32(prop_list_host + k * 4u);
                    sPortPropertiesByAreaRoom[area][room][k] = port_translate_room_callback(p);
                }
                sPortAreaTableByArea[area][room] = sPortPropertiesByAreaRoom[area][room];
            } else {
                sPortAreaTableByArea[area][room] = NULL;
            }
        }

        /* `gAreaRoomMaps[area]` is a pointer table: one ROM pointer per room
         * to a `map_bottom` / `map_top` MapDataDefinition chain (see
         * `data/map/map_headers.s`). Without resolving these, every room's
         * `RoomResInfo.map` stays NULL and `LoadMapData` either no-ops or
         * walks garbage — the usual symptom is an apparent hang right after
         * file select when `LoadRoomGfx` / `LoadRoomTileSet` run. */
        if (maps_ptr != 0u) {
            const uint8_t* maps_tbl = (const uint8_t*)port_rom_ptr_to_host(maps_ptr);
            for (room = 0; room < room_count; ++room) {
                uint32_t map_chain_rom = 0u;
                if (maps_tbl != NULL) {
                    map_chain_rom = port_read_u32_host(maps_tbl + room * 4u);
                }
                sPortRoomMapsByArea[area][room] = (void*)port_rom_ptr_to_host(map_chain_rom);
            }
        }

        /* `gAreaTileSets[area]` is indexed by `RoomHeader.tileSet_id`, not by
         * room index (see `data/map/tileset_headers.s`). */
        if (tile_sets_ptr != 0u) {
            const uint8_t* headers_base = (const uint8_t*)gAreaRoomHeaders[area];
            uint32_t max_ts = 0u;
            uint32_t tid;
            for (room = 0; room < room_count; ++room) {
                const uint8_t* rh = headers_base + room * 10u;
                uint16_t ts_id = port_room_header_tile_set_id(rh);
                if (ts_id != 0xFFFFu && ts_id > max_ts) {
                    max_ts = ts_id;
                }
            }
            /* Always include slot 0 when the table is non-empty — some areas
             * use only tile set 0. Cap by the fixed host array width. */
            if (max_ts >= PORT_MAX_ROOMS_PER_AREA) {
                max_ts = PORT_MAX_ROOMS_PER_AREA - 1u;
            }
            {
                const uint8_t* ts_tbl = (const uint8_t*)port_rom_ptr_to_host(tile_sets_ptr);
                for (tid = 0; tid <= max_ts && tid < PORT_MAX_ROOMS_PER_AREA; ++tid) {
                    uint32_t tileset_blob_rom = 0u;
                    if (ts_tbl != NULL) {
                        tileset_blob_rom = port_read_u32_host(ts_tbl + tid * 4u);
                    }
                    sPortTileSetsByArea[area][tid] = (void*)port_rom_ptr_to_host(tileset_blob_rom);
                }
            }
        }

        gAreaRoomMaps[area] = sPortRoomMapsByArea[area];
        gAreaTileSets[area] = sPortTileSetsByArea[area];
        gAreaTable[area] = sPortAreaTableByArea[area];
    }
}

#endif /* __PORT__ */
