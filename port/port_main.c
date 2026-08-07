#include "gba/io_reg.h"
#include "main.h"
#include "port_config.h"
#include "port_asset_bootstrap.h"
#include "port_capture.h"
#include "port_viewport.h"
#include "port_audio.h"
#include "port_gba_mem.h"
#include "port_ppu.h"
#include "port_rom.h"
#include "port_runtime_config.h"
#include "port_types.h"
#include "port_update_check.h"
#include "viewport.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>
#ifdef __ANDROID__
/* SDL.h deliberately does not pull in SDL_main.h. On Android there is no
 * process main() to call: SDLActivity loads the game's .so and looks up
 * "SDL_main" through JNI. SDL_main.h renames main() and exports it for us.
 *
 * Android-only on purpose. On Windows this same header defines
 * SDL_MAIN_AVAILABLE, which renames main() there too and pulls in SDL's
 * header-only WinMain shim -- a real change to a build this port already
 * ships. Linux and macOS define neither macro, so they would be unaffected,
 * but there is nothing to gain by including it for them either.
 */
#include <SDL3/SDL_main.h>
#include <android/log.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#endif
#include "port_launcher_bootstrap.h"

/*
 * Region-specific asset offset header is included based on detected ROM.
 * Both are always available; the correct mapDataBase is selected at runtime.
 */
#ifdef EU
#include "port_offset_EU.h"
#else
#include "port_offset_USA.h"
#endif

static bool Port_TryInitVideo(const char* videoDriver, const char* renderDriver, bool headless) {
    if (videoDriver) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, videoDriver);
    } else {
        SDL_ResetHint(SDL_HINT_VIDEO_DRIVER);
    }

    if (renderDriver) {
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, renderDriver);
    } else {
        SDL_ResetHint(SDL_HINT_RENDER_DRIVER);
    }

    if (SDL_Init(SDL_INIT_VIDEO)) {
        const char* currentDriver = SDL_GetCurrentVideoDriver();

        fprintf(stderr, "SDL video driver: %s\n", currentDriver ? currentDriver : "unknown");
        if (headless) {
            fprintf(stderr, "SDL initialized with headless video driver '%s'.\n", videoDriver);
        }
        return true;
    }

    return false;
}

static void Port_LogVideoDiagnostics(void) {
    int driverCount = SDL_GetNumVideoDrivers();

    fprintf(stderr,
            "Video env: DISPLAY='%s' WAYLAND_DISPLAY='%s' XDG_SESSION_TYPE='%s'\n",
            getenv("DISPLAY") ? getenv("DISPLAY") : "",
            getenv("WAYLAND_DISPLAY") ? getenv("WAYLAND_DISPLAY") : "",
            getenv("XDG_SESSION_TYPE") ? getenv("XDG_SESSION_TYPE") : "");

    fprintf(stderr, "SDL compiled video drivers:");
    for (int i = 0; i < driverCount; i++) {
        fprintf(stderr, " %s", SDL_GetVideoDriver(i));
    }
    fprintf(stderr, "\n");
}

static bool Port_InitVideo(void) {
    const char* err = NULL;
    const char* forcedDriver = getenv("SDL_VIDEODRIVER");
    const char* display = getenv("DISPLAY");
    const char* waylandDisplay = getenv("WAYLAND_DISPLAY");

    if (forcedDriver && forcedDriver[0] != '\0') {
        if (Port_TryInitVideo(NULL, NULL, false)) {
            return true;
        }
        err = SDL_GetError();
        SDL_Quit();
    }

    if (waylandDisplay && waylandDisplay[0] != '\0') {
        if (Port_TryInitVideo("wayland", NULL, false)) {
            return true;
        }
        err = SDL_GetError();
        SDL_Quit();
    }

    if (display && display[0] != '\0') {
        if (Port_TryInitVideo("x11", NULL, false)) {
            return true;
        }
        err = SDL_GetError();
        SDL_Quit();
    }

    if (Port_TryInitVideo(NULL, NULL, false)) {
        return true;
    }
    err = SDL_GetError();

    SDL_Quit();
    if (Port_TryInitVideo("dummy", "software", true)) {
        fprintf(stderr, "Initial SDL error: %s\n", err ? err : "unknown error");
        return true;
    }

    Port_LogVideoDiagnostics();
    fprintf(stderr, "SDL video init failed: normal='%s', fallback='%s'\n", err ? err : "unknown error", SDL_GetError());
    return false;
}

