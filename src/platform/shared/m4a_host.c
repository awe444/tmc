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
/* runs anyway, but it is exercised in isolation by                    */
/* `Port_M4ASelfCheck()` below. PR #7 part 2.2.2.2.1 promotes this    */
/* from a no-op stub to a real port of `asm/lib/m4a_asm.s`'s          */
/* MPlayMain top half (tempo accumulator + per-track command          */
/* dispatcher + LFO modulation tick). The per-track second loop       */
/* (TrkVolPitSet + chan walk) and the `ply_voice` / `ply_note`        */
/* handlers are still gated behind the next two substeps               */
/* (2.2.2.2.2 / 2.2.2.2.3); ply_note dispatches at runtime through    */
/* the still-stubbed `ply_note` C function and the post-dispatch      */
/* second loop is intentionally not yet executed.                      */

/* MPlayMain bit layout (mirrored from include/gba/m4a.h /            */
/* asm/lib/m4a_asm.s).                                                */
#define M4A_STATUS_OFF                                                                                                \
    0x80000000u /* mp->status sign bit: no tracks were serviced in this tick (no track had EXIST when iterated). A    \
                   track that kills itself via ply_fine still has its bit OR'd into acc for the current tick and only \
                   flips to OFF on the next call. */
#define M4A_TEMPO_TICK 150          /* tempoC subtract per inner-loop tick */
#define M4A_FLG_START MPT_FLG_START /* 0x40 */
#define M4A_FLG_EXIST MPT_FLG_EXIST /* 0x80 */
#define M4A_RUNSTAT_LO 0xBD         /* runningStatus is updated only for b >= 0xBD */
#define M4A_NOTE_BASE 0xCF          /* b >= 0xCF: ply_note with gateIdx = b - 0xCF */
#define M4A_WAIT_BASE 0x80          /* 0x80 <= b <= 0xB0: wait command */
#define M4A_JUMP_BASE 0xB1          /* 0xB1 <= b <= 0xCE: gMPlayJumpTable[b - 0xB1] */
#define M4A_JUMP_END 0xB0           /* boundary: b > 0xB0 enters the jump-table path */

/* gClockTable[] (defined in src/gba/m4a.c) is the MIDI-tick-count
 * table indexed by (waitCmd - 0x80). Imported for the dispatcher's
 * `track->wait = gClockTable[b - 0x80]` step. */
extern const u8 gClockTable[];

/* `ID_NUMBER` is m4a's internal sentinel stored in `mp->ident` /
 * `soundInfo->ident` to confirm the struct hasn't been zeroed by a
 * cold reset. Defined in src/gba/m4a.c as the multi-character
 * constant `'hsmS'` (= 0x68736D53). Mirror the value here. */
#define M4A_ID_NUMBER 0x68736D53u

/* `ClearChain` and `FadeOutBody` are defined in src/gba/m4a.c.
 * Forward-declare them so MPlayMain can call them from this TU. */
void ClearChain(void* x);
void FadeOutBody(MusicPlayerInfo* mp);

/* Forward decl for `ply_note` / `ply_voice` / `ply_note_impl`.
 * `ply_voice` was promoted to a real C port in PR #7 part 2.2.2.2.2.1
 * and `ply_note` was promoted in PR #7 part 2.2.2.2.2.2. The public
 * 2-arg `ply_note(mp, track)` symbol is the one stored into
 * `soundInfo->plynote` by `m4a.c::SoundInit`; it forwards into the
 * host-private 3-arg `ply_note_impl(mp, track, gateIdx)` with a
 * default `gateIdx = 0` (the asm reads `gClockTable[gateIdx]` into
 * `track->gateTime`, which only matters when the dispatcher reaches
 * `ply_note` via a `>= 0xCF` opcode encoding the gate index — every
 * other call path observes the no-op `gClockTable[0] = 0`). The
 * dispatcher in `m4a_dispatch_one` calls `ply_note_impl` directly so
 * the gate index extracted from the command byte survives the
 * 2-arg public signature. */
void ply_note(MusicPlayerInfo* mp, MusicPlayerTrack* track);
void ply_note_impl(MusicPlayerInfo* mp, MusicPlayerTrack* track, u32 gateIdx);
void ply_voice(MusicPlayerInfo* mp, MusicPlayerTrack* track);

/* SoundChannel statusFlags bits referenced by the chan walks. The
 * fuller comment is in the duplicate definition further down (kept
 * with the ply_fine / ply_endtie code from 2.2.2.1). They are
 * forward-defined here because the chan-walk helpers above need
 * them, and because the macros are hard-coded to bit patterns
 * dictated by m4a.c's CgbChannel / SoundChannel state machine —
 * not an internal-to-this-file assumption. */
#ifndef M4A_CHAN_FLAGS_ENV
#define M4A_CHAN_FLAGS_ENV 0xc7
#define M4A_CHAN_FLAGS_EXIST 0x83
#define M4A_CHAN_FLAG_RELEASE 0x40
#endif

/* The asm calls `Clear64byte(track)` on the START path. m4a.c's C
 * wrapper just calls `gMPlayJumpTable[35]`, which dispatches into our
 * SoundMainBTM stub. That stub already zeroes 64 bytes — but on the
 * host MusicPlayerTrack is much larger than 64 bytes (because of the
 * embedded ToneData + cmdPtr / patternStack pointers widening past
 * the GBA's 4-byte width). The asm relies on the fact that the
 * subsequent `track->flags = 0x80` / `track->wait = 2` /
 * `track->pitX = 0x40` / `track->lfoSpeed = 0x16` writes happen
 * *after* the 64-byte clear — so on the GBA the clear wipes the
 * dispatcher state (flags, wait, ...) and the writes restore the
 * ones MPlayMain needs.
 *
 * On the host we mirror that semantics using the public field names
 * directly so we don't depend on either the host's struct layout or
 * the 64-byte boundary of Clear64byte. */
static void m4a_track_start_init(MusicPlayerTrack* track) {
    /* asm clears bytes [0..0x40) on the GBA. We clear the equivalent
     * dispatcher-state fields by name, leaving cmdPtr / patternStack
     * (used by the dispatcher!) untouched — the asm preserves them
     * implicitly because they live past offset 0x40, which mirrors
     * what we're doing. */
    track->flags = 0;
    track->wait = 0;
    track->patternLevel = 0;
    track->repN = 0;
    track->gateTime = 0;
    track->key = 0;
    track->velocity = 0;
    track->runningStatus = 0;
    track->keyM = 0;
    track->pitM = 0;
    track->keyShift = 0;
    track->keyShiftX = 0;
    track->tune = 0;
    track->pitX = 0;
    track->bend = 0;
    track->bendRange = 0;
    track->volMR = 0;
    track->volML = 0;
    track->vol = 0;
    track->volX = 0;
    track->pan = 0;
    track->panX = 0;
    track->modM = 0;
    track->mod = 0;
    track->modT = 0;
    track->lfoSpeed = 0;
    track->lfoSpeedC = 0;
    track->lfoDelay = 0;
    track->lfoDelayC = 0;
    track->priority = 0;
    track->echoVolume = 0;
    track->echoLength = 0;
    track->chan = NULL;
    /* Clear the embedded ToneData so a reused track doesn't leak
     * stale wav / ADSR / key bytes into the next ply_voice /
     * ply_note pass. The asm's Clear64byte covers the first 64 bytes
     * starting at the track base, which on the GBA includes the
     * full embedded ToneData (it sits at offset 0x24 and is 0xc
     * bytes wide). On the host we just zero by name. */
    track->tone.type = 0;
    track->tone.key = 0;
    track->tone.length = 0;
    track->tone.pan_sweep = 0;
    track->tone.wav = NULL;
    track->tone.attack = 0;
    track->tone.decay = 0;
    track->tone.sustain = 0;
    track->tone.release = 0;
    /* Now the asm-matching defaults: */
    track->flags = M4A_FLG_EXIST;
    track->wait = 2;
    track->pitX = 0x40;
    track->lfoSpeed = 0x16;
    /* asm: `adds r1, r5, #6; strb r0, [r1, #0x1e]` — that hits
     * `(u8*)track + 0x24`, which is the first byte of the embedded
     * ToneData (`tone.type`). Set it to 1. */
    track->tone.type = 1;
}

/* Walk the track's chan list and tick the gate-time / release flag
 * machinery. Mirrors asm `_080AF960..._080AF9A8` (the chan-walk
 * portion of the per-track inner loop). Under the silent host
 * mixer track->chan is always NULL, so this is a runtime no-op.
 * Exercised by Port_M4ASelfCheck() against synthesized SoundChannel
 * lists. */
static void m4a_chan_gate_tick(MusicPlayerTrack* track) {
    SoundChannel* chan = track->chan;
    while (chan != NULL) {
        u8 status = chan->statusFlags;
        if (status & M4A_CHAN_FLAGS_ENV) {
            /* gateTime != 0 => decrement; on transition to 0, set
             * release flag. */
            u8 gate = chan->gateTime;
            if (gate != 0) {
                gate--;
                chan->gateTime = gate;
                if (gate == 0) {
                    chan->statusFlags = (u8)(status | M4A_CHAN_FLAG_RELEASE);
                }
            }
        } else {
            /* envelope dead — unlink the channel from the chain. */
            ClearChain(chan);
        }
        SoundChannel* next = (SoundChannel*)(uintptr_t)chan->next;
        if (next == chan) {
            chan->next = 0;
            next = NULL;
        }
        chan = next;
    }
}

/* The LFO modulation tick that follows the wait-decrement step.
 * Mirrors asm `_080AFA34..._080AFA82`. Returns void; mutates
 * track->modM, track->lfoSpeedC, track->lfoDelayC, and (when modM
 * crosses) track->flags' MPT_FLG_VOLCHG / MPT_FLG_PITCHG bits. */
static void m4a_track_lfo_tick(MusicPlayerTrack* track) {
    if (track->lfoSpeed == 0 || track->mod == 0) {
        return;
    }
    if (track->lfoDelayC != 0) {
        track->lfoDelayC--;
        return;
    }
    /* lfoSpeedC += lfoSpeed (8-bit wrap). */
    u8 lsc = (u8)(track->lfoSpeedC + track->lfoSpeed);
    track->lfoSpeedC = lsc;
    /* Triangle wave: |0x80 - lsc| with sign convention from the asm.
     *   if (s8)(lsc - 0x40) < 0  -> r2 = (s8)lsc
     *   else                     -> r2 = 0x80 - lsc
     * Then newModM = (s8)((mod * r2) >> 6). The asm stores the low
     * 8 bits via strb. */
    s32 r2;
    if ((s8)(lsc - 0x40) < 0) {
        r2 = (s8)lsc;
    } else {
        r2 = 0x80 - (s32)lsc;
    }
    s32 newModM_full = (s32)track->mod * r2;
    s8 newModM = (s8)(newModM_full >> 6);
    if ((s8)track->modM == newModM) {
        return;
    }
    track->modM = newModM;
    if (track->modT == 0) {
        track->flags |= MPT_FLG_PITCHG;
    } else {
        track->flags |= MPT_FLG_VOLCHG;
    }
}

/* Dispatch one command byte: read at track->cmdPtr (or use the
 * runningStatus shortcut for b < 0x80), advance cmdPtr if a real
 * opcode was consumed, and route to the appropriate handler. Returns
 * non-zero if the track was killed (track->flags == 0 after dispatch)
 * so the inner loop knows to skip the rest of the per-track work. */
static int m4a_dispatch_one(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    u8 cmd_byte = *track->cmdPtr;
    if (cmd_byte < 0x80) {
        /* running-status: reuse the previous opcode without advancing
         * cmdPtr (so the operand byte will be read by the handler). */
        cmd_byte = track->runningStatus;
    } else {
        track->cmdPtr++;
        if (cmd_byte >= M4A_RUNSTAT_LO) {
            track->runningStatus = cmd_byte;
        }
    }
    if (cmd_byte >= M4A_NOTE_BASE) {
        /* ply_note path. The asm computes `gateIdx = cmd - 0xCF` and
         * passes it as the first arg to ply_note (asm signature is
         * `ply_note(u32 gateIdx, mp, track)`). PR #7 part 2.2.2.2.2.2
         * promotes ply_note to a real C port in `ply_note_impl`,
         * which takes the gate index as an explicit 3rd arg. The
         * public 2-arg `ply_note` symbol stays around as a thin
         * wrapper (with gateIdx=0) for the engine's
         * `soundInfo->plynote` slot, but the dispatcher calls the
         * 3-arg helper directly so the gate index isn't lost. */
        u32 gateIdx = (u32)(cmd_byte - M4A_NOTE_BASE);
        ply_note_impl(mp, track, gateIdx);
    } else if (cmd_byte > M4A_JUMP_END) {
        /* 0xB1..0xCE -> gMPlayJumpTable[cmd - 0xB1]. mp->cmd holds
         * the index for the asm's command-byte introspection path
         * (e.g. ply_xcmd). */
        u8 idx = (u8)(cmd_byte - M4A_JUMP_BASE);
        mp->cmd = idx;
        typedef void (*PlyHandler)(MusicPlayerInfo*, MusicPlayerTrack*);
        PlyHandler handler = (PlyHandler)gMPlayJumpTable[idx];
        if (handler != NULL) {
            handler(mp, track);
        }
        if (track->flags == 0) {
            return 1; /* killed (e.g. ply_fine or ply_patt-overflow) */
        }
    } else {
        /* 0x80..0xB0: wait command. */
        track->wait = gClockTable[cmd_byte - M4A_WAIT_BASE];
    }
    return 0;
}

/* m4a_track_volpit_pass: the second per-track loop in MPlayMain
 * (`_080AFAB2..._080AFB60` in `asm/lib/m4a_asm.s`). Defined further
 * down because it touches the file-local `M4A_SoundInfo` /
 * `M4A_CgbChannel` overlays that aren't introduced until after the
 * ply_note machinery. PR #7 part 2.2.2.2.3. */
static void m4a_track_volpit_pass(MusicPlayerInfo* mp);

/* MPlayMain: tempo-stepped command dispatcher. This is the asm's
 * `MPlayMain` top half (`_080AF908..._080AFAB0`), ported verbatim
 * with the per-track `TrkVolPitSet` second loop
 * (`_080AFAB2..._080AFB60`, PR #7 part 2.2.2.2.3) appended.
 *
 * `src/gba/m4a.c` forward-declares this as `void MPlayMain();` (K&R
 * empty-parameter list, i.e. "any args") and stores its address into
 * `soundInfo->MPlayMainHead` whose typedef is
 * `void (*)(MusicPlayerInfo*)`. We define the parameter list
 * explicitly here so the body can use mp directly; the K&R decl in
 * m4a.c is compatible with any prototype. */
