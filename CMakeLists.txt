cmake_minimum_required(VERSION 3.16)

# ============================================================================
# tmc SDL port build system.
#
# This CMake project is INTENTIONALLY independent from the existing GBA ROM
# build (see Makefile / GBA.mk). The ROM is still produced by `make`; this
# CMakeLists builds the host-side SDL port (PR #1 of the SDL-port roadmap;
# see docs/sdl_port.md for the full plan).
#
# Quick start:
#   cmake -S . -B build/sdl -DTMC_GAME_VERSION=USA
#   cmake --build build/sdl -j
#   ./build/sdl/tmc_sdl
# ============================================================================

project(tmc_sdl
    VERSION 0.1.0
    DESCRIPTION "The Legend of Zelda: The Minish Cap - SDL port"
    LANGUAGES C
)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
set(TMC_PORT_PLATFORM "sdl" CACHE STRING
    "Target port platform. Reserved values: sdl, win32, psp, ps2.")
set_property(CACHE TMC_PORT_PLATFORM PROPERTY STRINGS sdl win32 psp ps2)

set(TMC_GAME_VERSION "USA" CACHE STRING
    "Game version to build. One of: USA, EU, JP, DEMO_USA, DEMO_JP.")
set_property(CACHE TMC_GAME_VERSION PROPERTY STRINGS USA EU JP DEMO_USA DEMO_JP)

option(TMC_ENABLE_AUDIO   "Open SDL audio device (silent stub mixer for now)." ON)
option(TMC_ENABLE_GAMEPAD "Enable SDL_GameController / XInput gamepad support." ON)
option(TMC_WIDESCREEN     "Reserve hooks for the future widescreen renderer."   OFF)
option(TMC_USE_FETCHCONTENT_SDL
    "Fall back to FetchContent for SDL2 if it isn't found on the system." OFF)

# Sub-step 2b.4a of the SDL-port roadmap (see docs/sdl_port.md). The
# `tmc_game_sources` static library (built from the subset of
# `src/**/*.c` that compiles cleanly under `__PORT__`) is **always**
# built as a dependency of `tmc_sdl` so CI catches regressions in the
# leaf set. When `TMC_LINK_GAME_SOURCES=ON`, the library is also linked
# into the executable, replacing the empty-loop `agb_main_stub.c`
# placeholder with the real `src/main.c::AgbMain`. The 2b.4a foundation
# (host definitions of the `linker.ld` globals + unprefixed BIOS aliases
# under `__PORT__`) satisfies the link references rooted at `AgbMain`,
# but the full call graph still pulls in symbols from `src/enemy/`,
# `src/manager/`, `src/menu/`, `src/data/`, `asm/src/`, ... -- those
# will be added or stubbed in 2b.4b. Until then this option stays
# default OFF, and turning it ON requires also providing the missing
# symbol stubs.
option(TMC_LINK_GAME_SOURCES
    "Link the src/**/*.c game sources into tmc_sdl (PR #2b.4)." OFF)

# Only the "sdl" platform is implemented in this PR. Other values are
# reserved so future ports can slot into the same option without breaking the
# CLI surface.
if(NOT TMC_PORT_PLATFORM STREQUAL "sdl")
    message(FATAL_ERROR
        "TMC_PORT_PLATFORM='${TMC_PORT_PLATFORM}' is reserved but not yet "
        "implemented. Only 'sdl' is currently supported.")
endif()

# Translate the game-version selector into the existing -D switches that
# the decompiled C code already understands (see GBA.mk).
set(TMC_VERSION_DEFINES "")
if(TMC_GAME_VERSION STREQUAL "USA")
    list(APPEND TMC_VERSION_DEFINES USA ENGLISH)
elseif(TMC_GAME_VERSION STREQUAL "EU")
    list(APPEND TMC_VERSION_DEFINES EU ENGLISH)
elseif(TMC_GAME_VERSION STREQUAL "JP")
    list(APPEND TMC_VERSION_DEFINES JP JAPANESE)
elseif(TMC_GAME_VERSION STREQUAL "DEMO_USA")
    list(APPEND TMC_VERSION_DEFINES DEMO_USA ENGLISH)
elseif(TMC_GAME_VERSION STREQUAL "DEMO_JP")
    list(APPEND TMC_VERSION_DEFINES DEMO_JP JAPANESE)
else()
    message(FATAL_ERROR "Unknown TMC_GAME_VERSION='${TMC_GAME_VERSION}'.")
endif()
list(APPEND TMC_VERSION_DEFINES REVISION=0)

# ---------------------------------------------------------------------------
# Dependencies: SDL2
# ---------------------------------------------------------------------------
set(_TMC_SDL_TARGET "")
find_package(SDL2 QUIET CONFIG)
if(SDL2_FOUND)
    if(TARGET SDL2::SDL2)
        set(_TMC_SDL_TARGET SDL2::SDL2)
    endif()
else()
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        # Older FindSDL2 modules expose only variables.
        if(TARGET SDL2::SDL2)
            set(_TMC_SDL_TARGET SDL2::SDL2)
        endif()
    endif()
endif()

