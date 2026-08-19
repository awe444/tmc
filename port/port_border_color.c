#include "port_border_color.h"

#include "global.h"
#include "main.h"
#include "structures.h"
#include "subtask.h"
#include "game.h"
#include "viewport.h"
#include "port_mapsource.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PC_PORT) && (UI_CENTER_DX > 0 || UI_CENTER_DY > 0)

extern u16 gBgPltt[256];
extern struct_020354C0 gUnk_020354C0[0x20];
void Port_FadeApply16(const u16* srcPtr, u16* dstPtr, u16 intensity, u8 color);

/* GBA colours are 5 bits per channel, blue in the high bits. */
#define BGR555(r, g, b) ((u16)((r) | ((g) << 5) | ((b) << 10)))

/* Taken from the scenes that already wear them, so "make X's border look like
 * Y's" stays literally that rather than a hand-mixed approximation. Each was
 * read back out of gPaletteBuffer[0] on the scene itself:
 *   0x46C8  file select and the pause menu   (#40b088 green)
 *   0x57FF  title screen and the barrel      (#f8f8a8 pale yellow)
 * The port renders 5-bit channels as v<<3, which is where the hex is from. */
#define BORDER_GREEN       BGR555(8, 22, 17)   /* 0x46C8 */
#define BORDER_PALE_YELLOW BGR555(31, 31, 21)  /* 0x57FF */
#define BORDER_BLACK       BGR555(0, 0, 0)

/* Index into sIntroSequenceHandlers (src/title.c). TASK_TITLE runs the
 * Nintendo/Capcom logos at 0 and the title screen proper at 1, then its
 * fade-out at 2 — and only the last two are the pale-yellow screen the
 * recolour is about, so the logos keep their own white. There is no enum for
 * this; the handler table is the definition. */
#define INTRO_STEP_TITLESCREEN 1

/* Does this scene override its border, and with what?
 *
 * Only scenes the renderer has already decided to centre are eligible — a
 * full-viewport world view has no margin to colour, and asking here would
 * repaint the backdrop *inside* the picture. Port_MapSource_UiCentered and
 * Port_MapSource_AffineCentered are that decision, so the two cannot drift
 * apart. */
static bool32 Port_BorderColor_Target(u16* out) {
    if (Port_MapSource_UiCentered()) {
        if (gMain.task == TASK_TITLE) {
            /* Two screens share this task and only the second one is meant.
             *
             * The intro step advances a fade *before* the picture does, so
             * lastState is already past the logos for the 32 frames they
             * spend fading out — the step alone recolours them for half a
             * second. And unlike every other scene here, the logo screen's
             * backdrop is drawn *inside* the centred 240x160 as well: it is
             * that screen's own white background, so overriding it there
             * repaints the whole screen rather than its border.
             *
             * Requiring the pale yellow the title itself carries settles
             * both. The logo screen is white (0x7FFF) throughout, including
             * while it fades, so it is never eligible. */
            if (gUI.lastState >= INTRO_STEP_TITLESCREEN && gPaletteBuffer[0] == BORDER_PALE_YELLOW) {
                *out = BORDER_GREEN;
                return TRUE;
            }
            return FALSE;
        }
        if (gMain.task == TASK_GAME && gMain.substate == GAMEMAIN_SUBTASK && gUI.lastState == SUBTASK_PAUSEMENU) {
            *out = BORDER_PALE_YELLOW;
            return TRUE;
        }
        return FALSE;
    }
    /* The only affine world view is Deepwood's rolling barrel: the title
     * screen is the other affine site in the game (`grep DISPCNT_MODE_ src/`)
     * and it is a UI screen, handled above. */
    if (Port_MapSource_AffineCentered()) {
        *out = BORDER_BLACK;
        return TRUE;
    }
    return FALSE;
}

void Port_BorderColor_Apply(void) {
    u16 target;
    u16 src[16];
    u16 dst[16];
    static int trace = -1;

    if (trace < 0)
        trace = (getenv("TMC_BORDER_TRACE") != NULL);
    if (trace)
        fprintf(stderr, "[border] task=%u state=%u substate=%u last=%u ui=%d affine=%d buffer=%04x\n", gMain.task,
                gMain.state, gMain.substate, gUI.lastState, (int)Port_MapSource_UiCentered(),
                (int)Port_MapSource_AffineCentered(), gPaletteBuffer[0]);

    if (!Port_BorderColor_Target(&target))
        return;

    /* Run the chosen colour through the same transform FadeVBlank just ran
     * over palette bank 0, so the border fades with the scene instead of
     * staying lit through every transition. Reproducing the fade rather than
     * reusing it would be a second copy to keep in step. */
    memset(src, 0, sizeof(src));
    memset(dst, 0, sizeof(dst));
    src[0] = target;
    Port_FadeApply16(src, dst, gUnk_020354C0[0].unk2, gUnk_020354C0[0].unk1);
    gBgPltt[0] = dst[0];
}

#else

void Port_BorderColor_Apply(void) {
}

#endif
