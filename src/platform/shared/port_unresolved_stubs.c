/**
 * @file port_unresolved_stubs.c
 * @brief Weak placeholders for link references that the SDL port
 *        has not yet provided a real implementation for.
 *
 * Sub-step 2b.4b of the SDL-port roadmap (see docs/sdl_port.md). With
 * TMC_LINK_GAME_SOURCES=ON, the tmc_game_sources static library
 * (every src[/]**[/]*.c that compiles under __PORT__) pulls in roughly
 * 850 additional symbols defined only by files the host build cannot
 * yet assemble or port: the raw ARM assembly in asm/src/
 * (ram_*, script_*, sub_080Bxxxx, area/enemy layout blobs), the
 * m4a sound engine in src/gba/m4a.c (not yet linked in), the large
 * const-data tables under data/ (sprite / palette / collision
 * layout, song + sfx ID tables, area metadata), and a handful of
 * forward-declared helpers that straddle remaining unported files.
 *
 * Each symbol below is a **weak** definition, so:
 *   - if a later PR provides a real (non-weak) definition with the same
 *     name, the linker picks the real one and these drop out;
 *   - if the symbol is read as data at runtime, the game observes a
 *     zero-filled buffer (BSS) - the same behaviour as freshly-erased
 *     Flash save memory, which the existing `src/save.c` validation
 *     checks handle idempotently;
 *   - if the symbol is called as a function, control jumps into the
 *     .bss section which is mapped read/write-but-not-execute on every
 *     host platform we support, producing an immediate SIGSEGV - the
 *     same "loud abort" contract asm_stubs.c uses.
 *
 * This file was produced by inspecting the ld error log from a
 * `TMC_LINK_GAME_SOURCES=ON` build. To regenerate after adding more
 * real definitions, see the roadmap notes in docs/sdl_port.md.
 */
#include <stdint.h>
#include "map.h"

#if defined(__GNUC__) || defined(__clang__)
#define PORT_WEAK __attribute__((weak))
#else
#define PORT_WEAK
#endif

/* ---- Function-like stubs ---------------------------------------------- */
/* No unresolved function placeholders remain in this TU.
 * Earlier abort-trap stubs have been replaced by strong host definitions
 * in dedicated files as systems were ported in. */
/* Tile clone/index/set helpers are strongly defined in
 * port_entity_runtime.c. */
/* Tile/act-tile/collision accessors are strongly defined in
 * port_entity_runtime.c. */
/* Tile-type query wrappers are strongly defined in
 * port_entity_runtime.c. */
/* The unprefixed `m4a*` entry points are now strongly defined by
 * `src/gba/m4a.c` itself (PR #7 part 1), which is compiled into
 * `tmc_game_sources` with the asm-only mixer routines stubbed
 * silently in `src/platform/shared/m4a_host.c`. The weak abort-trap
 * placeholders that used to live here have been removed; if a regression
 * drops `src/gba/m4a.c` from the leaf set, the link will fail with a
 * clear "undefined reference" instead of silently aborting at runtime. */
/* ram_ClearAndUpdateEntities, ram_CollideAll, ram_DrawEntities,
 * ram_MakeFadeBuff256, and ram_UpdateEntities have tailored silent
 * overrides in ram_silent_stubs.c (see roadmap 2b.4b "runtime flip"
 * notes in docs/sdl_port.md); they no longer need an abort-trap weak
 * placeholder here. ram_DrawDirect and ram_sub_080ADA04 are now
 * strongly defined in port_oam_renderer.c (PR A of the title-screen
 * OAM-pipeline plan), so no abort-trap placeholder is needed for
 * them either. */
/* UpdateScrollVram has a strong host definition in screenTileMap.c. */
/* ram_IntrMain has a strong host definition in interrupts.c. */
/* sub_080B1B84 / sub_080B1BA4 are strongly defined in
 * port_entity_runtime.c. */

/* ---- Data placeholders ------------------------------------------------ */
/*
 * This TU used to emit a large batch of generic blob placeholders.
 * As the host port matured, those have been replaced by either:
 *   - strong typed host definitions in dedicated TUs (`port_globals.c`,
 *     `m4a_host.c`, `port_rom_data_stubs.c`, etc.), or
 *   - explicit weak typed fallbacks below where "missing data degrades
 *     safely" and real extracted data may override at link time.
 *
 * Keep this section focused on named, typed fallbacks only.
 */

/* ButtonUIElement_Actions / EzloNagUIElement_Actions: strong host definitions
 * in ui.c (__PORT__). RupeeKeyDigits: port_ui_rom_data.c (USA baserom). */
