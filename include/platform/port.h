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
/* Hardware-address translation.                                            */
/*                                                                          */
/* A handful of game source files hand the BIOS / DMA helpers a literal     */
/* GBA hardware address (`0x06000000`, `0x02034570`, ...) rather than       */
/* going through the macro layer that already remaps to the gPort* arrays. */
/* On real hardware those literals are valid; on the host they would       */
/* dereference an unmapped low-memory page and SIGSEGV. The host-side       */
/* DMA / LZ77 / CpuSet helpers run a defensive translation pass through    */
/* `Port_TranslateHwAddr()` before touching memory: addresses in a known    */
/* GBA region are remapped to the matching offset inside `gPort*`;         */
/* addresses outside any known region (i.e. real host pointers from        */
/* `gPort* + N`) are returned unchanged.                                    */
/*                                                                          */
/* This is the single source of truth for the translation table; the       */
/* `PORT_HW_ADDR(x)` macro in `include/gba/defines.h` (used directly from  */
/* a few `#ifdef __PORT__` patches in the game source) forwards into here  */
/* under `__PORT__` for consistency.                                        */
/* ------------------------------------------------------------------------ */
void* Port_TranslateHwAddr(uintptr_t addr);

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
/* Scripted input (test harness).                                           */
/*                                                                          */
/* Lets headless CI runs drive button presses on specific frames so the     */
/* smoke test can advance past idle screens (title screen, file select,    */
/* ...) and exercise more game code. Implemented in                        */
/* `src/platform/shared/scripted_input.c`. See `--press=` /                */
/* `--input-script=` in `src/platform/sdl/main.c`.                          */
/*                                                                          */
/* The mask returned by `Port_ScriptedInputCurrentMask()` is OR'd into the  */
/* keyboard / gamepad mask by `src/platform/sdl/input.c::Port_InputPump()`, */
/* so scripted presses appear in the emulated `REG_KEYINPUT` slot through  */
/* the same code path real input does.                                     */
/* ------------------------------------------------------------------------ */

/* Bit layout matches the GBA `REG_KEYINPUT` layout used elsewhere in the   */
/* port (A=0x001 ... L=0x200). Repeated here so callers do not have to     */
/* pull in include/gba/io_reg.h. */
#define PORT_SCRIPTED_KEY_A 0x0001u
#define PORT_SCRIPTED_KEY_B 0x0002u
#define PORT_SCRIPTED_KEY_SELECT 0x0004u
#define PORT_SCRIPTED_KEY_START 0x0008u
#define PORT_SCRIPTED_KEY_RIGHT 0x0010u
#define PORT_SCRIPTED_KEY_LEFT 0x0020u
#define PORT_SCRIPTED_KEY_UP 0x0040u
#define PORT_SCRIPTED_KEY_DOWN 0x0080u
#define PORT_SCRIPTED_KEY_R 0x0100u
#define PORT_SCRIPTED_KEY_L 0x0200u
#define PORT_SCRIPTED_KEYS_MASK 0x03FFu

/**
 * Schedule `mask` to be held from `start_frame` (inclusive) for `duration`
 * frames. `start_frame` is 0-based and counts VBlanks since boot (see
 * `Port_GetFrameCount()`). Returns 0 on success, non-zero if the internal
 * table is full or the arguments are invalid.
 */
int Port_ScriptedInputAdd(uint32_t start_frame, uint32_t duration, uint16_t mask);

/**
 * Parse one or more scripted-input specs from `spec` and append them to
 * the internal table. Syntax (whitespace-insensitive):
 *
 *     SPEC    := ENTRY (',' ENTRY)*
 *     ENTRY   := KEYS '@' START [ '+' DURATION ]
 *     KEYS    := KEY ('|' KEY)*
 *     KEY     := A | B | START | SELECT | UP | DOWN | LEFT | RIGHT | L | R
 *     START   := unsigned decimal integer (frame index, 0-based)
 *     DURATION:= unsigned decimal integer (default: 1)
 *
 * Examples: `START@60+10`, `A@30,B@40+5`, `UP|A@120+2`. Returns 0 on
 * success, non-zero on parse error.
 */
int Port_ScriptedInputParse(const char* spec);

