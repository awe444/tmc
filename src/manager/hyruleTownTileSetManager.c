/**
 * @file hyruleTownTileSetManager.c
 * @ingroup Managers
 *
 * @brief Swap tileSet data in hyrule town depending on the position.
 */
#include "manager/hyruleTownTileSetManager.h"
#include "area.h"
#include "asm.h"
#include "flags.h"
#include "main.h"
#include "room.h"
#include "tiles.h"
#include "game.h"
#include "assets/gfx_offsets.h"
#include "player.h"
#include "viewport.h"
#ifdef PC_PORT
#include "port_tileset_residency.h"
#endif

void HyruleTownTileSetManager_UpdateLoadGfxGroups(HyruleTownTileSetManager*);
void HyruleTownTileSetManager_OnEnterRoom(HyruleTownTileSetManager*);

// clang-format off
static const u16 gHyruleTownTileSetManager_regions0[] = {
    0, 0x000, 0x000, 0x3f0, 0x200,
    1, 0x000, 0x280, 0x3f0, 0x140,
    0xff
};
static const u16 gHyruleTownTileSetManager_regions1[] = {
    2, 0x000, 0x000, 0x180, 0x3c0,
    3, 0x280, 0x000, 0x170, 0x3c0,
    0xff
};
static const u16 gHyruleTownTileSetManager_regions2[] = {
    5, 0x130, 0x1b0, 0x190, 0x140,
    4, 0x000, 0x000, 0x3f0, 0x3c0,
    0xff
};
static const u16 gHyruleTownTileSetManager_festivalRegions0[] = {
    0, 0x000, 0x000, 0x190, 0x1d0,
    1, 0x000, 0x2a0, 0x190, 0x120,
    0xff
};
static const u16 gHyruleTownTileSetManager_festivalRegions1[] = {
    0xff
};
static const u16 gHyruleTownTileSetManager_festivalRegions2[] = {
    5, 0x000, 0x1b0, 0x190, 0x140,
    4, 0x000, 0x000, 0x190, 0x3c0,
    0xff
};
// clang-format on

void HyruleTownTileSetManager_LoadGfxGroup(u32, u32);
void HyruleTownTileSetManager_BuildSecondOracleHouse(void);

bool32 HyruleTownTileSetManager_UpdateRoomGfxGroup(HyruleTownTileSetManager*, u32, u8*, const u16*);

extern u32 gUnk_086E8460;

typedef struct {
    u32 gfx1;
    void* dest1;
    u32 gfx2;
    void* dest2;
} HyruleTownTileSetManagerGfxInfo;

static const HyruleTownTileSetManagerGfxInfo gHyruleTownTileSetManagerGfxInfos[] = {
    { offset_gUnk_086D4460 + 0x8000, BG_SCREEN_ADDR(0), offset_gUnk_086D4460 + 0xE000, BG_SCREEN_ADDR(16) },
    { offset_gUnk_086D4460 + 0xB000, BG_SCREEN_ADDR(0), offset_gUnk_086D4460 + 0x11000, BG_SCREEN_ADDR(16) },
    { offset_gUnk_086D4460 + 0x9000, BG_SCREEN_ADDR(2), offset_gUnk_086D4460 + 0xF000, BG_SCREEN_ADDR(18) },
    { offset_gUnk_086D4460 + 0xC000, BG_SCREEN_ADDR(2), offset_gUnk_086D4460 + 0x12000, BG_SCREEN_ADDR(18) },
    { offset_gUnk_086D4460 + 0xA000, BG_SCREEN_ADDR(4), offset_gUnk_086D4460 + 0x10000, BG_SCREEN_ADDR(20) },
    { offset_gUnk_086D4460 + 0xD000, BG_SCREEN_ADDR(4), offset_gUnk_086D4460 + 0x13000, BG_SCREEN_ADDR(20) }
};
static const HyruleTownTileSetManagerGfxInfo gHyruleTownTileSetManagerGfxInfosFestival[] = {
    { offset_gUnk_086E8460 + 0x800, BG_SCREEN_ADDR(0), offset_gUnk_086E8460 + 0x6800, BG_SCREEN_ADDR(16) },
    { offset_gUnk_086E8460 + 0x3800, BG_SCREEN_ADDR(0), offset_gUnk_086E8460 + 0x9800, BG_SCREEN_ADDR(16) },
    { offset_gUnk_086E8460 + 0x1800, BG_SCREEN_ADDR(2), offset_gUnk_086E8460 + 0x7800, BG_SCREEN_ADDR(18) },
    { offset_gUnk_086E8460 + 0x4800, BG_SCREEN_ADDR(2), offset_gUnk_086E8460 + 0xA800, BG_SCREEN_ADDR(18) },
    { offset_gUnk_086E8460 + 0x2800, BG_SCREEN_ADDR(4), offset_gUnk_086E8460 + 0x8800, BG_SCREEN_ADDR(20) },
    { offset_gUnk_086E8460 + 0x5800, BG_SCREEN_ADDR(4), offset_gUnk_086E8460 + 0xB800, BG_SCREEN_ADDR(20) },
};
#ifdef PC_PORT
extern const u8* gGlobalGfxAndPalettes;
#else
extern const u8 gGlobalGfxAndPalettes[];
#endif

