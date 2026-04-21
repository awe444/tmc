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
 */
#include "platform/port.h"

#include <stdint.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#endif

static volatile int s_quit_requested = 0;
static volatile int s_frame_budget = 0;
static uint64_t s_last_vblank_ns = 0;

static uint64_t now_ns(void) {
#if defined(_WIN32)
    static LARGE_INTEGER s_freq;
    LARGE_INTEGER counter;
    if (s_freq.QuadPart == 0) {
        QueryPerformanceFrequency(&s_freq);
    }
    QueryPerformanceCounter(&counter);
    /* Convert to nanoseconds without overflowing for typical perf-counter
     * frequencies. */
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / (uint64_t)s_freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

static void sleep_ns(uint64_t ns) {
    if (ns == 0) {
        return;
    }
#if defined(_WIN32)
    /* Sleep() granularity is typically 15.6 ms. For sub-frame waits we
     * fall back to a short busy-loop after a coarse Sleep. */
    DWORD ms = (DWORD)(ns / 1000000ULL);
    if (ms > 1) {
        Sleep(ms - 1);
    }
    uint64_t target = now_ns() + (ns % 1000000ULL);
    while (now_ns() < target) {
        /* spin */
    }
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(ns / 1000000000ULL);
    ts.tv_nsec = (long)(ns % 1000000000ULL);
    while (nanosleep(&ts, &ts) == -1) {
        /* loop until we have actually waited the full duration */
    }
#endif
}

void Port_VBlankIntrWait(void) {
    if (s_quit_requested) {
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
    if (s_frame_budget > 0) {
        if (--s_frame_budget == 0) {
            s_quit_requested = 1;
        }
    }
}

void Port_SetFrameBudget(int frames) {
    s_frame_budget = (frames > 0) ? frames : 0;
}

int Port_ShouldQuit(void) {
    return s_quit_requested;
}

void Port_RequestQuit(void) {
    s_quit_requested = 1;
}
