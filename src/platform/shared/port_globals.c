/**
 * @file port_globals.c
 * @brief Host-side definitions of the GBA-build globals normally allocated
 *        by the linker script (see linker.ld).
 *
 * Sub-step 2b.4 of the SDL-port roadmap. On the GBA the globals below are
 * BSS-style allocations placed at fixed offsets in EWRAM/IWRAM by
 * `linker.ld` (e.g. `gMain = 0x03001000`, `gPaletteBuffer = 0x020176A0`).
 * The decompiled source declares them with `extern` in the matching
 * headers and never emits a definition in any C TU. That is fine for the
 * GBA build because the linker resolves them, but the SDL host build has
 * no equivalent SECTIONS block.
 *
 * This TU provides plain C definitions for every such symbol that is
 * actually referenced by the subset of `src/` linked into `tmc_sdl`.
 * Only the globals reached by the call graph rooted at `AgbMain` end up
 * here; the rest stay undefined and will be added in later PRs as more
 * game systems get wired in.
 *
 * Layout note: these structs do **not** match the GBA's
 * `static_assert(sizeof(X) == ...)` sizes on a 64-bit host because every
 * embedded pointer is 8 bytes instead of 4. The matching ROM build keeps
 * the real layout assertions; the SDL build collapsed them to no-ops in
 * `include/global.h` for exactly this reason (PR #2b.3 wave 1).
 */
#include "area.h"
#include "affine.h"
#include "backgroundAnimations.h"
#include "beanstalkSubtask.h"
#include "common.h"
#include "color.h"
#include "entity.h"
#include "fade.h"
#include "main.h"
#include "message.h"
#include "enemy.h"
#include "kinstone.h"
#include "manager/diggingCaveEntranceManager.h"
#include "pauseMenu.h"
#include "player.h"
#include "room.h"
#include "scroll.h"
#include "script.h"
#include "screen.h"
#include "save.h"
#include "sound.h"
#include "structures.h"
#include "subtask.h"
#include "ui.h"
#include "vram.h"

#include <stddef.h>

typedef struct LinkedList2 LinkedList2;
typedef struct {
    void* table;
    void* list_top;
    Entity* current_entity;
    void* restore_sp;
} UpdateContext;

/* Main system state. */
Main gMain;
Input gInput;
Screen gScreen;
RoomControls gRoomControls;
struct_02000010 gUnk_02000010;
u32 gRand;

/* Subtask dispatch tables from data/const/subtask.s.
 *
 * Leaving these as unresolved zero stubs makes GameMain_Subtask call a NULL
 * function pointer as soon as any room callback invokes `sub_080A71C4` /
 * MenuFadeIn. */
void Subtask_FadeIn(void);
void Subtask_Init(void);
void Subtask_Update(void);
void Subtask_FadeOut(void);
void Subtask_Die(void);
void Subtask_PauseMenu(void);
void Subtask_MapHint(void);
void Subtask_KinstoneMenu(void);
void Subtask_AuxCutscene(void);
void Subtask_PortalCutscene(void);
void Subtask_FigurineMenu(void);
void Subtask_WorldEvent(void);
void Subtask_FastTravel(void);
void Subtask_LocalMapHint(void);
void Subtask_MapHint_0(void);
void Subtask_MapHint_1(void);
void sub_080A6B04(void);
void sub_080A6C1C(void);
void Subtask_FastTravel_0(void);
void Subtask_FastTravel_1(void);
void Subtask_FastTravel_2(void);
void Subtask_FastTravel_3(void);
void Subtask_FastTravel_4(void);
void sub_080A59AC(void);
void sub_080A59C8(void);
void sub_080A5A54(void);
void sub_080A5A90(void);
void sub_080A5AF4(void);
void sub_080A5B34(void);
void sub_080A5BB8(void);
void sub_080A5C44(void);
void sub_080A5C9C(void);
void sub_080A6108(void);
void sub_080A612C(void);
void sub_080A6290(void);
void sub_080A62E0(void);
void sub_080A6650(void);
void sub_080A667C(void);
void sub_080A6024(void);
void sub_080A6044(void);

void (*const gUnk_0812901C[])(void) = {
    Subtask_FadeIn,
    Subtask_Init,
    Subtask_Update,
    Subtask_FadeOut,
    Subtask_Die,
};

void (*const gSubtasks[])(void) = {
    Subtask_Exit,
    Subtask_PauseMenu,
    Subtask_Exit,
    Subtask_MapHint,
    Subtask_KinstoneMenu,
    Subtask_AuxCutscene,
    Subtask_PortalCutscene,
    Subtask_FigurineMenu,
    Subtask_WorldEvent,
    Subtask_FastTravel,
    Subtask_LocalMapHint,
};

/* Subtask dispatch/data tables from data/const/subtask.s.
 *
 * These symbols were previously weak byte blobs from
 * port_roominit_graph_stubs.c. They are dereferenced as typed function/data
 * tables by map-hint and fast-travel subtasks, so provide concrete host
 * definitions to avoid indexing into untyped stub storage. */
void (*const Subtask_MapHint_Functions[])(void) = {
    Subtask_MapHint_0,
    Subtask_MapHint_1,
};

void (*const gUnk_08128F1C[])(void) = {
    sub_080A6B04,
    sub_080A6C1C,
};

