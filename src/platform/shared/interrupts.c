/**
 * @file interrupts.c
 * @brief Host-side replacement for the GBA VBlank-driven frame pacing.
 *
 * The real game code (src/interrupts.c) sets up DISPSTAT, registers an IRQ
 * vector, and parks the CPU in `VBlankIntrWait()` until the GBA hardware
 * fires VBlank ~59.7274 times per second. On the host we emulate that by
 * sleeping until the next 1/59.7274 s boundary and then ticking
 * Port_OnVBlank(). PR #2 will redirect the game's `VBlankIntrWait` macro
 * here.
 *
 * Supported host platforms: Linux and macOS only (POSIX
 * `clock_gettime` / `nanosleep`). Microsoft Windows / MSVC builds are
 * not supported -- see docs/sdl_port.md.
 */
#include "platform/port.h"

#include <setjmp.h>
#include <stdint.h>
#include <time.h>

static volatile int s_quit_requested = 0;
static volatile int s_frame_budget = 0;
static volatile uint32_t s_frame_count = 0;
static uint64_t s_last_vblank_ns = 0;

/* Non-local jump checkpoint installed by Port_RunGameLoop(). The real
 * `src/main.c::AgbMain` never returns under TMC_LINK_GAME_SOURCES=ON,
 * so when the host pacer detects shutdown it longjmps back here to
 * unwind the entry call without modifying any game source. */
static jmp_buf s_game_loop_jmp;
static int s_game_loop_active = 0;

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void sleep_ns(uint64_t ns) {
    if (ns == 0) {
        return;
    }
    struct timespec ts;
    ts.tv_sec = (time_t)(ns / 1000000000ULL);
    ts.tv_nsec = (long)(ns % 1000000000ULL);
    while (nanosleep(&ts, &ts) == -1) {
        /* loop until we have actually waited the full duration */
    }
}

void Port_VBlankIntrWait(void) {
    if (s_quit_requested) {
        if (s_game_loop_active) {
            /* Bail out of the game's infinite loop. See Port_RunGameLoop. */
            longjmp(s_game_loop_jmp, 1);
        }
        return;
    }

    uint64_t now = now_ns();
    uint64_t target = s_last_vblank_ns + (uint64_t)PORT_FRAME_NS;

    if (s_last_vblank_ns == 0 || now > target + (uint64_t)PORT_FRAME_NS * 4) {
        /* First call, or we have fallen so far behind that we should just
         * resync rather than try to "catch up" frames. */
        s_last_vblank_ns = now;
        target = now + (uint64_t)PORT_FRAME_NS;
    }

    if (now < target) {
        sleep_ns(target - now);
    }
    s_last_vblank_ns = target;

    Port_OnVBlank();
}

void Port_OnVBlank(void) {
    /* Hook for future mid-frame raster effects / scanline emulation. */
    s_frame_count++;
    if (s_frame_budget > 0) {
        if (--s_frame_budget == 0) {
            s_quit_requested = 1;
        }
    }
}

void Port_SetFrameBudget(int frames) {
    s_frame_budget = (frames > 0) ? frames : 0;
}

uint32_t Port_GetFrameCount(void) {
    return s_frame_count;
}

int Port_ShouldQuit(void) {
    return s_quit_requested;
}

void Port_RequestQuit(void) {
    s_quit_requested = 1;
}

void Port_RunGameLoop(void (*entry)(void)) {
    if (entry == NULL) {
        return;
    }
    if (setjmp(s_game_loop_jmp) == 0) {
        s_game_loop_active = 1;
        entry();
        /* Reachable only if `entry` returned on its own (i.e. not the
         * real AgbMain, which loops forever). */
        s_game_loop_active = 0;
        return;
    }
    /* Longjmp from Port_VBlankIntrWait. Game loop has been forcibly
     * unwound; clear the active flag so a stray late call returns
     * normally instead of jumping into a stale frame. */
    s_game_loop_active = 0;
}

/* Host has no hardware IRQ vector dispatch path. Keep this symbol defined
 * so INTR_VECTOR assignments from game code remain harmless if invoked. */
void ram_IntrMain(void) {
}

/* ---------- Unprefixed alias for VBlankIntrWait ------------------------ */
/*
 * Sub-step 2b.4: the real `src/main.c` and `src/interrupts.c` call
 * `VBlankIntrWait()` directly (declared in include/gba/syscall.h, normally
 * resolved by asm/lib/libagbsyscall.s on the GBA). Forward to the host
 * pacer so the game loop blocks on the same ~59.7274 Hz boundary. Only
 * defined under `__PORT__` so the matching ROM build is unaffected.
 *
 * On the GBA the IRQ controller invokes `src/interrupts.c::VBlankIntr`
 * once per frame; that handler is what flips `gMain.interruptFlag` and
 * lets the main loop in `src/main.c` make forward progress. On the host
 * there is no IRQ, so under TMC_LINK_GAME_SOURCES=ON we run that handler
 * synchronously immediately after the pacer returns. To keep the host
 * build portable across compilers, only provide the no-op placeholder
 * when the real game sources are not being linked; otherwise just
 * forward-declare the symbol and let `src/interrupts.c::VBlankIntr`
 * satisfy the reference.
 *
 * After `VBlankIntr()` has pushed the latest `gScreen.lcd.displayControl`
 * etc. into `REG_DISPCNT` etc. (via `DispCtrlSet`), we own the moment
 * where the emulated GBA memory regions are bit-for-bit equivalent to
 * what the hardware would scan out next. That is exactly when to (a)
 * upload the rasterized framebuffer to the SDL window via
 * `Port_VideoPresent()`, and (b) drain the SDL event queue and refresh
 * `REG_KEYINPUT` via `Port_InputPump()`. Doing both here means the real
 * `AgbMain` path (TMC_LINK_GAME_SOURCES=ON) gets exactly the same
 * per-frame present + input cadence as the placeholder
 * `agb_main_stub.c` loop (=OFF) -- without that, the SDL window stays
 * black for the entire run and the close button is unresponsive,
 * even though `Port_RenderFrame()` produces correct pixels (which is
 * still observable via `--frames=N --print-frame-hash` /
 * `--screenshot=`, both of which call the rasterizer directly).
 */
#ifdef __PORT__
#if !defined(TMC_LINK_GAME_SOURCES) || !(TMC_LINK_GAME_SOURCES)
void VBlankIntr(void) {
    /* Placeholder used when TMC_LINK_GAME_SOURCES=OFF. */
}
#else
void VBlankIntr(void);
#endif

void VBlankIntrWait(void) {
    Port_VBlankIntrWait();
    VBlankIntr();
    /* Per-frame host hooks. Order matters: input first so the keys the
     * player just released/pressed are visible to whatever the caller
     * does after we return; present last so the SDL window reflects
     * the post-VBlankIntr register state we just pushed.
     *
     * The TMC_LINK_GAME_SOURCES=OFF placeholder loop in
     * `src/platform/shared/agb_main_stub.c` calls these itself and
     * does not route through this shim (it calls the prefixed
     * `Port_VBlankIntrWait()` directly), so there is no double-pump
     * in =OFF mode. */
    Port_InputPump();
    Port_VideoPresent();
}
#endif
