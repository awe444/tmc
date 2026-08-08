# CLAUDE.md

The Minish Cap decompilation plus a PC port (`PC_PORT`, `port/`). Engine code
under `src/` and `include/` is decompiled — match its style, and treat
unexplained literals as load-bearing until proven otherwise.

## Current work: viewport expansion (240×160 → 320×240)

**Milestone 1 (width) is signed off. Milestone 2 (height) is functionally
complete — every spike landed and twenty-one of the twenty-four tracked bugs
are closed. Two are open, and both are decisions rather than work: frame time
is +41% over the canvas baseline with peak frames past the 16.67 ms deadline,
and B21's light shaft cannot reach the right edge without reallocating a BG
layer's screenbase. No go/no-go is recorded for either.**

There is also an **arm64 Android build** (`android/`), which is the same
viewport on other hardware and is played on an Ayaneo Pocket S 2K.

Read in this order:

1. `docs/milestone2-status.md` — where things stand, what is left, and the
   frame-time numbers the shipping decision rests on.
2. `docs/viewport-bug-tracker.md` — authoritative for behaviour. Twenty-four
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

**The faded room transition (VIEWPORT_SCROLL_FADE) defers the apply by 32
frames, and state that keys off "is a scroll in progress" reads *no* during
them.** B16 lost the player's facing that way; B24 lost a lily pad's whole
carry-across-the-scroll state, which exits on `reload_flags == 0` and so quit
28 frames before the room changed, leaving the pad and the player outside the
room. Anything that runs between a hand-off and the end of a scroll must be
checked against `ScrollTransitionIsPending()`. `minecart.c` has the same shape
and is still unexercised.

**B22 is the same assumption a fourth time, one axis over, and it broke
gameplay rather than rendering.** A room that is *exactly* viewport-sized on
hardware — 240x160 — pins the camera on the room origin, which makes
`scroll_y` and `origin_y` the same number and lets the engine spell a room
position either way. Above native size they differ by the centring offset. The
rolling barrel held the player 40 px off its own midline that way, so the doors
and the cobweb hole were out of reach. **In such a room, every camera-relative
expression is unverified code.** The width sweep did not catch this because it
asked about width; the vertical case has not been swept.

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

## Always refresh both playable builds at the end of a work cycle

`build/play-320x240/` and `build/play-240x160/` are the self-contained builds
the maintainer actually plays, and they are how every bug in the tracker that
needed a human at the controls was found. **Rebuild and reinstall both before
handing work back**, even when the change looks headless — a fix that is only in
`build/pc/tmc_pc` is a fix nobody can playtest, and the maintainer has no way to
tell a binary is stale short of not seeing their bug fixed.

The 240x160 one exists to answer *"is this the expansion's fault or was it
always like that?"* by hand. Four of Milestone 2's defects were live in the
shipping build all along and only looked new, and each cost rounds before that
was established. It is the first thing to ask of any new report.

**Order matters, and it makes the 240x160 copy free.** The gate below needs a
240x160 build and the other play build needs a 320x240 one, and `xmake f -c`
drops the previous size. Do the gate first, install its *exact binary* as the
240x160 play build, then configure 320x240 once:

```bash
# 1. gate (see below) at the default 240x160, then install that same binary
cd build/play-240x160
rm -f tmc_pc_240x160.prev            # older .prev is dropped, 45 MB apiece
mv tmc_pc_240x160 tmc_pc_240x160.prev 2>/dev/null || true
cp ../../build/pc/tmc_pc tmc_pc_240x160

# 2. then the expanded one
TMC_VIEW_W=320 TMC_VIEW_H=240 xmake f -c -y -m release
TMC_VIEW_W=320 TMC_VIEW_H=240 xmake build -y tmc_pc
cd build/play-320x240
rm -f tmc_pc_320x240.prev
mv tmc_pc_320x240 tmc_pc_320x240.prev
cp ../../build/pc/tmc_pc tmc_pc_320x240
```

Doing it in that order means the 240x160 play binary *is* the binary the gate
passed on, at no extra configure. Rebuilding it separately is not equivalent and
not free — and note a rebuild is no longer byte-reproducible anyway, because the
startup identity line embeds `__DATE__`/`__TIME__`; compare `.text` rather than
whole files if you need to prove two builds are the same code.

Neither play directory ships a `tmc.sav`. Do not create one: let the maintainer
copy their own in. Saves are viewport-independent, so the same file works in
both and moving one across is the fastest way to compare a scene at both sizes.

Confirm what you installed rather than assuming the copy was the right binary:
`cmp` each against `build/pc/tmc_pc` at the time you copy it, and replay a
capture script through it from a temp dir to check it renders what you verified.
The 240x160 one has a stronger check available and worth using — replay the
canonical route through the installed binary and diff against
`tools/capture/references/spike0-240x160`; it must be 11/11.

Then update those directories' `README.md`: they are written to the playtester
and say what changed since the last binary, so a stale one is worse than none.
Move the previous cycle's notes down a section rather than deleting them. The
240x160 README is about *why you would reach for it* rather than a per-cycle
changelog, so it usually needs nothing beyond the build date.

Two ways to damage these directories, both already done once:

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
