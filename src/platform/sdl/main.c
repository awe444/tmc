/**
 * @file main.c
 * @brief SDL entry point. Initializes the platform layer, then transfers
 *        control to the game's `AgbMain()` (currently the stub in
 *        src/platform/shared/agb_main_stub.c; PR #2 wires in the real
 *        src/main.c).
 */
#include "platform/port.h"

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int scale;
    int fullscreen;
    int mute;
    int frames; /* >0: exit after N frames (CI smoke test) */
    const char* save_dir;
} CliOptions;

static void print_usage(const char* argv0) {
    fprintf(stderr,
            "Usage: %s [options]\n"
            "  --scale=N        Integer window scale (default: 4)\n"
            "  --fullscreen     Start in fullscreen-desktop mode\n"
            "  --mute           Disable audio output\n"
            "  --save-dir=PATH  Directory containing tmc.sav (default: cwd)\n"
            "  --frames=N       Run for N frames then exit (for CI smoke tests)\n"
            "  --help           Show this message\n",
            argv0);
}

static int parse_int_suffix(const char* arg, const char* prefix, int* out) {
    size_t plen = strlen(prefix);
    if (strncmp(arg, prefix, plen) != 0) {
        return 0;
    }
    char* end = NULL;
    long v = strtol(arg + plen, &end, 10);
    if (end == arg + plen || v < 0 || v > 1000000) {
        return 0;
    }
    *out = (int)v;
    return 1;
}

static int parse_cli(int argc, char** argv, CliOptions* opts) {
    opts->scale = 4;
    opts->fullscreen = 0;
    opts->mute = 0;
    opts->frames = 0;
    opts->save_dir = NULL;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(a, "--fullscreen") == 0) {
            opts->fullscreen = 1;
            continue;
        }
        if (strcmp(a, "--mute") == 0) {
            opts->mute = 1;
            continue;
        }
        if (parse_int_suffix(a, "--scale=", &opts->scale))
            continue;
        if (parse_int_suffix(a, "--frames=", &opts->frames))
            continue;
        if (strncmp(a, "--save-dir=", 11) == 0) {
            opts->save_dir = a + 11;
            continue;
        }
        fprintf(stderr, "[tmc_sdl] Unknown option: %s\n", a);
        print_usage(argv[0]);
        return -1;
    }
    return 1;
}

/* When --frames is set, AgbMain's loop has to stop after N frames. We
 * implement that via Port_SetFrameBudget() which the shared interrupts
 * layer decrements on each VBlank. */

int main(int argc, char** argv) {
    CliOptions opts;
    int parsed = parse_cli(argc, argv, &opts);
    if (parsed <= 0) {
        return (parsed == 0) ? 0 : 1;
    }

    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "[tmc_sdl] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    Port_InitMemory();

    /* PR #2a: validate the rewired GBA headers as part of the headless
     * smoke test. Originally invoked from the agb_main_stub; moved here
     * in PR #2b.4 because the real `src/main.c::AgbMain` now takes over.
     * Defined in src/platform/shared/port_headers_check.c. */
    extern int Port_HeadersSelfCheck(void);
    if (Port_HeadersSelfCheck() != 0) {
        fprintf(stderr, "[tmc_sdl] FATAL: GBA header rewiring self-check failed. "
                        "See src/platform/shared/port_headers_check.c.\n");
        SDL_Quit();
        return 1;
    }
    /* PR #4: validate the software rasterizer (BG mode 0 + OBJ layer)
     * against a battery of known tilemap / sprite inputs so any future
     * regression surfaces in the headless smoke test. Defined in
     * src/platform/shared/render.c. */
    if (Port_RendererSelfCheck() != 0) {
        fprintf(stderr, "[tmc_sdl] FATAL: software rasterizer self-check failed. "
                        "See src/platform/shared/render.c::Port_RendererSelfCheck().\n");
        SDL_Quit();
        return 1;
    }

    if (Port_VideoInit(opts.scale, opts.fullscreen) != 0) {
        SDL_Quit();
        return 1;
    }

    if (Port_InputInit() != 0) {
        Port_VideoShutdown();
        SDL_Quit();
        return 1;
    }

#if TMC_ENABLE_AUDIO
    if (!opts.mute) {
        if (Port_AudioInit() != 0) {
            fprintf(stderr, "[tmc_sdl] Audio init failed; continuing without sound.\n");
        }
    }
#endif

    if (Port_SaveLoad(opts.save_dir) != 0) {
        fprintf(stderr, "[tmc_sdl] Warning: could not load save file (continuing).\n");
    }

    Port_SetFrameBudget(opts.frames);

    /* Hand off to the game. Port_ShouldQuit() returning non-zero exits
     * the loop in AgbMain (see agb_main_stub.c, and eventually
     * src/main.c after PR #2). */
    AgbMain();

    Port_SaveFlush();
#if TMC_ENABLE_AUDIO
    Port_AudioShutdown();
#endif
    Port_InputShutdown();
    Port_VideoShutdown();
    SDL_Quit();
    return 0;
}
