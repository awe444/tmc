/**
 * @file main.c
 * @brief SDL entry point. Initializes the platform layer, then transfers
 *        control to the game's `AgbMain()` (currently the stub in
 *        src/platform/shared/agb_main_stub.c; PR #2 wires in the real
 *        src/main.c).
 */
#include "platform/port.h"

#include <SDL.h>

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int scale;
    int fullscreen;
    int mute;
    int uncapped_fps;
    int frames; /* >0: exit after N frames (CI smoke test) */
    int print_frame_hash;
    int assert_audio_active;
    const char* save_dir;
    const char* screenshot_path;
} CliOptions;

static void print_usage(const char* argv0) {
    fprintf(stderr,
            "Usage: %s [options]\n"
            "  --scale=N            Integer window scale (default: 4)\n"
            "  --fullscreen         Start in fullscreen-desktop mode\n"
            "  --mute               Disable audio output\n"
            "  --uncapped-fps       Disable host frame pacing sleeps (run as fast as possible)\n"
            "  --save-dir=PATH      Directory containing tmc.sav (default: cwd)\n"
            "  --frames=N           Run for N frames then exit (for CI smoke tests)\n"
            "  --screenshot=PATH    After the final frame, write a PPM (P6) screenshot\n"
            "                       of the framebuffer to PATH (PR #8 of the SDL roadmap)\n"
            "  --print-frame-hash   After the final frame, print an FNV-1a 64-bit hash\n"
            "                       of the framebuffer to stdout in the form\n"
            "                       'frame-hash: 0x<hex>' (PR #8: golden-image CI test)\n"
            "  --press=SPEC         Schedule scripted button presses for the test\n"
            "                       harness. SPEC is a comma-separated list of\n"
            "                       'KEYS@FRAME[+DURATION]' entries, where KEYS is one\n"
            "                       or more of A,B,START,SELECT,UP,DOWN,LEFT,RIGHT,L,R\n"
            "                       joined with '|'. May be repeated. Examples:\n"
            "                       --press=START@60+10\n"
            "                       --press=A@30,B@40,UP|A@120+2\n"
            "  --input-script=PATH  Load scripted-input entries from a text file\n"
            "                       (one --press SPEC per non-comment line)\n"
            "  --assert-audio-active  Fail if host m4a runtime produced no non-zero\n"
            "                         stereo frames (deterministic headless check;\n"
            "                         meaningful with scripted progression)\n"
            "  --help               Show this message\n",
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
    opts->uncapped_fps = 0;
    opts->frames = 0;
    opts->print_frame_hash = 0;
    opts->assert_audio_active = 0;
    opts->save_dir = NULL;
    opts->screenshot_path = NULL;

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
        if (strcmp(a, "--uncapped-fps") == 0) {
            opts->uncapped_fps = 1;
            continue;
        }
        if (strcmp(a, "--print-frame-hash") == 0) {
            opts->print_frame_hash = 1;
            continue;
        }
        if (strcmp(a, "--assert-audio-active") == 0) {
            opts->assert_audio_active = 1;
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
        if (strncmp(a, "--screenshot=", 13) == 0) {
            opts->screenshot_path = a + 13;
            continue;
        }
        if (strncmp(a, "--press=", 8) == 0) {
            if (Port_ScriptedInputParse(a + 8) != 0) {
                fprintf(stderr, "[tmc_sdl] Failed to parse --press=%s\n", a + 8);
                return -1;
            }
            continue;
        }
        if (strncmp(a, "--input-script=", 15) == 0) {
            if (Port_ScriptedInputLoadFile(a + 15) != 0) {
                fprintf(stderr, "[tmc_sdl] Failed to load --input-script=%s\n", a + 15);
                return -1;
            }
            continue;
        }
        fprintf(stderr, "[tmc_sdl] Unknown option: %s\n", a);
        print_usage(argv[0]);
        return -1;
    }
    return 1;
}

/* PR #8 of the SDL-port roadmap: golden-image CI test infrastructure.
 *
 * After the `--frames=N` budget unwinds AgbMain we render one extra
 * frame straight from the emulated GBA memory regions into a local
 * buffer and either hash it, write it as a PPM, or both. Hashing uses
 * FNV-1a 64-bit so a future host-architecture change does not perturb
 * the value as long as the rasterizer output is identical.
 *
 * The hashing path lets the headless CI smoke test detect any
 * regression in the rasterizer that is observable in the produced
 * pixels — the OFF build's empty-backdrop output is fully
 * deterministic, so the CI step pins it to a known constant. The ON
 * build's title-screen frame is also bit-exact across runs (the
 * software pipeline is fully synchronous on the host) but pinning it
 * is deferred until the rest of PR #5..#7 stops churning the
 * still-stubbed code paths it reaches into.
 */
static uint64_t frame_hash_fnv1a(const uint32_t* fb, size_t pixel_count) {
    /* FNV-1a 64-bit. Hash each ARGB8888 pixel in a defined byte order so
     * the result is stable across host endianness. */
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < pixel_count; ++i) {
        uint32_t px = fb[i];
        h ^= (uint64_t)((px >> 24) & 0xFF);
        h *= 0x100000001b3ULL;
        h ^= (uint64_t)((px >> 16) & 0xFF);
        h *= 0x100000001b3ULL;
        h ^= (uint64_t)((px >> 8) & 0xFF);
        h *= 0x100000001b3ULL;
        h ^= (uint64_t)(px & 0xFF);
        h *= 0x100000001b3ULL;
    }
    return h;
}