static void Port_InitAudio(void) {
    const char* err = NULL;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) && Port_Audio_Init()) {
        return;
    }

    err = SDL_GetError();
    Port_Audio_Shutdown();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) && Port_Audio_Init()) {
        fprintf(stderr, "Audio device unavailable, using SDL dummy audio driver.\n");
        gMain.muteAudio = 1;
        return;
    }

    fprintf(stderr, "Audio disabled: normal='%s', fallback='%s'\n", err ? err : "unknown error", SDL_GetError());
    Port_Audio_Shutdown();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    gMain.muteAudio = 1;
}

/*
 * On Windows mingw, the heap allocator hands out addresses inside the
 * 0x02000000-0x0A000000 range — the same range port_resolve_addr treats
 * as GBA addresses. Heap pointers passed to DmaCopy* (palette/gfx loads
 * from std::vector buffers) get mistranslated to gEwram[] / gVram[] etc,
 * silently reading zeros and stalling the title-screen palette. Reserve
 * the GBA address window before any heap is opened so the OS allocator
 * can't place anything there. Linux glibc keeps malloc above 0x55... so
 * this is a no-op there; the call is Windows-only.
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <stdint.h>
#include <windows.h>

static int s_gba_va_reserve_done;

static uintptr_t Port_AlignDownU(uintptr_t x, DWORD gran) {
    return (x / (uintptr_t)gran) * (uintptr_t)gran;
}

static void Port_ReserveGbaAddressSpace(void);

#if defined(__GNUC__)
__attribute__((constructor(101)))
#endif
static void Port_ReserveGbaAddressSpaceEarly(void) {
    Port_ReserveGbaAddressSpace();
}

static void Port_ReserveGbaAddressSpace(void) {
    const uintptr_t range_lo = 0x02000000u;
    const uintptr_t range_hi = 0x0A000000u;
    const size_t want_bytes = (size_t)(range_hi - range_lo);

    if (s_gba_va_reserve_done) {
        return;
    }

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const DWORD page = si.dwPageSize ? si.dwPageSize : 4096u;

    LPVOID whole = VirtualAlloc((LPVOID)range_lo, (SIZE_T)want_bytes, MEM_RESERVE, PAGE_NOACCESS);
    if (whole != NULL) {
        if (getenv("TMC_VERBOSE_GBA_VA")) {
            fprintf(stderr, "Reserved GBA address window 0x%zx-0x%zx (heap can't land here).\n",
                    (size_t)range_lo, (size_t)range_hi);
        }
        s_gba_va_reserve_done = 1;
        return;
    }

    fprintf(stderr,
            "WARN: Single-block GBA VA reserve failed (err=%lu); filling window by sub-regions.\n",
            (unsigned long)GetLastError());

    size_t reserved = 0;
    uintptr_t q = range_lo;
    while (q < range_hi) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((LPCVOID)q, &mbi, sizeof(mbi)) == 0) {
            fprintf(stderr, "WARN: VirtualQuery stopped at 0x%zx (err=%lu).\n", (size_t)q,
                    (unsigned long)GetLastError());
            break;
        }

        const uintptr_t reg_base = (uintptr_t)mbi.BaseAddress;
        const uintptr_t reg_end = reg_base + (uintptr_t)mbi.RegionSize;

        if (q < reg_base) {
            q = reg_base;
            continue;
        }

        if (mbi.State != MEM_FREE) {
            q = reg_end;
            continue;
        }

        const uintptr_t lo_in = reg_base > range_lo ? reg_base : range_lo;
        const uintptr_t hi_in = reg_end < range_hi ? reg_end : range_hi;
        if (lo_in < hi_in) {
            uintptr_t u = Port_AlignDownU(lo_in, page);
            if (u < lo_in) {
                u += (uintptr_t)page;
            }
            while (u < hi_in) {
                uintptr_t next = u + (uintptr_t)page;
                if (next > hi_in) {
                    next = hi_in;
                }
                if (u >= lo_in) {
                    const SIZE_T sz = (SIZE_T)(next - u);
                    if (sz > 0 && VirtualAlloc((LPVOID)u, sz, MEM_RESERVE, PAGE_NOACCESS) != NULL) {
                        reserved += (size_t)sz;
                    }
                }
                if (next <= u) {
                    break;
                }
                u = next;
            }
        }

        q = reg_end;
    }

    if (reserved + (size_t)page >= want_bytes) {
        if (getenv("TMC_VERBOSE_GBA_VA")) {
            fprintf(stderr,
                    "Reserved GBA address window 0x%zx-0x%zx (%zu / %zu bytes, sub-regions; heap can't land here).\n",
                    (size_t)range_lo, (size_t)range_hi, reserved, want_bytes);
        }
    } else if (reserved > 0u) {
        fprintf(stderr,
                "WARN: Partial GBA VA reserve %zu / %zu bytes; heap may still use gaps — DmaCopy risk remains.\n",
                reserved, want_bytes);
    } else {
        fprintf(stderr,
                "WARN: Could not reserve GBA address window 0x%zx-0x%zx; DmaCopy may misbehave.\n",
                (size_t)range_lo, (size_t)range_hi);
    }

    s_gba_va_reserve_done = 1;
}
#else
static void Port_ReserveGbaAddressSpace(void) { /* not needed on Linux/macOS */ }
#endif

