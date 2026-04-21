/**
 * @file video.c
 * @brief SDL window + renderer + framebuffer presentation.
 *
 * This is the placeholder renderer for PR #1: it just paints a solid
 * color into the 240x160 framebuffer and uploads it to an SDL_Texture.
 * PR #4-5 will replace `Port_VideoPresent` with a real software
 * rasterizer that reads VRAM, OAM, PLTT, and the BG/BLEND/window
 * registers from `gPortVram` / `gPortOam` / `gPortPltt` / `gPortIo`.
 */
#include "platform/port.h"

#include <SDL.h>

#include <stdio.h>
#include <string.h>

#define GBA_DISPLAY_W 240
#define GBA_DISPLAY_H 160

static SDL_Window*   s_window    = NULL;
static SDL_Renderer* s_renderer  = NULL;
static SDL_Texture*  s_texture   = NULL;
static uint32_t      s_framebuffer[GBA_DISPLAY_W * GBA_DISPLAY_H];

int Port_VideoInit(int scale, int fullscreen)
{
    if (scale < 1) scale = 4;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "[tmc_sdl] SDL_INIT_VIDEO failed: %s\n", SDL_GetError());
        return -1;
    }

    Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    s_window = SDL_CreateWindow(
        "The Legend of Zelda: The Minish Cap (SDL port)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        GBA_DISPLAY_W * scale, GBA_DISPLAY_H * scale,
        flags);
    if (s_window == NULL) {
        fprintf(stderr, "[tmc_sdl] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return -1;
    }

    s_renderer = SDL_CreateRenderer(
        s_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
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

    s_texture = SDL_CreateTexture(
        s_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        GBA_DISPLAY_W, GBA_DISPLAY_H);
    if (s_texture == NULL) {
        fprintf(stderr, "[tmc_sdl] SDL_CreateTexture failed: %s\n", SDL_GetError());
        return -1;
    }

    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    return 0;
}

void Port_VideoShutdown(void)
{
    if (s_texture)  { SDL_DestroyTexture(s_texture);   s_texture  = NULL; }
    if (s_renderer) { SDL_DestroyRenderer(s_renderer); s_renderer = NULL; }
    if (s_window)   { SDL_DestroyWindow(s_window);     s_window   = NULL; }
}

void Port_VideoPresent(void)
{
    if (s_renderer == NULL || s_texture == NULL) {
        return;
    }

    /* PR #4-5 will compose backgrounds and sprites into s_framebuffer
     * here. For now keep it cleared to opaque black so the user sees a
     * stable window rather than an uninitialized buffer. */
    for (int i = 0; i < GBA_DISPLAY_W * GBA_DISPLAY_H; ++i) {
        s_framebuffer[i] = 0xFF000000u;  /* opaque black */
    }

    SDL_UpdateTexture(s_texture, NULL, s_framebuffer,
                      GBA_DISPLAY_W * (int)sizeof(uint32_t));
    SDL_RenderClear(s_renderer);
    SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
    SDL_RenderPresent(s_renderer);
}
