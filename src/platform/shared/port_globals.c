/**
 * @file port_globals.c
 * @brief Host-side definitions of the GBA-build globals normally allocated
 *        by the linker script (see linker.ld).
 *
 * Sub-step 2b.4 of the SDL-port roadmap. On the GBA the globals below are
 * BSS-style allocations placed at fixed offsets in EWRAM/IWRAM by
 * `linker.ld` (e.g. `gMain = 0x03001000`, `gPaletteBuffer = 0x020176A0`).
 * The decompiled source declares them with `extern` in the matching
 * headers and never emits a definition in any C TU. That is fine for the
 * GBA build because the linker resolves them, but the SDL host build has
 * no equivalent SECTIONS block.
 *
 * This TU provides plain C definitions for every such symbol that is
 * actually referenced by the subset of `src/` linked into `tmc_sdl`.
 * Only the globals reached by the call graph rooted at `AgbMain` end up
 * here; the rest stay undefined and will be added in later PRs as more
 * game systems get wired in.
 *
 * Layout note: these structs do **not** match the GBA's
 * `static_assert(sizeof(X) == ...)` sizes on a 64-bit host because every
 * embedded pointer is 8 bytes instead of 4. The matching ROM build keeps
 * the real layout assertions; the SDL build collapsed them to no-ops in
 * `include/global.h` for exactly this reason (PR #2b.3 wave 1).
 */
#include "common.h"
#include "main.h"
#include "message.h"
#include "room.h"
#include "screen.h"

/* Main system state. */
Main gMain;
Input gInput;
Screen gScreen;
RoomControls gRoomControls;
struct_02000010 gUnk_02000010;
u32 gRand;

/* Message subsystem. */
Message gMessage;
TextRender gTextRender;

/* `Window` is declared as a file-local typedef inside src/message.c (it
 * is not exposed via a header), but the storage for `gNewWindow` /
 * `gCurrentWindow` lives in BSS on the GBA via linker.ld. Mirror the
 * struct definition here verbatim so the host can allocate the same
 * backing storage; sizeof differences are irrelevant because message.c
 * only ever uses `sizeof(*ptr)` against its own definition and never
 * reads it from this TU. */
typedef struct PortWindow {
    u8 unk0;
    u8 active;
    u8 unk2;
    u8 unk3;
    u8 xPos;
    u8 yPos;
    u8 width;
    u8 height;
} PortWindow;
PortWindow gNewWindow;
PortWindow gCurrentWindow;

/* Palette buffer (mirrors BG_PLTT in EWRAM). On hardware this is sized
 * to fit the BG palette region copied via DmaCopy32(.., BG_PLTT_SIZE).
 * Allocate enough u16s to cover the full palette (256 entries = 512 B). */
u16 gPaletteBuffer[256];

/* IWRAM-resident byte arrays. `gUnk_03003DE4` is a 12-byte mailbox used
 * by the VBlank-DMA helpers in src/main.c::SetVBlankDMA(). */
u8 gUnk_03003DE4[0xC];