#ifdef __ANDROID__
/* Send stdout and stderr to logcat.
 *
 * Nothing does this for us -- SDL does not touch the process's stdio on
 * Android -- so without it every diagnostic the port prints is written to a
 * file descriptor pointing at nothing. That is most of what this port knows
 * how to say about itself: the [AREA], [sync] and [ASSET] traces, the asset
 * loader's status, the ROM probe, the fatal paths. A device with no way to
 * read them can only be debugged by guessing, which this project has already
 * paid for once (docs/viewport-bug-tracker.md B4).
 *
 *   adb logcat -s tmc:V
 *
 * Line-buffered so a trace appears as it happens rather than when a 4 KB
 * block fills, which matters when the interesting frame is the last one
 * before a hang. */
static void* Port_Android_LogPump(void* arg) {
    const int fd = (int)(intptr_t)arg;
    char line[512];
    size_t used = 0;
    for (;;) {
        char c;
        const ssize_t n = read(fd, &c, 1);
        if (n <= 0) {
            break;
        }
        if (c == '\n' || used == sizeof(line) - 1) {
            line[used] = '\0';
            if (used > 0) {
                __android_log_write(ANDROID_LOG_INFO, "tmc", line);
            }
            used = 0;
            if (c != '\n') {
                line[used++] = c;
            }
        } else {
            line[used++] = c;
        }
    }
    return NULL;
}

static void Port_Android_StartLogRedirect(void) {
    int fds[2];
    if (pipe(fds) != 0) {
        return;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);
    dup2(fds[1], STDOUT_FILENO);
    dup2(fds[1], STDERR_FILENO);
    close(fds[1]);

    pthread_t pump;
    if (pthread_create(&pump, NULL, Port_Android_LogPump, (void*)(intptr_t)fds[0]) != 0) {
        close(fds[0]);
        return;
    }
    pthread_detach(pump);
}

/* The port reaches the filesystem through relative paths throughout
 * (config.json, tmc.sav, assets/, rom_data/, baserom.gba). An Android
 * process starts with cwd "/", which is read-only, so every one of those
 * would miss. Move to the app's private files directory -- where the
 * activity has already staged the ROM and the baked assets tree -- and the
 * whole existing path-probing logic works unchanged.
 *
 * PreferredAssetRoot() in port_asset_bootstrap.cpp also falls back to the
 * cwd on Android, so this single chdir points the asset loader here too. */
