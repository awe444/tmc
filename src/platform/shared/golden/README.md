# Golden-image hashes for the SDL port (PR #8)

This directory stores the expected FNV-1a 64-bit hashes of the
`tmc_sdl` rasterizer's framebuffer at well-defined frame counts.
The hashes are produced by `tmc_sdl --frames=N --print-frame-hash`
(see `src/platform/sdl/main.c`); the CI golden-image step in
`.github/workflows/sdl.yml` runs the binary and asserts the printed
hash matches the value in the matching `*.txt` file here.

Each `*.txt` file contains a single line of the form

    frame-hash: 0x<16 hex digits>

— bit-for-bit what `--print-frame-hash` writes to stdout. The CI
step therefore reduces to a `grep -F -x` against the binary's output,
which gives a useful diff in the failure message.

## Files

| File                              | Build configuration       | What it pins                                                           |
|-----------------------------------|---------------------------|------------------------------------------------------------------------|
| `usa_off_frames30.txt`            | `TMC_LINK_GAME_SOURCES=OFF` (default) | The empty `agb_main_stub.c` loop's framebuffer after 30 frames. |

## Updating a hash

Hashes only change when the rasterizer's pixel output changes for a
given input. Legitimate reasons to update one include: a new
rasterizer feature whose default behaviour shifts pixels (e.g. a new
window mode), a fix to a long-standing rendering bug, or a host /
compiler change that should not be hidden by stale tests.

To regenerate one of the files:

```sh
cmake --preset sdl-release
cmake --build --preset sdl-release
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    ./build/sdl-release/tmc_sdl --frames=30 --print-frame-hash \
    > src/platform/shared/golden/usa_off_frames30.txt
```

Then commit the change with a message that explains why the hash
moved. The CI run will re-assert the new value.

## What is **not** pinned (yet)

- The `TMC_LINK_GAME_SOURCES=ON` build's title-screen frame: the
  pixels are deterministic across runs (the CI's
  "golden-image-on-determinism" step asserts that two consecutive
  `--frames=30` runs hash to the same value), but the value itself
  still moves with every PR that fleshes out one of the
  `port_unresolved_stubs.c` weak placeholders. PR #5..#7 progress
  needs to settle before this is worth pinning.
- Bitmap modes (3/4/5): the rasterizer does not implement them yet
  (PR #9 stretch), so neither the OFF nor the ON build exercises
  them today.

See `docs/sdl_port.md` (PR #8) for the full roadmap context.