#if VIEWPORT_TILESET_RESIDENCY
/* Which region table drives a slot. Festival town runs the same three slots
 * from its own tables, with slot 1's list empty. */
static const u16* HyruleTownTileSetManager_RegionsFor(u32 gfxIndex) {
    if (gRoomControls.area != AREA_FESTIVAL_TOWN) {
        switch (gfxIndex) {
            case 0:
                return gHyruleTownTileSetManager_regions0;
            case 1:
                return gHyruleTownTileSetManager_regions1;
            default:
                return gHyruleTownTileSetManager_regions2;
        }
    }
    switch (gfxIndex) {
        case 0:
            return gHyruleTownTileSetManager_festivalRegions0;
        case 1:
            return gHyruleTownTileSetManager_festivalRegions1;
        default:
            return gHyruleTownTileSetManager_festivalRegions2;
    }
}

/* Make both halves of a slot's pair reachable.
 *
 * The gfx-info table lists slot i's two alternatives at 2i and 2i+1 and both
 * write to the same two destinations, so the pair is one bit. The group that
 * is already in VRAM keeps reading it; the other is copied into its bank, and
 * the renderer picks per tile. */
static void HyruleTownTileSetManager_MakeGroupPairResident(u32 gfxIndex, u32 gfxGroup, u32 residentGroup) {
    const HyruleTownTileSetManagerGfxInfo* infos;
    PortTilesetBlock blocks[4];
    u32 pair[2];
    u32 i;

    if (gRoomControls.area != AREA_FESTIVAL_TOWN) {
        infos = gHyruleTownTileSetManagerGfxInfos;
    } else {
        infos = gHyruleTownTileSetManagerGfxInfosFestival;
    }
    pair[0] = gfxGroup;
    pair[1] = gfxGroup ^ 1;
    for (i = 0; i < 2; i++) {
        const HyruleTownTileSetManagerGfxInfo* info = &infos[pair[i]];
        blocks[i * 2].group = pair[i];
        blocks[i * 2].src = &gGlobalGfxAndPalettes[info->gfx1];
        blocks[i * 2].dest = info->dest1;
        blocks[i * 2].size = BG_SCREEN_SIZE * 2;
        blocks[i * 2 + 1].group = pair[i];
        blocks[i * 2 + 1].src = &gGlobalGfxAndPalettes[info->gfx2];
        blocks[i * 2 + 1].dest = info->dest2;
        blocks[i * 2 + 1].size = BG_SCREEN_SIZE * 2;
    }
    Port_TilesetResidency_DeclareSlot(gfxIndex, HyruleTownTileSetManager_RegionsFor(gfxIndex),
                                      residentGroup, blocks, 4);
}

