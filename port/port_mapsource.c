/* See port_mapsource.h. Kept in C because it reads engine headers, which
 * do not parse as C++ (they use `this` as a parameter name). */
#include "port_mapsource.h"

#include <stdio.h>
#include <stdlib.h>

#include "gba/gba.h"
#include "main.h"
#include "game.h"
#include "map.h"
#include "room.h"
#include "screen.h"
#include "subtask.h"
#include "ui.h"
#include "tileMap.h"
#include "vram.h"

#include "port_gba_mem.h"
#include "port_tileset_residency.h"
#include "cpu/mode1.h"
#include "viewport.h"

/* Which BG a map layer displays through is *per room*, not fixed: the
 * engine records it in MapLayer.bgSettings, which points at one of
 * gScreen.bg0..bg3 (e.g. beanstalkSubtask.c:151-153 binds bottom->bg2,
 * top->bg1 on the ordinary room-init path, but other paths differ).
 * Deriving the index from that pointer is what makes the binding correct
 * in every room; hardcoding the common case renders the wrong layer's
 * tilemap wherever a room deviates. */
static int mapsource_bg_index(const BgSettings* settings) {
    if (settings == (const BgSettings*)&gScreen.bg0) {
        return 0;
    }
    if (settings == (const BgSettings*)&gScreen.bg1) {
        return 1;
    }
    if (settings == (const BgSettings*)(const void*)&gScreen.bg2) {
        return 2;
    }
    if (settings == (const BgSettings*)(const void*)&gScreen.bg3) {
        return 3;
    }
    return -1;
}

/* The staging buffer each BG index normally streams from; a layer pointed
 * anywhere else is screenblock-shaped data, not a 0x80-stride room map. */
static const void* mapsource_expected_buffer(int bg_index) {
    switch (bg_index) {
        case 0: return &gBG0Buffer;
        case 1: return &gBG1Buffer;
        case 2: return &gBG2Buffer;
        case 3: return &gBG3Buffer;
        default: return NULL;
    }
}

static const void* mapsource_sub_tile_map(int bg_index) {
    switch (bg_index) {
        case 0: return gScreen.bg0.subTileMap;
        case 1: return gScreen.bg1.subTileMap;
        case 2: return gScreen.bg2.subTileMap;
        case 3: return gScreen.bg3.subTileMap;
        default: return NULL;
    }
}

/* Special-map geometry (include/tileMap.h): u16[0x4000] addressed as a
 * 128x128 grid of 8x8 tiles at a 0x80 row stride = 1024x1024 px, which
 * covers the largest room in the game (1024x1008). */
#define MAPSRC_STRIDE 0x80
#define MAPSRC_MAX_TILES 128

enum {
    REASON_BOUND = -1,
    REASON_TASK = 0,
    REASON_SUBSTATE,
    REASON_SCROLL_FLAGS,
    REASON_TRANSITION,
    REASON_REBOUND,
    REASON_NO_LAYER,
    REASON_GEOMETRY,
    REASON_DISABLED,
    REASON_COUNT
};

static const char* const kReasonName[REASON_COUNT] = {
    "task!=GAME", "substate!=UPDATE", "scroll_flags&1", "mid-transition",
    "subTileMap rebound", "layer off", "bad geometry", "disabled",
};

static bool sEnabled = true;
static int sLastReason[2] = { REASON_DISABLED, REASON_DISABLED };
static unsigned sBoundFrames[2];
static unsigned sRejectFrames[2][REASON_COUNT];

void Port_MapSource_SetEnabled(bool enabled) {
    sEnabled = enabled;
}

bool Port_MapSource_Enabled(void) {
    return sEnabled;
}

const char* Port_MapSource_ReasonName(int reason) {
    if (reason == REASON_BOUND) {
        return "bound";
    }
    if (reason < 0 || reason >= REASON_COUNT) {
        return "?";
    }
    return kReasonName[reason];
}

int Port_MapSource_LastReason(int layer) {
    return sLastReason[layer & 1];
}

/* The predicate. Each clause corresponds to an exclusion class established
 * statically in Spike 2 and confirmed over 7.6M runtime tile comparisons:
 *
 *   task/substate  - menu, title, file-select and subtask scratch uses
 *                    (fileselect save struct, kinstone array, pause-menu
 *                    dungeon bitmap, Gyorg collision bitmap)
 *   scroll_flags&1 - "degraded" rooms whose map came from the 0xffff
 *                    sentinel path: built 512x512 by sub_0807C5F4 and not
 *                    updated by the tile mutators
 *   scrollAction   - room-to-room transition scrollers blend two rooms
 *   subTileMap     - background managers that point a BG at the arrays as
 *                    screenblock-shaped data (bigGoron, minish paths,
 *                    minish rafters) rather than as a 0x80-stride map
 *   bgSettings     - layer detached entirely (e.g. weatherChangeManager
 *                    fog phase)
 */
static bool mapsource_is_ui_screen(void);
/* The same answer, held across a transition until switching it is invisible.
 * Defined with the clip it exists for; see mapsource_ui_latch_update. */
static bool mapsource_ui_screen_stable(void);
#if UI_CENTER_DX > 0 || UI_CENTER_DY > 0
static void mapsource_ui_latch_update(void);
#endif

