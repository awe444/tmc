/**
 * @file m4a_stub.c
 * @brief Silent-mixer stand-ins for the GBA m4a sound engine — used as
 *        fallback when `src/gba/m4a.c` itself is not linked in.
 *
 * The real m4a code lives in src/gba/m4a.c; PR #7 part 1 brings it
 * into the SDL build (with the asm-only mixer routines stubbed
 * silently in src/platform/shared/m4a_host.c) when
 * `TMC_LINK_GAME_SOURCES=ON`. Under `TMC_LINK_GAME_SOURCES=OFF`
 * (early-bring-up scaffold for new ports), m4a.c is absent and the
 * stubs below — all marked `weak` so m4a.c's strong defs win when
 * present — keep the platform layer self-contained. PR #7 part 2 will
 * land the real host mixer that actually produces audio output via
 * src/platform/sdl/audio.c.
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
 * keeps using the real `src/gba/m4a.c`.
 *
 * As of PR #7 part 1, `src/gba/m4a.c` is itself compiled into
 * `tmc_game_sources` (with the asm-only mixer routines stubbed silently
 * in `src/platform/shared/m4a_host.c`). The real `m4a*` entry points
 * therefore exist as strong symbols in the game library; the stubs
 * below are kept as a fallback for the `TMC_LINK_GAME_SOURCES=OFF`
 * build (which doesn't link `src/gba/m4a.c`) and are marked `weak` so
 * the strong versions in m4a.c win at link time when both are present.
 * See docs/sdl_port.md (PR #7). */
struct MusicPlayerInfo;
__attribute__((weak)) void m4aSoundInit(void) { /* silent */
}
__attribute__((weak)) void m4aSoundMain(void) { /* silent */
}
__attribute__((weak)) void m4aSoundVSync(void) { /* silent */
}
__attribute__((weak)) void m4aSoundVSyncOn(void) { /* silent */
}
__attribute__((weak)) void m4aSoundVSyncOff(void) { /* silent */
}
__attribute__((weak)) void m4aMPlayAllStop(void) { /* silent */
}
__attribute__((weak)) void m4aSongNumStart(unsigned short n) {
    (void)n;
}
__attribute__((weak)) void m4aSongNumStartOrContinue(unsigned short n) {
    (void)n;
}
__attribute__((weak)) void m4aSongNumStop(unsigned short n) {
    (void)n;
}
__attribute__((weak)) void m4aMPlayImmInit(struct MusicPlayerInfo* mp) {
    (void)mp;
}
__attribute__((weak)) void m4aMPlayTempoControl(struct MusicPlayerInfo* mp, unsigned short tempo) {
    (void)mp;
    (void)tempo;
}
__attribute__((weak)) void m4aMPlayVolumeControl(struct MusicPlayerInfo* mp, unsigned short trackBits, unsigned short volume) {
    (void)mp;
    (void)trackBits;
    (void)volume;
}
