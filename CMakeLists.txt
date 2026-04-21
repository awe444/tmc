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
# This PR only links the platform layer + a placeholder AgbMain (see
# src/platform/shared/agb_main_stub.c). Wiring the real game C sources
# (src/**/*.c) into this target is PR #2 of the roadmap and is a much
# larger change because the existing decomp still relies on agbcc-specific
# constructs and unported ARM assembly. The stub keeps the platform layer
# buildable and exercisable on its own in the meantime.
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
    src/platform/shared/agb_main_stub.c
    src/platform/shared/port_headers_check.c
)

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
message(STATUS "  SDL2 target     : ${_TMC_SDL_TARGET}")
message(STATUS "")
