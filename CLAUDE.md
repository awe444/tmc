# CLAUDE.md

The Minish Cap decompilation plus a PC port (`PC_PORT`, `port/`). Engine code
under `src/` and `include/` is decompiled — match its style, and treat
unexplained literals as load-bearing until proven otherwise.

## Current work: viewport expansion (240×160 → 320×240)

**Milestone 1 (width, 320×160) is complete and signed off. Milestone 2
(height, 320×240) is next.**

Read in this order:

1. `docs/viewport-bug-tracker.md` — authoritative for current state. Ten bugs,
   the decisions taken, the carry-forward list Milestone 2 inherits, and the
   lessons that cost the most to learn.
2. `docs/viewport-expansion-research-plan.md` §10.2 "State of the code entering
   Milestone 2" — how width was achieved and what to mirror vertically.
3. `tools/capture/README.md` — the capture/replay tooling and diagnostics.

The tracker wins wherever the plan disagrees with it; the plan's spike
write-ups are a historical record and several carry inline corrections.

## Building

```bash
xmake f -c -y -m release && xmake build tmc_pc     # 240 build -> build/pc/tmc_pc
```

For the wide build prefix **both** commands with `TMC_VIEW_W=320`. The `-c` is
required — a plain `xmake f` will not drop a previously configured width, and
the next build silently stays wide.

## Regression gate — run before any viewport commit

At the **default 240 build**, both must hold:

- canonical route: 11/11 waypoints pixel-identical
- map-source audit: 0 mismatches in 265,497,600 fetches

Exact commands in `tools/capture/README.md` ("Regression gate"). Both have
caught real regressions, including a change intended for the wide build that
altered what the shipping build renders. The 240 build is the shipping build.

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