/* bgm*: strong host stand-ins in port_bgm_data_stubs.c. */
/* gActiveItems: strong host definition in port_globals.c. */
/* gArea: strong definition in port_globals.c (real `Area` size on host). */
/* Area-indexed resource tables.
 *
 * These are pointer arrays indexed by `AreaID` (0..AREA_98), not opaque
 * blobs. The default 256-byte placeholder only holds 32 pointers on a
 * 64-bit host, so entering areas >= 32 (e.g. area 34 during save-load)
 * reads/writes out of bounds and corrupts neighboring globals in room init.
 * Keep them weak but give them the correct pointer-array extent so missing
 * data degrades to NULL entries instead of memory corruption. */
#define PORT_AREA_TABLE_COUNT 0x99
/*
 * Keep these placeholders weak so any real table emitted from baserom
 * extraction can override them. They are arrays of pointers-to-arrays:
 *   area -> room -> payload pointer
 * so the element type must preserve pointer depth (void** / void***),
 * not plain void*.
 */
void** gAreaRoomHeaders[PORT_AREA_TABLE_COUNT] PORT_WEAK __attribute__((aligned(16)));
void** gAreaRoomMaps[PORT_AREA_TABLE_COUNT] PORT_WEAK __attribute__((aligned(16)));
void*** gAreaTable[PORT_AREA_TABLE_COUNT] PORT_WEAK __attribute__((aligned(16)));
void** gAreaTileSets[PORT_AREA_TABLE_COUNT] PORT_WEAK __attribute__((aligned(16)));
void* gAreaTiles[PORT_AREA_TABLE_COUNT] PORT_WEAK __attribute__((aligned(16)));
/* gAuxPlayerEntities lives in port_globals.c (entity arena). */
/* BG tilemap buffers. Real declarations in include/vram.h are
 * `u16 gBG{0,1,2}Buffer[0x400]` (2 KiB each) and `u16 gBG3Buffer[0x800]`
 * (4 KiB). The default 256-byte PORT_UNRESOLVED_DATA stub overflowed
 * the same way `gOAMControls` did: `MemClear(&gBG0Buffer, sizeof(...))`
 * in the file-select task, the `MemCopy(&gBG3Buffer[0x80], &gBG1Buffer[0x80], 0x400)`
 * in `sub_08051458` (on-screen keyboard glyph copy), and the
 * `gBG3Buffer[a[0]*2 + 0xc3 + a[1]*0x40]` lookup in `sub_080610B8` all
 * read/write well past index 256, scribbling adjacent stub buffers and
 * making the keyboard cursor's character lookup return 0 for every
 * position. Sizing the stubs to match the real declarations fixes both
 * the corruption and the lookup. The matching ROM build keeps these in
 * EWRAM via linker.ld and is unaffected. */
char gBG0Buffer[0x800] PORT_WEAK __attribute__((aligned(16)));
char gBG1Buffer[0x800] PORT_WEAK __attribute__((aligned(16)));
char gBG2Buffer[0x800] PORT_WEAK __attribute__((aligned(16)));
char gBG3Buffer[0x1000] PORT_WEAK __attribute__((aligned(16)));
/* gBgAnimations: strong host definition in port_globals.c (real
 * `BgAnimation[MAX_BG_ANIMATIONS]` size; the 4 KiB placeholder was
 * oversized but untyped). */
/* gCarriedEntity: strong host definition in port_globals.c. */
/* gChooseFileState aliases gMenu via an `alias` attribute in
 * port_globals.c (preserves the GBA's union over offset 0x80). */
/* gCollisionMtx / gDungeonMap: strong host definitions in port_globals.c. */
/* gDiggingCaveEntranceTransition: strong host definition in port_globals.c. */
/* gEEPROMConfig: strong default in port_globals.c (non-demo builds). */
/* gPlayerEntity / gAuxPlayerEntities / gEntities have strong host
 * definitions in port_globals.c (placed contiguously in a single
 * named BSS section to satisfy `MemClear(&gPlayerEntity, 10880)`).
 * gEntityLists / gEntityListsBackup likewise. */
/* Figurine table fallback.
 *
 * `src/menu/figurineMenu.c` treats this as an array of
 *   struct { u8* pal; u8* gfx; int size; int zero; }
 * and indexes up to 136 entries depending on save progression.
 * Keep it weak and typed so reads stay in-bounds and future real data
 * definitions override this host fallback without link changes. */
