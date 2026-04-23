/**
 * @file bios.c
 * @brief Host-side stand-ins for the GBA BIOS syscalls declared in
 *        include/gba/syscall.h.
 *
 * Only a few of these (LZ77UnComp*, CpuSet/CpuFastSet, RegisterRamReset)
 * have non-trivial behaviour the game can observe; the rest are
 * placeholders that will be filled in as PR #2+ wires the real game
 * sources into the SDL build. They are exported with `Port_` prefixes so
 * they don't collide with the unprefixed declarations until PR #2
 * conditionally redirects the macros.
 */
#include "platform/port.h"

#include <stdint.h>
#include <string.h>

/* ---------- LZ77 (GBA BIOS-format) decompression ------------------------ */
/*
 * Header byte layout (little-endian):
 *   byte 0       : compression type (0x10 = LZ77)
 *   bytes 1..3   : decompressed size (24-bit LE)
 *
 * Block format: a 1-byte flag where each bit (MSB first) decides whether
 * the next chunk is a raw byte (bit clear) or a 2-byte back-reference
 * (bit set). Back-reference layout: high nibble of byte 0 = (length - 3),
 * remaining 12 bits = (offset - 1).
 */
static void lz77_decompress(const uint8_t* src, uint8_t* dst) {
    uint32_t header = (uint32_t)src[0] | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
    uint32_t out_size = header >> 8;
    src += 4;

    uint8_t* out_start = dst;
    uint8_t* out_end = dst + out_size;

    while (dst < out_end) {
        uint8_t flags = *src++;
        for (int i = 0; i < 8 && dst < out_end; ++i) {
            if (flags & 0x80) {
                uint8_t b0 = *src++;
                uint8_t b1 = *src++;
                uint32_t length = (b0 >> 4) + 3u;
                uint32_t offset = (((uint32_t)(b0 & 0x0F) << 8) | b1) + 1u;
                /* The reference must point inside what we have already
                 * written; clamp defensively. */
                if (offset > (uint32_t)(dst - out_start)) {
                    return;
                }
                const uint8_t* back = dst - offset;
                while (length-- > 0 && dst < out_end) {
                    *dst++ = *back++;
                }
            } else {
                if (dst < out_end) {
                    *dst++ = *src++;
                }
            }
            flags <<= 1;
        }
    }
}

void Port_LZ77UnCompVram(const void* src, void* dst) {
    /* The decomp passes literal GBA hardware addresses (e.g.
     * `0x06000000`) for `dst` whenever it asks the BIOS to drop a
     * decompressed tile blob straight into VRAM. Translate both
     * pointers through the host hardware-address translator so the
     * write lands inside `gPortVram`; real host pointers (from
     * `gPortVram + N` etc.) pass through unchanged. The same applies
     * to `src` because some paths point it at an EWRAM literal. */
    lz77_decompress((const uint8_t*)Port_TranslateHwAddr((uintptr_t)src),
                    (uint8_t*)Port_TranslateHwAddr((uintptr_t)dst));
}

void Port_LZ77UnCompWram(const void* src, void* dst) {
    /* See Port_LZ77UnCompVram. */
    lz77_decompress((const uint8_t*)Port_TranslateHwAddr((uintptr_t)src),
                    (uint8_t*)Port_TranslateHwAddr((uintptr_t)dst));
}

/* ---------- CpuSet / CpuFastSet ---------------------------------------- */

#define CPUSET_FILL_FLAG (1u << 24)
#define CPUSET_32BIT_FLAG (1u << 26)

void Port_CpuSet(const void* src, void* dst, uint32_t control) {
    uint32_t count = control & 0x1FFFFFu;
    int fill = (control & CPUSET_FILL_FLAG) != 0;
    int use32 = (control & CPUSET_32BIT_FLAG) != 0;
    /* Translate both pointers through the host hardware-address
     * translator so call sites that pass a literal GBA address (e.g.
     * `CpuSet(src, (void*)0x06000000, ...)`) land inside the matching
     * `gPort*` array. Real host pointers pass through unchanged. */
    src = (const void*)Port_TranslateHwAddr((uintptr_t)src);
    dst = (void*)Port_TranslateHwAddr((uintptr_t)dst);

    if (use32) {
        uint32_t* d = (uint32_t*)dst;
        if (fill) {
            uint32_t v = *(const uint32_t*)src;
            for (uint32_t i = 0; i < count; ++i)
                d[i] = v;
        } else {
            const uint32_t* s = (const uint32_t*)src;
            for (uint32_t i = 0; i < count; ++i)
                d[i] = s[i];
        }
    } else {
        uint16_t* d = (uint16_t*)dst;
        if (fill) {
            uint16_t v = *(const uint16_t*)src;
            for (uint32_t i = 0; i < count; ++i)
                d[i] = v;
        } else {
            const uint16_t* s = (const uint16_t*)src;
            for (uint32_t i = 0; i < count; ++i)
                d[i] = s[i];
        }
    }
}

