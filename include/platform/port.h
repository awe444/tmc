/**
 * @file port.h
 * @brief Public interface to the host-side platform layer.
 *
 * Anything declared here is provided by `src/platform/shared/` (and its
 * platform-specific siblings under `src/platform/sdl/`, etc.). Keep this
 * header free of SDL types — game code must be able to include it without
 * pulling in SDL.
 *
 * See docs/sdl_port.md for the overall layering rules.
 */
#ifndef TMC_PLATFORM_PORT_H
#define TMC_PLATFORM_PORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/* Emulated GBA memory regions.                                             */
/*                                                                          */
/* PR #2 of the roadmap will repoint the macros in include/gba/io_reg.h     */
/* (REG_DISPCNT, REG_KEYINPUT, OAM, VRAM, BG_PLTT, OBJ_PLTT, ...) at the    */
/* fixed offsets inside these arrays so the existing decompiled game code   */
/* can read/write them with no source changes.                              */
/* ------------------------------------------------------------------------ */
#define PORT_EWRAM_SIZE 0x40000u
#define PORT_IWRAM_SIZE 0x08000u
#define PORT_VRAM_SIZE 0x18000u
#define PORT_OAM_SIZE 0x00400u
#define PORT_PLTT_SIZE 0x00400u
#define PORT_IO_SIZE 0x00400u

extern uint8_t gPortEwram[PORT_EWRAM_SIZE];
extern uint8_t gPortIwram[PORT_IWRAM_SIZE];
extern uint8_t gPortVram[PORT_VRAM_SIZE];
extern uint8_t gPortOam[PORT_OAM_SIZE];
extern uint8_t gPortPltt[PORT_PLTT_SIZE];
extern uint8_t gPortIo[PORT_IO_SIZE];

/** Zero every emulated RAM region. Called once during boot. */
void Port_InitMemory(void);

/* ------------------------------------------------------------------------ */
/* Frame pacing / VBlank.                                                   */
/*                                                                          */
/* The GBA runs at exactly 280896 cycles/frame at SYSTEM_CLOCK = 16.78 MHz, */
/* which is ~59.7274 Hz. We pace to that rate (not 60 Hz) so the existing   */
/* game timing is unmodified.                                               */
/* ------------------------------------------------------------------------ */
#define PORT_FRAMES_PER_SECOND 59.7274
#define PORT_FRAME_NS 16743055 /* 1e9 / 59.7274 */

/** Block until the next emulated VBlank. Replacement for VBlankIntrWait. */
void Port_VBlankIntrWait(void);

/** Called from the SDL frame loop when VBlank fires. */
void Port_OnVBlank(void);

/** Returns non-zero once the user has requested shutdown (SDL_QUIT, etc.). */
int Port_ShouldQuit(void);

/** Set the quit flag. Wakes any thread parked in Port_VBlankIntrWait. */
void Port_RequestQuit(void);

/**
 * Auto-quit after this many VBlanks (used for the CI smoke test:
 * --frames=N on the command line). 0 disables the budget.
 */
void Port_SetFrameBudget(int frames);

/**
 * Run the GBA-style entry point under a non-local-jump frame so the
 * pacer can bail back out when shutdown is requested. The real game's
 * `AgbMain()` (src/main.c) is an infinite loop with no `Port_ShouldQuit`
 * polling, so on the host build we wrap it with a setjmp checkpoint
 * here and have the host VBlank pacer (`Port_VBlankIntrWait`) longjmp
 * back to this checkpoint when the user has requested shutdown or the
 * `--frames=N` budget is exhausted. Returns to the caller exactly once
 * the entry function would otherwise have looped forever.
 *
 * Implementation lives in src/platform/shared/interrupts.c and is only
 * used by the SDL `main()` (see src/platform/sdl/main.c). Safe to call
 * with a NULL entry pointer (returns immediately).
 */
void Port_RunGameLoop(void (*entry)(void));

