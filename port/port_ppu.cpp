#include "port_ppu.h"
#include "port_gba_mem.h"
#include "port_hdma.h"
#include "port_upscale.h"
#include "port_runtime_config.h"
#include "port_filter.h"
#include "port_touch_controls.h"
#include "port_viewport.h"

#ifdef launcher
#include "tmc_launcher.h"
#endif

#include <cpu/mode1.h>
#include <virtuappu.h>

#include <cstdint>
#include <cstdio>
#include <climits>
#include <cstdlib>
#include <cstring>

enum class RenderBackend {
    None,
    Renderer,
    Surface,
};

/* User-cycled presentation modes. F12 advances through these. */
enum class PresentMode {
    NearestRaw = 0,   /* upload 240x160 directly, nearest-neighbor stretch  */
    XbrzLinear,       /* xBRZ 4x → 960x640, linear stretch (smooth, default) */
    XbrzNearest,      /* xBRZ 4x → 960x640, nearest stretch (sharp)          */
    LinearRaw,        /* upload 240x160 directly, linear stretch (blurry)    */
    Count
};

/* xBRZ output dimensions: 4x the presentation canvas. */
static const int kHiResW = PORT_VIEW_WIDTH * 4;
static const int kHiResH = PORT_VIEW_HEIGHT * 4;

static RenderBackend sBackend = RenderBackend::None;
static SDL_Renderer* sRenderer = nullptr;
static SDL_Texture* sLowResTexture = nullptr;   /* 240x160 raw upload */
static SDL_Texture* sHiResTexture = nullptr;    /* 960x640 upscaled  */
/* Internal-render-scale streaming texture: re-sized lazily when scale
 * changes (240*S x 160*S). Used when Port_Config_InternalScale() > 1
 * and the user has chosen a non-xBRZ presentation mode — the framebuffer
 * is S*S nearest-replicated into sScaledBuf and uploaded here. */
static SDL_Texture* sScaledTexture = nullptr;
static int sScaledTextureScale = 0;
static uint32_t* sScaledBuf = nullptr;
static int sScaledBufScale = 0;
static SDL_Window* sWindow = nullptr;
#ifdef launcher
static SDL_Window* sBootstrapWindow = nullptr;
#endif

static SDL_Window* Port_PPU_ActiveWindow(void) {
#ifdef launcher
    return sWindow ? sWindow : sBootstrapWindow;
#else
    return sWindow;
#endif
}

#ifdef launcher
extern "C" void Port_SetBootstrapWindow(SDL_Window* window) {
    sBootstrapWindow = window;
}
#endif
static SDL_Surface* sFrameSurface = nullptr;
static PresentMode sPresentMode = PresentMode::NearestRaw;
static PortFilterType sFilter = PORT_FILTER_NONE;
static uint32_t* sUpscale2xBuf = nullptr;       /* canvas*2 intermediate */
static uint32_t* sUpscale4xBuf = nullptr;       /* canvas*4 final        */

static void Port_PPU_LoadConfig(void) {
    const char* method = Port_Config_UpscaleMethod();
    if (std::strcmp(method, "nearest") == 0) {
        sPresentMode = PresentMode::NearestRaw;
    } else if (std::strcmp(method, "linear") == 0) {
        sPresentMode = PresentMode::LinearRaw;
    } else if (std::strcmp(method, "xbrz_nearest") == 0) {
        sPresentMode = PresentMode::XbrzNearest;
    } else {
        sPresentMode = PresentMode::XbrzLinear;
    }
}

static const char* Port_PPU_MethodForMode(PresentMode mode) {
    switch (mode) {
        case PresentMode::NearestRaw:
            return "nearest";
        case PresentMode::LinearRaw:
            return "linear";
        case PresentMode::XbrzNearest:
            return "xbrz_nearest";
        case PresentMode::XbrzLinear:
        default:
            return "xbrz_linear";
    }
}

