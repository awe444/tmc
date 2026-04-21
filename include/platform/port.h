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
#define PORT_VRAM_SIZE  0x18000u
#define PORT_OAM_SIZE   0x00400u
#define PORT_PLTT_SIZE  0x00400u
#define PORT_IO_SIZE    0x00400u

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
#define PORT_FRAME_NS          16743055  /* 1e9 / 59.7274 */

/** Block until the next emulated VBlank. Replacement for VBlankIntrWait. */
void Port_VBlankIntrWait(void);

/** Called from the SDL frame loop when VBlank fires. */
void Port_OnVBlank(void);

/** Returns non-zero once the user has requested shutdown (SDL_QUIT, etc.). */
int  Port_ShouldQuit(void);

/** Set the quit flag. Wakes any thread parked in Port_VBlankIntrWait. */
void Port_RequestQuit(void);

/**
 * Auto-quit after this many VBlanks (used for the CI smoke test:
 * --frames=N on the command line). 0 disables the budget.
 */
void Port_SetFrameBudget(int frames);

/* ------------------------------------------------------------------------ */
/* Input.                                                                   */
/*                                                                          */
/* The host writes a 10-bit GBA-format key bitmask into a slot that PR #2   */
/* will alias as REG_KEYINPUT (active-low, like the hardware register).     */
/* ------------------------------------------------------------------------ */

/** Initialize keyboard + gamepad subsystems. Returns 0 on success. */
int  Port_InputInit(void);

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
int  Port_VideoInit(int scale, int fullscreen);

/** Tear down the SDL window. */
void Port_VideoShutdown(void);

/**
 * Present the current emulated framebuffer. PR #4-5 will replace the
 * placeholder solid-color fill with the real BG/OBJ rasterizer.
 */
void Port_VideoPresent(void);

/* ------------------------------------------------------------------------ */
/* Audio.                                                                   */
/* ------------------------------------------------------------------------ */

/** Open SDL audio (silent stub mixer for now). */
int  Port_AudioInit(void);

/** Close SDL audio. */
void Port_AudioShutdown(void);

/* ------------------------------------------------------------------------ */
/* Save backend.                                                            */
/* ------------------------------------------------------------------------ */

#define PORT_SAVE_SIZE 0x10000u  /* 64 KiB Flash layout */

/** Load `tmc.sav` from disk into the emulated Flash buffer. */
int  Port_SaveLoad(const char* save_dir);

/** Flush the emulated Flash buffer back out to disk. */
int  Port_SaveFlush(void);

/** Direct read access to a byte in the emulated Flash. */
uint8_t Port_SaveReadByte(uint32_t offset);

/** Direct write access to a byte in the emulated Flash. */
void    Port_SaveWriteByte(uint32_t offset, uint8_t value);

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
}  /* extern "C" */
#endif

#endif /* TMC_PLATFORM_PORT_H */
