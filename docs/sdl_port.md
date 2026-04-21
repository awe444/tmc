# SDL Port of The Minish Cap

This document describes the SDL host port of the TMC decompilation. It is the
companion to the top-level CMake build and lives alongside (not instead of)
the existing GBA ROM build (`make` / `GBA.mk`).

The port is modelled on the [SAT-R/sa2 SDL port](https://github.com/SAT-R/sa2),
which adds an `sa2.sdl` target on top of the matching ROM decompilation. The
goal here is the equivalent `tmc.sdl`.

> Status: **PR #1 of the roadmap is implemented, PR #2a (foundational
> `__PORT__` header rewiring) has landed, and PR #2b is feature-complete
> at the link level (2b.1, 2b.2, waves 1–3 of 2b.3, 2b.4a, and the
> link-resolution half of 2b.4b are done). Every `src/**/*.c` TU that
> the SDL port can sensibly consume now builds clean under `__PORT__`
> (618 of 618; the 13 files that needed file-local fixes were patched
> behind `#ifdef __PORT__`), and `tmc_game_sources` is the full game
> tree. A `TMC_LINK_GAME_SOURCES=ON` configuration now links cleanly
> into `tmc_sdl` — the ~850 symbols that still live only in unported
> `asm/src/`, the not-yet-linked `src/gba/m4a.c`, and the large const
> tables under `data/` are satisfied by weak abort/BSS stubs in
> `src/platform/shared/port_unresolved_stubs.c`. The runtime flip of
> the default to `ON` is intentionally deferred — `src/main.c::AgbMain`
> reaches enough unresolved stubs during early init that it SIGSEGVs,
> and fleshing those paths out is the scope of PRs #3 – #7.** The
> default build produces a `tmc_sdl` executable that opens a 240×160
> (scaled 4×) window, accepts keyboard and gamepad input (X-Input on
> Windows via `SDL_GameController`), opens a silent SDL audio device,
> and runs an empty 59.7274 Hz frame loop. The GBA decomp headers
> (`include/gba/*.h`, `include/global.h`) now compile under a host C
> compiler when `__PORT__` is defined, with `REG_*`, `BG_PLTT`,
> `OBJ_PLTT`, `BG_VRAM`, `OAM`, `EWRAM_START`, `IWRAM_START`,
> `INTR_CHECK` and `INTR_VECTOR` aliasing the host arrays in
> `src/platform/shared/gba_memory.c`, and the agbcc-isms (`EWRAM_DATA`,
> `IWRAM_DATA`, `NAKED`, `FORCE_REGISTER`, `MEMORY_BARRIER`, `ASM_FUNC`,
> `NONMATCH`, `SystemCall`, …) collapsing to no-ops. The real game
> logic **links cleanly** but does not yet **run** — closing that
> remaining gap is spread across PRs #3 – #7.


## Building

Prerequisites:

- CMake 3.16+
- A C compiler (GCC ≥ 9, Clang ≥ 10, or MSVC 2019+)
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

### Windows (MSVC + vcpkg)

```pwsh
vcpkg install sdl2:x64-windows
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
build\Release\tmc_sdl.exe
```

### Windows (MinGW / cross-compile from Linux)

```sh
sudo apt install mingw-w64 ninja-build
cmake --preset sdl-mingw      # downloads & static-links SDL2 via FetchContent
cmake --build --preset sdl-mingw
```

### CMake options