extern "C" const char* Port_PPU_PresentationModeName(void) {
    static const char* const kNames[] = {
        "nearest",
        "xBRZ smooth",
        "xBRZ sharp",
        "linear",
    };
    return kNames[(int)sPresentMode];
}

/* The presentation canvas (see port_viewport.h). The PPU's 240x160 output
 * is composited into its centre each frame; everything downstream works on
 * this, so the whole present path is already viewport-sized ahead of the
 * expansion work. */
static uint32_t sCanvas[PORT_VIEW_WIDTH * PORT_VIEW_HEIGHT];

extern "C" uint32_t* Port_Viewport_Canvas(void) {
    return sCanvas;
}

/* Fill the border, then blit the PPU frame into the centre.
 *
 * Composite order note (research plan Spike 1 DoD): this runs *before*
 * internal scale, xBRZ and the CRT/LCD filters, so those treat the border
 * as part of the frame. That is deliberate. Once Milestones 1-2 land, the
 * PPU emits borders itself for rooms smaller than the viewport, and they
 * are ordinary rendered pixels — filtering them now is what the shipped
 * build will do, so no ordering changes later. The black bars that must
 * *never* be filtered are the window letterbox from fit-rect when the
 * window aspect is not 4:3; those live outside the canvas entirely and
 * are painted by SDL_RenderClear. */
static void Port_PPU_ComposeCanvas(void) {
    const int cw = PORT_VIEW_WIDTH;
    const int fw = PORT_VIEW_CONTENT_WIDTH;
    const int fh = PORT_VIEW_CONTENT_HEIGHT;
    const int ox = PORT_VIEW_CONTENT_X;
    const int oy = PORT_VIEW_CONTENT_Y;

    /* Fill only the border ring — the centre is fully overwritten by the
     * blit below, so clearing it too would double the write traffic. The
     * ring is four rects: rows above and below the content, then the left
     * and right margins of each content row. Skipped entirely once content
     * fills the canvas (Milestone 2), which is why this is written against
     * the PORT_VIEW_CONTENT_* constants rather than hardcoded 40s. */
    {
        const int ch = PORT_VIEW_HEIGHT;
        for (int y = 0; y < oy; ++y) {
            for (int x = 0; x < cw; ++x) sCanvas[y * cw + x] = PORT_VIEW_BORDER_COLOR;
        }
        for (int y = oy + fh; y < ch; ++y) {
            for (int x = 0; x < cw; ++x) sCanvas[y * cw + x] = PORT_VIEW_BORDER_COLOR;
        }
        for (int y = oy; y < oy + fh; ++y) {
            uint32_t* row = &sCanvas[y * cw];
            for (int x = 0; x < ox; ++x) row[x] = PORT_VIEW_BORDER_COLOR;
            for (int x = ox + fw; x < cw; ++x) row[x] = PORT_VIEW_BORDER_COLOR;
        }
    }
    for (int y = 0; y < fh; ++y) {
        std::memcpy(&sCanvas[(oy + y) * cw + ox],
                    &virtuappu_frame_buffer[y * MODE1_GBA_WIDTH],
                    (size_t)fw * sizeof(uint32_t));
    }

    /* A centred UI screen (logo, title, file select, menus) occupies a
     * DISPLAY_WIDTH-wide band in the middle of a wider frame, and the columns
     * either side carry that screen's PPU backdrop. Those columns used to be
     * repainted PORT_VIEW_BORDER_COLOR here, to satisfy the plan's original
     * D3 "solid black borders".
     *
     * That repaint is gone, for two reasons. D3 was amended at Milestone 1
     * sign-off — coloured borders are accepted, because the backdrop is what
     * hardware shows outside every layer anyway. And the repaint only ever
     * covered the *horizontal* band: the rows above and below a centred
     * screen are PPU output too and were never touched, so a 320x240 UI
     * screen came out with black bars left and right and its backdrop colour
     * above and below. Leaving the PPU's own output alone is what makes the
     * two axes agree — white around the Nintendo/Capcom logo, pale yellow
     * around the title, green around file select.
     *
     * PORT_VIEW_BORDER_COLOR still fills the ring above, which is a
     * different thing: canvas the PPU does not render into at all. */
}