static void Port_Android_EnterStorageDir(void) {
    const char* dir = SDL_GetAndroidInternalStoragePath();
    if (!dir || dir[0] == '\0') {
        fprintf(stderr, "ANDROID: internal storage path unavailable: %s\n", SDL_GetError());
        return;
    }
    if (chdir(dir) != 0) {
        fprintf(stderr, "ANDROID: chdir(%s) failed\n", dir);
        return;
    }
    fprintf(stderr, "ANDROID: working directory is %s\n", dir);
}
#endif

int main(int argc, char* argv[]) {

#ifdef __ANDROID__
    /* First, so the chdir's own report is the first thing in the log. */
    Port_Android_StartLogRedirect();
    /* Before Port_Config_Load below, which is the first relative-path
     * open in the run. Safe this early: SDLActivity.onCreate calls
     * nativeSetupJNI (which caches the activity class this reads through)
     * before it starts the thread that calls us. */
    Port_Android_EnterStorageDir();
#endif

    /* --env=NAME=VALUE, applied before anything reads a flag.
     *
     * Every diagnostic in this port is gated on getenv — TMC_CAMTRACE,
     * TMC_REJECT_TRACE, TMC_HDMA_TRACE, TMC_OOB_TRACE, all of them — and most
     * cache the answer in a static on first call. On Android there is no
     * environment to put them in: an app has no argv either, so TmcActivity
     * reads arguments out of args.txt, and arguments were all it could carry.
     * The result was that the one platform where a bug reproduces is the one
     * platform where none of the instruments can be switched on, which is a
     * large part of why B16 cost six rounds of guessing what was different
     * about the device.
     *
     * A pre-pass rather than a case in the main loop below, so the variable is
     * set before any other argument handler can read its own flag; after the
     * Android log redirect above, so the confirmations reach logcat rather
     * than the descriptor pointing at nothing. On desktop it is redundant with
     * the shell; on Android it is the only way in. */
    {
        int argi;
        for (argi = 1; argi < argc; argi++) {
            if (strncmp(argv[argi], "--env=", 6) == 0) {
                const char* kv = argv[argi] + 6;
                const char* eq = strchr(kv, '=');
                if (eq != NULL) {
                    char name[64];
                    size_t n = (size_t)(eq - kv);
                    if (n > 0 && n < sizeof(name)) {
                        memcpy(name, kv, n);
                        name[n] = '\0';
                        setenv(name, eq + 1, 1);
                        fprintf(stderr, "[env] %s=%s\n", name, eq + 1);
                    }
                } else if (kv[0] != '\0') {
                    setenv(kv, "1", 1);
                    fprintf(stderr, "[env] %s=1\n", kv);
                }
            }
        }
    }

    /* Must run before any std::vector / new / malloc that could land in
     * the GBA window. Static initializers in C++ files are constructed
     * before main, so even this is technically not early enough — but
     * the affected allocations (Port_LoadPaletteGroupFromAssets cache)
     * happen later, after EnsureAssetGroupCache(), so reserving here is
     * sufficient in practice. */
    Port_ReserveGbaAddressSpace();

    fprintf(stderr, "Initializing port layer...\n");

    /* Say which binary this is, before anything else can go wrong.
     *
     * A bug report is a log plus a claim about which build produced it, and on
     * Android the second half is guesswork: the APK is not checked in, the
     * staging line says only "already staged for this install", and a device
     * can sit two fixes behind for days. A dungeon-softlock logcat arrived
     * that could not be attributed to a build at all, which is what this is
     * for. Compile time rather than a git hash so it needs no build-system
     * co-operation and cannot fail in a checkout without git; the viewport is
     * here because it is the one build option that changes what the engine
     * does, and "240x160" in a report about the expanded build ends the
     * investigation on line three. */
    fprintf(stderr, "Build: %s %s, viewport %dx%d\n", __DATE__, __TIME__, VIEWPORT_WIDTH,
            VIEWPORT_HEIGHT);

    // Initialize REG_KEYINPUT to all-keys-released (GBA: 1=not pressed)
    *(u16*)(gIoMem + REG_OFFSET_KEYINPUT) = 0x03FF;

    Port_Config_Load("config.json");

    u8 window_scale = Port_Config_WindowScale();
    bool noAudio = false;
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--window_scale=") == 0 || strncmp(argv[i], "--window_scale=", 15) == 0) {
                const char* valueStr = argv[i] + 15;
                int value = atoi(valueStr);
                if (value >= 1 && value <= 10) {
                    window_scale = (uint8_t)value;
                } else {
                    fprintf(stderr, "Invalid window scale '%s'. Must be an integer between 1 and 10.\n", valueStr);
                }
            }
            else if (strcmp(argv[i], "--loose-assets") == 0) {
                Port_LooseAssetsRequested = 1;
            }
            else if (strcmp(argv[i], "--no-audio") == 0) {
                noAudio = true;
            }
            else if (Port_Capture_HandleArg(argv[i])) {
                /* capture/replay tooling arg, consumed */
            }
            /* Already applied by the pre-pass at the top of main(). */
            else if (strncmp(argv[i], "--env=", 6) == 0) {
            }
            else if (strcmp(argv[i], "--help") == 0) {
                fprintf(stderr, "Usage: %s [--window_scale=<value>] [--loose-assets] [--no-audio]\n", argv[0]);
                fprintf(stderr, "  --window_scale=<value>: Set the window scale (1-10, default is 3)\n");
                fprintf(stderr, "  --loose-assets:         Ignore assets/*.pak archives and read loose files instead.\n");
                fprintf(stderr, "  --no-audio:             Skip audio init (workaround for agbplay crash)\n");
                fprintf(stderr, "  --env=NAME=VALUE:       Set an environment variable before anything reads it.\n");
                fprintf(stderr, "                          The only way to enable the TMC_* traces on Android.\n");
                Port_Capture_PrintUsage();
                fprintf(stderr, "  config.json: Set window_scale and bindings defaults\n");
                return 0;
            }
            else {
                fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            }
        }
    }

    // Initialize SDL video first. Audio is optional and handled separately.
    if (!Port_InitVideo()) {
        return 1;
    }

    Port_Config_OpenGamepads();

    /* Pre-window ROM presence check: bail out with a message box BEFORE
     * creating any window so the user gets clear feedback instead of a
     * black-screen launch. SDL_ShowSimpleMessageBox accepts NULL as
     * parent, so this is safe pre-window. */
    const char* romPath = Port_FindBaseRomPath();
    if (romPath == NULL) {
        static const char kMsg[] =
            "Could not find baserom.gba.\n\n"
            "Place baserom.gba next to tmc_pc and try again.\n"
            "Supported names: baserom.gba (USA), baserom_eu.gba (EU),\n"
            "tmc.gba, tmc_eu.gba.";
        fprintf(stderr, "%s\n", kMsg);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Minish Cap PC Port - ROM not found",
                                 kMsg, NULL);
        SDL_Quit();
        return 1;
    }

    /* Use SDL_CreateWindowAndRenderer so SDL picks the renderer
     * driver (opengl/vulkan/...) and creates the window with the
     * matching visual flags atomically. Calling SDL_CreateRenderer
     * AFTER SDL_CreateWindow on Linux/X11 forces SDL to internally
     * destroy and recreate the X11 window to add SDL_WINDOW_OPENGL,
     * which the user sees as "first window opens, goes black,
     * closes, then second window opens." Confirmed by H9 logs:
     * window flags went from 0x220 → 0x222 across the first
     * SDL_CreateRenderer call, with driver=opengl. */
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE;
#ifdef __ANDROID__
    /* Without this the Android status bar stays on top of the game.
     * SDLActivity.onCreate calls setWindowStyle(false), which does not merely
     * decline to go fullscreen -- it sets SYSTEM_UI_FLAG_VISIBLE and adds
     * FLAG_FORCE_NOT_FULLSCREEN, overriding the activity's fullscreen theme.
     * The only thing that undoes it is SDL's own fullscreen path: asking for
     * the flag here makes SDL_CreateWindow call setWindowStyle(true), which
     * applies the immersive-sticky flags. The window size below is ignored on
     * Android either way; the surface is always the display. */
    window_flags |= SDL_WINDOW_FULLSCREEN;
