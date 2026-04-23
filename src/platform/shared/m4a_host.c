/**
 * @file m4a_host.c
 * @brief Host-side support for `src/gba/m4a.c` under the SDL port.
 *
 * `src/gba/m4a.c` is the C half of the m4a sound engine. The other half
 * lives in `asm/lib/m4a_asm.s` (the actual mixer: `SoundMain`,
 * `SoundMainRAM`, `MPlayMain`, the asm-defined `ply_*` command handlers,
 * `umul3232H32`, etc.) and the linker-script provides four "constant"
 * symbols whose *address* is the value (`gNumMusicPlayers = 0x20;`,
 * `gMaxLines = 0;`) plus a handful of EWRAM globals
 * (`gSoundInfo`, `gMPlayJumpTable`, `gCgbChans`, `gMPlayMemAccArea`)
 * placed at fixed offsets by `linker.ld`.
 *
 * The SDL port doesn't link `asm/lib/m4a_asm.s` and doesn't run the GBA
 * linker script. This file plugs both gaps so `src/gba/m4a.c` can be
 * compiled & linked into `tmc_sdl`:
 *
 *   1. Provides host BSS for the four EWRAM globals (sized for the host's
 *      pointer width — m4a.c owns the layout so the GBA struct-size
 *      asserts in `include/gba/m4a.h` are no-ops here per PR #2b.3 wave 1).
 *   2. Provides empty stand-in tables for `gMusicPlayers[]` / `gSongTable[]`
 *      (the real ones live in `data/sound/music_player_table.s` and
 *      `data/sound/song_table.s`, which are unbuilt under SDL). The
 *      `m4aSoundInit()` loop in `src/gba/m4a.c` walks `NUM_MUSIC_PLAYERS`
 *      entries; we redefine that macro to literal 0 in m4a.c under
 *      `__PORT__`, so neither table is dereferenced during init.
 *   3. Provides silent strong stubs for every asm-defined symbol that
 *      `src/gba/m4a.c` references. The C half can therefore link cleanly
 *      and `m4aSoundInit()` runs to completion; the result is that the
 *      `SoundInfo` struct is properly initialised, the m4a IO regs in
 *      `gPortIo` get the expected boot-time values, and every `m4aSong*`
 *      / `m4aMPlay*` entry point becomes a real (non-stub) function. No
 *      audio is produced yet — the actual mixer hookup is PR #7 part 2.
 *
 * The matching GBA ROM build is unaffected: this file is only compiled
 * into the SDL `tmc_sdl` target (see `CMakeLists.txt`).
 *
 * Tracking: `docs/sdl_port.md`, PR #7 part 1.
 */

#include "global.h"
#include "gba/m4a.h"

/* ------------------------------------------------------------------ */
/* (1) Host BSS for the four linker-script EWRAM globals.             */
/*                                                                    */
/* The real ROM build places these via `linker.ld`:                   */
/*     gSoundInfo       = 0x020043D0;                                 */
/*     gMPlayJumpTable  = 0x02004D50;                                 */
/*     gCgbChans        = 0x02004DE0;                                 */
/*     gMPlayMemAccArea = 0x020055E0;                                 */
/* On the host they're plain BSS of the appropriate type. Sizes are   */
/* taken straight from the type definitions in `include/gba/m4a.h`    */
/* (the m4a internal header). `gMPlayMemAccArea` is sized 256 bytes,  */
/* matching the 0x100-byte gap between it and the next linker symbol  */
/* in `linker.ld`.                                                    */
/* ------------------------------------------------------------------ */

/* Forward-declared in src/gba/m4a.c via the linker.ld symbol pattern.
 * On the GBA `extern u8 gMPlayMemAccArea[]` resolves to a chunk of EWRAM
 * sized by the linker script. The host owns the layout. */
u8 gMPlayMemAccArea[256];

