# port/patches — retired

The build no longer patches `libs/ViruaPPU`. The submodule tracks
[awe444/VirtuaPPU](https://github.com/awe444/VirtuaPPU), which carries every
port-side PPU change as a real commit:

| Change | Fork commit |
|---|---|
| HDMA per-line callback hook (`virtuappu_mode1_pre_line_callback`) | `5cf5e99`, `e69f60b` |
| Mosaic support (`MODE1_IO_MOSAIC`) | `e69f60b` |
| Sub-pixel affine OAM overlay (`virtuappu_mode1_render_affine_obj_overlay`) | `276c73a` |

**To change the PPU:** commit in `libs/ViruaPPU`, push to the fork, then commit
the submodule bump here. Do not add new patch files — the viewport-expansion
work (`docs/viewport-expansion-research-plan.md`, Spike 3) adds a BG mode far
larger than context diffs can carry, which is why the pipeline was retired.

`xmake.lua` keeps one cheap guard: if the checked-out submodule lacks the
`virtuappu_mode1_render_affine_obj_overlay` symbol it fails fast with a
"run git submodule update" message, instead of a confusing link error.

## Why one patch file remains

`viruappu-widescreen.patch` was **never applied** — it was not in the build's
patch table, and the `widescreen_width` option that nominally drove it was
never consumed (see the research plan §2.2). It is kept as design reference:
its per-scanline `io_snapshots` sketch is the starting point for Spike 9's
240-line HDMA work. It is documentation, not build input.
