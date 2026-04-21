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
    src/platform/shared/save_file.c
    src/platform/shared/port_globals.c
    src/platform/shared/port_headers_check.c
    src/platform/shared/asm_stubs.c
)

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
# Since PR #2b.4 the library is linked into `tmc_sdl` by default; the
# unresolved cross-TU references that 2b.3 had outstanding are now
# satisfied by `src/platform/shared/port_globals.c` (host BSS for the
# linker-script globals) and the `__PORT__` BIOS aliases in `bios.c` /
# `interrupts.c`.
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
if(TMC_LINK_GAME_SOURCES)
    # PR #2b.4a foundation makes this link possible for the call graph
    # rooted at `AgbMain` provided the missing-symbol stubs from 2b.4b
    # are also present (not yet checked in -- see docs/sdl_port.md).
    target_link_libraries(tmc_sdl PRIVATE tmc_game_sources)
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