void (*const Subtask_FastTravel_Functions[])(void) = {
    Subtask_FastTravel_0,
    Subtask_FastTravel_1,
    Subtask_FastTravel_2,
    Subtask_FastTravel_3,
    Subtask_FastTravel_4,
};

/* gUnk_08128F4C is a map-hint bit table indexed by `gUI.field_0x3`.
 * Use a stable one-hot fallback mapping so hints still set deterministic
 * bits without depending on unresolved data blobs. */
const u16 gUnk_08128F4C[16] = {
    0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
    0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000, 0x8000,
};

/* Pause-menu dispatch tables from data/const/subtask.s.
 * These are indexed directly by menuType in pause-menu handlers. */
void (*const gUnk_08128D14[])(void) = {
    sub_080A59AC,
    sub_080A59C8,
    sub_080A5A54,
    sub_080A5A90,
};

void (*const gUnk_08128D24[])(void) = {
    sub_080A5AF4,
    sub_080A5B34,
    sub_080A5BB8,
};

void (*const gUnk_08128D30[])(void) = {
    sub_080A5C44,
    sub_080A5C9C,
};

void (*const gUnk_08128DB0[])(void) = {
    sub_080A6108,
    sub_080A612C,
};

void (*const gUnk_08128DCC[])(void) = {
    sub_080A6290,
    sub_080A62E0,
};

void (*const gUnk_08128E78[])(void) = {
    sub_080A6650,
    sub_080A667C,
};

/* Pause/subtask small data tables that are still ROM-only in this port.
 * Keep host fallbacks explicit and bounded instead of weak blob stubs. */
u8 gUnk_08128DD4[4] = { 0, 0, 0, 0 };
u8 gUnk_08128E80[4] = { 0, 0, 0, 0 };

/* KeyButtonLayout streams consumed by sub_080A70AC(). Setting
 * aButtonText=0xFF makes the encoded follow-up entry terminate the walk
 * after one CreateUIElement() step, matching the host-safe sentinel
 * strategy used in fileselect.c. */
u8 gUnk_08128DD8[9] = {
    0xFF, 0xD8, 0xFF, /* A button */
    0xFF, 0xD8, 0x00, /* B button */
    0xFF, 0xD8, 0x00, /* R button */
};

u8 gUnk_08128E84[9] = {
    0xFF, 0xD8, 0xFF, /* A button */
    0xFF, 0xD8, 0x00, /* B button */
    0xFF, 0xD8, 0x00, /* R button */
};

/* sub_080A6F40() walks 2-byte pairs until first byte is zero. */
u8 gUnk_08128F38[2] = { 0, 0 };

/* Map-hint icon table: loops terminate on frameIndex == 0. */
struct_gUnk_08128F58 gUnk_08128F58[] = {
    { 0, 0, 0, 0, 0, 0 },
};

/* Text resources used by subtask.c.
 * Keep these as explicit typed stand-ins instead of weak blob stubs. */
u16 gDungeonNames[32] = { 0 };
Font gUnk_08128FA8 = { 0 };
Font gUnk_08128FC0 = { 0 };
Font gUnk_08128FD8 = { 0 };
Font gUnk_08129004 = { 0 };

/* Pause-menu screen 7 tables. */
void (*const gUnk_08128D58[])(void) = {
    sub_080A6024,
    sub_080A6044,
};

/* Input remap/routing byte tables consumed through gMenu.field_0xc. */
u8 gUnk_08128C00[4] = { 0, 0, 0, 0 };
u8 gUnk_08128DB8[4] = { 0, 0, 0, 0 };

/* KeyButtonLayout blobs consumed by sub_080A70AC(). */
u8 gUnk_08128D60[9] = {
    0xFF, 0xD8, 0xFF, /* A button */
    0xFF, 0xD8, 0x00, /* B button */
    0xFF, 0xD8, 0x00, /* R button */
};

u8 gUnk_08128DBC[9] = {
    0xFF, 0xD8, 0xFF, /* A button */
    0xFF, 0xD8, 0x00, /* B button */
    0xFF, 0xD8, 0x00, /* R button */
};

/* Pause-menu screen 2/5 routing and placement tables. */
u8 gUnk_08128D38[4] = { 0, 0, 0, 0 };
u8 gUnk_08128D3C[32] = { 0 };
u8 gUnk_08128D43[64] = { 0 };
u8 gUnk_08128D51[4] = { 0, 0, 0, 0 };

u8 gUnk_08128C04[9] = {
    0xFF, 0xD8, 0xFF, /* A button */
    0xFF, 0xD8, 0x00, /* B button */
    0xFF, 0xD8, 0x00, /* R button */
};

typedef struct {
    u8 unk0;
    u8 unk1;
    u8 unk2;
    u8 unk3;
    u8 unk4;
    u8 unk5;
    u8 unk6;
    u8 unk7;
} PortPauseMenuGridEntry;

#define PORT_PMENU_GRID_ENTRY \
    {                         \
        0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0 \
    }

PortPauseMenuGridEntry gUnk_08128C14[16] = {
    PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY,
    PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY,
    PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY,
    PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY,
};

PortPauseMenuGridEntry gUnk_08128C94[16] = {
    PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY,
    PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY,
    PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY,
    PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY, PORT_PMENU_GRID_ENTRY,
};

/* Remaining pause/map-hint metadata tables. */
const struct_gUnk_08128D70 gUnk_08128D70[8] = {
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 },
};