/* gMPlayJumpTable[] holds 36 function-pointer entries (the 27 base ones
 * from `gMPlayJumpTableTemplate[]` defined in `src/gba/m4a.c`, plus the
 * 9 extender slots filled in by `MPlayExtender()`). Indices touched in
 * `src/gba/m4a.c`: [0] (ply_xxx), [1] (ply_memacc cond branch),
 * [8,17,19,28..33] (MPlayExtender), [34] (ClearChain), [35] (Clear64byte). */
void* gMPlayJumpTable[36];

/* CgbChannel and SoundInfo are declared in src/gba/m4a.c (file-local
 * `typedef struct ...`) and not in the public m4a.h header — so we can
 * neither size-check them here nor allocate the BSS by type. Declare
 * the engine names as plain byte arrays of generous, well-aligned size
 * (link-only deals in symbol names; the type is irrelevant once the
 * compiler is done). The matching `extern CgbChannel gCgbChans[];`
 * and `extern SoundInfo gSoundInfo;` declarations in m4a.c resolve
 * to the symbols below at link time.
 *
 * For `gCgbChans[4]`: the file-local `CgbChannel` in m4a.c is ~64
 * bytes (5 u32 trailing fields + many u8 fields). 256 bytes per
 * channel is generous.
 *
 * For `gSoundInfo`: the file-local `SoundInfo` in m4a.c contains a
 * 1584-byte `pcmBuffer[]` plus 12 `SoundChannel` structs (~48 bytes
 * each on the GBA, larger here because of the embedded pointers
 * widening to 8 bytes), plus header bytes and pointers. 4096 bytes
 * is comfortably above the host upper bound.
 *
 * NB: aligned(16) keeps the start of `pcmBuffer[]` (offset ~256
 * inside SoundInfo) suitably aligned for any future SIMD path the
 * host mixer might want; nothing in m4a.c relies on a specific
 * alignment beyond `u32`. */
u8 gCgbChans[4 * 256] __attribute__((aligned(16)));
u8 gSoundInfo[4096] __attribute__((aligned(16)));

/* ------------------------------------------------------------------ */
/* (2) Notes on the song / music-player tables.                       */
/*                                                                    */
/* The real `gMusicPlayers[]` and `gSongTable[]` definitions live in  */
/* `src/sound.c` (already in the leaf set since 2b.3 wave 1) and      */
/* point at per-player `MusicPlayerInfo` + `MusicPlayerTrack` arenas  */
/* whose host BSS is provided by the weak `gMPlayInfos` /             */
/* `gMPlayInfos2` / `gMPlayTracks` placeholders in                    */
/* `port_unresolved_stubs.c`. We don't touch those tables here.       */
/*                                                                    */
/* The only m4a.c caller that walks `gMusicPlayers` during boot is    */
/* `m4aSoundInit()`'s `for (i = 0; i < NUM_MUSIC_PLAYERS; i++)` loop. */
/* That loop is rendered no-op by redefining `NUM_MUSIC_PLAYERS` to   */
/* literal 0 under `__PORT__` in `src/gba/m4a.c` itself, so no entry  */
/* is dereferenced and the stale-MusicPlayerInfo BSS stays at zero    */
/* (matching freshly-erased EWRAM). The `m4aSongNum*` entry points    */
/* are never reached during the headless smoke test because           */
/* `src/sound.c::SoundReq` is gated by `gMain.unkA` / `gMain.unkE`    */
/* state machinery that doesn't activate before the title-screen      */
/* idle CI exercises.                                                 */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* (3) Silent strong stubs for the asm-defined symbols.               */
/*                                                                    */
/* Every symbol below is exported by `asm/lib/m4a_asm.s` on the GBA   */
/* (see the `thumb_func_start` / `arm_func_start` lines in that       */
/* file) and is referenced from `src/gba/m4a.c`. Each stub is silent  */
/* — m4a's data structures stay coherent because the C code never     */
/* observes the mixer's internal state, only the SoundInfo /          */
/* MusicPlayerInfo / MusicPlayerTrack memory it itself maintains.     */
/* PR #7 part 2 will replace the `SoundMain` / `MPlayMain` /          */
/* `ply_*` family with a real host C reimplementation.                */
/* ------------------------------------------------------------------ */

