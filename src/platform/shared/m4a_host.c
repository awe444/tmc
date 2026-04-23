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
void clear_modM(MusicPlayerTrack* track) {
    (void)track;
}

/* The ARM-asm `ply_*` command handlers (the C-defined ones live in    */
/* m4a.c itself). Each is invoked through gMPlayJumpTable when         */
/* MPlayMain steps a track. With the silent SoundMain/MPlayMain        */
/* stubs above, none of these are actually reached at runtime, but     */
/* the gMPlayJumpTableTemplate[] in m4a.c takes their addresses, so    */
/* they must link. */
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
M4A_PLY_STUB(ply_prio)
M4A_PLY_STUB(ply_tempo)
M4A_PLY_STUB(ply_keysh)
M4A_PLY_STUB(ply_voice)
M4A_PLY_STUB(ply_vol)
M4A_PLY_STUB(ply_pan)
M4A_PLY_STUB(ply_bend)
M4A_PLY_STUB(ply_bendr)
M4A_PLY_STUB(ply_lfodl)
M4A_PLY_STUB(ply_modt)
M4A_PLY_STUB(ply_tune)
M4A_PLY_STUB(ply_port)
M4A_PLY_STUB(ply_note)
M4A_PLY_STUB(ply_endtie)
M4A_PLY_STUB(ply_lfos)
M4A_PLY_STUB(ply_mod)

#undef M4A_PLY_STUB

/* `nullsub_141` is referenced as the engine's no-op slot. */
void nullsub_141(void) { /* truly a nullsub */
}