const struct_gUnk_08128E94 gUnk_08128E94[16] = {
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
};

typedef struct {
    u8 unk0;
    u8 unk1;
    u8 unk2;
    u8 unk3;
    u8 unk4;
    u8 unk5;
    u8 unk6;
    u8 unk7;
} PortUnk08128DE8Entry;

PortUnk08128DE8Entry gUnk_08128DE8[17] = {
    { 0, 0, 0, 0, 0, 0, 0, 0 },
};

/* Core gameplay/session globals.
 *
 * These are heavily written during the TASK_GAME room-init path
 * (`MemClear(&gRoomTransition, sizeof...)`, `MemClear(&gRoomVars, sizeof...)`,
 * `MemCopy` into `gSave`, etc.). Leaving them as 256-byte unresolved stubs
 * causes large host-side overruns and state corruption that manifests as
 * black-screen stalls after "scene: room init". */
FadeControl gFadeControl;
GfxSlotList gGFXSlots;
PauseMenuOptions gPauseMenuOptions;
PlayerState gPlayerState;
PossibleInteraction gPossibleInteraction;
PriorityHandler gPriorityHandler;
RoomTransition gRoomTransition;
RoomVars gRoomVars;
SaveFile gSave;
void** gCurrentRoomProperties;
struct_02000040 gUnk_02000040;
struct_gUnk_020000C0 gUnk_020000C0[0x30];
ActiveScriptInfo gActiveScriptInfo;
ScriptExecutionContext gScriptExecutionContextArray[0x20];
ScriptExecutionContext gPlayerScriptExecutionContext;
SoundPlayingInfo gSoundPlayingInfo;
RoomMemory gRoomMemory[8];
RoomMemory* gCurrentRoomMemory = gRoomMemory;
Entity* gEnemyTarget;
CarriedEntity gCarriedEntity;
DiggingCaveEntranceTransition gDiggingCaveEntranceTransition;
FuseInfo gFuseInfo;
Entity* gPlayerClones[3];
const s8 gShakeOffsets[256];
const void* gLilypadRails[256];
UpdateContext gUpdateContext;
u8 gEntCount;
u8 gManagerCount;
u8 gCollidableCount;

/* Collision interaction matrix (`data/const/collisionMatrix.s::gCollisionMtx`
 * on GBA). `src/collision.c` indexes `ColSettings gCollisionMtx[173 * 34]`
 * (70584 B). The previous 4 KiB weak placeholder in port_unresolved_stubs.c
 * was far too small and could corrupt neighbouring BSS once collision runs. */
typedef struct {
    u8 orgKnockbackSpeed;
    u8 orgIframes;
    u8 orgKnockbackDuration;
    u8 tgtDamage;
    u8 orgConfusedTime;
    u8 tgtKnockbackSpeed;
    s8 tgtIframes;
    u8 tgtKnockbackDuration;
    u8 orgDamage;
    u8 tgtConfusedTime;
    u8 flags;
    u8 pad;
} PortCollisionMatrixColSettings;

_Static_assert(sizeof(PortCollisionMatrixColSettings) == 12,
               "sync with ColSettings in src/collision.c");

PortCollisionMatrixColSettings gCollisionMtx[173 * 34] __attribute__((aligned(16)));

/* Dungeon map scratch buffer (`src/common.c::DrawDungeonMap`). Needs
 * `sizeof(u32) * 0x800` bytes; the 4 KiB unresolved stub underran by 4 KiB. */
u32 gDungeonMap[0x800] __attribute__((aligned(16)));

/* Background animation slots (`include/backgroundAnimations.h`). */
BgAnimation gBgAnimations[MAX_BG_ANIMATIONS] __attribute__((aligned(16)));

u32 gUsedPalettes;
Palette gPaletteList[0x10];
Palette gUnk_02001A3C;
VBlankDMA gVBlankDMA;

/* Minimal host-safe sprite table.
 *
 * `gFrameObjLists` itself is provided by generated host assets
 * (`generated/port_assets/port_rom_assets.c`). Here we only provide an
 * inert fallback for `gSpritePtrs`.
 */
static const SpriteFrame sPortEmptySpriteFrame = {
    .numTiles = 0,
    .unk_1 = 0,
    .firstTileIndex = 0,
};

/* `gSpritePtrs[idx].frames[frame]` is read in multiple UI/entity paths.
 * Point every entry at a single inert frame table so any unresolved sprite
 * index safely yields a zero-tile frame. */
const SpritePtr gSpritePtrs[2048] = {
    [0 ... 2047] = {
        .animations = NULL,
        .frames = (SpriteFrame*)&sPortEmptySpriteFrame,
        .ptr = NULL,
        .pad = 0,
    },
};

TileEntity gSmallChests[8];
SpecialTileEntry gTilesForSpecialTiles[MAX_SPECIAL_TILES];
InteractableObject gInteractableObjects[0x20];

typedef struct {
    u16 tileType;
    u16 kind;
    u16 id;
    u16 type;
    u16 type2;
    u16 unk_a;
} PortSpecialTileSpawnData;

/* `sub_0801AC98` scans these until tileType==0xffff. Keeping unresolved
 * zero-filled placeholders causes an unbounded walk past the table. Use
 * explicit terminators so the scan is a safe no-op until real data lands. */
const PortSpecialTileSpawnData gUnk_080B44C0[] = {
    { .tileType = 0xFFFF, .kind = 0, .id = 0, .type = 0, .type2 = 0, .unk_a = 0 },
};

