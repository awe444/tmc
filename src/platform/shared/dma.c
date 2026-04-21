/**
 * @file dma.c
 * @brief Host-side replacements for the GBA DMA helpers used throughout
 *        the decompiled game source (DmaSet/DmaCopy/DmaFill/DmaStop/DmaWait
 *        and friends in include/gba/macro.h).
 *
 * For the SDL port these are simple memcpy/memset wrappers — every "DMA"
 * is just a synchronous copy executed on the host CPU. The notable
 * exception is DMA0 used by `PerformVBlankDMA` (src/interrupts.c) which on
 * real hardware is queued and runs at the next VBlank; PR #2 will plumb
 * that into the queue exposed from src/platform/shared/interrupts.c.
 *
 * Until the game source is wired in (PR #2) these symbols are unused; we
 * still build them so the platform layer is link-tested in CI.
 */
#include "platform/port.h"

#include <stdint.h>
#include <string.h>

/* The signatures here intentionally match the macros in
 * include/gba/macro.h (DmaSet, DmaCopy16, DmaCopy32, DmaFill16, DmaFill32,
 * etc.) so that PR #2 can replace those macros with calls into here. */

void Port_DmaCopy16(int channel, const void* src, void* dst, uint32_t size) {
    (void)channel;
    memcpy(dst, src, size);
}

void Port_DmaCopy32(int channel, const void* src, void* dst, uint32_t size) {
    (void)channel;
    memcpy(dst, src, size);
}

void Port_DmaFill16(int channel, uint16_t value, void* dst, uint32_t size) {
    (void)channel;
    uint16_t* p = (uint16_t*)dst;
    uint32_t cnt = size / sizeof(uint16_t);
    for (uint32_t i = 0; i < cnt; ++i) {
        p[i] = value;
    }
}

void Port_DmaFill32(int channel, uint32_t value, void* dst, uint32_t size) {
    (void)channel;
    uint32_t* p = (uint32_t*)dst;
    uint32_t cnt = size / sizeof(uint32_t);
    for (uint32_t i = 0; i < cnt; ++i) {
        p[i] = value;
    }
}

void Port_DmaStop(int channel) {
    (void)channel;
    /* No-op: we have no in-flight DMAs in the synchronous host model. */
}

void Port_DmaWait(int channel) {
    (void)channel;
    /* No-op for the same reason. */
}

void Port_DmaSet(int channel, const void* src, void* dst, uint32_t control) {
    /* `control` packs the count + transfer flags exactly the way the GBA
     * REG_DMAxCNT register does. PR #2 will decode it; for now we treat
     * any DmaSet as a synchronous copy of `control & 0xFFFF` units. */
    (void)channel;
    uint32_t count = control & 0xFFFFu;
    int is_32 = (control & (1u << 26)) != 0;
    uint32_t bytes = is_32 ? count * 4u : count * 2u;
    memcpy(dst, src, bytes);
}
