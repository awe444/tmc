#ifndef GUARD_GBA_DEFINES
#define GUARD_GBA_DEFINES

#include <stddef.h>

#ifdef __PORT__
/* Pull in the host emulated memory map. The platform layer owns the backing
 * arrays (gPortEwram, gPortIwram, gPortVram, gPortOam, gPortPltt, gPortIo);
 * by making EWRAM_START, IWRAM_START, PLTT, VRAM, OAM and (in io_reg.h)
 * REG_BASE point at those arrays, every derived macro in this header and in
 * io_reg.h automatically resolves to a host-array address with no other
 * source changes. See docs/sdl_port.md (PR #2). */
#include "platform/port.h"
#include <stdint.h>
#endif

#define TRUE 1
#define FALSE 0

#ifdef __PORT__
/* These attributes only have meaning on the GBA toolchain, where they place
 * data into the IWRAM/EWRAM linker sections. On the host we let the linker
 * put EWRAM_DATA/IWRAM_DATA in regular .data/.bss. */
#define IWRAM_DATA
#define EWRAM_DATA
#elif defined(__APPLE__)
#define IWRAM_DATA __attribute__((section("__DATA,iwram_data")))
#define EWRAM_DATA __attribute__((section("__DATA,ewram_data")))
#else
#define IWRAM_DATA __attribute__((section("iwram_data")))
#define EWRAM_DATA __attribute__((section("ewram_data")))
#endif

#ifdef __PORT__
/* `naked` is meaningless on hosted compilers (and refused outright by clang
 * on x86_64); the SDL port either decompiles or stubs naked routines. */
#define NAKED
#else
#define NAKED __attribute__((naked))
#endif
#define UNUSED __attribute__((unused))
#ifdef __CLION_IDE__
#define PACKED
#define ALIGNED(n)
#else
#define PACKED __attribute__((packed))
#define ALIGNED(n) __attribute__((aligned(n)))
#endif

#ifdef __PORT__
/* On the host, native pointers may be 8 bytes and require stronger alignment
 * than the original GBA BIOS scratch offsets provide. Model the tail of IWRAM
 * with a packed overlay so these slots remain lvalue macros while all host
 * pointer-sized fields stay within the 0x8000-byte gPortIwram region. The
 * overlay must be exactly PORT_IWRAM_SIZE bytes; that invariant is pinned
 * down in src/platform/shared/port_headers_check.c. */
struct PortBiosScratch {
    uint8_t filler0[0x7FE8];
    struct SoundInfo* soundInfoPtr;
    uint16_t intrCheck;
    uint8_t filler1[6];
    void* intrVector;
} PACKED;

#define SOUND_INFO_PTR (((struct PortBiosScratch*)(void*)gPortIwram)->soundInfoPtr)
#define INTR_CHECK (((struct PortBiosScratch*)(void*)gPortIwram)->intrCheck)
#define INTR_VECTOR (((struct PortBiosScratch*)(void*)gPortIwram)->intrVector)
#else
#define SOUND_INFO_PTR (*(struct SoundInfo**)0x3007FF0)
#define INTR_CHECK (*(u16*)0x3007FF8)
#define INTR_VECTOR (*(void**)0x3007FFC)
#endif

#ifdef __PORT__
#define EWRAM_START ((uintptr_t)gPortEwram)
#define IWRAM_START ((uintptr_t)gPortIwram)
#else
#define EWRAM_START 0x02000000
#define IWRAM_START 0x03000000
#endif
#define EWRAM_END (EWRAM_START + 0x40000)
#define IWRAM_END (IWRAM_START + 0x8000)

#ifdef __PORT__
#define PLTT ((uintptr_t)gPortPltt)
#else
#define PLTT 0x5000000
#endif
#define PLTT_SIZE 0x400
#define PAL_RAM ((u8*)(PLTT))

#define BG_PLTT PLTT
#define BG_PLTT_SIZE 0x200

#define OBJ_PLTT (PLTT + 0x200)
#define OBJ_PLTT_SIZE 0x200

#ifdef __PORT__
#define VRAM ((uintptr_t)gPortVram)
#else
#define VRAM 0x6000000
#endif
#define VRAM_SIZE 0x18000