if(NOT _TMC_SDL_TARGET AND TMC_USE_FETCHCONTENT_SDL)
    message(STATUS "SDL2 not found locally; fetching via FetchContent.")
    include(FetchContent)
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-2.30.7
        GIT_SHALLOW    TRUE
    )
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON  CACHE BOOL "" FORCE)
    set(SDL_TEST   OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL2)
    set(_TMC_SDL_TARGET SDL2::SDL2-static)
endif()

if(NOT _TMC_SDL_TARGET)
    if(SDL2_FOUND AND DEFINED SDL2_LIBRARIES)
        # Last-resort fallback: build a synthetic INTERFACE target out of
        # whatever the legacy FindSDL2 module gave us.
        add_library(tmc_sdl2_iface INTERFACE)
        target_include_directories(tmc_sdl2_iface INTERFACE ${SDL2_INCLUDE_DIRS})
        target_link_libraries(tmc_sdl2_iface INTERFACE ${SDL2_LIBRARIES})
        set(_TMC_SDL_TARGET tmc_sdl2_iface)
    else()
        message(FATAL_ERROR
            "SDL2 not found. Install libsdl2-dev (Linux), brew install sdl2 "
            "(macOS), or install SDL2 from https://libsdl.org. Alternatively "
            "configure with -DTMC_USE_FETCHCONTENT_SDL=ON to download it.")
    endif()
endif()

# Optional SDL2main (Windows entry-point shim).
if(TARGET SDL2::SDL2main)
    set(_TMC_SDL_MAIN SDL2::SDL2main)
endif()

# ---------------------------------------------------------------------------
# Source list
#
# `tmc_sdl` always links the SDL platform layer + the cross-port shared
# layer (memory / interrupts / DMA / BIOS / save / asm stubs / port
# globals). When `TMC_LINK_GAME_SOURCES=ON`, the real game sources from
# `src/**/*.c` are pulled in via the `tmc_game_sources` static library
# defined further below, and the placeholder `agb_main_stub.c` is
# dropped so that `src/main.c::AgbMain` becomes the entry point. With
# the default `OFF`, the stub takes over and the platform layer is
# exercised in isolation (PR #2b.4b will flip the default to ON once
# the remaining link references from outside the leaf set are stubbed
# or ported — see docs/sdl_port.md).
# ---------------------------------------------------------------------------
set(TMC_PLATFORM_SDL_SOURCES
    src/platform/sdl/main.c
    src/platform/sdl/video.c
    src/platform/sdl/input.c
    src/platform/sdl/audio.c
)

set(TMC_PLATFORM_SHARED_SOURCES
    src/platform/shared/gba_memory.c
    src/platform/shared/interrupts.c
    src/platform/shared/dma.c
    src/platform/shared/bios.c
    src/platform/shared/m4a_stub.c
    src/platform/shared/ram_silent_stubs.c
    src/platform/shared/save_file.c
    src/platform/shared/port_globals.c
    src/platform/shared/port_headers_check.c
    src/platform/shared/asm_stubs.c
    src/platform/shared/render.c
    src/platform/shared/port_rom_data_stubs.c
)
# `port_unresolved_stubs.c` is intentionally NOT compiled directly into
# `tmc_sdl`: its weak symbols would otherwise be resolved before the
# linker scans `tmc_game_sources`, and a static library only pulls in
# objects to satisfy *undefined* symbols. To keep real definitions from
# being shadowed by the placeholders, the stubs live in their own
# `tmc_unresolved_stubs` archive that is linked **after**
# `tmc_game_sources` (see further below). Per the same logic, it is only
# linked when `TMC_LINK_GAME_SOURCES=ON`.

if(NOT TMC_LINK_GAME_SOURCES)
    # Fallback empty-loop AgbMain when the game sources are not linked.
    list(APPEND TMC_PLATFORM_SHARED_SOURCES src/platform/shared/agb_main_stub.c)
endif()

add_executable(tmc_sdl
    ${TMC_PLATFORM_SDL_SOURCES}
    ${TMC_PLATFORM_SHARED_SOURCES}
)

target_include_directories(tmc_sdl PRIVATE
    include
    include/platform
)

target_compile_definitions(tmc_sdl PRIVATE
    __PORT__
    PORT_SDL
    ${TMC_VERSION_DEFINES}
)

if(TMC_ENABLE_AUDIO)
    target_compile_definitions(tmc_sdl PRIVATE TMC_ENABLE_AUDIO=1)
else()
    target_compile_definitions(tmc_sdl PRIVATE TMC_ENABLE_AUDIO=0)
endif()
if(TMC_ENABLE_GAMEPAD)
    target_compile_definitions(tmc_sdl PRIVATE TMC_ENABLE_GAMEPAD=1)
else()
    target_compile_definitions(tmc_sdl PRIVATE TMC_ENABLE_GAMEPAD=0)
endif()
if(TMC_WIDESCREEN)
    target_compile_definitions(tmc_sdl PRIVATE TMC_WIDESCREEN=1)
endif()
# Mirror the CMake option into a preprocessor define so platform/shared
# TUs (e.g. src/platform/shared/interrupts.c) can compile out their
# placeholder definitions of symbols (like VBlankIntr) that the real
# game library will provide when linked. Without this, the placeholder
# is always defined and collides with src/interrupts.c::VBlankIntr at
# link time. Defined as 1/0 so `#if TMC_LINK_GAME_SOURCES` works too.
if(TMC_LINK_GAME_SOURCES)
    target_compile_definitions(tmc_sdl PRIVATE TMC_LINK_GAME_SOURCES=1)
