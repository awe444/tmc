/**
 * @file audio.c
 * @brief SDL audio device open/close. Currently feeds silence into the
 *        output stream; PR #9 of the roadmap will hook the GBA m4a mixer
 *        in here.
 */
#include "platform/port.h"

#include <SDL.h>

#include <stdio.h>

static SDL_AudioDeviceID s_audio_dev = 0;

static void silent_audio_callback(void* userdata, Uint8* stream, int len)
{
    (void)userdata;
    SDL_memset(stream, 0, (size_t)len);
}

int Port_AudioInit(void)
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "[tmc_sdl] SDL_INIT_AUDIO failed: %s\n", SDL_GetError());
        return -1;
    }

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq     = 13379;       /* GBA m4a default sample rate. */
    want.format   = AUDIO_S16SYS;
    want.channels = 2;
    want.samples  = 1024;
    want.callback = silent_audio_callback;

    SDL_AudioSpec have;
    s_audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (s_audio_dev == 0) {
        fprintf(stderr, "[tmc_sdl] SDL_OpenAudioDevice failed: %s\n",
                SDL_GetError());
        return -1;
    }
    SDL_PauseAudioDevice(s_audio_dev, 0);
    return 0;
}

void Port_AudioShutdown(void)
{
    if (s_audio_dev != 0) {
        SDL_CloseAudioDevice(s_audio_dev);
        s_audio_dev = 0;
    }
}
