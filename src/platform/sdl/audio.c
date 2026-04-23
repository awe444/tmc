/**
 * @file audio.c
 * @brief SDL audio device open/close + the consumer side of the
 *        host-side PCM ring buffer.
 *
 * As of PR #7 part 2.1 the SDL audio callback no longer ignores its
 * `stream` argument — it drains an SPSC ring buffer fed by
 * `Port_AudioPushSamples()`. While the host mixer is still stubbed
 * (PR #7 parts 2.2 / 2.3 will land it) no producer pushes anything
 * yet, so the ring permanently underflows and the callback fills
 * `stream` with zero samples. The audible behaviour is therefore
 * identical to the previous `silent_audio_callback`, but the data
 * path the real mixer will plug into is now in place. See
 * `src/platform/shared/audio_ring.{h,c}` for the ring contract and
 * `docs/sdl_port.md` for the roadmap.
 */
#include "platform/port.h"

#include "../shared/audio_ring.h"

#include <SDL.h>

#include <stdio.h>

static SDL_AudioDeviceID s_audio_dev = 0;
static int s_sample_rate = PORT_AUDIO_DEFAULT_RATE;

/* The SDL audio thread invokes this on its own clock. We pull S16
 * stereo samples out of the SPSC ring; any shortfall is zero-filled
 * by `Port_AudioRingPull` itself, so the SDL stream is always fully
 * written even when the producer is silent.
 *
 * SDL's `len` is in bytes and the stereo S16 frame size is 4 bytes;
 * we round the request down to whole stereo frames and explicitly
 * zero any odd trailing byte(s) so the entire SDL buffer is always
 * deterministically initialised even if the device negotiates a
 * pathological `len` that isn't frame-aligned. */
static void ring_drain_audio_callback(void* userdata, Uint8* stream, int len) {
    (void)userdata;
    if (len <= 0) {
        return;
    }
    /* Ensure determinism for any byte the ring won't touch. */
    SDL_memset(stream, 0, (size_t)len);
    /* Round down to whole stereo frames (4 bytes each). */
    size_t whole_frames_bytes = (size_t)len & ~(size_t)3;
    size_t sample_count = whole_frames_bytes / sizeof(int16_t);
    if (sample_count > 0) {
        Port_AudioRingPull((int16_t*)stream, sample_count);
    }
}

int Port_AudioInit(void) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "[tmc_sdl] SDL_INIT_AUDIO failed: %s\n", SDL_GetError());
        return -1;
    }

    Port_AudioRingReset();

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = PORT_AUDIO_DEFAULT_RATE; /* GBA m4a default sample rate. */
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = ring_drain_audio_callback;

    SDL_AudioSpec have;
    s_audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (s_audio_dev == 0) {
        fprintf(stderr, "[tmc_sdl] SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return -1;
    }
    s_sample_rate = have.freq > 0 ? have.freq : PORT_AUDIO_DEFAULT_RATE;
    SDL_PauseAudioDevice(s_audio_dev, 0);
    return 0;
}

void Port_AudioShutdown(void) {
    if (s_audio_dev != 0) {
        SDL_CloseAudioDevice(s_audio_dev);
        s_audio_dev = 0;
    }
    /* Reset the cached rate so a later --mute / no-init query gets the
     * documented default rather than a stale one. */
    s_sample_rate = PORT_AUDIO_DEFAULT_RATE;
}

int Port_AudioGetSampleRate(void) {
    return s_sample_rate;
}

int Port_AudioPushSamples(const int16_t* samples, int frame_count) {
    if (samples == NULL || frame_count <= 0) {
        return 0;
    }
    /* The ring is interleaved-sample-counted, frames are 2 samples each. */
    size_t sample_count = (size_t)frame_count * 2u;
    size_t pushed = Port_AudioRingPush(samples, sample_count);
    return (int)(pushed / 2u);
}