else()
    target_compile_definitions(tmc_sdl PRIVATE TMC_LINK_GAME_SOURCES=0)
endif()

# Compiler flags. The decompiled game code relies heavily on type-punning,
# so strict-aliasing must stay off for any future PR that links it. We
# enable the same flag here so the platform layer matches.
if(MSVC)
    target_compile_options(tmc_sdl PRIVATE /W3 /wd4146 /wd4244 /wd4305)
else()
    target_compile_options(tmc_sdl PRIVATE
        -O2 -g
        -Wall
        -Wno-multichar
        -Wno-parentheses
        -Wno-unused-parameter
        -Wno-unused-function
        -Wno-unused-variable
        -fno-strict-aliasing
    )
endif()

target_link_libraries(tmc_sdl PRIVATE ${_TMC_SDL_TARGET})
if(_TMC_SDL_MAIN)
    target_link_libraries(tmc_sdl PRIVATE ${_TMC_SDL_MAIN})
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(tmc_sdl PRIVATE m)
endif()

# ---------------------------------------------------------------------------
# Optional: compile the leaf-utility subset of src/**/*.c (PR #2b.2-2b.4).
#
# These TUs are almost all const data tables / function-pointer dispatch
# tables with no struct-layout dependencies, so they already compile clean
# under `__PORT__` with the SDL build's flags. The rest of `src/` (the
# `src/enemy/`, `src/data/`, `src/manager/`, `src/menu/`, ... subdirs)
# still needs the agbcc-ism / 32-bit-pointer fixes that future PRs will
# track, so this list only grows by hand as each file is verified.
#
# Since PR #2b.4 the library can be built cleanly as a dependency; it is
# only linked into `tmc_sdl` when `TMC_LINK_GAME_SOURCES=ON`. The unresolved
# cross-TU references that 2b.3 had outstanding are now satisfied by
# `src/platform/shared/port_globals.c` (host BSS for the linker-script
# globals), the `__PORT__` BIOS aliases in `bios.c` / `interrupts.c`,
# and - as of 2b.4b - the weak abort/BSS placeholders in
# `src/platform/shared/port_unresolved_stubs.c` for the ~850 symbols
# that still live only in unported `asm/src/`, the not-yet-linked
# `src/gba/m4a.c`, and the large const tables under `data/`.
# ---------------------------------------------------------------------------
set(TMC_GAME_LEAF_SOURCES
    # Original 2b.2 leaf set: pure const data / function-pointer dispatch
    # tables.
    src/droptables.c
    src/enemy.c
    src/flagDebug.c
    src/manager.c
    src/npcDefinitions.c
    src/npcFunctions.c
    src/object.c
    src/objectDefinitions.c
    src/playerHitbox.c
    src/playerItemDefinitions.c
    src/projectile.c
    src/sineTable.c
    # 2b.3 wave 1: files that only needed the `static_assert` no-op in
    # include/global.h to compile cleanly under __PORT__. These TUs do not
    # make struct-layout assumptions that break on a 64-bit host, so with
    # the header fix in place they build as-is.
    src/beanstalkSubtask.c
    src/code_08049CD4.c
    src/code_08049DF4.c
    src/collision.c
    src/color.c
    src/debug.c
    src/demo.c
    src/enemyUtils.c
    src/enterPortalSubtask.c
    src/entity.c
    src/fade.c
    src/fileselect.c
    src/flags.c
    src/game.c
    src/gameData.c
    src/gameOverTask.c
    src/gameUtils.c
    src/interrupts.c
    src/item.c
    src/itemDefinitions.c
    src/itemMetaData.c
    src/itemUtils.c
    src/kinstone.c
    src/main.c
    src/message.c
    src/movement.c
    src/npc.c
    src/objectUtils.c
    src/physics.c
    src/player.c
    src/playerItem.c
    src/playerItemUtils.c
    src/playerUtils.c
    src/projectileUtils.c
    src/roomInit.c
    src/save.c
    src/screenTileMap.c
    src/script.c
    src/scroll.c
    src/sound.c
    src/staffroll.c
    src/subtask.c
    src/text.c
    src/title.c
    src/ui.c
    src/vram.c
    # 2b.3 wave 2: files that needed a small `#ifdef __PORT__` rewrite of
    # the legacy GCC cast-as-lvalue extension (`(u8*)p += n;`), and the
    # `r5` register-clobber asm hint in `room.c` that exists purely to
    # nudge the matching ROM compiler's regalloc.
    src/affine.c
    src/code_0805EC04.c
    src/eeprom.c
    src/npcUtils.c
    src/room.c
    # 2b.3 wave 3: files that depend on assets/*_offsets.h (normally
    # produced by the asset_processor during the GBA ROM build) and on
    # storing ARM function addresses in a u32 slot. The offset headers
    # have host-port stubs under src/platform/shared/generated/assets/
    # (regenerated by tools/port/gen_offset_stubs.py); the u32 slot is
    # widened to uintptr_t under __PORT__ via a small include/room.h
    # tweak and the matching PORT_ROM_PTR() macro in include/global.h.
    src/backgroundAnimations.c
    src/common.c
    src/cutscene.c
    # 2b.4b: the full remaining src/**/*.c tree. All of these compile
    # cleanly under `__PORT__` with the SDL build flags; the link step
    # resolves any still-missing BSS globals / script + enemy / m4a
    # symbols via src/platform/shared/port_globals.c and
    # src/platform/shared/port_unresolved_stubs.c (weak abort/BSS
    # placeholders). See docs/sdl_port.md (PR #2b.4b).
    src/data/areaMetadata.c
    src/data/caveBorderMapData.c
    src/data/data_080046A4.c
    src/data/figurineMenuData.c
    src/data/hitbox.c
    src/data/mapActTileToSurfaceType.c
    src/data/objPalettes.c
    src/data/screenTransitions.c
    src/data/transitions.c
    src/enemy/acroBandits.c
    src/enemy/armos.c
    src/enemy/ballChainSoldier.c
    src/enemy/beetle.c
    src/enemy/bladeTrap.c
    src/enemy/bobomb.c
    src/enemy/bombPeahat.c
    src/enemy/bombarossa.c
    src/enemy/bowMoblin.c
    src/enemy/businessScrub.c
    src/enemy/businessScrubPrologue.c
    src/enemy/chaser.c
    src/enemy/chuchu.c
    src/enemy/chuchuBoss.c
    src/enemy/cloudPiranha.c
    src/enemy/crow.c
    src/enemy/cuccoAggr.c
    src/enemy/cuccoChickAggr.c
    src/enemy/curtain.c
    src/enemy/darkNut.c
    src/enemy/doorMimic.c
    src/enemy/dust.c
    src/enemy/enemy4D.c
    src/enemy/enemy50.c
    src/enemy/enemy64.c
    src/enemy/enemy66.c
    src/enemy/enemyE.c
    src/enemy/eyegore.c
    src/enemy/fallingBoulder.c
    src/enemy/fireballGuy.c
    src/enemy/flyingPot.c
    src/enemy/flyingSkull.c
    src/enemy/ghini.c
    src/enemy/gibdo.c
    src/enemy/gleerok.c
    src/enemy/gyorgChild.c
    src/enemy/gyorgFemale.c
    src/enemy/gyorgFemaleEye.c
    src/enemy/gyorgFemaleMouth.c
    src/enemy/gyorgMale.c
    src/enemy/gyorgMaleEye.c
    src/enemy/hangingSeed.c
    src/enemy/helmasaur.c
    src/enemy/keaton.c
    src/enemy/keese.c
    src/enemy/lakitu.c
    src/enemy/lakituCloud.c
    src/enemy/leever.c
    src/enemy/likeLike.c
    src/enemy/madderpillar.c
    src/enemy/mazaalBracelet.c
    src/enemy/mazaalHand.c
    src/enemy/mazaalHead.c
    src/enemy/mazaalMacro.c
    src/enemy/miniFireballGuy.c
    src/enemy/miniSlime.c
    src/enemy/moldorm.c
    src/enemy/moldworm.c
    src/enemy/mulldozer.c
    src/enemy/octorok.c
    src/enemy/octorokBoss.c
    src/enemy/octorokGolden.c
    src/enemy/peahat.c
    src/enemy/pesto.c
    src/enemy/puffstool.c
    src/enemy/rockChuchu.c
    src/enemy/rollobite.c
    src/enemy/rope.c
    src/enemy/ropeGolden.c
    src/enemy/rupeeLike.c
    src/enemy/scissorsBeetle.c
    src/enemy/sensorBladeTrap.c
    src/enemy/slime.c
    src/enemy/sluggula.c
    src/enemy/smallPesto.c
    src/enemy/spark.c
    src/enemy/spearMoblin.c
    src/enemy/spikedBeetle.c
    src/enemy/spinyBeetle.c
    src/enemy/spinyChuchu.c
    src/enemy/stalfos.c
    src/enemy/sub_080451CC.c
    src/enemy/takkuri.c
    src/enemy/tektite.c
    src/enemy/tektiteGolden.c
    src/enemy/torchTrap.c
    src/enemy/treeItem.c
    src/enemy/vaatiArm.c
    src/enemy/vaatiBall.c
    src/enemy/vaatiEyesMacro.c
    src/enemy/vaatiProjectile.c
    src/enemy/vaatiRebornEnemy.c
    src/enemy/vaatiTransfigured.c
    src/enemy/vaatiTransfiguredEye.c
    src/enemy/vaatiWrath.c
    src/enemy/vaatiWrathEye.c
    src/enemy/wallMaster.c
    src/enemy/wallMaster2.c
    src/enemy/waterDrop.c
    src/enemy/wisp.c
    src/enemy/wizzrobeFire.c
    src/enemy/wizzrobeIce.c
    src/enemy/wizzrobeWind.c
    src/item/itemBomb.c
    src/item/itemBoomerang.c
    src/item/itemBow.c
    src/item/itemDebug.c
    src/item/itemGustJar.c
    src/item/itemJarEmpty.c
    src/item/itemLantern.c
    src/item/itemMoleMitts.c
    src/item/itemOcarina.c
    src/item/itemPacciCane.c
    src/item/itemPegasusBoots.c
    src/item/itemRocsCape.c
    src/item/itemShield.c
    src/item/itemSword.c
    src/item/itemTryPickupObject.c
    src/manager/angryStatueManager.c
    src/manager/animatedBackgroundManager.c
    src/manager/armosInteriorManager.c
    src/manager/bombableWallManager.c
    src/manager/bridgeManager.c
    src/manager/cameraTargetManager.c
    src/manager/cloudOverlayManager.c
    src/manager/cloudStaircaseTransitionManager.c
    src/manager/delayedEntityLoadManager.c
    src/manager/destructibleTileObserveManager.c
    src/manager/diggingCaveEntranceManager.c
    src/manager/enemyInteractionManager.c
    src/manager/enterRoomTextboxManager.c
    src/manager/entitySpawnManager.c
    src/manager/ezloHintManager.c
    src/manager/fallingItemManager.c
    src/manager/fightManager.c
    src/manager/flagAndOperatorManager.c
    src/manager/flameManager.c
    src/manager/floatingPlatformManager.c
    src/manager/goronMerchantShopManager.c
    src/manager/holeManager.c
    src/manager/horizontalMinishPathBackgroundManager.c
    src/manager/houseSignManager.c
    src/manager/hyruleTownBellManager.c
    src/manager/hyruleTownTileSetManager.c
    src/manager/lightLevelSetManager.c
    src/manager/lightManager.c
    src/manager/lightRayManager.c
    src/manager/manager29.c
    src/manager/minishPortalManager.c
    src/manager/minishRaftersBackgroundManager.c
    src/manager/minishSizedEntranceManager.c
    src/manager/minishVillageTileSetManager.c
    src/manager/miscManager.c
    src/manager/moveableObjectManager.c
    src/manager/powBackgroundManager.c
    src/manager/pushableFurnitureManager.c
    src/manager/railIntersectionManager.c
    src/manager/rainfallManager.c
    src/manager/regionTriggerManager.c
    src/manager/repeatedSoundManager.c
    src/manager/rollingBarrelManager.c
    src/manager/secretManager.c
    src/manager/specialWarpManager.c
    src/manager/staticBackgroundManager.c
    src/manager/steamOverlayManager.c
    src/manager/templeOfDropletsManager.c
    src/manager/tileChangeObserveManager.c
    src/manager/tilePuzzleManager.c
    src/manager/vaati3BackgroundManager.c
    src/manager/vaati3InsideArmManager.c
    src/manager/vaati3StartManager.c
    src/manager/vaatiAppearingManager.c
    src/manager/verticalMinishPathBackgroundManager.c
    src/manager/waterfallBottomManager.c
    src/manager/weatherChangeManager.c
    src/menu/figurineMenu.c
    src/menu/kinstoneMenu.c
    src/menu/pauseMenu.c
    src/menu/pauseMenuScreen6.c
    src/npc/anju.c
    src/npc/beedle.c
    src/npc/bigGoron.c
    src/npc/bladeBrothers.c
    src/npc/brocco.c
    src/npc/carlov.c
    src/npc/carpenter.c
    src/npc/castleMaid.c
    src/npc/castorWildsStatue.c
    src/npc/cat.c
    src/npc/clothesRack.c
    src/npc/cow.c
    src/npc/cucco.c
    src/npc/cuccoChick.c
    src/npc/dampe.c
    src/npc/din.c
    src/npc/dog.c
    src/npc/drLeft.c
    src/npc/emma.c
    src/npc/epona.c
    src/npc/ezlo.c
    src/npc/farmers.c
    src/npc/farore.c
    src/npc/festari.c
    src/npc/forestMinish.c
    src/npc/fusionMenuNPC.c
    src/npc/gentari.c
    src/npc/ghostBrothers.c
    src/npc/gina.c
    src/npc/gorman.c
    src/npc/goron.c
    src/npc/goronMerchant.c
    src/npc/gregal.c
    src/npc/guard.c
    src/npc/guardWithSpear.c
    src/npc/hurdyGurdyMan.c
    src/npc/kid.c
    src/npc/kingDaltus.c
    src/npc/kingGustaf.c
    src/npc/librari.c
    src/npc/librarians.c
    src/npc/mailbox.c
    src/npc/malon.c
    src/npc/mama.c
    src/npc/marcy.c
    src/npc/mayorHagen.c
    src/npc/melari.c
    src/npc/milkCart.c
    src/npc/minishEzlo.c
    src/npc/ministerPotho.c
    src/npc/moblinLady.c
    src/npc/mountainMinish.c
    src/npc/mutoh.c
    src/npc/nayru.c
    src/npc/npc23.c
    src/npc/npc26.c
    src/npc/npc4E.c
    src/npc/npc4F.c
    src/npc/npc5.c
    src/npc/npc58.c
    src/npc/npc9.c
    src/npc/percy.c
    src/npc/phonograph.c
    src/npc/picolyteBottle.c
    src/npc/pina.c
    src/npc/pita.c
    src/npc/postman.c
    src/npc/rem.c
    src/npc/simon.c
    src/npc/sittingPerson.c
    src/npc/smallTownMinish.c
    src/npc/smith.c
    src/npc/stamp.c
    src/npc/stockwell.c
    src/npc/sturgeon.c
    src/npc/syrup.c
    src/npc/talon.c
    src/npc/teachers.c
    src/npc/tingleSiblings.c
    src/npc/townMinish.c
    src/npc/townsperson.c
    src/npc/vaati.c
    src/npc/vaatiReborn.c
    src/npc/wheaton.c
    src/npc/windTribespeople.c
    src/npc/zelda.c
    src/npc/zeldaFollower.c
    src/object/ambientClouds.c
    src/object/angryStatue.c
    src/object/archway.c
    src/object/backgroundCloud.c
    src/object/bakerOven.c
    src/object/barrelInside.c
    src/object/barrelSpiderweb.c
    src/object/beanstalk.c
    src/object/bedCover.c
    src/object/bell.c
    src/object/bench.c
    src/object/bigBarrel.c
    src/object/bigIceBlock.c
    src/object/bigPushableLever.c
    src/object/bigVortex.c
    src/object/bird.c
    src/object/blockPushed.c
    src/object/board.c
    src/object/bollard.c
    src/object/book.c
    src/object/bossDoor.c
    src/object/bush.c
    src/object/button.c
    src/object/cabinFurniture.c
    src/object/cameraTarget.c
    src/object/carlovObject.c
    src/object/chestSpawner.c
    src/object/chuchuBossCutscene.c
    src/object/chuchuBossParticle.c
    src/object/chuchuBossStartParticle.c
    src/object/cloud.c
    src/object/crackingGround.c
    src/object/crenelBeanSprout.c
    src/object/cuccoMinigame.c
    src/object/cutsceneMiscObject.c
    src/object/cutsceneOrchestrator.c
    src/object/deathFx.c
    src/object/dirtParticle.c
    src/object/doubleBookshelf.c
    src/object/elementsBackground.c
    src/object/enemyItem.c
    src/object/evilSpirit.c
    src/object/eyeSwitch.c
    src/object/ezloCap.c
    src/object/ezloCapFlying.c
    src/object/fairy.c
    src/object/fan.c
    src/object/fanWind.c
    src/object/figurineDevice.c
    src/object/fileScreenObjects.c
    src/object/fireballChain.c
    src/object/fireplace.c
    src/object/flame.c
    src/object/floatingBlock.c
    src/object/floatingPlatform.c
    src/object/fourElements.c
    src/object/frozenFlower.c
    src/object/frozenOctorok.c
    src/object/frozenWaterElement.c
    src/object/furniture.c
    src/object/gentariCurtain.c
    src/object/giantBookLadder.c
    src/object/giantLeaf.c
    src/object/giantRock.c
    src/object/giantRock2.c
    src/object/giantTwig.c
    src/object/gleerokParticle.c
    src/object/graveyardKey.c
    src/object/greatFairy.c
    src/object/guruguruBar.c
    src/object/gustJarParticle.c
    src/object/gyorgBossObject.c
    src/object/heartContainer.c
    src/object/hiddenLadderDown.c
    src/object/hitSwitch.c
    src/object/hittableLever.c
    src/object/houseDoorExterior.c
    src/object/houseDoorInterior.c
    src/object/houseSign.c
    src/object/itemForSale.c
    src/object/itemOnGround.c
    src/object/jailBars.c
    src/object/japaneseSubtitle.c
    src/object/jarPortal.c
    src/object/keyStealingTakkuri.c
    src/object/kinstoneSpark.c
    src/object/ladderHoleInBookshelf.c
    src/object/ladderUp.c
    src/object/lamp.c
    src/object/lampParticle.c
    src/object/lavaPlatform.c
    src/object/lightDoor.c
    src/object/lightRay.c
    src/object/lightableSwitch.c
    src/object/lilypadLarge.c
    src/object/lilypadLargeFalling.c
    src/object/lilypadSmall.c
    src/object/linkAnimation.c
    src/object/linkEmptyingBottle.c
    src/object/linkFire.c
    src/object/linkHoldingItem.c
    src/object/litArea.c
    src/object/lockedDoor.c
    src/object/macroAcorn.c
    src/object/macroBook.c
    src/object/macroDecorations.c
    src/object/macroMushroomStalk.c
    src/object/macroPlayer.c
    src/object/macroShoe.c
    src/object/mask.c
    src/object/mazaalBossObject.c
    src/object/mazaalObject.c
    src/object/metalDoor.c
    src/object/minecart.c
    src/object/minecartDoor.c
    src/object/mineralWaterSource.c
    src/object/minishEmoticon.c
    src/object/minishLight.c
    src/object/minishPortalCloseup.c
    src/object/minishPortalStone.c
    src/object/minishSizedArchway.c
    src/object/minishSizedEntrance.c
    src/object/minishVillageObject.c
    src/object/moleMittsParticle.c
    src/object/mulldozerSpawnPoint.c
    src/object/object1D.c
    src/object/object1F.c
    src/object/object30.c
    src/object/object37.c
    src/object/object53.c
    src/object/object63.c
    src/object/object70.c
    src/object/objectA.c
    src/object/objectA2.c
    src/object/objectA7.c
    src/object/objectA8.c
    src/object/objectBF.c
    src/object/objectBlockingStairs.c
    src/object/objectOnPillar.c
    src/object/objectOnSpinyBeetle.c
    src/object/octorokBossObject.c
    src/object/palaceArchway.c
    src/object/paper.c
    src/object/parallaxRoomView.c
    src/object/picoBloom.c
    src/object/pinwheel.c
    src/object/playerClone.c
    src/object/pot.c
    src/object/pressurePlate.c
    src/object/pullableLever.c
    src/object/pullableMushroom.c
    src/object/pushableFurniture.c
    src/object/pushableGrave.c
    src/object/pushableLever.c
    src/object/pushableRock.c
    src/object/pushableStatue.c
    src/object/railtrack.c
    src/object/rotatingTrapdoor.c
    src/object/rupee.c
    src/object/sanctuaryStoneTablet.c
    src/object/shrinkingHieroglyphs.c
    src/object/smallIceBlock.c
    src/object/smoke.c
    src/object/smokeParticle.c
    src/object/specialChest.c
    src/object/specialFx.c
    src/object/steam.c
    src/object/stoneTablet.c
    src/object/swordParticle.c
    src/object/swordsmanNewsletter.c
    src/object/thoughtBubble.c
    src/object/thunderbolt.c
    src/object/titleScreenObject.c
    src/object/trapdoor.c
    src/object/treeHidingPortal.c
    src/object/treeThorns.c
    src/object/unusedSkull.c
    src/object/vaati1Portal.c
    src/object/vaati2Particle.c
    src/object/vaati3Arm.c
    src/object/vaati3Death.c
    src/object/vaati3PlayerObject.c
    src/object/warpPoint.c
    src/object/waterDropObject.c
    src/object/waterElement.c
    src/object/waterfallOpening.c
    src/object/well.c
    src/object/whirlwind.c
    src/object/whiteTriangleEffect.c
    src/object/windTribeFlag.c
    src/object/windTribeTeleporter.c
    src/object/windcrest.c
    src/playerItem/playerItemBomb.c
    src/playerItem/playerItemBoomerang.c
    src/playerItem/playerItemBottle.c
    src/playerItem/playerItemBow.c
    src/playerItem/playerItemCellOverwriteSet.c
    src/playerItem/playerItemDashSword.c
    src/playerItem/playerItemFireRodProjectile.c
    src/playerItem/playerItemGust.c
    src/playerItem/playerItemGustBig.c
    src/playerItem/playerItemGustJar.c
    src/playerItem/playerItemHeldObject.c
    src/playerItem/playerItemLantern.c
    src/playerItem/playerItemPacciCane.c
    src/playerItem/playerItemPacciCaneProjectile.c
    src/playerItem/playerItemShield.c
    src/playerItem/playerItemSpiralBeam.c
    src/playerItem/playerItemSword.c
    src/playerItem/playerItemSwordBeam.c
    src/projectile/arrowProjectile.c
    src/projectile/ballAndChain.c
    src/projectile/boneProjectile.c
    src/projectile/cannonballProjectile.c
    src/projectile/darkNutSwordSlash.c
    src/projectile/dekuSeedProjectile.c
    src/projectile/dirtBallProjectile.c
    src/projectile/fireProjectile.c
    src/projectile/gleerokProjectile.c
    src/projectile/guardLineOfSight.c
    src/projectile/guruguruBarProjectile.c
    src/projectile/gyorgMaleEnergyProjectile.c
    src/projectile/gyorgTail.c
    src/projectile/iceProjectile.c
    src/projectile/keatonDagger.c
    src/projectile/lakituCloudProjectile.c
    src/projectile/lakituLightning.c
    src/projectile/mandiblesProjectile.c
    src/projectile/mazaalEnergyBeam.c
    src/projectile/moblinSpear.c
    src/projectile/octorokBossProjectile.c
    src/projectile/projectile5.c
    src/projectile/removableDust.c
    src/projectile/rockProjectile.c
    src/projectile/spiderWeb.c
    src/projectile/spikedRollers.c
    src/projectile/stalfosProjectile.c
    src/projectile/torchTrapProjectile.c
    src/projectile/v1DarkMagicProjectile.c
    src/projectile/v1EyeLaser.c
    src/projectile/v1FireProjectile.c
    src/projectile/v2Projectile.c
    src/projectile/v3ElectricProjectile.c
    src/projectile/v3HandProjectile.c
    src/projectile/v3TennisBallProjectile.c
    src/projectile/windProjectile.c
    src/projectile/winder.c
    src/subtask/subtaskAuxCutscene.c
    src/subtask/subtaskFastTravel.c
    src/subtask/subtaskLocalMapHint.c
    src/subtask/subtaskMapHint.c
    src/subtask/subtaskWorldEvent.c
    src/worldEvent/worldEvent0.c
    src/worldEvent/worldEvent1.c
    src/worldEvent/worldEvent10.c
    src/worldEvent/worldEvent11.c
    src/worldEvent/worldEvent12.c
    src/worldEvent/worldEvent13.c
    src/worldEvent/worldEvent14.c
    src/worldEvent/worldEvent15.c
    src/worldEvent/worldEvent16.c
    src/worldEvent/worldEvent17.c
    src/worldEvent/worldEvent19.c
    src/worldEvent/worldEvent2.c
    src/worldEvent/worldEvent20.c
    src/worldEvent/worldEvent21.c
    src/worldEvent/worldEvent22.c
    src/worldEvent/worldEvent23.c
    src/worldEvent/worldEvent24.c
    src/worldEvent/worldEvent25.c
    src/worldEvent/worldEvent3.c
    src/worldEvent/worldEvent4.c
    src/worldEvent/worldEvent5.c
    src/worldEvent/worldEvent6.c
    src/worldEvent/worldEvent7.c
    src/worldEvent/worldEvent8.c
    src/worldEvent/worldEvent9.c
)