/**
 * Load scripted-input specs from a UTF-8 text file. Lines starting with
 * `#` and blank lines are ignored; every other line is passed to
 * `Port_ScriptedInputParse`. Returns 0 on success.
 */
int Port_ScriptedInputLoadFile(const char* path);

/** Reset the scripted-input table to empty. */
void Port_ScriptedInputReset(void);

/** Return the OR of every scheduled mask whose interval covers the
 *  current frame counter (`Port_GetFrameCount()`). */
uint16_t Port_ScriptedInputCurrentMask(void);

/** Return the OR of every scheduled mask whose interval covers the
 *  given frame index. Exposed mainly so tests can drive the lookup
 *  without depending on the live VBlank counter. */
uint16_t Port_ScriptedInputMaskForFrame(uint32_t frame);

/**
 * Headless self-check for the scripted-input parser and table. Exercises
 * every parser branch (single key, multi-key with `|`, durations, file
 * loading, error paths) and verifies the resulting frame-indexed mask
 * matches what was scheduled. Returns 0 on success, non-zero on any
 * mismatch. Saves and restores the table state so it is safe to call at
 * any time. Called from `src/platform/sdl/main.c` alongside
 * `Port_RendererSelfCheck()`.
 */
int Port_ScriptedInputSelfCheck(void);

/** Number of VBlanks observed since boot. Wraps at UINT32_MAX (~830 days
 *  at 59.7274 Hz). Implemented in `interrupts.c`. */
uint32_t Port_GetFrameCount(void);

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
/* framebuffer (row-major, no padding). Future ports (PSP, PS2) reuse it */
/* by calling `Port_RenderFrame()` and uploading the result to their own  */
/* swap chain.                                                            */
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

/** Open SDL audio. The device drains an SPSC ring buffer that the
 *  host m4a mixer feeds via `Port_AudioPushSamples()`. While no
 *  producer is wired in (PR #7 part 2.2 / 2.3) the consumer underflows
 *  to zero-fill, so the device plays silence — same audible behaviour
 *  as the original silent-callback stub but with the data path that
 *  the real mixer will plug into already in place. */
int Port_AudioInit(void);

/** Close SDL audio. Idempotent. */
void Port_AudioShutdown(void);

/**
 * Negotiated SDL audio sample rate in Hz, or `PORT_AUDIO_DEFAULT_RATE`
 * if the device has not been opened yet (`--mute`, `TMC_ENABLE_AUDIO=OFF`,
 * or when running before `Port_AudioInit()`). The host mixer uses this
 * to compute how many stereo frames it needs to produce per VBlank.
 *
 * Always reflects two channels of interleaved S16 samples — the only
 * format the SDL device is opened with.
 */
int Port_AudioGetSampleRate(void);

/**
 * Default SDL audio sample rate the device is opened with. Matches the
 * GBA m4a default of 13379 Hz so the existing engine timing translates
 * 1:1; `SDL_OpenAudioDevice` may negotiate a different value, which
 * `Port_AudioGetSampleRate()` will then reflect.
 */
#define PORT_AUDIO_DEFAULT_RATE 13379

/**
 * Push `frame_count` interleaved S16 stereo frames (so 2 * frame_count
 * samples) into the audio device's ring buffer. Producer-only; safe to
 * call from the game/audio thread (e.g. from `m4aSoundVSync` or its
 * host equivalent). If the ring is too full to take everything, the
 * tail is dropped — the audio device prefers an underflow-to-silence
 * over blocking the game thread. The drop counter is exposed via the
 * shared ring buffer header (`audio_ring.h`) for diagnostics.
 *
 * Returns the number of stereo frames actually accepted.
 */
int Port_AudioPushSamples(const int16_t* samples, int frame_count);

/**
 * Headless self-check for the audio ring buffer plumbing (PR #7
 * part 2.1). Exercises push / pull / overflow / underflow / wrap
 * paths without touching SDL, so it runs identically with or without
 * an open audio device. Saves and restores the ring state so it is
 * safe to call alongside the other smoke-test self-checks.
 *
 * Returns 0 on success, non-zero on any mismatch.
 */
int Port_AudioSelfCheck(void);

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