/* Forward-decl the m4a internal types we need parameter syntax for.  */
typedef struct MusicPlayerInfo MusicPlayerInfo;
typedef struct MusicPlayerTrack MusicPlayerTrack;

/* `extern char SoundMainRAM[]` in m4a.c is the source of a            */
/* `CpuCopy32(SoundMainRAM, SoundMainRAM_Buffer, 0x380)` at startup    */
/* — the GBA copies the SoundMainRAM thumb code into IWRAM for speed. */
/* Provide an all-zero source so the copy is harmless.                 */
char SoundMainRAM[0x380] __attribute__((aligned(4))) = { 0 };

/* The mixer entry points. */
void SoundMain(void) { /* PR #7 part 2: software mixer */
}

/* `SoundMainBTM` is dual-role on the GBA: as the `gMPlayJumpTable[35]`
 * target (see `gMPlayJumpTableTemplate[]` in m4a.c), it is invoked via
 * the `Clear64byte(void* addr)` C wrapper and its first action is to
 * zero exactly 64 bytes at `addr` before continuing into the bottom-half
 * mixer. The C callers (`MPlayOpen`, `m4aMPlayImmInit`) rely on that
 * 64-byte clear to initialise per-track state — leaving it as an empty
 * stub leaves `MusicPlayerInfo` / `MusicPlayerTrack` fields with stale
 * values from previous calls.
 *
 * Implement the 64-byte clear here (the bottom-half mixer step is left
 * for PR #7 part 2). The signature uses `void*` so the same function
 * can serve both call sites: SoundMain's argumentless invocation passes
 * a stale arg register that we ignore, and the Clear64byte path passes
 * the buffer pointer. */
void SoundMainBTM(void* addr) {
    if (addr != NULL) {
        u32* p = (u32*)addr;
        int i;
        for (i = 0; i < 16; i++) {
            p[i] = 0;
        }
    }
    /* PR #7 part 2: bottom-half mixer step goes here. */
}

/* The per-music-player main step routine — installed into            */
/* `soundInfo->MPlayMainHead` by `MPlayOpen` and walked by             */
/* `SoundMain`. With NUM_MUSIC_PLAYERS == 0 on the host it never       */
/* runs anyway, but it is taken by-address.                            */
void MPlayMain(void) { /* PR #7 part 2: walks active MusicPlayerInfo list */
}

/* `RealClearChain` is the `gMPlayJumpTable[34]` target invoked by the  */
/* `ClearChain(void* x)` C wrapper. On the GBA the asm walks a linked   */
/* list of channels rooted at `x` and unlinks each one. Under the       */
/* silent host mixer no channel is ever linked into the list, so the    */
/* unlink-walk has nothing to do; the safe stand-in is a no-op.         */
void RealClearChain(void* x) {
    (void)x;
}

/* `MPlayJumpTableCopy` populates `gMPlayJumpTable[]` from the          */
/* `gMPlayJumpTableTemplate[]` defined in m4a.c. The asm impl walks    */
/* `template -> gMPlayJumpTable`, 36 entries. Re-implement on host.    */
extern void* const gMPlayJumpTableTemplate[];
void MPlayJumpTableCopy(void** mplayJumpTable) {
    int i;
    for (i = 0; i < 36; i++) {
        mplayJumpTable[i] = gMPlayJumpTableTemplate[i];
    }
}

