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

/* Per-frame entity dispatcher. The real (still-unported) ARM
 * implementation in `asm/src/intr.s::arm_UpdateEntities` walks
 * `gEntityLists[0..7]` (mode 0) or `gEntityLists[8]` (mode 1, managers)
 * and, for each entity in the list, dispatches to a per-kind update
 * function (PlayerUpdate / EnemyUpdate / ProjectileUpdate /
 * ObjectUpdate / NPCUpdate / ItemUpdate / ManagerUpdate /
 * DeleteThisEntity), then calls `UpdateCollision` on the survivor.
 *
 * A silent no-op is safe during the runtime-flip ramp-up because the
 * entity lists during the headless `--frames=30` smoke test are still
 * populated only by code paths whose loaders are themselves stubs (see
 * `port_rom_data_stubs.c::sPortGfxGroupTerminator` -- `LoadGfxGroup`
 * short-circuits before any entity is spawned). With no entities live,
 * the real iterator's body would also be a no-op, so skipping the walk
 * matches the engine's observable state and prevents the SIGABRT that
 * the previous weak `Port_UnresolvedTrap` placeholder produced. When
 * the real C decomp of `arm_UpdateEntities` lands (or a future PR
 * starts spawning entities through real loaders), this stub must be
 * removed in the same commit so the strong real definition wins. */
void ram_UpdateEntities(unsigned int mode) {
    (void)mode;
}

/* Longjmp-style "abort the current entity update and resume the
 * iteration loop" helper. The real ARM implementation in
 * `asm/src/intr.s::arm_ClearAndUpdateEntities` reuses
 * `arm_UpdateEntities`'s saved register frame (stashed in
 * `gUpdateContext.restore_sp`) to skip past the dispatch's call site
 * straight to the per-entity post-update bookkeeping. It is reached
 * from `entity.c::DeleteThisEntity`, which is itself only called from
 * inside the entity update path -- and that path is short-circuited
 * by the no-op `ram_UpdateEntities` above. A silent no-op here is
 * therefore reachable only by future code paths that we have not yet
 * exercised; if such a path lands before the real decomp does, this
 * stub is preferable to an abort because it does not deadlock the
 * frame loop. The same removal contract as `ram_UpdateEntities`
 * applies. */
void ram_ClearAndUpdateEntities(void) {
}

/* Per-frame OAM compositor for entities. The real ARM implementation
 * in `asm/src/code_080A1A30.s::arm_DrawEntities` walks the entity
 * lists again (this time for rendering only) and pushes per-entity
 * `OAMCommand`s into `gOamBuffer`. Because the host has no live
 * entities (see `ram_UpdateEntities` notes above), there is nothing
 * to draw; the host renderer (`render.c`) reads OAM directly from
 * `gPortOam` and composites whatever the rest of the engine has
 * already written there. Same removal contract as
 * `ram_UpdateEntities`. */
void ram_DrawEntities(void) {
}

/* Single-shot OAM helper (`affine.c::DrawDirect` -> ram_DrawDirect).
 * Invoked from item / player paths that are themselves gated behind
 * the entity update loop, so this is unreachable while
 * `ram_UpdateEntities` is a no-op. The stub keeps the link / call
 * convention correct if a non-entity caller is added later. */
void ram_DrawDirect(void* cmd, unsigned int spriteIndex, unsigned int frameIndex) {
    (void)cmd;
    (void)spriteIndex;
    (void)frameIndex;
}

/* `affine.c::sub_080ADA04` -> ram_sub_080ADA04. Same reachability
 * argument as `ram_DrawDirect`. */
void ram_sub_080ADA04(void* cmd, void* dst) {
    (void)cmd;
    (void)dst;
}

/* `collision.c::CollisionMain` -> ram_CollideAll. Walks the entity
 * collision list (also empty during the smoke test). With nothing to
 * collide, a no-op is faithful. Same removal contract. */
void ram_CollideAll(void) {
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
