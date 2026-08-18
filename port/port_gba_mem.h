#pragma once
#include "viewport.h"
#include "port_types.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include <intrin.h>
#endif

extern u8 gIoMem[0x400];       // I/O Memory (0x04000000-0x040003FF)
extern u8 gEwram[0x40000];     // EWRAM (0x02000000-0x0203FFFF)
extern u8 gIwram[0x8000];      // IWRAM (0x03000000-0x03007FFF)
extern u16 gBgPltt[256];       // 0x200 bytes
extern u16 gObjPltt[256];      // 0x200 bytes
extern u16 gOamMem[0x400 / 2]; // 0x400 bytes (OAM)

/* The HUD/UI tilemap. Standalone on PC rather than an alias into gEwram
 * (Spike 6), but on the GBA it lived at EWRAM 0x02034CB0 and some data
 * still names it by that raw address — see GBA_BG0_BUFFER_ADDR below. */
extern u16 gBG0Buffer[UI_BG0_ENTRIES];

/* Where gBG0Buffer sat in the GBA's EWRAM.
 *
 * Spike 6 moved the buffer out of gEwram and fixed the one C-source Font
 * that had hardcoded this address. It could not fix the ones stored as
 * *data*: a 24-byte GBA Font blob in ROM carries its `dest` as a raw
 * pointer, so Port_DecodeFontGBA resolves e.g. 0x02034E0E — row 5 of this
 * buffer, where the area-name banner is drawn — and without the mapping
 * below it lands in dead gEwram and the text is never seen.
 *
 * The mapping is linear, which is only right while the buffer keeps the
 * GBA's 32x32 shape. viewport.h explains why it does (widening was tried
 * and abandoned; the whole layer is centred instead) and warns that the
 * failures from changing the stride are silent. Assert it here so a second
 * attempt at widening stops at a compile error on this line. */
#define GBA_BG0_BUFFER_ADDR 0x02034CB0u
PORT_STATIC_ASSERT(UI_BG0_WIDTH_TILES == 32 && UI_BG0_ENTRIES == 0x400,
                   "gBG0Buffer no longer has the GBA's 32x32 shape: the linear GBA_BG0_BUFFER_ADDR "
                   "mapping in gba_TryMemPtr would silently write to the wrong rows");

/* VRAM, plus port-only shadow banks the GBA does not have.
 *
 * The GBA's 96 KB is PORT_VRAM_GBA_SIZE and is all the engine can address:
 * every gba_read/write guard in port_gba_mem.c still stops at 0x06017FFF, so
 * no engine write can reach past it and no engine read can see what is there.
 *
 * Above it sits one bank per tileset gfx *group*, holding that group's
 * character data for the areas that swap tilesets by camera position (B27).
 * A tilemap entry's tile index is 10 bits and BGxCNT's charbase is 2 bits, so
 * no hardware encoding could reach a second copy — the renderer adds the
 * offset itself, per tile, from the tile's own room position.
 *
 * Indexed by group id rather than by "the other one", because Minish Village
 * has five alternatives for the same addresses and can need three of them on
 * screen at once; group id keeps the mapping constant and stateless. Each
 * bank mirrors the whole 96 KB so one stride serves any charbase window. Six
 * banks covers Hyrule Town's group ids 0..5, the widest in use.
 *
 * The group the engine has actually loaded keeps reading the GBA's own VRAM
 * — offset 0, no bank — so anything that writes character data behind the
 * manager's back is still seen. Hyrule Town's second oracle house is exactly
 * that: an overlay written into slot 1's range after the group load. */
