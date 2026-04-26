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
#include "common.h"
#include "entity.h"
#include "main.h"
#include "message.h"
#include "player.h"
#include "room.h"
#include "screen.h"
#include "ui.h"

#include <stddef.h>

/* Main system state. */
Main gMain;
Input gInput;
Screen gScreen;
RoomControls gRoomControls;
struct_02000010 gUnk_02000010;
u32 gRand;

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

/* IWRAM-resident byte arrays. `gUnk_03003DE4` is a 12-byte mailbox used
 * by the VBlank-DMA helpers in src/main.c::SetVBlankDMA(). */
u8 gUnk_03003DE4[0xC];

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
}
