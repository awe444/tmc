#ifdef __PORT__

#include "global.h"
#include "gba/syscall.h"

u32 CalcDistance(s32 x, s32 y);
u32 CalculateDirectionFromOffsets(s32 x, s32 y);
extern const u16 gUnk_080C93E0[];

u32 CalcDistance(s32 x, s32 y) {
    /*
     * asm/src/code_08003FC4.s::CalcDistance:
     *   return Sqrt(((x*x) + (y*y)) << 8);
     *
     * Keep math in u32 to match the GBA's low-32-bit MUL behavior and
     * avoid signed-overflow UB in host C.
     */
    const u32 xx = (u32)x * (u32)x;
    const u32 yy = (u32)y * (u32)y;
    return Sqrt((xx + yy) << 8);
}

u32 CalculateDirectionFromOffsets(s32 x, s32 y) {
    u32 octant = 0x40;

    if (x != 0) {
        u32 ratio = (u32)Div(y << 8, x);
        u32 start;
        u32 end;

        if ((s32)ratio < 0) {
            ratio = (u32)(-(s32)ratio);
        }

        if (ratio < 0x106) {
            if (ratio < 0x6e) {
                octant = 0;
                start = 0x00;
                end = 0x20;
            } else {
                start = 0x20;
                end = 0x40;
            }
        } else if (ratio < 0x280) {
            start = 0x40;
            end = 0x60;
        } else {
            start = 0x60;
            end = 0x7e;
        }

        for (u32 i = start; i < end; i += 2) {
            if ((ratio >= gUnk_080C93E0[i]) && (ratio < gUnk_080C93E0[i + 1])) {
                octant = (i >> 1) + 1;
                break;
            }
        }
    }

    if (x < 0) {
        return (y < 0) ? (0xc0 + octant) : (0xc0 - octant);
    }
    return (y < 0) ? (0x40 - octant) : (0x40 + octant);
}

#endif /* __PORT__ */
