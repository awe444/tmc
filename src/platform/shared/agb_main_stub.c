/**
 * @file agb_main_stub.c
 * @brief Placeholder definition of `AgbMain`.
 *
 * The real `AgbMain` lives in src/main.c and is the entry point of the
 * decompiled game. With the default `TMC_LINK_GAME_SOURCES=ON` (PR #2b.4b
 * of the SDL-port roadmap) the real `AgbMain` is what runs; this TU only
 * enters the executable when `TMC_LINK_GAME_SOURCES=OFF` is passed
 * explicitly, in which case it provides an empty frame loop so the SDL
 * platform layer can be built and exercised on its own (a blank window
 * paced at the GBA's 59.7274 Hz with working keyboard + gamepad input).
 * That `=OFF` mode is preserved as an early-bring-up scaffold for future
 * ports (PSP / PS2 / ...) and as a platform-layer isolation harness.
 */
#include "platform/port.h"

#include <stdio.h>
#include <stdlib.h>

/* PR #2a: validate the rewired GBA headers at runtime as part of the
 * headless smoke test. Defined in src/platform/shared/port_headers_check.c. */
int Port_HeadersSelfCheck(void);

void AgbMain(void) {
    fprintf(stderr, "[tmc_sdl] AgbMain stub running (TMC_LINK_GAME_SOURCES=OFF). "
                    "The real game source is not linked into this build, so the "
                    "window will stay blank. See docs/sdl_port.md.\n");

    if (Port_HeadersSelfCheck() != 0) {
        fprintf(stderr, "[tmc_sdl] FATAL: GBA header rewiring self-check failed. "
                        "See src/platform/shared/port_headers_check.c.\n");
        Port_RequestQuit();
        exit(1);
    }

    /* Mirror the structure of the real game loop in src/main.c so that
     * future PRs only have to delete this file rather than restructure
     * the platform layer. */
    while (!Port_ShouldQuit()) {
        Port_InputPump();
        Port_VideoPresent();
        Port_VBlankIntrWait();
    }
}
