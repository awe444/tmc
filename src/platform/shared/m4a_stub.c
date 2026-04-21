/**
 * @file m4a_stub.c
 * @brief Silent-mixer stand-ins for the GBA m4a sound engine.
 *
 * The real m4a code lives in src/gba/m4a.c and is heavily intertwined
 * with the GBA's sound hardware (REG_SOUNDCNT_*, the sound FIFOs, timer
 * interrupts on HBlank). Bringing it up under SDL is a substantial PR of
 * its own (see roadmap PR #9). This file provides no-op replacements so
 * the rest of the port can be built and exercised in the meantime.
 *
 * The actual SDL audio device open/close lives in src/platform/sdl/audio.c
 * — this file is platform-agnostic so it can be reused by future ports.
 */
#include "platform/port.h"

/* These names match the public surface of the m4a library used by the
 * game (see include/gba/m4a.h, src/sound.c). They are exported with the
 * "Port_" prefix to keep the names clearly host-side; PR #2 will alias
 * the unprefixed names to these stubs when audio is disabled and to the
 * real implementation when it is enabled. */

void Port_m4aSoundInit(void)        { /* silent */ }
void Port_m4aSoundMain(void)        { /* silent */ }
void Port_m4aSoundVSync(void)       { /* silent */ }
void Port_m4aSoundVSyncOn(void)     { /* silent */ }
void Port_m4aSoundVSyncOff(void)    { /* silent */ }
void Port_m4aSoundClear(void)       { /* silent */ }
void Port_SoundReq(int song)        { (void)song; }
