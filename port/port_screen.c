/* Forwards full-width window bounds from the engine to the PPU.
 *
 * The engine stores window bounds as winreg_t (32-bit, two 16-bit edges;
 * see include/screen.h). The GBA registers carry only 8 bits per edge, so
 * interrupts.c writes the truncated form there for hardware fidelity and
 * calls this with the untruncated values.
 *
 * Bounds are pushed every frame from the same place the registers are
 * committed, so the PPU sees window state from the same generation as
 * everything else it renders — the frame-alignment lesson from Spike 3.
 */
#include "screen.h"

#include <stdio.h>
#include <stdlib.h>

#include "cpu/mode1.h"

/* TMC_WINTRACE=1 reports the widest edge committed during a run, which is
 * how the >255 path is shown to be live rather than merely compiled. */
static int sTraceEnabled = -1;
static int sMaxEdge = 0;

static void wintrace_report(void) {
    fprintf(stderr, "[wintrace] widest window edge committed: %d (8-bit register ceiling is 255)\n",
            sMaxEdge);
}

static void wintrace_note(int a, int b) {
    if (sTraceEnabled < 0) {
        sTraceEnabled = (getenv("TMC_WINTRACE") != NULL);
        if (sTraceEnabled) {
            atexit(wintrace_report);
        }
    }
    if (a > sMaxEdge) {
        sMaxEdge = a;
    }
    if (b > sMaxEdge) {
        sMaxEdge = b;
    }
}

void Port_Screen_CommitWindows(winreg_t win0h, winreg_t win0v, winreg_t win1h, winreg_t win1v) {
    VirtuaPPUMode1WindowBounds b;

    wintrace_note((int)WIN_GET_HIGHER(win0h), (int)WIN_GET_LOWER(win0h));
    wintrace_note((int)WIN_GET_HIGHER(win1h), (int)WIN_GET_LOWER(win1h));

    b.left = (int)WIN_GET_HIGHER(win0h);
    b.right = (int)WIN_GET_LOWER(win0h);
    b.top = (int)WIN_GET_HIGHER(win0v);
    b.bottom = (int)WIN_GET_LOWER(win0v);
    virtuappu_mode1_set_window_bounds(0, &b);

    b.left = (int)WIN_GET_HIGHER(win1h);
    b.right = (int)WIN_GET_LOWER(win1h);
    b.top = (int)WIN_GET_HIGHER(win1v);
    b.bottom = (int)WIN_GET_LOWER(win1v);
    virtuappu_mode1_set_window_bounds(1, &b);
}