void MPlayMain(MusicPlayerInfo* mp) {
    if (mp->ident != M4A_ID_NUMBER) {
        return;
    }
    mp->ident++; /* asm: `adds r3, #1; str r3, [r0, #0x34]` */

    /* asm: `if (mp->func) mp->func(mp->intp);` — chain to the
     * predecessor MPlayMain installed by MPlayOpen. */
    if (mp->func != NULL) {
        mp->func((MusicPlayerInfo*)mp->intp);
    }

    /* status sign-bit set => music player is paused / off */
    if ((s32)mp->status < 0) {
        goto done;
    }

    FadeOutBody(mp);
    if ((s32)mp->status < 0) {
        goto done;
    }

    /* Tempo accumulator: the asm sequence is
     *   (1) tempoC = tempoC + tempoI         (write sum to mp->tempoC)
     *   (2) while (tempoC >= 150):
     *         per-track tick work
     *         clock++, status = acc
     *         tempoC = mp->tempoC - 150      (subtract AFTER tick)
     *         mp->tempoC = tempoC
     * Mirror that ordering — including the post-sum / pre-subtract
     * write of mp->tempoC — so any handler that reads mp->tempoC
     * mid-tick observes what the asm would. */
    mp->tempoC = (u16)((u32)mp->tempoC + (u32)mp->tempoI);
    while (mp->tempoC >= M4A_TEMPO_TICK) {
        /* Per-track inner loop. */
        u32 acc = 0;
        u32 bit = 1;
        s32 i = mp->trackCount;
        MusicPlayerTrack* track = mp->tracks;
        for (; i > 0; i--, track++, bit <<= 1) {
            if (!(track->flags & M4A_FLG_EXIST)) {
                continue;
            }
            acc |= bit;

            /* (1) chan-walk: gate-time decrement / dead-chan unlink. */
            m4a_chan_gate_tick(track);

            /* (2) START flag: clear track + apply dispatcher defaults. */
            if (track->flags & M4A_FLG_START) {
                m4a_track_start_init(track);
            } else {
                /* (3) Dispatcher: drain commands until wait > 0. */
                while (track->wait == 0) {
                    if (m4a_dispatch_one(mp, track) != 0) {
                        goto next_track;
                    }
                }
                /* (4) wait--; LFO tick if there's still a wait. */
                track->wait--;
                m4a_track_lfo_tick(track);
            }
        next_track:;
        }

        mp->clock++;
        if (acc == 0) {
            mp->status = M4A_STATUS_OFF;
            goto done;
        }
        mp->status = acc;

        /* Subtract 150 after the tick (asm: `subs r0, #0x96`). The
         * inner loop may have written mp->tempoC via ply_tempo, so
         * re-load before subtracting. */
        mp->tempoC = (u16)(mp->tempoC - M4A_TEMPO_TICK);
    }

    /* Per-track second loop: TrkVolPitSet + chan-update. Mirrors
     * asm `_080AFAB2..._080AFB60`. Implemented as a separate helper
     * because it depends on the `M4A_SoundInfo` / `M4A_CgbChannel`
     * overlays defined further down. */
    m4a_track_volpit_pass(mp);

done:
    mp->ident = M4A_ID_NUMBER;
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
/* PR #7 part 2.2.1 promoted the simple parameter-setting handlers    */
/* from no-op stubs to real C ports of the asm in `asm/lib/m4a_asm.s`. */
/* PR #7 part 2.2.2.1 then promoted the control-flow handlers          */
/* (`ply_fine`, `ply_goto`, `ply_patt`, `ply_pend`, `ply_rept`) and    */
/* the `track->chan` walk in `ply_endtie`. PR #7 part 2.2.2.2.2.1     */
/* promoted `ply_voice` (it just copies `mp->tone[index]` into        */
/* `track->tone`). The remaining handlers (`ply_note`, `ply_port`)    */
/* need either the channel-allocation walk against `gSoundInfo`        */
/* (`ply_note`, lands in PR #7 part 2.2.2.2.2.2) or the CGB register   */
/* pokes that PR #7 part 2.3 will introduce (`ply_port`).              */
/*                                                                     */
/* `NUM_MUSIC_PLAYERS == 0` under `__PORT__` (see src/gba/m4a.c) — so  */
/* even though `MPlayMain` is now a real port, it never iterates a    */
/* live track in the production runtime path. The handlers above are  */
/* therefore reachable today only via `Port_M4ASelfCheck()` below,     */
/* which feeds each handler a synthesized cmdPtr byte stream + (for    */
/* `ply_voice`) a synthesized tone bank, and verifies the resulting    */
/* track state against the asm semantics in `asm/lib/m4a_asm.s`.       */

/* MusicPlayerTrack flag bits used by the ply_* handlers (mirrored from */
/* include/gba/m4a.h's MPT_FLG_*).                                      */
#define M4A_FLG_VOLCHG MPT_FLG_VOLCHG                    /* 0x03 */
#define M4A_FLG_PITCHG MPT_FLG_PITCHG                    /* 0x0C */
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

/* SoundChannel statusFlags bits referenced by ply_fine / ply_endtie below.
 *
 * The asm at `_080AF976` (ply_fine) and `_080AFE1E` (ply_endtie) uses
 * raw immediates 0xc7, 0x83, 0x40 to test envelope-active / "still
 * exists" / release bits. We give them mnemonic names here for
 * legibility; the bit layout itself is dictated by `src/gba/m4a.c`'s
 * file-local `CgbChannel` / `SoundChannel` state machine and is
 * mirrored in `asm/lib/m4a_asm.s`. */
#ifndef M4A_CHAN_FLAGS_ENV
#define M4A_CHAN_FLAGS_ENV 0xc7    /* "envelope is doing something useful" */
#define M4A_CHAN_FLAGS_EXIST 0x83  /* "channel is still alive" */
#define M4A_CHAN_FLAG_RELEASE 0x40 /* set by the engine to start release */
#endif

/* `ply_fine` — terminate the current track. For each SoundChannel
 * linked from track->chan, optionally tag it with the release flag
 * (if its envelope is still active per M4A_CHAN_FLAGS_ENV) and then
 * always call RealClearChain to unlink it. Mirrors the asm at
 * `_080AF716`/`_080AF724`: the 0xc7 test gates the release-bit OR but
 * the bl to RealClearChain is unconditional. Then clear track->flags
 * so MPlayMain stops servicing the track on subsequent passes.
 * cmdPtr is *not* advanced — the
 * `0xb1` ply_fine command byte is consumed by MPlayMain's dispatcher
 * before the call lands here.
 *
 * Under the silent host mixer no SoundChannel is ever linked into the
 * track->chan list (SoundMain is a no-op so it never allocates one),
 * so the loop body never executes in actual gameplay. The
 * implementation still mirrors the asm so a future PR #7 part 2.3
 * mixer can rely on the right semantics, and so the chan-walk
 * structure itself is exercised by `Port_M4ASelfCheck()` below
 * against a synthesized SoundChannel. */
void ply_fine(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    (void)mp;
    SoundChannel* chan = track->chan;
    while (chan != NULL) {
        u8 status = chan->statusFlags;
        if (status & M4A_CHAN_FLAGS_ENV) {
            chan->statusFlags = (u8)(status | M4A_CHAN_FLAG_RELEASE);
        }
        RealClearChain(chan);
        /* Walk to the next channel via chan->next (offset 0x34 on the
         * GBA — typed as u32 in the public header but used here as a
         * SoundChannel*). The asm self-loop terminator (`if next ==
         * self, set next = 0`) is preserved so a malformed
         * mixer-allocated chain doesn't trap us in an infinite loop. */
        SoundChannel* next = (SoundChannel*)(uintptr_t)chan->next;
        if (next == chan) {
            chan->next = 0;
            next = NULL;
        }
        chan = next;
    }
    track->flags = 0;
}

/* `ply_endtie` — end a tied note. The first byte (if < 0x80) is the
 * key to release; otherwise the previous track->key is reused without
 * advancing cmdPtr. The asm then walks track->chan looking for the
 * channel currently sustaining that key and sets the release flag.
 *
 * Under the silent host mixer no SoundChannel is ever linked into the
 * track->chan list during normal gameplay, so this channel walk is a
 * no-op there. The host-side chan-walk logic is currently exercised
 * only via `Port_M4ASelfCheck()`. The matching condition mirrors the
 * asm: the channel must be "still exists" (M4A_CHAN_FLAGS_EXIST), not
 * already in release (M4A_CHAN_FLAG_RELEASE clear), and its
 * `chan->midiKey` must equal the target key. The first match is
 * tagged for release and the walk stops. */
void ply_endtie(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    (void)mp;
    u8 b = *track->cmdPtr;
    u8 target_key;
    if (b < 0x80) {
        track->key = b;
        track->cmdPtr++;
        target_key = b;
    } else {
        target_key = track->key;
    }
    SoundChannel* chan = track->chan;
    while (chan != NULL) {
        u8 status = chan->statusFlags;
        if ((status & M4A_CHAN_FLAGS_EXIST) != 0 && (status & M4A_CHAN_FLAG_RELEASE) == 0 &&
            chan->midiKey == target_key) {
            chan->statusFlags = (u8)(status | M4A_CHAN_FLAG_RELEASE);
            return;
        }
        SoundChannel* next = (SoundChannel*)(uintptr_t)chan->next;
        if (next == chan) {
            chan->next = 0;
            next = NULL;
        }
        chan = next;
    }
}

/* Pattern-stack / control-flow handlers.
 *
 * All four read a 4-byte little-endian command-stream address out of
 * track->cmdPtr (matching the asm's per-byte assembly into r0); on the
 * GBA this is always a ROM pointer, but on the host it can be any
 * `u8*` we like. We avoid the asm's `sub_080AF75C` ROM-bounds clamp:
 * it exists purely to convert obviously-bogus pointers into a "stop
 * the song" sentinel so the asm doesn't fault, and the host has no
 * comparable "definitely faulting" address range — so applying the
 * clamp would refuse our self-check's stack pointers. The runtime
 * cmdPtrs come from baked song data linked by the game in any case,
 * so the clamp is irrelevant once the dispatcher hands us a song. */

/* Decode `ptr[0..3]` as an unaligned little-endian pointer. On any
 * sensible host the resulting `uintptr_t` either fits a `u8*` for
 * 32-bit pointer hosts (the GBA case via the ROM build) or is the
 * low-32 bits we must extend to a 64-bit pointer. For the actively
 * supported host targets we just need the two narrow ports of the
 * cmdPtr address space already exercised by the self-check below. */
static u8* m4a_read_cmd_ptr(const u8* p) {
    u32 lo = (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
    return (u8*)(uintptr_t)lo;
}

/* `ply_goto` — unconditional jump. Replaces track->cmdPtr with the
 * 4-byte address embedded after the goto opcode. */
void ply_goto(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    (void)mp;
    track->cmdPtr = m4a_read_cmd_ptr(track->cmdPtr);
}

/* `ply_patt` — push the current cmdPtr+4 onto the pattern stack and
 * jump. If the stack is full (patternLevel >= 3) the asm falls
 * through to `ply_fine`, terminating the track. */
void ply_patt(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    if (track->patternLevel >= 3) {
        ply_fine(mp, track);
        return;
    }
    track->patternStack[track->patternLevel] = track->cmdPtr + 4;
    track->patternLevel++;
    /* Fall through into the goto-to-the-embedded-address path. */
    track->cmdPtr = m4a_read_cmd_ptr(track->cmdPtr);
}

/* `ply_pend` — pop the pattern stack. If patternLevel is already zero
 * (no nesting), the asm just returns without touching cmdPtr; the
 * dispatcher is expected to have consumed the opcode byte itself. */
void ply_pend(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    (void)mp;
    if (track->patternLevel == 0) {
        return;
    }
    track->patternLevel--;
    track->cmdPtr = track->patternStack[track->patternLevel];
}

/* `ply_rept` — bounded repeat. Reads a 1-byte count, then a 4-byte
 * address. count==0 means "repeat forever" (just take the goto). For
 * count > 0, increment track->repN; if repN < count, take the goto;
 * else reset repN and skip past the (count + address) payload (5
 * bytes total). */
void ply_rept(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    (void)mp;
    u8 count = track->cmdPtr[0];
    if (count == 0) {
        track->cmdPtr = m4a_read_cmd_ptr(track->cmdPtr + 1);
        return;
    }
    track->repN++;
    if (track->repN < count) {
        track->cmdPtr = m4a_read_cmd_ptr(track->cmdPtr + 1);
    } else {
        track->repN = 0;
        track->cmdPtr += 5;
    }
}

/* `ply_voice` — load a voice-bank entry into the track's embedded
 * ToneData. Reads a single byte (the voice index) from cmdPtr,
 * advances cmdPtr by 1, then copies `mp->tone[index]` (a 12-byte
 * ToneData entry on the GBA) into `track->tone`.
 *
 * The asm at `_080AF838` does the copy as three back-to-back 4-byte
 * loads/stores from `mp->tone + index * 12` into `track->tone`,
 * routing each load through `sub_080AF75E` (the ROM-bounds clamp we
 * already skip elsewhere — see the m4a_read_cmd_ptr comment). On the
 * host the `wav` field of ToneData widens from 4 to 8 bytes, so a
 * literal 12-byte memcpy would silently truncate `tone.wav`; we use
 * a struct assignment instead so the copy uses the host `ToneData`
 * layout and copies the full host object representation (including
 * the widened `wav` pointer) rather than the GBA's 12-byte layout.
 * The asm's logical field order (type/key/length/pan_sweep → wav →
 * attack/decay/sustain/release) matches the C struct definition in
 * `include/gba/m4a.h`.
 *
 * `ply_voice` is `gMPlayJumpTable[12]` (opcode 0xBD), so it is
 * reachable through MPlayMain's running-status path; under the
 * silent host mixer the only caller is `Port_M4ASelfCheck()` because
 * NUM_MUSIC_PLAYERS == 0 keeps MPlayMain out of the production
 * path. */
void ply_voice(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    u8 voice = m4a_consume_byte(track);
    /* mp->tone is a pointer to a flat array of ToneData entries.
     * The asm computes the byte offset as `voice * 12` (3 << 2);
     * here we use array indexing via the host ToneData type so the
     * compiler does the size scaling correctly. */
    track->tone = mp->tone[voice];
}

/* The remaining gated handler is `ply_port` (needs the CGB register
 * pokes that the host mixer doesn't have a backing-store for yet —
 * PR #7 part 2.3). `ply_note` was promoted to a real C port in PR #7
 * part 2.2.2.2.2.2 (the implementation lives below this stub block,
 * after the SoundInfo / CgbChannel host mirrors it depends on). */
#define M4A_PLY_STUB(name)                                 \
    void name(MusicPlayerInfo* mp, MusicPlayerTrack* tr) { \
        (void)mp;                                          \
        (void)tr;                                          \
    }

M4A_PLY_STUB(ply_port)

#undef M4A_PLY_STUB

/* ------------------------------------------------------------------ */
/* PR #7 part 2.2.2.2.2.2: ply_note.                                  */
/*                                                                    */
/* Host C reimplementation of `asm/lib/m4a_asm.s::ply_note`. The asm  */
/* signature is `ply_note(u32 gateIdx, MusicPlayerInfo* mp,           */
/* MusicPlayerTrack* track)` (gateIdx in r0, mp in r1, track in r2);  */
/* the C-defined `void ply_note(MusicPlayerInfo*, MusicPlayerTrack*)` */
/* declared by m4a.c is the one stored into `soundInfo->plynote`, so  */
/* we expose that 2-arg public symbol as a thin wrapper around the    */
/* host-private 3-arg `ply_note_impl` that the dispatcher calls       */
/* directly.                                                          */
/*                                                                    */
/* The asm performs three big things:                                 */
/*   1. Operand decode: read up to three optional bytes from          */
/*      `track->cmdPtr` (key, velocity, gate-time delta), all of      */
/*      which are absent if the next byte is >= 0x80 (running-status  */
/*      reuse). gateTime is initialised from `gClockTable[gateIdx]`   */
/*      first, then the optional delta is added.                      */
/*   2. Tone resolution: handle key-split / rhythm tone tables. For   */
/*      a plain DirectSound or CGB tone, the active sub-tone is just  */
/*      `&track->tone`. The key-split / rhythm paths overload the     */
/*      `tone.attack..release` bytes as a u8* sub-key table and       */
/*      `tone.wav` as a `ToneData[]` pointer — those overloads don't  */
/*      survive a 64-bit host's pointer width (the bytes can't hold   */
/*      a real pointer), so we reproduce the asm's logical behaviour  */
/*      using host-friendly fields instead: when the active tone has  */
/*      `TONEDATA_TYPE_RHY | TONEDATA_TYPE_SPL` set we walk through   */
/*      via the same byte-offset trick the asm uses and rely on the   */
/*      caller to set `tone.wav` to a real `ToneData*`. Self-check    */
/*      exercises both the plain and rhythm paths.                    */
/*   3. Channel allocation: walk `soundInfo->cgbChannels[type-1]`     */
/*      for CGB voices (sub-tone type & 7 in 1..6), or scan           */
/*      `soundInfo->chans[0..maxChannels)` for a free DirectSound     */
/*      channel (preferring envelope-dead, then releasing, then       */
/*      lower-priority chans; ties broken by track-ptr ordering).     */
/*      The chosen chan is pulled out of any existing list via        */
/*      `ClearChain`, inserted at the head of `track->chan`,          */
/*      bound back to the track via `chan->track = track`, primed     */
/*      with the resolved key / velocity / wav / ADSR, and given a    */
/*      starting `chan->frequency` from `MidiKeyToFreq` (DirectSound) */
/*      or `soundInfo->MidiKeyToCgbFreq` (CGB).                       */
/*                                                                    */
/* Finally, `track->flags &= 0xF0` clears the lower nibble's per-tick */
/* request flags (VOLSET, VOLCHG, PITSET, PITCHG) — the chan walk     */
/* above and the post-ply_note `TrkVolPitSet` step have already       */
/* applied them.                                                      */
/*                                                                    */
/* On the silent host mixer this is reached only via the dispatcher   */
/* (which itself is reached only via Port_M4ASelfCheck because        */
/* NUM_MUSIC_PLAYERS == 0 keeps MPlayMain out of the production       */
/* path). Both code paths and the self-check exercise the same C      */
/* implementation.                                                    */
/* ------------------------------------------------------------------ */

/* Mirror of the file-local SoundInfo / CgbChannel typedefs from
 * src/gba/m4a.c. They aren't exposed in include/gba/m4a.h, so we
 * redeclare them here for the host port to walk by name. The two
 * typedefs are identical to m4a.c's (same field names, same C
 * types), so both TUs see the same struct layout and the
 * `extern SoundInfo gSoundInfo;` resolution against the byte-array
 * BSS symbol below works cleanly.
 *
 * Keep these in sync with src/gba/m4a.c::CgbChannel / SoundInfo. */
typedef void (*m4a_CgbSoundFunc)(void);
typedef void (*m4a_CgbOscOffFunc)(u8);
typedef u32 (*m4a_MidiKeyToCgbFreqFunc)(u8, u8, u8);
typedef void (*m4a_ExtVolPitFunc)(void);

typedef struct M4A_CgbChannel {
    u8 statusFlags;
    u8 type;
    u8 rightVolume;
    u8 leftVolume;
    u8 attack;
    u8 decay;
    u8 sustain;
    u8 release;
    u8 key;
    u8 envelopeVolume;
    u8 envelopeGoal;
    u8 envelopeCounter;
    u8 echoVolume;
    u8 echoLength;
    u8 unk0;
    u8 unk1;
    u8 gateTime;
    u8 midiKey;
    u8 velocity;
    u8 priority;
    u8 rhythmPan;
    u8 unk2[3];
    u8 unk3;
    u8 sustainGoal;
    u8 n4;
    u8 pan;
    u8 panMask;
    u8 modify;
    u8 length;
    u8 sweep;
    u32 frequency;
    u32* nextWave;
    u32* currentWave;
    u32 track;
    u32 prev;
    u32 next;
    u8 unk4[8];
} M4A_CgbChannel;

#define M4A_MAX_DIRECTSOUND_CHANNELS 12
#define M4A_PCM_DMA_BUF_SIZE 1584

typedef struct M4A_SoundInfo {
    u32 ident;
    u8 pcmDmaCounter;
    u8 reverb;
    u8 maxChannels;
    u8 masterVolume;
    u8 frequency;
    u8 mode;
    u8 c15;
    u8 pcmDmaPeriod;
    u8 maxLines;
    u8 gap[3];
    s32 pcmSamplesPerVBlank;
    s32 pcmFreqency;
    s32 divFreq;
    M4A_CgbChannel* cgbChannels;
    MPlayMainFunc MPlayMainHead;
    void* intp;
    m4a_CgbSoundFunc CgbSound;
    m4a_CgbOscOffFunc CgbOscOff;
    m4a_MidiKeyToCgbFreqFunc MidiKeyToCgbFreq;
    void* MPlayJumpTable;
    void* plynote;
    m4a_ExtVolPitFunc ExtVolPit;
    u8 gap2[16];
    SoundChannel chans[M4A_MAX_DIRECTSOUND_CHANNELS];
    s8 pcmBuffer[M4A_PCM_DMA_BUF_SIZE];
} M4A_SoundInfo;

/* `gSoundInfo` is declared up top as `u8 gSoundInfo[4096]`. m4a.c sees
 * it as the file-local `SoundInfo` typedef; we cast through this host
 * mirror to walk by name. A static-assert keeps the BSS allocation
 * comfortably above the host SoundInfo footprint. */
_Static_assert(sizeof(M4A_SoundInfo) <= 4096, "gSoundInfo BSS too small for host SoundInfo");

/* m4a.c's MidiKeyToFreq — used for DirectSound chans. */
extern u32 MidiKeyToFreq(WaveData* wav, u8 key, u8 fineAdjust);
/* m4a.c's TrkVolPitSet — invoked just before the chan->frequency
 * setup so the freshly-allocated chan inherits the track's vol/pit. */
extern void TrkVolPitSet(MusicPlayerInfo* mp, MusicPlayerTrack* track);

/* ------------------------------------------------------------------ */
/* PR #7 part 2.2.2.2.3: m4a_track_volpit_pass                         */
/*                                                                    */
/* Port of the asm `MPlayMain` second per-track loop                  */
/* (`_080AFAB2..._080AFB60` in `asm/lib/m4a_asm.s`). Runs after the   */
/* tempo-stepped dispatcher loop. For each live track with a pending  */
/* VOLCHG/PITCHG bit set in the low nibble of `track->flags`:         */
/*   - Calls `TrkVolPitSet(mp, track)` (the C-defined per-track vol/  */
/*     pitch recompute in `src/gba/m4a.c`), which folds in modM /     */
/*     bend / pan and clears the VOLSET / PITSET bits.                */
/*   - Walks `track->chan` (a SoundChannel linked list rooted on the  */
/*     track and chained via the `chan->next` u32-ified pointer):      */
/*       * dead-envelope chan (`!(statusFlags & 0xc7)`) is unlinked    */
/*         via `ClearChain(chan)`.                                     */
/*       * else: VOLCHG → `ChnVolSetAsm(chan, track)` (silent stub on  */
/*         the host); CGB-typed chans additionally OR bit 0 into the   */
/*         CgbChannel `modify` byte (offset 0x1d) so the next CGB     */
/*         mixer pass re-pokes the CGB volume regs.                   */
/*       * PITCHG → newKey = chan->key + (s8)track->keyM, clamped to  */
/*         >= 0; CGB chans use `soundInfo->MidiKeyToCgbFreq` (when    */
/*         non-NULL) and OR bit 1 into `modify`; DirectSound chans   */
/*         use `MidiKeyToFreq(chan->wav, ...)`. Result lands in       */
/*         `chan->frequency`.                                         */
/*       * The asm's chan->next == self self-loop break is preserved. */
/*   - Always clears the lower nibble of `track->flags` after the     */
/*     per-track work (whether or not a chan list existed).            */
/*                                                                    */
/* Under the silent host mixer no `track->chan` ever gets connected   */
/* by the production runtime path (`NUM_MUSIC_PLAYERS == 0` keeps     */
/* `MPlayMain` itself out of the live path), so the chan walk is      */
/* exercised by `Port_M4ASelfCheck()` only. The flag-clear step does  */
/* run for any synthesized track the dispatcher feeds.                */
/* ------------------------------------------------------------------ */
static void m4a_track_volpit_pass(MusicPlayerInfo* mp) {
    s32 remaining = mp->trackCount;
    MusicPlayerTrack* track = mp->tracks;
    M4A_SoundInfo* soundInfo = (M4A_SoundInfo*)gSoundInfo;

    for (; remaining > 0; remaining--, track++) {
        u8 flags = track->flags;
        if (!(flags & M4A_FLG_EXIST)) {
            continue;
        }
        if ((flags & 0x0F) == 0) {
            /* No VOLCHG/PITCHG pending. */
            continue;
        }

        /* Per-track recompute (modT folds in, sets volMR/volML, keyM/pitM,
         * and clears VOLSET/PITSET). */
        TrkVolPitSet(mp, track);

        /* Chan walk. */
        SoundChannel* chan = track->chan;
        while (chan != NULL) {
            u8 chanStatus = chan->statusFlags;
            if ((chanStatus & M4A_CHAN_FLAGS_ENV) == 0) {
                /* envelope dead — unlink from the chain. */
                ClearChain(chan);
            } else {
                u8 cgbType = (u8)(chan->type & 7);

                /* VOLCHG path (track flags & 3). */
                if (flags & MPT_FLG_VOLCHG) {
                    ChnVolSetAsm();
                    if (cgbType != 0) {
                        /* CgbChannel byte at offset 0x1d (`modify`)
                         * — same byte as SoundChannel.fw[1] on GBA;
                         * we go through the M4A_CgbChannel overlay
                         * because the host SoundChannel layout
                         * widens past byte 0x14. */
                        M4A_CgbChannel* cgb = (M4A_CgbChannel*)chan;
                        cgb->modify = (u8)(cgb->modify | 1);
                    }
                }

                /* PITCHG path (track flags & 0xc). */
                if (flags & MPT_FLG_PITCHG) {
                    s32 newKey = (s32)chan->key + (s32)(s8)track->keyM;
                    if (newKey < 0) {
                        newKey = 0;
                    }
                    if (cgbType != 0) {
                        M4A_CgbChannel* cgb = (M4A_CgbChannel*)chan;
                        if (soundInfo->MidiKeyToCgbFreq != NULL) {
                            cgb->frequency = soundInfo->MidiKeyToCgbFreq(
                                cgbType, (u8)newKey, track->pitM);
                        }
                        cgb->modify = (u8)(cgb->modify | 2);
                    } else {
                        chan->frequency = MidiKeyToFreq(
                            chan->wav, (u8)newKey, track->pitM);
                    }
                }
            }

            /* Walk to the next channel via chan->next (offset 0x34
             * on GBA — held as a u32 for binary parity with the
             * asm). The asm's self-loop guard (`if next == chan,
             * set chan->next = 0`) breaks the cycle the engine
             * occasionally sets up to mark the tail. */
            SoundChannel* next = (SoundChannel*)(uintptr_t)chan->next;
            if (next == chan) {
                chan->next = 0;
                next = NULL;
            }
            chan = next;
        }

        /* Clear the lower nibble (VOLSET/VOLCHG/PITSET/PITCHG bits)
         * of track->flags whether or not a chan list existed. The
         * asm does this at `_080AFB50` after the chan loop. */
        track->flags = (u8)(track->flags & 0xF0);
    }
}

/* The 3-arg helper. The asm signature is `ply_note(gateIdx, mp, track)`. */
void ply_note_impl(MusicPlayerInfo* mp, MusicPlayerTrack* track, u32 gateIdx) {
    M4A_SoundInfo* soundInfo = (M4A_SoundInfo*)gSoundInfo;

    /* (1) Operand decode. */
    /* gateTime = gClockTable[gateIdx]. The asm: `ldrb r0, [r1]` after
     * `r1 = &gClockTable[gateIdx]`. gClockTable is 49 entries; the
     * dispatcher hands gateIdx in [0..48]. */
    track->gateTime = gClockTable[gateIdx];

    /* Optional bytes from track->cmdPtr: key, velocity, gateTime delta.
     * Each is gated on `< 0x80` (running-status sentinel). */
    u8* cp = track->cmdPtr;
    u8 b0 = cp[0];
    if (b0 < 0x80) {
        track->key = b0;
        cp++;
        u8 b1 = cp[0];
        if (b1 < 0x80) {
            track->velocity = b1;
            cp++;
            u8 b2 = cp[0];
            if (b2 < 0x80) {
                track->gateTime = (u8)(track->gateTime + b2);
                cp++;
            }
        }
        track->cmdPtr = cp;
    }

    /* (2) Tone resolution.
     *
     * `subTone` points at the active ToneData. For plain tones it's
     * `&track->tone`; for key-split / rhythm tones it's an entry in a
     * sub-tone table.
     *
     * `forcedRelease` (asm: sp[0x14]) is the rhythm-pan override that
     * gets stamped into chan->rhythmPan; it stays 0 unless we enter
     * the rhythm path AND the sub-tone has TONEDATA_P_S_PAN set in
     * its pan_sweep byte.
     *
     * `key` (asm: r3) starts as `track->key` (after the operand
     * update above) and is reassigned to the sub-tone's `key` field
     * on the rhythm path.
     */
    ToneData* tone = &track->tone;
    ToneData* subTone;
    u32 forcedRelease = 0;
    u8 key = track->key;

    if (tone->type & (TONEDATA_TYPE_RHY | TONEDATA_TYPE_SPL)) {
        u8 subKey;
        if (tone->type & TONEDATA_TYPE_SPL) {
            /* Key split: the asm reuses tone.attack..release as a
             * `u8*` to a key→subTone-index table (`ldr [r5, #0x2c]`
             * 4-byte load). On a 64-bit host that 32-bit value
             * cannot hold a real pointer, so dereferencing it is
             * undefined and would almost certainly crash on any
             * SPL tone we encountered outside a synthetic harness.
             * No real song data routed through the host mixer is
             * known to use the SPL path (TMC's tone bank has no
             * SPL tones), so defensively bail out instead of
             * fabricating a sub-key. If host SPL support ever
             * matters, the fix is to add a real ToneData* side
             * field to the host overlay and populate it from a
             * GBA→host pointer table. */
            return;
        } else {
            subKey = key;
        }
        /* tone.wav is a ToneData* (pointing to the sub-tones
         * array) on this path. */
        ToneData* subBank = (ToneData*)tone->wav;
        subTone = &subBank[subKey];
        /* If the resolved sub-tone is itself a rhythm/key-split
         * tone, the asm bails (no recursion). */
        if (subTone->type & (TONEDATA_TYPE_RHY | TONEDATA_TYPE_SPL)) {
            return;
        }
        if (tone->type & TONEDATA_TYPE_RHY) {
            /* Rhythm: optional pan override + key from sub-tone. */
            if (subTone->pan_sweep & 0x80) {
                /* sp[0x14] = (pan_sweep - 0xC0) << 1. */
                forcedRelease = (u32)(((s32)subTone->pan_sweep - 0xC0) << 1) & 0xff;
            }
            key = subTone->key;
        }
    } else {
        subTone = tone;
    }

    /* The dispatched key (asm sp[8]) is the post-resolution key. */
    u8 dispatchKey = key;

    /* The note's allocation priority is the track's priority plus the
     * mp-wide priority bias (mp->priority), saturated at 0xFF.
     * (asm: `r0 = track->priority + mp->priority; if r0 > 0xff: r0 = 0xff`) */
    u32 notePriority = (u32)track->priority + (u32)mp->priority;
    if (notePriority > 0xff) {
        notePriority = 0xff;
    }

    /* (3) Channel allocation. */
    u32 cgbType = subTone->type & 7;

    SoundChannel* chosenDS = NULL;
    M4A_CgbChannel* chosenCGB = NULL;

    if (cgbType != 0) {
        /* CGB path: a single fixed channel chosen by voice type. */
        M4A_CgbChannel* cgb = soundInfo->cgbChannels;
        if (cgb == NULL) {
            return;
        }
        M4A_CgbChannel* slot = &cgb[cgbType - 1];
        u8 sf = slot->statusFlags;
        if ((sf & M4A_CHAN_FLAGS_ENV) == 0) {
            chosenCGB = slot; /* envelope dead → take it */
        } else if (sf & M4A_CHAN_FLAG_RELEASE) {
            chosenCGB = slot; /* releasing → preempt */
        } else if (slot->priority < notePriority) {
            chosenCGB = slot; /* lower priority → evict */
        } else if (slot->priority == notePriority) {
            /* Tie-break by track ptr using the same 32-bit form on
             * both sides (slot->track is stored as a 32-bit value
             * in the host overlay to mirror the GBA ABI), so the
             * comparison is meaningful on 64-bit hosts where a
             * raw `(uintptr_t)track` would dwarf any value held
             * in `slot->track`. */
            if ((uintptr_t)(u32)slot->track >= (uintptr_t)(u32)(uintptr_t)track) {
                chosenCGB = slot;
            } else {
                return;
            }
        } else {
            return;
        }
    } else {
        /* DirectSound path: scan soundInfo->chans[0..maxChannels). */
        SoundChannel* best = NULL;
        u32 bestPriority = notePriority;
        uintptr_t bestTrack = (uintptr_t)track;
        u32 hasReleasingCandidate = 0;

        u8 maxChans = soundInfo->maxChannels;
        for (u8 i = 0; i < maxChans; i++) {
            SoundChannel* c = &soundInfo->chans[i];
            u8 sf = c->statusFlags;
            if ((sf & M4A_CHAN_FLAGS_ENV) == 0) {
                /* Envelope-dead chan: take immediately, asm's
                 * `_080AFD40` direct branch (the inner-loop scan
                 * doesn't continue once a free chan is found). */
                best = c;
                break;
            }
            if (sf & M4A_CHAN_FLAG_RELEASE) {
                /* Releasing chan: prefer over any non-releasing. */
                if (!hasReleasingCandidate) {
                    hasReleasingCandidate = 1;
                    bestPriority = c->priority;
                    bestTrack = (uintptr_t)c->track;
                    best = c;
                } else {
                    if (c->priority < bestPriority) {
                        bestPriority = c->priority;
                        bestTrack = (uintptr_t)c->track;
                        best = c;
                    } else if (c->priority == bestPriority) {
                        uintptr_t ct = (uintptr_t)c->track;
                        if (ct > bestTrack) {
                            bestTrack = ct;
                            best = c;
                        }
                    }
                }
            } else if (!hasReleasingCandidate) {
                /* Non-releasing, no releasing candidate yet: compete
                 * on priority + track-ptr tiebreak vs. our note. */
                if (c->priority < bestPriority) {
                    bestPriority = c->priority;
                    bestTrack = (uintptr_t)c->track;
                    best = c;
                } else if (c->priority == bestPriority) {
                    uintptr_t ct = (uintptr_t)c->track;
                    if (ct > bestTrack) {
                        bestTrack = ct;
                        best = c;
                    }
                }
            }
        }

        if (best == NULL) {
            return; /* no candidate → drop the note */
        }
        chosenDS = best;
    }

    /* Install the chosen chan into track->chan (head of list).
     *
     * The two cases (DirectSound vs. CGB) differ in struct layout
     * past byte 0x14 (CgbChannel has its own n4/pan/panMask/... block
     * where SoundChannel has count/fw/wav, and the trailing prev/next/
     * track u32s land at different host offsets due to embedded
     * pointers in SoundChannel). The fields up through `rhythmPan`
     * (offset 0x14), plus `echoVolume`/`echoLength` (0x0C/0x0D) and
     * `frequency` (offset 0x20 in both), are byte-identical on host;
     * the diverging post-rhythmPan fields are written through the
     * matching struct type. */

    /* lfo + TrkVolPitSet are common to both paths and don't touch
     * the chan pointer. */
    track->lfoDelayC = track->lfoDelay;
    if (track->lfoDelay != 0) {
        clear_modM(track);
    }
    TrkVolPitSet(mp, track);

    /* Compute the freq seed: r3 = chan->key + (s8)track->keyM,
     * clamped to >= 0. (chan->key isn't yet set; both the asm and
     * us use the resolved dispatchKey here.) */
    s32 freqKey = (s32)dispatchKey + (s32)(s8)track->keyM;
    if (freqKey < 0) {
        freqKey = 0;
    }

    if (chosenDS != NULL) {
        SoundChannel* chan = chosenDS;

        ClearChain(chan);
        chan->prev = 0;
        chan->next = (u32)(uintptr_t)track->chan;
        if (track->chan != NULL) {
            track->chan->prev = (u32)(uintptr_t)chan;
        }
        track->chan = chan;
        chan->track = track;

        chan->gateTime = track->gateTime;
        /* The asm seeds chan+0x10..0x13 (gateTime/midiKey/velocity/
         * priority) with a 4-byte load from track+4 (gateTime/key/
         * velocity/runningStatus) before overwriting priority. So
         * chan->midiKey / chan->velocity inherit the just-decoded
         * key/velocity operands from the track. */
        chan->midiKey = track->key;
        chan->velocity = track->velocity;
        chan->priority = (u8)notePriority;
        chan->key = dispatchKey;
        chan->rhythmPan = (u8)forcedRelease;
        chan->type = subTone->type;
        chan->wav = subTone->wav;
        chan->attack = subTone->attack;
        chan->decay = subTone->decay;
        chan->sustain = subTone->sustain;
        chan->release = subTone->release;
        chan->echoVolume = track->echoVolume;
        chan->echoLength = track->echoLength;
        ChnVolSetAsm();

        chan->frequency = MidiKeyToFreq((WaveData*)subTone->wav, (u8)freqKey, track->pitM);

        chan->statusFlags = 0x80;
    } else {
        M4A_CgbChannel* cgb = chosenCGB;

        /* CgbChannel-side install: same field semantics, different
         * trailing-field layout. The asm's ClearChain takes a void*
         * and walks bytes; on the host it's a no-op stub. */
        ClearChain(cgb);
        cgb->prev = 0;
        cgb->next = (u32)(uintptr_t)track->chan;
        if (track->chan != NULL) {
            /* Old head's prev field. The host SoundChannel.prev is
             * a u32 at SoundChannel offset 0x40 (after pointer-
             * widened wav/currentPointer/track). track->chan is a
             * SoundChannel*, so we use named-field access. */
            track->chan->prev = (u32)(uintptr_t)cgb;
        }
        /* track->chan is typed SoundChannel*. The CgbChannel install
         * stores the cgb-typed pointer here; downstream walks treat
         * the head as opaque bytes anyway (see m4a_chan_gate_tick),
         * so the type punning is faithful to the asm. */
        track->chan = (SoundChannel*)cgb;
        cgb->track = (u32)(uintptr_t)track;

        cgb->gateTime = track->gateTime;
        /* See the matching DirectSound path: the asm's 4-byte
         * track+4 → chan+0x10 store seeds midiKey/velocity from
         * the just-decoded operands. */
        cgb->midiKey = track->key;
        cgb->velocity = track->velocity;
        cgb->priority = (u8)notePriority;
        cgb->key = dispatchKey;
        cgb->rhythmPan = (u8)forcedRelease;
        cgb->type = subTone->type;
        cgb->attack = subTone->attack;
        cgb->decay = subTone->decay;
        cgb->sustain = subTone->sustain;
        cgb->release = subTone->release;
        cgb->echoVolume = track->echoVolume;
        cgb->echoLength = track->echoLength;
        ChnVolSetAsm();

        /* CGB-only byte pokes: the asm writes subTone->length and a
         * derived n4 byte at chan + 0x1e / chan + 0x1f. On the
         * CgbChannel host layout those bytes land in `length` and
         * `sweep` respectively (CgbChannel.length at 0x1E,
         * CgbChannel.sweep at 0x1F per src/gba/m4a.c). */
        cgb->length = subTone->length;
        u8 ps = subTone->pan_sweep;
        u8 n4 = 0;
        if (ps & 0x80) {
            n4 = 8;
        } else if (ps & 0x70) {
            n4 = 8;
        } else {
            n4 = ps;
        }
        cgb->sweep = n4;

        if (soundInfo->MidiKeyToCgbFreq != NULL) {
            cgb->frequency = soundInfo->MidiKeyToCgbFreq((u8)cgbType, (u8)freqKey, track->pitM);
        }

        cgb->statusFlags = 0x80;
    }

    /* Final: clear the lower nibble of track->flags. */
    track->flags &= 0xF0;
}

/* The public 2-arg ply_note that m4a.c stores into soundInfo->plynote.
 * Calls into ply_note_impl with gateIdx=0 (gClockTable[0] = 0, so
 * the gateTime initialisation is a no-op). */
void ply_note(MusicPlayerInfo* mp, MusicPlayerTrack* track) {
    ply_note_impl(mp, track, 0);
}

/* `nullsub_141` is referenced as the engine's no-op slot. */
void nullsub_141(void) { /* truly a nullsub */
}

/* ------------------------------------------------------------------ */
/* (4) PR #7 part 2.2.* self-check.                                   */
/*                                                                    */
/* Builds a stack-allocated MusicPlayerInfo + MusicPlayerTrack and    */
/* drives each implemented ply_* handler with a synthesized cmdPtr    */
/* byte stream, asserting that the right field is updated, the right */
/* flag bits are set, and cmdPtr advanced as the asm does.            */
/* PR #7 part 2.2.2.1 extends the harness with the control-flow      */
/* handlers (`ply_fine`, `ply_goto`, `ply_patt`, `ply_pend`,          */
/* `ply_rept`) and the `track->chan` walk shared by `ply_fine` and    */
/* `ply_endtie`, using a stack-allocated synthesized SoundChannel.    */
/*                                                                    */
/* This is the m4a equivalent of `Port_RendererSelfCheck()` /         */
/* `Port_AudioSelfCheck()` and runs from `src/platform/sdl/main.c`    */
/* on every smoke-test invocation, so any future regression in the    */
/* command-byte / field-offset / flag-bit contracts surfaces in CI.   */
/* ------------------------------------------------------------------ */

#define M4A_CHECK(cond)                                                                              \
    do {                                                                                             \
        if (!(cond)) {                                                                               \
            fprintf(stderr, "[Port_M4ASelfCheck] failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                                                \
        }                                                                                            \
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
#define M4A_RUN(handler, byte)                 \
    do {                                       \
        memset(&mp, 0, sizeof(mp));            \
        memset(&track, 0, sizeof(track));      \
        stream[0] = (u8)(byte);                \
        track.cmdPtr = stream;                 \
        handler(&mp, &track);                  \
        M4A_CHECK(track.cmdPtr == stream + 1); \
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

/* ----------------------------------------------------------- */
/* PR #7 part 2.2.2.1: control-flow handlers + chan walk.      */
/* ----------------------------------------------------------- */

/* Helper: encode a 4-byte little-endian "pointer" into stream[off..off+3]
 * so we can verify the m4a_read_cmd_ptr decode path without depending
 * on a host's pointer width. The value `0xDEADBEEFu` casts to a
 * deterministic synthetic address; we only check the bit pattern. */
#define M4A_WRITE_LE32(buf, off, val)                  \
    do {                                               \
        (buf)[(off) + 0] = (u8)((val)&0xff);           \
        (buf)[(off) + 1] = (u8)(((val) >> 8) & 0xff);  \
        (buf)[(off) + 2] = (u8)(((val) >> 16) & 0xff); \
        (buf)[(off) + 3] = (u8)(((val) >> 24) & 0xff); \
    } while (0)

    /* ply_goto: replace cmdPtr with the embedded LE pointer. */
    {
        u8 stream2[8];
        memset(&mp, 0, sizeof(mp));
        memset(&track, 0, sizeof(track));
        M4A_WRITE_LE32(stream2, 0, 0xDEADBEEFu);
        track.cmdPtr = stream2;
        ply_goto(&mp, &track);
        M4A_CHECK(track.cmdPtr == (u8*)(uintptr_t)0xDEADBEEFu);
    }

    /* ply_patt: push cmdPtr+4 onto patternStack[patternLevel++], then
     * jump to the embedded pointer. */
    {
        u8 stream2[8];
        memset(&mp, 0, sizeof(mp));
        memset(&track, 0, sizeof(track));
        M4A_WRITE_LE32(stream2, 0, 0x12345678u);
        track.cmdPtr = stream2;
        track.patternLevel = 0;
        ply_patt(&mp, &track);
        M4A_CHECK(track.patternLevel == 1);
        M4A_CHECK(track.patternStack[0] == stream2 + 4);
        M4A_CHECK(track.cmdPtr == (u8*)(uintptr_t)0x12345678u);

        /* Nested push: patternLevel grows to 3. */
        u8 stream3[8];
        M4A_WRITE_LE32(stream3, 0, 0xCAFEBABEu);
        track.cmdPtr = stream3;
        ply_patt(&mp, &track);
        M4A_CHECK(track.patternLevel == 2);
        M4A_CHECK(track.patternStack[1] == stream3 + 4);
        M4A_CHECK(track.cmdPtr == (u8*)(uintptr_t)0xCAFEBABEu);

        /* Stack-full path falls through to ply_fine: clears flags, no
         * push. patternLevel stays at 3 (the bound). */
        track.patternLevel = 3;
        track.flags = MPT_FLG_EXIST | MPT_FLG_VOLCHG;
        u8 stream4[8];
        M4A_WRITE_LE32(stream4, 0, 0x11111111u);
        track.cmdPtr = stream4;
        track.chan = NULL;
        ply_patt(&mp, &track);
        M4A_CHECK(track.patternLevel == 3);
        M4A_CHECK(track.flags == 0); /* ply_fine zero-out */
    }

    /* ply_pend: pop the pattern stack; no-op when patternLevel == 0. */
    {
        u8 stream2[8];
        u8 saved[8];
        memset(&mp, 0, sizeof(mp));
        memset(&track, 0, sizeof(track));
        track.patternLevel = 2;
        track.patternStack[0] = (u8*)(uintptr_t)0xAABBCCDDu;
        track.patternStack[1] = saved + 3;
        track.cmdPtr = stream2;
        ply_pend(&mp, &track);
        M4A_CHECK(track.patternLevel == 1);
        M4A_CHECK(track.cmdPtr == saved + 3);
        ply_pend(&mp, &track);
        M4A_CHECK(track.patternLevel == 0);
        M4A_CHECK(track.cmdPtr == (u8*)(uintptr_t)0xAABBCCDDu);

        /* No-op when already empty: cmdPtr unchanged. */
        track.cmdPtr = stream2 + 1;
        ply_pend(&mp, &track);
        M4A_CHECK(track.patternLevel == 0);
        M4A_CHECK(track.cmdPtr == stream2 + 1);
    }

    /* ply_rept: count==0 -> infinite loop (just take the goto);
     * count>0 -> increment repN; jump while repN < count; otherwise
     * reset and skip past the 5-byte payload. */
    {
        u8 stream2[8];
        memset(&mp, 0, sizeof(mp));
        memset(&track, 0, sizeof(track));
        stream2[0] = 0; /* count == 0 */
        M4A_WRITE_LE32(stream2, 1, 0xFEEDFACEu);
        track.cmdPtr = stream2;
        ply_rept(&mp, &track);
        M4A_CHECK(track.cmdPtr == (u8*)(uintptr_t)0xFEEDFACEu);
        M4A_CHECK(track.repN == 0); /* unchanged on the count==0 path */

        /* count == 3: first two iterations take the goto. */
        memset(&mp, 0, sizeof(mp));
        memset(&track, 0, sizeof(track));
        stream2[0] = 3;
        M4A_WRITE_LE32(stream2, 1, 0x01020304u);
        track.cmdPtr = stream2;
        ply_rept(&mp, &track);
        M4A_CHECK(track.repN == 1);
        M4A_CHECK(track.cmdPtr == (u8*)(uintptr_t)0x01020304u);
        track.cmdPtr = stream2;
        ply_rept(&mp, &track);
        M4A_CHECK(track.repN == 2);
        M4A_CHECK(track.cmdPtr == (u8*)(uintptr_t)0x01020304u);
        /* Third iteration: repN reaches count, reset and skip payload. */
        track.cmdPtr = stream2;
        ply_rept(&mp, &track);
        M4A_CHECK(track.repN == 0);
        M4A_CHECK(track.cmdPtr == stream2 + 5);
    }

    /* ply_fine with NULL chan: the runtime case (silent host mixer
     * never connects channels). Just clears track->flags. */
    {
        memset(&mp, 0, sizeof(mp));
        memset(&track, 0, sizeof(track));
        track.flags = MPT_FLG_EXIST | MPT_FLG_VOLCHG | MPT_FLG_PITCHG;
        track.chan = NULL;
        ply_fine(&mp, &track);
        M4A_CHECK(track.flags == 0);
    }

    /* ply_fine with one envelope-active SoundChannel: status gets
     * tagged with M4A_CHAN_FLAG_RELEASE, the chan->next == NULL
     * terminator stops the walk after one iteration, track->flags
     * cleared. */
    {
        SoundChannel chan;
        memset(&mp, 0, sizeof(mp));
        memset(&track, 0, sizeof(track));
        memset(&chan, 0, sizeof(chan));
        chan.statusFlags = 0x83; /* envelope-active (& 0xc7 != 0) */
        chan.next = 0;
        track.chan = &chan;
        track.flags = MPT_FLG_EXIST;
        ply_fine(&mp, &track);
        M4A_CHECK(chan.statusFlags == (0x83 | M4A_CHAN_FLAG_RELEASE));
        M4A_CHECK(track.flags == 0);
    }

    /* ply_fine with chan->statusFlags & 0xc7 == 0: RealClearChain
     * (no-op host stub) is still called unconditionally per the asm,
     * but the release flag is NOT set because the M4A_CHAN_FLAGS_ENV
     * gate is closed. */
    {
        SoundChannel chan;
        memset(&mp, 0, sizeof(mp));
        memset(&track, 0, sizeof(track));
        memset(&chan, 0, sizeof(chan));
        chan.statusFlags = 0x08; /* not in M4A_CHAN_FLAGS_ENV */
        chan.next = 0;
        track.chan = &chan;
        ply_fine(&mp, &track);
        M4A_CHECK(chan.statusFlags == 0x08); /* unchanged */
        M4A_CHECK(track.flags == 0);
    }

    /* ply_endtie chan walk: matches the first channel whose
     * (statusFlags & M4A_CHAN_FLAGS_EXIST) != 0,
     * (statusFlags & M4A_CHAN_FLAG_RELEASE) == 0, and
     * midiKey == target_key. */
    {
        SoundChannel chan;
        u8 stream2[2];
        memset(&mp, 0, sizeof(mp));
        memset(&track, 0, sizeof(track));
        memset(&chan, 0, sizeof(chan));
        chan.statusFlags = 0x83; /* exists */
        chan.midiKey = 0x3C;
        chan.next = 0;
        stream2[0] = 0x3C; /* target key */
        track.cmdPtr = stream2;
        track.chan = &chan;
        ply_endtie(&mp, &track);
        M4A_CHECK(track.cmdPtr == stream2 + 1);
        M4A_CHECK(track.key == 0x3C);
        M4A_CHECK(chan.statusFlags == (0x83 | M4A_CHAN_FLAG_RELEASE));

        /* Mismatched midiKey: chan untouched. */
        memset(&track, 0, sizeof(track));
        memset(&chan, 0, sizeof(chan));
        chan.statusFlags = 0x83;
        chan.midiKey = 0x40; /* != target */
        chan.next = 0;
        stream2[0] = 0x3C;
        track.cmdPtr = stream2;
        track.chan = &chan;
        ply_endtie(&mp, &track);
        M4A_CHECK(chan.statusFlags == 0x83); /* untouched */

        /* Already releasing: skipped. */
        memset(&track, 0, sizeof(track));
        memset(&chan, 0, sizeof(chan));
        chan.statusFlags = 0x83 | M4A_CHAN_FLAG_RELEASE;
        chan.midiKey = 0x3C;
        chan.next = 0;
        stream2[0] = 0x3C;
        track.cmdPtr = stream2;
        track.chan = &chan;
        ply_endtie(&mp, &track);
        M4A_CHECK(chan.statusFlags == (0x83 | M4A_CHAN_FLAG_RELEASE));

        /* >= 0x80 path with chan walk: cmdPtr unchanged, key
         * preserved, channel still scanned against the saved key. */
        memset(&track, 0, sizeof(track));
        memset(&chan, 0, sizeof(chan));
        chan.statusFlags = 0x83;
        chan.midiKey = 0x55;
        chan.next = 0;
        stream2[0] = 0x80; /* >= 0x80, no consume */
        track.cmdPtr = stream2;
        track.key = 0x55;
        track.chan = &chan;
        ply_endtie(&mp, &track);
        M4A_CHECK(track.cmdPtr == stream2);
        M4A_CHECK(track.key == 0x55);
        M4A_CHECK(chan.statusFlags == (0x83 | M4A_CHAN_FLAG_RELEASE));
    }

    /* ----------------------------------------------------------- */
    /* PR #7 part 2.2.2.2.1: MPlayMain dispatcher.                 */
    /*                                                             */
    /* Runs MPlayMain against a synthesized stack-allocated         */
    /* MusicPlayerInfo + array of MusicPlayerTracks driven by a    */
    /* hand-rolled cmd-byte stream. Verifies:                      */
    /*   - ident sentinel: != ID_NUMBER short-circuits.            */
    /*   - status sign bit: short-circuits before tempo work.      */
    /*   - tempo accumulator: tempoI=150 ticks once per call;      */
    /*     tempoC retains the unconsumed remainder.                */
    /*   - wait command (0x80..0xB0): sets track->wait via          */
    /*     gClockTable[]; subsequent ticks decrement wait.         */
    /*   - jump table command (0xB1..0xCE): ply_prio updates       */
    /*     track->priority.                                        */
    /*   - running status: a < 0x80 byte after a >= 0xBD opcode    */
    /*     reuses runningStatus without advancing cmdPtr.          */
    /*   - ply_fine kills the track (flags=0) — when no other      */
    /*     tracks have EXIST, mp->status = M4A_STATUS_OFF.         */
    /*   - clock counter advances once per inner-loop tick.        */
    /*   - LFO modulation tick: lfoSpeedC accumulates, modM        */
    /*     updates, MPT_FLG_PITCHG set when modT == 0.             */
    /*                                                             */
    /* gMPlayJumpTable[] is populated at the top of this section   */
    /* via MPlayJumpTableCopy() so the dispatcher can route to     */
    /* the real handlers. (m4aSoundInit() does the same at boot,   */
    /* but the self-check runs before m4aSoundInit().)             */
    /* ----------------------------------------------------------- */

    /* Populate gMPlayJumpTable[] from the template so the         */
    /* dispatcher's `gMPlayJumpTable[cmd - 0xB1]` lookups resolve  */
    /* to the right ply_* handlers. */
    MPlayJumpTableCopy(gMPlayJumpTable);

    /* ident != ID_NUMBER on entry: short-circuit; nothing changes. */
    {
        MusicPlayerInfo mp_local;
        memset(&mp_local, 0, sizeof(mp_local));
        mp_local.ident = 0; /* not ID_NUMBER */
        mp_local.tempoI = 150;
        mp_local.tempoC = 0;
        MPlayMain(&mp_local);
        M4A_CHECK(mp_local.ident == 0);  /* untouched */
        M4A_CHECK(mp_local.tempoC == 0); /* untouched */
        M4A_CHECK(mp_local.clock == 0);  /* untouched */
    }

    /* status sign-bit set: enters but skips tempo work. ident is  */
    /* restored to ID_NUMBER on exit. */
    {
        MusicPlayerInfo mp_local;
        memset(&mp_local, 0, sizeof(mp_local));
        mp_local.ident = M4A_ID_NUMBER;
        mp_local.status = M4A_STATUS_OFF; /* sign bit set */
        mp_local.tempoI = 150;
        mp_local.tempoC = 0;
        MPlayMain(&mp_local);
        M4A_CHECK(mp_local.ident == M4A_ID_NUMBER);
        M4A_CHECK(mp_local.status == M4A_STATUS_OFF); /* unchanged */
        M4A_CHECK(mp_local.tempoC == 0);              /* no tick */
        M4A_CHECK(mp_local.clock == 0);
    }

    /* Tempo accumulator with no live tracks: trackCount=0 means    */
    /* the per-track loop is a no-op, acc stays 0, and mp->status   */
    /* gets the sign-bit "off" sentinel. */
    {
        MusicPlayerInfo mp_local;
        memset(&mp_local, 0, sizeof(mp_local));
        mp_local.ident = M4A_ID_NUMBER;
        mp_local.tempoI = 150;
        mp_local.tempoC = 0;
        mp_local.trackCount = 0;
        mp_local.tracks = NULL;
        MPlayMain(&mp_local);
        M4A_CHECK(mp_local.status == M4A_STATUS_OFF);
        M4A_CHECK(mp_local.clock == 1); /* one tick happened */
        M4A_CHECK(mp_local.ident == M4A_ID_NUMBER);
    }

    /* Tempo remainder: tempoI=100 doesn't reach a tick (need 150).
     * tempoC accumulates, no inner-loop iterations. */
    {
        MusicPlayerInfo mp_local;
        memset(&mp_local, 0, sizeof(mp_local));
        mp_local.ident = M4A_ID_NUMBER;
        mp_local.tempoI = 100;
        mp_local.tempoC = 0;
        mp_local.trackCount = 0;
        MPlayMain(&mp_local);
        M4A_CHECK(mp_local.tempoC == 100);
        M4A_CHECK(mp_local.clock == 0);
        /* status untouched (still 0) since the inner loop never ran. */
        M4A_CHECK(mp_local.status == 0);
    }

    /* Dispatcher: wait command (0x80..0xB0) sets track->wait. With
     * gClockTable[0x18] = 0x18 (third row, first entry from
     * src/gba/m4a.c), opcode 0x98 (= 0x80 + 0x18) produces wait=0x18.
     * After one tick, wait decrements to 0x17. */
    {
        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        u8 stream_local[8];
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        mp_local.ident = M4A_ID_NUMBER;
        mp_local.tempoI = 150;
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        track_local.flags = M4A_FLG_EXIST;
        stream_local[0] = 0x98; /* wait command, gClockTable[0x18] */
        stream_local[1] = 0xB1; /* ply_fine: never reached (wait != 0) */
        track_local.cmdPtr = stream_local;
        MPlayMain(&mp_local);
        M4A_CHECK(mp_local.clock == 1);
        M4A_CHECK(mp_local.status == 1);
        /* gClockTable[0x18] = 0x18 (the 25th entry, index 0x18,
         * which is the boundary between the unit-step row 0..0x10
         * and the rounder ticks at index 0x19+). After one
         * decrement: 0x18 - 1 = 0x17. */
        M4A_CHECK(track_local.wait == 0x18 - 1);
        M4A_CHECK(track_local.cmdPtr == stream_local + 1); /* opcode consumed */
        M4A_CHECK(track_local.flags == M4A_FLG_EXIST);     /* still alive */
    }

    /* Dispatcher: ply_prio (opcode 0xBA = jump-table[9]). Sets
     * priority, no flag side-effect; subsequent wait command stops
     * the dispatch. */
    {
        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        u8 stream_local[8];
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        mp_local.ident = M4A_ID_NUMBER;
        mp_local.tempoI = 150;
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        track_local.flags = M4A_FLG_EXIST;
        stream_local[0] = 0xBA; /* ply_prio */
        stream_local[1] = 0x55; /* priority value */
        stream_local[2] = 0x80; /* wait, gClockTable[0] = 0x00 */
        stream_local[3] = 0x80; /* second wait — but wait==0 from 0x80 keeps draining */
        stream_local[4] = 0x81; /* wait, gClockTable[1] = 0x01 */
        track_local.cmdPtr = stream_local;
        MPlayMain(&mp_local);
        /* ply_prio + ply_prio operand (2 bytes) + 0x80 (wait=0) +
         * 0x80 (wait=0) + 0x81 (wait=1) = 5 bytes consumed. */
        M4A_CHECK(track_local.priority == 0x55);
        M4A_CHECK(track_local.cmdPtr == stream_local + 5);
        M4A_CHECK(track_local.wait == 0); /* 1 - 1 = 0 after tick */
        M4A_CHECK(mp_local.status == 1);
        M4A_CHECK(mp_local.clock == 1);
    }

    /* Running status: opcode 0xBE (ply_vol, >= 0xBD) latches
     * runningStatus. The next byte is its operand (vol value).
     * After that, a byte < 0x80 reuses runningStatus = 0xBE without
     * advancing past the byte before the operand read. */
    {
        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        u8 stream_local[8];
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        mp_local.ident = M4A_ID_NUMBER;
        mp_local.tempoI = 150;
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        track_local.flags = M4A_FLG_EXIST;
        stream_local[0] = 0xBE; /* ply_vol */
        stream_local[1] = 0x40; /* vol value (operand for ply_vol) */
        stream_local[2] = 0x60; /* < 0x80: running-status reuse;
                                 * dispatcher leaves cmdPtr at this
                                 * byte, ply_vol consumes it as its
                                 * operand. */
        stream_local[3] = 0x81; /* wait=1 (gClockTable[1] = 1) */
        track_local.cmdPtr = stream_local;
        MPlayMain(&mp_local);
        /* After: ply_vol(0x40) -> ply_vol(0x60) -> wait=1 -> 0.
         * cmdPtr advanced past: 0xBE 0x40 (2) + 0x60 (1, no opcode
         * advance for the running-status reuse) + 0x81 (1) = 4. */
        M4A_CHECK(track_local.vol == 0x60); /* second update wins */
        M4A_CHECK(track_local.runningStatus == 0xBE);
        M4A_CHECK(track_local.cmdPtr == stream_local + 4);
        /* PR #7 part 2.2.2.2.3: the second loop runs TrkVolPitSet on
         * the track (which clears VOLSET) and then `flags &= 0xF0`,
         * so VOLCHG is consumed by the end of the tick. EXIST stays
         * set. */
        M4A_CHECK((track_local.flags & 0x0F) == 0);
        M4A_CHECK((track_local.flags & MPT_FLG_EXIST) == MPT_FLG_EXIST);
    }

    /* ply_fine kills the track. With trackCount==1 and ply_fine as
     * the first command, the inner loop's track-killed early-out
     * triggers, acc stays 0, and mp->status gets the OFF sentinel. */
    {
        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        u8 stream_local[8];
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        mp_local.ident = M4A_ID_NUMBER;
        mp_local.tempoI = 150;
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        track_local.flags = M4A_FLG_EXIST;
        track_local.chan = NULL;
        stream_local[0] = 0xB1; /* ply_fine (jump-table[0]) */
        track_local.cmdPtr = stream_local;
        MPlayMain(&mp_local);
        M4A_CHECK(track_local.flags == 0);
        /* acc stays 0 because the inner loop's `goto next_track`
         * fires before `acc |= bit` would matter — wait, the asm
         * actually OR's acc *before* the dispatcher runs. So after
         * the kill, acc still has the bit set, which means mp->status
         * comes out as 1 (the bit), not OFF. Verify against the asm. */
        M4A_CHECK(mp_local.status == 1);
        /* Subsequent calls find no live track, so status flips to OFF. */
        track_local.cmdPtr = stream_local; /* irrelevant — flags=0 means skipped */
        MPlayMain(&mp_local);
        M4A_CHECK(mp_local.status == M4A_STATUS_OFF);
    }

    /* LFO modulation tick: with lfoSpeed != 0, mod != 0,
     * lfoDelayC == 0, the wait-decrement step also advances the LFO
     * accumulator and updates modM (and the corresponding MODCHG
     * flag). Use a track that's already mid-wait so the dispatcher
     * doesn't need to consume any bytes — only the wait decrement +
     * LFO tick fires. */
    {
        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        mp_local.ident = M4A_ID_NUMBER;
        mp_local.tempoI = 150;
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        track_local.flags = M4A_FLG_EXIST;
        track_local.wait = 5;
        track_local.lfoSpeed = 0x10;
        track_local.mod = 0x40;
        track_local.lfoSpeedC = 0;
        track_local.lfoDelayC = 0;
        track_local.modT = 0; /* pitch */
        MPlayMain(&mp_local);
        M4A_CHECK(track_local.wait == 4);
        M4A_CHECK(track_local.lfoSpeedC == 0x10);
        /* lsc=0x10, (s8)(0x10-0x40) < 0 => r2=(s8)0x10=0x10;
         * newModM = (0x40 * 0x10) >> 6 = 0x10. */
        M4A_CHECK((s8)track_local.modM == 0x10);
        /* PR #7 part 2.2.2.2.3: the LFO tick set PITCHG; the second
         * loop then runs TrkVolPitSet (clears PITSET) and `flags
         * &= 0xF0`, so PITCHG is consumed by the end of the tick. */
        M4A_CHECK((track_local.flags & 0x0F) == 0);
        M4A_CHECK((track_local.flags & MPT_FLG_EXIST) == MPT_FLG_EXIST);
    }

    /* LFO modulation with lfoDelayC > 0: only delays decrement,
     * lfoSpeedC and modM untouched. */
    {
        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        mp_local.ident = M4A_ID_NUMBER;
        mp_local.tempoI = 150;
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        track_local.flags = M4A_FLG_EXIST;
        track_local.wait = 5;
        track_local.lfoSpeed = 0x10;
        track_local.mod = 0x40;
        track_local.lfoSpeedC = 0;
        track_local.lfoDelayC = 3;
        MPlayMain(&mp_local);
        M4A_CHECK(track_local.wait == 4);
        M4A_CHECK(track_local.lfoDelayC == 2);
        M4A_CHECK(track_local.lfoSpeedC == 0); /* untouched */
        M4A_CHECK(track_local.modM == 0);
    }

    /* START flag: clears the track's dispatcher state and applies
     * the asm-matching defaults. The dispatcher also exits early
     * for this iteration without draining commands. */
    {
        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        u8 stream_local[8];
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        mp_local.ident = M4A_ID_NUMBER;
        mp_local.tempoI = 150;
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        track_local.flags = M4A_FLG_EXIST | M4A_FLG_START;
        track_local.priority = 0x77; /* would be cleared by START */
        track_local.cmdPtr = stream_local;
        MPlayMain(&mp_local);
        M4A_CHECK(track_local.flags == M4A_FLG_EXIST);
        M4A_CHECK(track_local.wait == 2);
        M4A_CHECK(track_local.pitX == 0x40);
        M4A_CHECK(track_local.lfoSpeed == 0x16);
        M4A_CHECK(track_local.tone.type == 1);
        M4A_CHECK(track_local.priority == 0);          /* cleared */
        M4A_CHECK(track_local.cmdPtr == stream_local); /* untouched */
    }

    /* ----------------------------------------------------------- */
    /* PR #7 part 2.2.2.2.2.1: ply_voice.                          */
    /*                                                             */
    /* Verifies the standalone handler (cmdPtr advance + per-field */
    /* ToneData copy) and the MPlayMain dispatch path through      */
    /* gMPlayJumpTable[12] (opcode 0xBD). The asm clones           */
    /* `mp->tone[index]` into `track->tone` as 3 back-to-back      */
    /* 4-byte loads/stores, but the host port uses a struct        */
    /* assignment so it survives the wider `wav` pointer field on  */
    /* 64-bit hosts.                                               */
    /* ----------------------------------------------------------- */
    {
        ToneData bank[3];
        memset(bank, 0, sizeof(bank));
        /* Distinguishing patterns per slot so any cross-slot read
         * shows up as a wrong-field assertion failure. */
        bank[0].type = 0x01;
        bank[0].key = 0x10;
        bank[0].length = 0x20;
        bank[0].pan_sweep = 0x30;
        bank[0].wav = (WaveData*)(uintptr_t)0x11111111u;
        bank[0].attack = 0x41;
        bank[0].decay = 0x51;
        bank[0].sustain = 0x61;
        bank[0].release = 0x71;
        bank[1].type = 0x02;
        bank[1].key = 0x12;
        bank[1].length = 0x22;
        bank[1].pan_sweep = 0x32;
        bank[1].wav = (WaveData*)(uintptr_t)0x22222222u;
        bank[1].attack = 0x42;
        bank[1].decay = 0x52;
        bank[1].sustain = 0x62;
        bank[1].release = 0x72;
        bank[2].type = 0x03;
        bank[2].key = 0x13;
        bank[2].length = 0x23;
        bank[2].pan_sweep = 0x33;
        bank[2].wav = (WaveData*)(uintptr_t)0x33333333u;
        bank[2].attack = 0x43;
        bank[2].decay = 0x53;
        bank[2].sustain = 0x63;
        bank[2].release = 0x73;

        /* Standalone ply_voice with index 1: every field of
         * track.tone must equal bank[1] and cmdPtr must advance by 1. */
        u8 stream_local[2];
        memset(&mp, 0, sizeof(mp));
        memset(&track, 0, sizeof(track));
        mp.tone = bank;
        stream_local[0] = 1; /* voice index */
        track.cmdPtr = stream_local;
        ply_voice(&mp, &track);
        M4A_CHECK(track.cmdPtr == stream_local + 1);
        M4A_CHECK(track.tone.type == bank[1].type);
        M4A_CHECK(track.tone.key == bank[1].key);
        M4A_CHECK(track.tone.length == bank[1].length);
        M4A_CHECK(track.tone.pan_sweep == bank[1].pan_sweep);
        M4A_CHECK(track.tone.wav == bank[1].wav);
        M4A_CHECK(track.tone.attack == bank[1].attack);
        M4A_CHECK(track.tone.decay == bank[1].decay);
        M4A_CHECK(track.tone.sustain == bank[1].sustain);
        M4A_CHECK(track.tone.release == bank[1].release);
        M4A_CHECK(track.flags == 0); /* no flag side-effects */

        /* Boundary check at index 0 picks up bank[0], not bank[1]. */
        memset(&track, 0, sizeof(track));
        stream_local[0] = 0;
        track.cmdPtr = stream_local;
        ply_voice(&mp, &track);
        M4A_CHECK(track.tone.type == bank[0].type);
        M4A_CHECK(track.tone.wav == bank[0].wav);
        M4A_CHECK(track.tone.release == bank[0].release);

        /* Dispatcher path: opcode 0xBD = M4A_JUMP_BASE + 12 = ply_voice
         * via gMPlayJumpTable. Followed by an operand byte (voice
         * index) and a wait command so MPlayMain stops after the
         * voice load. */
        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        u8 disp_stream[8];
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        mp_local.ident = M4A_ID_NUMBER;
        mp_local.tempoI = 150;
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        mp_local.tone = bank;
        track_local.flags = M4A_FLG_EXIST;
        disp_stream[0] = 0xBD; /* ply_voice */
        disp_stream[1] = 2;    /* voice index */
        disp_stream[2] = 0x81; /* wait, gClockTable[1] = 0x01 */
        track_local.cmdPtr = disp_stream;
        MPlayMain(&mp_local);
        M4A_CHECK(track_local.cmdPtr == disp_stream + 3);
        M4A_CHECK(track_local.tone.type == bank[2].type);
        M4A_CHECK(track_local.tone.wav == bank[2].wav);
        M4A_CHECK(track_local.tone.release == bank[2].release);
        M4A_CHECK(track_local.runningStatus == 0xBD);
        M4A_CHECK(track_local.wait == 0); /* 1 - 1 = 0 after tick */
    }

    /* ----------------------------------------------------------- */
    /* PR #7 part 2.2.2.2.2.2: ply_note.                           */
    /*                                                             */
    /* Verifies the standalone handler against synthesized         */
    /* SoundInfo / SoundChannel arrays. Each sub-test sets up a    */
    /* fresh SoundInfo (cast over the host gSoundInfo BSS via the  */
    /* host-mirror M4A_SoundInfo struct), wires up an active tone, */
    /* runs ply_note_impl, and asserts the channel-allocation walk */
    /* picked the right slot, the chan was inserted at             */
    /* track->chan, and the ADSR / wav / frequency / lower-nibble  */
    /* flag bits all match the asm contract.                       */
    /*                                                             */
    /* ply_note calls `ClearChain(chan)`, which dispatches through */
    /* `gMPlayJumpTable[34]` (the m4a engine's RealClearChain      */
    /* slot). That table isn't populated until `m4aSoundInit()` is */
    /* called by the engine, which happens AFTER Port_M4ASelfCheck */
    /* returns — so install the pointer manually here. The         */
    /* equivalent install in production happens via the            */
    /* `gMPlayJumpTableTemplate[]` copy in `m4a.c::SoundInit`. */
    /* ----------------------------------------------------------- */
    {
        extern void RealClearChain(void* x);
        gMPlayJumpTable[34] = (void*)RealClearChain;
    }

    {
        M4A_SoundInfo* si = (M4A_SoundInfo*)gSoundInfo;

        /* Empty-chan immediate alloc: with one envelope-dead chan,
         * ply_note picks chans[0] without bothering to scan further. */
        memset(si, 0, sizeof(*si));
        si->maxChannels = 4;
        /* All chans envelope-dead by default (statusFlags = 0). */
        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        ToneData my_tone;
        WaveData my_wav;
        u8 stream_local[8];
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        memset(&my_tone, 0, sizeof(my_tone));
        memset(&my_wav, 0, sizeof(my_wav));
        my_wav.freq = 0x10000000u; /* arbitrary nonzero base freq */
        my_tone.type = 0;          /* DirectSound (cgbType == 0) */
        my_tone.attack = 0xA0;
        my_tone.decay = 0xB0;
        my_tone.sustain = 0xC0;
        my_tone.release = 0xD0;
        my_tone.wav = &my_wav;
        track_local.tone = my_tone;
        track_local.priority = 0x10;
        track_local.flags = MPT_FLG_VOLSET | MPT_FLG_PITSET; /* lower nibble bits to clear */
        stream_local[0] = 0x3C;                              /* key */
        stream_local[1] = 0x40;                              /* velocity */
        stream_local[2] = 0x80;                              /* sentinel: skip gateTime delta */
        track_local.cmdPtr = stream_local;
        ply_note_impl(&mp_local, &track_local, /*gateIdx=*/0);

        M4A_CHECK(track_local.cmdPtr == stream_local + 2); /* key + velocity consumed */
        M4A_CHECK(track_local.key == 0x3C);
        M4A_CHECK(track_local.velocity == 0x40);
        /* gateTime = gClockTable[0] (= 0) — stays 0 since no delta. */
        M4A_CHECK(track_local.chan == &si->chans[0]);
        M4A_CHECK(si->chans[0].track == &track_local);
        M4A_CHECK(si->chans[0].statusFlags == 0x80);
        M4A_CHECK(si->chans[0].key == 0x3C);
        M4A_CHECK(si->chans[0].priority == 0x10);
        M4A_CHECK(si->chans[0].attack == 0xA0);
        M4A_CHECK(si->chans[0].decay == 0xB0);
        M4A_CHECK(si->chans[0].sustain == 0xC0);
        M4A_CHECK(si->chans[0].release == 0xD0);
        M4A_CHECK(si->chans[0].wav == &my_wav);
        M4A_CHECK(si->chans[0].next == 0); /* no prior head */
        M4A_CHECK(si->chans[0].prev == 0);
        M4A_CHECK(si->chans[0].frequency != 0); /* MidiKeyToFreq ran */
        /* track->flags &= 0xF0: lower nibble cleared. */
        M4A_CHECK((track_local.flags & 0x0f) == 0);
    }

    {
        /* Releasing-chan preference: chans[0] is envelope-active +
         * not releasing, chans[1] is releasing → ply_note picks
         * chans[1] (releasing wins over high-priority active). */
        M4A_SoundInfo* si = (M4A_SoundInfo*)gSoundInfo;
        memset(si, 0, sizeof(*si));
        si->maxChannels = 3;
        si->chans[0].statusFlags = 0x83;        /* envelope-active */
        si->chans[0].priority = 0x05;           /* low → would otherwise win */
        si->chans[1].statusFlags = 0x83 | 0x40; /* releasing */
        si->chans[1].priority = 0xFF;
        si->chans[2].statusFlags = 0x83; /* envelope-active, high prio */
        si->chans[2].priority = 0x80;

        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        WaveData wav;
        u8 stream_local[4];
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        memset(&wav, 0, sizeof(wav));
        wav.freq = 0x10000000u;
        track_local.tone.type = 0;
        track_local.tone.wav = &wav;
        track_local.priority = 0x40;
        stream_local[0] = 0x80; /* no key/velocity/delta */
        track_local.cmdPtr = stream_local;
        ply_note_impl(&mp_local, &track_local, /*gateIdx=*/0);

        M4A_CHECK(track_local.chan == &si->chans[1]);
        M4A_CHECK(si->chans[1].statusFlags == 0x80); /* reset to fresh "ready" */
    }

    {
        /* Priority compare among non-releasing chans: chans[0]
         * priority 0x80 (>= notePriority 0x40 → not evictable),
         * chans[1] priority 0x10 (< 0x40 → evict). */
        M4A_SoundInfo* si = (M4A_SoundInfo*)gSoundInfo;
        memset(si, 0, sizeof(*si));
        si->maxChannels = 3;
        si->chans[0].statusFlags = 0x83;
        si->chans[0].priority = 0x80;
        si->chans[1].statusFlags = 0x83;
        si->chans[1].priority = 0x10;
        si->chans[2].statusFlags = 0x83;
        si->chans[2].priority = 0x90;

        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        WaveData wav;
        u8 stream_local[4];
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        memset(&wav, 0, sizeof(wav));
        wav.freq = 0x10000000u;
        track_local.tone.type = 0;
        track_local.tone.wav = &wav;
        track_local.priority = 0x40;
        stream_local[0] = 0x80;
        track_local.cmdPtr = stream_local;
        ply_note_impl(&mp_local, &track_local, /*gateIdx=*/0);

        M4A_CHECK(track_local.chan == &si->chans[1]);
    }

    {
        /* No suitable chan: all chans envelope-active + high
         * priority → ply_note drops the note (track->chan stays
         * at its initial value, no statusFlags writes). */
        M4A_SoundInfo* si = (M4A_SoundInfo*)gSoundInfo;
        memset(si, 0, sizeof(*si));
        si->maxChannels = 2;
        si->chans[0].statusFlags = 0x83;
        si->chans[0].priority = 0xFF;
        si->chans[1].statusFlags = 0x83;
        si->chans[1].priority = 0xFF;

        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        u8 stream_local[4];
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        track_local.tone.type = 0;
        track_local.priority = 0x10;
        track_local.flags = MPT_FLG_VOLSET; /* should NOT be cleared on drop */
        stream_local[0] = 0x80;
        track_local.cmdPtr = stream_local;
        ply_note_impl(&mp_local, &track_local, /*gateIdx=*/0);

        M4A_CHECK(track_local.chan == NULL);
        M4A_CHECK(si->chans[0].statusFlags == 0x83); /* untouched */
        M4A_CHECK(si->chans[1].statusFlags == 0x83);
        /* Lower nibble flag NOT cleared on the drop path. */
        M4A_CHECK((track_local.flags & 0x0f) != 0);
    }

    {
        /* Head-of-list insertion: when track->chan already points
         * at an existing chan, the newly-allocated one becomes the
         * new head and chains forward to the old head. */
        M4A_SoundInfo* si = (M4A_SoundInfo*)gSoundInfo;
        memset(si, 0, sizeof(*si));
        si->maxChannels = 2;

        SoundChannel oldHead;
        memset(&oldHead, 0, sizeof(oldHead));
        oldHead.statusFlags = 0x83;

        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        WaveData wav;
        u8 stream_local[4];
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        memset(&wav, 0, sizeof(wav));
        wav.freq = 0x10000000u;
        track_local.tone.type = 0;
        track_local.tone.wav = &wav;
        track_local.chan = &oldHead; /* simulate prior allocation */
        stream_local[0] = 0x80;
        track_local.cmdPtr = stream_local;
        ply_note_impl(&mp_local, &track_local, /*gateIdx=*/0);

        M4A_CHECK(track_local.chan == &si->chans[0]);
        /* chan->next stores oldHead address, truncated to u32 — which
         * is faithful to the m4a SoundChannel layout. */
        M4A_CHECK(si->chans[0].next == (u32)(uintptr_t)&oldHead);
        M4A_CHECK(oldHead.prev == (u32)(uintptr_t)&si->chans[0]);
        M4A_CHECK(si->chans[0].prev == 0); /* new head has no predecessor */
    }

    {
        /* gateIdx + delta: gateTime = gClockTable[gateIdx] + delta. */
        M4A_SoundInfo* si = (M4A_SoundInfo*)gSoundInfo;
        memset(si, 0, sizeof(*si));
        si->maxChannels = 1;

        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        WaveData wav;
        u8 stream_local[6];
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        memset(&wav, 0, sizeof(wav));
        wav.freq = 0x10000000u;
        track_local.tone.type = 0;
        track_local.tone.wav = &wav;
        stream_local[0] = 0x3C; /* key */
        stream_local[1] = 0x40; /* velocity */
        stream_local[2] = 0x05; /* gateTime delta */
        stream_local[3] = 0x80;
        track_local.cmdPtr = stream_local;
        /* gateIdx 1 → gClockTable[1] = 1; + 5 = 6. */
        ply_note_impl(&mp_local, &track_local, /*gateIdx=*/1);
        M4A_CHECK(track_local.cmdPtr == stream_local + 3);
        M4A_CHECK(track_local.gateTime == (u8)(gClockTable[1] + 5));
        M4A_CHECK(si->chans[0].gateTime == track_local.gateTime);
    }

    {
        /* CGB path: tone.type & 7 in {1..6} routes to soundInfo->
         * cgbChannels[type-1]. With a synthesized cgb-chan slot,
         * verify chan picked, MidiKeyToCgbFreq invoked, etc. */
        M4A_SoundInfo* si = (M4A_SoundInfo*)gSoundInfo;
        memset(si, 0, sizeof(*si));
        si->maxChannels = 1;
        M4A_CgbChannel cgb[4];
        memset(cgb, 0, sizeof(cgb));
        si->cgbChannels = cgb;

        /* Leave si->MidiKeyToCgbFreq = NULL: the host port skips
         * the frequency lookup gracefully, so chan->frequency stays
         * at its memset(0) initial value and the rest of the chan
         * install path is what's exercised. */

        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        u8 stream_local[4];
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        track_local.tone.type = 2; /* CGB type 2 → cgbChannels[1] */
        track_local.tone.length = 0x20;
        track_local.tone.pan_sweep = 0x44;
        track_local.tone.attack = 0xA1;
        track_local.tone.decay = 0xA2;
        track_local.tone.sustain = 0xA3;
        track_local.tone.release = 0xA4;
        track_local.priority = 0x30;
        stream_local[0] = 0x3C; /* key */
        stream_local[1] = 0x80;
        track_local.cmdPtr = stream_local;
        ply_note_impl(&mp_local, &track_local, /*gateIdx=*/0);

        SoundChannel* picked = (SoundChannel*)&cgb[1];
        M4A_CHECK(track_local.chan == picked);
        M4A_CHECK(cgb[1].statusFlags == 0x80);
        M4A_CHECK(cgb[1].type == 2);
        M4A_CHECK(cgb[1].key == 0x3C);
        M4A_CHECK(cgb[1].priority == 0x30);
        M4A_CHECK(cgb[1].attack == 0xA1);
        M4A_CHECK(cgb[1].release == 0xA4);
        /* CGB-only byte pokes: subTone->length lands in cgb->length,
         * derived n4 byte lands in cgb->sweep (CgbChannel host
         * layout — see m4a.c). */
        M4A_CHECK(cgb[1].length == 0x20);
        /* pan_sweep 0x44: not & 0x80, but & 0x70 → n4 = 8. */
        M4A_CHECK(cgb[1].sweep == 8);
        M4A_CHECK(cgb[1].track == (u32)(uintptr_t)&track_local);
        /* MidiKeyToCgbFreq was NULL → frequency stays 0. */
        M4A_CHECK(cgb[1].frequency == 0);
    }

    {
        /* Dispatcher: opcode 0xCF (= M4A_NOTE_BASE) drives ply_note
         * with gateIdx = 0; opcode 0xD0 with gateIdx = 1. Use a
         * fresh SoundInfo with a single empty chan and verify the
         * note lands and runningStatus latches. */
        M4A_SoundInfo* si = (M4A_SoundInfo*)gSoundInfo;
        memset(si, 0, sizeof(*si));
        si->maxChannels = 2;

        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        WaveData wav;
        u8 stream_local[8];
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        memset(&wav, 0, sizeof(wav));
        wav.freq = 0x10000000u;
        mp_local.ident = M4A_ID_NUMBER;
        mp_local.tempoI = 150;
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        track_local.flags = M4A_FLG_EXIST;
        track_local.tone.type = 0;
        track_local.tone.wav = &wav;
        stream_local[0] = 0xD0; /* gateIdx = 1 */
        stream_local[1] = 0x3C; /* key */
        stream_local[2] = 0x40; /* velocity */
        stream_local[3] = 0x80; /* end of operands; wait command */
        stream_local[4] = 0x81; /* wait, gClockTable[1] = 1 */
        track_local.cmdPtr = stream_local;
        MPlayMain(&mp_local);

        /* runningStatus latched at 0xD0 (>= 0xBD). */
        M4A_CHECK(track_local.runningStatus == 0xD0);
        M4A_CHECK(track_local.chan == &si->chans[0]);
        M4A_CHECK(si->chans[0].key == 0x3C);
        M4A_CHECK(si->chans[0].statusFlags == 0x80);
        /* Wait was 1 → decremented to 0 by the LFO step. */
        M4A_CHECK(track_local.wait == 0);
    }

    /* ----------------------------------------------------------- */
    /* PR #7 part 2.2.2.2.3: per-track TrkVolPitSet second loop.  */
    /*                                                             */
    /* Verifies the asm `_080AFAB2..._080AFB60` second pass:       */
    /*   - !EXIST and (flags & 0xF)==0 tracks are skipped.         */
    /*   - TrkVolPitSet runs (clears VOLSET/PITSET via the C       */
    /*     impl in src/gba/m4a.c) when the low nibble has a bit.  */
    /*   - track->flags low nibble is cleared after the per-track  */
    /*     pass (whether or not chan!=NULL).                       */
    /*   - chan walk: dead-envelope chan -> ClearChain.            */
    /*   - chan walk: VOLCHG sets CgbChannel.modify bit 0 on CGB.  */
    /*   - chan walk: PITCHG drives MidiKeyToFreq for DirectSound  */
    /*     and MidiKeyToCgbFreq + modify bit 1 for CGB; negative   */
    /*     newKey clamps to 0.                                     */
    /*   - chan walk progresses past a NULL terminator.            */
    /* ----------------------------------------------------------- */

    /* Skip when (flags & 0xF) == 0: track is unchanged. */
    {
        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        track_local.flags = M4A_FLG_EXIST; /* low nibble = 0 */
        track_local.vol = 0xAB;
        track_local.volMR = 0x55;
        track_local.volML = 0x66;
        m4a_track_volpit_pass(&mp_local);
        M4A_CHECK(track_local.flags == M4A_FLG_EXIST);
        M4A_CHECK(track_local.volMR == 0x55); /* TrkVolPitSet not run */
        M4A_CHECK(track_local.volML == 0x66);
    }

    /* Skip when !EXIST: even with VOLCHG set, untouched. */
    {
        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        track_local.flags = MPT_FLG_VOLCHG; /* no EXIST */
        m4a_track_volpit_pass(&mp_local);
        M4A_CHECK(track_local.flags == MPT_FLG_VOLCHG);
    }

    /* VOLCHG with track->chan == NULL: TrkVolPitSet runs (volMR /
     * volML get computed from vol/volX/pan/panX), low nibble
     * cleared, no chan walk. */
    {
        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        track_local.flags = M4A_FLG_EXIST | MPT_FLG_VOLCHG;
        track_local.vol = 0x40;
        track_local.volX = 0x40;
        track_local.pan = 0;
        track_local.panX = 0;
        m4a_track_volpit_pass(&mp_local);
        /* Low nibble cleared, EXIST preserved. */
        M4A_CHECK(track_local.flags == M4A_FLG_EXIST);
        /* TrkVolPitSet computes:
         *   x = (vol * volX) >> 5 = (0x40 * 0x40) >> 5 = 128
         *   y = 2*pan + panX = 0
         *   volMR = ((y+128) * x) >> 8 = (128 * 128) >> 8 = 64
         *   volML = ((127-y) * x) >> 8 = (127 * 128) >> 8 = 63 */
        M4A_CHECK(track_local.volMR == 64);
        M4A_CHECK(track_local.volML == 63);
    }

    /* PITCHG with track->chan == NULL: TrkVolPitSet runs (keyM /
     * pitM get recomputed), low nibble cleared. */
    {
        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        track_local.flags = M4A_FLG_EXIST | MPT_FLG_PITCHG;
        track_local.keyShift = 1;
        track_local.keyShiftX = 0;
        track_local.tune = 0;
        track_local.bend = 0;
        track_local.bendRange = 0;
        track_local.modT = 0;
        track_local.modM = 0;
        m4a_track_volpit_pass(&mp_local);
        M4A_CHECK(track_local.flags == M4A_FLG_EXIST);
        /* x = ((keyShift << 8) + ...) = 256; keyM = 1, pitM = 0. */
        M4A_CHECK(track_local.keyM == 1);
        M4A_CHECK(track_local.pitM == 0);
    }

    /* VOLCHG with a dead-envelope chan: ClearChain is called (silent
     * no-op stub here), the chan is walked, the loop terminates via
     * chan->next == NULL, and the track flags low nibble is cleared. */
    {
        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        SoundChannel chan;
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        memset(&chan, 0, sizeof(chan));
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        track_local.flags = M4A_FLG_EXIST | MPT_FLG_VOLCHG;
        track_local.chan = &chan;
        chan.statusFlags = 0x08; /* not in M4A_CHAN_FLAGS_ENV (0xc7) */
        chan.key = 60;
        chan.next = 0;
        m4a_track_volpit_pass(&mp_local);
        M4A_CHECK(track_local.flags == M4A_FLG_EXIST);
        /* statusFlags untouched by ClearChain stub. */
        M4A_CHECK(chan.statusFlags == 0x08);
    }

    /* PITCHG with a live DirectSound chan (type & 7 == 0): the chan
     * walk computes newKey = chan->key + (s8)track->keyM and writes
     * MidiKeyToFreq(chan->wav, newKey, track->pitM) into
     * chan->frequency. */
    {
        M4A_SoundInfo* si = (M4A_SoundInfo*)gSoundInfo;
        memset(si, 0, sizeof(*si));

        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        SoundChannel chan;
        WaveData wav;
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        memset(&chan, 0, sizeof(chan));
        memset(&wav, 0, sizeof(wav));
        wav.freq = 0x10000000u;
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        track_local.flags = M4A_FLG_EXIST | MPT_FLG_PITCHG;
        track_local.chan = &chan;
        track_local.keyM = 0;
        track_local.pitM = 0;
        /* TrkVolPitSet inputs that produce keyM == 0: leave all the
         * key/tune/bend/modM fields zero so the recompute stays at 0. */
        chan.statusFlags = 0x83;        /* in M4A_CHAN_FLAGS_ENV */
        chan.type = 0;                  /* DirectSound */
        chan.key = 60;
        chan.frequency = 0xDEADBEEF;    /* sentinel; will be overwritten */
        chan.wav = &wav;
        chan.next = 0;
        m4a_track_volpit_pass(&mp_local);
        M4A_CHECK(track_local.flags == M4A_FLG_EXIST);
        /* MidiKeyToFreq returned a real value (replaced sentinel). */
        M4A_CHECK(chan.frequency != 0xDEADBEEF);
    }

    /* PITCHG with negative-key clamp: chan->key + (s8)track->keyM < 0
     * clamps to 0 before MidiKeyToFreq. */
    {
        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        SoundChannel chan, chan_ref;
        WaveData wav;
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        memset(&chan, 0, sizeof(chan));
        memset(&chan_ref, 0, sizeof(chan_ref));
        memset(&wav, 0, sizeof(wav));
        wav.freq = 0x10000000u;
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        track_local.flags = M4A_FLG_EXIST | MPT_FLG_PITCHG;
        track_local.chan = &chan;
        chan.statusFlags = 0x83;
        chan.type = 0;
        chan.key = 5;
        chan.wav = &wav;
        chan.next = 0;
        /* Inject keyM = -10 directly via the post-TrkVolPitSet path:
         * TrkVolPitSet would overwrite keyM. Bypass by clearing
         * PITCHG's PITSET bit so TrkVolPitSet's "if (PITSET)" leaves
         * keyM/pitM untouched. PITCHG = PITSET|something (0x0c =
         * 0x04|0x08); the bit that gates the recompute is PITSET=0x04
         * — clear it but leave 0x08 so the second loop still sees
         * PITCHG. */
        track_local.flags = M4A_FLG_EXIST | 0x08; /* PITCHG without PITSET */
        track_local.keyM = (u8)(s8)(-10);
        m4a_track_volpit_pass(&mp_local);

        /* Reference: clamp(5 + (-10), 0) = 0; MidiKeyToFreq(wav, 0, 0). */
        u32 expected = MidiKeyToFreq(&wav, 0, 0);
        M4A_CHECK(chan.frequency == expected);
    }

    /* VOLCHG + PITCHG with a live CGB chan (type & 7 == 2): the
     * chan walk uses MidiKeyToCgbFreq and sets CgbChannel.modify
     * bits 0|1. */
    {
        M4A_SoundInfo* si = (M4A_SoundInfo*)gSoundInfo;
        memset(si, 0, sizeof(*si));
        /* Provide a MidiKeyToCgbFreq impl. The asm one is the C
         * function in src/gba/m4a.c.) */
        extern u32 MidiKeyToCgbFreq(u8 chanNum, u8 key, u8 fineAdjust);
        si->MidiKeyToCgbFreq = MidiKeyToCgbFreq;

        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        M4A_CgbChannel cgb;
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        memset(&cgb, 0, sizeof(cgb));
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        /* PITCHG (high bit of nibble) without PITSET (low bit) so
         * TrkVolPitSet leaves keyM/pitM untouched. */
        track_local.flags = M4A_FLG_EXIST | MPT_FLG_VOLCHG | 0x08;
        track_local.chan = (SoundChannel*)&cgb;
        track_local.keyM = 0;
        track_local.pitM = 0;
        cgb.statusFlags = 0x83;
        cgb.type = 2;            /* CGB type 2 */
        cgb.key = 60;
        cgb.modify = 0;
        cgb.frequency = 0;
        cgb.next = 0;
        m4a_track_volpit_pass(&mp_local);

        M4A_CHECK(track_local.flags == M4A_FLG_EXIST);
        /* modify bit 0 set by VOLCHG path; bit 1 set by PITCHG. */
        M4A_CHECK(cgb.modify == 3);
        u32 expectedCgb = MidiKeyToCgbFreq(2, 60, 0);
        M4A_CHECK(cgb.frequency == expectedCgb);
    }

    /* PITCHG with a CGB chan but soundInfo->MidiKeyToCgbFreq == NULL:
     * frequency is left untouched (host port skips the callback
     * gracefully); modify bit 1 still gets set. */
    {
        M4A_SoundInfo* si = (M4A_SoundInfo*)gSoundInfo;
        memset(si, 0, sizeof(*si));
        /* MidiKeyToCgbFreq stays NULL. */

        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        M4A_CgbChannel cgb;
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        memset(&cgb, 0, sizeof(cgb));
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        track_local.flags = M4A_FLG_EXIST | 0x08; /* PITCHG, no PITSET */
        track_local.chan = (SoundChannel*)&cgb;
        cgb.statusFlags = 0x83;
        cgb.type = 1;
        cgb.key = 60;
        cgb.modify = 0;
        cgb.frequency = 0xDEADBEEF;
        cgb.next = 0;
        m4a_track_volpit_pass(&mp_local);

        M4A_CHECK(cgb.frequency == 0xDEADBEEF); /* untouched */
        M4A_CHECK(cgb.modify == 2);             /* bit 1 still set */
    }

    /* Multi-chan walk: NULL-terminated 2-chan chain via chan->next.
     * The walk processes both chans in order and terminates on NULL.
     * (The self-loop break in the walker is preserved for parity
     * with the asm but isn't testable on a 64-bit host because
     * SoundChannel.next is a u32 and stack addresses don't fit.) */
    {
        MusicPlayerInfo mp_local;
        MusicPlayerTrack track_local;
        SoundChannel chan_a, chan_b;
        WaveData wav;
        memset(&mp_local, 0, sizeof(mp_local));
        memset(&track_local, 0, sizeof(track_local));
        memset(&chan_a, 0, sizeof(chan_a));
        memset(&chan_b, 0, sizeof(chan_b));
        memset(&wav, 0, sizeof(wav));
        wav.freq = 0x10000000u;
        mp_local.trackCount = 1;
        mp_local.tracks = &track_local;
        track_local.flags = M4A_FLG_EXIST | MPT_FLG_VOLCHG;
        track_local.chan = &chan_a;
        chan_a.statusFlags = 0x83;
        chan_a.type = 0;
        chan_a.key = 60;
        chan_a.wav = &wav;
        /* chan_a.next is u32; widening from a stack pointer would
         * truncate. Verify the walk progresses past chan_a using
         * a dead-envelope second chan reachable via uintptr_t →
         * u32 truncation: skip by leaving NULL termination. The
         * 2-chan walk is exercised by the engine's real chan
         * arena (where addresses fit u32) at runtime. */
        chan_a.next = 0;
        (void)chan_b;
        m4a_track_volpit_pass(&mp_local);
        M4A_CHECK(track_local.flags == M4A_FLG_EXIST);
        M4A_CHECK(chan_a.statusFlags == 0x83); /* unchanged by walk */
    }

    /* Multi-track: trackCount=3 with mixed flag states; verify the
     * loop processes each one independently. */
    {
        MusicPlayerInfo mp_local;
        MusicPlayerTrack tracks[3];
        memset(&mp_local, 0, sizeof(mp_local));
        memset(tracks, 0, sizeof(tracks));
        mp_local.trackCount = 3;
        mp_local.tracks = tracks;
        tracks[0].flags = M4A_FLG_EXIST | MPT_FLG_VOLCHG;
        tracks[1].flags = MPT_FLG_VOLCHG; /* !EXIST -> skip */
        tracks[2].flags = M4A_FLG_EXIST;  /* low nibble 0 -> skip */
        m4a_track_volpit_pass(&mp_local);
        M4A_CHECK(tracks[0].flags == M4A_FLG_EXIST);  /* nibble cleared */
        M4A_CHECK(tracks[1].flags == MPT_FLG_VOLCHG); /* untouched */
        M4A_CHECK(tracks[2].flags == M4A_FLG_EXIST);  /* untouched */
    }

    /* Restore gSoundInfo to a zero state so any other code that
     * looks at it post-self-check sees a clean slate (matches the
     * zero-fill that m4aSoundInit will perform when the engine
     * boots). */
    memset(gSoundInfo, 0, sizeof(gSoundInfo));

#undef M4A_WRITE_LE32
#undef M4A_RUN

    return 0;
}

#undef M4A_CHECK