#define PORT_VRAM_GBA_SIZE 0x18000u
#define PORT_VRAM_BANK_STRIDE 0x18000u
#define PORT_VRAM_BANKS 6u
#define PORT_VRAM_SHADOW_OFFSET PORT_VRAM_GBA_SIZE
#define PORT_VRAM_BANK_OFFSET(group) (PORT_VRAM_SHADOW_OFFSET + (u32)(group) * PORT_VRAM_BANK_STRIDE)
#define PORT_VRAM_TOTAL_SIZE (PORT_VRAM_GBA_SIZE + PORT_VRAM_BANKS * PORT_VRAM_BANK_STRIDE)
extern u8 gVram[PORT_VRAM_TOTAL_SIZE]; // 0x06000000-0x06017FFF, plus the group banks

/* OAM Y side channel — the sprite y that attr0's 8 bits cannot hold.
 *
 * A GBA OBJ stores y in 8 bits and hardware resolves "above the top edge"
 * by wrapping mod 256, which an emulator recovers as "values >= the screen
 * height are negative". That recovery needs the off-screen band [height,255]
 * to be wide enough for the most negative y a sprite can have. At 160 lines
 * the band is 96 px and every sprite fits; at 240 it is 16 px and sprites
 * straddling the top edge land back on the visible screen instead (measured:
 * docs/spike2b-height-probe.md §3 — 103 frames / 118 entries on the route).
 *
 * The port owns both ends of this path — RenderSpritePieces (port_draw.c)
 * is the only writer of an *enabled* OAM entry, everything else parks 0x2A0
 * — so the untruncated y travels beside OAM rather than inside it. Two
 * arrays because OAM itself is double-buffered: the shadow is filled as
 * sprites are rendered, and latched to the hardware-side copy by the same
 * conditional DmaCopy32 that publishes gOAMControls.oam, so a frame that
 * skips the DMA keeps a matched pair rather than a fresh y against a stale
 * attr0. Indices match OAM slots 1:1.
 */
extern s16 gOamYExtShadow[0x80]; /* written by RenderSpritePieces */
extern s16 gOamYExt[0x80];       /* published to the PPU; read during raster */
void Port_OamYExt_Latch(void);

/* Per-scanline WIN0H side channel — the same problem one register along.
 *
 * The circular windows (lantern, fade iris, white triangle) rasterise a
 * per-line table of window edges that an HBlank DMA feeds to WIN0H. That
 * table is byte pairs, because WIN0H is byte pairs, so an edge past 255 has
 * nowhere to go — and the visible width is now 320. sub_0801E290 fills this
 * alongside the hardware table, indexed by scanline, and port_hdma_step_line
 * hands it to the PPU for the line it is about to draw.
 *
 * Validity is self-checking against the byte the DMA just wrote, exactly as
 * for the OAM y channel: a line whose low byte disagrees is one this channel
 * did not produce, and the hardware bytes are used instead.
 */
extern s16 gWin0hExtLeft[VIEWPORT_HEIGHT];
extern s16 gWin0hExtRight[VIEWPORT_HEIGHT];
/* The bytes actually written to the hardware table for the same line, kept
 * as the validity key.
 *
 * Matching the wide value's own low byte against the register was wrong, and
 * wrong precisely where this channel earns its keep: the hardware table is
 * *clamped* to 240, not truncated, so an edge of 286 stores 240 there and 286
 * here, the low bytes are 240 and 30, and the check rejected the very value
 * it exists to carry. The iris was sliced flat at x=240 for exactly that
 * reason. Comparing against what was written removes the coincidence. */
extern s16 gWin0hExtLeftKey[VIEWPORT_HEIGHT];
extern s16 gWin0hExtRightKey[VIEWPORT_HEIGHT];
void Port_Win0hExt_Reset(void);

// ROM data (loaded from baserom.gba)
extern u8* gRomData;
extern u32 gRomSize;

// ROM access logging (defined in port_rom.c)
void Port_LogRomAccess(u32 gba_addr, const char* caller);

void gba_write8(uint32_t addr, uint8_t v);
u8 gba_read8(uint32_t addr);
void gba_write16(uint32_t addr, uint16_t v);
u16 gba_read16(uint32_t addr);
void gba_write32(uint32_t addr, uint32_t v);
u32 gba_read32(uint32_t addr);

