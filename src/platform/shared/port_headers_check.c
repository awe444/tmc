/**
 * @file port_headers_check.c
 * @brief Compile-time smoke test for the GBA decomp headers under `__PORT__`.
 *
 * PR #2a of the SDL-port roadmap (see docs/sdl_port.md) repoints the GBA
 * memory-map macros (`REG_BASE`, `EWRAM_START`, `IWRAM_START`, `PLTT`,
 * `VRAM`, `OAM`, `SOUND_INFO_PTR`, `INTR_*`) at the host arrays owned by
 * `src/platform/shared/gba_memory.c`, and turns the agbcc-only constructs
 * (`EWRAM_DATA`, `IWRAM_DATA`, `NAKED`, `FORCE_REGISTER`, `ASM_FUNC`,
 * `NONMATCH`, `MEMORY_BARRIER`, `BLOCK_CROSS_JUMP`, `asm_comment`,
 * `asm_unified`, `SystemCall`) into no-ops on the host.
 *
 * This translation unit exists purely to exercise those rewired macros
 * during the SDL build so that any future edit to the GBA headers that
 * breaks host compilation is caught by CI before PR #2b starts pulling in
 * the real game sources under `src/`. It is **not** linked into AgbMain and
 * deliberately performs no runtime work — every function below has either
 * external linkage with no callers (the linker drops it) or is reachable
 * only through the explicit `Port_HeadersSelfCheck` entry point used by
 * the smoke test in `agb_main_stub.c`.
 *
 * If you find yourself fighting this file, the right fix is almost always
 * in the GBA headers under `include/gba/` or in `include/global.h`, gated
 * behind `#ifdef __PORT__`.
 */

#include "global.h"
#include "gba/gba.h"
#include "platform/port.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------------ */
/* 1. Memory-map macros must point inside the host-owned arrays.            */
/* ------------------------------------------------------------------------ */

/* Static asserts validate the rewiring at compile time without running
 * anything; if they ever fire the build fails with a clear message. */
_Static_assert(sizeof(gPortEwram) == PORT_EWRAM_SIZE, "EWRAM size mismatch");
_Static_assert(sizeof(gPortIwram) == PORT_IWRAM_SIZE, "IWRAM size mismatch");
_Static_assert(sizeof(gPortVram) == PORT_VRAM_SIZE, "VRAM size mismatch");
_Static_assert(sizeof(gPortOam) == PORT_OAM_SIZE, "OAM size mismatch");
_Static_assert(sizeof(gPortPltt) == PORT_PLTT_SIZE, "PLTT size mismatch");
_Static_assert(sizeof(gPortIo) == PORT_IO_SIZE, "IO size mismatch");

/* ------------------------------------------------------------------------ */
/* 2. agbcc-isms must compile (as no-ops) on the host.                      */
/* ------------------------------------------------------------------------ */

EWRAM_DATA static u32 sPortHeaderEwramSlot;
IWRAM_DATA static u32 sPortHeaderIwramSlot;

/* `NAKED` becomes a no-op under __PORT__; the body still has to be valid C. */
NAKED static void PortHeaderNaked(void) {
    sPortHeaderIwramSlot = sPortHeaderEwramSlot;
}

/* FORCE_REGISTER / MEMORY_BARRIER / BLOCK_CROSS_JUMP / asm_comment /
 * asm_unified must all expand cleanly with no inline assembly. */
static u32 PortHeaderAgbccIsms(u32 in) {
    FORCE_REGISTER(u32 r0, r0) = in;
    MEMORY_BARRIER
    BLOCK_CROSS_JUMP
    asm_comment("port-headers smoke test");
    asm_unified("");
    return r0;
}

/* ------------------------------------------------------------------------ */
/* 3. The repointed register macros must read/write into gPortIo[0..0x400). */
/* ------------------------------------------------------------------------ */

/**
 * Touch a representative cross-section of the rewired macros and return a
 * non-zero status if anything looks wrong. Called from `agb_main_stub.c`
 * during the headless smoke test; with NDEBUG defined the asserts compile
 * out and the function reduces to a handful of memory loads/stores.
 */
int Port_HeadersSelfCheck(void) {
    /* I/O registers must alias the start / interior of gPortIo. */
    assert((uintptr_t)&REG_DISPCNT == (uintptr_t)gPortIo + 0x000);
    assert((uintptr_t)&REG_KEYINPUT == (uintptr_t)gPortIo + 0x130);
    assert((uintptr_t)&REG_IME == (uintptr_t)gPortIo + 0x208);

    /* Palette / VRAM / OAM bases must alias their respective host arrays. */
    assert((uintptr_t)BG_PLTT == (uintptr_t)gPortPltt);
    assert((uintptr_t)OBJ_PLTT == (uintptr_t)gPortPltt + 0x200);
    assert((uintptr_t)BG_VRAM == (uintptr_t)gPortVram);
    assert((uintptr_t)OAM == (uintptr_t)gPortOam);

    /* IWRAM scratch slots must land inside gPortIwram. */
    assert((uintptr_t)&INTR_CHECK == (uintptr_t)gPortIwram + 0x7FF8);
    assert((uintptr_t)&INTR_VECTOR == (uintptr_t)gPortIwram + 0x7FFC);

    /* Round-trip a value through REG_DISPCNT to prove the macro really is
     * a writable lvalue backed by gPortIo, then put it back. */
    u16 saved = REG_DISPCNT;
    REG_DISPCNT = 0xBEEF;
    int ok = (REG_DISPCNT == 0xBEEF) && (gPortIo[0] == 0xEF) && (gPortIo[1] == 0xBE);
    REG_DISPCNT = saved;

    /* Exercise the agbcc-ism no-ops too so the linker can't drop them. */
    PortHeaderNaked();
    (void)PortHeaderAgbccIsms(0x1234);

    return ok ? 0 : 1;
}
