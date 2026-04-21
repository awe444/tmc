/**
 * @file video.c
 * @brief SDL window + renderer + framebuffer presentation.
 *
 * As of PR #4 of the SDL-port roadmap (see docs/sdl_port.md), this
 * file is purely the SDL "swap chain": every frame it asks the
 * cross-port software rasterizer in `src/platform/shared/render.c`
 * to fill the 240x160 ARGB8888 framebuffer from the emulated
 * gPortVram / gPortOam / gPortPltt / gPortIo arrays, then uploads
 * the result into an SDL streaming texture and presents it. PR #5
 * will extend the rasterizer with affine BGs, windows and blending
 * without touching this TU.
 */
#include "platform/port.h"

#include <SDL.h>

#include <stdio.h>
#include <string.h>

#define GBA_DISPLAY_W PORT_GBA_DISPLAY_WIDTH
#define GBA_DISPLAY_H PORT_GBA_DISPLAY_HEIGHT

static SDL_Window* s_window = NULL;
static SDL_Renderer* s_renderer = NULL;
static SDL_Texture* s_texture = NULL;
static uint32_t s_framebuffer[GBA_DISPLAY_W * GBA_DISPLAY_H];

int Port_VideoInit(int scale, int fullscreen) {
    if (scale < 1)
        scale = 4;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "[tmc_sdl] SDL_INIT_VIDEO failed: %s\n", SDL_GetError());
        return -1;
    }

    Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    s_window = SDL_CreateWindow("The Legend of Zelda: The Minish Cap (SDL port)", SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, GBA_DISPLAY_W * scale, GBA_DISPLAY_H * scale, flags);
    if (s_window == NULL) {
        fprintf(stderr, "[tmc_sdl] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return -1;
    }

    s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (s_renderer == NULL) {
        /* Fall back to software renderer (e.g. SDL_VIDEODRIVER=dummy in CI). */
        s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (s_renderer == NULL) {
        fprintf(stderr, "[tmc_sdl] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return -1;
    }

    /* Preserve the GBA aspect ratio with letterboxing on resize. */
    SDL_RenderSetLogicalSize(s_renderer, GBA_DISPLAY_W, GBA_DISPLAY_H);
    SDL_RenderSetIntegerScale(s_renderer, SDL_TRUE);

    s_texture = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, GBA_DISPLAY_W,
                                  GBA_DISPLAY_H);
    if (s_texture == NULL) {
        fprintf(stderr, "[tmc_sdl] SDL_CreateTexture failed: %s\n", SDL_GetError());
        return -1;
    }

    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    return 0;
}

void Port_VideoShutdown(void) {
    if (s_texture) {
        SDL_DestroyTexture(s_texture);
        s_texture = NULL;
    }
    if (s_renderer) {
        SDL_DestroyRenderer(s_renderer);
        s_renderer = NULL;
    }
    if (s_window) {
        SDL_DestroyWindow(s_window);
        s_window = NULL;
    }
}

void Port_VideoPresent(void) {
    if (s_renderer == NULL || s_texture == NULL) {
        return;
    }

    /* PR #4: hand off to the cross-port software rasterizer. It writes
     * a packed 240x160 ARGB8888 image into s_framebuffer using the
     * emulated GBA memory regions exposed in include/platform/port.h.
     * Future ports (PSP, PS2, Win32) reuse the same renderer. */
    Port_RenderFrame(s_framebuffer);

    SDL_UpdateTexture(s_texture, NULL, s_framebuffer, GBA_DISPLAY_W * (int)sizeof(uint32_t));
    SDL_RenderClear(s_renderer);
    SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
    SDL_RenderPresent(s_renderer);
}
