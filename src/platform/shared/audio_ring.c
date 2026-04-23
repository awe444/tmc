/**
 * @file audio_ring.c
 * @brief Implementation of the SPSC PCM ring buffer declared in
 *        audio_ring.h. See that header for the contract.
 *
 * The ring uses `_Atomic size_t` indices in acquire/release ordering so
 * the producer (game / m4a tick) and the consumer (SDL audio callback,
 * running on its own thread) can communicate without taking a lock.
 * The overflow/underflow diagnostic counters are also `_Atomic` (with
 * relaxed ordering) so the public `*Count()` accessors can be sampled
 * concurrently without UB.
 *
 * C11 `<stdatomic.h>` is *required* — there is no `volatile`-only
 * fallback because plain `volatile` is not sufficient for cross-thread
 * synchronisation in standard C and would leave the producer/consumer
 * data exchange a data race on weak memory models. The two host
 * platforms supported by this port (Linux, macOS) both ship a C11
 * compiler in their toolchains.
 *
 * Tracking: docs/sdl_port.md, PR #7 part 2.1.
 */
#include "audio_ring.h"

#include <string.h>

#if defined(__STDC_NO_ATOMICS__) || (__STDC_VERSION__ < 201112L)
#error "audio_ring.c requires a C11 compiler with <stdatomic.h> support"
#endif

#include <stdatomic.h>

#define PORT_AR_LOAD_ACQ(p) atomic_load_explicit((p), memory_order_acquire)
#define PORT_AR_LOAD_RLX(p) atomic_load_explicit((p), memory_order_relaxed)
#define PORT_AR_STORE_REL(p, v) atomic_store_explicit((p), (v), memory_order_release)
#define PORT_AR_STORE_RLX(p, v) atomic_store_explicit((p), (v), memory_order_relaxed)

/* Compile-time check that the capacity is a power of two so the
 * modulo at the wrap point can be a single mask. */
#if (PORT_AUDIO_RING_CAPACITY & (PORT_AUDIO_RING_CAPACITY - 1u)) != 0u
#error "PORT_AUDIO_RING_CAPACITY must be a power of two"
#endif
#define PORT_AR_MASK (PORT_AUDIO_RING_CAPACITY - 1u)

static int16_t s_buffer[PORT_AUDIO_RING_CAPACITY];

/* Indices in samples, monotonically increasing modulo SIZE_MAX. The
 * "available read" count is `write - read`; the "available write"
 * count is `CAPACITY - (write - read)`. Wraparound of either index
 * is fine because both move by the same amount. */
static _Atomic size_t s_read = 0;
static _Atomic size_t s_write = 0;

/* Diagnostic counters. Updated from the producer (`s_overflow_samples`)
 * and the consumer (`s_underflow_samples`); read by either thread via
 * the public `*Count()` accessors. Relaxed ordering is fine — the
 * counters carry no happens-before relationship with the ring data. */
static _Atomic uint_fast64_t s_overflow_samples = 0;
static _Atomic uint_fast64_t s_underflow_samples = 0;

void Port_AudioRingReset(void) {
    PORT_AR_STORE_RLX(&s_read, 0);
    PORT_AR_STORE_RLX(&s_write, 0);
    atomic_store_explicit(&s_overflow_samples, 0, memory_order_relaxed);
    atomic_store_explicit(&s_underflow_samples, 0, memory_order_relaxed);
    /* Clear the buffer so any latent reads see deterministic silence. */
    memset(s_buffer, 0, sizeof s_buffer);
}

size_t Port_AudioRingAvailableRead(void) {
    size_t w = PORT_AR_LOAD_ACQ(&s_write);
    size_t r = PORT_AR_LOAD_RLX(&s_read);
    return w - r;
}

size_t Port_AudioRingAvailableWrite(void) {
    return PORT_AUDIO_RING_CAPACITY - Port_AudioRingAvailableRead();
}

size_t Port_AudioRingPush(const int16_t* samples, size_t count) {
    if (samples == NULL || count == 0) {
        return 0;
    }
    size_t r = PORT_AR_LOAD_ACQ(&s_read);
    size_t w = PORT_AR_LOAD_RLX(&s_write);
    size_t free_slots = PORT_AUDIO_RING_CAPACITY - (w - r);
    size_t to_write = count <= free_slots ? count : free_slots;
    if (to_write < count) {
        atomic_fetch_add_explicit(&s_overflow_samples, (uint_fast64_t)(count - to_write), memory_order_relaxed);
    }
    /* Two-segment copy so we wrap cleanly at the buffer end. */
    size_t head = w & PORT_AR_MASK;
    size_t first = PORT_AUDIO_RING_CAPACITY - head;
    if (first > to_write) {
        first = to_write;
    }
    memcpy(&s_buffer[head], samples, first * sizeof(int16_t));
    if (to_write > first) {
        memcpy(&s_buffer[0], samples + first, (to_write - first) * sizeof(int16_t));
    }
    PORT_AR_STORE_REL(&s_write, w + to_write);
    return to_write;
}