static int write_ppm_screenshot(const char* path, const uint32_t* fb, int w, int h) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        fprintf(stderr, "[tmc_sdl] Could not open --screenshot=%s for writing: %s\n", path, strerror(errno));
        return -1;
    }
    if (fprintf(f, "P6\n%d %d\n255\n", w, h) < 0) {
        fprintf(stderr, "[tmc_sdl] Could not write PPM header to --screenshot=%s: %s\n", path, strerror(errno));
        fclose(f);
        return -1;
    }
    /* PPM is RGB; framebuffer is ARGB8888. */
    for (int i = 0; i < w * h; ++i) {
        uint32_t px = fb[i];
        unsigned char rgb[3];
        rgb[0] = (unsigned char)((px >> 16) & 0xFF);
        rgb[1] = (unsigned char)((px >> 8) & 0xFF);
        rgb[2] = (unsigned char)(px & 0xFF);
        if (fwrite(rgb, 1, 3, f) != 3) {
            fprintf(stderr, "[tmc_sdl] Could not write pixel data to --screenshot=%s: %s\n", path, strerror(errno));
            fclose(f);
            return -1;
        }
    }
    if (fclose(f) != 0) {
        fprintf(stderr, "[tmc_sdl] Error closing --screenshot=%s: %s\n", path, strerror(errno));
        return -1;
    }
    return 0;
}

