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
 * A small set of symbols that are *always* called as functions (by
 * naming convention) get a proper `abort()` stub that prints the
 * offending name before exiting. This saves a round trip through a
 * debugger for the common case of "the port called into unported code".
 *
 * This file was produced by inspecting the ld error log from a
 * `TMC_LINK_GAME_SOURCES=ON` build. To regenerate after adding more
 * real definitions, see the roadmap notes in docs/sdl_port.md.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
#define PORT_WEAK __attribute__((weak))
#define PORT_NORETURN __attribute__((noreturn))
#else
#define PORT_WEAK
#define PORT_NORETURN
#endif

/* ---- Function-like stubs ---------------------------------------------- */

static PORT_NORETURN void Port_UnresolvedTrap(const char* name) {
    fprintf(stderr,
            "[tmc_sdl] FATAL: unresolved function called: %s()\n"
            "         See docs/sdl_port.md, roadmap PR #2b.4b.\n",
            name);
    fflush(stderr);
    abort();
}

/* Each function stub is tagged `weak` so that when the real definition
 * comes online (either from a newly-linked src/ TU or from a future
 * port_* implementation), it transparently replaces the stub. */
#define PORT_UNRESOLVED_FUNC(name)            \
    PORT_NORETURN void name(void);            \
    PORT_WEAK PORT_NORETURN void name(void) { \
        Port_UnresolvedTrap(#name);           \
    }
/* 41 function-like symbols. */
PORT_UNRESOLVED_FUNC(CloneTile)
PORT_UNRESOLVED_FUNC(GetActTileAtEntity)
PORT_UNRESOLVED_FUNC(GetActTileAtTilePos)
PORT_UNRESOLVED_FUNC(GetActTileAtWorldCoords)
PORT_UNRESOLVED_FUNC(GetActTileForTileType)
PORT_UNRESOLVED_FUNC(GetActTileRelativeToEntity)
PORT_UNRESOLVED_FUNC(GetCollisionDataAtEntity)
PORT_UNRESOLVED_FUNC(GetCollisionDataAtTilePos)
PORT_UNRESOLVED_FUNC(GetCollisionDataAtWorldCoords)
PORT_UNRESOLVED_FUNC(GetCollisionDataRelativeTo)
PORT_UNRESOLVED_FUNC(GetTileIndex)
PORT_UNRESOLVED_FUNC(GetTileTypeAtEntity)
PORT_UNRESOLVED_FUNC(GetTileTypeAtRoomCoords)
PORT_UNRESOLVED_FUNC(GetTileTypeAtTilePos)
PORT_UNRESOLVED_FUNC(GetTileTypeAtWorldCoords)
PORT_UNRESOLVED_FUNC(GetTileTypeRelativeToEntity)
PORT_UNRESOLVED_FUNC(SetActTileAtTilePos)
PORT_UNRESOLVED_FUNC(SetTile)
PORT_UNRESOLVED_FUNC(UpdateScrollVram)
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
PORT_UNRESOLVED_FUNC(ram_IntrMain)
PORT_UNRESOLVED_FUNC(sub_080B1B84)
PORT_UNRESOLVED_FUNC(sub_080B1BA4)

/* ---- Data placeholders ------------------------------------------------ */
/*
 * Weak zero-filled BSS for the remaining 809 symbols. Each placeholder
 * is 256 bytes with 16-byte alignment -- large enough for any single
 * struct the game references, plus slack so adjacent stubs do not
 * alias into one another if the game happens to index past the end.
 * The matching ROM build never sees this file (it is SDL-only) and the
 * real layouts of these globals live in the as-yet-unported ARM asm
 * source / data tables, so there is no struct-size requirement to
 * honour here.
 */
#define PORT_UNRESOLVED_DATA(name) char name[256] PORT_WEAK __attribute__((aligned(16)))

PORT_UNRESOLVED_DATA(ButtonUIElement_Actions);
PORT_UNRESOLVED_DATA(EzloNagUIElement_Actions);
PORT_UNRESOLVED_DATA(RupeeKeyDigits);
PORT_UNRESOLVED_DATA(bgmBeanstalk);
PORT_UNRESOLVED_DATA(bgmBeatVaati);
PORT_UNRESOLVED_DATA(bgmBossTheme);
PORT_UNRESOLVED_DATA(bgmCastleCollapse);
PORT_UNRESOLVED_DATA(bgmCastleMotif);
PORT_UNRESOLVED_DATA(bgmCastleTournament);
PORT_UNRESOLVED_DATA(bgmCastorWilds);
PORT_UNRESOLVED_DATA(bgmCaveOfFlames);
PORT_UNRESOLVED_DATA(bgmCloudTops);
PORT_UNRESOLVED_DATA(bgmCredits);
PORT_UNRESOLVED_DATA(bgmCrenelStorm);
PORT_UNRESOLVED_DATA(bgmCuccoMinigame);
PORT_UNRESOLVED_DATA(bgmDarkHyruleCastle);
PORT_UNRESOLVED_DATA(bgmDeepwoodShrine);
PORT_UNRESOLVED_DATA(bgmDiggingCave);
PORT_UNRESOLVED_DATA(bgmDungeon);
PORT_UNRESOLVED_DATA(bgmElementGet);
PORT_UNRESOLVED_DATA(bgmElementTheme);
PORT_UNRESOLVED_DATA(bgmElementalSanctuary);
PORT_UNRESOLVED_DATA(bgmEzloGet);
PORT_UNRESOLVED_DATA(bgmEzloStory);
PORT_UNRESOLVED_DATA(bgmEzloTheme);
PORT_UNRESOLVED_DATA(bgmFairyFountain);
PORT_UNRESOLVED_DATA(bgmFairyFountain2);
PORT_UNRESOLVED_DATA(bgmFestivalApproach);
PORT_UNRESOLVED_DATA(bgmFightTheme);
PORT_UNRESOLVED_DATA(bgmFightTheme2);
PORT_UNRESOLVED_DATA(bgmFileSelect);
PORT_UNRESOLVED_DATA(bgmFortressOfWinds);
PORT_UNRESOLVED_DATA(bgmGameover);
PORT_UNRESOLVED_DATA(bgmHouse);
PORT_UNRESOLVED_DATA(bgmHyruleCastle);
PORT_UNRESOLVED_DATA(bgmHyruleCastleNointro);
PORT_UNRESOLVED_DATA(bgmHyruleField);
PORT_UNRESOLVED_DATA(bgmHyruleTown);
PORT_UNRESOLVED_DATA(bgmIntroCutscene);
PORT_UNRESOLVED_DATA(bgmLearnScroll);
PORT_UNRESOLVED_DATA(bgmLostWoods);
PORT_UNRESOLVED_DATA(bgmLttpTitle);
PORT_UNRESOLVED_DATA(bgmMinishCap);
PORT_UNRESOLVED_DATA(bgmMinishVillage);
PORT_UNRESOLVED_DATA(bgmMinishWoods);
PORT_UNRESOLVED_DATA(bgmMtCrenel);
PORT_UNRESOLVED_DATA(bgmPalaceOfWinds);
PORT_UNRESOLVED_DATA(bgmPicoriFestival);
PORT_UNRESOLVED_DATA(bgmRoyalCrypt);
PORT_UNRESOLVED_DATA(bgmRoyalValley);
PORT_UNRESOLVED_DATA(bgmSavingZelda);
PORT_UNRESOLVED_DATA(bgmSecretCastleEntrance);
PORT_UNRESOLVED_DATA(bgmStory);
PORT_UNRESOLVED_DATA(bgmSwiftbladeDojo);
PORT_UNRESOLVED_DATA(bgmSyrupTheme);
PORT_UNRESOLVED_DATA(bgmTempleOfDroplets);
PORT_UNRESOLVED_DATA(bgmTitleScreen);
PORT_UNRESOLVED_DATA(bgmUnused);
PORT_UNRESOLVED_DATA(bgmVaatiMotif);
PORT_UNRESOLVED_DATA(bgmVaatiReborn);
PORT_UNRESOLVED_DATA(bgmVaatiTheme);
PORT_UNRESOLVED_DATA(bgmVaatiTransfigured);
PORT_UNRESOLVED_DATA(bgmVaatiWrath);
PORT_UNRESOLVED_DATA(bgmWindRuins);
PORT_UNRESOLVED_DATA(gActiveItems);
PORT_UNRESOLVED_DATA(gActiveScriptInfo);
PORT_UNRESOLVED_DATA(gArea);
PORT_UNRESOLVED_DATA(gAreaRoomHeaders);
PORT_UNRESOLVED_DATA(gAreaRoomMaps);
PORT_UNRESOLVED_DATA(gAreaTable);
PORT_UNRESOLVED_DATA(gAreaTileSets);
PORT_UNRESOLVED_DATA(gAreaTiles);
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
PORT_UNRESOLVED_DATA(gBgAnimations);
PORT_UNRESOLVED_DATA(gCarriedEntity);
/* gChooseFileState aliases gMenu via an `alias` attribute in
 * port_globals.c (preserves the GBA's union over offset 0x80). */
PORT_UNRESOLVED_DATA(gCollidableCount);
PORT_UNRESOLVED_DATA(gCollisionMtx);
PORT_UNRESOLVED_DATA(gCurrentRoomMemory);
PORT_UNRESOLVED_DATA(gCurrentRoomProperties);
PORT_UNRESOLVED_DATA(gDiggingCaveEntranceTransition);
PORT_UNRESOLVED_DATA(gDungeonMap);
PORT_UNRESOLVED_DATA(gDungeonNames);
PORT_UNRESOLVED_DATA(gEEPROMConfig);
PORT_UNRESOLVED_DATA(gEnemyTarget);
PORT_UNRESOLVED_DATA(gEntCount);
/* gPlayerEntity / gAuxPlayerEntities / gEntities have strong host
 * definitions in port_globals.c (placed contiguously in a single
 * named BSS section to satisfy `MemClear(&gPlayerEntity, 10880)`).
 * gEntityLists / gEntityListsBackup likewise. */
PORT_UNRESOLVED_DATA(gExtraFrameOffsets);
PORT_UNRESOLVED_DATA(gFadeControl);
PORT_UNRESOLVED_DATA(gFigurines);
PORT_UNRESOLVED_DATA(gFixedTypeGfxData);
PORT_UNRESOLVED_DATA(gFrameObjLists);
PORT_UNRESOLVED_DATA(gFuseInfo);
PORT_UNRESOLVED_DATA(gGFXSlots);
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
PORT_UNRESOLVED_DATA(gInteractableObjects);
/* gIntroState aliases gMenu via an `alias` attribute in port_globals.c. */
PORT_UNRESOLVED_DATA(gLilypadRails);
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
PORT_UNRESOLVED_DATA(gManagerCount);
PORT_UNRESOLVED_DATA(gMapBottom);
PORT_UNRESOLVED_DATA(gMapData);
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
PORT_UNRESOLVED_DATA(gMapTop);
/* gMenu has its strong host definition in port_globals.c (with
 * gIntroState / gChooseFileState aliased to it). */
PORT_UNRESOLVED_DATA(gMessageChoices);
PORT_UNRESOLVED_DATA(gMoreSpritePtrs);
PORT_UNRESOLVED_DATA(gOamCmd);
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
char gOAMControls[0x920] PORT_WEAK __attribute__((aligned(16)));
PORT_UNRESOLVED_DATA(gPaletteBufferBackup);
/* gPaletteGroups has moved to src/platform/shared/port_rom_data_stubs.c
 * for the same reason as gGfxGroups above (LoadPaletteGroup needs a
 * non-NULL terminator entry to short-circuit). */
PORT_UNRESOLVED_DATA(gPaletteList);
PORT_UNRESOLVED_DATA(gPauseMenuOptions);
PORT_UNRESOLVED_DATA(gPeahatChargeDirectionOffsets);
PORT_UNRESOLVED_DATA(gPlayerClones);
/* gPlayerEntity lives in port_globals.c (entity arena). */
PORT_UNRESOLVED_DATA(gPlayerScriptExecutionContext);
PORT_UNRESOLVED_DATA(gPlayerState);
PORT_UNRESOLVED_DATA(gPossibleInteraction);
PORT_UNRESOLVED_DATA(gPriorityHandler);
PORT_UNRESOLVED_DATA(gRoomMemory);
PORT_UNRESOLVED_DATA(gRoomTransition);
PORT_UNRESOLVED_DATA(gRoomVars);
PORT_UNRESOLVED_DATA(gSave);
PORT_UNRESOLVED_DATA(gScriptExecutionContextArray);
PORT_UNRESOLVED_DATA(gShakeOffsets);
PORT_UNRESOLVED_DATA(gSmallChests);
PORT_UNRESOLVED_DATA(gSoundPlayingInfo);
PORT_UNRESOLVED_DATA(gSpriteAnimations_322);
PORT_UNRESOLVED_DATA(gSpritePtrs);
PORT_UNRESOLVED_DATA(gSubtasks);
PORT_UNRESOLVED_DATA(gTextGfxBuffer);
PORT_UNRESOLVED_DATA(gTilesForSpecialTiles);
PORT_UNRESOLVED_DATA(gTranslations);
/* gUI has moved to src/platform/shared/port_globals.c (real `UI`
 * type) for the same reason as gHUD above. */
/* gUIElementDefinitions has moved to
 * src/platform/shared/port_rom_data_stubs.c, which provides a strong
 * host stand-in whose `updateFunction` slot is a no-op. The previous
 * 256-byte zero-init weak placeholder NULL-deref'd as soon as the
 * file-select task created its first UI element. */
PORT_UNRESOLVED_DATA(gUnk_02000040);
PORT_UNRESOLVED_DATA(gUnk_020000C0);
PORT_UNRESOLVED_DATA(gUnk_02001A3C);
PORT_UNRESOLVED_DATA(gUnk_02006F00);
PORT_UNRESOLVED_DATA(gUnk_0200B640);
PORT_UNRESOLVED_DATA(gUnk_02017830);
PORT_UNRESOLVED_DATA(gUnk_02017AA0);
PORT_UNRESOLVED_DATA(gUnk_02018EA0);
PORT_UNRESOLVED_DATA(gUnk_02018EB0);
PORT_UNRESOLVED_DATA(gUnk_02018EE0);
PORT_UNRESOLVED_DATA(gUnk_020227DC);
PORT_UNRESOLVED_DATA(gUnk_020227E8);
PORT_UNRESOLVED_DATA(gUnk_020227F0);
PORT_UNRESOLVED_DATA(gUnk_020227F8);
PORT_UNRESOLVED_DATA(gUnk_02022800);
PORT_UNRESOLVED_DATA(gUnk_02022830);
PORT_UNRESOLVED_DATA(gUnk_020246B0);
PORT_UNRESOLVED_DATA(gUnk_02033290);
PORT_UNRESOLVED_DATA(gUnk_02034330);
PORT_UNRESOLVED_DATA(gUnk_02034480);
PORT_UNRESOLVED_DATA(gUnk_02034492);
PORT_UNRESOLVED_DATA(gUnk_020344A0);
PORT_UNRESOLVED_DATA(gUnk_020354C0);
PORT_UNRESOLVED_DATA(gUnk_02035542);
PORT_UNRESOLVED_DATA(gUnk_02036540);
PORT_UNRESOLVED_DATA(gUnk_02036A58);
PORT_UNRESOLVED_DATA(gUnk_02036AD8);
PORT_UNRESOLVED_DATA(gUnk_02036BB8);
PORT_UNRESOLVED_DATA(gUnk_03000420);
/* `gUnk_03000C30` is a strong override in port_load_resource.c (the
 * deferred-resource queue buffer; placeholder size was too small). */
PORT_UNRESOLVED_DATA(gUnk_03001020);
PORT_UNRESOLVED_DATA(gUnk_03003C70);
PORT_UNRESOLVED_DATA(gUnk_03003DE0);
PORT_UNRESOLVED_DATA(gUnk_080012C8);
PORT_UNRESOLVED_DATA(gUnk_08001A7C);
PORT_UNRESOLVED_DATA(gUnk_08001DCC);
PORT_UNRESOLVED_DATA(gUnk_0800275C);
PORT_UNRESOLVED_DATA(gUnk_08003E44);
PORT_UNRESOLVED_DATA(gUnk_08007DF4);
PORT_UNRESOLVED_DATA(gUnk_0800823C);
PORT_UNRESOLVED_DATA(gUnk_080082DC);
PORT_UNRESOLVED_DATA(gUnk_0800833C);
PORT_UNRESOLVED_DATA(gUnk_0800845C);
PORT_UNRESOLVED_DATA(gUnk_080084BC);
PORT_UNRESOLVED_DATA(gUnk_0800851C);
PORT_UNRESOLVED_DATA(gUnk_08016984);
PORT_UNRESOLVED_DATA(gUnk_080B2CD8);
PORT_UNRESOLVED_DATA(gUnk_080B4410);
PORT_UNRESOLVED_DATA(gUnk_080B4458);
PORT_UNRESOLVED_DATA(gUnk_080B4468);
PORT_UNRESOLVED_DATA(gUnk_080B4478);
PORT_UNRESOLVED_DATA(gUnk_080B4488);
PORT_UNRESOLVED_DATA(gUnk_080B4490);
PORT_UNRESOLVED_DATA(gUnk_080B44A0);
PORT_UNRESOLVED_DATA(gUnk_080B44A8);
PORT_UNRESOLVED_DATA(gUnk_080B44B8);
PORT_UNRESOLVED_DATA(gUnk_080B44C0);
PORT_UNRESOLVED_DATA(gUnk_080B44C2);
PORT_UNRESOLVED_DATA(gUnk_080C8F2C);
PORT_UNRESOLVED_DATA(gUnk_080C8F54);
PORT_UNRESOLVED_DATA(gUnk_080C8F7C);
PORT_UNRESOLVED_DATA(gUnk_080C9044);
PORT_UNRESOLVED_DATA(gUnk_080C9058);
PORT_UNRESOLVED_DATA(gUnk_080C9094);
PORT_UNRESOLVED_DATA(gUnk_080CA2B4);
PORT_UNRESOLVED_DATA(gUnk_080CA6D4);
PORT_UNRESOLVED_DATA(gUnk_080CC944);
PORT_UNRESOLVED_DATA(gUnk_080CD7C4);
PORT_UNRESOLVED_DATA(gUnk_080CD7E4);
PORT_UNRESOLVED_DATA(gUnk_080CD7F8);
PORT_UNRESOLVED_DATA(gUnk_080CD810);
PORT_UNRESOLVED_DATA(gUnk_080CD828);
PORT_UNRESOLVED_DATA(gUnk_080CD840);
PORT_UNRESOLVED_DATA(gUnk_080CD844);
PORT_UNRESOLVED_DATA(gUnk_080CD848);
PORT_UNRESOLVED_DATA(gUnk_080CD850);
PORT_UNRESOLVED_DATA(gUnk_080CD854);
PORT_UNRESOLVED_DATA(gUnk_080CD86C);
PORT_UNRESOLVED_DATA(gUnk_080CD878);
PORT_UNRESOLVED_DATA(gUnk_080CD884);
PORT_UNRESOLVED_DATA(gUnk_080D15B4);
PORT_UNRESOLVED_DATA(gUnk_080FEAC8);
PORT_UNRESOLVED_DATA(gUnk_080FEBE8);
PORT_UNRESOLVED_DATA(gUnk_080FEC28);
PORT_UNRESOLVED_DATA(gUnk_080FECC8);
PORT_UNRESOLVED_DATA(gUnk_080FED18);
PORT_UNRESOLVED_DATA(gUnk_080FED58);
PORT_UNRESOLVED_DATA(gUnk_080FEE18);
PORT_UNRESOLVED_DATA(gUnk_080FEE38);
PORT_UNRESOLVED_DATA(gUnk_080FEE48);
PORT_UNRESOLVED_DATA(gUnk_080FEE58);
PORT_UNRESOLVED_DATA(gUnk_080FEE78);
PORT_UNRESOLVED_DATA(gUnk_08109230);
PORT_UNRESOLVED_DATA(gUnk_08109244);
PORT_UNRESOLVED_DATA(gUnk_08109248);
PORT_UNRESOLVED_DATA(gUnk_0810926C);
PORT_UNRESOLVED_DATA(gUnk_081092AC);
PORT_UNRESOLVED_DATA(gUnk_081092D4);
PORT_UNRESOLVED_DATA(gUnk_0810942E);
PORT_UNRESOLVED_DATA(gUnk_081094CE);
PORT_UNRESOLVED_DATA(gUnk_0811BE38);
PORT_UNRESOLVED_DATA(gUnk_0811BE40);
PORT_UNRESOLVED_DATA(gUnk_08122AE0);
PORT_UNRESOLVED_DATA(gUnk_08122AE8);
PORT_UNRESOLVED_DATA(gUnk_08122AF8);
PORT_UNRESOLVED_DATA(gUnk_08122B00);
PORT_UNRESOLVED_DATA(gUnk_08122B0E);
PORT_UNRESOLVED_DATA(gUnk_08122B1E);
PORT_UNRESOLVED_DATA(gUnk_08122B2E);
PORT_UNRESOLVED_DATA(gUnk_08122B3C);
PORT_UNRESOLVED_DATA(gUnk_081272F0);
PORT_UNRESOLVED_DATA(gUnk_08127644);
PORT_UNRESOLVED_DATA(gUnk_08127998);
PORT_UNRESOLVED_DATA(gUnk_08127CEC);
PORT_UNRESOLVED_DATA(gUnk_08127D00);
PORT_UNRESOLVED_DATA(gUnk_08127D10);
PORT_UNRESOLVED_DATA(gUnk_08128C00);
PORT_UNRESOLVED_DATA(gUnk_08128C04);
PORT_UNRESOLVED_DATA(gUnk_08128C14);
PORT_UNRESOLVED_DATA(gUnk_08128C94);
PORT_UNRESOLVED_DATA(gUnk_08128D14);
PORT_UNRESOLVED_DATA(gUnk_08128D24);
PORT_UNRESOLVED_DATA(gUnk_08128D30);
PORT_UNRESOLVED_DATA(gUnk_08128D38);
PORT_UNRESOLVED_DATA(gUnk_08128D3C);
PORT_UNRESOLVED_DATA(gUnk_08128D43);
PORT_UNRESOLVED_DATA(gUnk_08128D51);
PORT_UNRESOLVED_DATA(gUnk_08128D58);
PORT_UNRESOLVED_DATA(gUnk_08128D60);
PORT_UNRESOLVED_DATA(gUnk_08128D70);
PORT_UNRESOLVED_DATA(gUnk_08128DB0);
PORT_UNRESOLVED_DATA(gUnk_08128DB8);
PORT_UNRESOLVED_DATA(gUnk_08128DBC);
PORT_UNRESOLVED_DATA(gUnk_08128DCC);
PORT_UNRESOLVED_DATA(gUnk_08128DD4);
PORT_UNRESOLVED_DATA(gUnk_08128DD8);
PORT_UNRESOLVED_DATA(gUnk_08128DE8);
PORT_UNRESOLVED_DATA(gUnk_08128E78);
PORT_UNRESOLVED_DATA(gUnk_08128E80);
PORT_UNRESOLVED_DATA(gUnk_08128E84);
PORT_UNRESOLVED_DATA(gUnk_08128E94);
PORT_UNRESOLVED_DATA(gUnk_08128F38);
PORT_UNRESOLVED_DATA(gUnk_08128F58);
PORT_UNRESOLVED_DATA(gUnk_08128FA8);
PORT_UNRESOLVED_DATA(gUnk_08128FC0);
PORT_UNRESOLVED_DATA(gUnk_08128FD8);
PORT_UNRESOLVED_DATA(gUnk_08129004);
PORT_UNRESOLVED_DATA(gUnk_0812901C);
PORT_UNRESOLVED_DATA(gUnk_085C4620);
PORT_UNRESOLVED_DATA(gUnk_086E8460);
PORT_UNRESOLVED_DATA(gUpdateContext);
PORT_UNRESOLVED_DATA(gUpdateVisibleTiles);
PORT_UNRESOLVED_DATA(gUsedPalettes);
PORT_UNRESOLVED_DATA(gVBlankDMA);
PORT_UNRESOLVED_DATA(gZeldaFollowerText);
PORT_UNRESOLVED_DATA(gzHeap);
PORT_UNRESOLVED_DATA(script_08012C48);
PORT_UNRESOLVED_DATA(script_08015B14);
PORT_UNRESOLVED_DATA(script_BedAtSimons);
PORT_UNRESOLVED_DATA(script_BedInLinksRoom);
PORT_UNRESOLVED_DATA(script_BigGoronKinstone1);
PORT_UNRESOLVED_DATA(script_BigGoronKinstone2);
PORT_UNRESOLVED_DATA(script_BigGoronKinstone3);
PORT_UNRESOLVED_DATA(script_BusinessScrubIntro);
PORT_UNRESOLVED_DATA(script_CarlovKinstone);
PORT_UNRESOLVED_DATA(script_CutsceneMiscObjectMinishCap);
PORT_UNRESOLVED_DATA(script_CutsceneMiscObjectSwordInChest);
PORT_UNRESOLVED_DATA(script_CutsceneMiscObjectTheLittleHat);
PORT_UNRESOLVED_DATA(script_CutsceneOrchestratorIntro);
PORT_UNRESOLVED_DATA(script_CutsceneOrchestratorIntro2);
PORT_UNRESOLVED_DATA(script_CutsceneOrchestratorMinishVaati);
PORT_UNRESOLVED_DATA(script_CutsceneOrchestratorTakeoverCutscene);
PORT_UNRESOLVED_DATA(script_EzloTalkOcarina);
PORT_UNRESOLVED_DATA(script_GhostBrotherKinstone);
PORT_UNRESOLVED_DATA(script_GormanFirstAppearance);
PORT_UNRESOLVED_DATA(script_Goron1Kinstone2);
PORT_UNRESOLVED_DATA(script_Goron1Kinstone3);
PORT_UNRESOLVED_DATA(script_Goron1Kinstone4);
PORT_UNRESOLVED_DATA(script_Goron1Kinstone5);
PORT_UNRESOLVED_DATA(script_Goron1Kinstone6);
PORT_UNRESOLVED_DATA(script_Goron2Kinstone2);
PORT_UNRESOLVED_DATA(script_Goron2Kinstone3);
PORT_UNRESOLVED_DATA(script_Goron2Kinstone4);
PORT_UNRESOLVED_DATA(script_Goron2Kinstone5);
PORT_UNRESOLVED_DATA(script_Goron2Kinstone6);
PORT_UNRESOLVED_DATA(script_Goron3Kinstone3);
PORT_UNRESOLVED_DATA(script_Goron4Kinstone4);
PORT_UNRESOLVED_DATA(script_Goron5Kinstone5);
PORT_UNRESOLVED_DATA(script_Goron6Kindstone6);
PORT_UNRESOLVED_DATA(script_GoronKinstone);
PORT_UNRESOLVED_DATA(script_GoronMerchantArriving);
PORT_UNRESOLVED_DATA(script_GuardTakeover);
PORT_UNRESOLVED_DATA(script_HouseDoorIntro);
PORT_UNRESOLVED_DATA(script_IntroCameraTarget);
PORT_UNRESOLVED_DATA(script_KingDaltusTakeover);
PORT_UNRESOLVED_DATA(script_KinstoneSparkKinstoneSpark);
PORT_UNRESOLVED_DATA(script_KinstoneSparkKinstoneSparkFromBottom);
PORT_UNRESOLVED_DATA(script_KinstoneSparkKinstoneSparkGoron);
PORT_UNRESOLVED_DATA(script_KinstoneSparkKinstoneSparkGoronMerchang);
PORT_UNRESOLVED_DATA(script_MazaalMacroDefeated);
PORT_UNRESOLVED_DATA(script_MinishEzlo);
PORT_UNRESOLVED_DATA(script_MinisterPothoTakeover);
PORT_UNRESOLVED_DATA(script_MutohKinstone);
PORT_UNRESOLVED_DATA(script_SmithIntro);
PORT_UNRESOLVED_DATA(script_StampKinstone);
PORT_UNRESOLVED_DATA(script_SyrupKinstone);
PORT_UNRESOLVED_DATA(script_Vaati);
PORT_UNRESOLVED_DATA(script_VaatiTakeover);
PORT_UNRESOLVED_DATA(script_ZeldaIntro);
PORT_UNRESOLVED_DATA(script_ZeldaLeaveLinksHouse);
PORT_UNRESOLVED_DATA(script_ZeldaMagic);
PORT_UNRESOLVED_DATA(script_ZeldaMoveToLinksHouse);
PORT_UNRESOLVED_DATA(script_ZeldaStoneDHC);
PORT_UNRESOLVED_DATA(script_ZeldaStoneInDHC);
PORT_UNRESOLVED_DATA(script_ZeldaStoneTakeover);
PORT_UNRESOLVED_DATA(sfx100);
PORT_UNRESOLVED_DATA(sfx101);
PORT_UNRESOLVED_DATA(sfx102);
PORT_UNRESOLVED_DATA(sfx103);
PORT_UNRESOLVED_DATA(sfx104);
PORT_UNRESOLVED_DATA(sfx105);
PORT_UNRESOLVED_DATA(sfx106);
PORT_UNRESOLVED_DATA(sfx107);
PORT_UNRESOLVED_DATA(sfx108);
PORT_UNRESOLVED_DATA(sfx109);
PORT_UNRESOLVED_DATA(sfx10A);
PORT_UNRESOLVED_DATA(sfx10B);
PORT_UNRESOLVED_DATA(sfx10D);
PORT_UNRESOLVED_DATA(sfx10E);
PORT_UNRESOLVED_DATA(sfx10F);
PORT_UNRESOLVED_DATA(sfx110);
PORT_UNRESOLVED_DATA(sfx111);
PORT_UNRESOLVED_DATA(sfx112);
PORT_UNRESOLVED_DATA(sfx113);
PORT_UNRESOLVED_DATA(sfx114);
PORT_UNRESOLVED_DATA(sfx115);
PORT_UNRESOLVED_DATA(sfx116);
PORT_UNRESOLVED_DATA(sfx117);
PORT_UNRESOLVED_DATA(sfx11C);
PORT_UNRESOLVED_DATA(sfx11D);
PORT_UNRESOLVED_DATA(sfx122);
PORT_UNRESOLVED_DATA(sfx123);
PORT_UNRESOLVED_DATA(sfx124);
PORT_UNRESOLVED_DATA(sfx125);
PORT_UNRESOLVED_DATA(sfx126);
PORT_UNRESOLVED_DATA(sfx12A);
PORT_UNRESOLVED_DATA(sfx12B);
PORT_UNRESOLVED_DATA(sfx12C);
PORT_UNRESOLVED_DATA(sfx12D);
PORT_UNRESOLVED_DATA(sfx12E);
PORT_UNRESOLVED_DATA(sfx12F);
PORT_UNRESOLVED_DATA(sfx130);
PORT_UNRESOLVED_DATA(sfx131);
PORT_UNRESOLVED_DATA(sfx132);
PORT_UNRESOLVED_DATA(sfx133);
PORT_UNRESOLVED_DATA(sfx134);
PORT_UNRESOLVED_DATA(sfx135);
PORT_UNRESOLVED_DATA(sfx136);
PORT_UNRESOLVED_DATA(sfx137);
PORT_UNRESOLVED_DATA(sfx138);
PORT_UNRESOLVED_DATA(sfx139);
PORT_UNRESOLVED_DATA(sfx13A);
PORT_UNRESOLVED_DATA(sfx13B);
PORT_UNRESOLVED_DATA(sfx13C);
PORT_UNRESOLVED_DATA(sfx140);
PORT_UNRESOLVED_DATA(sfx143);
PORT_UNRESOLVED_DATA(sfx144);
PORT_UNRESOLVED_DATA(sfx145);
PORT_UNRESOLVED_DATA(sfx146);
PORT_UNRESOLVED_DATA(sfx147);
PORT_UNRESOLVED_DATA(sfx148);
PORT_UNRESOLVED_DATA(sfx149);
PORT_UNRESOLVED_DATA(sfx14A);
PORT_UNRESOLVED_DATA(sfx14B);
PORT_UNRESOLVED_DATA(sfx14C);
PORT_UNRESOLVED_DATA(sfx14D);
PORT_UNRESOLVED_DATA(sfx14E);
PORT_UNRESOLVED_DATA(sfx14F);
PORT_UNRESOLVED_DATA(sfx150);
PORT_UNRESOLVED_DATA(sfx151);
PORT_UNRESOLVED_DATA(sfx153);
PORT_UNRESOLVED_DATA(sfx154);
PORT_UNRESOLVED_DATA(sfx155);
PORT_UNRESOLVED_DATA(sfx156);
PORT_UNRESOLVED_DATA(sfx157);
PORT_UNRESOLVED_DATA(sfx158);
PORT_UNRESOLVED_DATA(sfx159);
PORT_UNRESOLVED_DATA(sfx15A);
PORT_UNRESOLVED_DATA(sfx15B);
PORT_UNRESOLVED_DATA(sfx15C);
PORT_UNRESOLVED_DATA(sfx15D);
PORT_UNRESOLVED_DATA(sfx15E);
PORT_UNRESOLVED_DATA(sfx15F);
PORT_UNRESOLVED_DATA(sfx160);
PORT_UNRESOLVED_DATA(sfx161);
PORT_UNRESOLVED_DATA(sfx162);
PORT_UNRESOLVED_DATA(sfx164);
PORT_UNRESOLVED_DATA(sfx165);
PORT_UNRESOLVED_DATA(sfx166);
PORT_UNRESOLVED_DATA(sfx167);
PORT_UNRESOLVED_DATA(sfx168);
PORT_UNRESOLVED_DATA(sfx169);
PORT_UNRESOLVED_DATA(sfx16A);
PORT_UNRESOLVED_DATA(sfx16C);
PORT_UNRESOLVED_DATA(sfx16D);
PORT_UNRESOLVED_DATA(sfx16E);
PORT_UNRESOLVED_DATA(sfx171);
PORT_UNRESOLVED_DATA(sfx172);
PORT_UNRESOLVED_DATA(sfx174);
PORT_UNRESOLVED_DATA(sfx175);
PORT_UNRESOLVED_DATA(sfx176);
PORT_UNRESOLVED_DATA(sfx177);
PORT_UNRESOLVED_DATA(sfx178);
PORT_UNRESOLVED_DATA(sfx179);
PORT_UNRESOLVED_DATA(sfx17A);
PORT_UNRESOLVED_DATA(sfx180);
PORT_UNRESOLVED_DATA(sfx181);
PORT_UNRESOLVED_DATA(sfx182);
PORT_UNRESOLVED_DATA(sfx183);
PORT_UNRESOLVED_DATA(sfx184);
PORT_UNRESOLVED_DATA(sfx185);
PORT_UNRESOLVED_DATA(sfx186);
PORT_UNRESOLVED_DATA(sfx189);
PORT_UNRESOLVED_DATA(sfx18A);
PORT_UNRESOLVED_DATA(sfx18B);
PORT_UNRESOLVED_DATA(sfx18C);
PORT_UNRESOLVED_DATA(sfx18D);
PORT_UNRESOLVED_DATA(sfx18E);
PORT_UNRESOLVED_DATA(sfx18F);
PORT_UNRESOLVED_DATA(sfx190);
PORT_UNRESOLVED_DATA(sfx191);
PORT_UNRESOLVED_DATA(sfx192);
PORT_UNRESOLVED_DATA(sfx193);
PORT_UNRESOLVED_DATA(sfx194);
PORT_UNRESOLVED_DATA(sfx195);
PORT_UNRESOLVED_DATA(sfx196);
PORT_UNRESOLVED_DATA(sfx197);
PORT_UNRESOLVED_DATA(sfx198);
PORT_UNRESOLVED_DATA(sfx199);
PORT_UNRESOLVED_DATA(sfx19A);
PORT_UNRESOLVED_DATA(sfx19B);
PORT_UNRESOLVED_DATA(sfx19C);
PORT_UNRESOLVED_DATA(sfx19D);
PORT_UNRESOLVED_DATA(sfx19E);
PORT_UNRESOLVED_DATA(sfx19F);
PORT_UNRESOLVED_DATA(sfx1A0);
PORT_UNRESOLVED_DATA(sfx1A1);
PORT_UNRESOLVED_DATA(sfx1A2);
PORT_UNRESOLVED_DATA(sfx1A3);
PORT_UNRESOLVED_DATA(sfx1A4);
PORT_UNRESOLVED_DATA(sfx1A5);
PORT_UNRESOLVED_DATA(sfx1A6);
PORT_UNRESOLVED_DATA(sfx1A7);
PORT_UNRESOLVED_DATA(sfx1A8);
PORT_UNRESOLVED_DATA(sfx1A9);
PORT_UNRESOLVED_DATA(sfx1AA);
PORT_UNRESOLVED_DATA(sfx1AB);
PORT_UNRESOLVED_DATA(sfx1AC);
PORT_UNRESOLVED_DATA(sfx1AD);
PORT_UNRESOLVED_DATA(sfx1AE);
PORT_UNRESOLVED_DATA(sfx1AF);
PORT_UNRESOLVED_DATA(sfx1B0);
PORT_UNRESOLVED_DATA(sfx1B4);
PORT_UNRESOLVED_DATA(sfx1B5);
PORT_UNRESOLVED_DATA(sfx1B6);
PORT_UNRESOLVED_DATA(sfx1BC);
PORT_UNRESOLVED_DATA(sfx1BD);
PORT_UNRESOLVED_DATA(sfx1BE);
PORT_UNRESOLVED_DATA(sfx1BF);
PORT_UNRESOLVED_DATA(sfx1C0);
PORT_UNRESOLVED_DATA(sfx1C1);
PORT_UNRESOLVED_DATA(sfx1C2);
PORT_UNRESOLVED_DATA(sfx1C3);
PORT_UNRESOLVED_DATA(sfx1C4);
PORT_UNRESOLVED_DATA(sfx1C5);
PORT_UNRESOLVED_DATA(sfx1C6);
PORT_UNRESOLVED_DATA(sfx1C7);
PORT_UNRESOLVED_DATA(sfx1C8);
PORT_UNRESOLVED_DATA(sfx1C9);
PORT_UNRESOLVED_DATA(sfx1CA);
PORT_UNRESOLVED_DATA(sfx1CB);
PORT_UNRESOLVED_DATA(sfx1CC);
PORT_UNRESOLVED_DATA(sfx1D0);
PORT_UNRESOLVED_DATA(sfx1D2);
PORT_UNRESOLVED_DATA(sfx1D3);
PORT_UNRESOLVED_DATA(sfx1D4);
PORT_UNRESOLVED_DATA(sfx1D5);
PORT_UNRESOLVED_DATA(sfx1DB);
PORT_UNRESOLVED_DATA(sfx1DC);
PORT_UNRESOLVED_DATA(sfx1DD);
PORT_UNRESOLVED_DATA(sfx1DE);
PORT_UNRESOLVED_DATA(sfx1DF);
PORT_UNRESOLVED_DATA(sfx1E0);
PORT_UNRESOLVED_DATA(sfx1E1);
PORT_UNRESOLVED_DATA(sfx1E2);
PORT_UNRESOLVED_DATA(sfx1E3);
PORT_UNRESOLVED_DATA(sfx1E4);
PORT_UNRESOLVED_DATA(sfx1E5);
PORT_UNRESOLVED_DATA(sfx1E6);
PORT_UNRESOLVED_DATA(sfx1E7);
PORT_UNRESOLVED_DATA(sfx1E8);
PORT_UNRESOLVED_DATA(sfx1E9);
PORT_UNRESOLVED_DATA(sfx1EA);
PORT_UNRESOLVED_DATA(sfx1EB);
PORT_UNRESOLVED_DATA(sfx1EC);
PORT_UNRESOLVED_DATA(sfx1ED);
PORT_UNRESOLVED_DATA(sfx1EE);
PORT_UNRESOLVED_DATA(sfx1EF);
PORT_UNRESOLVED_DATA(sfx1F0);
PORT_UNRESOLVED_DATA(sfx1F1);
PORT_UNRESOLVED_DATA(sfx1F2);
PORT_UNRESOLVED_DATA(sfx1F3);
PORT_UNRESOLVED_DATA(sfx1F4);
PORT_UNRESOLVED_DATA(sfx1F5);
PORT_UNRESOLVED_DATA(sfx1F6);
PORT_UNRESOLVED_DATA(sfx1F7);
PORT_UNRESOLVED_DATA(sfx1F8);
PORT_UNRESOLVED_DATA(sfx1F9);
PORT_UNRESOLVED_DATA(sfx1FA);
PORT_UNRESOLVED_DATA(sfx1FB);
PORT_UNRESOLVED_DATA(sfx1FC);
PORT_UNRESOLVED_DATA(sfx1FD);
PORT_UNRESOLVED_DATA(sfx1FE);
PORT_UNRESOLVED_DATA(sfx1FF);
PORT_UNRESOLVED_DATA(sfx200);
PORT_UNRESOLVED_DATA(sfx201);
PORT_UNRESOLVED_DATA(sfx202);
PORT_UNRESOLVED_DATA(sfx203);
PORT_UNRESOLVED_DATA(sfx204);
PORT_UNRESOLVED_DATA(sfx205);
PORT_UNRESOLVED_DATA(sfx206);
PORT_UNRESOLVED_DATA(sfx207);
PORT_UNRESOLVED_DATA(sfx208);
PORT_UNRESOLVED_DATA(sfx209);
PORT_UNRESOLVED_DATA(sfx20A);
PORT_UNRESOLVED_DATA(sfx20B);
PORT_UNRESOLVED_DATA(sfx20C);
PORT_UNRESOLVED_DATA(sfx20D);
PORT_UNRESOLVED_DATA(sfx20E);
PORT_UNRESOLVED_DATA(sfx20F);
PORT_UNRESOLVED_DATA(sfx210);
PORT_UNRESOLVED_DATA(sfx211);
PORT_UNRESOLVED_DATA(sfx212);
PORT_UNRESOLVED_DATA(sfx213);
PORT_UNRESOLVED_DATA(sfx214);
PORT_UNRESOLVED_DATA(sfx215);
PORT_UNRESOLVED_DATA(sfx216);
PORT_UNRESOLVED_DATA(sfx217);
PORT_UNRESOLVED_DATA(sfx218);
PORT_UNRESOLVED_DATA(sfx219);
PORT_UNRESOLVED_DATA(sfx21A);
PORT_UNRESOLVED_DATA(sfx21B);
PORT_UNRESOLVED_DATA(sfx21C);
PORT_UNRESOLVED_DATA(sfx21D);
PORT_UNRESOLVED_DATA(sfx21E);
PORT_UNRESOLVED_DATA(sfx21F);
PORT_UNRESOLVED_DATA(sfx220);
PORT_UNRESOLVED_DATA(sfx221);
PORT_UNRESOLVED_DATA(sfx6B);
PORT_UNRESOLVED_DATA(sfx7E);
PORT_UNRESOLVED_DATA(sfx80);
PORT_UNRESOLVED_DATA(sfx81);
PORT_UNRESOLVED_DATA(sfx82);
PORT_UNRESOLVED_DATA(sfx86);
PORT_UNRESOLVED_DATA(sfx88);
PORT_UNRESOLVED_DATA(sfx9B);
PORT_UNRESOLVED_DATA(sfx9C);
PORT_UNRESOLVED_DATA(sfx9D);
PORT_UNRESOLVED_DATA(sfx9E);
PORT_UNRESOLVED_DATA(sfx9F);
PORT_UNRESOLVED_DATA(sfxA0);
PORT_UNRESOLVED_DATA(sfxA8);
PORT_UNRESOLVED_DATA(sfxA9);
PORT_UNRESOLVED_DATA(sfxAA);
PORT_UNRESOLVED_DATA(sfxAC);
PORT_UNRESOLVED_DATA(sfxAE);
PORT_UNRESOLVED_DATA(sfxAF);
PORT_UNRESOLVED_DATA(sfxApparate);
PORT_UNRESOLVED_DATA(sfxB0);
PORT_UNRESOLVED_DATA(sfxB5);
PORT_UNRESOLVED_DATA(sfxB6);
PORT_UNRESOLVED_DATA(sfxB7);
PORT_UNRESOLVED_DATA(sfxB8);
PORT_UNRESOLVED_DATA(sfxB9);
PORT_UNRESOLVED_DATA(sfxBA);
PORT_UNRESOLVED_DATA(sfxBB);
PORT_UNRESOLVED_DATA(sfxBC);
PORT_UNRESOLVED_DATA(sfxBD);
PORT_UNRESOLVED_DATA(sfxBE);
PORT_UNRESOLVED_DATA(sfxBF);
PORT_UNRESOLVED_DATA(sfxBarrelEnter);
PORT_UNRESOLVED_DATA(sfxBarrelRelease);
PORT_UNRESOLVED_DATA(sfxBarrelRoll);
PORT_UNRESOLVED_DATA(sfxBarrelRollStop);
PORT_UNRESOLVED_DATA(sfxBeep);
PORT_UNRESOLVED_DATA(sfxBossDie);
PORT_UNRESOLVED_DATA(sfxBossExplode);
PORT_UNRESOLVED_DATA(sfxBossHit);
PORT_UNRESOLVED_DATA(sfxButtonDepress);
PORT_UNRESOLVED_DATA(sfxButtonPress);
PORT_UNRESOLVED_DATA(sfxC0);
PORT_UNRESOLVED_DATA(sfxC1);
PORT_UNRESOLVED_DATA(sfxC2);
PORT_UNRESOLVED_DATA(sfxC3);
PORT_UNRESOLVED_DATA(sfxC4);
PORT_UNRESOLVED_DATA(sfxC5);
PORT_UNRESOLVED_DATA(sfxC6);
PORT_UNRESOLVED_DATA(sfxC7);
PORT_UNRESOLVED_DATA(sfxC8);
PORT_UNRESOLVED_DATA(sfxC9);
PORT_UNRESOLVED_DATA(sfxCA);
PORT_UNRESOLVED_DATA(sfxCB);
PORT_UNRESOLVED_DATA(sfxCF);
PORT_UNRESOLVED_DATA(sfxChargingUp);
PORT_UNRESOLVED_DATA(sfxChestOpen);
PORT_UNRESOLVED_DATA(sfxCuccoMinigameBell);
PORT_UNRESOLVED_DATA(sfxD0);
PORT_UNRESOLVED_DATA(sfxD9);
PORT_UNRESOLVED_DATA(sfxDA);
PORT_UNRESOLVED_DATA(sfxE3);
PORT_UNRESOLVED_DATA(sfxE4);
PORT_UNRESOLVED_DATA(sfxEA);
PORT_UNRESOLVED_DATA(sfxEB);
PORT_UNRESOLVED_DATA(sfxEC);
PORT_UNRESOLVED_DATA(sfxED);
PORT_UNRESOLVED_DATA(sfxEE);
PORT_UNRESOLVED_DATA(sfxEF);
PORT_UNRESOLVED_DATA(sfxElementCharge);
PORT_UNRESOLVED_DATA(sfxElementFloat);
PORT_UNRESOLVED_DATA(sfxElementInfuse);
PORT_UNRESOLVED_DATA(sfxElementPlace);
PORT_UNRESOLVED_DATA(sfxEmArmosOn);
PORT_UNRESOLVED_DATA(sfxEmDekuscrubHit);
PORT_UNRESOLVED_DATA(sfxEmMoblinSpear);
PORT_UNRESOLVED_DATA(sfxEvaporate);
PORT_UNRESOLVED_DATA(sfxEzloUi);
PORT_UNRESOLVED_DATA(sfxF0);
PORT_UNRESOLVED_DATA(sfxF1);
PORT_UNRESOLVED_DATA(sfxF2);
PORT_UNRESOLVED_DATA(sfxF3);
PORT_UNRESOLVED_DATA(sfxF5);
PORT_UNRESOLVED_DATA(sfxF8);
PORT_UNRESOLVED_DATA(sfxFA);
PORT_UNRESOLVED_DATA(sfxFB);
PORT_UNRESOLVED_DATA(sfxFC);
PORT_UNRESOLVED_DATA(sfxFF);
PORT_UNRESOLVED_DATA(sfxFallHole);
PORT_UNRESOLVED_DATA(sfxHammer1);
PORT_UNRESOLVED_DATA(sfxHammer2);
PORT_UNRESOLVED_DATA(sfxHammer3);
PORT_UNRESOLVED_DATA(sfxHammer4);
PORT_UNRESOLVED_DATA(sfxHammer5);
PORT_UNRESOLVED_DATA(sfxHammer6);
PORT_UNRESOLVED_DATA(sfxHeartBounce);
PORT_UNRESOLVED_DATA(sfxHeartContainerSpawn);
PORT_UNRESOLVED_DATA(sfxHeartGet);
PORT_UNRESOLVED_DATA(sfxHit);
PORT_UNRESOLVED_DATA(sfxIceBlockMelt);
PORT_UNRESOLVED_DATA(sfxIceBlockSlide);
PORT_UNRESOLVED_DATA(sfxIceBlockStop);
PORT_UNRESOLVED_DATA(sfxItemBombExplode);
PORT_UNRESOLVED_DATA(sfxItemGlovesKnockback);
PORT_UNRESOLVED_DATA(sfxItemLanternOff);
PORT_UNRESOLVED_DATA(sfxItemLanternOn);
PORT_UNRESOLVED_DATA(sfxItemShieldBounce);
PORT_UNRESOLVED_DATA(sfxItemSwordBeam);
PORT_UNRESOLVED_DATA(sfxItemSwordCharge);
PORT_UNRESOLVED_DATA(sfxItemSwordChargeFinish);
PORT_UNRESOLVED_DATA(sfxKeyAppear);
PORT_UNRESOLVED_DATA(sfxLavaTitleFlip);
PORT_UNRESOLVED_DATA(sfxLavaTitleLand);
PORT_UNRESOLVED_DATA(sfxLavaTitleSink);
PORT_UNRESOLVED_DATA(sfxLavaTitleStep);
PORT_UNRESOLVED_DATA(sfxLavaTitleWobble);
PORT_UNRESOLVED_DATA(sfxLowHealth);
PORT_UNRESOLVED_DATA(sfxMenuCancel);
PORT_UNRESOLVED_DATA(sfxMenuError);
PORT_UNRESOLVED_DATA(sfxMetalClink);
PORT_UNRESOLVED_DATA(sfxMinish1);
PORT_UNRESOLVED_DATA(sfxMinish2);
PORT_UNRESOLVED_DATA(sfxMinish3);
PORT_UNRESOLVED_DATA(sfxMinish4);
PORT_UNRESOLVED_DATA(sfxNearPortal);
PORT_UNRESOLVED_DATA(sfxNone);
PORT_UNRESOLVED_DATA(sfxPlyDie);
PORT_UNRESOLVED_DATA(sfxPlyGrow);
PORT_UNRESOLVED_DATA(sfxPlyJump);
PORT_UNRESOLVED_DATA(sfxPlyLand);
PORT_UNRESOLVED_DATA(sfxPlyLift);
PORT_UNRESOLVED_DATA(sfxPlyShrinking);
PORT_UNRESOLVED_DATA(sfxPlyVo1);
PORT_UNRESOLVED_DATA(sfxPlyVo2);
PORT_UNRESOLVED_DATA(sfxPlyVo3);
PORT_UNRESOLVED_DATA(sfxPlyVo4);
PORT_UNRESOLVED_DATA(sfxPlyVo5);
PORT_UNRESOLVED_DATA(sfxPlyVo6);
PORT_UNRESOLVED_DATA(sfxPlyVo7);
PORT_UNRESOLVED_DATA(sfxPressurePlate);
PORT_UNRESOLVED_DATA(sfxRemSleep);
PORT_UNRESOLVED_DATA(sfxRupeeBounce);
PORT_UNRESOLVED_DATA(sfxRupeeGet);
PORT_UNRESOLVED_DATA(sfxSecret);
PORT_UNRESOLVED_DATA(sfxSecretBig);
PORT_UNRESOLVED_DATA(sfxSparkles);
PORT_UNRESOLVED_DATA(sfxSpiritsRelease);
PORT_UNRESOLVED_DATA(sfxStairs);
PORT_UNRESOLVED_DATA(sfxStairsAscend);
PORT_UNRESOLVED_DATA(sfxStairsDescend);
PORT_UNRESOLVED_DATA(sfxSummon);
PORT_UNRESOLVED_DATA(sfxTaskComplete);
PORT_UNRESOLVED_DATA(sfxTeleporter);
PORT_UNRESOLVED_DATA(sfxTextboxChoice);
PORT_UNRESOLVED_DATA(sfxTextboxClose);
PORT_UNRESOLVED_DATA(sfxTextboxNext);
PORT_UNRESOLVED_DATA(sfxTextboxOpen);
PORT_UNRESOLVED_DATA(sfxTextboxSelect);
PORT_UNRESOLVED_DATA(sfxTextboxSwap);
PORT_UNRESOLVED_DATA(sfxThudHeavy);
PORT_UNRESOLVED_DATA(sfxToggleDiving);
PORT_UNRESOLVED_DATA(sfxVoBeedle);
PORT_UNRESOLVED_DATA(sfxVoCat);
PORT_UNRESOLVED_DATA(sfxVoCheep);
PORT_UNRESOLVED_DATA(sfxVoCow);
PORT_UNRESOLVED_DATA(sfxVoCucco1);
PORT_UNRESOLVED_DATA(sfxVoCucco2);
PORT_UNRESOLVED_DATA(sfxVoCucco3);
PORT_UNRESOLVED_DATA(sfxVoCucco4);
PORT_UNRESOLVED_DATA(sfxVoCucco5);
PORT_UNRESOLVED_DATA(sfxVoCuccoCall);
PORT_UNRESOLVED_DATA(sfxVoDog);
PORT_UNRESOLVED_DATA(sfxVoEpona);
PORT_UNRESOLVED_DATA(sfxVoEzlo1);
PORT_UNRESOLVED_DATA(sfxVoEzlo2);
PORT_UNRESOLVED_DATA(sfxVoEzlo3);
PORT_UNRESOLVED_DATA(sfxVoEzlo4);
PORT_UNRESOLVED_DATA(sfxVoEzlo5);
PORT_UNRESOLVED_DATA(sfxVoEzlo6);
PORT_UNRESOLVED_DATA(sfxVoEzlo7);
PORT_UNRESOLVED_DATA(sfxVoGoron1);
PORT_UNRESOLVED_DATA(sfxVoGoron2);
PORT_UNRESOLVED_DATA(sfxVoGoron3);
PORT_UNRESOLVED_DATA(sfxVoGoron4);
PORT_UNRESOLVED_DATA(sfxVoKing1);
PORT_UNRESOLVED_DATA(sfxVoKing2);
PORT_UNRESOLVED_DATA(sfxVoKing3);
PORT_UNRESOLVED_DATA(sfxVoKing4);
PORT_UNRESOLVED_DATA(sfxVoKing5);
PORT_UNRESOLVED_DATA(sfxVoSturgeon);
PORT_UNRESOLVED_DATA(sfxVoTingle1);
PORT_UNRESOLVED_DATA(sfxVoTingle2);
PORT_UNRESOLVED_DATA(sfxVoZelda1);
PORT_UNRESOLVED_DATA(sfxVoZelda2);
PORT_UNRESOLVED_DATA(sfxVoZelda3);
PORT_UNRESOLVED_DATA(sfxVoZelda4);
PORT_UNRESOLVED_DATA(sfxVoZelda5);
PORT_UNRESOLVED_DATA(sfxVoZelda6);
PORT_UNRESOLVED_DATA(sfxVoZelda7);
PORT_UNRESOLVED_DATA(sfxWaterSplash);
PORT_UNRESOLVED_DATA(sfxWaterWalk);
PORT_UNRESOLVED_DATA(sfxWind1);
PORT_UNRESOLVED_DATA(sfxWind2);
PORT_UNRESOLVED_DATA(sfxWind3);

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
PORT_UNRESOLVED_FUNC(SetCollisionData)
PORT_UNRESOLVED_FUNC(GetActTileAtRoomCoords)
PORT_UNRESOLVED_DATA(script_PlayerGetElement);
PORT_UNRESOLVED_DATA(script_MazaalBossObjectMazaal);
PORT_UNRESOLVED_DATA(gUnk_080DD750);
PORT_UNRESOLVED_DATA(gUnk_0812079C);
PORT_UNRESOLVED_DATA(gUnk_081207A4);
PORT_UNRESOLVED_DATA(gUnk_081207AC);
PORT_UNRESOLVED_DATA(gUnk_020342F8);
PORT_UNRESOLVED_DATA(gUnk_02021F00);
PORT_UNRESOLVED_DATA(gUnk_080E4C08);
PORT_UNRESOLVED_DATA(gUnk_085A97A0);