static int mapsource_reason(int layer) {
    if (!sEnabled) {
        return REASON_DISABLED;
    }
    {   /* TMC_MAPSRC_LAYERS=0|1|2(both) — bisection aid */
        const char* only = getenv("TMC_MAPSRC_LAYERS");
        if (only != NULL) {
            int want = only[0] - '0';
            if (want != 2 && want != layer) {
                return REASON_DISABLED;
            }
        }
    }
    if (gMain.task != TASK_GAME) {
        return REASON_TASK;
    }
    /* Ordinary gameplay, and also the subtasks that are *views of the
     * world* — cutscenes and world events. Those call UpdateScrollVram
     * (subtaskAuxCutscene.c:85, subtaskWorldEvent.c:57), so the special
     * maps are live and full-room during them, and they must fill the
     * viewport rather than be centred: leaving them on the screenblock
     * path made the opening cutscenes wrap and repeat past column 240.
     * Menu subtasks are excluded — that is where the arrays get
     * repurposed as scratch (Spike 2 §5.1). */
    if (gMain.substate != GAMEMAIN_UPDATE) {
#if VIEWPORT_WIDTH > DISPLAY_WIDTH
        /* Only a wider viewport needs this. At GBA-native size the
         * screenblock path already covers the whole screen, and it is the
         * behaviour verified bit-identical to the pre-expansion build, so
         * do not change what the shipping build renders. */
        /* The two room-change substates keep their map source above native
         * size, and this is only safe because of the transition fade.
         *
         * Both render the room while a fade runs over it -- CHANGEROOM after
         * the swap, CHANGEAREA before it -- and neither maintains the VRAM
         * screenblock while doing so. At 240x160 that costs nothing: the
         * screenblock covers the screen and holds the room. Above it, dropping
         * the map source falls back to a screenblock that was never kept
         * current, and what fades is a stale slice on black -- the room's
         * furniture drawn as sprites over nothing, which is how it was
         * reported.
         *
         * The reason this could not be done before VIEWPORT_SCROLL_FADE: a
         * sliding CHANGEROOM has the camera between two rooms, and one map
         * source renders one room, so binding filled part of the frame and
         * left the rest backdrop (B5, measured at 14.5% against 12.0%). With
         * the slide replaced by a fade the camera is always at rest on a
         * single room by the time anything is visible, so the map source is
         * exactly right. */
        if (gMain.substate == GAMEMAIN_CHANGEROOM || gMain.substate == GAMEMAIN_CHANGEAREA) {
            /* fall through to the geometry checks */
        } else if (gMain.substate != GAMEMAIN_SUBTASK || mapsource_ui_screen_stable()) {
            return REASON_SUBSTATE;
        }
#else
        return REASON_SUBSTATE;
#endif
    }
    if (gRoomControls.scroll_flags & 1) {
#if VIEWPORT_MAINTAIN_DEGRADED_MAP
        /* A degraded room -- map built by the 0xffff sentinel path. Excluded
         * on hardware because the tile mutators do not maintain that map, so
         * sampling it would render a mutated tile as its original.
         *
         * Above native size the exclusion costs more than it saves: the
         * screenblock covers 256 px and cannot fill the viewport, so refusing
         * here draws the room as sprites over black (B17, measured at 5.4% of
         * the frame against 99.5% outside). The mutators maintain the map at
         * this viewport -- see VIEWPORT_MAINTAIN_DEGRADED_MAP -- which removes
         * the reason for the exclusion, so bind it.
         *
         * sub_0807C5F4 builds this map into the same arrays at the same 0x80
         * stride the sampler reads, which is why it renders correctly at all. */
#else
        return REASON_SCROLL_FLAGS;
#endif
    }
    if (gRoomControls.scrollAction >= 2) {
        return REASON_TRANSITION;
    }
    {
        const BgSettings* settings =
            (layer == 0) ? gMapBottom.bgSettings : gMapTop.bgSettings;
        int bg;
        if (settings == NULL) {
            return REASON_NO_LAYER;
        }
        bg = mapsource_bg_index(settings);
        if (bg < 0) {
            return REASON_NO_LAYER;
        }
        if (mapsource_sub_tile_map(bg) != mapsource_expected_buffer(bg)) {
            return REASON_REBOUND;
        }
    }
    /* Camera origin must be inside the room and the room must fit the
     * special-map grid; otherwise sampling would read outside the data. */
    {
        int cx = (int)gRoomControls.scroll_x - (int)gRoomControls.origin_x;
        int cy = (int)gRoomControls.scroll_y - (int)gRoomControls.origin_y;
        int tw = gRoomControls.width >> 3;
        int th = gRoomControls.height >> 3;
        /* cx/cy may be negative: a room narrower (or shorter) than the
         * viewport is centred, which puts screen column 0 outside the room.
         * The PPU renders those columns as backdrop, so only the room
         * extent needs to be sane. */
        (void)cx;
        (void)cy;
        if (tw <= 0 || th <= 0 || tw > MAPSRC_MAX_TILES || th > MAPSRC_MAX_TILES) {
            return REASON_GEOMETRY;
        }
    }
    return REASON_BOUND;
}

bool Port_MapSource_LayerAuthoritative(int layer) {
    int r = mapsource_reason(layer & 1);
    sLastReason[layer & 1] = r;
    return r == REASON_BOUND;
}

/* UI centring (D1: screens and the text box are centered; the in-game HUD
 * is edge-anchored and handled separately).
 *
 * Content authored for a 240-wide screen is centred by clipping each UI
 * layer to a 240-px span starting UI_CENTER_DX in. A clip is used rather
 * than a map source because title and file select load their tilemaps
 * straight into VRAM screenblocks — a map source would read the staging
 * buffer, which for those screens is stale or empty. The clip works on the
 * layer's normal screenblock fetch, so it is agnostic about where the
 * content came from, and it suppresses the wrap that a plain scroll offset
 * would produce.
 *
 * BG0 during gameplay is deliberately *not* clipped: it carries the
 * edge-anchored HUD, authored out to the viewport edges. The text box also
 * lives on BG0 there and centres itself per-window (message.c). */
static bool sUiCentered = false;
static bool sAffineCentered = false;
static int sClippedBgMask = 0; /* which BGs the clip rule caught, for the trace */

bool Port_MapSource_UiCentered(void) {
    return sUiCentered;
}

static int sBg3ScreenAnchor = PORT_BG3_ANCHOR_NONE;
static u8 sBg3AnchorArea = 0xFF;
static u8 sBg3AnchorRoom = 0xFF;

static u32 sTiledOverlayMask = 0;
static u8 sTiledOverlayArea = 0xFF;
static u8 sTiledOverlayRoom = 0xFF;

void Port_MapSource_DeclareTiledOverlay(int bg_index) {
    if (bg_index < 0 || bg_index >= 4) {
        return;
    }
    if (gRoomControls.area != sTiledOverlayArea || gRoomControls.room != sTiledOverlayRoom) {
        sTiledOverlayMask = 0;
        sTiledOverlayArea = gRoomControls.area;
        sTiledOverlayRoom = gRoomControls.room;
    }
    sTiledOverlayMask |= 1u << bg_index;
}

void Port_MapSource_DeclareBg3ScreenAnchor(int anchor) {
    sBg3ScreenAnchor = anchor;
    sBg3AnchorArea = gRoomControls.area;
    sBg3AnchorRoom = gRoomControls.room;
}

/* The declaration's lifetime is the *overlay's*, not the declaring handler's.
 *
 * Clearing it every frame and requiring the handler to say so again was the
 * first attempt, and it was wrong in both directions the maintainer's
 * recordings found. A light-ray fade-out sets `unk_21` to the trigger type on
 * its very first frame, so `gUnk_08107C48` dispatches to `nullsub_494` and the
 * state-4 handler stops running while eighty frames of visible fade remain —
 * the band jumped 80 px left as it began to fade. And a text box suspends the
 * managers outright, which did the same thing for the 254 frames of a
 * conversation in the barrel minish house.
 *
 * The overlay ends when BG3 goes off (`LightRayManager_OnExitRoom` clears
 * `DISPCNT_BG3_ON`) or when the room changes. Those are the two conditions,
 * both observable here, and neither depends on anyone remembering to tick.
 * B30 and B31 are still the hazard on the other side — a declaration that
 * outlives what it describes — which is why a light state that wants the old
 * unclipped behaviour declares `PORT_BG3_ANCHOR_NONE` rather than going quiet
 * (`sub_080573AC`). Silence means "unchanged", not "off". */
static void mapsource_bg3_anchor_expire(void) {
    if (sBg3ScreenAnchor == PORT_BG3_ANCHOR_NONE) {
        return;
    }
    if ((gScreen.lcd.displayControl & 0x0800) == 0 ||
        gRoomControls.area != sBg3AnchorArea ||
        gRoomControls.room != sBg3AnchorRoom) {
        sBg3ScreenAnchor = PORT_BG3_ANCHOR_NONE;
    }
}

/* The room is the whole of a tiled-overlay declaration's lifetime: within one
 * room the layer either carries the overlay or has its map source back, and
 * the clip only applies in the first case. */
static void mapsource_tiled_overlay_expire(void) {
    if (sTiledOverlayMask != 0 &&
        (gRoomControls.area != sTiledOverlayArea || gRoomControls.room != sTiledOverlayRoom)) {
        sTiledOverlayMask = 0;
    }
}

bool Port_MapSource_AffineCentered(void) {
    return sAffineCentered;
}