// Largest canvas-aspect rect fitting inside (w, h), centered.
static void Port_PPU_ComputeFitRect(int w, int h, int* outX, int* outY, int* outW, int* outH) {
    const int FW = PORT_VIEW_WIDTH;
    const int FH = PORT_VIEW_HEIGHT;
    int rw;
    int rh;
    if (w * FH >= h * FW) {
        rh = h;
        rw = (h * FW) / FH;
    } else {
        rw = w;
        rh = (w * FH) / FW;
    }
    *outX = (w - rw) / 2;
    *outY = (h - rh) / 2;
    *outW = rw;
    *outH = rh;
}

static void Port_PPU_QueryOutputSize(int* outW, int* outH) {
    int w = 0;
    int h = 0;
    if (sRenderer) {
        SDL_GetCurrentRenderOutputSize(sRenderer, &w, &h);
    }
    if (w > 0 && h > 0) {
        *outW = w;
        *outH = h;
        return;
    }
    if (sWindow) {
        SDL_GetWindowSize(sWindow, &w, &h);
    }
    if (w > 0 && h > 0) {
        *outW = w;
        *outH = h;
        return;
    }
    *outW = 960;
    *outH = 540;
}

/* Build (or reuse) sScaledBuf at scale S and S*S-replicate the 240x160
 * framebuffer into it. Returns the buffer + dims via out-params; returns
 * nullptr if S<=1. The buffer survives across frames so we don't realloc
 * unless the scale changes.
 *
 * This is the Stage-1 shape of internal-render-scale: pure post-process
 * nearest-replicate on the CPU. By itself it produces visually the same
 * result as SDL_SCALEMODE_NEAREST presentation, but it puts the scaled
 * framebuffer in the pipeline so future PPU patches can render affine
 * paths directly at sub-pixel density and the rest of the path doesn't
 * need to change. */
static uint32_t* Port_PPU_BuildScaledFrame(int S, int* outW, int* outH) {
    if (S <= 1) {
        if (outW) *outW = 0;
        if (outH) *outH = 0;
        return nullptr;
    }
    const int FW = PORT_VIEW_WIDTH;
    const int FH = PORT_VIEW_HEIGHT;
    const int w = FW * S;
    const int h = FH * S;
    if (sScaledBuf == nullptr || sScaledBufScale != S) {
        std::free(sScaledBuf);
        sScaledBuf = (uint32_t*)std::malloc((size_t)w * (size_t)h * sizeof(uint32_t));
        sScaledBufScale = S;
        if (sScaledBuf == nullptr) {
            sScaledBufScale = 0;
            if (outW) *outW = 0;
            if (outH) *outH = 0;
            return nullptr;
        }
    }
    /* Nearest-replicate: each src pixel writes to an SxS block. Loop
     * order is src-major so the source line stays cache-resident while
     * we scatter S output rows. */
    for (int sy = 0; sy < FH; ++sy) {
        const uint32_t* src = &sCanvas[sy * FW];
        for (int dy = 0; dy < S; ++dy) {
            uint32_t* dst = &sScaledBuf[(sy * S + dy) * w];
            for (int sx = 0; sx < FW; ++sx) {
                uint32_t c = src[sx];
                uint32_t* d = &dst[sx * S];
                for (int dx = 0; dx < S; ++dx) {
                    d[dx] = c;
                }
            }
        }
    }

    if (outW) *outW = w;
    if (outH) *outH = h;
    return sScaledBuf;
}