void Port_CpuFastSet(const void* src, void* dst, uint32_t control) {
    /* CpuFastSet always operates on 8-word (32-byte) units of u32. */
    uint32_t count = (control & 0x1FFFFFu) * 8u;
    int fill = (control & CPUSET_FILL_FLAG) != 0;
    /* See Port_CpuSet for the address-translation rationale. */
    src = (const void*)Port_TranslateHwAddr((uintptr_t)src);
    dst = (void*)Port_TranslateHwAddr((uintptr_t)dst);
    uint32_t* d = (uint32_t*)dst;
    if (fill) {
        uint32_t v = *(const uint32_t*)src;
        for (uint32_t i = 0; i < count; ++i)
            d[i] = v;
    } else {
        const uint32_t* s = (const uint32_t*)src;
        for (uint32_t i = 0; i < count; ++i)
            d[i] = s[i];
    }
}

/* ---------- RegisterRamReset / SoftReset ------------------------------- */

void Port_RegisterRamReset(uint32_t flags) {
    /* Mirrors the GBA BIOS function: clear selected memory regions and
     * reset some I/O registers. We honour the EWRAM/IWRAM/VRAM/PALETTE
     * /OAM flags and ignore the rest (no I/O or SIO emulation here). */
    if (flags & 0x01)
        memset(gPortEwram, 0, sizeof(gPortEwram));
    if (flags & 0x02)
        memset(gPortIwram, 0, sizeof(gPortIwram));
    if (flags & 0x04)
        memset(gPortPltt, 0, sizeof(gPortPltt));
    if (flags & 0x08)
        memset(gPortVram, 0, sizeof(gPortVram));
    if (flags & 0x10)
        memset(gPortOam, 0, sizeof(gPortOam));
}

void Port_SoftReset(uint32_t flags) {
    Port_RegisterRamReset(flags);
    /* On real hardware this jumps back to the ROM entry; on the host we
     * just request a quit and let main.c restart the loop in PR #2. */
    Port_RequestQuit();
}

/* ---------- Math helpers ------------------------------------------------ */

uint32_t Port_BiosSqrt(uint32_t value) {
    /* Integer sqrt — same observable result as the BIOS routine. */
    uint32_t lo = 0, hi = (value < (1u << 16)) ? (1u << 16) : value;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo + 1) / 2;
        if (mid <= value / (mid == 0 ? 1 : mid)) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    return lo;
}

/* ---------- Unprefixed aliases for the GBA BIOS entry points ----------- */
/*
 * Sub-step 2b.4 of the SDL-port roadmap: the real `src/main.c::AgbMain`
 * (and a handful of other src/ TUs) calls these BIOS functions by their
 * unprefixed names declared in `include/gba/syscall.h`. On the GBA they
 * resolve to `asm/lib/libagbsyscall.s`; on the host we forward them to
 * the matching `Port_*` implementation above. Compiling them only under
 * `__PORT__` keeps the matching ROM build untouched.
 */
#ifdef __PORT__
void RegisterRamReset(uint32_t flags) {
    Port_RegisterRamReset(flags);
}
void SoftReset(uint32_t flags) {
    Port_SoftReset(flags);
}

/* `Stop()` (include/gba/syscall.h) sandwiches its `SystemCall(3)` between
 * SoundBiasReset()/SoundBiasSet(). On hardware those silence the audio
 * DAC before powering down; on the host there is nothing to silence and
 * `Stop()` itself is reduced to a no-op (SystemCall is `((void)0)` under
 * __PORT__), so these are pure stubs to keep the linker happy. */
void SoundBiasReset(void) {
}
void SoundBiasSet(void) {
}

/* Remaining BIOS-syscall aliases referenced by src/ when
 * TMC_LINK_GAME_SOURCES=ON. See docs/sdl_port.md (PR #2b.4b). */