/**
 * Idempotent host-side initialiser for globals (entity-list sentinel
 * heads, etc.) that the GBA build expects to be non-zero before any
 * game code runs. Implemented in src/platform/shared/port_globals.c.
 * Must be called once before `AgbMain`.
 */
void Port_GlobalsInit(void);

/* ------------------------------------------------------------------------ */
/* Input.                                                                   */
/*                                                                          */
/* The host writes a 10-bit GBA-format key bitmask into a slot that PR #2   */
/* will alias as REG_KEYINPUT (active-low, like the hardware register).     */
/* ------------------------------------------------------------------------ */

/** Initialize keyboard + gamepad subsystems. Returns 0 on success. */
int Port_InputInit(void);

/** Tear down input. Safe to call multiple times. */
void Port_InputShutdown(void);

/** Pump SDL events and refresh the emulated REG_KEYINPUT slot. */
void Port_InputPump(void);

/** Returns the most recent active-high key bitmask (for tests / debug). */
uint16_t Port_GetKeyMask(void);

/* ------------------------------------------------------------------------ */
/* Video.                                                                   */
/* ------------------------------------------------------------------------ */

/** Open the SDL window and renderer. Returns 0 on success. */
int Port_VideoInit(int scale, int fullscreen);

/** Tear down the SDL window. */
void Port_VideoShutdown(void);

/**
 * Present the current emulated framebuffer. The software rasterizer
 * (`Port_RenderFrame()` in `src/platform/shared/render.c`) reads
 * VRAM, OAM, PLTT, and the BG/OBJ I/O registers from `gPortVram` /
 * `gPortOam` / `gPortPltt` / `gPortIo`. As of PR #5 it covers the
 * full text + affine BG pipeline (modes 0/1/2), regular + affine
 * sprites, windows 0/1/OBJ/outside, BLDCNT/BLDALPHA/BLDY blending
 * and mosaic. Bitmap modes (3/4/5) are still deferred.
 */
void Port_VideoPresent(void);

/* ------------------------------------------------------------------------ */
/* Software rasterizer (PR #4).                                              */
/*                                                                          */
/* The rasterizer is host-portable and lives in `src/platform/shared/        */
/* render.c`. It has no SDL or OS dependency: it reads exclusively from the  */
/* host arrays declared above and writes a packed 240 x 160 ARGB8888         */
/* framebuffer (row-major, no padding). Future ports (PSP, PS2, win32)       */
/* reuse it by calling `Port_RenderFrame()` and uploading the result to      */
/* their own swap chain.                                                     */
/* ------------------------------------------------------------------------ */
#define PORT_GBA_DISPLAY_WIDTH 240
#define PORT_GBA_DISPLAY_HEIGHT 160

/**
 * Render one frame's worth of GBA video output into `framebuffer`.
 *
 * Scope (PRs #4 + #5):
 *   - BG mode 0 (4 text BGs) and the text BGs of mode 1 (BG0/BG1).
 *   - Affine BGs in modes 1 (BG2) and 2 (BG2/BG3), with the
 *     four screen sizes (128/256/512/1024 px square) and the
 *     BGCNT bit 13 wrap-vs-transparent toggle.
 *   - Regular OBJ sprites (4 bpp + 8 bpp, 1D + 2D mapping, all 12
 *     shape x size combinations, hflip / vflip, attr0 disable).
 *   - Affine OBJ sprites (32 affine groups, DOUBLE_SIZE bounding box).
 *   - Windows 0 / 1 / OBJ window / outside (WIN0H/V, WIN1H/V,
 *     WININ, WINOUT, OBJ-window source pixels via attr0 mode 2).
 *   - Alpha blending (BLDCNT mode 1, BLDALPHA), brightness fade up
 *     (mode 2, BLDY) and fade down (mode 3, BLDY); OBJ semi-
 *     transparent (attr0 mode 1) forces alpha regardless of BLDCNT
 *     mode.
 *   - Mosaic (MOSAIC, BGCNT bit 6 for BGs, OBJ attr0 mosaic for
 *     OBJs).
 *
 * Still deferred: bitmap modes 3/4/5 (the renderer falls through to
 * a backdrop fill so the screen stays in a defined state), and
 * mid-scanline raster effects driven from HBlank IRQs.
 *
 * `framebuffer` must point at a buffer of at least
 * `PORT_GBA_DISPLAY_WIDTH * PORT_GBA_DISPLAY_HEIGHT` 32-bit pixels.
 * Each pixel is 0xAARRGGBB; alpha is always 0xFF.
 */