/* Declare a slot the camera never selected.
 *
 * UpdateRoomGfxGroup only loads a slot whose regions CheckRegionsOnScreen
 * matches, and B26 made that test the centred DISPLAY_WIDTH x DISPLAY_HEIGHT
 * sub-rect — the screen the GBA would have had. So a slot whose scenery is
 * only ever in the outer 40 px is never loaded, gRoomVars.graphicsGroups
 * keeps the 0xff OnEnterRoom put there, and LoadGfxGroup — the only thing
 * that declares a slot — never runs for it. Those tiles then read whatever
 * the previous room left in that VRAM range.
 *
 * On hardware that is unreachable and therefore harmless: the 240x160 screen
 * cannot show the scenery in question, which is why the engine is right not
 * to load it. Above native size it is on screen from the moment the room
 * loads, and pops to its proper tiles the instant the camera moves far enough
 * for the region test to match.
 *
 * Declare the pair anyway. No group is resident, because this room put
 * neither of them in the GBA's own VRAM — unlike the loaded case, where the
 * resident group is named so the second oracle house survives. Tiles in the
 * authored gaps between regions still fall back to VRAM (their group is
 * 0xff, which PublishForBg reads as "no bank"), so this only ever adds an
 * answer where the region tables have one. */
static void HyruleTownTileSetManager_DeclareUnselectedSlot(u32 gfxIndex) {
    if (gRoomVars.graphicsGroups[gfxIndex] != 0xff) {
        return; /* loaded, so LoadGfxGroup already declared it */
    }
    /* The gfx-info table lists slot i's alternatives at 2i and 2i+1, and
     * MakeGroupPairResident takes either of a pair. */
    HyruleTownTileSetManager_MakeGroupPairResident(gfxIndex, gfxIndex * 2, PORT_TILESET_NO_RESIDENT);
}
#endif

void HyruleTownTileSetManager_Main(HyruleTownTileSetManager* this) {
    if (super->action == 0) {
        super->action = 1;
        this->gfxGroup2 = 0xff;
        this->gfxGroup1 = 0xff;
        this->gfxGroup0 = 0xff;
        RegisterTransitionHandler(this, HyruleTownTileSetManager_OnEnterRoom, NULL);
        SetEntityPriority((Entity*)this, PRIO_PLAYER_EVENT);
#if VIEWPORT_TILESET_RESIDENCY
        /* Drop the previous room's pairs — but only if they *are* the previous
         * room's. This entity is created a frame after OnEnterRoom has already
         * declared the new room's slots, and resetting unconditionally threw
         * those away. Nothing re-declares a loaded slot except a group change,
         * so they stayed gone until the camera crossed a region threshold, and
         * until then every tile in their character range read whatever the
         * centred 240x160's own group had left in VRAM (B31). */
        if (!Port_TilesetResidency_SlotDeclared(0)) {
            Port_TilesetResidency_Reset();
        }
#endif
    }
    HyruleTownTileSetManager_UpdateLoadGfxGroups(this);
}

void HyruleTownTileSetManager_OnEnterRoom(HyruleTownTileSetManager* this) {
    gRoomVars.graphicsGroups[2] = 0xff;
    gRoomVars.graphicsGroups[1] = 0xff;
    gRoomVars.graphicsGroups[0] = 0xff;
    this->gfxGroup2 = 0xff;
    this->gfxGroup1 = 0xff;
    this->gfxGroup0 = 0xff;
#if VIEWPORT_TILESET_RESIDENCY
    /* The room is about to reload every slot, so the pairs the shadow bank
     * holds are the previous room's. Drop them before the loads below, which
     * are the ones that decide the new resident groups. */
    Port_TilesetResidency_Reset();
#endif
    HyruleTownTileSetManager_UpdateLoadGfxGroups(this);
}