size_t Port_AudioRingPull(int16_t* out, size_t count) {
    if (out == NULL || count == 0) {
        return 0;
    }
    size_t w = PORT_AR_LOAD_ACQ(&s_write);
    size_t r = PORT_AR_LOAD_RLX(&s_read);
    size_t avail = w - r;
    size_t to_read = count <= avail ? count : avail;
    size_t head = r & PORT_AR_MASK;
    size_t first = PORT_AUDIO_RING_CAPACITY - head;
    if (first > to_read) {
        first = to_read;
    }
    memcpy(out, &s_buffer[head], first * sizeof(int16_t));
    if (to_read > first) {
        memcpy(out + first, &s_buffer[0], (to_read - first) * sizeof(int16_t));
    }
    PORT_AR_STORE_REL(&s_read, r + to_read);

    if (to_read < count) {
        size_t shortfall = count - to_read;
        memset(out + to_read, 0, shortfall * sizeof(int16_t));
        atomic_fetch_add_explicit(&s_underflow_samples, (uint_fast64_t)shortfall, memory_order_relaxed);
    }
    return to_read;
}

uint64_t Port_AudioRingOverflowCount(void) {
    return (uint64_t)atomic_load_explicit(&s_overflow_samples, memory_order_relaxed);
}

uint64_t Port_AudioRingUnderflowCount(void) {
    return (uint64_t)atomic_load_explicit(&s_underflow_samples, memory_order_relaxed);
}

/* ------------------------------------------------------------------ */
/* Headless self-check (PR #7 part 2.1).                              */
/*                                                                    */
/* Exercises every branch of the push / pull / overflow / underflow / */
/* wrap paths without touching SDL.                                   */
/*                                                                    */
/* IMPORTANT: this routine temporarily mutates the shared ring state  */
/* (buffer + indices + counters) and then restores it. It is therefore */
/* only safe to call while the audio path is quiescent — i.e. before  */
/* `Port_AudioInit()`, after `Port_AudioShutdown()`, or while         */
/* playback has been paused via `SDL_PauseAudioDevice` *and* no        */
/* producer thread is running. The smoke-test path in                  */
/* `src/platform/sdl/main.c` invokes it before `Port_AudioInit()` so  */
/* the contract holds there. Counterpart of                           */
/* `Port_RendererSelfCheck()` for the audio plumbing.                 */
/* ------------------------------------------------------------------ */

#include "platform/port.h"