static SDL_Texture* Port_PPU_EnsureScaledTexture(int S) {
    if (S <= 1) return nullptr;
    if (sScaledTexture != nullptr && sScaledTextureScale == S) {
        return sScaledTexture;
    }
    if (sScaledTexture != nullptr) {
        SDL_DestroyTexture(sScaledTexture);
        sScaledTexture = nullptr;
        sScaledTextureScale = 0;
    }
    sScaledTexture = SDL_CreateTexture(sRenderer, SDL_PIXELFORMAT_ABGR8888,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       PORT_VIEW_WIDTH * S, PORT_VIEW_HEIGHT * S);
    if (sScaledTexture) {
        sScaledTextureScale = S;
    }
    return sScaledTexture;
}

static void Port_PPU_PresentSurfaceFrame(void) {
    SDL_Surface* windowSurface = SDL_GetWindowSurface(sWindow);
    int x;
    int y;
    int w;
    int h;
    SDL_Rect dstRect;

    if (!windowSurface) {
        return;
    }

    Port_PPU_ComputeFitRect(windowSurface->w, windowSurface->h, &x, &y, &w, &h);
    dstRect = {x, y, w, h};
    SDL_FillSurfaceRect(windowSurface, nullptr, 0);
    SDL_BlitSurfaceScaled(sFrameSurface, nullptr, windowSurface, &dstRect, SDL_SCALEMODE_NEAREST);
    SDL_UpdateWindowSurface(sWindow);
}

static bool sVSyncEnabled = true;

extern "C" void Port_PPU_SetVSync(bool enabled) {
    if (sRenderer == nullptr) {
        sVSyncEnabled = enabled;
        return;
    }
    if (sVSyncEnabled == enabled) {
        return;
    }
    sVSyncEnabled = enabled;
    SDL_SetRenderVSync(sRenderer, enabled ? 1 : 0);
}

extern "C" void Port_PPU_Init(SDL_Window* window) {
    sWindow = window;
#ifdef launcher
    sBootstrapWindow = nullptr;
#endif
    Port_PPU_LoadConfig();

    /* Reuse the renderer the bootstrap progress UI created (if any)
     * instead of destroying it and making a new one. SDL only allows
     * one renderer per window, and recreating it on the same window
     * causes a visible compositor flash on most platforms — exactly
     * what made the asset-extractor screen look like a separate
     * window from the game. SDL_GetRenderer returns NULL when no
     * renderer has been associated with the window, in which case we
     * fall back to creating one ourselves. */
    sRenderer = SDL_GetRenderer(window);
    if (!sRenderer) {
        sRenderer = SDL_CreateRenderer(window, nullptr);
    }
    if (sRenderer) {
        SDL_SetRenderTarget(sRenderer, nullptr);
        SDL_SetRenderClipRect(sRenderer, nullptr);
    }
    if (!sRenderer) {
        printf("Port_PPU_Init: SDL_CreateRenderer failed: %s\n", SDL_GetError());
    } else {
        if (!SDL_SetRenderVSync(sRenderer, 1)) {
            printf("Port_PPU_Init: SDL_SetRenderVSync failed: %s\n", SDL_GetError());
        }
        sLowResTexture = SDL_CreateTexture(sRenderer, SDL_PIXELFORMAT_ABGR8888,
                                           SDL_TEXTUREACCESS_STREAMING,
                                           PORT_VIEW_WIDTH, PORT_VIEW_HEIGHT);
        sHiResTexture = SDL_CreateTexture(sRenderer, SDL_PIXELFORMAT_ABGR8888,
                                          SDL_TEXTUREACCESS_STREAMING, kHiResW, kHiResH);
        if (!sLowResTexture || !sHiResTexture) {
            printf("Port_PPU_Init: SDL_CreateTexture failed: %s\n", SDL_GetError());
            SDL_DestroyRenderer(sRenderer);
            sRenderer = nullptr;
        } else {
            sUpscale2xBuf = (uint32_t*)std::malloc(
                (size_t)(PORT_VIEW_WIDTH * 2) * (PORT_VIEW_HEIGHT * 2) * sizeof(uint32_t));
            sUpscale4xBuf = (uint32_t*)std::malloc((size_t)kHiResW * kHiResH * sizeof(uint32_t));
            sBackend = RenderBackend::Renderer;
        }
    }

    {
        /* TMC_OAMY_LEGACY=1 unbinds the untruncated-y channel, leaving the
         * PPU on the pure 8-bit wrap heuristic. The A/B that shows what the
         * channel is worth at 240 lines: with it, a sprite straddling the
         * top edge stays there; without it, the same encoding resolves onto
         * the visible screen. At 160 lines the two agree by construction. */
        const bool oam_y_legacy = std::getenv("TMC_OAMY_LEGACY") != nullptr;
        VirtuaPPUMode1GbaMemory memory = {
            gIoMem,
            gVram,
            gBgPltt,
            gObjPltt,
            gOamMem,
            oam_y_legacy ? nullptr : gOamYExt,
        };
        virtuappu_mode1_bind_gba_memory(&memory);
    }

    virtuappu_mode1_pre_line_callback = nullptr;

    virtuappu_registers.frame_width = MODE1_GBA_WIDTH;
    virtuappu_registers.mode = 1;

    if (sBackend == RenderBackend::None) {
        sFrameSurface = SDL_CreateSurfaceFrom(
            PORT_VIEW_WIDTH,
            PORT_VIEW_HEIGHT,
            SDL_PIXELFORMAT_ABGR8888,
            sCanvas,
            PORT_VIEW_WIDTH * static_cast<int>(sizeof(uint32_t)));
        if (!sFrameSurface) {
            printf("Port_PPU_Init: SDL_CreateSurfaceFrom failed: %s\n", SDL_GetError());
            return;
        }

        if (!SDL_SetWindowSurfaceVSync(window, 1)) {
            printf("Port_PPU_Init: SDL_SetWindowSurfaceVSync failed: %s\n", SDL_GetError());
        }

        sBackend = RenderBackend::Surface;
        SDL_ShowWindow(window);
        SDL_RaiseWindow(window);
        SDL_SyncWindow(window);
        Port_PPU_PresentSurfaceFrame();
        printf("PPU initialized with SDL window surface fallback.\n");
    } else {
        printf("PPU initialized with SDL renderer backend.\n");
    }
}

