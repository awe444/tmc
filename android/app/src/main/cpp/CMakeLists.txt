# Native build for the Android port of tmc_pc.
#
# Produces libmain.so, which is what SDLActivity.getLibraries() loads after
# libSDL3.so and where it looks up SDL_main. Everything the desktop build
# compiles into tmc_pc is compiled here too -- the source list is READ FROM
# xmake.lua rather than copied, so a file added to the desktop target joins
# this one without a second edit. xmake.lua stays the single source of truth.

cmake_minimum_required(VERSION 3.22)
project(tmc_android C CXX)

get_filename_component(TMC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)

if(NOT EXISTS "${TMC_ROOT}/xmake.lua")
    message(FATAL_ERROR "Repository root not found at ${TMC_ROOT} (no xmake.lua).")
endif()

# ---------------------------------------------------------------------------
# Source list, parsed out of the tmc_pc target in xmake.lua
# ---------------------------------------------------------------------------
# Matches `    add_files("path")` between target("tmc_pc") and its target_end().
# The leading-whitespace anchor is what excludes the commented-out launcher
# entries (`    --   add_files(...)`), which must NOT be built: the
# tmc-Modern-Launcher submodule is not publicly reachable.

file(STRINGS "${TMC_ROOT}/xmake.lua" TMC_XMAKE_LINES)

# Version string, from the same constant at the top of xmake.lua that the
# desktop build injects (port_version.h only carries an indexer fallback).
set(TMC_PC_VERSION "")
foreach(line IN LISTS TMC_XMAKE_LINES)
    if(line MATCHES "^local TMC_PC_VERSION = \"([^\"]+)\"")
        set(TMC_PC_VERSION "${CMAKE_MATCH_1}")
        break()
    endif()
endforeach()
if(NOT TMC_PC_VERSION)
    message(FATAL_ERROR "Could not read TMC_PC_VERSION from xmake.lua.")
endif()

set(TMC_IN_TARGET FALSE)
set(TMC_FILE_PATTERNS "")
foreach(line IN LISTS TMC_XMAKE_LINES)
    if(line MATCHES "^target\\(\"tmc_pc\"\\)")
        set(TMC_IN_TARGET TRUE)
    elseif(TMC_IN_TARGET AND line MATCHES "^target_end\\(\\)")
        set(TMC_IN_TARGET FALSE)
    elseif(TMC_IN_TARGET AND line MATCHES "^[ \t]+add_files\\(\"([^\"]+)\"")
        list(APPEND TMC_FILE_PATTERNS "${CMAKE_MATCH_1}")
    endif()
endforeach()

list(LENGTH TMC_FILE_PATTERNS TMC_PATTERN_COUNT)
if(TMC_PATTERN_COUNT LESS 100)
    message(FATAL_ERROR
        "Only ${TMC_PATTERN_COUNT} add_files() entries were parsed out of the "
        "tmc_pc target in xmake.lua; the desktop target has well over a "
        "hundred. The parser above has gone stale against xmake.lua's "
        "formatting -- fix it rather than shipping a half-linked binary.")
endif()

set(TMC_SOURCES "")
foreach(pattern IN LISTS TMC_FILE_PATTERNS)
    if(pattern MATCHES "[*?]")
        file(GLOB matched CONFIGURE_DEPENDS "${TMC_ROOT}/${pattern}")
        if(NOT matched)
            message(FATAL_ERROR "xmake.lua pattern '${pattern}' matched no files.")
        endif()
        list(APPEND TMC_SOURCES ${matched})
    else()
        if(NOT EXISTS "${TMC_ROOT}/${pattern}")
            message(FATAL_ERROR "xmake.lua lists '${pattern}', which does not exist.")
        endif()
        list(APPEND TMC_SOURCES "${TMC_ROOT}/${pattern}")
    endif()
endforeach()
list(REMOVE_DUPLICATES TMC_SOURCES)

# Re-run CMake when the source list itself changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${TMC_ROOT}/xmake.lua")

# ---------------------------------------------------------------------------
# Header-only third-party deps
# ---------------------------------------------------------------------------
# fmt (FMT_HEADER_ONLY) and nlohmann_json are pure C++ headers with no target
# assumptions, so the copies the desktop build already resolved are reused
# rather than vendored a second time. Override either with -D on the CMake
# command line (see android/README.md) if they live somewhere else.
#
# Whatever is found gets mirrored into the build tree and included from there.
# That is not tidiness: CMake refuses to emit -I/usr/include, so a distro
# package found at its normal location would be located here and then silently
# dropped from the compile line, which is exactly how this first failed.

if(NOT TMC_FMT_INCLUDE)
    file(GLOB TMC_FMT_CANDIDATES "$ENV{HOME}/.xmake/packages/f/fmt/*/*/include")
    foreach(candidate IN LISTS TMC_FMT_CANDIDATES)
        if(EXISTS "${candidate}/fmt/format.h")
            set(TMC_FMT_INCLUDE "${candidate}")
            break()
        endif()
    endforeach()
endif()
if(NOT TMC_FMT_INCLUDE AND EXISTS "/usr/include/fmt/format.h")
    set(TMC_FMT_INCLUDE "/usr/include")
endif()
if(NOT TMC_FMT_INCLUDE)
    message(FATAL_ERROR
        "fmt headers not found. Build the desktop target once (which fetches "
        "them into ~/.xmake/packages) or pass -DTMC_FMT_INCLUDE=<dir>.")