/* Is the screen currently showing a full-screen surface that was *authored*
 * for a 240-wide display, as opposed to a view of the world?
 *
 * `substate != GAMEMAIN_UPDATE` is NOT the right test, which is what an
 * earlier version used. Cutscenes run as subtasks too, but they are views
 * of the world and must fill the viewport like gameplay does; centring them
 * shrinks the cutscene to 240 and shifts its sprites, which is what made
 * the opening cutscenes render narrow and misaligned.
 *
 * So discriminate on the subtask *type* (gUI.lastState, the value handed to
 * MenuFadeIn): menus are 240-authored UI, cutscenes and world events are
 * world views. Title and file select are not subtasks at all — they are
 * separate tasks — and are always UI. */
static bool mapsource_is_ui_screen(void) {
    if (gMain.task != TASK_GAME) {
        return true; /* title, file select, gameover, staffroll */
    }
    if (gMain.substate != GAMEMAIN_SUBTASK) {
        return false; /* ordinary gameplay */
    }
    switch (gUI.lastState) {
        case SUBTASK_PAUSEMENU:
        case SUBTASK_MAPHINT:
        case SUBTASK_KINSTONEMENU:
        case SUBTASK_FIGURINEMENU:
        case SUBTASK_LOCALMAPHINT:
            return true;
        case SUBTASK_AUXCUTSCENE:
            /* A cutscene is a view of the world and must fill the viewport
             * (B3) — except that one of them spends its first half showing
             * something else. The Picori legend's stained-glass cards are a
             * whole 240x160 authored panel, and they run as states 0..10 of
             * the *same* AUXCUTSCENE that later fades into Zelda walking
             * through Hyrule Field. So the subtask cannot decide this; it is
             * one subtask wearing two hats.
             *
             * What separates them is mechanical rather than a guess about
             * which cutscene is playing: a story panel has no world behind
             * it, and says so by detaching both map layers
             * (cutscene.c:230-231, gMapBottom/gMapTop.bgSettings = NULL).
             * With no layer bound there is no world view to fill, and what is
             * on screen is a 240-authored surface like any menu. SetBGDefaults
             * rebinds them when the cutscene switches to its world half, which
             * is what puts the hat back.
             *
             * This is only about the *vertical* shift. The panels already took
             * the horizontal one via the no-map-source clip rule, which is why
             * B2 and B9 are about them. */
            return gMapBottom.bgSettings == NULL && gMapTop.bgSettings == NULL;
        default:
            /* PORTALCUTSCENE, WORLDEVENT, FASTTRAVEL: world. */
            return false;
    }
}

/* B1: the text box centres itself per-window (message.c) because it shares
 * BG0 with the edge-anchored HUD during gameplay. On a UI screen the whole
 * BG0 layer is already shifted, so applying the per-window shift as well
 * moves the box twice — which is what clipped the "Saving file..." and
 * "Erasing file..." popups on the file-select and pause-menu screens. */
int Port_MapSource_MessageTileShift(void) {
    /* Always zero now. BG0 is a 32-column map, so shifting a ~28-column
     * text box by 5 columns would overflow the row; the whole layer is
     * centred instead, which carries the box with it. */
    return 0;
}

/* The vertical sibling, and unlike the x one it is *not* always zero.
 *
 * A world view's BG0 is not shifted vertically — it carries the HUD, which
 * stays at the top — so the text box has to take UI_CENTER_DY on its own to
 * keep its authored position inside the centred frame. A UI screen's BG0 *is*
 * shifted by exactly that, so a popup on one already travels with the layer
 * and adding this on top would move it twice. That is the same double-shift that B1
 * clipped the "Saving file..." and "Erasing file..." popups with, one axis
 * over: at 240 rows the second shift would push a popup past the 160-row
 * content span and the clip would delete it outright. */
int Port_MapSource_MessageTileShiftY(void) {
    return sUiCentered ? 0 : UI_TEXTBOX_TILE_DY;
}

/* Decide, per layer, how it can legitimately fill a wider viewport.
 *
 * There are only two ways a text BG can:
 *   1. It is bound to a full-room map source (the world layers during
 *      gameplay and cutscenes, and BG0 whose buffer we widened). Those
 *      render the whole viewport.
 *   2. It is not — in which case it is reading a 32-tile VRAM screenblock,
 *      which covers 256 px and *wraps*. Such a layer cannot fill 320 no
 *      matter what state the game is in: stretching it just repeats its
 *      content, which is what duplicated the stained-glass artwork and the
 *      pause-menu frame.
 *
 * So the rule is mechanical rather than a guess about game state: a layer
 * without a map source is clipped to DISPLAY_WIDTH and centred. That covers
 * every 240-authored surface (title, file select, menus, legend artwork)
 * and every transient screenblock fallback (room-to-room scrolling) with no
 * need to classify subtasks correctly for *rendering* — classification is
 * only still needed to decide whether the world layers may be map-sourced
 * at all, and whether sprites should travel with a shifted layer.
 */
#if UI_CENTER_DX > 0 || UI_CENTER_DY > 0
/* Is every colour the frame could use black?
 *
 * Used only to decide whether changing the clip *right now* would be visible,
 * so it deliberately does not model the fade. gFadeControl's progress/sustain
 * pair means different things per fade type, and the question here is never
 * "how far through the fade are we" but "would anyone notice". An all-black
 * palette answers that directly — and if the scene is legitimately black
 * rather than mid-fade, flipping during it is still invisible, which is the
 * entire requirement. */
/* Would this frame show anything but black?
 *
 * Two ways the engine reaches black across a menu transition and only one of
 * them touches colour, which is why the first version of this returned false
 * on every frame of both pause transitions (measured with TMC_UILATCH_TRACE):
 *
 *  - it darkens the palette, which the palette scan below catches; or
 *  - it switches the layers off and leaves the backdrop showing, which is what
 *    the pause menu actually does (PauseMenu_Variant3 clears the BG enable
 *    bits, sub_0801E104 clears two more). The palette stays bright the whole
 *    time and the screen is still pure black.
 *
 * DISPCNT is read from gIoMem rather than gScreen because that is the copy the
 * PPU renders from; gScreen is the engine's staging copy and can be a frame
 * ahead. Bits 8..12 are BG0..BG3 and OBJ. */
static bool mapsource_layers_all_off(void) {
    u16 dispcnt = (u16)(gIoMem[0] | (gIoMem[1] << 8));
    return (dispcnt & 0x1F00) == 0;
}

static bool mapsource_backdrop_is_black(void) {
    u16 c = gBgPltt[0];
    return (c & 0x1F) <= 1 && ((c >> 5) & 0x1F) <= 1 && ((c >> 10) & 0x1F) <= 1;
}

/* Is the frame a hand-built screen-space effect rather than a view of a room?
 *
 * GBA display modes 1 and 2 make BG2 an *affine* layer, and an affine layer is
 * not a room: its picture is produced by a matrix the engine recomputes per
 * scanline against screen coordinates it spells as literals. There is no room
 * map behind it to sample and nothing that follows the camera, so it cannot be
 * widened or heightened — it is a 240x160-authored surface in exactly the sense
 * the clip below already means, and mode2.c says the same thing where it
 * honours the clip on that layer.
 *
 * Two sites in the whole game leave mode 0 (`grep DISPCNT_MODE_ src/`): the
 * title screen, which is a UI screen and centred already, and the rolling
 * barrel's interior in Deepwood Shrine. So during a world view this is that
 * room, reached by what it does rather than by its area and room number — a
 * later scene that builds a screen-space effect the same way wants the same
 * treatment for the same reason.
 *
 * Read from gIoMem for the reason mapsource_layers_all_off() gives: it is the
 * copy the PPU renders from. A frame of lag is invisible because the mode is
 * set from the room's transition handler, with the screen faded out. */