/* TMC_OAMY_PROBE=<signed y> — park a copy of the first enabled sprite at a
 * chosen y in a spare OAM slot, publishing the same y through the untruncated
 * channel. The route's own content only ever hangs ~31 px above the top edge,
 * so this is how the deeper overhangs get exercised: at 240 lines the 8-bit
 * field can only express -16, and a sprite at -64 has no encoding at all.
 * Runs after the engine's OAM publish, so nothing overwrites it. */
static void Port_PPU_OamYProbe(void) {
    /* Resolved once — this sits in the per-frame present path. */
    static const int sProbeY = [] {
        const char* env = std::getenv("TMC_OAMY_PROBE");
        return env != nullptr ? std::atoi(env) : INT_MIN;
    }();
    if (sProbeY == INT_MIN) {
        return;
    }
    const int y = sProbeY;
    const int slot = 0x7F; /* top of the array; the engine fills upward */
    int src = -1;

    for (int i = 0; i < slot; i++) {
        uint16_t attr0 = gOamMem[i * 4];
        if ((attr0 & 0x0300) != 0x0200) { /* enabled */
            src = i;
            break;
        }
    }
    if (src < 0) {
        return; /* nothing on screen to copy tiles from */
    }

    /* Forced to a 64x64 square so the visible-row arithmetic is predictable:
     * at y the sprite occupies screen rows y..y+63, so a negative y leaves
     * exactly 64+y rows showing and y <= -64 must leave none. Shape bits are
     * attr0[15:14], size bits attr1[15:14]; x is pinned to 128. */
    gOamMem[slot * 4 + 0] = (uint16_t)((gOamMem[src * 4 + 0] & 0x3F00) | (y & 0xFF));
    gOamMem[slot * 4 + 1] = (uint16_t)(0xC000 | 0x0080);
    gOamMem[slot * 4 + 2] = gOamMem[src * 4 + 2];
    gOamYExt[slot] = (int16_t)y;
}