void HyruleTownTileSetManager_UpdateLoadGfxGroups(HyruleTownTileSetManager* this) {
    if (gRoomControls.area != AREA_FESTIVAL_TOWN) {
        if (HyruleTownTileSetManager_UpdateRoomGfxGroup(this, 0, &this->gfxGroup0,
                                                        gHyruleTownTileSetManager_regions0) != 0) {
            HyruleTownTileSetManager_LoadGfxGroup(0, this->gfxGroup0);
        }
        if (HyruleTownTileSetManager_UpdateRoomGfxGroup(this, 1, &this->gfxGroup1,
                                                        gHyruleTownTileSetManager_regions1) != 0) {
            HyruleTownTileSetManager_LoadGfxGroup(1, this->gfxGroup1);
            if (this->gfxGroup1 == 2) {
                HyruleTownTileSetManager_BuildSecondOracleHouse();
            }
        }
        if (HyruleTownTileSetManager_UpdateRoomGfxGroup(this, 2, &this->gfxGroup2,
                                                        gHyruleTownTileSetManager_regions2) != 0) {
            HyruleTownTileSetManager_LoadGfxGroup(2, this->gfxGroup2);
        }
#if VIEWPORT_TILESET_RESIDENCY
        HyruleTownTileSetManager_DeclareUnselectedSlot(0);
        HyruleTownTileSetManager_DeclareUnselectedSlot(1);
        HyruleTownTileSetManager_DeclareUnselectedSlot(2);
#endif
    } else {
        if (HyruleTownTileSetManager_UpdateRoomGfxGroup(this, 0, &this->gfxGroup0,
                                                        gHyruleTownTileSetManager_festivalRegions0) != 0) {
            HyruleTownTileSetManager_LoadGfxGroup(0, this->gfxGroup0);
        }
        if (HyruleTownTileSetManager_UpdateRoomGfxGroup(this, 2, &this->gfxGroup2,
                                                        gHyruleTownTileSetManager_festivalRegions2) != 0) {
            HyruleTownTileSetManager_LoadGfxGroup(2, this->gfxGroup2);
        }
#if VIEWPORT_TILESET_RESIDENCY
        /* Slot 1 is left out here as it is above: festival town's list for it
         * is empty, so there are no regions to answer with. */
        HyruleTownTileSetManager_DeclareUnselectedSlot(0);
        HyruleTownTileSetManager_DeclareUnselectedSlot(2);
#endif
    }
}

void HyruleTownTileSetManager_BuildSecondOracleHouse(void) {
    u32 loopVar;
    u32 innerLoopVar;

    if (CheckGlobalFlag(TATEKAKE_HOUSE) != 0) {
        for (loopVar = 0; loopVar < 4; ++loopVar) {
            for (innerLoopVar = 0; innerLoopVar < 4; ++innerLoopVar) {
                SetTileByIndex(loopVar * 0x10 + TILE_TYPE_1195 + innerLoopVar,
                               TILE_LOCAL(0x28 + innerLoopVar * 0x10, 0x188 + loopVar * 0x10), 1);
            }
        }

        for (loopVar = 0; loopVar < 3; ++loopVar) {
            for (innerLoopVar = 0; innerLoopVar < 4; ++innerLoopVar) {
                SetTileByIndex(loopVar * 0x10 + TILE_TYPE_1088 + innerLoopVar,
                               TILE_LOCAL(0x28 + innerLoopVar * 0x10, 0x188 + loopVar * 0x10), 2);
            }
        }
        SetTileByIndex(TILE_TYPE_214, TILE_POS(2, 23), LAYER_TOP);
        SetTileByIndex(TILE_TYPE_215, TILE_POS(3, 23), LAYER_TOP);
        LoadResourceAsync(&gUnk_086E8460, BG_SCREEN_ADDR(3), BG_SCREEN_SIZE);
    } else {
        if (CheckGlobalFlag(TATEKAKE_TOCHU) != 0) {
            for (loopVar = 0; loopVar < 5; ++loopVar) {
                for (innerLoopVar = 0; innerLoopVar < 4; ++innerLoopVar) {
                    SetTileByIndex(loopVar * 0x10 + TILE_TYPE_1190 + innerLoopVar,
                                   TILE_LOCAL(0x28 + innerLoopVar * 0x10, 0x188 + loopVar * 0x10), 1);
                }
            }
            SetTileByIndex(TILE_TYPE_1092, TILE_POS(2, 24), LAYER_TOP);
            SetTileByIndex(TILE_TYPE_1093, TILE_POS(5, 24), LAYER_TOP);
            SetTileByIndex(TILE_TYPE_1108, TILE_POS(2, 25), LAYER_TOP);
            SetTileByIndex(TILE_TYPE_1109, TILE_POS(5, 25), LAYER_TOP);
        }
    }
}

