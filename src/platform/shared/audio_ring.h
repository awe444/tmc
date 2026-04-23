/**
 * @file audio_ring.h
 * @brief Lock-free single-producer / single-consumer ring buffer for
 *        interleaved S16 stereo PCM samples.
 *
 * This is the plumbing layer between the host-side m4a mixer (the
 * producer, called from the game's per-frame audio tick) and the SDL
 * audio device callback (the consumer, called from the SDL audio
 * thread). The two threads communicate exclusively through a fixed-
 * size lock-free FIFO so the audio callback never blocks on the game
 * thread.
 *
 * The ring is sized for several frames of audio at the SDL sample rate
 * so the producer can stuff a whole frame's worth of samples per
 * VBlank without worrying about the consumer's exact callback cadence.
 *
 * Tracking: docs/sdl_port.md, PR #7 part 2.1.
 */
#ifndef TMC_PLATFORM_SHARED_AUDIO_RING_H
#define TMC_PLATFORM_SHARED_AUDIO_RING_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Capacity of the ring in *interleaved S16 samples* (i.e. one stereo
 * frame counts as 2 samples). 16384 interleaved samples = 8192 stereo
 * frames, which at the GBA m4a default rate of 13379 Hz is ~0.61 s of
 * audio (~36 vblanks at 59.7274 Hz) — comfortably above any
 * per-vblank producer / per-callback consumer drain. Must be a power
 * of two so the modulo can be a mask.
 */
#define PORT_AUDIO_RING_CAPACITY 16384u

/** Reset the ring to empty. Call from a quiescent state only (e.g. at
 *  init time, or while SDL audio is paused). */
void Port_AudioRingReset(void);

/** Number of S16 samples currently available to the consumer. */
size_t Port_AudioRingAvailableRead(void);

/** Number of S16 samples that can be written before overrun. */
size_t Port_AudioRingAvailableWrite(void);

/**
 * Producer: push up to `count` interleaved S16 samples (so for stereo,
 * `count` should be 2 * frame_count). Returns the number actually
 * written; if the ring is too full to take everything, the tail of
 * `samples` is dropped. The drop counter is exposed via
 * `Port_AudioRingOverflowCount()` for diagnostics.
 */
size_t Port_AudioRingPush(const int16_t* samples, size_t count);

/**
 * Consumer: pull up to `count` interleaved S16 samples into `out`. Any
 * shortfall is filled with zero samples (silence). Returns the number
 * actually pulled from the ring (so `count - return` is the underflow
 * count for this call).
 */
size_t Port_AudioRingPull(int16_t* out, size_t count);

/** Total number of samples dropped on overflow since the last reset. */
uint64_t Port_AudioRingOverflowCount(void);

/** Total number of zero samples emitted on underflow since reset. */
uint64_t Port_AudioRingUnderflowCount(void);

#ifdef __cplusplus
}
#endif

#endif /* TMC_PLATFORM_SHARED_AUDIO_RING_H */