/* `umul3232H32(a, b)` returns the high 32 bits of (u64)a * (u64)b;     */
/* it is the cornerstone of `MidiKeyToFreq`. The C reimplementation is */
/* trivial — and unlike the rest of these stubs it is _not_ silent: it */
/* needs to compute a real value because `MidiKeyToFreq` returns its   */
/* result and the engine uses it later. It's only ever called from     */
/* `MidiKeyToFreq`, which is itself only called from asm `ply_note`    */
/* (silent below) and from `SoundMain` (silent above). */
u32 umul3232H32(u32 multiplier, u32 multiplicand) {
    return (u32)(((u64)multiplier * (u64)multiplicand) >> 32);
}

/* `TrackStop` / `ChnVolSetAsm` / `clear_modM` — installed in           */
/* gMPlayJumpTable or called directly from m4a.c.                       */
void TrackStop(MusicPlayerInfo* mplayInfo, MusicPlayerTrack* track) {
    (void)mplayInfo;
    (void)track;
}
void ChnVolSetAsm(void) { /* PR #7 part 2 */
}
/* `clear_modM` is implemented as part of the PR #7 part 2.2.1 ply_*    */
/* family below (it is invoked by ply_lfos / ply_mod).                  */

/* The ARM-asm `ply_*` command handlers (the C-defined ones live in    */
/* m4a.c itself). Each is invoked through gMPlayJumpTable when         */
/* MPlayMain steps a track.                                            */
/*                                                                     */
/* PR #7 part 2.2.1 promotes the simple parameter-setting handlers     */
/* from no-op stubs to real C ports of the asm in `asm/lib/m4a_asm.s`. */
/* The remaining handlers (`ply_fine`, `ply_goto`, `ply_patt`,         */
/* `ply_pend`, `ply_rept`, `ply_voice`, `ply_note`, `ply_port`) need   */
/* `MPlayMain`'s surrounding state (channel-list walking, ROM-address  */
/* loads against gSongTable, CGB register pokes) and remain stubs      */
/* until PR #7 parts 2.2.2 / 2.3 land.                                 */
/*                                                                     */
/* `MPlayMain` is itself a no-op stub and `NUM_MUSIC_PLAYERS == 0`     */
/* under `__PORT__` (see src/gba/m4a.c), so none of these handlers is  */
/* reached at runtime today. They are instead exercised in isolation   */
/* by `Port_M4ASelfCheck()` below, which feeds each handler a          */
/* synthesized cmdPtr byte stream and verifies the resulting track     */
/* state against the asm semantics in `asm/lib/m4a_asm.s`.             */

/* MusicPlayerTrack flag bits used by the ply_* handlers (mirrored from */
/* include/gba/m4a.h's MPT_FLG_*).                                      */
#define M4A_FLG_VOLCHG MPT_FLG_VOLCHG /* 0x03 */
#define M4A_FLG_PITCHG MPT_FLG_PITCHG /* 0x0C */
#define M4A_FLG_MODCHG (MPT_FLG_VOLCHG | MPT_FLG_PITCHG) /* 0x0F */

/* Read a single command byte from track->cmdPtr and advance the pointer.
 * The asm equivalent (`sub_080AF778` / `sub_080AF77A`) is the workhorse
 * read-byte primitive used by every parameter-setting handler. */
static u8 m4a_consume_byte(MusicPlayerTrack* track) {
    u8 b = *track->cmdPtr;
    track->cmdPtr++;
    return b;
}

/* `ply_prio` — set track priority (1 byte). Asm: strb r3, [r1, #0x1d]. */
void ply_prio(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    (void)mp;
    track->priority = m4a_consume_byte(track);
}

/* `ply_tempo` — set song tempo (1 byte; the byte is the tempo / 2 in BPM
 * so the asm doubles it before storing). Writes three MusicPlayerInfo
 * fields: tempoD = byte * 2; tempoI = (tempoD * tempoU) >> 8. */
void ply_tempo(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    u32 tempoD = (u32)m4a_consume_byte(track) << 1;
    mp->tempoD = (u16)tempoD;
    mp->tempoI = (u16)((tempoD * (u32)mp->tempoU) >> 8);
}