#endif
    SDL_Window* window = NULL;
    SDL_Renderer* prerenderer = NULL;
    if (!SDL_CreateWindowAndRenderer(
            "The Minish Cap",
            PORT_VIEW_WIDTH * window_scale, PORT_VIEW_HEIGHT * window_scale,
            window_flags,
            &window, &prerenderer)) {
        fprintf(stderr, "SDL_CreateWindowAndRenderer Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    (void)prerenderer; /* Owned by the window; retrieved via SDL_GetRenderer(window) later. */

    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);
    SDL_SyncWindow(window);

#ifdef launcher
    if (!Port_RunBootstrapLauncher(window)) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    }
#endif

    /* Paint a "LOADING" splash IMMEDIATELY so the window never
     * shows a blank black rectangle. Without this, ROM load + asset
     * check + update check + PPU init add up to ~1.4 s of unpainted
     * window before the first SDL_RenderPresent, which the user
     * perceives as "black screen, then the game relaunches". The
     * SDL_Renderer was already created atomically with the window
     * by SDL_CreateWindowAndRenderer above, so this call just
     * fetches it via SDL_GetRenderer(window) and presents on it. */
    Port_PaintBootSplash(window, "LOADING");

    /* Load the ROM before showing the progress bar so the extractor
     * can reuse the in-memory buffer (skip a second 16 MB read) AND
     * so we can validate the region BEFORE extracting. Previously the
     * order was reversed and a wrong-region ROM would happily extract
     * 3-4 seconds of bad assets before we noticed. Use the path the
     * pre-window probe just resolved so we don't re-walk candidates. */
    Port_LoadRom(romPath);
    Port_EnsureAssetsReadyWithDisplay(window, gRomData, gRomSize);
    Port_CheckForUpdates(window);

    // Verify ROM region matches compiled region