endif()

if(NOT TMC_JSON_INCLUDE)
    file(GLOB TMC_JSON_CANDIDATES "$ENV{HOME}/.xmake/packages/n/nlohmann_json/*/*/include")
    foreach(candidate IN LISTS TMC_JSON_CANDIDATES)
        if(EXISTS "${candidate}/nlohmann/json.hpp")
            set(TMC_JSON_INCLUDE "${candidate}")
            break()
        endif()
    endforeach()
endif()
if(NOT TMC_JSON_INCLUDE AND EXISTS "/usr/include/nlohmann/json.hpp")
    set(TMC_JSON_INCLUDE "/usr/include")
endif()
if(NOT TMC_JSON_INCLUDE)
    message(FATAL_ERROR
        "nlohmann/json.hpp not found. Install nlohmann-json3-dev, or pass "
        "-DTMC_JSON_INCLUDE=<dir>.")
endif()

message(STATUS "tmc: fmt headers  ${TMC_FMT_INCLUDE}")
message(STATUS "tmc: json headers ${TMC_JSON_INCLUDE}")

set(TMC_VENDORED_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/vendored-include")
file(COPY "${TMC_FMT_INCLUDE}/fmt" DESTINATION "${TMC_VENDORED_INCLUDE}")
file(COPY "${TMC_JSON_INCLUDE}/nlohmann" DESTINATION "${TMC_VENDORED_INCLUDE}")

# ---------------------------------------------------------------------------
# SDL3
# ---------------------------------------------------------------------------
if(NOT EXISTS "${TMC_ROOT}/libs/SDL/CMakeLists.txt")
    message(FATAL_ERROR
        "libs/SDL is empty. Run: git submodule update --init libs/SDL")
endif()

set(SDL_SHARED ON  CACHE BOOL "" FORCE)   # SDLActivity dlopens libSDL3.so
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${TMC_ROOT}/libs/SDL" "${CMAKE_CURRENT_BINARY_DIR}/SDL")

# ---------------------------------------------------------------------------
# libmain.so
# ---------------------------------------------------------------------------
add_library(main SHARED ${TMC_SOURCES})

set_target_properties(main PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON)

target_include_directories(main PRIVATE
    "${TMC_ROOT}/include"
    "${TMC_ROOT}/libs"
    "${TMC_ROOT}/port"
    "${TMC_ROOT}"
    "${TMC_ROOT}/build/USA"              # assets/map_offsets.h, assets/gfx_offsets.h
    "${TMC_ROOT}/libs/ViruaPPU/include"
    "${TMC_ROOT}/libs/VirtuaAPU/include"
    "${TMC_ROOT}/libs/agbplay_core"
    "${TMC_ROOT}/tools/src/assets_extractor"
    "${TMC_VENDORED_INCLUDE}")

target_compile_definitions(main PRIVATE
    PC_PORT NON_MATCHING USE_HDMA USA ENGLISH REVISION=0
    FMT_HEADER_ONLY=1
    TMC_PC_VERSION="${TMC_PC_VERSION}"
    TMC_PORT_VERSION="${TMC_PC_VERSION}"
    # No on-screen controls in any form: this build takes input from SDL
    # gamepads only. Selects the no-op branch in port_touch_controls.cpp.
    PORT_NO_TOUCH_CONTROLS)

# Viewport size, from gradle.properties (tmc.viewWidth / tmc.viewHeight), which
# always passes both. As on the desktop side, each axis needs all three names --
# the PPU raster, the canvas composite and the engine camera each own one, and a
# define that reaches only some of them renders sheared garbage. The default
# 240x160 is what all three headers fall back to when undefined, so passing it
# explicitly is equivalent to the desktop build's unset case.
if(TMC_VIEW_W)
    target_compile_definitions(main PRIVATE
        MODE1_GBA_WIDTH=${TMC_VIEW_W}
        PORT_VIEW_CONTENT_WIDTH=${TMC_VIEW_W}
        VIEWPORT_WIDTH=${TMC_VIEW_W})
endif()
if(TMC_VIEW_H)
    target_compile_definitions(main PRIVATE
        MODE1_GBA_HEIGHT=${TMC_VIEW_H}
        PORT_VIEW_CONTENT_HEIGHT=${TMC_VIEW_H}
        VIEWPORT_HEIGHT=${TMC_VIEW_H})
endif()

# Flags mirror the tmc_pc target in xmake.lua, including its optimisation
# split: C at -O0, C++ at -O3. That looks like an oversight but it is what the
# desktop build the regression gate passes on actually compiles, and the decomp
# C is the half that carries the load-bearing literals -- so it is the default
# here too. TMC_C_OPTIMIZATION raises it for anyone willing to re-verify;
# nothing in this repo has measured the engine at anything but -O0.
set(TMC_C_OPTIMIZATION "-O0" CACHE STRING "Optimisation level for the decomp C sources")

target_compile_options(main PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Wall -Wextra -Wno-unused-parameter
        -Wno-missing-field-initializers
        -fno-strict-aliasing -fwrapv -fno-strict-overflow
        ${TMC_C_OPTIMIZATION} -g -fvisibility=default>
    $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Wno-unused-parameter
        -fno-strict-aliasing -fwrapv -fno-strict-overflow
        -O3 -g>)

target_link_libraries(main PRIVATE SDL3::SDL3 android log m)
