# CLAUDE.md

The Minish Cap decompilation plus a PC port (`PC_PORT`, `port/`). Engine code
under `src/` and `include/` is decompiled — match its style, and treat
unexplained literals as load-bearing until proven otherwise.

## Current work: viewport expansion (240×160 → 320×240)

**Milestone 1 (width) is signed off. Milestone 2 (height) is functionally
complete — every spike landed and every reported bug fixed. The one open item
is a decision, not work: frame time is +41% over the canvas baseline and peak
frames exceed the 16.67 ms deadline. No go/no-go is recorded.**

Read in this order:

1. `docs/milestone2-status.md` — where things stand, what is left, and the
   frame-time numbers the shipping decision rests on.
2. `docs/viewport-bug-tracker.md` — authoritative for behaviour. Thirteen
   bugs, the decisions taken, and the lessons that cost the most to learn.
3. `tools/capture/README.md` — the capture/replay tooling and diagnostics.
4. `docs/viewport-expansion-research-plan.md` — the original plan and the
   per-spike write-ups, a historical record.

The tracker wins wherever the plan disagrees with it; several spike write-ups
carry inline "superseded" notes pointing at later work.

**Four of this milestone's defects were live in the shipping 240×160 build or
through all of Milestone 1** — the expansion exposed them rather than causing
them. The regression gate proves the shipping build did not *move*; it cannot
prove it was right. When a change alters a mechanism rather than a surface,
count the frames that exercise the mechanism instead of reading a gate pass as
coverage.

**When a bug needs a human at the controls, ask for a recording early.**
`record-bug.sh` found B13 in one pass after a round of inference found
nothing. B4 and B5 have been open since Milestone 1 for want of using it.

## Building

```bash
xmake f -c -y -m release && xmake build tmc_pc     # 240x160 build -> build/pc/tmc_pc
```

For 320x160 prefix **both** commands with `TMC_VIEW_W=320`; for 320x240 add
`TMC_VIEW_H=240` as well. The `-c` is required — a plain `xmake f` will not
drop a previously configured size, and the next build silently stays expanded.

Name builds WxH (240x160, 320x160, 320x240), never "the 240 build" — with two
axes in play a bare number no longer says which.

## Regression gate — run before any viewport commit

At the **default 240x160 build**, both must hold:

- canonical route: 11/11 waypoints pixel-identical
- map-source audit: 0 mismatches in 265,497,600 fetches

Exact commands in `tools/capture/README.md` ("Regression gate"). Both have
caught real regressions, including a change intended for an expanded build
that altered what the shipping build renders. 240x160 is the shipping build.

## Conventions

- **Do not commit extracted assets.** Everything in `assets/` is derived from
  `baserom.gba` except five hand-authored config files; `assets/.gitignore`
  has the allowlist and the reasoning.
- `baserom.gba`, `tmc.sav` and `config.json` are gitignored and must stay so.
- Verify a border by counting **distinct colours per column**, not by testing
  for black — a clipped UI screen's border is the PPU backdrop, which is not
  black. This has produced false results before.
- When a fix makes a measurement jump to its theoretical best, confirm the code
  you think produced it actually ran. That has produced false results too.