#ifdef EU
    if (gRomRegion != ROM_REGION_EU) {
        fprintf(stderr,
                "WARNING: This binary was compiled for EU but the ROM is %s.\n"
                "         Asset offsets may be incorrect. Rebuild with the correct --game_version.\n",
                gRomRegion == ROM_REGION_USA ? "USA" : "UNKNOWN");
    }
#else
    if (gRomRegion != ROM_REGION_USA) {
        fprintf(stderr,
                "WARNING: This binary was compiled for USA but the ROM is %s.\n"
                "         Asset offsets may be incorrect. Rebuild with: xmake f --game_version=EU\n",
                gRomRegion == ROM_REGION_EU ? "EU" : "UNKNOWN");
    }
#endif

    // Initialize PPU renderer
    Port_PPU_Init(window);

    /* Bridge frame: between the progress bar reaching 100% and
     * AgbMain producing its first GBA frame, audio init and AgbMain
     * warmup take long enough to leave the window blank. Paint a
     * single "Starting..." card on the same renderer so the user
     * sees one continuous experience instead of an extractor screen
     * followed by a blank window followed by the title screen. */
    /* Repaint the same "LOADING" card on each transition so the
     * user sees one continuous splash from window-open to first
     * GBA frame instead of multiple flickering states. */
    Port_PaintBootSplash(window, "LOADING");
    fprintf(stderr, "PPU init complete.\n");
    if (noAudio) {
        gMain.muteAudio = 1;
        fprintf(stderr, "Audio disabled by --no-audio flag.\n");
    } else {
        Port_InitAudio();
        Port_PaintBootSplash(window, "LOADING");
        fprintf(stderr, "Audio init complete.\n");
    }

    fprintf(stderr, "Port layer initialized. Entering AgbMain...\n");

    AgbMain();

    Port_Audio_Shutdown();
    Port_PPU_Shutdown();
    Port_Config_CloseGamepads();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