/* `ply_keysh` — set track keyShift (1 byte). Sets PITCHG so the next
 * MPlayMain pass recomputes the channel frequency. */
void ply_keysh(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    (void)mp;
    track->keyShift = (s8)m4a_consume_byte(track);
    track->flags |= M4A_FLG_PITCHG;
}

/* `ply_vol` — set track volume (1 byte, 0..127). Sets VOLCHG. */
void ply_vol(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    (void)mp;
    track->vol = m4a_consume_byte(track);
    track->flags |= M4A_FLG_VOLCHG;
}

/* `ply_pan` — set track pan (1 byte, encoded as 0x40 + signed pan).
 * Asm subtracts 0x40 to recover the signed value. Sets VOLCHG. */
void ply_pan(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    (void)mp;
    track->pan = (s8)(m4a_consume_byte(track) - 0x40);
    track->flags |= M4A_FLG_VOLCHG;
}

/* `ply_bend` — set track pitch bend (1 byte, encoded 0x40 + signed).
 * Sets PITCHG. */
void ply_bend(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    (void)mp;
    track->bend = (s8)(m4a_consume_byte(track) - 0x40);
    track->flags |= M4A_FLG_PITCHG;
}

/* `ply_bendr` — set track bend range (1 byte, semitones). Sets PITCHG. */
void ply_bendr(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    (void)mp;
    track->bendRange = m4a_consume_byte(track);
    track->flags |= M4A_FLG_PITCHG;
}

/* `ply_lfodl` — set LFO delay (1 byte). No flag-setting in the asm. */
void ply_lfodl(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    (void)mp;
    track->lfoDelay = m4a_consume_byte(track);
}

/* `ply_modt` — set modulation type (1 byte: 0 = pitch, 1 = vol, 2 = pan).
 * Only sets MODCHG (= VOLCHG|PITCHG) when the value actually changes,
 * matching the asm's `cmp r0, r3 / beq` branch. */
void ply_modt(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    (void)mp;
    u8 v = m4a_consume_byte(track);
    if (track->modT != v) {
        track->modT = v;
        track->flags |= M4A_FLG_MODCHG;
    }
}

/* `ply_tune` — set fine tune (1 byte, encoded 0x40 + signed). Sets PITCHG. */
void ply_tune(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    (void)mp;
    track->tune = (s8)(m4a_consume_byte(track) - 0x40);
    track->flags |= M4A_FLG_PITCHG;
}

/* `clear_modM` — invoked from ply_lfos / ply_mod when the new value is
 * zero. Resets the modulation accumulator and tags the next MPlayMain
 * pass with PITCHG (modT == 0) or VOLCHG (modT != 0). */
void clear_modM(MusicPlayerTrack* track) {
    track->modM = 0;
    track->lfoSpeedC = 0;
    if (track->modT == 0) {
        track->flags |= M4A_FLG_PITCHG;
    } else {
        track->flags |= M4A_FLG_VOLCHG;
    }
}

/* `ply_lfos` — set LFO speed (1 byte). On zero, also call clear_modM. */
void ply_lfos(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    (void)mp;
    u8 v = m4a_consume_byte(track);
    track->lfoSpeed = v;
    if (v == 0) {
        clear_modM(track);
    }
}

/* `ply_mod` — set modulation depth (1 byte). On zero, call clear_modM. */
void ply_mod(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    (void)mp;
    u8 v = m4a_consume_byte(track);
    track->mod = v;
    if (v == 0) {
        clear_modM(track);
    }
}