static bool mapsource_affine_display_mode(void) {
    u16 dispcnt = (u16)(gIoMem[0] | (gIoMem[1] << 8));
    return (dispcnt & 7) == 1 || (dispcnt & 7) == 2;
}

static bool mapsource_palette_is_black(void) {
    int i;
    if (mapsource_layers_all_off()) {
        return mapsource_backdrop_is_black();
    }
    /* gBgPltt/gObjPltt, not gPaletteBuffer. gPaletteBuffer is the engine's
     * working copy; what the PPU renders is the emulated palette RAM the
     * engine DMAs into, and the fade only darkens the latter. Reading the
     * working copy made this return false on every frame of both pause
     * transitions -- verified with TMC_UILATCH_TRACE, which showed black=0
     * throughout and the latch running entirely on its timeout.
     *
     * Threshold rather than equality for the same reason: a fade lands on
     * near-black, not exact zero, and one stray dark-but-nonzero entry would
     * veto the whole frame. 1 of 31 per channel is below anything a display
     * separates from black. */
    for (i = 0; i < 256; i++) {
        u16 c = (u16)(gBgPltt[i] | gObjPltt[i]);
        if ((c & 0x1F) > 1 || ((c >> 5) & 0x1F) > 1 || ((c >> 10) & 0x1F) > 1) {
            return false;
        }
    }
    return true;
}

/* Frames to wait for a black frame before taking the change anyway. A
 * transition that never blacks out must not strand the clip in the wrong
 * state; taking it late is the old behaviour and no worse than it. Both pause
 * transitions reach black in about ten frames. */
#define UI_LATCH_MAX_HOLD 40

/* Change the UI/world classification only when the change cannot be seen.
 *
 * mapsource_is_ui_screen() answers from gMain.substate and gUI.lastState, and
 * MenuFadeIn sets both the instant the pause menu is *requested* — several
 * frames before the menu has drawn anything, and about eight before the screen
 * reaches black. In between, the world is still the thing on screen but is
 * already clipped to 240x160 and shifted 40 px. Measured on the pause
 * transition: all four border bands collapse from (152, 70, 24, 51) distinct
 * colours to 1 four frames before the centre goes black, and on the way out
 * gameplay fades back *in* inside the small box and then snaps to full size at
 * full brightness. The snap is the artifact; the clip itself is right.
 *
 * So this does not classify differently, it just waits for a frame where
 * switching is invisible. The engine already fades both ways across this
 * transition, so such a frame exists.
 *
 * Latched on first call rather than defaulting, because boot starts on the
 * title — a UI screen — and treating that as a pending change would hold the
 * world classification through the first frames for no reason. */
static bool sUiLatched = false;

/* Advanced exactly once per frame, at the top of Port_MapSource_Update.
 *
 * It has to be one place and it has to be first, because two things read the
 * answer and they run in the other order: mapsource_reason() decides whether a
 * world layer may keep its map source, and it runs for both layers *before*
 * mapsource_bind_ui() applies the clip. If those two disagreed for a frame the
 * result would be worse than the bug — the layer refused its map source and
 * then left unclipped, i.e. a 256 px screenblock wrapping across 320. */
static void mapsource_ui_latch_update(void) {
    static bool sHaveLatched = false;
    static int sHeld = 0;
    bool want = mapsource_is_ui_screen();

    /* Leaving a menu has to be anticipated, because the two directions are not
     * symmetric.
     *
     * Opening one changes the engine's state first and reaches black second, so
     * waiting for black is enough. Closing one is the other way round: the
     * screen goes black while the subtask is still current, and by the time
     * gMain.substate returns to GAMEMAIN_UPDATE the world is already fading
     * back *in*. There is no black frame left to wait for, which is why the
     * latch alone fixed opening and did nothing for closing -- measured, the
     * close still snapped from the 240x160 box to full size at full
     * brightness.
     *
     * Subtask_Exit sets nextToLoad = 3 at the top of the fade out and the
     * teardown steps past it, so `>= 3` is "on its way out" and it starts well
     * before black. Treating that as already a world view makes the pending
     * change exist early enough for the black frame in the middle to apply it,
     * and the world then fades in at full size. The menu itself keeps the clip
     * while it is still visible, because the latch does not act until black.
     *
     * It has to be the whole teardown rather than `== 3`: traced, nextToLoad is
     * already 4 on the single black frame in the middle of the close, so an
     * exact-match window shuts one frame too early and leaves nothing to
     * apply.
     *
     * Guarded on `want` so it can only ever bring a UI screen forward to a
     * world view, never the reverse. */
    if (want && gUI.nextToLoad >= 3) {
        want = false;
    }

    if (!sHaveLatched) {
        sHaveLatched = true;
        sUiLatched = want;
        return;
    }
    if (want == sUiLatched) {
        sHeld = 0;
        return;
    }
    {
        bool black = mapsource_palette_is_black();
        if (getenv("TMC_UILATCH_TRACE") != NULL) {
            fprintf(stderr,
                    "[uilatch] want=%d latched=%d black=%d held=%d substate=%u next=%u\n",
                    (int)want, (int)sUiLatched, (int)black, sHeld, gMain.substate, gUI.nextToLoad);
        }
        if (black || ++sHeld >= UI_LATCH_MAX_HOLD) {
            sUiLatched = want;
            sHeld = 0;
        }
    }
}

static bool mapsource_ui_screen_stable(void) {
    return sUiLatched;
}
#else
/* Nothing to latch: at GBA-native size the clip does not exist and the
 * rejection predicate below never asks. */
static bool mapsource_ui_screen_stable(void) {
    return mapsource_is_ui_screen();
}
#endif

