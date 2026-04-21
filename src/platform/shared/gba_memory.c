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

void Port_InitMemory(void)
{
    memset(gPortEwram, 0, sizeof(gPortEwram));
    memset(gPortIwram, 0, sizeof(gPortIwram));
    memset(gPortVram,  0, sizeof(gPortVram));
    memset(gPortOam,   0, sizeof(gPortOam));
    memset(gPortPltt,  0, sizeof(gPortPltt));
    memset(gPortIo,    0, sizeof(gPortIo));
}
