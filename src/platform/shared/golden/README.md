# Golden-image hashes for the SDL port (PR #8)

This directory stores the expected FNV-1a 64-bit hashes of the
`tmc_sdl` rasterizer's framebuffer at well-defined frame counts.
The hashes are produced by `tmc_sdl --frames=N --print-frame-hash`
(see `src/platform/sdl/main.c`); the CI golden-image step in
`.github/workflows/sdl.yml` runs the binary and asserts the printed
hash matches the value in the matching `*.txt` file here.

Each `*.txt` file contains a single line of the form

    frame-hash: 0x<16 hex digits>

— bit-for-bit what `--print-frame-hash` writes to stdout. In CI, the
workflow extracts the `frame-hash:` line from the binary's stdout and
compares the resulting `actual` value against the matching file's
`expected` contents in shell.

## Files

| File                              | Build configuration       | What it pins                                                           |
|-----------------------------------|---------------------------|------------------------------------------------------------------------|
| `usa_off_frames30.txt`            | `TMC_LINK_GAME_SOURCES=OFF`           | The empty `agb_main_stub.c` loop's framebuffer after 30 frames. |
| `usa_on_frames30.txt`             | `TMC_LINK_GAME_SOURCES=ON` (default)  | The real `src/main.c::AgbMain`'s framebuffer after 30 frames — i.e. the title-screen idle reached after `HandleNintendoCapcomLogos` -> `HandleTitlescreen`. The hash has been verified stable across `--frames={30, 60, 120, 240, 600, 1200}`, so locking it in catches both rasterizer regressions and boot-time game-state divergences. |

## Updating a hash

Hashes only change when the rasterizer's pixel output changes for a
given input. Legitimate reasons to update one include: a new
rasterizer feature whose default behaviour shifts pixels (e.g. a new
window mode), a fix to a long-standing rendering bug, or a host /
compiler change that should not be hidden by stale tests.

To regenerate one of the files:

```sh
# OFF build (default):
cmake --preset sdl-release
cmake --build --preset sdl-release
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    ./build/sdl-release/tmc_sdl --frames=30 --print-frame-hash \
    > src/platform/shared/golden/usa_off_frames30.txt

# ON build:
cmake -S . -B build/sdl-on -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DTMC_LINK_GAME_SOURCES=ON
cmake --build build/sdl-on
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    ./build/sdl-on/tmc_sdl --frames=30 --mute --print-frame-hash \
    > src/platform/shared/golden/usa_on_frames30.txt
```

Then commit the change with a message that explains why the hash
moved. The CI run will re-assert the new value.

## What is **not** pinned (yet)

- Bitmap modes (3/4/5): the rasterizer does not implement them yet
  (PR #9 stretch), so neither the OFF nor the ON build exercises
  them today.

See `docs/sdl_port.md` (PR #8) for the full roadmap context.