| Option                       | Default | Description                                              |
|------------------------------|---------|----------------------------------------------------------|
| `TMC_PORT_PLATFORM`          | `sdl`   | Reserved for future ports (`win32`, `psp`, `ps2`).       |
| `TMC_GAME_VERSION`           | `USA`   | One of `USA`, `EU`, `JP`, `DEMO_USA`, `DEMO_JP`.         |
| `TMC_ENABLE_AUDIO`           | `ON`    | Open the SDL audio device (silent until PR #9).          |
| `TMC_ENABLE_GAMEPAD`         | `ON`    | Initialise `SDL_GameController` for X-Input pads.        |
| `TMC_WIDESCREEN`             | `OFF`   | Reserve hooks for future widescreen renderer.            |
| `TMC_USE_FETCHCONTENT_SDL`   | `OFF`   | Build SDL2 from source via FetchContent if not on disk.  |
| `TMC_LINK_GAME_SOURCES`      | `OFF`   | Link the `src/**/*.c` leaves that build under `__PORT__` into `tmc_sdl` (sub-step 2b.4a). The library is always **built** as a dependency; this option controls whether it is also **linked**. Turning it ON requires the 2b.4b stubs for the symbols still reaching into `src/enemy/`, `src/manager/`, etc. |

## Running

```
tmc_sdl [options]
  --scale=N        Integer window scale (default 4 → 960×640).
  --fullscreen     Start in fullscreen-desktop mode.
  --mute           Disable audio output.
  --save-dir=PATH  Where to read/write tmc.sav (default: working dir).
  --frames=N       Run for N frames then exit (used by CI smoke tests).
```

## Controls

| GBA button | Keyboard       | Gamepad (`SDL_GameController` / XInput on Windows) |
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

The contract for adding a new port (e.g. `psp/`, `ps2/`, `win32/` later) is:

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
    real `src/main.c::AgbMain`. The `TMC_LINK_GAME_SOURCES` option now
    additionally **links** the `tmc_game_sources` library into the
    `tmc_sdl` executable (replacing the stub) instead of merely building
    it — and the stub TU is now opt-out: it only enters the executable
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

    Behaviour with `TMC_LINK_GAME_SOURCES=OFF` (the still-default) is
    unchanged: `tmc_sdl` builds from the empty-loop `agb_main_stub.c`,
    and the smoke test passes. The Ubuntu CI job now only runs the
    default build (the second `=ON` configure pass was redundant once
    the library was switched to "always built as a dependency"); the
    leaf set is still compile-checked on every PR by
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
  - [ ] **2b.4b** (runtime flip) Flip `TMC_LINK_GAME_SOURCES` to ON by
    default. Blocks on making `AgbMain` boot: the weak stubs above
    keep the link succeeding but trap on call, so the existing
    headless smoke test (`--frames=30`) would SIGSEGV once any unported
    path is exercised during init. The remaining work is distributed
    across the later roadmap entries (input in PR #3, software
    rasterizer in PR #4, m4a in PR #7) rather than landing as a
    single big step here; once those ports reduce the stub surface
    enough that `AgbMain` survives the smoke-test budget, the default
    can flip.
- [ ] **PR #3.** Have `Port_InputPump()` write `~mask & 0x3FF` into the
  emulated `REG_KEYINPUT` slot. The existing `src/common.c::ReadKeyInput`
  then works unchanged. Smoke test: title-screen menu navigation logged
  by the game responds to the keyboard.
- [ ] **PR #4.** Software rasterizer for BG mode 0 (4 text BGs) and OBJ
  layer (regular sprites, 4 bpp + 8 bpp). Reuse sa2's renderer as a
  structural reference (MIT-licensed; preserve attribution).
- [ ] **PR #5.** Affine BGs (modes 1/2), windows 0/1/OBJ/outside, alpha
  blending (BLDCNT/BLDALPHA/BLDY), brightness fade, mosaic.
- [ ] **PR #6.** Wire `Port_SaveReadByte` / `Port_SaveWriteByte` into
  `src/eeprom.c` and the Flash routines so the file-select screen
  persists across runs.
- [ ] **PR #7.** Bring `src/gba/m4a.c` into the SDL build, hook the
  emulated sound FIFOs into `src/platform/sdl/audio.c` so songs play.
- [ ] **PR #8.** Golden-image CI test: snapshot the title-screen
  framebuffer and assert against a stored hash.
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
  not compile on clang/gcc/MSVC unmodified. PR #2 conditionally
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
