#ifndef GLOBAL_H
#define GLOBAL_H

/**
 * @defgroup Tasks Tasks
 * @defgroup Subtasks Subtasks
 * @brief Subtasks override the game task for short periods.
 * @defgroup WorldEvents World Events
 * @brief Cutscenes that happen after a kinstone fusion.
 */

/**
 * @defgroup Entities Entities
 */
///@{
/**
 * @defgroup Player Player
 * @defgroup Enemies Enemies
 * @defgroup Projectiles Projectiles
 * @defgroup Objects Objects
 * @defgroup NPCs NPCs
 * @defgroup Items Items
 * @defgroup Managers Managers
 * @brief Entities with a smaller footprint of 0x40 bytes.
 */
///@}

#include "gba/gba.h"
#include <string.h>

// Prevent cross-jump optimization.
#ifdef __PORT__
#define BLOCK_CROSS_JUMP
#else
#define BLOCK_CROSS_JUMP asm("");
#endif

// to help in decompiling
#ifdef __PORT__
/* Inline-asm comments and `.syntax` directives are GBA/agbcc-specific and
 * have no equivalent on the host toolchain — make them no-ops under the
 * SDL port (and any future host port that defines __PORT__). */
#define asm_comment(x)
#define asm_unified(x)
#else
#define asm_comment(x) asm volatile("@ -- " x " -- ")
#define asm_unified(x) asm(".syntax unified\n" x "\n.syntax divided")
#endif

#if defined(__APPLE__) || defined(__CYGWIN__)
// Get the IDE to stfu

// We define it this way to fool preproc.
#define INCBIN(...) \
    { 0 }
#define INCBIN_U8 INCBIN
#define INCBIN_U16 INCBIN
#define INCBIN_U32 INCBIN
#define INCBIN_S8 INCBIN
#define INCBIN_S16 INCBIN
#define INCBIN_S32 INCBIN
#define _(x) (x)
#define __(x) (x)
#endif // __APPLE__

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#define SWAP(a, b, temp) \
    {                    \
        (temp) = a;      \
        (a) = b;         \
        (b) = temp;      \
    }

// useful math macros

// Converts a number to Q8.8 fixed-point format
#define Q_8_8(n) ((s16)((n)*256))

// Converts a number to Q16.16 fixed-point format
#define Q_16_16(n) ((s32)((n) * (1 << 16)))

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) >= (b) ? (a) : (b))

#ifdef __PORT__
// Host pointers are 8 bytes (vs 4 on the GBA), so most TMC struct-layout
// assertions are guaranteed to trip in a host build. They encode hardware
// layout that doesn't apply to the SDL port, so collapse them to a no-op
// here -- the matching ROM build still gets the real check via GBA.mk.
// See docs/sdl_port.md, PR #2b.3. The expansion intentionally leaves the
// trailing `;` at the call site as a stray null declaration (permitted at
// file scope in C), which side-steps any tag-name collisions.
#define static_assert(cond)
#else
#define static_assert(cond) extern char assertion[(cond) ? 1 : -1]
#endif

#define super (&this->base)

#if NON_MATCHING || defined(__PORT__)
#define ASM_FUNC(path, decl)
#else
#define ASM_FUNC(path, decl)    \
    NAKED decl {                \
        asm(".include " #path); \
    }
#endif

#if NON_MATCHING || defined(__PORT__)
#define NONMATCH(path, decl) decl
#define END_NONMATCH
#else
#define NONMATCH(path, decl)    \
    NAKED decl {                \
        asm(".include " #path); \
        if (0)
#define END_NONMATCH }
#endif

#if NON_MATCHING || defined(__PORT__)
/* `register T x asm("rN")` is an arm-gcc agbcc-ism that won't compile on
 * the host. The SDL port treats FORCE_REGISTER as a plain declaration. */
#define FORCE_REGISTER(var, reg) var
#else
#define FORCE_REGISTER(var, reg) register var asm(#reg)
#endif

#if NON_MATCHING || defined(__PORT__)
#define MEMORY_BARRIER
#else
#define MEMORY_BARRIER asm("" ::: "memory")
#endif

/* Some decomp'd data tables store a function/data address in a `u32`
 * slot because on the GBA `sizeof(void*) == sizeof(u32) == 4`. On a
 * 64-bit host that cast truncates the pointer, so it is also not a
 * valid constant initializer. `PORT_ROM_PTR(x)` casts directly to
 * `uintptr_t` on the host to preserve the full address width without
 * going through `void*`, which is only valid for object pointers. The
 * matching ROM build keeps the original `(u32)` cast so agbcc emits
 * the same instruction stream. See docs/sdl_port.md (PR #2b.3). */
#ifdef __PORT__
#include <stdint.h>
#define PORT_ROM_PTR(x) ((uintptr_t)(x))
#else
#define PORT_ROM_PTR(x) ((u32)(x))
#endif

typedef union {
    s32 WORD;
    struct {
        s16 x, y;
    } HALF;
} Coords;

typedef struct {
    s8 x;
    s8 y;
} PACKED Coords8;

union SplitDWord {
    s64 DWORD;
    u64 DWORD_U;
    struct {
        s32 LO, HI;
    } HALF;
    struct {
        u32 LO, HI;
    } HALF_U;
};

union SplitWord {
    s32 WORD;
    u32 WORD_U;
    struct {
        s16 LO, HI;
    } HALF;
    struct {
        u16 LO, HI;
    } HALF_U;
    struct {
        u8 byte0, byte1, byte2, byte3;
    } BYTES;
};

union SplitHWord {
    u16 HWORD;
    struct {
        u8 LO, HI;
    } PACKED HALF;
} PACKED;

#define FORCE_WORD_ALIGNED __attribute__((packed, aligned(2)))

/* forward decls */
struct Entity_;

/**
 * bitset macros
 */

#define BIT(bit) (1 << (bit))
#define IS_BIT_SET(value, bit) ((value)&BIT(bit))

/**
 * Multi return function data type casts
 */
typedef u64 (*MultiReturnTypeSingleEntityArg)(struct Entity_*);
typedef s64 (*MultiReturnTypeTwoS32Arg)(s32, s32);

#endif // GLOBAL_H
