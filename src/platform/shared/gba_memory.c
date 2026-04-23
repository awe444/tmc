/**
 * @file gba_memory.c
 * @brief Host-side allocations for the regions the GBA exposes via its
 *        memory map. PR #2 will repoint the macros in include/gba/io_reg.h
 *        at fixed offsets inside these arrays.
 */
#include "platform/port.h"

#include <string.h>

uint8_t gPortEwram[PORT_EWRAM_SIZE];
uint8_t gPortIwram[PORT_IWRAM_SIZE];
uint8_t gPortVram[PORT_VRAM_SIZE];
uint8_t gPortOam[PORT_OAM_SIZE];
uint8_t gPortPltt[PORT_PLTT_SIZE];
uint8_t gPortIo[PORT_IO_SIZE];

void Port_InitMemory(void) {
    memset(gPortEwram, 0, sizeof(gPortEwram));
    memset(gPortIwram, 0, sizeof(gPortIwram));
    memset(gPortVram, 0, sizeof(gPortVram));
    memset(gPortOam, 0, sizeof(gPortOam));
    memset(gPortPltt, 0, sizeof(gPortPltt));
    memset(gPortIo, 0, sizeof(gPortIo));
}

void* Port_TranslateHwAddr(uintptr_t a) {
    /* The bounds match the GBA hardware memory map. The unmodified
     * decomp can hand any host helper a literal hardware address (most
     * commonly via the const tables in `data/gfx/...s` that store
     * `dest=0x06000000`-style targets), and on the host that literal
     * would dereference an unmapped low page. Translate it to the
     * matching offset inside the host-emulated array. Addresses outside
     * any known region (i.e. real host pointers from `gPort* + N` or
     * stack/heap pointers) are returned unchanged. */
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
