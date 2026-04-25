/**
 * @file ram_silent_stubs.c
 * @brief Silent (non-fatal) overrides for unported `ram_*` ARM-assembly
 *        helpers that the boot-path call graph reaches during the
 *        TMC_LINK_GAME_SOURCES=ON smoke test.
 *
 * Sub-step 2b.4b of the SDL-port roadmap (see docs/sdl_port.md). The
 * decompiled game is built around a small set of helpers that live in
 * `asm/src/code_*.s` / `asm/src/intr.s` and are loaded into RAM at boot
 * (hence the `ram_` prefix). The host build does not assemble those
 * files, so `port_unresolved_stubs.c` provides weak `noreturn`
 * abort-traps for every such symbol; that is fine for "must never run"
 * symbols but makes `AgbMain` SIGABRT the moment its main loop calls
 * into one of them.
 *
 * For symbols whose only effect is a visual/audio polish (palette
 * fades, sprite drawing, etc.), it is safe to install a real (non-weak)
 * silent stub during the runtime-flip ramp-up: the game state stays
 * consistent and the smoke test no longer aborts. When the real C port
 * lands later in the roadmap (or the original `.s` file is wired into
 * the same host binary), this stub must be removed or compiled out:
 * both it and the real implementation are strong definitions, so
 * keeping both would cause a multiple-definition link error. The
 * matching ROM build is unchanged because none of these `__PORT__`-only
 * TUs participate in the GBA Makefile.
 *
 * Each stub is intentionally declared with a `void(void)` prototype --
 * the linker resolves by name only, so callers compiled against the
 * "real" multi-arg signature still resolve correctly; the args land in
 * registers per the host SysV / Win64 ABI and are ignored by these
 * empty bodies. (The same convention is used by `asm_stubs.c`.)
 */

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#endif

#include <string.h>

/* Palette fade compositor.
 *
 * Real (still-unported) implementation in
 * `asm/src/intr.s::arm_MakeFadeBuff256` reads 16 BGR555 colours from
 * `src` (one palette in `gPaletteBuffer`) and writes 16 BGR555 colours
 * to `dst` (the matching slot of `PAL_RAM` / `BG_PLTT`), with each
 * channel passed through `gUnk_08000F54[brightness][channel]` (a 64-
 * entry per-channel LUT) and blended by `(unk1, unk2)` for the
 * fade-in / fade-out animation. Hit by `FadeVBlank` once per "used"
 * palette per frame.
 *
 * On the host port we only need this enough to land the loaded palette
 * data on the renderer's side. When no fade is active -- which is the
 * default after `InitFade`'s `MemClear(&gUnk_020354C0, ...)` and is
 * the state at the title screen and during normal gameplay outside
 * room transitions -- `(unk1, unk2)` are both 0 and the ARM kernel
 * collapses to a per-channel LUT lookup. With brightness 0 that LUT
 * is the identity, so the resulting blit is a straight 32-byte copy
 * from `src` to `dst`, which is what we do here.
 *
 * What this stub deliberately does *not* yet replicate (out of scope
 * for the asset-integration PR; tracked for follow-up):
 *
 *   - The per-channel brightness LUT (`gUnk_08000F54`). At
 *     `SetBrightness(1)` / brightness 2 the real game scales each 5-
 *     bit channel through a saturated curve. Without the LUT data we
 *     show colours at raw brightness, which matches what the comment
 *     above the previous no-op already promised ("full saturation
 *     throughout fades").
 *   - The (`unk1`, `unk2`) blend used by `FadeMain`/`FadeIn`/`FadeOut`
 *     for the area-transition cross-fade. Until this lands, fades
 *     snap instantaneously; no state is corrupted.
 *
 * Both follow-ups are pure visual polish on top of the now-correct
 * palette flush; neither blocks any boot-path code.
 */
void ram_MakeFadeBuff256(const void* src, void* dst, unsigned int unk2, unsigned int unk1) {
    (void)unk2;
    (void)unk1;
    /* One palette = 16 colours * 2 bytes = 32 bytes, matching the
     * `mov ip, #0x10` / `ldrh ... [r0], #2` / `strh ... [r1], #2`
     * loop in `arm_MakeFadeBuff256`. */
    if (src != NULL && dst != NULL) {
        memcpy(dst, src, 32);
    }
}

/* `ram_UpdateEntities`, `ram_DrawEntities`, and
 * `ram_ClearAndUpdateEntities` previously lived here as silent no-ops;
 * they now have strong host-side ports in `port_entity_runtime.c`,
 * which also provides the real `DrawEntity` (no longer a trap stub
 * in `asm_stubs.c`). Keeping the stubs here would multiply-define
 * the symbols at link time. See that TU's banner comment for why
 * directly emitting OAM from `DrawEntity` is the smallest slice that
 * gets the title-screen Zelda logo and chain sprites onto the screen.
 * The matching ROM build never sees this file or the entity-runtime
 * port (both are `__PORT__`-only). */

/* `ram_DrawDirect` and `ram_sub_080ADA04` previously lived here as
 * silent no-ops; they now have strong host-side ports in
 * `port_oam_renderer.c`. Keeping the stubs here would multiply-define
 * the symbols at link time. See that TU's banner comment for why this
 * was the smallest slice that restores the title-screen "PRESS START"
 * and copyright "©" sprites. The matching ROM build never sees either
 * file. */

/* `UpdateCollision` is the survivor-bookkeeping call the entity
 * iterator (now in `port_entity_runtime.c::ram_UpdateEntities`) makes
 * after each per-entity dispatch. The ROM body lives in
 * `asm/src/intr.s` and starts by checking `entity->flags & ENT_COLLIDE`
 * (offset 0x10, mask 0x80); for any entity with that bit clear it
 * exits in a single instruction. Title-screen `TITLE_SCREEN_OBJECT`
 * entities never set `ENT_COLLIDE` (`AppendEntityToList` does not
 * touch it and `sub_080A2340` never opts in via `COLLISION_ON`), so a
 * silent no-op here is observably identical to the real body for the
 * title screen. When a future scene spawns colliding entities, this
 * stub must be replaced with a real port (or the ARM body must be
 * wired in) -- otherwise hitbox / damage processing silently drops on
 * the floor. Same removal contract as the other silent ram_* stubs. */
void UpdateCollision(void* this) {
    (void)this;
}

/* `collision.c::CollisionMain` -> ram_CollideAll. Walks the entity
 * collision list (also empty during the smoke test). With nothing to
 * collide, a no-op is faithful. Same removal contract. */
void ram_CollideAll(void) {
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