extern "C" void Port_PPU_PresentFrame(void) {
    uint16_t dispcnt;
    uint8_t gbaMode;

    if (sBackend == RenderBackend::None) {
        return;
    }

    dispcnt = (uint16_t)(gIoMem[0x00] | (gIoMem[0x01] << 8));
    gbaMode = (uint8_t)(dispcnt & 0x07);

    /* GBA mode 1 = BG0/BG1 text + BG2 affine + OBJ. VirtuaPPU's mode 2
     * matches that hardware behaviour; routing GBA mode 1 to VirtuaPPU mode
     * 1 reads BG2 with text-BG indexing and the title-screen affine sword
     * comes out as garbage tiles. Keep GBA mode 0 on VirtuaPPU mode 1.
     * (Originally fixed in ad9b4d94, regressed in matheo merge dec390c2.) */
    switch (gbaMode) {
        case 0:
            virtuappu_registers.mode = 1;
            break;
        case 1:
        case 2:
            virtuappu_registers.mode = 2;
            break;
        default:
            virtuappu_registers.mode = 1;
            break;
    }

    /* HBlank-DMA simulation: only enable the scanline callback while a
     * channel is active. Affine BG rendering treats BG2X/BG2Y differently
     * when HDMA has already supplied per-line reference points. */
    virtuappu_mode1_pre_line_callback =
        port_hdma_has_active_channels() ? port_hdma_step_line : nullptr;
    /* The affine renderer must not accumulate pb/pd across lines while the DMA
     * is rewriting BG2X/BG2Y per scanline. Re-evaluated every frame because the
     * registration comes and goes with the scene. */
    virtuappu_mode1_set_bg2_ref_per_line(port_hdma_drives_bg2_reference() != 0);

    Port_PPU_OamYProbe();
    /* TMC_DISABLE_OBJ=1 drops sprites, TMC_DISABLE_BG0=1 drops the HUD.
     *
     * Comparing two frames to decide whether a *background* defect is gone
     * fails on both of them. Sprite animation differs wherever an NPC moved.
     * The HUD is worse and less obvious: two frames one pixel of camera apart
     * have to be shifted a pixel to line the world up, and that shift
     * misaligns everything drawn at a fixed *screen* position, so the HUD
     * reports as a difference on every edge it has. Both land in the same
     * periphery the defect does. B26 measured per-layer contributions the
     * same way. Diagnostic only. */
    {
        static int layerMask = -1;
        if (layerMask < 0) {
            layerMask = 0xFF;
            if (getenv("TMC_DISABLE_OBJ") != nullptr) {
                layerMask &= ~0x10; /* DISPCNT bit 12: OBJ */
            }
            if (getenv("TMC_DISABLE_BG0") != nullptr) {
                layerMask &= ~0x01; /* DISPCNT bit 8: BG0 */
            }
        }
        if (layerMask != 0xFF) {
            gIoMem[1] = (u8)(gIoMem[1] & layerMask);
        }
    }
    virtuappu_render_frame();
    Port_PPU_ComposeCanvas();

    if (sBackend == RenderBackend::Renderer) {
        int outW = 0;
        int outH = 0;
        Port_PPU_QueryOutputSize(&outW, &outH);
        Port_TouchControls_NotifyRenderSize(outW, outH);
        int x;
        int y;
        int w;
        int h;
        Port_PPU_ComputeFitRect(outW, outH, &x, &y, &w, &h);
        SDL_FRect dst = { (float)x, (float)y, (float)w, (float)h };

        SDL_Texture* tex;
        SDL_ScaleMode scale;
        const int internalS = (int)Port_Config_InternalScale();
        switch (sPresentMode) {
            case PresentMode::XbrzLinear:
            case PresentMode::XbrzNearest:
                /* xBRZ owns its own 4x upscaler — internal-render-scale
                 * is mutually exclusive with it. The xBRZ path always
                 * consumes the unscaled GBA-native framebuffer. */
                Port_Upscale_xBRZ_4x(sCanvas,
                                     PORT_VIEW_WIDTH, PORT_VIEW_HEIGHT,
                                     sUpscale2xBuf, sUpscale4xBuf);
                /* CRT/LCD filter at the upscaled resolution (4x). The
                 * pattern needs >= 3 px per phosphor cell to read
                 * correctly, so xBRZ's 4x output is always large enough. */
                Port_Filter_Apply(sUpscale4xBuf, kHiResW, kHiResH, 4, sFilter);
                SDL_UpdateTexture(sHiResTexture, nullptr, sUpscale4xBuf,
                                  kHiResW * (int)sizeof(uint32_t));
                tex = sHiResTexture;
                scale = (sPresentMode == PresentMode::XbrzLinear)
                            ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST;
                break;
            case PresentMode::LinearRaw:
            case PresentMode::NearestRaw:
            default: {
                int sw = 0, sh = 0;
                /* Filter needs a scaled buffer to operate on (1x has too
                 * few pixels per phosphor cell). Force at least 4x when
                 * a filter is active, otherwise honour the user's
                 * internal-scale setting. */
                int effScale = internalS;
                if (sFilter != PORT_FILTER_NONE && effScale < 4) {
                    effScale = 4;
                }
                uint32_t* scaled = Port_PPU_BuildScaledFrame(effScale, &sw, &sh);
                SDL_Texture* scaledTex = Port_PPU_EnsureScaledTexture(effScale);
                if (scaled && scaledTex) {
                    Port_Filter_Apply(scaled, sw, sh, effScale, sFilter);
                    SDL_UpdateTexture(scaledTex, nullptr, scaled, sw * (int)sizeof(uint32_t));
                    tex = scaledTex;
                } else {
                    SDL_UpdateTexture(sLowResTexture, nullptr, sCanvas,
                                      PORT_VIEW_WIDTH * (int)sizeof(uint32_t));
                    tex = sLowResTexture;
                }
                scale = (sPresentMode == PresentMode::LinearRaw)
                            ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST;
                break;
            }
        }
        SDL_SetTextureScaleMode(tex, scale);
        SDL_SetRenderDrawColor(sRenderer, 0, 0, 0, 255);
        SDL_RenderClear(sRenderer);
        SDL_RenderTexture(sRenderer, tex, nullptr, &dst);
        {
            extern void Port_DebugMenu_Render(SDL_Renderer*, int, int);
            Port_DebugMenu_Render(sRenderer, outW, outH);
            extern void Port_SoftSlots_RenderOverlay(void*, int, int);
            Port_SoftSlots_RenderOverlay(sRenderer, outW, outH);
            Port_TouchControls_Render(sRenderer, outW, outH);
        }
        SDL_RenderPresent(sRenderer);
        return;
    }

    Port_PPU_PresentSurfaceFrame();
}