const PortSpecialTileSpawnData gUnk_080B44C2[] = {
    { .tileType = 0xFFFF, .kind = 0, .id = 0, .type = 0, .type2 = 0, .unk_a = 0 },
};

typedef struct {
    u16 index;
} PortFrameAnimation;

static const PortFrameAnimation sPortFrameAnimationZero = {
    .index = 0,
};

const PortFrameAnimation* gSpriteAnimations_322[512] = {
    [0 ... 511] = &sPortFrameAnimationZero,
};

static u16 sPortMoreSpritePtrsLut[4096];
static u16 sPortMoreSpritePtrsTiles[4096];
u16* gMoreSpritePtrs[3] = {
    NULL,
    sPortMoreSpritePtrsLut,
    sPortMoreSpritePtrsTiles,
};

u16 gExtraFrameOffsets[4096];

/* `Area` is ~0x894 bytes on the GBA but much larger on the host (64-bit
 * pointers in `RoomResInfo`). The 256-byte `PORT_UNRESOLVED_DATA(gArea)`
 * placeholder was far too small: `InitRoom` / `LoadRoomGfx` write the
 * full `roomResInfos[]` table and `pCurrentRoomInfo`, corrupting adjacent
 * BSS and producing flaky SIGSEGVs during the CI smoke test. */
Area gArea;

/* HUD / UI subsystem state.
 *
 * These were previously 256-byte weak placeholders in
 * port_unresolved_stubs.c, but the file-select task does
 * `MemClear(&gHUD, sizeof(gHUD))` (0x4b8 bytes on the host) and
 * `MemClear(&gUI, sizeof(gUI))` (0x480 bytes), and `UpdateUIElements`
 * iterates `gHUD.elements[0..MAX_UI_ELEMENTS-1]`. With a 256-byte
 * stub buffer those overrun adjacent BSS (clobbering e.g.
 * `gIntroState`) and the loop body reads garbage `used` bits past the
 * end of the buffer, calling a NULL `updateFunction` pointer. Defining
 * them with their real struct types here gives the host build BSS of
 * the correct size, mirroring how `gMain` / `gScreen` are handled. */
HUD gHUD;
UI gUI;

/* gUIElementDefinitions[] -- per-UI-element-type dispatch table.
 *
 * The real ROM table is defined in unported asm/data on disk. Each
 * entry's `updateFunction` is invoked for every "used" UI element on
 * every frame by `UpdateUIElements()`. The default 256-byte zero-filled
 * weak BSS placeholder previously used in `port_unresolved_stubs.c`
 * NULL-deref'd the first time the file-select task created a UI element
 * (`HandleFileScreenEnter -> sub_080A70AC -> CreateUIElement` populated
 * two slots; on the next frame `UpdateUIElements` called
 * `gUIElementDefinitions[type].updateFunction` which was NULL).
 *
 * We provide a strong host table whose `updateFunction` slot is a
 * no-op. This lives in `port_globals.c` (rather than the previously
 * tried `port_rom_data_stubs.c`) so it is unconditionally linked
 * regardless of whether `-DTMC_BASEROM=...` is set: the baserom-driven
 * build replaces `port_rom_data_stubs.c` with the generated
 * `port_rom_assets.c`, which does not (yet) provide this table.
 *
 * `UIElementDefinition` is a file-local typedef inside `src/ui.c`
 * (anonymous struct), so the only cross-TU contract for this symbol is
 * (a) the linker name `gUIElementDefinitions` and (b) the per-element
 * stride/layout the engine indexes through. The port-local
 * `PortUIElementDefinition` below mirrors the engine's field layout
 * exactly. The function-pointer signature uses `void*` so the type
 * never references the engine's anonymous `UIElement` typedef, keeping
 * the host TU self-contained. UIElementType currently has 11 values
 * (0..10); 16 entries provides headroom. */
typedef struct {
    u16 unk_0;
    u16 unk_2;
    u16 unk_4;
    u16 spriteIndex;
    void (*updateFunction)(void*);
    u8 buttonElementId;
    u8 unk_d;
    u8 unk_e;
    u8 unk_f;
} PortUIElementDefinition;

static void Port_UIElementUpdateNoOp(void* element) {
    (void)element;
    /* The real per-type update functions advance frame timers, swap
     * sprite frames, etc. With no real sprite/animation data wired in
     * yet, doing nothing is the safe behaviour for the SDL port: the
     * UI element stays in its initial state and does not trigger any
     * downstream NULL-deref through gSpritePtrs / gFrameObjLists. */
}

#define PORT_UIDEF_NOOP                                                                                   \
    {                                                                                                     \
        .unk_0 = 0, .unk_2 = 0, .unk_4 = 0, .spriteIndex = 0, .updateFunction = Port_UIElementUpdateNoOp, \
        .buttonElementId = 0, .unk_d = 0, .unk_e = 0, .unk_f = 0                                          \
    }

PortUIElementDefinition gUIElementDefinitions[16] = {
    PORT_UIDEF_NOOP, PORT_UIDEF_NOOP, PORT_UIDEF_NOOP, PORT_UIDEF_NOOP, PORT_UIDEF_NOOP, PORT_UIDEF_NOOP,
    PORT_UIDEF_NOOP, PORT_UIDEF_NOOP, PORT_UIDEF_NOOP, PORT_UIDEF_NOOP, PORT_UIDEF_NOOP, PORT_UIDEF_NOOP,
    PORT_UIDEF_NOOP, PORT_UIDEF_NOOP, PORT_UIDEF_NOOP, PORT_UIDEF_NOOP,
};