static void mapsource_bind_ui(void) {
#if UI_CENTER_DX > 0 || UI_CENTER_DY > 0
    bool ui_screen = mapsource_ui_screen_stable();
    /* A world view built out of an affine layer is a 240x160-authored surface
     * the same way a menu is, and takes the vertical centring for the same
     * reason — see mapsource_affine_display_mode(). It is not folded into
     * `ui_screen` because the two want different things of everything else:
     * this is still the world, so the sprites and the HUD stay where the
     * engine put them and only the layers are confined. */
    bool affine_screen = !ui_screen && mapsource_affine_display_mode();
    VirtuaPPUMode1BgClip clip;
    int bg;

    virtuappu_mode1_clear_bg_clips();
    sUiCentered = ui_screen;
    sAffineCentered = affine_screen;

    /* One rule, applied uniformly: a layer with no map source is reading a
     * 32-tile screenblock. It covers 256 px and wraps, so it cannot fill a
     * wider viewport — stretching it only repeats its content. Clip it to
     * the authored width and centre it.
     *
     * BG0 is now such a layer (its buffer stayed 32 wide), so it needs no
     * special case: the HUD, the text box and every UI screen are all
     * carried by the same clip, which is what keeps their authored
     * relationship to each other intact. */
    clip.offset_x = UI_CENTER_DX;
    clip.content_width = DISPLAY_WIDTH;
    /* Vertically the rule is *not* uniform, and this is the one place the two
     * axes differ. A whole authored screen is centred, so it takes the shift.
     * A world view's BG0 carries the HUD, which is anchored to the top of the
     * screen — and the top of a taller screen is still the top — so it keeps
     * offset_y = 0 and is allowed the full frame. See UI_CENTER_DY. */
    clip.offset_y = ui_screen ? UI_CENTER_DY : 0;
    clip.content_height = ui_screen ? DISPLAY_HEIGHT : MODE1_GBA_HEIGHT;
    sClippedBgMask = 0;
    for (bg = 0; bg < 4; bg++) {
        VirtuaPPUMode1BgClip bg_clip = clip;
        /* BG3 during a world view is a *gameplay overlay* — hole parallax,
         * cloud shadows, light/dark, weather, steam, POW — not a 240-authored
         * surface. Two things go wrong if the rule catches it.
         *
         * It loses the overlay in the border columns, and, worse, the clip
         * also shifts the layer by UI_CENTER_DX. Most of these overlays are
         * world-locked (bg3.xOffset = scroll_x + k, e.g. holeManager.c:299,
         * powBackgroundManager.c:32), and at a wider viewport scroll_x is
         * already 40px further left, so the layer aligns with the world by
         * itself. Adding the shift on top misaligns it — visible in the
         * middle of the screen, not just the borders.
         *
         * Leaving it unclipped lets it wrap its 32-tile screenblock past
         * 256px, which for these tiled patterns is exactly what covers the
         * viewport: measured pixel-identical to the 240 build through the
         * centre 240 columns, where clipping it differed by 6197 px. On a UI
         * screen BG3 *is* authored content and still takes the clip. */
        if (bg == 3 && !ui_screen) {
            /* ...unless this frame's overlay has declared itself anchored to
             * the screen instead of the world (B21). Then both halves of the
             * reasoning above invert: there is no world alignment to preserve,
             * because its xOffset never came from the camera; and the wrap is
             * not more pattern but the blank left end of a 256-px map. Clip it
             * to the authored width and pin it to the declared edge — for the
             * light shaft that is the right one, which is where the artwork
             * ends and where the band is drawn against. */
            if (sBg3ScreenAnchor != PORT_BG3_ANCHOR_RIGHT) {
                continue;
            }
            /* The right edge of the *room*, which is not always the right edge
             * of the viewport. Exactly two rooms in the game run this handler
             * and they differ on precisely this point: Minish Woods is 1008 px
             * wide, fills the screen, and the two edges coincide; the barrel
             * minish house is 240x368, narrower than the viewport, so the room
             * is centred with 40 px of border either side. Pinning to the
             * viewport there hangs the band out into that border — measured at
             * cols 195..319 against a room ending at 279.
             *
             * Same span the sprite clip below computes, and for the same
             * reason: outside it is border, not world. Reduces to 0 at
             * GBA-native width, where the room span is the whole screen. */
            {
                int span_right = MODE1_GBA_WIDTH;
                /* Mid-scroll the room metrics are not coherent — the sprite
                 * clip below already refuses to trust them then, for reasons
                 * established when it was written, and takes the centred span
                 * instead. Same source, same distrust. */
                if (gRoomControls.scrollAction >= 2) {
                    span_right = UI_CENTER_DX + DISPLAY_WIDTH;
                } else if ((int)gRoomControls.width < MODE1_GBA_WIDTH) {
                    int cx = (int)gRoomControls.scroll_x -
                             (int)gRoomControls.origin_x;
                    span_right = -cx + (int)gRoomControls.width;
                    if (span_right > MODE1_GBA_WIDTH) {
                        span_right = MODE1_GBA_WIDTH;
                    }
                    if (span_right < DISPLAY_WIDTH) {
                        span_right = DISPLAY_WIDTH;
                    }
                }
                bg_clip.offset_x = span_right - DISPLAY_WIDTH;
            }
        }
        /* The affine screen's own layers are confined to the authored 160
         * rows, centred — the vertical twin of the width clip above, and true
         * for the same mechanical reason: a screenblock is 32 tiles in both
         * axes, so past row 160 there is no more authored content and what
         * shows is whatever else the block holds. In the barrel that was 80
         * rows of unrelated tiles below the barrel, which is the half of this
         * report you can see.
         *
         * BG0 is excluded because it is not one of that screen's layers — it
         * carries the HUD, whose two bands anchor to the top and bottom edges
         * of the *viewport* (UI_CENTER_DY, UI_HUD_LOWER_DY). Confining it
         * would push the hearts down 40 px and delete the rupee and key
         * counters outright. */
        if (affine_screen && bg != 0) {
            bg_clip.offset_y = UI_CENTER_DY;
            bg_clip.content_height = DISPLAY_HEIGHT;
        }
        /* A layer that has said it is carrying a tiled pattern wants the wrap,
         * for the same reason BG3 above does — see
         * Port_MapSource_DeclareTiledOverlay. */
        if (!ui_screen && ((sTiledOverlayMask >> bg) & 1u) != 0u) {
            continue;
        }
        if (!virtuappu_mode1_has_map_source(bg)) {
            virtuappu_mode1_set_bg_clip(bg, &bg_clip);
            sClippedBgMask |= 1 << bg;
        }
    }

    /* On a UI screen everything on show is centred, sprites included. In a
     * world view the world sprites must stay where the engine put them —
     * the HUD's own sprites are shifted at their source instead
     * (UI_HUD_SPRITE_DX in ui.c), so they travel with the HUD layer without
     * dragging Link along with them. */
    virtuappu_mode1_set_obj_offset(ui_screen ? UI_CENTER_DX : 0,
                                   ui_screen ? UI_CENTER_DY : 0);

    if (ui_screen) {
        virtuappu_mode1_set_obj_clip(UI_CENTER_DX, UI_CENTER_DX + DISPLAY_WIDTH);
        virtuappu_mode1_set_obj_clip_v(UI_CENTER_DY, UI_CENTER_DY + DISPLAY_HEIGHT);
        return;
    }
    virtuappu_mode1_set_obj_clip_v(0, MODE1_GBA_HEIGHT);

    /* World view: confine sprites to the room's on-screen span so the border
     * stays border. A room narrower than the viewport is centred, so the
     * columns either side are outside the room and hardware would never
     * have drawn an entity standing there. When the world layers have fallen
     * back to the screenblock path (mid-transition) they are clipped to the
     * authored width above, so the sprites match that instead. */
    {
        int span_left = 0;
        int span_right = MODE1_GBA_WIDTH;
        if (gRoomControls.scrollAction >= 2 || !virtuappu_mode1_has_map_source(2)) {
            span_left = UI_CENTER_DX;
            span_right = UI_CENTER_DX + DISPLAY_WIDTH;
        } else if ((int)gRoomControls.width < MODE1_GBA_WIDTH) {
            int cx = (int)gRoomControls.scroll_x - (int)gRoomControls.origin_x;
            span_left = -cx;
            span_right = span_left + (int)gRoomControls.width;
            if (span_left < 0) span_left = 0;
            if (span_right > MODE1_GBA_WIDTH) span_right = MODE1_GBA_WIDTH;
        }
        virtuappu_mode1_set_obj_clip(span_left, span_right);
    }
#endif
}

/* TMC_LAYER_TRACE=1: which BG indices have a map source and which the clip
 * rule caught, with DISPCNT and all four BGxCNT. The question this answers is
 * "which layer is that artwork actually on, and did the rule reach it" — so it
 * must run after mapsource_bind_ui(), not before. */
