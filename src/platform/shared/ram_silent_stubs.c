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

/* Palette fade compositor. Real implementation blends `src` into `dst`
 * scaled by a 0..256 ratio. A silent no-op leaves PAL_RAM untouched,
 * which renders the screen at full saturation throughout fades but
 * does not corrupt state. Hit by `FadeVBlank` once per visible frame. */
void ram_MakeFadeBuff256(void) {
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