void Port_RenderFrame(uint32_t* framebuffer);

/**
 * Headless self-check for the rasterizer (PRs #4 + #5). Programs a
 * battery of known tilemap / sprite / palette / blend / window /
 * mosaic / affine inputs into gPortVram / gPortPltt / gPortOam /
 * gPortIo, runs `Port_RenderFrame()`, and verifies the produced
 * pixels. Returns 0 on success, non-zero on any mismatch. Called
 * from the platform layer's headless smoke test alongside
 * `Port_HeadersSelfCheck()`.
 *
 * The function saves and restores the affected I/O register / palette /
 * VRAM / OAM bytes so it can be invoked at any time without side
 * effects on subsequent rendering.
 */
int Port_RendererSelfCheck(void);

/* ------------------------------------------------------------------------ */
/* Audio.                                                                   */
/* ------------------------------------------------------------------------ */

/** Open SDL audio (silent stub mixer for now). */
int Port_AudioInit(void);

/** Close SDL audio. */
void Port_AudioShutdown(void);

/* ------------------------------------------------------------------------ */
/* Synchronous DMA helpers.                                                 */
/*                                                                          */
/* The GBA `Dma*` macros in include/gba/macro.h write to memory-mapped DMA  */
/* registers and rely on the DMA hardware to perform the transfer and to   */
/* clear the enable bit when done. On the host neither happens — so the    */
/* transfer never runs and any subsequent `DmaWait` spins forever. Under   */
/* `__PORT__` the macros in macro.h forward into these synchronous host    */
/* implementations (memcpy/memset) so the transfer happens immediately and */
/* the matching wait is a no-op.                                           */
/* ------------------------------------------------------------------------ */
void Port_DmaCopy16(int channel, const void* src, void* dst, uint32_t size);
void Port_DmaCopy32(int channel, const void* src, void* dst, uint32_t size);
void Port_DmaFill16(int channel, uint16_t value, void* dst, uint32_t size);
void Port_DmaFill32(int channel, uint32_t value, void* dst, uint32_t size);
void Port_DmaStop(int channel);
void Port_DmaWait(int channel);
void Port_DmaSet(int channel, const void* src, void* dst, uint32_t control);

/* ------------------------------------------------------------------------ */
/* Save backend.                                                            */
/* ------------------------------------------------------------------------ */

#define PORT_SAVE_SIZE 0x10000u /* 64 KiB Flash layout */

/** Load `tmc.sav` from disk into the emulated Flash buffer. */
int Port_SaveLoad(const char* save_dir);

/** Flush the emulated Flash buffer back out to disk. */
int Port_SaveFlush(void);

/** Direct read access to a byte in the emulated Flash. */
uint8_t Port_SaveReadByte(uint32_t offset);

/** Direct write access to a byte in the emulated Flash. */
void Port_SaveWriteByte(uint32_t offset, uint8_t value);

/* ------------------------------------------------------------------------ */
/* Game entry point.                                                        */
/*                                                                          */
/* `AgbMain` is defined in src/main.c by the real game source. Until that   */
/* file is wired into the SDL target (PR #2), src/platform/shared/          */
/* agb_main_stub.c provides a placeholder that runs an empty frame loop so  */
/* the platform layer is still exercisable on its own.                      */
/* ------------------------------------------------------------------------ */
void AgbMain(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TMC_PLATFORM_PORT_H */