add_library(tmc_game_sources STATIC ${TMC_GAME_LEAF_SOURCES})
target_include_directories(tmc_game_sources PRIVATE
    include
    include/platform
    # Host-port stub headers for `assets/*_offsets.h`, which the
    # matching ROM build generates from the asset_processor. The
    # SDL port must not touch `assets/`, so these stubs live under
    # a separate include root. See tools/port/gen_offset_stubs.py
    # and docs/sdl_port.md (PR #2b.3, wave 3).
    src/platform/shared/generated
)
target_compile_definitions(tmc_game_sources PRIVATE
    __PORT__
    PORT_SDL
    ${TMC_VERSION_DEFINES}
)
if(MSVC)
    target_compile_options(tmc_game_sources PRIVATE /W3 /wd4146 /wd4244 /wd4305)
else()
    target_compile_options(tmc_game_sources PRIVATE
        -O2 -g
        -Wall
        -Wno-multichar
        -Wno-parentheses
        -Wno-unused-parameter
        -Wno-unused-function
        -Wno-unused-variable
        -Wno-missing-braces
        -fno-strict-aliasing
    )
endif()
# Always build the library alongside tmc_sdl so CI catches regressions
# in the leaf set on every PR. Only LINK it into the executable when
# `TMC_LINK_GAME_SOURCES=ON`; with the default OFF, the build still
# produces a working `tmc_sdl` from the empty-loop `agb_main_stub.c`,
# while the leaf set is continuously compile-verified.
add_dependencies(tmc_sdl tmc_game_sources)