typedef struct {
    u8* pal;
    u8* gfx;
    int size;
    int zero;
} PortFigurine;
PORT_WEAK PortFigurine gFigurines[136] __attribute__((aligned(16)));
/* gFuseInfo: strong host definition in port_globals.c. */
/* gGfxGroups / gGlobalGfxAndPalettes have moved to
 * src/platform/shared/port_rom_data_stubs.c, which provides strong
 * host-side stand-ins so that LoadGfxGroup() short-circuits cleanly
 * during the Nintendo / Capcom logo step instead of NULL-deref'ing on
 * an empty BSS placeholder. See docs/sdl_port.md (PR #2b.4b). */
/* gHUD has moved to src/platform/shared/port_globals.c, where it is
 * defined with its real `HUD` struct type so that the host build's
 * BSS allocation matches sizeof(gHUD) (== 0x4b8 on a 64-bit host).
 * The 256-byte weak placeholder previously here was overrun by
 * `MemClear(&gHUD, sizeof(gHUD))` in HandleFileScreenEnter() and the
 * out-of-bounds reads in `UpdateUIElements` produced a NULL function
 * pointer call. */
/* gIntroState aliases gMenu via an `alias` attribute in port_globals.c. */
/* gLilypadRails: strong host definition in port_globals.c. */
/* gMPlayInfos / gMPlayInfos2 / gMPlayTracks have moved to
 * src/platform/shared/m4a_host.c, which provides strong host BSS of
 * the proper `MusicPlayerInfo` / `MusicPlayerTrack` types. The
 * host-side extents are 0x1C / 0x4 / 0x52 entries; for gMPlayTracks,
 * the 0x52 size is derived from gMusicPlayers[] usage/high-water-mark
 * in the sound code rather than from an explicit bound in
 * src/sound.c. The 256-byte weak placeholders previously here were
 * dereferenced way past their end by `m4aSoundInit()`'s `MPlayOpen`
 * loop once `NUM_MUSIC_PLAYERS` was promoted from 0 back to 0x20 in
 * PR #7 part 2.2.2.3 — see docs/sdl_port.md. */
MapLayer gMapBottom __attribute__((aligned(16)));
/* Main map-data blob source (`LoadMapData` reads offset ranges out of this).
 * Keep a host-local backing store large enough that offset-based reads don't
 * walk off a 256-byte placeholder when room/map assets are missing. */
u8 gMapData[16 * 1024 * 1024] __attribute__((aligned(16)));
/* gMapDataBottomSpecial / gMapDataTopSpecial are the special-tile
 * scratch buffers for the two map layers. ClearTileMaps() (called
 * from HandleFileScreenEnter() via gameUtils.c, and from playerUtils.c
 * map setup) does `MemClear(&gMapDataBottomSpecial, 0x8000)` and
 * `MemClear(&gMapDataTopSpecial, 0x8000)`, and playerUtils.c indexes
 * up to gMapDataBottomSpecial[0x2000] + 0x4000 bytes (== byte offset
 * 0x8000). The 256-byte default placeholder is therefore far too
 * small: the MemClear overruns into adjacent BSS and clobbers
 * gEEPROMConfig, which then NULL-derefs on the next EEPROMRead and
 * crashes the file-select screen. Size both buffers to 0x8000 bytes
 * (== 0x4000 u16 entries) to match the game's expected extent. */
char gMapDataBottomSpecial[0x8000] PORT_WEAK __attribute__((aligned(16)));
char gMapDataTopSpecial[0x8000] PORT_WEAK __attribute__((aligned(16)));
MapLayer gMapTop __attribute__((aligned(16)));
/* gMenu has its strong host definition in port_globals.c (with
 * gIntroState / gChooseFileState aliased to it). */
/* gOAMControls is sized explicitly. The struct in include/vram.h is
 *   8 (header) + 0x18 (_0[]) + 0x80*8 (oam[]) + 0xA0*8 (unk[]) = 0x920 bytes,
 * far larger than the 256-byte default that PORT_UNRESOLVED_DATA hands out.
 * `CopyOAM` in src/affine.c writes `*d = 0x2A0` to every OAM slot from the
 * `updated` index through 0x80 (i.e. up to 1 KiB of writes via
 * `&gOAMControls.oam[i]`), so a 256-byte allocation overflowed by ~1.7 KiB
 * and silently scribbled `0x02 0xA0 0x00 0x00 0x00 0x00 0x00 0x00` every
 * 8 bytes into whichever globals followed gOAMControls in the SDL link
 * order. With this layout that overflow landed on `gMenu`, resetting
 * `gMenu.column_idx` to 2 and `gGenericMenu.unk10.a[0..1]` to {0xA0, 0x02}
 * every frame, which made the file-select on-screen keyboard appear stuck
 * (HandleFileNew could not advance because START / A re-targeted the same
 * row each frame, and the chosen "character" indexed off the end of
 * gBG3Buffer so `gSave.name` never received a non-zero byte). The matching
 * ROM build is unaffected: linker.ld places gOAMControls at IWRAM 0x0
 * with plenty of room before the next symbol. */