#define BG_VRAM VRAM
#define BG_VRAM_SIZE 0x10000
#define BG_CHAR_SIZE 0x4000
#define BG_SCREEN_SIZE 0x800
#define BG_CHAR_ADDR(n) (void*)(BG_VRAM + (BG_CHAR_SIZE * (n)))
#define BG_SCREEN_ADDR(n) (void*)(BG_VRAM + (BG_SCREEN_SIZE * (n)))
#define BG_TILE_ADDR(n) (void*)(BG_VRAM + (0x80 * (n)))

#define BG_TILE_H_FLIP(n) (0x400 + (n))
#define BG_TILE_V_FLIP(n) (0x800 + (n))

// text-mode BG
#define OBJ_VRAM0 (void*)(VRAM + 0x10000)
#define OBJ_VRAM0_SIZE 0x8000

// bitmap-mode BG
#define OBJ_VRAM1 (void*)(VRAM + 0x14000)
#define OBJ_VRAM1_SIZE 0x4000

#ifdef __PORT__
#define OAM ((uintptr_t)gPortOam)
#else
#define OAM 0x7000000
#endif
#define OAM_SIZE 0x400

#ifdef __PORT__
/* A handful of `src/...` translation units write to GBA hardware
 * regions through a literal integer constant (e.g. `(u8*)0x07000000`,
 * `(void*)0x600C000`) rather than the named macros above. On real
 * hardware those literals are valid; on the host they would dereference
 * an unmapped low-memory page and SIGSEGV. `PORT_HW_ADDR(x)` translates
 * the literal to the corresponding offset inside the matching host
 * array (`gPortVram`, `gPortOam`, `gPortPltt`, `gPortEwram`,
 * `gPortIwram`, `gPortIo`); addresses outside the recognised regions
 * are returned unchanged so the call site keeps its original
 * (likely-faulty) semantics rather than silently misdirecting to a
 * different region. Intended use site is `#ifdef __PORT__` patches in
 * the GBA decomp; see docs/sdl_port.md (PR #2b.4b). */
static inline void* Port_HwAddr(uintptr_t a) {
    if (a >= 0x06000000u && a < 0x06000000u + PORT_VRAM_SIZE)
        return (void*)((uintptr_t)gPortVram + (a - 0x06000000u));
    if (a >= 0x07000000u && a < 0x07000000u + PORT_OAM_SIZE)
        return (void*)((uintptr_t)gPortOam + (a - 0x07000000u));
    if (a >= 0x05000000u && a < 0x05000000u + PORT_PLTT_SIZE)
        return (void*)((uintptr_t)gPortPltt + (a - 0x05000000u));
    if (a >= 0x02000000u && a < 0x02000000u + PORT_EWRAM_SIZE)
        return (void*)((uintptr_t)gPortEwram + (a - 0x02000000u));
    if (a >= 0x03000000u && a < 0x03000000u + PORT_IWRAM_SIZE)
        return (void*)((uintptr_t)gPortIwram + (a - 0x03000000u));
    if (a >= 0x04000000u && a < 0x04000000u + PORT_IO_SIZE)
        return (void*)((uintptr_t)gPortIo + (a - 0x04000000u));
    return (void*)a;
}
#define PORT_HW_ADDR(a) Port_HwAddr((uintptr_t)(a))
#endif

#define ROM_HEADER_SIZE 0xC0

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 160

#define TILE_SIZE_4BPP 32
#define TILE_SIZE_8BPP 64

#define TILE_OFFSET_4BPP(n) ((n)*TILE_SIZE_4BPP)
#define TILE_OFFSET_8BPP(n) ((n)*TILE_SIZE_8BPP)

#define TOTAL_OBJ_TILE_COUNT 1024

#define RGB(r, g, b) ((r) | ((g) << 5) | ((b) << 10))
#define RGB2(r, g, b) (((b) << 10) | ((g) << 5) | (r))
#define _RGB(r, g, b) ((((b)&0x1F) << 10) + (((g)&0x1F) << 5) + ((r)&0x1F))

#define RGB_BLACK RGB(0, 0, 0)
#define RGB_WHITE RGB(31, 31, 31)
#define RGB_RED RGB(31, 0, 0)
#define RGB_GREEN RGB(0, 31, 0)
#define RGB_BLUE RGB(0, 0, 31)
#define RGB_YELLOW RGB(31, 31, 0)
#define RGB_MAGENTA RGB(31, 0, 31)
#define RGB_CYAN RGB(0, 31, 31)
#define RGB_WHITEALPHA (RGB_WHITE | 0x8000)

#define SYSTEM_CLOCK (16 * 1024 * 1024) // System Clock

#endif // GUARD_GBA_DEFINES