static int emit_golden_image_artifacts(const CliOptions* opts) {
    if (!opts->print_frame_hash && opts->screenshot_path == NULL) {
        return 0;
    }

    static uint32_t scratch[PORT_GBA_DISPLAY_WIDTH * PORT_GBA_DISPLAY_HEIGHT];
    Port_RenderFrame(scratch);

    if (opts->screenshot_path != NULL) {
        if (write_ppm_screenshot(opts->screenshot_path, scratch, PORT_GBA_DISPLAY_WIDTH, PORT_GBA_DISPLAY_HEIGHT) !=
            0) {
            return -1;
        }
    }
    if (opts->print_frame_hash) {
        uint64_t h = frame_hash_fnv1a(scratch, PORT_GBA_DISPLAY_WIDTH * PORT_GBA_DISPLAY_HEIGHT);
        /* Stable, parseable form for the CI golden-image check; printed
         * to stdout so CI can capture it without piping stderr. */
        printf("frame-hash: 0x%016" PRIx64 "\n", h);
        fflush(stdout);
    }
    return 0;
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
    /* Validate the scripted-input parser/table used by the test harness
     * (`--press=` / `--input-script=`). Defined in
     * src/platform/shared/scripted_input.c. */
    if (Port_ScriptedInputSelfCheck() != 0) {
        fprintf(stderr, "[tmc_sdl] FATAL: scripted-input self-check failed. "
                        "See src/platform/shared/scripted_input.c::Port_ScriptedInputSelfCheck().\n");
        SDL_Quit();
        return 1;
    }
    /* PR #7 part 2.1: validate the host-side audio ring buffer that the
     * future m4a host mixer will feed into the SDL audio device. The
     * self-check requires the ring to be quiescent (no producer / no
     * audio callback running), so it runs here — before
     * `Port_AudioInit()` opens the SDL audio device. Defined in
     * src/platform/shared/audio_ring.c. */
    if (Port_AudioSelfCheck() != 0) {
        fprintf(stderr, "[tmc_sdl] FATAL: audio ring-buffer self-check failed. "
                        "See src/platform/shared/audio_ring.c::Port_AudioSelfCheck().\n");
        SDL_Quit();
        return 1;
    }

    /* PR #7 part 2.2.1: validate the m4a host shim's ply_* command
     * handlers. Only meaningful when the game library (and therefore
     * src/gba/m4a.c + src/platform/shared/m4a_host.c) is linked in;
     * the =OFF build skips the call. Defined in
     * src/platform/shared/m4a_host.c. */
#if TMC_LINK_GAME_SOURCES
    if (Port_M4ASelfCheck() != 0) {
        fprintf(stderr, "[tmc_sdl] FATAL: m4a host-shim self-check failed. "
                        "See src/platform/shared/m4a_host.c::Port_M4ASelfCheck().\n");
        SDL_Quit();
        return 1;
    }
    /* Reset host m4a runtime diagnostics after the self-check so
     * the post-run assertion sees only live game-loop activity. */
    Port_M4AAudioDiagReset();
#endif

    Port_SetVideoVSync(!opts.uncapped_fps);
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
    Port_SetUncappedFramePacing(opts.uncapped_fps);

    /* Pre-initialise host globals (entity-list sentinels, etc.) that the
     * engine expects to be non-zero before any game code runs. */
    Port_GlobalsInit();

    /* Hand off to the game. Port_ShouldQuit() returning non-zero unwinds
     * the loop in the agb_main_stub directly; the real AgbMain (linked
     * under TMC_LINK_GAME_SOURCES=ON, src/main.c) loops forever, so we
     * route both through Port_RunGameLoop which installs a setjmp
     * checkpoint and longjmps from Port_VBlankIntrWait when shutdown
     * (or the --frames=N budget) hits. */
    Port_RunGameLoop(AgbMain);

    /* PR #8: emit golden-image CI artifacts (hash and/or PPM) from the
     * final emulated framebuffer before tearing down the host state. */
    if (emit_golden_image_artifacts(&opts) != 0) {
        Port_SaveFlush();
#if TMC_ENABLE_AUDIO
        Port_AudioShutdown();
#endif
        Port_InputShutdown();
        Port_VideoShutdown();
        SDL_Quit();
        return 1;
    }

#if TMC_LINK_GAME_SOURCES
    if (opts.assert_audio_active) {
        uint64_t frames_generated = Port_M4AAudioFramesGenerated();
        uint64_t frames_nonzero = Port_M4AAudioFramesNonZero();
        uint64_t mix_ticks = Port_M4AAudioMixTicks();
        uint64_t song_starts = Port_M4AAudioSongStartRequests();
        if (frames_generated == 0 || mix_ticks == 0 || (frames_nonzero == 0 && song_starts == 0)) {
            fprintf(stderr,
                    "[tmc_sdl] --assert-audio-active failed: generated=%" PRIu64
                    " nonzero=%" PRIu64 " ticks=%" PRIu64 " song_starts=%" PRIu64 "\n",
                    frames_generated, frames_nonzero, mix_ticks, song_starts);
            Port_SaveFlush();
#if TMC_ENABLE_AUDIO
            Port_AudioShutdown();
#endif
            Port_InputShutdown();
            Port_VideoShutdown();
            SDL_Quit();
            return 1;
        }
    }
#endif

    Port_SaveFlush();
#if TMC_ENABLE_AUDIO
    Port_AudioShutdown();
#endif
    Port_InputShutdown();
    Port_VideoShutdown();
    SDL_Quit();
    return 0;
}
