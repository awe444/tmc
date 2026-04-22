# SDL Port of The Minish Cap

This document describes the SDL host port of the TMC decompilation. It is the
companion to the top-level CMake build and lives alongside (not instead of)
the existing GBA ROM build (`make` / `GBA.mk`).

The port is modelled on the [SAT-R/sa2 SDL port](https://github.com/SAT-R/sa2),
which adds an `sa2.sdl` target on top of the matching ROM decompilation. The
goal here is the equivalent `tmc.sdl`.

> Status: **PR #1 of the roadmap is implemented, PR #2a (foundational
> `__PORT__` header rewiring) has landed, PR #2b is feature-complete:
> 2b.1, 2b.2, waves 1–3 of 2b.3, 2b.4a, the link-resolution half of
> 2b.4b, and the runtime flip of 2b.4b are all done — `tmc_sdl` now
> builds with `TMC_LINK_GAME_SOURCES=ON` by default, so the default
> binary boots into `src/main.c::AgbMain`, runs through
> `HandleNintendoCapcomLogos` into `HandleTitlescreen`, and reaches the
> title-screen idle. PR #3 (mirroring the
> SDL key bitmask into the emulated `REG_KEYINPUT` slot) has landed,
> PR #4 (software rasterizer for BG mode 0 + OBJ regular sprites,
> 4 bpp + 8 bpp) now drives `Port_VideoPresent()` from the emulated
> VRAM/OAM/PLTT/IO arrays, PR #5 has extended the rasterizer to
> cover affine BGs (modes 1/2), affine sprites, windows 0/1/OBJ/outside,
> alpha blending (BLDCNT/BLDALPHA), brightness fade up/down (BLDY) and
> mosaic, PR #6 (EEPROM-backed save persistence) routes
> `EEPROMRead` / `EEPROMWrite` through `Port_SaveReadByte` /
> `Port_SaveWriteByte` so file-select data survives a process exit,
> and PR #8 (golden-image CI mechanism) has landed: `tmc_sdl
> --print-frame-hash` emits a stable FNV-1a 64-bit hash of the
> rasterizer's framebuffer, and CI now asserts strict equality
> against the pinned value for *both* the default ON build (in
> `src/platform/shared/golden/usa_on_frames30.txt`) and the
> explicit-OFF build (in
> `src/platform/shared/golden/usa_off_frames30.txt`) on
> every run — the only renderer features still deferred are the
> bitmap modes (3/4/5) and HBlank-driven mid-scanline raster
> effects.** Every
> `src/**/*.c` TU that
> the SDL port can sensibly consume now builds clean under `__PORT__`
> (618 of 618; the 13 files that needed file-local fixes were patched
> behind `#ifdef __PORT__`), and `tmc_game_sources` is the full game
> tree. The default build links the game library into `tmc_sdl` — the
> ~850 symbols that still live only in unported `asm/src/`, the
> not-yet-linked `src/gba/m4a.c`, and the large const tables under
> `data/` are satisfied by weak abort/BSS stubs in
> `src/platform/shared/port_unresolved_stubs.c`. The `=OFF` build
> remains supported as an early-bring-up scaffold for future ports
> (PSP / PS2 / ...) and as a platform-layer isolation harness.** The
> default build produces a `tmc_sdl` executable that opens a 240×160
> (scaled 4×) window, accepts keyboard and gamepad input (via
> `SDL_GameController`), opens a silent SDL audio device,
> presents the emulated GBA framebuffer composited from the
> background tilemaps and OBJ layer, and runs at 59.7274 Hz. The
> GBA decomp headers
> (`include/gba/*.h`, `include/global.h`) now compile under a host C
> compiler when `__PORT__` is defined, with `REG_*`, `BG_PLTT`,
> `OBJ_PLTT`, `BG_VRAM`, `OAM`, `EWRAM_START`, `IWRAM_START`,
> `INTR_CHECK` and `INTR_VECTOR` aliasing the host arrays in
> `src/platform/shared/gba_memory.c`, and the agbcc-isms (`EWRAM_DATA`,
> `IWRAM_DATA`, `NAKED`, `FORCE_REGISTER`, `MEMORY_BARRIER`, `ASM_FUNC`,
> `NONMATCH`, `SystemCall`, …) collapsing to no-ops. The real game
> logic now both **links cleanly** and **boots** as far as the
> title-screen idle; advancing further into file-select / game-task
> transitions is the remaining audio-and-asm work tracked under
> PR #7 (m4a) and the asm-decomp track.


## Building

> **Supported host platforms: Linux and macOS only.** Microsoft
> Windows builds (MSVC, MinGW, vcpkg, cross-compile, etc.) are
> explicitly **not** supported by this port. Patches that re-add
> Windows-specific code paths will be rejected.

Prerequisites:

- CMake 3.16+
- GCC ≥ 9 or Clang ≥ 10
- SDL2 ≥ 2.0.18 (`SDL_GameController` + `SDL_RenderSetIntegerScale` are used)

### Linux

```sh
sudo apt install cmake ninja-build libsdl2-dev   # Debian/Ubuntu
cmake --preset sdl-release
cmake --build --preset sdl-release
./build/sdl-release/tmc_sdl
```

### macOS

```sh
brew install cmake ninja sdl2
cmake --preset sdl-release
cmake --build --preset sdl-release
./build/sdl-release/tmc_sdl
```

### CMake options