char gOAMControls[0xB74] PORT_WEAK __attribute__((aligned(16)));
/* gPaletteGroups has moved to src/platform/shared/port_rom_data_stubs.c
 * for the same reason as gGfxGroups above (LoadPaletteGroup needs a
 * non-NULL terminator entry to short-circuit). */
/* gPaletteList has a strong typed host definition in port_globals.c. */
/* `src/enemy/peahat.c` uses this as `const s8[2]` (±4 heading jitter).
 * The decomp keeps its in-file definition commented out for alignment,
 * so provide a weak typed fallback here. */
PORT_WEAK const s8 gPeahatChargeDirectionOffsets[2] __attribute__((aligned(16))) = {
    4, -4,
};
/* gPlayerClones: strong host definition in port_globals.c. */
/* gPlayerEntity lives in port_globals.c (entity arena). */
/* gFadeControl / gPlayerState are strongly defined in port_globals.c. */
/* gRoomMemory / gRoomTransition / gRoomVars / gSave / script context
 * globals are strongly defined in port_globals.c. */
/* gShakeOffsets: strong host definition in port_globals.c. */
/* Sprite metadata tables have strong typed host definitions in
 * port_globals.c. */
/* gCollidableCount / gFrameObjLists / gSpritePtrs have strong typed host
 * definitions in port_globals.c. */
/* gCurrentRoomProperties / gInteractableObjects / gSmallChests /
 * gTilesForSpecialTiles have strong typed host definitions (or alias)
 * in port_globals.c. */
/* gUI has moved to src/platform/shared/port_globals.c (real `UI`
 * type) for the same reason as gHUD above. */
/* gUIElementDefinitions has moved to
 * src/platform/shared/port_rom_data_stubs.c, which provides a strong
 * host stand-in whose `updateFunction` slot is a no-op. The previous
 * 256-byte zero-init weak placeholder NULL-deref'd as soon as the
 * file-select task created its first UI element. */
/* gUnk_02001A3C / gUnk_02006F00 / gUnk_0200B640: strong host definitions
 * in port_globals.c. */
/* gUnk_02018EB0 / gUnk_02018EE0: strong host definitions in port_globals.c. */
/* Cleared as a 2 KiB scratch block by `EraseAllEntities()`. */
u8 gUnk_02033290[2048] PORT_WEAK __attribute__((aligned(16)));
/* gUnk_02034330 / gUnk_02034480 / gUnk_02034492 / gUnk_020344A0 /
 * gUnk_020354C0 / gUnk_02035542 / gUnk_02036540:
 * strong host definitions in port_globals.c. */
/* gUnk_03000420: strong host definition in port_globals.c. */
/* `gUnk_03000C30` is a strong override in port_load_resource.c (the
 * deferred-resource queue buffer; placeholder size was too small). */
/* collision.c uses this as LinkedList2[16]; host size is >= 0x200 bytes. */
u8 gUnk_03003C70[0x200] __attribute__((aligned(16)));
/* gUnk_03003DE0: strong host definition in port_globals.c. */
/* gUnk_080012C8 / gUnk_08001A7C / gUnk_08001DCC:
 * strong host fallback tables in port_globals.c. */
/* gUnk_0800275C: strong host definition in port_asm_misc.c. */
/* gUnk_08003E44 / gUnk_08007DF4 / gUnk_0800823C / gUnk_080082DC /
 * gUnk_0800833C / gUnk_0800845C / gUnk_080084BC / gUnk_0800851C:
 * strong host definitions in port_collision_rom_data.c. */
/* gUnk_08016984 / gUnk_080B2CD8 / gUnk_080B4410 / gUnk_080B4458 /
 * gUnk_080B4468 / gUnk_080B4478 / gUnk_080B4488 / gUnk_080B4490 /
 * gUnk_080B44A0 / gUnk_080B44A8 / gUnk_080B44B8:
 * strong host fallback definitions in port_globals.c. */