extern "C" void Port_PPU_SetWindowTitle(const char* title) {
    if (!sWindow || !title) {
        return;
    }
    SDL_SetWindowTitle(sWindow, title);
}

extern "C" void Port_PPU_ToggleFullscreen(void) {
    SDL_Window* w = Port_PPU_ActiveWindow();
    if (!w) {
        return;
    }
    SDL_WindowFlags flags = SDL_GetWindowFlags(w);
    bool wantFullscreen = (flags & SDL_WINDOW_FULLSCREEN) == 0;
    SDL_SetWindowFullscreen(w, wantFullscreen);
    SDL_SyncWindow(w);
}

extern "C" bool Port_PPU_IsFullscreen(void) {
    SDL_Window* w = Port_PPU_ActiveWindow();
    if (!w) {
        return false;
    }
    return (SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN) != 0;
}

extern "C" unsigned char Port_PPU_WindowScale(void) {
    return Port_Config_WindowScale();
}

extern "C" void Port_PPU_CycleWindowScale(int direction) {
    u8 scale = Port_Config_WindowScale();
    if (direction < 0) {
        scale = scale <= 1 ? 10 : (u8)(scale - 1);
    } else {
        scale = scale >= 10 ? 1 : (u8)(scale + 1);
    }
    Port_Config_SetWindowScale(scale);
    SDL_Window* w = Port_PPU_ActiveWindow();
    if (w && !Port_PPU_IsFullscreen()) {
        SDL_SetWindowSize(w, PORT_VIEW_WIDTH * scale, PORT_VIEW_HEIGHT * scale);
        SDL_SyncWindow(w);
    }
}