| Option                       | Default | Description                                              |
|------------------------------|---------|----------------------------------------------------------|
| `TMC_PORT_PLATFORM`          | `sdl`   | Reserved for future ports (`psp`, `ps2`).                |
| `TMC_GAME_VERSION`           | `USA`   | One of `USA`, `EU`, `JP`, `DEMO_USA`, `DEMO_JP`.         |
| `TMC_ENABLE_AUDIO`           | `ON`    | Open the SDL audio device (silent until PR #9).          |
| `TMC_ENABLE_GAMEPAD`         | `ON`    | Initialise `SDL_GameController` for gamepad input.       |
| `TMC_WIDESCREEN`             | `OFF`   | Reserve hooks for future widescreen renderer.            |
| `TMC_USE_FETCHCONTENT_SDL`   | `OFF`   | Build SDL2 from source via FetchContent if not on disk.  |
| `TMC_LINK_GAME_SOURCES`      | `ON`    | Link the `src/**/*.c` leaves that build under `__PORT__` into `tmc_sdl` (sub-step 2b.4). The library is always **built** as a dependency; this option controls whether it is also **linked** so that `src/main.c::AgbMain` becomes the entry point. Setting `=OFF` falls back to the empty-loop `agb_main_stub.c` placeholder, which is preserved as an early-bring-up scaffold for future ports and as a platform-layer isolation harness. |

## Running

```
tmc_sdl [options]
  --scale=N            Integer window scale (default 4 → 960×640).
  --fullscreen         Start in fullscreen-desktop mode.
  --mute               Disable audio output.
  --save-dir=PATH      Where to read/write tmc.sav (default: working dir).
  --frames=N           Run for N frames then exit (used by CI smoke tests).
  --screenshot=PATH    After the final frame, write a PPM (P6) screenshot
                       of the rasterizer's framebuffer to PATH (PR #8 of
                       the SDL roadmap; useful for local debugging).
  --print-frame-hash   After the final frame, print an FNV-1a 64-bit hash
                       of the framebuffer to stdout in the form
                       `frame-hash: 0x<16 hex>`. The CI golden-image
                       check (PR #8) consumes this; see
                       `src/platform/shared/golden/README.md`.
```

## Controls

| GBA button | Keyboard       | Gamepad (`SDL_GameController`)                     |
|------------|----------------|----------------------------------------------------|
| A          | X              | A button                                           |
| B          | Z              | B button                                           |
| Start      | Enter          | Start                                              |
| Select     | Right Shift / Backspace | Back                                      |
| D-pad      | Arrow keys     | D-pad **and** left analog stick (deadzone ≈ 8000)  |
| L          | A              | LB (also LT > half-press)                          |
| R          | S              | RB (also RT > half-press)                          |
| Quit       | Esc            | —                                                  |

A `gamecontrollerdb.txt` placed next to the executable will be loaded at
startup via `SDL_GameControllerAddMappingsFromFile`, which lets users add
mappings for unrecognised controllers without rebuilding.

## Save data

The port emulates a 64 KiB Flash chip in `tmc.sav` next to the executable
(or in the directory passed to `--save-dir=`). The file is created on first
write; it is loaded at startup and flushed on clean shutdown. Erased Flash
reads as `0xFF`, so any region not yet written by the game returns that
value — this matches real hardware so the existing `src/save.c` validation
checks behave identically.

## Layering

```
include/platform/port.h        ← public host API; no SDL types here
src/platform/sdl/              ← SDL-specific glue
    main.c    audio.c   input.c   video.c
src/platform/shared/           ← cross-port C: reused by future ports
    gba_memory.c               ← EWRAM/IWRAM/VRAM/OAM/PLTT/IO host arrays
    interrupts.c               ← VBlankIntrWait / frame pacing
    dma.c                      ← DmaCopy/DmaFill/DmaSet stand-ins
    bios.c                     ← LZ77UnComp, CpuSet, RegisterRamReset, …
    save_file.c                ← Flash backend (tmc.sav)
    m4a_stub.c                 ← silent m4a stand-ins
    asm_stubs.c                ← trap stubs for unported asm/src/*.s functions
    agb_main_stub.c            ← placeholder for src/main.c::AgbMain (PR #2)
    generated/assets/          ← host-port stubs for the ROM build's
                                  `assets/*_offsets.h`; regenerate with
                                  `tools/port/gen_offset_stubs.py`
src/                           ← unchanged GBA-decomp game source
include/                       ← unchanged GBA-decomp headers
```

The contract for adding a new port (e.g. `psp/`, `ps2/` later) is:

1. Provide an implementation of every function in `include/platform/port.h`.
2. List your sources in `CMakeLists.txt` under a new `if(TMC_PORT_PLATFORM
   STREQUAL "...")` branch instead of `sdl`.
3. Do **not** modify `src/` (the game) or `include/` (the GBA headers) — if
   you need to, add a `#ifdef __PORT__` switch and propose the change
   alongside the existing port.

## Roadmap

This document tracks the multi-PR plan. PR #1 is in this commit; the rest
are tracked here for future contributors.

- [x] **PR #1.** CMake skeleton + SDL platform stubs. Blank
  window, working keyboard + X-Input, silent audio, save-file scaffold,
  Ubuntu CI build + headless smoke test.
- [x] **PR #2a.** Foundational `__PORT__` header rewiring (this commit).
  `include/gba/io_reg.h`, `include/gba/defines.h`, and `include/gba/syscall.h`
  now repoint `REG_BASE`, `EWRAM_START`, `IWRAM_START`, `PLTT`, `VRAM`, `OAM`,
  `INTR_CHECK` and `INTR_VECTOR` at the host arrays from
  `include/platform/port.h` whenever `__PORT__` is defined, so every derived
  `REG_*` / `BG_*` / `OBJ_*` macro automatically resolves into
  `gPortIo` / `gPortVram` / `gPortPltt` / `gPortOam`. `EWRAM_DATA`,
  `IWRAM_DATA`, `NAKED`, `FORCE_REGISTER`, `MEMORY_BARRIER`, `ASM_FUNC`,
  `NONMATCH`, `BLOCK_CROSS_JUMP`, `asm_comment`, `asm_unified` and
  `SystemCall` collapse to no-ops on the host. A new
  `src/platform/shared/port_headers_check.c` translation unit exercises the
  rewired macros at compile time (static asserts) and at runtime (called
  once from the `agb_main_stub.c` smoke test) so any future header change
  that breaks host compilation is caught by the existing Ubuntu CI job.
- [ ] **PR #2b.** Wire `src/**/*.c` into the SDL target on top of the 2a
  foundation. Stub or temporarily decompile the remaining `asm/src/*.s`
  files (`crt0.s` / `intr.s` / `stack_check.s` / `veneer.s` drop out;
  `enemy.s`, `script.s`, `code_080xxxxx.s`, `player.s`, `projectileUpdate.s`
  need real decomp or `assert(0 && "not yet ported")` stubs). At the end
  of this PR the SDL window should run the real `AgbMain` loop instead of
  the placeholder in `src/platform/shared/agb_main_stub.c`.
  - [x] **2b.1** `src/platform/shared/asm_stubs.c` provides
    `assert(0 && "not yet ported")` trap stubs for all 106
    `thumb_func_start` / `arm_func_start` symbols exported by the
    non-boot files in `asm/src/` (excluding `crt0.s`, `intr.s`,
    `stack_check.s`, `veneer.s`, which the SDL platform layer subsumes).
    A `Port_AsmStubCount()` anchor + `sPortAsmStubTable[]` keep the
    symbols alive against `--gc-sections` so the linker can resolve
    callers from `src/` as those files start being added in 2b.2.
  - [x] **2b.2** Added the `TMC_LINK_GAME_SOURCES` CMake option
    (default OFF). A `tmc_game_sources` static library is always built
    as a dependency of `tmc_sdl` (so the leaf set is compile-checked on
    every PR) from the subset of `src/**/*.c` that already compiles
    cleanly under `__PORT__` with the SDL build's flags: originally
    `droptables.c`, `enemy.c`, `flagDebug.c`, `manager.c`,
    `npcDefinitions.c`, `npcFunctions.c`, `object.c`,
    `objectDefinitions.c`, `playerHitbox.c`, `playerItemDefinitions.c`,
    `projectile.c`, `sineTable.c` (all of them pure const data / function-
    pointer dispatch tables, so no struct-layout assumptions are in play).
    Whether the library is also **linked** into `tmc_sdl` is gated by
    `TMC_LINK_GAME_SOURCES` (see 2b.4a). New leaves are added by appending
    to `TMC_GAME_LEAF_SOURCES` in `CMakeLists.txt`.
  - [x] **2b.3** (in progress) Iteratively bring in the remaining game TUs,
    fixing any new agbcc-isms behind `#ifdef __PORT__` in the GBA headers.
    - Wave 1: neutralised the dominant blocker — `static_assert(sizeof(X)
      == ...)` on GBA struct sizes — by collapsing `static_assert` to a
      forward-declared unused `struct` tag under `__PORT__` in
      `include/global.h`. These assertions encode GBA hardware layout (4-
      byte pointers) that doesn't hold on a 64-bit host; the matching ROM
      build keeps the real check. With that one header tweak, 46
      additional `src/**/*.c` TUs compile cleanly under `__PORT__`, and
      they've been folded into `TMC_GAME_LEAF_SOURCES`: `beanstalkSubtask`,
      `code_08049CD4`, `code_08049DF4`, `collision`, `color`, `debug`,
      `demo`, `enemyUtils`, `enterPortalSubtask`, `entity`, `fade`,
      `fileselect`, `flags`, `game`, `gameData`, `gameOverTask`,
      `gameUtils`, `interrupts`, `item`, `itemDefinitions`, `itemMetaData`,
      `itemUtils`, `kinstone`, `main`, `message`, `movement`, `npc`,
      `objectUtils`, `physics`, `player`, `playerItem`, `playerItemUtils`,
      `playerUtils`, `projectileUtils`, `roomInit`, `save`,
      `screenTileMap`, `script`, `scroll`, `sound`, `staffroll`,
      `subtask`, `text`, `title`, `ui`, `vram`.
    - Still blocked on file-specific fixes (deferred to subsequent 2b.3
      waves): (none — wave 3 cleared the remaining `src/*.c` leaves).
    - Wave 2: rewrote five small agbcc-isms behind `#ifdef __PORT__` so
      the matching ROM build still emits the original instruction
      sequence, and added the files to `TMC_GAME_LEAF_SOURCES`. Four of
      them (`affine.c`, `code_0805EC04.c`, `eeprom.c`, `npcUtils.c`)
      used GCC's removed cast-as-lvalue extension (`(u8*)p += n;` /
      `(u16*)ent->child = a2 + 1;`); the host build now writes the
      explicit `p = (T*)((u8*)p + n);` form. The fifth (`room.c`) had
      an `asm("" ::: "r5")` register-clobber hint to nudge agbcc's
      regalloc, which the host build skips.
    - Wave 3: cleared the three remaining top-level `src/*.c` blockers
      (`backgroundAnimations.c`, `common.c`, `cutscene.c`).
      `assets/gfx_offsets.h` and `assets/map_offsets.h` are generated
      by the ROM-build `asset_processor` from the extracted base ROM
      and are therefore unavailable in the SDL build; a new
      `tools/port/gen_offset_stubs.py` walks every `.c`/`.h` that
      pulls in either header, collects the 1,293 unique `offset_*`
      identifiers it references, and emits host-port stubs under
      `src/platform/shared/generated/assets/` that collapse each one
      to `0` (the SDL port does not yet dereference the underlying
      asset blobs). The CMake build adds `src/platform/shared/generated/`
      to the include path for `tmc_game_sources` only, leaving the
      real `assets/` directory untouched. `cutscene.c` stores 25
      script-function addresses in the `EntityData::spritePtr` slot
      via `(u32)&script_X`, which truncates on a 64-bit host and stops
      being a valid constant initializer; the field is widened to
      `uintptr_t` under `__PORT__` in `include/room.h`, and a new
      `PORT_ROM_PTR(x)` macro in `include/global.h` expands to the
      matching `(u32)` cast under the ROM build and `(uintptr_t)` under
      the SDL build. `cutscene.c`'s initializers use `PORT_ROM_PTR(...)`
      in both branches. The CI matrix's
      `TMC_LINK_GAME_SOURCES=ON` build now compiles every top-level
      `src/*.c`.
  - [x] **2b.4a** Foundation for replacing `agb_main_stub.c` with the
    real `src/main.c::AgbMain`. The `TMC_LINK_GAME_SOURCES` option
    additionally **links** the `tmc_game_sources` library into the
    `tmc_sdl` executable (replacing the stub) instead of merely building
    it — and the stub TU is opt-out: it only enters the executable
    when `TMC_LINK_GAME_SOURCES=OFF`. Three pieces of host-side
    infrastructure make the link possible for the call graph rooted at
    `AgbMain`:
    1. `src/platform/shared/port_globals.c` allocates host BSS for the
       linker-script globals that `linker.ld` would otherwise fix at
       hard-coded EWRAM/IWRAM offsets — `gMain`, `gInput`, `gScreen`,
       `gRoomControls`, `struct_02000010 gUnk_02000010`, `gRand`,
       `gMessage`, `gTextRender`, `gNewWindow` / `gCurrentWindow` (with
       the file-local `Window` struct from `src/message.c` mirrored
       here), `gPaletteBuffer[256]`, and `u8 gUnk_03003DE4[0xC]`. The
       struct sizes do not match the ROM build's
       `static_assert(sizeof(X) == ...)` (those are no-ops under
       `__PORT__` per 2b.3 wave 1), and that's fine — the host owns the
       layout because pointers are 8 bytes.
    2. `bios.c` and `interrupts.c` now define unprefixed
       `RegisterRamReset`, `SoftReset`, `SoundBiasReset`,
       `SoundBiasSet`, and `VBlankIntrWait` under `#ifdef __PORT__`,
       forwarding to the existing `Port_*` implementations. On the GBA
       these resolve via `asm/lib/libagbsyscall.s`; on the host that
       file is unbuilt, so the unprefixed names need real definitions.
    3. `src/main.c::InitOverlays` had a small `#ifndef __PORT__` patch
       wrapped around the GBA-only EWRAM clear and the
       `RAMFUNCS_END`/`sub_080B197C` / `gCopyToEndOfEwram_*` /
       `gEndOfEwram` ROM-to-RAM relocation block. Those symbols are
       linker-script artefacts; rather than fabricate matching weak
       aliases we skip the whole sequence on the host (no overlays, no
       EWRAM-tail clobber). The matching ROM build is byte-identical.

    The `Port_HeadersSelfCheck()` runtime check has moved from the
    dropped stub into `src/platform/sdl/main.c` so it still runs during
    the headless smoke test.

    Behaviour with `TMC_LINK_GAME_SOURCES=OFF` is preserved as an
    early-bring-up scaffold for future ports and as a platform-layer
    isolation harness: `tmc_sdl` builds from the empty-loop
    `agb_main_stub.c` and the smoke test passes. Both build flavours
    are exercised by the Ubuntu CI job; the leaf set is also still
    compile-checked on every PR by
    `add_dependencies(tmc_sdl tmc_game_sources)`.
  - [x] **2b.4b** (link only) Widened `TMC_GAME_LEAF_SOURCES` to the
    entire `src/**/*.c` tree (618 TUs). The 13 files that did not
    compile under `__PORT__` with the wave-3 header fixes got small
    file-local patches behind `#ifdef __PORT__`:
    `src/worldEvent/worldEvent{12,13,14,16,17,19,20,21,22,25}.c` now
    use the existing `PORT_ROM_PTR(&script_X)` macro (same treatment
    `cutscene.c` got in wave 3) so the 64-bit-host script addresses fit
    the widened `EntityData::spritePtr` slot;
    `src/manager/diggingCaveEntranceManager.c` and
    `src/playerItem/playerItemGust.c` dropped the `static` qualifier
    on file-local const tables that a header had forward-declared
    non-static (agbcc tolerated the mismatch; clang/gcc don't);
    `src/menu/kinstoneMenu.c` rewrote the one `--(s16)u16_lvalue`
    cast-as-lvalue to an explicit sign-extending temp-then-store (same
    pattern wave 2 used elsewhere). With the full tree in, the game
    library still pulled in ~850 symbols from outside the host build's
    reach (raw ARM asm in `asm/src/`, the unbuilt `src/gba/m4a.c`, and
    the large const tables under `data/`). A new
    `src/platform/shared/port_unresolved_stubs.c` (machine-generated
    from the ld error log) provides weak placeholders for every one of
    them: function-like names (`sub_*`, `m4a*`, `Get*`, `Set*`,
    `Update*`, `Clone*`, `Clear*`, `ram_*`) get `abort()` trap stubs
    that print the offending symbol; everything else gets 256 B of
    weak, 16-B-aligned BSS which satisfies both data references and
    indirect jumps (the latter SIGSEGV immediately because BSS is not
    executable on any supported host — the same "loud abort" contract
    `asm_stubs.c` uses). `bios.c` grew unprefixed aliases for
    `LZ77UnCompVram`, `LZ77UnCompWram`, `CpuSet`, `CpuFastSet`, `Sqrt`,
    `Div`, `Mod`, `BgAffineSet`, `ObjAffineSet`, `RLUnCompWram`,
    `RLUnCompVram`, `ArcTan2` so the real host implementations replace
    the weak stubs. With this in place, `cmake -DTMC_LINK_GAME_SOURCES=ON`
    builds a `tmc_sdl` binary that **links cleanly** from the real
    `src/main.c::AgbMain`; the Ubuntu CI now runs both a default
    (`OFF`) build+smoke test and a second `=ON` build pass that
    verifies the link.
  - [x] **2b.4b** (runtime flip) Flipped `TMC_LINK_GAME_SOURCES` to ON
    by default. The weak stubs in
    `src/platform/shared/port_unresolved_stubs.c`, the targeted
    silent overrides in `src/platform/shared/ram_silent_stubs.c`, the
    contiguous entity-arena layout in `src/platform/shared/port_globals.c`,
    and the `Port_RunGameLoop()` / `Port_VBlankIntrWait()` plumbing
    in `src/platform/shared/interrupts.c` (all itemised in the
    "Progress so far" notes below) cumulatively let `AgbMain` survive
    the headless `--frames=N` smoke test budget for
    `N ∈ {30, 60, 120, 240, 600, 1200}` and reach the title-screen
    idle. The Ubuntu CI now runs the default (ON) build's smoke test
    and golden-hash check, plus an explicit `=OFF` configure / build
    / smoke / golden-hash check so the empty-loop scaffold doesn't
    bit-rot. Continuing past the title-screen idle (file-select /
    game-task transitions) needs additional unported `ram_*` helpers,
    manager dispatchers, and EWRAM-layout work, all tracked under the
    later roadmap entries (PR #7 m4a + the asm-decomp track) rather
    than under this checkbox.

    *Progress so far (incremental landings under this checkbox):*
    The `--frames=N` headless smoke test against
    `cmake -DTMC_LINK_GAME_SOURCES=ON` now exits cleanly: `AgbMain`
    runs through `HandleNintendoCapcomLogos`, lands in
    `HandleTitlescreen`, advances its internal state machine past
    `EraseAllEntities` / `UpdateEntities`, and reaches the title-screen
    animation before the host pacer unwinds it. Specifically:
    * Added `PORT_HW_ADDR(addr)` in `include/gba/defines.h` -- a host
      translator from a literal GBA hardware address (`0x07000000`,
      `0x06000000`, ...) to the matching offset inside the emulated
      `gPort{Vram,Oam,Pltt,Ewram,Iwram,Io}` array. Routed
      `src/common.c::ClearOAM` / `DispReset` through it (the very first
      `*(u16*)0x07000000 = 0x2A0` was the original SIGSEGV).
    * Under `__PORT__`, redefined the `Dma{Set,Copy,Fill,Stop,Wait}`
      macros in `include/gba/macro.h` to forward into the existing
      synchronous `Port_Dma*` host helpers in
      `src/platform/shared/dma.c`. The unmodified macros poke
      memory-mapped DMA registers and busy-wait for the controller to
      clear `DMA_ENABLE`; on the host that bit never clears and
      `InitDMA()` would spin forever.
    * Added unprefixed silent `m4a*` stubs (`m4aSoundInit`,
      `m4aMPlayAllStop`, ...) to `src/platform/shared/m4a_stub.c` so
      `InitSound()` no longer SIGABRTs on the weak `Port_UnresolvedTrap`.
      The real m4a engine is PR #7.
    * Neutralized `src/eeprom.c::DMA3Transfer` and the bare
      `REG_EEPROM` poll under `__PORT__` (EEPROM-backed save persistence
      is PR #6) so `InitSaveData` runs to completion against an
      empty-EEPROM model.
    * Rewired `gSaveHeader` in `include/save.h` to point at the start
      of the host EWRAM array under `__PORT__` (the literal
      `0x02000000` pointer SIGSEGV'd inside `CheckHeaderValid()`).
    * Wired the host `VBlankIntrWait` in
      `src/platform/shared/interrupts.c` to invoke the game's
      `VBlankIntr()` handler synchronously after pacing -- the GBA
      relies on the IRQ controller calling it, which sets
      `gMain.interruptFlag = 1` and lets `WaitForNextFrame()` return.
    * Added `src/platform/shared/ram_silent_stubs.c` to provide
      real silent overrides for `ram_*` ARM-assembly helpers reached
      during boot (currently just `ram_MakeFadeBuff256` for
      `FadeVBlank()`); the ROM build is unaffected because these TUs
      are `__PORT__`-only.
    * Added `src/platform/shared/port_rom_data_stubs.c` with strong
      host stand-ins for the gfx-table data symbols `gGfxGroups[]`
      (133 entries, all pointing at a single shared `GfxItem` whose
      control byte is `0x0D` so `LoadGfxGroup` returns immediately
      instead of NULL-deref'ing the previous BSS placeholder),
      `gPaletteGroups[]` (208 entries, all pointing at a single
      `PaletteGroup` whose `numPalettes` high bit is clear so
      `LoadPaletteGroup` exits after one iteration; the iteration
      writes a 32-byte zero copy into palette 0, which is harmless),
      and a 4 KiB zero buffer for `gGlobalGfxAndPalettes[]`.
      `port_unresolved_stubs.c` no longer emits weak BSS for those
      three names. With this in place `AgbMain` advances past
      `HandleNintendoCapcomLogos` into `HandleTitlescreen`.
    * Added tailored silent overrides in
      `src/platform/shared/ram_silent_stubs.c` for the entity-related
      ARM-asm helpers reached from `HandleTitlescreen` --
      `ram_UpdateEntities`, `ram_ClearAndUpdateEntities`,
      `ram_DrawEntities`, `ram_DrawDirect`, `ram_sub_080ADA04`, and
      `ram_CollideAll`. Each is a no-op because the entity lists
      stay empty during the headless smoke test (no real spawners
      have been wired up yet); the matching tracking notes inside
      each stub spell out the removal contract (drop the host stub
      in the same commit that lands the C decomp of the ARM source).
    * Promoted the entity arena to strong, contiguous host BSS in
      `src/platform/shared/port_globals.c`. The GBA build relies on
      `gPlayerEntity` / `gAuxPlayerEntities` / `gEntities` being
      laid out adjacently in EWRAM (via `linker.ld`); engine code
      such as `entity.c::EraseAllEntities` walks
      `MemClear(&gPlayerEntity, 10880)` straight off the player
      struct into the auxiliary and pooled entities. With the prior
      256-byte-per-symbol weak placeholders that memset overflowed
      into unrelated globals -- notably wiping `gIntroState` every
      frame so `HandleTitlescreen` never advanced past its `case 0:`.
      The fix is a single `port_entity_arena` struct (player + aux +
      ents) plus `__asm__ .set` aliases that publish the three
      engine names at their matching offsets, with `_Static_assert`s
      pinning the host-side `sizeof(PlayerEntity)` /
      `sizeof(GenericEntity)` literals.
    * Added a strong host definition for `gEntityLists` /
      `gEntityListsBackup` (also in `port_globals.c`); their
      sentinel-self-loop pattern `head->first == head->last == head`
      is now installed by `Port_GlobalsInit()`, called during SDL
      startup from `src/platform/sdl/main.c`, before
      `entity.c::DeleteAllEntities` can make its first traversal.
      The matching ROM build relies on `entity.c::sub_0805E98C` to
      install that pattern, but its first invocation happens
      *inside* `EraseAllEntities` *after* `DeleteAllEntities`, so on
      the host (whose BSS comes up zero-filled instead of pre-init'd
      by the boot ROM image) the very first `DeleteAllEntities`
      would dereference a NULL list head.
    * Added `Port_RunGameLoop()` in
      `src/platform/shared/interrupts.c` -- a `setjmp`/`longjmp`
      wrapper around the entry function so `Port_VBlankIntrWait` can
      bail out of the game's infinite loop when shutdown (or the
      `--frames=N` budget) is requested. The real `src/main.c::AgbMain`
      is `while (TRUE)` with no `Port_ShouldQuit` polling (the
      stubbed `agb_main_stub.c` polled it explicitly), so without
      this hook the smoke-test budget would tick down to zero but
      the binary would never reach `Port_SaveFlush()` /
      `Port_VideoShutdown()`. `src/platform/sdl/main.c` now invokes
      `AgbMain` through this wrapper.

    With the above, `tmc_sdl --frames={30,60,120,240}` against the
    `=ON` build all complete within their budgets and exit `0`.

    The next blocker is the next host-runtime divergence past the
    title-screen idle: continuing to advance the `--frames=N` budget
    will eventually reach the file-select / game-task transitions
    where additional unported `ram_*` helpers, manager dispatchers,
    and EWRAM-layout assumptions await -- tracked under the relevant
    later roadmap entries (PR #5 affine BGs / windows, PR #6 EEPROM
    save persistence, PR #7 m4a, asm-decomp track) rather than
    landing under this checkbox.
- [x] **PR #3.** `Port_InputPump()` now writes `~mask & 0x3FF` into the
  emulated `REG_KEYINPUT` slot (`gPortIo + 0x130`) every frame, and
  `Port_InputInit()` primes the slot to `0x3FF` (no keys pressed) before
  any game code can sample it. The existing
  `src/common.c::ReadKeyInput()` works unchanged on the host because
  `REG_KEYINPUT` is rewired to that same byte slot under `__PORT__` (PR
  #2a). `Port_HeadersSelfCheck()` was extended with a round-trip through
  the rewired `REG_KEYINPUT` macro so any future change that desyncs the
  repeated `PORT_REG_OFFSET_KEYINPUT` constant in `input.c` from
  `REG_OFFSET_KEYINPUT` in `include/gba/io_reg.h` fails the headless
  smoke test in CI.
- [x] **PR #4.** Software rasterizer for BG mode 0 (4 text BGs) and OBJ
  layer (regular sprites, 4 bpp + 8 bpp). Added a cross-port renderer in
  `src/platform/shared/render.c` plus a public entry point
  `Port_RenderFrame()` (declared in `include/platform/port.h`) that
  reads exclusively from the emulated `gPortIo` / `gPortVram` /
  `gPortPltt` / `gPortOam` arrays and writes a packed 240x160 ARGB8888
  framebuffer. `src/platform/sdl/video.c::Port_VideoPresent()` calls
  the renderer instead of the previous opaque-black fill, so future
  ports (PSP, PS2) can reuse the renderer verbatim.
  Implemented:
  * `DISPCNT` mode + bg/obj enable bits + forced-blank (white screen).
  * BG mode 0 -- all four text BGs, each with `BGxCNT` priority,
    char base / screen base, 16-color (4 bpp) and 256-color (8 bpp),
    and the four screen-size codes (256x256 / 512x256 / 256x512 /
    512x512 with the standard SC0..SC3 sub-screen wrapping); per-axis
    `BGxHOFS` / `BGxVOFS` scrolling; tilemap entries decode tile id
    (10 bits), hflip, vflip, palette bank (4 bpp only).
  * OBJ layer -- regular (non-affine) sprites in 4 bpp and 8 bpp,
    1D and 2D OBJ tile mapping (DISPCNT bit 6), all 12 shape x size
    combinations (8x8 .. 64x64), hflip / vflip, attr0 disable bit,
    correct GBA priority composition (OBJ over BG of equal priority;
    lower BG number / lower OAM index wins within an equal-priority
    tier; OBJ wraps at the GBA's 9-bit signed X / 8-bit unsigned Y).
  * BGR555 -> ARGB8888 conversion uses 5-bit replication so palette
    entry 0x7FFF expands exactly to 0xFFFFFFFF.

  A new `Port_RendererSelfCheck()` programs a known tilemap +
  sprite into the emulated memory regions and verifies the produced
  pixels for backdrop fill, BG tile sampling, hflip / vflip, palette
  bank selection, transparency vs. palette colour 0, BG scroll, OBJ
  drawing, attr0 disable, and BG-vs-OBJ priority resolution. The
  Ubuntu CI smoke test (`--frames=30`) now runs both
  `Port_HeadersSelfCheck()` and `Port_RendererSelfCheck()` before
  entering the frame loop, so any regression in the rasterizer that
  is observable through the public memory-region contract fails CI
  with a clear message.

  Out of scope (deferred to PR #5+ as planned): affine BGs (modes
  1/2 affine layers), bitmap modes 3/4/5, windows 0/1/OBJ/outside,
  BLDCNT/BLDALPHA/BLDY blending and brightness fade, mosaic, OBJ
  semi-transparent / OBJ-window modes. The renderer falls through
  cleanly for those (affine sprites are skipped; bitmap modes draw
  the backdrop) so the screen stays in a defined state until those
  features land.
- [x] **PR #5.** Affine BGs (modes 1/2), affine sprites,
  windows 0/1/OBJ/outside, alpha blending (BLDCNT/BLDALPHA/BLDY),
  brightness fade, mosaic. Implemented in
  `src/platform/shared/render.c` on top of the PR #4 layered
  scanline pipeline:
  * **Affine BGs.** `render_affine_bg_scanline()` reads BG2/3 PA, PB,
    PC, PD (s8.8) and BG2/3 X, Y (s19.8) per scanline and walks the
    transformed sample point across the line. The four affine
    screen-size codes (128 / 256 / 512 / 1024 px square) and the
    BGCNT bit 13 wrap-vs-transparent behaviour are honoured. Each
    affine tilemap entry is a single byte (8 bpp tile id, no flip /
    palette bank — matching real hardware). Mode 1 routes BG2 here
    while BG0 / BG1 stay in the text path; mode 2 routes BG2 + BG3.
  * **Affine sprites.** OAM attr0 AFFINE bit takes the sprite through
    `obj_read_affine_params()` (which decodes the 5-bit affine-group
    index in attr1[9..13] to the four PA/PB/PC/PD slots interleaved
    across an OAM rotation entry) and the centred-affine 2x2
    transform. The DOUBLE_SIZE flag doubles the on-screen bounding
    box (`box_w = 2*sw`, `box_h = 2*sh`) so the rotated sprite has
    room to fit; texture sampling still uses `sw x sh`.
  * **Windows.** `compute_window_mask()` produces a per-pixel
    layer-enable byte (BG0..BG3 / OBJ / colour-special-effect).
    Priority is WIN0 > WIN1 > OBJ window > outside, matching the GBA.
    OBJ-window source pixels come from sprites in attr0 mode 2:
    those sprites do not paint colour, only an OBJ-window mask
    consumed by `WINOUT[15..8]`.
  * **Blending.** Per pixel the compositor finds the topmost opaque
    layer (`find_top_layer()`), and — when alpha is selected via
    BLDCNT mode 1 with the layer in the 1st-target mask, *or* the
    layer is OBJ semi-transparent (attr0 mode 1) — searches again
    for the layer immediately below (`start_layer` parameter) to use
    as the 2nd target. `blend_alpha()` uses the GBA's
    `min(31, (top*EVA + bot*EVB) >> 4)` per channel. Brightness fade
    up / down (modes 2 / 3) uses BLDY via `blend_brighter()` and
    `blend_darker()`. The compositor never blends the backdrop into
    itself and never applies fades to OBJ pixels that are
    semi-transparent.
  * **Mosaic.** `mosaic_sizes()` decodes the MOSAIC register; BGs
    with BGCNT bit 6 set sample at `((y / Mv) * Mv)` and run a
    horizontal post-pass that snaps each pixel to its mosaic block's
    leftmost on-screen column. OBJ mosaic is applied per-sprite by
    snapping both the sampled row (`y -> (y / Mv) * Mv`) and the
    sampled column (`screen_x -> (screen_x / Mh) * Mh`).

  `Port_RendererSelfCheck()` grew six new test groups (affine BG with
  wrap on / off, WIN0 rectangle masking, alpha blend with EVA = EVB
  = 8, BLDY fade-to-white at EVY = 16, BLDY fade-to-black at EVY =
  16, OBJ mode-1 forcing alpha, BG horizontal mosaic at H = 4, and
  an identity-affine OBJ that must match a regular OBJ pixel-for-
  pixel) so any regression surfaces in CI's headless `--frames=30`
  smoke test.

  Out of scope (deferred to PR #9 stretch): bitmap modes 3 / 4 / 5
  and the BG2/3 internal-counter latching that the PPU performs
  every scanline based on PB / PD (the renderer resamples BG2X /
  BG2Y from I/O on every scanline, so games that lean on the
  per-line counter advance see a flat affine instead of a per-line
  one — the decomp does not appear to use that effect).
- [x] **PR #6.** Wire `Port_SaveReadByte` / `Port_SaveWriteByte` into
  `src/eeprom.c` so the file-select screen persists across runs. Under
  `__PORT__`, `EEPROMRead` and `EEPROMWrite` skip the GBA bit-stream
  protocol entirely and read/write the 8-byte payload directly against
  the `tmc.sav`-backed buffer in `src/platform/shared/save_file.c`
  (using EEPROM-row addresses scaled by 8, which mirrors the
  `address /= 8` rescale in `src/save.c::DataRead` / `DataWrite` /
  `DataCompare`). Each successful `EEPROMWrite` calls `Port_SaveFlush`
  so saves land on disk without requiring a clean shutdown. The
  short-circuit makes the previous boot-time neutralisations
  unnecessary: the `__PORT__` `DMA3Transfer` body is reduced to a
  no-op safety net (it is no longer reached on the host) and the
  `REG_EEPROM` host stub (the constant `1` that satisfied the
  busy-wait) is gone — the host `EEPROMWrite` returns from its
  short-circuit before ever sampling the status register or
  `REG_VCOUNT`. Flash-backed save (the `include/gba/flash_internal.h`
  routines) is not used by TMC: the game configures EEPROM via
  `EEPROMConfigure(0x40)` for an 8 KiB chip, and the entire save
  surface fits inside the existing 64 KiB `PORT_SAVE_SIZE` buffer.
  The matching ROM build is unchanged. Verified by an off-tree
  round-trip harness: `EEPROMConfigure(0x40)` → `EEPROMWrite(addr,
  buf, 0)` → `EEPROMRead(addr, out)` round-trips byte-for-byte;
  `Port_SaveLoad` reload from the freshly-written `tmc.sav` returns
  the same payload; out-of-range addresses still return
  `EEPROM_OUT_OF_RANGE`.
- [ ] **PR #7.** Bring `src/gba/m4a.c` into the SDL build, hook the
  emulated sound FIFOs into `src/platform/sdl/audio.c` so songs play.
- [x] **PR #8.** Golden-image CI test: snapshot the
  rasterizer's framebuffer at the end of `--frames=N` and assert the
  result against a stored hash. `src/platform/sdl/main.c` grew two
  CLI options:
  * `--print-frame-hash` renders one extra frame after the
    `--frames=N` budget unwinds `AgbMain`, hashes the resulting
    240×160 ARGB8888 buffer with FNV-1a 64-bit, and prints the value
    to stdout as `frame-hash: 0x<16 hex>`. The hash is computed over
    the pixels in an explicit ARGB byte order rather than the native
    in-memory `uint32_t` layout, so it is expected to be stable across
    little- and big-endian hosts.
  * `--screenshot=PATH` writes the same buffer as a PPM (P6) so
    rendering regressions are debuggable locally without rebuilding.

  Both the `=OFF` build (empty `agb_main_stub.c` loop) and the
  default `=ON` build (real `src/main.c::AgbMain` reaching the
  title-screen idle) have their
  `--frames=30` hashes pinned in
  `src/platform/shared/golden/usa_off_frames30.txt` and
  `src/platform/shared/golden/usa_on_frames30.txt` respectively, and
  the Ubuntu CI now asserts strict equality against both files on
  every run (any rasterizer regression observable through either
  surface fails with a clear actual-vs-expected diff). The ON-build
  hash has been verified stable across `--frames={30, 60, 120, 240,
  600, 1200}`, so locking it in catches both rasterizer regressions
  and any boot-time game-state divergences in the same check; the
  CI step also keeps the two-run determinism diagnostic so any
  host-side non-determinism still surfaces with a specific error.

  See `src/platform/shared/golden/README.md` for how to regenerate
  a hash and the policy for when it is and is not appropriate to
  do so.
- [ ] **PR #9.** (Stretch) Threaded renderer; widescreen mode under
  `TMC_WIDESCREEN`; Win32-OpenGL backend; PSP / PS2 ports following
  the same `src/platform/<name>/` pattern.

## Coexistence with the GBA ROM build

The SDL port deliberately does not touch `Makefile`, `GBA.mk`,
`Toolchain.mk`, `linker.ld`, the `tools/`, `assets/`, `data/`, or
`asm/` directories. `make tmc` continues to produce the matching ROM
that hashes against `tmc.sha1`, and Jenkins continues to verify it.

## Risks / known caveats

- **Incomplete decomp.** ~6,300 lines of ARM assembly remain in
  `asm/src/`. A handful (`crt0.s`, `intr.s`, `stack_check.s`, `veneer.s`)
  drop out on the host because they are pure GBA boot/interrupt glue.
  The rest (`enemy.s`, `script.s`, several `code_080xxxxx.s`) execute
  game logic and must either be decompiled to C first or stubbed out
  with explicit `assert(0)` so the unported paths fail loudly. PR #2
  picks one of those two strategies per file.
- **agbcc-isms.** `FORCE_REGISTER`, naked interrupt handlers, packed
  struct layout assumptions, and inline assembly in `include/asm.h` will
  not compile on clang / gcc unmodified. PR #2 conditionally
  replaces them with no-ops under `#ifdef __PORT__`.
- **Mid-scanline raster effects.** TMC may use HBlank-driven palette or
  scroll changes (`src/scroll.c`, `src/fade.c`). The PR #4 renderer
  starts per-frame; per-scanline support is added in PR #5 only if the
  game requires it.
- **Endianness.** The decomp targets little-endian ARM; building on a
  big-endian host is out of scope.
- **License compatibility.** Code copied from sa2 (MIT) or other pret
  projects must keep the original attribution headers; verify before
  vendoring.