/* `ply_endtie` — end a tied note. The first byte (if < 0x80) is the
 * key to release; otherwise the previous track->key is reused without
 * advancing cmdPtr. The asm then walks track->chan looking for an
 * envelope-active channel matching that key and sets bit 6 (release).
 *
 * Under the silent host mixer no SoundChannel is ever linked into the
 * track->chan list (MPlayMain is itself a no-op), so the channel walk
 * is unreachable. Implement only the cmdPtr / track->key half here;
 * the channel-walk half lands in PR #7 part 2.2.2 alongside MPlayMain. */
void ply_endtie(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    (void)mp;
    u8 b = *track->cmdPtr;
    if (b < 0x80) {
        track->key = b;
        track->cmdPtr++;
    }
    /* Channel walk deferred to PR #7 part 2.2.2; track->chan is NULL
     * under the silent host mixer so the GBA loop is a no-op anyway. */
}

/* The remaining handlers stay no-op stubs until PR #7 part 2.2.2 lands
 * MPlayMain and the channel-list / pattern-stack / ROM-address machinery
 * they need. */
#define M4A_PLY_STUB(name)                                 \
    void name(MusicPlayerInfo* mp, MusicPlayerTrack* tr) { \
        (void)mp;                                          \
        (void)tr;                                          \
    }

M4A_PLY_STUB(ply_fine)
M4A_PLY_STUB(ply_goto)
M4A_PLY_STUB(ply_patt)
M4A_PLY_STUB(ply_pend)
M4A_PLY_STUB(ply_rept)
M4A_PLY_STUB(ply_voice)
M4A_PLY_STUB(ply_port)
M4A_PLY_STUB(ply_note)

#undef M4A_PLY_STUB

/* `nullsub_141` is referenced as the engine's no-op slot. */
void nullsub_141(void) { /* truly a nullsub */
}

/* ------------------------------------------------------------------ */
/* (4) PR #7 part 2.2.1 self-check.                                   */
/*                                                                    */
/* Builds a stack-allocated MusicPlayerInfo + MusicPlayerTrack and    */
/* drives each implemented ply_* handler with a synthesized cmdPtr    */
/* byte stream, asserting that the right field is updated, the right */
/* flag bits are set, and cmdPtr advanced by exactly one byte.       */
/*                                                                    */
/* This is the m4a equivalent of `Port_RendererSelfCheck()` /         */
/* `Port_AudioSelfCheck()` and runs from `src/platform/sdl/main.c`    */
/* on every smoke-test invocation, so any future regression in the    */
/* command-byte / field-offset / flag-bit contracts surfaces in CI.   */
/* ------------------------------------------------------------------ */