extern "C" void Port_PPU_CyclePresentationMode(int direction) {
    int next = (int)sPresentMode + (direction < 0 ? -1 : 1);
    if (next < 0) {
        next = (int)PresentMode::Count - 1;
    } else if (next >= (int)PresentMode::Count) {
        next = 0;
    }
    sPresentMode = (PresentMode)next;
    Port_Config_SetUpscaleMethod(Port_PPU_MethodForMode(sPresentMode));
    fprintf(stderr, "PPU upscale: %s\n", Port_PPU_PresentationModeName());
}

extern "C" void Port_PPU_ToggleSmoothing(void) {
    Port_PPU_CyclePresentationMode(1);
}

extern "C" void Port_PPU_CycleFilter(int direction) {
    int next = (int)sFilter + (direction < 0 ? -1 : 1);
    if (next < 0) {
        next = (int)PORT_FILTER_COUNT - 1;
    } else if (next >= (int)PORT_FILTER_COUNT) {
        next = 0;
    }
    sFilter = (PortFilterType)next;
    fprintf(stderr, "PPU filter: %s\n", Port_Filter_Name(sFilter));
}

extern "C" const char* Port_PPU_FilterName(void) {
    return Port_Filter_Name(sFilter);
}

extern "C" bool Port_InGameSettingsModalIsOpen(void) {
#ifdef launcher
    return TmcSettings_IsModalOpen();
#else
    return false;
#endif
}

extern "C" void Port_OpenInGameSettingsModal(void) {
#ifdef launcher
    if (TmcSettings_IsModalOpen()) {
        return;
    }
    SDL_Window* w = sWindow ? sWindow : sBootstrapWindow;
    if (!w) {
        return;
    }
    SDL_Renderer* r = sRenderer;
    if (!r) {
        r = SDL_GetRenderer(w);
    }
    if (!r) {
        return;
    }
    if (!TmcSettings_RunModalInGame(w, r)) {
        SDL_Event ev;
        std::memset(&ev, 0, sizeof(ev));
        ev.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&ev);
    }
#else
    /* No launcher: settings UI is not linked. */
#endif
}

extern "C" void Port_PPU_Shutdown(void) {
    if (sWindow && sBackend == RenderBackend::Surface) {
        SDL_DestroyWindowSurface(sWindow);
    }
    if (sFrameSurface) {
        SDL_DestroySurface(sFrameSurface);
        sFrameSurface = nullptr;
    }
    if (sLowResTexture) {
        SDL_DestroyTexture(sLowResTexture);
        sLowResTexture = nullptr;
    }
    if (sHiResTexture) {
        SDL_DestroyTexture(sHiResTexture);
        sHiResTexture = nullptr;
    }
    if (sUpscale2xBuf) {
        std::free(sUpscale2xBuf);
        sUpscale2xBuf = nullptr;
    }
    if (sUpscale4xBuf) {
        std::free(sUpscale4xBuf);
        sUpscale4xBuf = nullptr;
    }
    if (sRenderer) {
        SDL_DestroyRenderer(sRenderer);
        sRenderer = nullptr;
    }
    sBackend = RenderBackend::None;
    sWindow = nullptr;
}
