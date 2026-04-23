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

/* Forward decl for `ply_note` / `ply_voice` (still no-op stubs for
 * 2.2.2.2.1 — promoted to real ports in 2.2.2.2.2). The dispatcher
 * needs the prototype so the call site below typechecks. */
void ply_note(MusicPlayerInfo* mp, MusicPlayerTrack* track);
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
         * `ply_note(u32 gateIdx, mp, track)`). Our C `ply_note` is a
         * still a no-op stub from 2.2.2.1 with the m4a.c-declared
         * `(mp, track)` shape, so the gate index is intentionally
         * discarded here. PR #7 part 2.2.2.2.2 will land the real
         * `ply_note` and revisit this dispatch site to thread the
         * gate index through (likely via a host-private 3-arg
         * variant called from here, with the 2-arg `ply_note`
         * symbol kept around as a thin trampoline that reads a
         * dispatcher-stashed gateIdx). */
        (void)cmd_byte; /* gate index unused while ply_note is a stub */
        ply_note(mp, track);
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

/* MPlayMain: tempo-stepped command dispatcher. This is the asm's
 * `MPlayMain` top half (`_080AF908..._080AFAB0`), ported verbatim in
 * structure with the channel-update second loop deferred to PR #7
 * part 2.2.2.2.3.
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

    /* PR #7 part 2.2.2.2.3 will land the per-track second loop here
     * (TrkVolPitSet + chan-update). For now the second loop is a
     * no-op — under the silent host mixer track->chan is always
     * NULL, so the chan-update half would no-op anyway, and the
     * MPT_FLG_VOLCHG / MPT_FLG_PITCHG bits stay set on the track
     * until the next tick (which is faithful to a paused mixer). */

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
/* the `track->chan` walk in `ply_endtie`. The remaining handlers      */
/* (`ply_voice`, `ply_note`, `ply_port`) need `MPlayMain`'s            */
/* surrounding state (ROM-address loads against `gSongTable`, CGB      */
/* register pokes) and remain stubs until PR #7 parts 2.2.2.2 / 2.3    */
/* land.                                                                */
/*                                                                     */
/* `MPlayMain` is itself a no-op stub and `NUM_MUSIC_PLAYERS == 0`     */
/* under `__PORT__` (see src/gba/m4a.c), so none of these handlers is  */
/* reached at runtime today. They are instead exercised in isolation   */
/* by `Port_M4ASelfCheck()` below, which feeds each handler a          */
/* synthesized cmdPtr byte stream and verifies the resulting track     */
/* state against the asm semantics in `asm/lib/m4a_asm.s`.             */

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

/* The handlers still gated on PR #7 part 2.2.2.2 (need `MPlayMain`'s
 * surrounding state — channel-list walking, ROM-address loads against
 * `gSongTable`, CGB register pokes). Stay no-op stubs for now. */
#define M4A_PLY_STUB(name)                                 \
    void name(MusicPlayerInfo* mp, MusicPlayerTrack* tr) { \
        (void)mp;                                          \
        (void)tr;                                          \
    }

M4A_PLY_STUB(ply_voice)
M4A_PLY_STUB(ply_port)
M4A_PLY_STUB(ply_note)

#undef M4A_PLY_STUB

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
        M4A_CHECK((track_local.flags & MPT_FLG_VOLCHG) == MPT_FLG_VOLCHG);
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
        M4A_CHECK((track_local.flags & MPT_FLG_PITCHG) == MPT_FLG_PITCHG);
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

#undef M4A_WRITE_LE32
#undef M4A_RUN

    return 0;
}

#undef M4A_CHECK