int Port_AudioSelfCheck(void) {
    /* Snapshot the live state so we can restore it on exit. The
     * single-threaded "quiescent" precondition (see comment above)
     * means these reads are race-free. */
    int16_t saved_buffer[PORT_AUDIO_RING_CAPACITY];
    memcpy(saved_buffer, s_buffer, sizeof saved_buffer);
    size_t saved_read = PORT_AR_LOAD_RLX(&s_read);
    size_t saved_write = PORT_AR_LOAD_RLX(&s_write);
    uint_fast64_t saved_overflow = atomic_load_explicit(&s_overflow_samples, memory_order_relaxed);
    uint_fast64_t saved_underflow = atomic_load_explicit(&s_underflow_samples, memory_order_relaxed);

    int rc = 0;

    /* Reset to a known-empty state. */
    Port_AudioRingReset();
    if (Port_AudioRingAvailableRead() != 0) {
        rc = -1;
        goto restore;
    }
    if (Port_AudioRingAvailableWrite() != PORT_AUDIO_RING_CAPACITY) {
        rc = -1;
        goto restore;
    }

    /* Push / pull a small payload, verify byte-for-byte. */
    static const int16_t payload[8] = { 1, -2, 3, -4, 5, -6, 7, -8 };
    size_t pushed = Port_AudioRingPush(payload, 8);
    if (pushed != 8) {
        rc = -1;
        goto restore;
    }
    if (Port_AudioRingAvailableRead() != 8) {
        rc = -1;
        goto restore;
    }
    int16_t out[16];
    memset(out, 0x55, sizeof out);
    size_t pulled = Port_AudioRingPull(out, 16);
    if (pulled != 8) {
        /* underflow path: 8 samples available, 16 requested. */
        rc = -1;
        goto restore;
    }
    for (int i = 0; i < 8; i++) {
        if (out[i] != payload[i]) {
            rc = -1;
            goto restore;
        }
    }
    /* The shortfall (samples 8..15) must have been zero-filled. */
    for (int i = 8; i < 16; i++) {
        if (out[i] != 0) {
            rc = -1;
            goto restore;
        }
    }
    if (Port_AudioRingUnderflowCount() != 8) {
        rc = -1;
        goto restore;
    }

    /* Wrap test: push beyond the buffer end and verify a contiguous
     * pull produces the expected pattern. We need a payload whose
     * push/pull straddles the wrap point. Pick a head position
     * (CAPACITY - 4) and push 16 samples. */
    Port_AudioRingReset();
    /* Advance the cursors to put the head 4 from the wrap. We do this
     * by pushing then pulling a (CAPACITY - 4)-sample dummy. */
    static int16_t dummy[256];
    size_t advance = (size_t)PORT_AUDIO_RING_CAPACITY - 4u;
    /* Push in chunks. */
    while (Port_AudioRingAvailableRead() < advance) {
        size_t want = advance - Port_AudioRingAvailableRead();
        if (want > sizeof(dummy) / sizeof(dummy[0])) {
            want = sizeof(dummy) / sizeof(dummy[0]);
        }
        if (Port_AudioRingPush(dummy, want) != want) {
            rc = -1;
            goto restore;
        }
    }
    /* Pull it all back to advance the read cursor too — leaving both
     * indices at advance, so the next push starts at offset
     * (advance & MASK) = CAPACITY - 4, four below the wrap. */
    while (Port_AudioRingAvailableRead() > 0) {
        size_t got = Port_AudioRingPull(dummy, sizeof(dummy) / sizeof(dummy[0]));
        if (got == 0) {
            rc = -1;
            goto restore;
        }
    }
    /* Now push 16 samples; the first 4 land before the wrap, the next
     * 12 land at the start of the buffer. Pull them back contiguously
     * and verify. */
    static const int16_t wrap_payload[16] = {
        100, 101, 102, 103, 104, 105, 106, 107, -100, -101, -102, -103, -104, -105, -106, -107,
    };
    if (Port_AudioRingPush(wrap_payload, 16) != 16) {
        rc = -1;
        goto restore;
    }
    int16_t wrap_out[16];
    if (Port_AudioRingPull(wrap_out, 16) != 16) {
        rc = -1;
        goto restore;
    }
    for (int i = 0; i < 16; i++) {
        if (wrap_out[i] != wrap_payload[i]) {
            rc = -1;
            goto restore;
        }
    }

    /* Overflow test: push more than the capacity and verify the tail
     * is dropped + the overflow counter advances. */
    Port_AudioRingReset();
    /* Build a small repeating pattern source (we only check the
     * counters and the available-read total, not byte-for-byte). */
    size_t big = (size_t)PORT_AUDIO_RING_CAPACITY + 17u;
    /* Push in chunks of `dummy`. */
    size_t total_pushed = 0;
    size_t total_attempted = 0;
    while (total_attempted < big) {
        size_t want = big - total_attempted;
        if (want > sizeof(dummy) / sizeof(dummy[0])) {
            want = sizeof(dummy) / sizeof(dummy[0]);
        }
        size_t got = Port_AudioRingPush(dummy, want);
        total_pushed += got;
        total_attempted += want;
    }
    if (total_pushed != PORT_AUDIO_RING_CAPACITY) {
        rc = -1;
        goto restore;
    }
    if (Port_AudioRingOverflowCount() != (uint64_t)(big - PORT_AUDIO_RING_CAPACITY)) {
        rc = -1;
        goto restore;
    }
    if (Port_AudioRingAvailableWrite() != 0) {
        rc = -1;
        goto restore;
    }
    if (Port_AudioRingAvailableRead() != PORT_AUDIO_RING_CAPACITY) {
        rc = -1;
        goto restore;
    }

    /* NULL / zero-count guards must be no-ops, never crash. */
    if (Port_AudioRingPush(NULL, 8) != 0) {
        rc = -1;
        goto restore;
    }
    if (Port_AudioRingPush(payload, 0) != 0) {
        rc = -1;
        goto restore;
    }
    if (Port_AudioRingPull(NULL, 8) != 0) {
        rc = -1;
        goto restore;
    }
    if (Port_AudioRingPull(out, 0) != 0) {
        rc = -1;
        goto restore;
    }

restore:
    /* Restore the live ring state. Quiescent precondition (see the
     * function's docstring) means these stores are race-free. */
    memcpy(s_buffer, saved_buffer, sizeof saved_buffer);
    PORT_AR_STORE_RLX(&s_read, saved_read);
    PORT_AR_STORE_RLX(&s_write, saved_write);
    atomic_store_explicit(&s_overflow_samples, saved_overflow, memory_order_relaxed);
    atomic_store_explicit(&s_underflow_samples, saved_underflow, memory_order_relaxed);
    return rc;
}