static void mapsource_trace_layers(void) {
    static int en = -1;
    static int last = -1;
    int mapsrc = 0, key, b;
    if (en < 0) en = (getenv("TMC_LAYER_TRACE") != NULL);
    if (!en) return;
    for (b = 0; b < 4; b++) {
        if (virtuappu_mode1_has_map_source(b)) mapsrc |= 1 << b;
    }
    key = mapsrc | (sClippedBgMask << 4);
    if (key == last) return;
    last = key;
    fprintf(stderr, "[layers] task=%d sub=%d ui=%d area=0x%02X room=0x%02X "
                    "mapsrc_mask=0x%X clip_mask=0x%X dispcnt=0x%04X "
                    "bg0ctl=0x%04X bg1ctl=0x%04X bg2ctl=0x%04X bg3ctl=0x%04X\n",
            gMain.task, gMain.substate, gUI.lastState,
            gRoomControls.area, gRoomControls.room, mapsrc, sClippedBgMask,
            gScreen.lcd.displayControl, gScreen.bg0.control, gScreen.bg1.control,
            gScreen.bg2.control, gScreen.bg3.control);
}

/* TMC_BG3_TRACE=1: log every time BG3 is switched on or off during gameplay.
 *
 * BG3 carries the screen-fixed gameplay overlays (hole parallax, light/dark,
 * weather, steam, POW). It is off in ordinary rooms, and it never has a map
 * source — so the "no map source ⇒ clip" rule always catches it, which is
 * fine while it is off and is exactly what needs checking when it is not.
 * This is the sweep hook for that (docs/viewport-bug-tracker.md). */
static void mapsource_trace_bg3(void) {
    static int en = -1;
    static int lastOn = -1;
    static u8 lastArea = 0xFF, lastRoom = 0xFF;
    static unsigned frame = 0, onFrames = 0;
    int on;
    if (en < 0) {
        const char* v = getenv("TMC_BG3_TRACE");
        /* =2 prints every frame instead of only on transitions. A
         * transition-only trace answers "is BG3 on here"; it cannot answer
         * "why is this frame's overlay in that position", which is what a
         * per-frame `anchor`/`clipped` pair does — the B31 shape, where the
         * engine's own choice was right the whole time and the question was
         * what the renderer did with it. */
        en = (v == NULL) ? 0 : (v[0] == '2' ? 2 : 1);
    }
    if (!en) return;
    /* Port_MapSource_Update runs once per VBlank, the same cadence as the
     * capture frame counter, so this lines up with --dump frame numbers. */
    frame++;
    on = (gScreen.lcd.displayControl & 0x0800) != 0;
    if (on) {
        onFrames++;
    }
    if (en < 2 && on == lastOn && gRoomControls.area == lastArea &&
        gRoomControls.room == lastRoom) {
        return;
    }
    if (on || lastOn == 1) {
        fprintf(stderr, "[bg3] f=%-6u onFrames=%-5u %s task=%d sub=%d area=0x%02X room=0x%02X "
                        "bg3ctl=0x%04X ofs=(%d,%d) roomw=%u clipped=%d\n",
                frame, onFrames,
                on ? "ON " : "off", gMain.task, gMain.substate,
                gRoomControls.area, gRoomControls.room, gScreen.bg3.control,
                (int)gScreen.bg3.xOffset, (int)gScreen.bg3.yOffset,
                gRoomControls.width, (sClippedBgMask >> 3) & 1);
        if (en >= 2) {
            fprintf(stderr, "          anchor=%d camy=%d\n", sBg3ScreenAnchor,
                    (int)gRoomControls.scroll_y - (int)gRoomControls.origin_y);
        }
    }
    lastOn = on;
    lastArea = gRoomControls.area;
    lastRoom = gRoomControls.room;
}

/* TMC_BLEND_TRACE=2 also reports every palette group as it loads, with how
 * much colour it left in the engine's working buffer. A group that loads and
 * leaves the buffer black is a different bug from a buffer that was fine and
 * got overwritten afterwards, and the frame they happen on is the same one. */
/* Catch the moment the working palette loses its colour, whoever did it.
 * Called from the palette sink and once per frame, so a drop caused by a
 * direct write rather than by LoadPalettes still shows up -- attributed to
 * "frame" instead of a group. */
void Port_TracePaletteDrop(const char* where) {
    static int en = -1;
    static int last = -1;
    int i, nonblack = 0;
    if (en < 0) {
        const char* v = getenv("TMC_BLEND_TRACE");
        en = (v != NULL && (v[0] == '2' || v[0] == '3'));
    }
    if (!en) return;
    for (i = 1; i < 256; i++) {
        if (gPaletteBuffer[i] != 0) nonblack++;
    }
    if (last >= 0 && nonblack < last - 32) {
        int row, col, rowCount;
        fprintf(stderr, "[pltt-drop] %s: %d -> %d non-black  area=0x%02X room=0x%02X "
                        "task=%d substate=%d\n",
                where, last, nonblack, gRoomControls.area, gRoomControls.room,
                gMain.task, gMain.substate);
        /* Per 16-colour row: which survived says what the writer's extent was. */
        fprintf(stderr, "            rows:");
        for (row = 0; row < 16; row++) {
            rowCount = 0;
            for (col = 0; col < 16; col++) {
                if (gPaletteBuffer[row * 16 + col] != 0) rowCount++;
            }
            fprintf(stderr, " %X:%02d", row, rowCount);
        }
        fprintf(stderr, "\n");
    }
    last = nonblack;
}

void Port_TracePaletteGroup(u32 group) {
    static int en = -1;
    int i, nonblack = 0;
    if (en < 0) {
        const char* v = getenv("TMC_BLEND_TRACE");
        en = (v != NULL && v[0] == '2');
    }
    if (!en) return;
    for (i = 1; i < 256; i++) {
        if (gPaletteBuffer[i] != 0) nonblack++;
    }
    fprintf(stderr, "[pltt] group=0x%02X area=0x%02X room=0x%02X -> palbuffer_nonblack=%d/255\n",
            group, gRoomControls.area, gRoomControls.room, nonblack);
}

/* TMC_BLEND_TRACE=1 — the colour-special-effects registers and a palette
 * sample, printed whenever any of them changes.
 *
 * "The room is black except the sprites" has at least three causes that look
 * identical in a frame: the layer draws nothing, the layer draws tiles whose
 * palette entries are black, or the layer draws correctly and BLDCNT darkens
 * it to black afterwards. TMC_MASK_BG<n> separates the first from the other
 * two — it bypasses both the palette and the blend — and this separates those
 * two from each other. B20 needed the same distinction and had to infer it. */