# Weak-symbol placeholder archive for the ~850 references that
# `tmc_game_sources` still pulls in from outside the host build's
# reach (raw ARM asm in `asm/src/`, the unbuilt `src/gba/m4a.c`, and
# the const tables under `data/*.c`). Built as its own static library
# so it can be ordered **after** `tmc_game_sources` on the linker
# command line: a static archive only contributes objects that
# satisfy currently-undefined symbols, so this ordering ensures any
# real (strong) definition that exists inside `tmc_game_sources` is
# extracted first, and the weak placeholder only fills genuine gaps.
# See docs/sdl_port.md (PR #2b.4b) and the same-named source comment.
add_library(tmc_unresolved_stubs STATIC
    src/platform/shared/port_unresolved_stubs.c
)
target_include_directories(tmc_unresolved_stubs PRIVATE
    include
    include/platform
)
target_compile_definitions(tmc_unresolved_stubs PRIVATE
    __PORT__
    PORT_SDL
    ${TMC_VERSION_DEFINES}
)
if(MSVC)
    target_compile_options(tmc_unresolved_stubs PRIVATE /W3)
else()
    target_compile_options(tmc_unresolved_stubs PRIVATE
        -O2 -g -Wall -Wno-unused-parameter -Wno-unused-function
        -fno-strict-aliasing
    )