bool32 HyruleTownTileSetManager_UpdateRoomGfxGroup(HyruleTownTileSetManager* this, u32 gfxIndex, u8* pGfxGroup,
                                                   const u16* regions) {
    *pGfxGroup = CheckRegionsOnScreen(regions);
    if ((*pGfxGroup != 0xff) && (gRoomVars.graphicsGroups[gfxIndex] != *pGfxGroup)) {
        gRoomVars.graphicsGroups[gfxIndex] = *pGfxGroup;
        return TRUE;
    }
    return FALSE;
}

void HyruleTownTileSetManager_LoadGfxGroup(u32 gfxIndex, u32 gfxGroup) {
    const HyruleTownTileSetManagerGfxInfo* gfxInfo;

    gRoomVars.graphicsGroups[gfxIndex] = gfxGroup;
    if (gRoomControls.area != AREA_FESTIVAL_TOWN) {
        gfxInfo = &gHyruleTownTileSetManagerGfxInfos[gfxGroup];
    } else {
        gfxInfo = &gHyruleTownTileSetManagerGfxInfosFestival[gfxGroup];
    }
    LoadResourceAsync(&gGlobalGfxAndPalettes[gfxInfo->gfx1], gfxInfo->dest1, BG_SCREEN_SIZE * 2);
    LoadResourceAsync(&gGlobalGfxAndPalettes[gfxInfo->gfx2], gfxInfo->dest2, BG_SCREEN_SIZE * 2);
#if VIEWPORT_TILESET_RESIDENCY
    /* Put the alternative beside it, so the renderer can pick per tile.
     *
     * The camera-driven swap above is deliberately left running rather than
     * disabled: whichever group it has just loaded is the one in the GBA's
     * own VRAM, and re-pairing here makes that fact true by construction on
     * every swap. Suppressing the swap instead means carrying state that
     * says which group is resident, and that state goes stale on any path
     * that reloads VRAM without telling the manager — which is what an
     * earlier attempt at this did, silently losing a slot mid-room. B27. */
    HyruleTownTileSetManager_MakeGroupPairResident(gfxIndex, gfxGroup, gfxGroup);
#endif
}

void TryLoadPrologueHyruleTown(void) {
    u32 gfxGroup;

#if VIEWPORT_TILESET_RESIDENCY
    Port_TilesetResidency_Reset();
#endif
    if (gRoomControls.area != AREA_FESTIVAL_TOWN) {
        gfxGroup = CheckRegionsOnScreen(gHyruleTownTileSetManager_regions0);
        if (gfxGroup != 0xff) {
            HyruleTownTileSetManager_LoadGfxGroup(0, gfxGroup);
        }
        gfxGroup = CheckRegionsOnScreen(gHyruleTownTileSetManager_regions1);
        if (gfxGroup != 0xff) {
            HyruleTownTileSetManager_LoadGfxGroup(1, gfxGroup);
            if (gfxGroup == 2) {
                HyruleTownTileSetManager_BuildSecondOracleHouse();
            }
        }
        gfxGroup = CheckRegionsOnScreen(gHyruleTownTileSetManager_regions2);
        if (gfxGroup != 0xff) {
            HyruleTownTileSetManager_LoadGfxGroup(2, gfxGroup);
        }
    } else {
        gfxGroup = CheckRegionsOnScreen(gHyruleTownTileSetManager_festivalRegions0);
        if (gfxGroup != 0xff) {
            HyruleTownTileSetManager_LoadGfxGroup(0, gfxGroup);
        }
        gfxGroup = CheckRegionsOnScreen(gHyruleTownTileSetManager_festivalRegions2);
        if (gfxGroup != 0xff) {
            HyruleTownTileSetManager_LoadGfxGroup(2, gfxGroup);
        }
    }
}
