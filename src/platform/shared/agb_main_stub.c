/**
 * @file agb_main_stub.c
 * @brief Placeholder definition of `AgbMain`.
 *
 * The real `AgbMain` lives in src/main.c and is the entry point of the
 * decompiled game. PR #2 of the SDL-port roadmap will add the existing
 * src C sources to the SDL CMake target, at which point this stub
 * will be excluded and the real `AgbMain` will take over.
 *
 * Until then we provide an empty frame loop so the SDL platform layer
 * can be built and exercised on its own (a blank window paced at the
 * GBA's 59.7274 Hz with working keyboard + gamepad input).
 */
#include "platform/port.h"

#include <stdio.h>

void AgbMain(void) {
    fprintf(stderr, "[tmc_sdl] AgbMain stub running. The real game source has not "
                    "yet been linked into the SDL build (see docs/sdl_port.md, "
                    "roadmap PR #2). The window will stay blank until then.\n");

    /* Mirror the structure of the real game loop in src/main.c so that
     * future PRs only have to delete this file rather than restructure
     * the platform layer. */
    while (!Port_ShouldQuit()) {
        Port_InputPump();
        Port_VideoPresent();
        Port_VBlankIntrWait();
    }
}