static void mapsource_trace_blend(void) {
    static int en = -1;
    static u32 last = 0xFFFFFFFFu;
    u16 bldcnt, bldalpha, bldy;
    u32 key;
    int i, nonblack, workingNonblack, semiObjs;
    if (en < 0) en = (getenv("TMC_BLEND_TRACE") != NULL);
    if (!en) return;
    bldcnt = *(volatile u16*)&gIoMem[0x50];
    bldalpha = *(volatile u16*)&gIoMem[0x52];
    bldy = *(volatile u16*)&gIoMem[0x54];
    /* How much of the BG palette is not black, as a one-number summary of
     * "is the palette loaded at all". */
    nonblack = 0;
    for (i = 1; i < 256; i++) {
        if (gBgPltt[i] != 0) nonblack++;
    }
    /* And the engine's working copy. The two answer different questions: the
     * working copy says whether the room's palette was ever loaded, the live
     * one says what the fade left behind. A colourful working copy over a
     * black live one is a fade that never lifted. */
    workingNonblack = 0;
    for (i = 1; i < 256; i++) {
        if (gPaletteBuffer[i] != 0) workingNonblack++;
    }
    /* How many enabled sprites are in OBJ mode 1 (semi-transparent). A scene
     * that blends its sprites through the OBJ mode rather than through
     * BLDCNT's first-target bits reports 0 for tgt1 and non-zero here, which
     * is the pair that explains an opaque sprite the game meant to see
     * through. It is also the honest coverage measure for that path: a route
     * whose count is 0 throughout does not exercise it at all. */
    semiObjs = 0;
    for (i = 0; i < 128; i++) {
        u16 a0 = gOamMem[i * 4];
        if ((a0 & 0x0300u) == 0x0200u) {
            continue; /* hidden (not affine, double-size bit set) */
        }
        if (((a0 >> 10) & 3u) == 1u) {
            semiObjs++;
        }
    }
    key = ((u32)bldcnt << 16) ^ ((u32)bldalpha << 8) ^ (u32)bldy ^
          ((u32)nonblack << 24) ^ ((u32)workingNonblack << 12) ^ ((u32)semiObjs << 4);
    if (key == last) return;
    last = key;
    fprintf(stderr,
            "[blend] area=0x%02X room=0x%02X bldcnt=0x%04X (tgt1=0x%02X effect=%d "
            "tgt2=0x%02X) bldalpha=0x%04X eva=%d evb=%d bldy=0x%04X evy=%d "
            "bgpltt_nonblack=%d/255 palbuffer_nonblack=%d/255 semi_objs=%d\n",
            gRoomControls.area, gRoomControls.room, bldcnt,
            bldcnt & 0x3F, (bldcnt >> 6) & 3, (bldcnt >> 8) & 0x3F,
            bldalpha, bldalpha & 0x1F, (bldalpha >> 8) & 0x1F,
            bldy, bldy & 0x1F, nonblack, workingNonblack, semiObjs);
}

/* TMC_TILE_PROBE=col,row — what the renderer will do with one room tile.
 *
 * Prints, for both world layers, the tilemap entry at that room tile, the
 * character address it resolves to, and the offset the per-tile tileset
 * selection would apply. The point is to tell "the region did not match"
 * apart from "the address is in no slot" apart from "the tile is not drawn
 * by a map-sourced layer at all" — three explanations for the same symptom,
 * indistinguishable from the picture. */
static void mapsource_trace_tile_probe(void) {
    static int parsed = -1;
    static int pcol = -1, prow = -1;
    static unsigned frame = 0;
    int layer;

    if (parsed < 0) {
        const char* env = getenv("TMC_TILE_PROBE");
        parsed = 0;
        if (env != NULL && sscanf(env, "%d,%d", &pcol, &prow) == 2) {
            parsed = 1;
        }
    }
    frame++;
    if (parsed != 1) {
        return;
    }
    {
        /* Which of the three things that can change under a tileset swap
         * actually moved: the character window the manager owns, the one it
         * does not, or the palette. */
        u32 owned = 0, unowned = 0, pltt = 0;
        int k;
        for (k = 0; k < 0x4000; k++) {
            owned = owned * 31u + gVram[k];
            owned = owned * 31u + gVram[0x8000 + k];
            unowned = unowned * 31u + gVram[0x4000 + k];
        }
        for (k = 0; k < 256; k++) {
            pltt = pltt * 31u + gBgPltt[k];
        }
        fprintf(stderr, "[probe] f=%u chars_owned=%08X chars_4000_7FFF=%08X bgpltt=%08X\n", frame,
                owned, unowned, pltt);
    }
    for (layer = 0; layer < 2; layer++) {
        const u16* map = (layer == 0) ? gMapDataBottomSpecial : gMapDataTopSpecial;
        int bg = mapsource_bg_index((layer == 0) ? gMapBottom.bgSettings : gMapTop.bgSettings);
        u16 entry;
        u32 charBase;
        u32 charAddr;

        if (!virtuappu_mode1_has_map_source(bg) || map == NULL) {
            fprintf(stderr, "[probe] f=%u layer=%d bg=%d NO MAP SOURCE\n", frame, layer, bg);
            continue;
        }
        entry = map[(size_t)prow * MAPSRC_STRIDE + pcol];
        charBase = (u32)(((layer == 0) ? gScreen.bg2.control : gScreen.bg1.control) >> 2 & 3) *
                   0x4000u;
        charAddr = charBase + (u32)(entry & 0x3FF) * 32u;
        fprintf(stderr,
                "[probe] f=%u layer=%d bg=%d tile(%d,%d) entry=0x%04X idx=%u pal=%u "
                "charAddr=0x%05X offset=0x%05X\n",
                frame, layer, bg, pcol, prow, entry, entry & 0x3FF, (entry >> 12) & 0xF, charAddr,
                Port_TilesetResidency_OffsetFor(charAddr, pcol, prow));
    }
}

/* TMC_REJECT_TRACE=1: why each world layer was refused a map source. */
static void mapsource_trace_reject(void) {
    static int en = -1;
    static int lastR0 = -99, lastR1 = -99;
    if (en < 0) en = (getenv("TMC_REJECT_TRACE") != NULL);
    if (!en) return;
    if (sLastReason[0] != lastR0 || sLastReason[1] != lastR1) {
        lastR0 = sLastReason[0]; lastR1 = sLastReason[1];
        fprintf(stderr, "[reject] task=%d substate=%d uiState=%d area=0x%02X room=0x%02X "
                        "w=%u sf=0x%02X sa=%u -> bottom=%s top=%s\n",
                gMain.task, gMain.substate, gUI.lastState,
                gRoomControls.area, gRoomControls.room, gRoomControls.width,
                gRoomControls.scroll_flags, gRoomControls.scrollAction,
                Port_MapSource_ReasonName(sLastReason[0]),
                Port_MapSource_ReasonName(sLastReason[1]));
    }
}