/* gUnk_080C8F2C / gUnk_080C8F54 / gUnk_080C8F7C / gUnk_080C9044 /
 * gUnk_080C9058 / gUnk_080C9094 / gUnk_080CA2B4 / gUnk_080CA6D4 /
 * gUnk_080CC944: strong host fallback definitions in port_globals.c. */
/* gUnk_080CD7C4 / gUnk_080CD7E4 / gUnk_080CD7F8 / gUnk_080CD810 /
 * gUnk_080CD828 / gUnk_080CD840 / gUnk_080CD844 / gUnk_080CD848 /
 * gUnk_080CD850 / gUnk_080CD854 / gUnk_080CD86C / gUnk_080CD878 /
 * gUnk_080CD884: strong host fallback definitions in port_globals.c. */
/* gUnk_080D15B4 / gUnk_080FEAC8 / gUnk_080FEBE8 / gUnk_080FEC28 /
 * gUnk_080FECC8 / gUnk_080FED18 / gUnk_080FED58 / gUnk_080FEE18 /
 * gUnk_080FEE38 / gUnk_080FEE48 / gUnk_080FEE58 / gUnk_080FEE78:
 * strong host fallback definitions in port_globals.c. */
/* gUnk_08109230 / gUnk_08109244 / gUnk_08109248 / gUnk_0810926C /
 * gUnk_081092AC / gUnk_081092D4 / gUnk_0810942E / gUnk_081094CE:
 * strong host text-data fallbacks in port_text_data_stubs.c. */
/* gUnk_0811BE38 / gUnk_0811BE40 / gUnk_08122AE0 / gUnk_08122AE8 /
 * gUnk_08122AF8 / gUnk_08122B00 / gUnk_08122B0E / gUnk_08122B1E /
 * gUnk_08122B2E / gUnk_08122B3C / gUnk_081272F0 / gUnk_08127644 /
 * gUnk_08127998 / gUnk_08127CEC / gUnk_08127D00 / gUnk_08127D10 /
 * gUnk_086E8460: strong host definitions in port_globals.c. */
/* gUnk_0812901C: strong definition in port_globals.c (subtask dispatch). */
/* gUnk_085C4620: strong host definition in port_globals.c. */
/* gUsedPalettes / gVBlankDMA have strong typed host definitions in
 * port_globals.c. */
/* gZeldaFollowerText / gzHeap: strong host definitions in port_globals.c. */
/* script_* in this block now have strong host definitions in
 * port_script_data_stubs.c. */
/* Numeric sfxXXX symbols in this range now have strong host definitions in
 * port_sfx_data_stubs.c. */
/* Named sfx* symbols now have strong host definitions in
 * port_sfx_named_data_stubs.c. */
/* Remaining low hex-coded sfxXX aliases now have strong host definitions in
 * port_sfx_hexlow_data_stubs.c. */

/* ---- Title-screen entity-pipeline transitive deps -------------------- */
/*
 * The following symbols become required when `port_entity_runtime.c`
 * (PR: title-screen Zelda-logo OAM emission) takes a strong reference to
 * `ObjectUpdate`. That single reference pulls `object.c.o` into the
 * link, which in turn carries function-pointer tables that reference
 * every per-subkind object updater (`bench.c`, `cloud.c`, `fourElements.c`,
 * `greatFairy.c`, `mask.c`, `mazaalBossObject.c`, `pinwheel.c`,
 * `pullableLever.c`, `pushableFurniture.c`) plus a couple of manager
 * neighbours (`minishPortalManager.c`, `templeOfDropletsManager.c`).
 * None of those updaters run on the title screen (the only OBJECT
 * subkind alive then is `TITLE_SCREEN_OBJECT`), but their unresolved
 * external references still need *something* at link time. These
 * weak placeholders satisfy that requirement; if the title-screen
 * code path ever actually reaches one of them we get the same
 * loud-abort / zero-data behaviour the rest of this file uses. Real
 * definitions land when the corresponding scenes are wired in.
 */
/* script_PlayerGetElement / script_MazaalBossObjectMazaal: strong host
 * definitions in port_script_data_stubs.c. */
/* gUnk_080DD750 / gUnk_080E4C08 / gUnk_085A97A0 / gUnk_0812079C /
 * gUnk_081207AC: strong USA baserom mirrors in port_title_transitive_rom_data.c. */
/* gUnk_081207A4: strong host definition in greatFairy.c (__PORT__). */
/* gUnk_020342F8 / gUnk_02021F00: strong host definitions in port_globals.c. */