/* Message subsystem. */
Message gMessage;
TextRender gTextRender;

/* gMenu / gIntroState / gChooseFileState all alias the same EWRAM region
 * on the GBA (linker.ld lines 20-22 place all three at offset 0x80).
 * The decompiled code relies on this aliasing implicitly: e.g. after a
 * file-select state transition, `FileSelectTask` does
 *   MemClear(&gChooseFileState, sizeof(gChooseFileState));
 * which on the GBA also zeros gMenu (so menuType resets to 0 and
 * `HandleFileNew[0] == sub_08051090` runs on entry to STATE_NEW to
 * initialise the on-screen keyboard). The previous SDL stubs gave each
 * symbol its own 256-byte buffer, so MemClear via one alias did NOT
 * clear the others; menuType stayed at the value
 * `sub_08050848` had set for the previous state (1), and
 * `HandleFileNew` jumped straight into the keyboard handler
 * `sub_080610B8` without ever calling `sub_08051090` to load the
 * keyboard glyph table. Provide a single shared backing buffer and
 * alias the other two symbols to it (GCC/Clang `alias` attribute), so
 * the GBA's union semantics are preserved on the host. The three
 * structs are all <= 0x40 bytes (Menu/ChooseFileState are 0x30 each;
 * IntroState fits in the same span); 0x40 leaves headroom and matches
 * the spacing to the next aliased symbol gUnk_02000090 in linker.ld. */
char gMenu[0x40] __attribute__((aligned(16)));
extern char gIntroState[0x40] __attribute__((alias("gMenu")));
extern char gChooseFileState[0x40] __attribute__((alias("gMenu")));

/* `Window` is declared as a file-local typedef inside src/message.c (it
 * is not exposed via a header), but the storage for `gNewWindow` /
 * `gCurrentWindow` lives in BSS on the GBA via linker.ld. Mirror the
 * exact struct tag and typedef name here so the host-side definition is
 * type-compatible with `extern Window ...` declarations in message.c.
 * The field layout is kept verbatim from the message subsystem's local
 * definition. */
typedef struct Window {
    u8 unk0;
    u8 active;
    u8 unk2;
    u8 unk3;
    u8 xPos;
    u8 yPos;
    u8 width;
    u8 height;
} Window;
Window gNewWindow;
Window gCurrentWindow;

/* Palette buffer (mirrors the full hardware palette in EWRAM). Linked
 * game code treats this as a 0x400-byte mirror and may copy the entire
 * BG+OBJ palette into it, so allocate 512 u16 entries (1024 B). */
u16 gPaletteBuffer[512];
u8 gPaletteBufferBackup[0x400];

/* VBlank DMA staging buffers used by several map/effect managers.
 * 0x500 u16 entries per page; host path uses one of two pages via
 * gUnk_03003DE4[0], so allocate 0xA00 entries total. */
u16 gUnk_02017AA0[0xA00];
u16 gUnk_02017BA0[0xA00];

/* Dungeon-map palette rotation scratch (8 entries). */
u16 gUnk_02017830[8];


/* IWRAM-resident byte arrays. `gUnk_03003DE4` is a 12-byte mailbox used
 * by the VBlank-DMA helpers in src/main.c::SetVBlankDMA(). */
u8 gUnk_03003DE4[0xC];
u8 gUnk_03003DE0;
Screen gUnk_03001020;
LinkedList2* gUnk_02018EA0;

static void Port_EnemyActionNoOp(Entity* entity) {
    (void)entity;
}

static void Port_MenuOverlayNoOp(void) {
}

/* Generic enemy action-dispatch fallback used by many enemy TUs. */
void (*const gUnk_080012C8[256])(Entity*) = {
    [0 ... 255] = Port_EnemyActionNoOp,
};

/* Fuser metadata fallback:
 * - gUnk_08001A7C[fuserId][0..2] are consumed as three u16 fields.
 * - gUnk_08001DCC[fuserId][0] gates offer availability against progress.
 * Keep both tables safe and deterministic on host when ROM data is absent. */
static u16 sPortFuserNpcDefault[3] = { 0, 0, 0 };
u16* gUnk_08001A7C[256] = {
    [0 ... 255] = sPortFuserNpcDefault,
};

static u8 sPortFuserOfferDefault[6] = { 0xFF, 0, 0, 0, 0, 0 };
u8* gUnk_08001DCC[256] = {
    [0 ... 255] = sPortFuserOfferDefault,
};

