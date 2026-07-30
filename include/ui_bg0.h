#ifndef UI_BG0_H
#define UI_BG0_H

#include "global.h"
#include "viewport.h"

/* BG0 staging tilemap: the HUD, text boxes and every full-screen UI
 * surface are drawn here.
 *
 * Under PC_PORT this used to be a #define aliasing a fixed offset inside
 * gEwram. It is a real array now, because edge-anchored UI at a wider
 * viewport needs more than a hardware screenblock's 32 columns
 * (docs/viewport-expansion-research-plan.md §4 blocker 1). Declared in one
 * place so the row stride below cannot disagree with it. */
#ifdef PC_PORT
extern u16 gBG0Buffer[UI_BG0_ENTRIES];
#else
extern u16 gBG0Buffer[0x400];
#endif

/* Row stride of the tilemap a UI drawing routine is writing into.
 *
 * These routines were written against a 32-entry row, because that is a
 * hardware screenblock's width. gBG0Buffer can now be wider, so advancing
 * by a literal 0x20 would step into the middle of the next row. Deriving
 * the stride from the destination keeps BG1/BG3 and VRAM destinations on
 * 0x20 while BG0 uses its real width; at GBA-native width every case is
 * 0x20 and nothing changes. */
static inline u32 UiDestStride(const u16* dest) {
#ifdef PC_PORT
    if (dest >= gBG0Buffer && dest < gBG0Buffer + UI_BG0_ENTRIES) {
        return (u32)UI_BG0_WIDTH_TILES;
    }
#else
    (void)dest;
#endif
    return 0x20u;
}

#endif // UI_BG0_H