void Port_MapSource_Update(void) {
    int layer;
    /* Clear every layer first: which BG a map binds to can change between
     * frames, so a stale binding on a BG this frame's layers no longer use
     * would keep sampling after the engine moved on. */
    { void Port_MapSource_CamTrace(void); Port_MapSource_CamTrace(); }
    /* Before mapsource_bind_ui() reads it, and before the traces print it. */
    mapsource_bg3_anchor_expire();
    mapsource_tiled_overlay_expire();
#if UI_CENTER_DX > 0 || UI_CENTER_DY > 0
    /* Once per frame, and before the layer loop below: mapsource_reason() and
     * mapsource_bind_ui() must both see the same answer this frame. */
    mapsource_ui_latch_update();
#endif
    Port_TilesetResidency_TraceGroups();
    virtuappu_mode1_clear_map_sources();
    virtuappu_mode1_clear_char_slots();
    for (layer = 0; layer < 2; layer++) {
        int reason = mapsource_reason(layer);
        int bg;
        sLastReason[layer] = reason;
        if (reason != REASON_BOUND) {
            sRejectFrames[layer][reason]++;
            continue;
        }
        bg = mapsource_bg_index((layer == 0) ? gMapBottom.bgSettings
                                             : gMapTop.bgSettings);
        sBoundFrames[layer]++;
        if (getenv("TMC_MAPSRC_DIAG") != NULL && sBoundFrames[layer] % 400 == 1) {
            const u16* m = (layer == 0) ? gMapDataBottomSpecial : gMapDataTopSpecial;
            int cx = (int)gRoomControls.scroll_x - (int)gRoomControls.origin_x;
            int cy = (int)gRoomControls.scroll_y - (int)gRoomControls.origin_y;
            int bgcnt = (int)((layer == 0) ? gScreen.bg2.control : gScreen.bg1.control);
            int sb = (bgcnt >> 8) & 0x1F;
            const u16* blk = (const u16*)(gVram + sb * 0x800);
            int hofs = 0, vofs = 0, agree = 0, tot = 0, sx, sy;
            hofs = (layer == 0) ? gScreen.bg2.xOffset : gScreen.bg1.xOffset;
            vofs = (layer == 0) ? gScreen.bg2.yOffset : gScreen.bg1.yOffset;
            for (sy = 0; sy < 20; sy++) {
                for (sx = 0; sx < 30; sx++) {
                    int mx = (cx >> 3) + sx, my = (cy >> 3) + sy;
                    int vr = (((vofs >> 3) + sy) & 0x1F), vc = (((hofs >> 3) + sx) & 0x1F);
                    tot++;
                    if (blk[vr * 32 + vc] == m[my * MAPSRC_STRIDE + mx]) agree++;
                }
            }
            fprintf(stderr, "[mapsrc-diag] area=0x%02X room=0x%02X layer=%d bg=%d "
                            "room=%ux%u cam=(%d,%d) ofs=(%d,%d) sb=%d cb=0x%04X "
                            "bgcnt=0x%04X agree=%d/%d\n",
                    gRoomControls.area, gRoomControls.room, layer, bg,
                    gRoomControls.width, gRoomControls.height, cx, cy, hofs, vofs, sb,
                    ((bgcnt >> 2) & 3) * 0x4000, bgcnt, agree, tot);
        }
        {
            /* origin = camera position in room pixels. This is exactly the
             * world pixel the hardware path resolves for screen column 0:
             * the engine keeps hofs = cx & 15 and vofs = (cy & 15) + 8 over
             * a *sliding* (non-wrapping) window, and the two addressings
             * reduce to the same map entry. That identity is what makes the
             * map-sampled output bit-identical at GBA-native size. */
            VirtuaPPUMode1MapSource src;
            src.map = (layer == 0) ? gMapDataBottomSpecial : gMapDataTopSpecial;
            src.stride = MAPSRC_STRIDE;
            src.width_tiles = gRoomControls.width >> 3;
            src.height_tiles = gRoomControls.height >> 3;
            src.origin_x = (int)gRoomControls.scroll_x - (int)gRoomControls.origin_x;
            src.origin_y = (int)gRoomControls.scroll_y - (int)gRoomControls.origin_y;
            virtuappu_mode1_set_map_source(bg, &src);
            /* Per-tile tileset selection rides on this binding and only on
             * this binding: it needs the room coordinates the map source
             * addresses in, so a layer that fell back to a screenblock has
             * nothing to test and keeps the camera-based group (B27). */
            Port_TilesetResidency_PublishForBg(bg);
        }
    }

    /* The UI clip rule must run *after* the world bindings: it asks which
     * layers have a map source, and a layer bound earlier in this same
     * function would otherwise be seen as unbound and wrongly clipped to the
     * authored width — which silently confined the whole world to 240 and
     * made cutscenes look centred even while the trace said "bound".
     *
     * It must also run *unconditionally*, on every frame. An earlier attempt
     * fixed the ordering by moving this call into Port_MapSource_CamTrace,
     * which is a diagnostic: it returns early unless TMC_CAMTRACE is set and
     * the room has just changed. That did order it after the bindings, but it
     * also meant the clips were never applied in an ordinary run — which is
     * what regressed B2 (legend artwork repeating past x=240) and disarmed
     * the sprite clip and offset with it. */
    mapsource_bind_ui();
    mapsource_trace_tile_probe();
    mapsource_trace_reject();
    mapsource_trace_layers();
    mapsource_trace_bg3();
    mapsource_trace_blend();
    Port_TracePaletteDrop("frame");
}

/* Spike 5: per-room camera-range report (TMC_CAMTRACE=1). Confirms the
 * horizontal clamp keeps the camera inside its room and that rooms
 * narrower than the viewport sit centred. */
void Port_MapSource_CamTrace(void) {
    static u8 lastArea = 0xFF, lastRoom = 0xFF;
    static int enabled = -1;
    if (enabled < 0) enabled = (getenv("TMC_CAMTRACE") != NULL);
    if (!enabled) return;
    if (gRoomControls.area == lastArea && gRoomControls.room == lastRoom) return;
    if (gMain.task != TASK_GAME || gRoomControls.width == 0) return;
    lastArea = gRoomControls.area; lastRoom = gRoomControls.room;
    {
        int cx = (int)gRoomControls.scroll_x - (int)gRoomControls.origin_x;
        int mnx = VIEWPORT_CAM_MIN_X(0, gRoomControls.width);
        int mxx = VIEWPORT_CAM_MAX_X(0, gRoomControls.width);
        int narrowx = ((int)gRoomControls.width <= VIEWPORT_WIDTH);
        int cy = (int)gRoomControls.scroll_y - (int)gRoomControls.origin_y;
        int mny = VIEWPORT_CAM_MIN_Y(0, gRoomControls.height);
        int mxy = VIEWPORT_CAM_MAX_Y(0, gRoomControls.height);
        int shorty = ((int)gRoomControls.height <= VIEWPORT_HEIGHT);
        fprintf(stderr,
                "[camtrace] area=0x%02X room=0x%02X %ux%u viewport=%dx%d "
                "camx=%d rangex=[%d,%d] %s%s | camy=%d rangey=[%d,%d] %s%s\n",
                gRoomControls.area, gRoomControls.room,
                gRoomControls.width, gRoomControls.height,
                VIEWPORT_WIDTH, VIEWPORT_HEIGHT,
                cx, mnx, mxx, narrowx ? "NARROW(centred)" : "scrollable",
                (cx < mnx || cx > mxx) ? "  ** X OUT OF RANGE **" : "",
                cy, mny, mxy, shorty ? "SHORT(centred)" : "scrollable",
                (cy < mny || cy > mxy) ? "  ** Y OUT OF RANGE **" : "");
    }
}

void Port_MapSource_Report(void) {
    int layer;
    for (layer = 0; layer < 2; layer++) {
        int r;
        fprintf(stderr, "[mapsource] layer %d (%s): bound %u frames",
                layer, layer == 0 ? "bottom/BG2" : "top/BG1", sBoundFrames[layer]);
        for (r = 0; r < REASON_COUNT; r++) {
            if (sRejectFrames[layer][r]) {
                fprintf(stderr, ", %s %u", kReasonName[r], sRejectFrames[layer][r]);
            }
        }
        fprintf(stderr, "\n");
    }
}

/* Temporary Spike 3 audit hook (see mode1.c): compares every map-sampled
 * tile fetch against the screenblock entry the hardware path would use. */
extern int mode1_map_source_audit;
extern unsigned long mode1_map_source_audit_total, mode1_map_source_audit_bad;
void Port_MapSource_AuditEnable(void) { mode1_map_source_audit = 1; }
void Port_MapSource_AuditReport(void) {
    fprintf(stderr, "[mapsrc-audit] fetches=%lu mismatched=%lu\n",
            mode1_map_source_audit_total, mode1_map_source_audit_bad);
}

int Port_MapSource_LayerBgIndex(int layer) {
    const BgSettings* s = ((layer & 1) == 0) ? gMapBottom.bgSettings : gMapTop.bgSettings;
    return (s == NULL) ? -1 : mapsource_bg_index(s);
}