const u16 gUnk_08016984 = 0;
u16 gUnk_080B2CD8[4];
u16 gUnk_080B4410[7];
void (*const gUnk_080B4458[8])(void) = {
    [0 ... 7] = Port_MenuOverlayNoOp,
};
s16 gUnk_080B4468[16];
s16 gUnk_080B4478[16];
s16 gUnk_080B4488[16];
s16 gUnk_080B4490[16];
u16 gUnk_080B44A0[32];
s16 gUnk_080B44A8[16];
u32 gUnk_080B44B8[256];
u16 gUnk_080C8F2C[11];
u16 gUnk_080C8F54[11];
static u32 sPortChargeBarTiles0[0x30];
static u32 sPortChargeBarTiles1[0x30];
static u32 sPortChargeBarTiles2[0x30];
static u32 sPortChargeBarTiles3[0x30];
u32* gUnk_080C8F7C[4] = {
    sPortChargeBarTiles0,
    sPortChargeBarTiles1,
    sPortChargeBarTiles2,
    sPortChargeBarTiles3,
};
u8 gUnk_080C9044[8];
u16 gUnk_080C9058[2];
/* Minimal looping frame script for Ezlo nag icon animation. */
Frame gUnk_080C9094[3] = {
    { .index = 0, .duration = 1, .spriteSettings.raw = 0, .frameSettings.raw = 0x80 },
    { .index = 0, .duration = 30, .spriteSettings.raw = 0, .frameSettings.raw = 0x80 },
    { .index = 1, .duration = 30, .spriteSettings.raw = 0, .frameSettings.raw = 0x80 },
};
const s8 gUnk_080CA2B4[16];
const s8 gUnk_080CA6D4[3] = { 0, 8, -8 };
static const Hitbox sPortSpearMoblinHitbox = {
    .offset_x = 0,
    .offset_y = 0,
    .width = 8,
    .height = 8,
};
const Hitbox* const gUnk_080CC944[4] = {
    &sPortSpearMoblinHitbox,
    &sPortSpearMoblinHitbox,
    &sPortSpearMoblinHitbox,
    &sPortSpearMoblinHitbox,
};
typedef struct {
    union SplitHWord unk0;
    u8 unk2;
    u8 unk3;
} PortGleerokHeapStruct2;
PortGleerokHeapStruct2 gUnk_080CD7C4[8];
static void Port_GleerokNoOp(void* this_) {
    (void)this_;
}
void (*const gUnk_080CD7E4[16])(void*) = {
    [0 ... 15] = Port_GleerokNoOp,
};
u8 gUnk_080CD7F8[1] = { 0xFF };
void (*const gUnk_080CD810[16])(void*) = {
    [0 ... 15] = Port_GleerokNoOp,
};
void (*const gUnk_080CD828[8])(void*) = {
    [0 ... 7] = Port_GleerokNoOp,
};
u8 gUnk_080CD840[4];
u8 gUnk_080CD844[128];
void (*const gUnk_080CD848[8])(void*) = {
    [0 ... 7] = Port_GleerokNoOp,
};
u8 gUnk_080CD850[1] = { 0xFF };
u8 gUnk_080CD854[1] = { 0 };
static const u8 sPortGleerokWeightTable0[1] = { 0xFF };
static const u8 sPortGleerokWeightTable1[1] = { 0xFF };
static const u8 sPortGleerokWeightTable2[1] = { 0xFF };
const u8* gUnk_080CD86C[3] = {
    sPortGleerokWeightTable0,
    sPortGleerokWeightTable1,
    sPortGleerokWeightTable2,
};
static const u8 sPortGleerokThresholdTable0[1] = { 0 };
static const u8 sPortGleerokThresholdTable1[1] = { 0 };
static const u8 sPortGleerokThresholdTable2[1] = { 0 };
const u8* gUnk_080CD878[3] = {
    sPortGleerokThresholdTable0,
    sPortGleerokThresholdTable1,
    sPortGleerokThresholdTable2,
};
u8 gUnk_080CD884[2];
const u8 gUnk_080D15B4[4];
TileEntity gUnk_080FEAC8[256];
const EntityData gUnk_080FEBE8[1] = {
    { .kind = 0xFF },
};
EntityData gUnk_080FEC28[1] = {
    { .kind = 0xFF },
};
EntityData gUnk_080FECC8[1] = {
    { .kind = 0xFF },
};
const EntityData gUnk_080FED18[1] = {
    { .kind = 0xFF },
};
EntityData gUnk_080FED58[1] = {
    { .kind = 0xFF },
};
const EntityData gUnk_080FEE18[1] = {
    { .kind = 0xFF },
};
const EntityData gUnk_080FEE38[1] = {
    { .kind = 0xFF },
};
const EntityData gUnk_080FEE48[1] = {
    { .kind = 0xFF },
};
const EntityData gUnk_080FEE58[1] = {
    { .kind = 0xFF },
};
EntityData gUnk_080FEE78[1] = {
    { .kind = 0xFF },
};

/* Item pickup / Pegasus dash input masks (indexed by `animationState >> 1`). */
u16 gUnk_0811BE38[8];
const u16 gUnk_0811BE40[8];

/* `cutsceneMiscObject.c` ROM tables (host zero-init until assets land). */
u8 gUnk_08122AE0[32];
u16 gUnk_08122AE8[32];
s8 gUnk_08122AF8[64];
u16 gUnk_08122B00[32];
s16 gUnk_08122B0E[8];
u16 gUnk_08122B1E[8];
Coords8 gUnk_08122B2E[256];
typedef struct {
    Hitbox hit;
    u8 _8[4];
    u8 _c;
} PortCutsceneHitboxCfg;
PortCutsceneHitboxCfg gUnk_08122B3C[256];

/* Staffroll task tables (layout must match `src/staffroll.c`). */
typedef struct {
    u8 menuType;
    u16 font;
    u8 unk_3;
    u16 transitionTimer;
    u16 gfxEntry;
    u16 bg2XOffset;
    u16 sm_unk_14;
} PACKED StaffrollEntry;

