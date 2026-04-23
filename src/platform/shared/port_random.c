/**
 * @file port_random.c
 * @brief Host-C port of the `Random` PRNG from
 *        `asm/src/code_08000E44.s` (lines 18-28).
 *
 * `Random` is a tiny multiply-then-rotate PRNG that the game seeds
 * once in `AgbMain` (`gRand = 0x1234567`) and then steps every time it
 * needs a random number. The THUMB body is:
 *
 *     ldr  r2, =gRand
 *     ldr  r0, [r2]       @ x = gRand
 *     lsls r1, r0, #1
 *     adds r0, r0, r1     @ x = x + (x << 1)   = x * 3
 *     movs r1, #0xd
 *     rors r0, r1         @ x = ROR32(x, 13)
 *     str  r0, [r2]       @ gRand = x
 *     lsrs r0, r0, #1     @ return x >> 1
 *     bx   lr
 *
 * That arithmetic is unsigned 32-bit throughout (`gRand` is `u32`), so
 * the C port below is a literal transcription -- no behavioural drift,
 * same sequence of `gRand` values as the GBA build for the same seed.
 *
 * Why port this now: the SDL host build's earliest user of `Random`
 * is `src/fileselect.c::sub_0805066C` (the file-select cursor blink
 * driver), which fires every frame the file-select task is alive. The
 * matching `PORT_ASM_STUB(Random)` in `asm_stubs.c` `abort()`s on the
 * first invocation, so the boot path SIGABRTs as soon as the file-
 * select task starts ticking. Replacing the stub with this strong
 * definition unblocks that path; the `Random` entry in the
 * `asm_stubs.c` include list is removed in the same change so the
 * stub is no longer compiled and no symbol-clash warning fires.
 */

#ifdef __PORT__

#include "global.h"

extern u32 gRand;

u32 Random(void);

u32 Random(void) {
    /* x = gRand * 3 (32-bit wrap). */
    u32 x = gRand;
    x = x + (x << 1);
    /* ROR32(x, 13). */
    x = (x >> 13) | (x << (32 - 13));
    gRand = x;
    return x >> 1;
}

#endif /* __PORT__ */
