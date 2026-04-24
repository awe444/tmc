/**
 * @file port_load_resource.c
 * @brief Host-C port of the deferred-resource enqueue helpers from
 *        `asm/src/code_08000E44.s` (`LoadResourceAsync`,
 *        `sub_08000E92`) plus the matching host-side reader that
 *        `interrupts.c::LoadResources` dispatches into under
 *        `__PORT__`.
 *
 * The ROM build encodes each queue entry as 12 packed bytes:
 *
 *   entry[0]    = type   (u8)
 *   entry[2..3] = size   (u16)
 *   entry[4..7] = src    (u32 pointer on GBA)
 *   entry[8..11]= dst    (u32 pointer on GBA)
 *
 * That layout is hard-baked into the THUMB writer in
 * `code_08000E44.s` (and into the byte-offset reader in
 * `interrupts.c::LoadResources` that we keep for the GBA build). On a
 * 64-bit host the layout is unsafe two ways:
 *
 *   1) `void*` is 8 bytes, not 4, so two pointers + type + size no
 *      longer fits in 12 bytes.
 *   2) Even if you widen the entry, the original reader reads the
 *      pointer slots through `*(u32*)`, which truncates the high 32
 *      bits of any host pointer and SIGSEGVs `DmaCopy32` on the
 *      first dispatched entry.
 *
 * Fix: define a host-pointer-safe `PortLoadResourceEntry` struct
 * shared by the writer and the host reader, and route
 * `interrupts.c::LoadResources` through `Port_LoadResources()` (this
 * file) under `#ifdef __PORT__`. The GBA byte-offset path is kept
 * intact so the ROM build is unchanged.
 *
 * The first user that trips the original 12-byte layout on the host
 * is `sub_0805F918` -> `LoadResourceAsync(&gUnk_02036AD8, ...)` from
 * `src/text.c:743`, which fires the moment the file-select save-
 * creation dialog draws its first text-box border (this is the next
 * crash the 120 s alternating-A/START scripted-input test hits at
 * f≈721 after the `gUnk_081092AC` zero-buffer and the `sub_0805EEB4`
 * pointer-truncation fixes earlier in this PR).
 */

#ifdef __PORT__

#include <stdint.h>
#include <string.h>

#include "global.h"
#include "asm.h"

extern u8 gUnk_03003DE0;

void LoadResourceAsync(const void* src, void* dest, u32 size);
void sub_08000E92(const void* src, void* dest, u32 size);
void Port_LoadResources(void);

/* Host-pointer-safe queue entry. `type` and `size` keep the same
 * semantics as the GBA layout; `src` and `dst` are full host
 * pointers. The struct intentionally mirrors the field order of the
 * THUMB writer so the file reads side-by-side with `code_08000E44.s`. */
typedef struct {
    u8 type;        /* 0 = DMA copy, 1 = LZ77 decompress, 2 = DMA fill */
    u8 _pad;
    u16 size;       /* byte size for type 0 / 2 (matches asm `strh r2`) */
    const void* src;
    void* dst;
} PortLoadResourceEntry;

/* Asm cap is `0x28` (40) entries; one extra slot of headroom keeps
 * the writer's bounds check trivially correct under any future
 * tweaks. */
#define PORT_LOAD_RESOURCE_QUEUE_CAP 40

static PortLoadResourceEntry sPortLoadResourceQueue[PORT_LOAD_RESOURCE_QUEUE_CAP];

/* The shared symbol `gUnk_03000C30` still has to exist for the GBA
 * `interrupts.c` byte-offset reader to link against (it is referenced
 * via `extern u8 gUnk_03000C30;` in `interrupts.c`). On the host the
 * reader is `Port_LoadResources()` instead, so the placeholder buffer
 * is unused, but providing a strong, correctly-sized definition here
 * keeps the linker happy and overrides the 256-byte weak placeholder
 * in `port_unresolved_stubs.c` (which was too small for 40 entries
 * anyway under the old layout). 16-byte alignment matches the
 * placeholder. */
u8 gUnk_03000C30[12 * PORT_LOAD_RESOURCE_QUEUE_CAP] __attribute__((aligned(16)));

/* The asm tail at `_08000E98` shared by both entry points. Type 0 is
 * the DMA-copy path, type 1 is the LZ77-decompress path. */
static void Port_EnqueueResource(u8 type, const void* src, void* dest, u32 size) {
    u8 count = gUnk_03003DE0;
    if (count >= PORT_LOAD_RESOURCE_QUEUE_CAP) {
        return; /* queue full -> drop, matches asm `bhs _08000EB6` */
    }
    /* Asm post-increments `count` then writes at `12 * count`; with
     * the queue stored as an array of structs the equivalent target
     * slot is `[count]` (asm slot 0 at base+0 is intentionally never
     * used; reader walks from base+12). */
    sPortLoadResourceQueue[count].type = type;
    sPortLoadResourceQueue[count]._pad = 0;
    sPortLoadResourceQueue[count].size = (u16)size;
    sPortLoadResourceQueue[count].src = src;
    sPortLoadResourceQueue[count].dst = dest;
    gUnk_03003DE0 = (u8)(count + 1);
}

void LoadResourceAsync(const void* src, void* dest, u32 size) {
    Port_EnqueueResource(0, src, dest, size);
}

void sub_08000E92(const void* src, void* dest, u32 size) {
    Port_EnqueueResource(1, src, dest, size);
}

/* Host-side drain, called from `interrupts.c::LoadResources` under
 * `__PORT__`. Mirrors the original C reader's switch arms but
 * dereferences the queue through the host-pointer-safe struct so no
 * pointer is truncated to 32 bits. */
void Port_LoadResources(void) {
    u8 count = gUnk_03003DE0;
    u8 i;

    if (count == 0) {
        return;
    }
    if (count > PORT_LOAD_RESOURCE_QUEUE_CAP) {
        count = PORT_LOAD_RESOURCE_QUEUE_CAP;
    }
    /* Reset before dispatch so that any LoadResourceAsync calls a
     * dispatched routine itself makes are honoured next frame
     * (matches the original reader's `gUnk_03003DE0 = 0;` ordering). */
    gUnk_03003DE0 = 0;

    for (i = 0; i < count; i++) {
        const PortLoadResourceEntry* e = &sPortLoadResourceQueue[i];
        switch (e->type) {
            case 0:
                DmaCopy32(3, e->src, e->dst, e->size);
                break;
            case 1:
                LZ77UnCompVram(e->src, (u8*)e->dst);
                break;
            case 2:
                DmaFill32(3, (u32)(uintptr_t)e->src, e->dst, e->size);
                break;
            default:
                /* Unknown command type — the GBA reader silently
                 * ignores anything outside 0..2 and so do we. */
                break;
        }
    }
}

#endif /* __PORT__ */