/*
 * gba_TryMemPtr — non-aborting address resolver.
 * Returns native pointer for known GBA ranges, NULL otherwise.
 */
static inline void* gba_TryMemPtr(uint32_t addr) {
    /* Before the generic EWRAM case: this range is gBG0Buffer, which no
     * longer lives inside gEwram. */
    if (addr >= GBA_BG0_BUFFER_ADDR && addr < GBA_BG0_BUFFER_ADDR + sizeof(gBG0Buffer))
        return (u8*)gBG0Buffer + (addr - GBA_BG0_BUFFER_ADDR);
    if (addr >= 0x02000000u && addr < 0x02040000u)
        return &gEwram[addr - 0x02000000u];
    if (addr >= 0x03000000u && addr < 0x03008000u)
        return &gIwram[addr - 0x03000000u];
    if (addr >= 0x04000000u && addr < 0x04000400u)
        return &gIoMem[addr - 0x04000000u];
    if (addr >= 0x05000000u && addr < 0x05000200u)
        return &gBgPltt[(addr - 0x05000000u) >> 1];
    if (addr >= 0x05000200u && addr < 0x05000400u)
        return &gObjPltt[(addr - 0x05000200u) >> 1];
    if (addr >= 0x06000000u && addr < 0x06018000u)
        return &gVram[addr - 0x06000000u];
    if (addr >= 0x07000000u && addr < 0x07000400u)
        return &gOamMem[(addr - 0x07000000u) >> 1];
    if (gRomData && addr >= 0x08000000u && addr < 0x08000000u + gRomSize) {
        Port_LogRomAccess(addr, "gba_TryMemPtr");
        return &gRomData[addr - 0x08000000u];
    }
    return NULL;
}

static inline void* gba_MemPtr(uint32_t addr) {
    void* ptr = gba_TryMemPtr(addr);
    if (ptr)
        return ptr;
#if defined(__GNUC__)
    void* caller = __builtin_return_address(0);
    fprintf(stderr, "FATAL: gba_MemPtr: invalid address 0x%08X (called from %p)\n", addr, caller);
#elif defined(_MSC_VER)
    void* caller = _ReturnAddress();
    fprintf(stderr, "FATAL: gba_MemPtr: invalid address 0x%08X (called from %p)\n", addr, caller);
#else
    fprintf(stderr, "FATAL: gba_MemPtr: invalid address 0x%08X\n", addr);
#endif
    fflush(stderr);
#if defined(_MSC_VER)
    __debugbreak();
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    __asm__ volatile("int3");
#endif
    abort();
    return NULL;
}

/*
 * port_resolve_addr — resolve a value that may be a GBA address or native ptr.
 * Used by the DMA emulation layer.
 */
#ifdef __cplusplus
extern "C" {
#endif
void* port_resolve_addr(uintptr_t val);
#ifdef __cplusplus
}
#endif

static inline void gba_MemClear(u32 addr, u32 size) {
    void* ptr = gba_MemPtr(addr);
    if (ptr != NULL) {
        for (u32 i = 0; i < size; i++) {
            ((u8*)ptr)[i] = 0;
        }
    }
}

static inline void gba_MemCopy(u32 srcAddr, u32 destAddr, u32 size) {
    void* src = gba_MemPtr(srcAddr);
    void* dest = gba_MemPtr(destAddr);
    if (src != NULL && dest != NULL) {
        for (u32 i = 0; i < size; i++) {
            ((u8*)dest)[i] = ((u8*)src)[i];
        }
    }
}

static inline void port_MemCopyToGBA(const void* src, u32 destAddr, u32 size) {
    void* dest = gba_TryMemPtr(destAddr);
    if (src != NULL && dest != NULL) {
        for (u32 i = 0; i < size; i++) {
            ((u8*)dest)[i] = ((const u8*)src)[i];
        }
    }
}
