/**
 * @file audio_ring.c
 * @brief Implementation of the SPSC PCM ring buffer declared in
 *        audio_ring.h. See that header for the contract.
 *
 * The ring uses `_Atomic size_t` indices in acquire/release ordering so
 * the producer (game / m4a tick) and the consumer (SDL audio callback,
 * running on its own thread) can communicate without taking a lock.
 * On hosts where C11 atomics are unavailable for some reason the file
 * falls back to plain `volatile` indices, which is still safe under the
 * SPSC discipline because only the producer writes the write index and
 * only the consumer writes the read index.
 *
 * Tracking: docs/sdl_port.md, PR #7 part 2.1.
 */
#include "audio_ring.h"

#include <string.h>

#if !defined(__STDC_NO_ATOMICS__) && (__STDC_VERSION__ >= 201112L)
#include <stdatomic.h>
#define PORT_AR_ATOMIC_INDEX _Atomic size_t
#define PORT_AR_LOAD_ACQ(p) atomic_load_explicit((p), memory_order_acquire)
#define PORT_AR_LOAD_RLX(p) atomic_load_explicit((p), memory_order_relaxed)
#define PORT_AR_STORE_REL(p, v) atomic_store_explicit((p), (v), memory_order_release)
#define PORT_AR_STORE_RLX(p, v) atomic_store_explicit((p), (v), memory_order_relaxed)
#else
/* SPSC fallback: a single producer writes only `s_write`, a single
 * consumer writes only `s_read`. Tagging both as `volatile` is enough
 * to prevent the compiler from caching them across the loop. */
#define PORT_AR_ATOMIC_INDEX volatile size_t
#define PORT_AR_LOAD_ACQ(p) (*(p))
#define PORT_AR_LOAD_RLX(p) (*(p))
#define PORT_AR_STORE_REL(p, v) (*(p) = (v))
#define PORT_AR_STORE_RLX(p, v) (*(p) = (v))
#endif

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
static PORT_AR_ATOMIC_INDEX s_read = 0;
static PORT_AR_ATOMIC_INDEX s_write = 0;

static uint64_t s_overflow_samples = 0;
static uint64_t s_underflow_samples = 0;

void Port_AudioRingReset(void) {
    PORT_AR_STORE_RLX(&s_read, 0);
    PORT_AR_STORE_RLX(&s_write, 0);
    s_overflow_samples = 0;
    s_underflow_samples = 0;
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
        s_overflow_samples += (uint64_t)(count - to_write);
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
        s_underflow_samples += (uint64_t)shortfall;
    }
    return to_read;
}

uint64_t Port_AudioRingOverflowCount(void) {
    return s_overflow_samples;
}

uint64_t Port_AudioRingUnderflowCount(void) {
    return s_underflow_samples;
}

/* ------------------------------------------------------------------ */
/* Headless self-check (PR #7 part 2.1).                              */
/*                                                                    */
/* Exercises every branch of the push / pull / overflow / underflow / */
/* wrap paths without touching SDL. Saves and restores the ring state */
/* so it is safe to call from the smoke-test path even after the      */
/* audio device has been opened. Counterpart of                       */
/* `Port_RendererSelfCheck()` for the audio plumbing.                 */
/* ------------------------------------------------------------------ */

#include "platform/port.h"

#define PORT_AR_SELFCHECK_REPORT(msg) \
    do {                              \
        return -1;                    \
    } while (0)

int Port_AudioSelfCheck(void) {
    /* Snapshot the live state so we can restore it on exit. */
    int16_t saved_buffer[PORT_AUDIO_RING_CAPACITY];
    memcpy(saved_buffer, s_buffer, sizeof saved_buffer);
    size_t saved_read = PORT_AR_LOAD_RLX(&s_read);
    size_t saved_write = PORT_AR_LOAD_RLX(&s_write);
    uint64_t saved_overflow = s_overflow_samples;
    uint64_t saved_underflow = s_underflow_samples;

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
    /* Restore the live ring state so any caller that had data queued
     * before the self-check doesn't lose it (currently nobody does,
     * but the contract is "safe to call any time"). */
    memcpy(s_buffer, saved_buffer, sizeof saved_buffer);
    PORT_AR_STORE_RLX(&s_read, saved_read);
    PORT_AR_STORE_RLX(&s_write, saved_write);
    s_overflow_samples = saved_overflow;
    s_underflow_samples = saved_underflow;
    return rc;
}
