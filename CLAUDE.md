# CLAUDE.md

The Minish Cap decompilation plus a PC port (`PC_PORT`, `port/`). Engine code
under `src/` and `include/` is decompiled — match its style, and treat
unexplained literals as load-bearing until proven otherwise.

## Current work: viewport expansion (240×160 → 320×240)

**Milestone 1 (width) is signed off. Milestone 2 (height) is functionally
complete — every spike landed and all nineteen tracked bugs closed. The one
open item is a decision, not work: frame time is +41% over the canvas baseline
and peak frames exceed the 16.67 ms deadline. No go/no-go is recorded.**

There is also an **arm64 Android build** (`android/`), which is the same
viewport on other hardware and is played on an Ayaneo Pocket S 2K.

Read in this order:

1. `docs/milestone2-status.md` — where things stand, what is left, and the
   frame-time numbers the shipping decision rests on.
2. `docs/viewport-bug-tracker.md` — authoritative for behaviour. Nineteen
   bugs, the decisions taken, the screenblock-fallback sweep, and the lessons
   that cost the most to learn.
3. `tools/capture/README.md` — the capture/replay tooling and diagnostics.
4. `android/README.md` — the Android build, and how to drive the same
   capture/replay tooling on a device.
5. `docs/viewport-expansion-research-plan.md` — the original plan and the
   per-spike write-ups, a historical record.

The tracker wins wherever the plan disagrees with it; several spike write-ups
carry inline "superseded" notes pointing at later work.

**Four of this milestone's defects were live in the shipping 240×160 build or
through all of Milestone 1** — the expansion exposed them rather than causing
them. The regression gate proves the shipping build did not *move*; it cannot
prove it was right. When a change alters a mechanism rather than a surface,
count the frames that exercise the mechanism instead of reading a gate pass as
coverage. B16 and B17 both lived in code the canonical route never executes.

**When a bug needs a human at the controls, ask for a recording early.**
`record-bug.sh` found B13, B5, B15 and B16 in one pass each, after rounds of
inference found nothing.

**Three bugs — B5, B15, B17 — were one defect reported three times:** a world
layer loses its map source, falls back to the VRAM screenblock, and 32 tiles
cover 256 px, not 320. If a room renders as sprites over black above native
size, start from `TMC_REJECT_TRACE=1`, which names the rejection class in one
run. The sweep in the tracker enumerates the rest.

**A platform-only symptom is not a platform bug.** B16 reproduced on Android 2
runs in 3 and never on desktop, and six rounds went into what was different
about the device — all wrong. The engine was identical; one out-of-bounds read
of a four-entry table returned different padding per toolchain and let desktop
recover from a fault both platforms had. In decompiled code an out-of-range
index is defined on hardware (ROM is contiguous) and undefined here.

## Building

```bash
xmake f -c -y -m release && xmake build tmc_pc     # 240x160 build -> build/pc/tmc_pc
```

For 320x160 prefix **both** commands with `TMC_VIEW_W=320`; for 320x240 add
`TMC_VIEW_H=240` as well. The `-c` is required — a plain `xmake f` will not
drop a previously configured size, and the next build silently stays expanded.

Name builds WxH (240x160, 320x160, 320x240), never "the 240 build" — with two
axes in play a bare number no longer says which.

The **Android** build is separate and lives in `android/` (Gradle, arm64-v8a,
defaults to 320x240):

```bash
cd android && ./gradlew assembleDebug
```

It parses the source list out of `xmake.lua`'s `tmc_pc` target rather than
duplicating it, so an engine file added there joins that build too. See
`android/README.md` — including how to run capture scripts and read the port's
own traces on a device, which is what identified B16.

## Always refresh the playable build at the end of a work cycle

`build/play-320x240/` is the self-contained build the maintainer actually
plays, and it is how every bug in the tracker that needed a human at the
controls was found. **Rebuild and reinstall it before handing work back**, even
when the change looks headless — a fix that is only in `build/pc/tmc_pc` is a
fix nobody can playtest, and the maintainer has no way to tell the binary is
stale short of not seeing their bug fixed.

**Do this after the regression gate below, not before.** The gate needs a
240x160 build and this needs a 320x240 one, and `xmake f -c` drops the previous
size — running them the other way round means configuring twice and handing
over a play binary you have not gated.

```bash
TMC_VIEW_W=320 TMC_VIEW_H=240 xmake f -c -y -m release
TMC_VIEW_W=320 TMC_VIEW_H=240 xmake build -y tmc_pc
cd build/play-320x240
rm -f tmc_pc_320x240.prev            # older .prev is dropped, 45 MB apiece
mv tmc_pc_320x240 tmc_pc_320x240.prev
cp ../../build/pc/tmc_pc tmc_pc_320x240
```

Confirm what you installed rather than assuming the copy was the right binary:
`cmp` it against `build/pc/tmc_pc`, and replay a capture script through it from
a temp dir to check it renders what you verified.

Then update that directory's `README.md`: it is written to the playtester and
says what changed since the last binary, so a stale one is worse than none.
Move the previous cycle's notes down a section rather than deleting them.

Two ways to damage that directory, both already done once:

- **Never run anything from inside it.** The game overwrites `tmc.sav` as it
  plays, and that save is the maintainer's. Run captures from a `mktemp -d`
  with `baserom.gba` and `assets/` symlinked in, the way
  `tools/capture/run_route.sh` does.
- **Do not symlink `rom_data/` into such a harness.** The port extracts ROM
  pages into it on boot, and the write follows the link — one verification run
  left 2789 files and 11 MB behind. Let the temp dir keep its own.

`build/` is gitignored, so none of this appears in `git status` and no commit
will remind you. It is only ever as fresh as the last time someone did it.

**The APK is the same rule on the other platform, and it drifts further.** The
maintainer plays 320x240 on an Ayaneo as well as on the desktop, so a fix that
is only in `build/play-320x240/` is a fix half the playtesting cannot see.
Rebuild it in the same cycle:

```bash
cd android && ./gradlew assembleDebug     # APK in app/build/outputs/apk/debug/
```

Confirm the fix reached the artifact rather than the intermediate — the APK
carries a *stripped* `libmain.so`, so read that one:

```bash
unzip -p android/app/build/outputs/apk/debug/app-debug.apk lib/arm64-v8a/libmain.so > /tmp/libmain.so
```

and disassemble the function you changed with the NDK's `llvm-objdump`. This is
the same "confirm what you installed" step the desktop copy gets, and it is
cheap: the Gradle build reuses its native cache and takes well under a minute
when only a few files changed.

`./gradlew installDebug` puts it on a connected device; with none attached the
APK is all that can be produced here, and someone has to install it. Say so
when handing back, because nothing else will.

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