#define M4A_CHECK(cond)                                                            \
    do {                                                                           \
        if (!(cond)) {                                                             \
            fprintf(stderr,                                                        \
                    "[Port_M4ASelfCheck] failed at %s:%d: %s\n",                   \
                    __FILE__, __LINE__, #cond);                                    \
            return 1;                                                              \
        }                                                                          \
    } while (0)

#include <stdio.h>
#include <string.h>

int Port_M4ASelfCheck(void) {
    MusicPlayerInfo mp;
    MusicPlayerTrack track;
    u8 stream[8];

    /* Helper macro: prime mp/track, point cmdPtr at `stream`, write
     * `byte` into stream[0], call `handler`, then assert cmdPtr
     * advanced by exactly one byte. */
    #define M4A_RUN(handler, byte)                                              \
        do {                                                                    \
            memset(&mp, 0, sizeof(mp));                                         \
            memset(&track, 0, sizeof(track));                                   \
            stream[0] = (u8)(byte);                                             \
            track.cmdPtr = stream;                                              \
            handler(&mp, &track);                                               \
            M4A_CHECK(track.cmdPtr == stream + 1);                              \
        } while (0)

    /* ply_prio: stores raw byte into track->priority. */
    M4A_RUN(ply_prio, 0x42);
    M4A_CHECK(track.priority == 0x42);
    M4A_CHECK(track.flags == 0); /* no flag side-effects */

    /* ply_keysh: stores raw byte (signed) into keyShift, sets PITCHG. */
    M4A_RUN(ply_keysh, 0xFE); /* -2 */
    M4A_CHECK(track.keyShift == (s8)-2);
    M4A_CHECK((track.flags & M4A_FLG_PITCHG) == M4A_FLG_PITCHG);

    /* ply_vol: stores raw byte into vol, sets VOLCHG. */
    M4A_RUN(ply_vol, 0x7F);
    M4A_CHECK(track.vol == 0x7F);
    M4A_CHECK((track.flags & M4A_FLG_VOLCHG) == M4A_FLG_VOLCHG);

    /* ply_pan: subtracts 0x40 to recover signed pan, sets VOLCHG. */
    M4A_RUN(ply_pan, 0x40); /* -> 0 */
    M4A_CHECK(track.pan == 0);
    M4A_CHECK((track.flags & M4A_FLG_VOLCHG) == M4A_FLG_VOLCHG);
    M4A_RUN(ply_pan, 0x80); /* -> +0x40 */
    M4A_CHECK(track.pan == (s8)0x40);
    M4A_RUN(ply_pan, 0x00); /* -> -0x40 */
    M4A_CHECK(track.pan == (s8)-0x40);

    /* ply_bend: signed (byte - 0x40), PITCHG. */
    M4A_RUN(ply_bend, 0x44); /* +4 */
    M4A_CHECK(track.bend == 4);
    M4A_CHECK((track.flags & M4A_FLG_PITCHG) == M4A_FLG_PITCHG);

    /* ply_bendr: raw byte, PITCHG. */
    M4A_RUN(ply_bendr, 0x0C);
    M4A_CHECK(track.bendRange == 0x0C);
    M4A_CHECK((track.flags & M4A_FLG_PITCHG) == M4A_FLG_PITCHG);

    /* ply_lfodl: raw byte, no flag side-effect. */
    M4A_RUN(ply_lfodl, 0x18);
    M4A_CHECK(track.lfoDelay == 0x18);
    M4A_CHECK(track.flags == 0);

    /* ply_modt: writes only when value changes; sets MODCHG. */
    M4A_RUN(ply_modt, 0x01); /* track->modT was 0, now 1 */
    M4A_CHECK(track.modT == 1);
    M4A_CHECK((track.flags & M4A_FLG_MODCHG) == M4A_FLG_MODCHG);
    /* Now run with same value: must not set MODCHG. */
    memset(&mp, 0, sizeof(mp));
    memset(&track, 0, sizeof(track));
    track.modT = 1;
    stream[0] = 1;
    track.cmdPtr = stream;
    ply_modt(&mp, &track);
    M4A_CHECK(track.cmdPtr == stream + 1);
    M4A_CHECK(track.flags == 0); /* unchanged */

    /* ply_tune: signed (byte - 0x40), PITCHG. */
    M4A_RUN(ply_tune, 0x40); /* -> 0 */
    M4A_CHECK(track.tune == 0);
    M4A_CHECK((track.flags & M4A_FLG_PITCHG) == M4A_FLG_PITCHG);

    /* ply_lfos: nonzero -> just store; zero -> also clear_modM. */
    M4A_RUN(ply_lfos, 0x10);
    M4A_CHECK(track.lfoSpeed == 0x10);
    M4A_CHECK(track.flags == 0); /* nonzero path: no flag */
    /* Zero path with modT == 0: clear_modM should set PITCHG. */
    memset(&mp, 0, sizeof(mp));
    memset(&track, 0, sizeof(track));
    track.modM = 5;
    track.lfoSpeedC = 7;
    stream[0] = 0;
    track.cmdPtr = stream;
    ply_lfos(&mp, &track);
    M4A_CHECK(track.cmdPtr == stream + 1);
    M4A_CHECK(track.lfoSpeed == 0);
    M4A_CHECK(track.modM == 0);
    M4A_CHECK(track.lfoSpeedC == 0);
    M4A_CHECK((track.flags & M4A_FLG_PITCHG) == M4A_FLG_PITCHG);
    /* Zero path with modT == 1: clear_modM should set VOLCHG. */
    memset(&mp, 0, sizeof(mp));
    memset(&track, 0, sizeof(track));
    track.modT = 1;
    stream[0] = 0;
    track.cmdPtr = stream;
    ply_lfos(&mp, &track);
    M4A_CHECK((track.flags & M4A_FLG_VOLCHG) == M4A_FLG_VOLCHG);

    /* ply_mod: same shape as ply_lfos. */
    M4A_RUN(ply_mod, 0x20);
    M4A_CHECK(track.mod == 0x20);
    M4A_CHECK(track.flags == 0);
    memset(&mp, 0, sizeof(mp));
    memset(&track, 0, sizeof(track));
    track.modM = 9;
    stream[0] = 0;
    track.cmdPtr = stream;
    ply_mod(&mp, &track);
    M4A_CHECK(track.mod == 0);
    M4A_CHECK(track.modM == 0);
    M4A_CHECK((track.flags & M4A_FLG_PITCHG) == M4A_FLG_PITCHG);

    /* ply_tempo: tempoD = byte * 2; tempoI = (tempoD * tempoU) >> 8. */
    memset(&mp, 0, sizeof(mp));
    memset(&track, 0, sizeof(track));
    mp.tempoU = 0x100; /* unit scale */
    stream[0] = 0x96;  /* 150 */
    track.cmdPtr = stream;
    ply_tempo(&mp, &track);
    M4A_CHECK(track.cmdPtr == stream + 1);
    M4A_CHECK(mp.tempoD == 0x12C); /* 150 * 2 = 300 */
    M4A_CHECK(mp.tempoI == 0x12C); /* (0x12C * 0x100) >> 8 = 0x12C */
    /* Verify the multiplication path with a different tempoU. */
    memset(&mp, 0, sizeof(mp));
    memset(&track, 0, sizeof(track));
    mp.tempoU = 0x80; /* half scale */
    stream[0] = 0x40;
    track.cmdPtr = stream;
    ply_tempo(&mp, &track);
    M4A_CHECK(mp.tempoD == 0x80);
    M4A_CHECK(mp.tempoI == 0x40); /* (0x80 * 0x80) >> 8 = 0x40 */

    /* ply_endtie: < 0x80 consumes one byte and sets track->key;
     * >= 0x80 leaves cmdPtr untouched and key unchanged. */
    memset(&mp, 0, sizeof(mp));
    memset(&track, 0, sizeof(track));
    stream[0] = 0x3C;
    track.cmdPtr = stream;
    track.key = 0x55;
    ply_endtie(&mp, &track);
    M4A_CHECK(track.cmdPtr == stream + 1);
    M4A_CHECK(track.key == 0x3C);
    /* >= 0x80 path. */
    memset(&track, 0, sizeof(track));
    stream[0] = 0x80;
    track.cmdPtr = stream;
    track.key = 0x42;
    ply_endtie(&mp, &track);
    M4A_CHECK(track.cmdPtr == stream); /* not advanced */
    M4A_CHECK(track.key == 0x42);      /* unchanged */

    /* clear_modM standalone: when modT != 0, sets VOLCHG only. */
    memset(&track, 0, sizeof(track));
    track.modT = 1;
    track.modM = 7;
    track.lfoSpeedC = 9;
    clear_modM(&track);
    M4A_CHECK(track.modM == 0);
    M4A_CHECK(track.lfoSpeedC == 0);
    M4A_CHECK((track.flags & M4A_FLG_VOLCHG) == M4A_FLG_VOLCHG);
    M4A_CHECK((track.flags & M4A_FLG_PITCHG) == 0);

    #undef M4A_RUN

    return 0;
}

#undef M4A_CHECK