endif()
add_dependencies(tmc_sdl tmc_unresolved_stubs)

if(TMC_LINK_GAME_SOURCES)
    # PR #2b.4a foundation makes this link possible for the call graph
    # rooted at `AgbMain` provided the missing-symbol stubs from 2b.4b
    # are also present. Order matters: `tmc_unresolved_stubs` MUST come
    # after `tmc_game_sources` so the linker pulls real (strong)
    # definitions out of the game library first and only falls through
    # to the weak placeholders for genuine gaps.
    target_link_libraries(tmc_sdl PRIVATE tmc_game_sources tmc_unresolved_stubs)
endif()

# ---------------------------------------------------------------------------
# Install (optional, mostly for packaging future releases)
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS tmc_sdl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES docs/sdl_port.md DESTINATION ${CMAKE_INSTALL_DOCDIR})

# ---------------------------------------------------------------------------
# Status banner
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "tmc SDL port configuration:")
message(STATUS "  Platform        : ${TMC_PORT_PLATFORM}")
message(STATUS "  Game version    : ${TMC_GAME_VERSION}")
message(STATUS "  Audio           : ${TMC_ENABLE_AUDIO}")
message(STATUS "  Gamepad         : ${TMC_ENABLE_GAMEPAD}")
message(STATUS "  Widescreen hook : ${TMC_WIDESCREEN}")
message(STATUS "  Link game srcs  : ${TMC_LINK_GAME_SOURCES}")
message(STATUS "  SDL2 target     : ${_TMC_SDL_TARGET}")
message(STATUS "")