typedef struct {
    u8 paletteGroup;
    u8 gfxGroup;
} PACKED StaffrollGfxEntry;

StaffrollEntry gUnk_081272F0[128];
StaffrollEntry gUnk_08127644[128];
StaffrollEntry gUnk_08127998[128];
const StaffrollGfxEntry gUnk_08127CEC[64];

extern void StaffrollTask_State0(void);
extern void StaffrollTask_State1(void);
extern void StaffrollTask_State2(void);
extern void StaffrollTask_State3(void);
extern void StaffrollTask_State1MenuType0(void);
extern void StaffrollTask_State1MenuType1(void);
extern void StaffrollTask_State1MenuType2(void);
extern void StaffrollTask_State1MenuType3(void);
extern void StaffrollTask_State1MenuType4(void);
extern void StaffrollTask_State1MenuType5(void);
extern void StaffrollTask_State1MenuType6(void);
extern void StaffrollTask_State1MenuType7(void);

void (*const gUnk_08127D00[])(void) = {
    StaffrollTask_State0,
    StaffrollTask_State1,
    StaffrollTask_State2,
    StaffrollTask_State3,
};

void (*const gUnk_08127D10[])(void) = {
    StaffrollTask_State1MenuType0,
    StaffrollTask_State1MenuType1,
    StaffrollTask_State1MenuType2,
    StaffrollTask_State1MenuType3,
    StaffrollTask_State1MenuType4,
    StaffrollTask_State1MenuType5,
    StaffrollTask_State1MenuType6,
    StaffrollTask_State1MenuType7,
};

/* `HyruleTownTileSetManager_BuildSecondOracleHouse` DMAs one BG screen from here. */
u8 gUnk_086E8460[0x800] __attribute__((aligned(16)));

#if !defined(DEMO_USA) && !defined(DEMO_JP)
/* `eeprom.c` declares this `extern` and assigns it in EEPROMConfigure(); a
 * NULL default before configure runs was observable on the host when
 * adjacent BSS was clobbered. Point at the TU-local 512-byte profile. */
typedef struct EEPROMConfig EEPROMConfig;
extern const EEPROMConfig gEEPROMConfig512;
const EEPROMConfig* gEEPROMConfig = &gEEPROMConfig512;
#endif

/* Portal / macro-player scratch (`enterPortalSubtask.c`, `subtask.c`,
 * `macroPlayer.c`, etc.); `MemClear(&gUnk_02018EB0, 0x28)` matches the GBA
 * span (see `struct_02018EB0` in beanstalkSubtask.h). */
struct_02018EB0 gUnk_02018EB0;

/* `common.c::sub_0801AE44` clears 0x780 bytes as u16 fill. */
s16 gUnk_02018EE0[0x780 / sizeof(s16)];

/* `subtask.c` mirrors `gUI.unk_2a8` (0x100 bytes, see main.h). */
u8 gUnk_03000420[0x100];

/* Pause / UI digit tiles: `DmaCopy32` indexes up to (19 * 8) u32 words. */
u32 gUnk_085C4620[256];

u16* gZeldaFollowerText[8];

u8 gzHeap[0x1000];

u8 gUpdateVisibleTiles;

OAMCommand gOamCmd;
u8 gTextGfxBuffer[0x1000];
struct {
    u8 unk_00;
    u8 unk_01[1];
    s8 choiceCount;
    s8 currentChoice;
    u8 unk_04[4];
    u16 unk_08[4];
    u16 unk_10[4];
} gMessageChoices;
String8 gUnk_020227E8[8];
u8 gUnk_020227DC;
u8 gUnk_020227F0;
u8 gUnk_020227F8;
u8 gUnk_02022800;
u16 gUnk_02022830[0xC00];
u16 gUnk_020246B0[0xC00];

typedef struct {
    u16 unk0;
    s8 unk2;
    s8 unk3;
    u8 filler[0x4];
    u16 unk8[4];
    u16 unk10[4];
} PortTextState02034330;

PortTextState02034330 gUnk_02034330;
struct_02034480 gUnk_02034480;
u8 gUnk_02034492[0x20];
u8 gUnk_020344A0[8];

typedef struct {
    u8 unk0;
    u8 unk1;
    u16 unk2;
} PortFadePaletteState;

PortFadePaletteState gUnk_020354C0[0x20];
/* `common.c` treats this symbol as the heap-entry table rooted at gzHeap+2. */
extern u8 gUnk_02035542[0x0];
__asm__(".global gUnk_02035542\n.set gUnk_02035542, gzHeap+2");
WStruct gUnk_02036540[4];

/* Minish-path / rafters parallax tilemap source (second plane at +0x2000). */
u8 gUnk_02006F00[0x4000] __attribute__((aligned(16)));
u16 gUnk_0200B640;
u8 gUnk_02036A58[0x100];
u8 gUnk_02036AD8[0x200];
bool32 gUnk_02036BB8;

