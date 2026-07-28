#ifndef PORT_CAPTURE_H
#define PORT_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Spike 0 capture/replay tooling: deterministic input scripting,
 * framebuffer dumps, and frame-time statistics.
 *
 * The engine's RNG is fully deterministic (gRand seeded to a constant in
 * AgbMain, Random() is a pure LCG in port_linked_stubs.c), so a script
 * that supplies frame-exact KEYINPUT makes whole runs reproducible:
 * two runs of the same script produce byte-identical framebuffer dumps.
 * Combine with SDL_VIDEODRIVER=dummy for headless capture.
 *
 * See docs/viewport-expansion-research-plan.md (Spike 0) and
 * tools/capture/README.md for the script format and workflow. */

/* Parses --script= / --dump-dir= / --frame-stats / --exit-frame=.
 * Returns true if the argument was consumed. */
bool Port_Capture_HandleArg(const char* arg);
void Port_Capture_PrintUsage(void);

/* True while an input script is driving KEYINPUT. Port_UpdateInput uses
 * this to suppress its own title auto-START hack, which would otherwise
 * inject non-scripted input during a deterministic run. */
bool Port_Capture_ScriptActive(void);

/* --uncapped: VBlankIntrWait skips frame pacing (and vsync), so scripted
 * headless runs complete as fast as the machine allows. */
bool Port_Capture_Uncapped(void);

/* Called at the end of Port_UpdateInput: while a script is active,
 * overwrites the committed KEYINPUT with the script's held-key state. */
void Port_Capture_OverrideInput(volatile uint16_t* keyinput);

/* Called once per frame from VBlankIntrWait, after PresentFrame (so a
 * dump command reads exactly the frame that was just presented).
 * logicNs   = engine time between the previous VBlankIntrWait return and
 *             this call's entry (game logic for this frame);
 * presentNs = time spent in Port_PPU_PresentFrame this call. */
void Port_Capture_OnVBlank(uint64_t logicNs, uint64_t presentNs);

#ifdef __cplusplus
}
#endif

#endif /* PORT_CAPTURE_H */