void LZ77UnCompVram(const void* src, void* dest) {
    Port_LZ77UnCompVram(src, dest);
}
void LZ77UnCompWram(const void* src, void* dest) {
    Port_LZ77UnCompWram(src, dest);
}
void CpuSet(const void* src, void* dest, uint32_t control) {
    Port_CpuSet(src, dest, control);
}
void CpuFastSet(const void* src, void* dest, uint32_t control) {
    Port_CpuFastSet(src, dest, control);
}

/* Simple host implementations of the remaining arithmetic syscalls. The
 * GBA BIOS defines Div to return num/denom in r0 and num%denom in r1;
 * `Mod` is an unprefixed alias callers use when they only want the
 * remainder. On host builds, provide an explicit, standards-conforming
 * implementation of the combined quotient/remainder behaviour instead of
 * relying on ARM ABI register state. */
static void port_divmod(int32_t num, int32_t denom, int32_t* quotient, int32_t* remainder) {
    if (denom == 0) {
        *quotient = 0;
        *remainder = 0;
        return;
    }

    *quotient = num / denom;
    *remainder = num % denom;
}

int32_t Div(int32_t num, int32_t denom) {
    int32_t quotient;
    int32_t remainder;

    port_divmod(num, denom, &quotient, &remainder);
    return quotient;
}

int64_t DivAndMod(int32_t num, int32_t denom) {
    int32_t quotient;
    int32_t remainder;

    port_divmod(num, denom, &quotient, &remainder);
    return (int64_t)(((uint64_t)(uint32_t)remainder << 32) | (uint32_t)quotient);
}

int32_t Mod(int32_t num, int32_t denom) {
    int32_t quotient;
    int32_t remainder;

    port_divmod(num, denom, &quotient, &remainder);
    return remainder;
}
uint16_t Sqrt(uint32_t value) {
    return (uint16_t)Port_BiosSqrt(value);
}

/* Affine math helpers. These compute the BG/OBJ affine transformation
 * matrices the GBA BIOS would otherwise produce. The host software
 * rasterizer (PR #4 / #5) will consume these results once it comes
 * online; for now they are called during initialisation paths that
 * need a valid identity-ish matrix. */
struct BgAffineSrcData {
    int32_t texX, texY;
    int16_t scrX, scrY;
    int16_t sx, sy;
    uint16_t alpha;
};
struct BgAffineDstData {
    int16_t pa, pb, pc, pd;
    int32_t startX, startY;
};
struct ObjAffineSrcData {
    int16_t xScale, yScale;
    uint16_t rotation;
};
void BgAffineSet(struct BgAffineSrcData* src, struct BgAffineDstData* dst, int32_t count) {
    for (int32_t i = 0; i < count; ++i) {
        /* Identity transform placeholder -- good enough to let game code
         * validate that the call returned without corrupting state. The
         * real affine math will arrive with the PR #5 renderer. */
        dst[i].pa = 0x0100;
        dst[i].pb = 0;
        dst[i].pc = 0;
        dst[i].pd = 0x0100;
        dst[i].startX = src[i].texX;
        dst[i].startY = src[i].texY;
    }
}
void ObjAffineSet(struct ObjAffineSrcData* src, void* dst_, int32_t count, int32_t offset) {
    int16_t* dst = (int16_t*)dst_;
    for (int32_t i = 0; i < count; ++i) {
        dst[0] = 0x0100;
        dst[1] = 0;
        dst[2] = 0;
        dst[3] = 0x0100;
        (void)src;
        dst = (int16_t*)((char*)dst + offset * 4);
    }
    (void)count;
}

/* RL (run-length) decompression is used by a small number of asset
 * blobs. The real decoder has not been ported yet; until it is,
 * treat these as no-ops. Callers that dereference the destination
 * will observe whatever the host-side memory arena was initialised
 * to (zero on startup), which is the same state the game sees on
 * freshly-erased EWRAM. */
void RLUnCompWram(const void* src, void* dst) {
    (void)src;
    (void)dst;
}
void RLUnCompVram(const void* src, void* dst) {
    (void)src;
    (void)dst;
}

/* Temporary stub for the BIOS arc-tangent. Returns 0 until a real
 * atan2 implementation is wired in (the existing call sites feed the
 * result into rotation math that the SDL port's PR #5 renderer will
 * consume). */
uint16_t ArcTan2(int16_t x, int16_t y) {
    (void)x;
    (void)y;
    return 0;
}
#endif