/* Entity arena layout. The GBA build places `gPlayerEntity`,
 * `gAuxPlayerEntities`, and `gEntities` contiguously in EWRAM via
 * `linker.ld`, and a number of subsystems exploit that adjacency --
 * notably `src/entity.c::EraseAllEntities`, which calls
 * `MemClear(&gPlayerEntity.base, 10880)` and walks past the end of
 * `gPlayerEntity` into the auxiliary and pooled entities. On the
 * host build the three globals would otherwise be scattered in BSS
 * (each previously a 256-byte weak placeholder), so the oversized
 * clear would punch through unrelated globals (e.g. `gIntroState`)
 * and wedge boot in a loop.
 *
 * Pin them into a single struct in declaration order and expose the
 * three engine names as symbol aliases (via inline assembly `.set`
 * directives) that point at the matching struct fields. Sizes on the
 * host are larger than on the GBA (8-byte pointers instead of 4),
 * so the engine's `10880` byte clear is *short* of what it would
 * need to wipe the whole host-sized arena -- but that matches the
 * ROM build's observable behaviour and we deliberately preserve it. */
struct port_entity_arena {
    PlayerEntity player;
    GenericEntity aux[MAX_AUX_PLAYER_ENTITIES];
    GenericEntity ents[MAX_ENTITIES];
};

struct port_entity_arena sPortEntityArena __attribute__((aligned(16), used));

/* Hard-code the host offsets and sanity-check them with static asserts
 * so the asm() directives below remain compile-time constants. The
 * numbers reflect:
 *   offsetof(player) = 0
 *   offsetof(aux)    = sizeof(PlayerEntity)
 *   offsetof(ents)   = sizeof(PlayerEntity) + 7 * sizeof(GenericEntity)
 * with sizeof(PlayerEntity)=184 and sizeof(GenericEntity)=176 on a
 * 64-bit host (entity.h struct layout assertions are no-ops here, so
 * we re-assert the relevant offsets explicitly). */
#define PORT_ARENA_OFF_AUX 184
#define PORT_ARENA_OFF_ENTS (184 + 1232)

_Static_assert(offsetof(struct port_entity_arena, aux) == PORT_ARENA_OFF_AUX,
               "host PlayerEntity size drifted; update PORT_ARENA_OFF_AUX");
_Static_assert(offsetof(struct port_entity_arena, ents) == PORT_ARENA_OFF_ENTS,
               "host GenericEntity size drifted; update PORT_ARENA_OFF_ENTS");

#define PORT_STR(x) #x
#define PORT_XSTR(x) PORT_STR(x)
/* Publish the three engine names as symbol aliases pointing at the
 * matching fields of `sPortEntityArena`. Uses GNU `.set` directive
 * syntax via `__asm__`; this is GCC / Clang only. Microsoft Windows
 * (MSVC) builds are not supported by this port -- see
 * docs/sdl_port.md -- so no MSVC fallback is needed here. */
__asm__(".globl gPlayerEntity\n"
        ".set   gPlayerEntity, sPortEntityArena\n"
        ".globl gAuxPlayerEntities\n"
        ".set   gAuxPlayerEntities, sPortEntityArena+" PORT_XSTR(
            PORT_ARENA_OFF_AUX) "\n"
                                ".globl gEntities\n"
                                ".set   gEntities, sPortEntityArena+" PORT_XSTR(PORT_ARENA_OFF_ENTS) "\n");

/* Entity-list heads. The empty-list convention is `head->first ==
 * head->last == head` (a one-element circular list whose only node is
 * the sentinel head itself); any other value -- including the all-
 * zero BSS state -- causes the sentinel-walk loops in
 * `src/entity.c::DeleteAllEntities` / `EraseAllEntities` to deref
 * NULL on the very first iteration. The matching ROM build relies on
 * `src/entity.c::sub_0805E98C()` running before the first list
 * traversal to install that pattern, but the host build's call graph
 * reaches `EraseAllEntities` *before* its own `sub_0805E98C` line,
 * and the (previously weak) BSS-only definition of `gEntityLists`
 * came up zero-filled. Promoting the array to a strong host
 * definition and having `Port_GlobalsInit()` pre-install the
 * self-loop pattern before `AgbMain` makes the boot path immediately
 * match the engine's "freshly initialised" expectation, regardless
 * of which entry point runs first. The same treatment applies to
 * `gEntityListsBackup`, which `sub_0805E958` / `sub_0805E974`
 * `MemCopy` against and which any later traversal would otherwise
 * also walk through NULL. */
/* EWRAM mirrors (linker.ld): gUnk_02021F00 @ 0x02021F00..0x02021F20,
 * gUnk_020342F8 @ 0x020342F8..0x02034330. Pulled in by the title-screen
 * object pipeline (pullableLever / npc / delayedEntityLoadManager). */
u8 gUnk_020342F8[0x38];
u16 gUnk_02021F00[0x10];

/* Active item behaviors (see include/player.h static_assert). */
ItemBehavior gActiveItems[MAX_ACTIVE_ITEMS];

LinkedList gEntityLists[9];
LinkedList gEntityListsBackup[9];

static void port_init_entity_lists(LinkedList* lists) {
    int i;
    for (i = 0; i < 9; i++) {
        lists[i].first = (Entity*)&lists[i];
        lists[i].last = (Entity*)&lists[i];
    }
}

/* Idempotent host-side initializer for globals that the engine assumes
 * are non-zero before any game code runs. Called explicitly from
 * `src/platform/sdl/main.c` before `AgbMain` so we do not rely on
 * compiler-specific constructor ordering. Safe to call multiple
 * times. */
void Port_GlobalsInit(void) {
    static int initialised = 0;
    if (initialised) {
        return;
    }
    initialised = 1;
    port_init_entity_lists(gEntityLists);
    port_init_entity_lists(gEntityListsBackup);
    Port_MapDataInit();
}
