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

void Port_m4aSoundInit(void) { /* silent */
}
void Port_m4aSoundMain(void) { /* silent */
}
void Port_m4aSoundVSync(void) { /* silent */
}
void Port_m4aSoundVSyncOn(void) { /* silent */
}
void Port_m4aSoundVSyncOff(void) { /* silent */
}
void Port_m4aSoundClear(void) { /* silent */
}
void Port_SoundReq(int song) {
    (void)song;
}

/* Unprefixed silent stubs for the m4a entry points that the game calls
 * directly (see include/gba/m4a.h). With TMC_LINK_GAME_SOURCES=ON
 * `src/sound.c` and friends pull these symbols in during boot. The
 * weak abort-trap in `port_unresolved_stubs.c` made AgbMain SIGABRT on
 * the first sound init; providing real (non-weak) silent stubs here
 * lets boot proceed past `InitSound`, while the matching ROM build
 * keeps using the real `src/gba/m4a.c`. PR #7 will replace this whole
 * file with the actual ported m4a engine. See docs/sdl_port.md
 * (PR #2b.4b runtime flip / PR #7). */
struct MusicPlayerInfo;
void m4aSoundInit(void) { /* silent */
}
void m4aSoundMain(void) { /* silent */
}
void m4aSoundVSync(void) { /* silent */
}
void m4aSoundVSyncOn(void) { /* silent */
}
void m4aSoundVSyncOff(void) { /* silent */
}
void m4aMPlayAllStop(void) { /* silent */
}
void m4aSongNumStart(unsigned short n) {
    (void)n;
}
void m4aSongNumStartOrContinue(unsigned short n) {
    (void)n;
}
void m4aSongNumStop(unsigned short n) {
    (void)n;
}
void m4aMPlayImmInit(struct MusicPlayerInfo* mp) {
    (void)mp;
}
void m4aMPlayTempoControl(struct MusicPlayerInfo* mp, unsigned short tempo) {
    (void)mp;
    (void)tempo;
}
void m4aMPlayVolumeControl(struct MusicPlayerInfo* mp, unsigned short trackBits, unsigned short volume) {
    (void)mp;
    (void)trackBits;
    (void)volume;
}
