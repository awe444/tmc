# Android build (arm64)

A barebones Gradle project that packages the PC port as an arm64-v8a APK.
Input is SDL gamepads only — no touch controls of any kind are compiled in.
The ROM and the extracted assets are baked into the APK at build time.

This is a fresh, minimal build; the upstream Android project was a private
submodule and none of it is here. The traces of it that survive in the engine
(`TMC_ANDROID_PORT`, `TMC_ANDROID_RUNTIME_DIR`) are **not** used by this build —
see "What this deliberately does not do" below.

**Status: played at 320x240 fullscreen on an Ayaneo Pocket S 2K (Android 13).**
Audio, video and controller input all work. No other hardware has been tried.

Two bugs have been reported from this build and fixed — B16 and B17 in
`docs/viewport-bug-tracker.md`. **Neither was a platform bug.** Both were
viewport defects that this build happened to expose, and B16 in particular cost
six rounds of investigating what was different about Android before the answer
turned out to be that an out-of-bounds read returned different padding per
toolchain and let desktop recover from a fault both platforms had. Treat a
device-only symptom here as a viewport bug until a trace says otherwise.

## Prerequisites

```bash
export JAVA_HOME=/snap/android-studio/235/jbr
export ANDROID_HOME=/home/awaszczak/Android/Sdk
git submodule update --init libs/SDL
```

Also needed, all of which a working desktop build already provides:

- `xmake` on `PATH` (or at `~/.local/bin/xmake`) — builds the asset extractor
- `baserom.gba` at the repository root
- fmt and nlohmann/json headers — found automatically in `~/.xmake/packages`
  or `/usr/include`; override with `-DTMC_FMT_INCLUDE=` / `-DTMC_JSON_INCLUDE=`

`local.properties` holds `sdk.dir` and is gitignored; `ANDROID_HOME` works in
its place.

## Building

```bash
cd android && ./gradlew assembleDebug
```

The APK lands in `app/build/outputs/apk/debug/`. To build and install onto a
connected device in one step:

```bash
cd android && ./gradlew installDebug
```

**Rebuild it in the same cycle as `build/play-320x240/`.** Nothing here is
checked in and nothing in `git status` will mention it, so the APK is only as
fresh as the last time someone ran the command — it had gone two fixes stale
once, which is how this note came to exist. When you do rebuild, check the
*packaged* library rather than the intermediate, since only the former is what
ships:

```bash
unzip -p app/build/outputs/apk/debug/app-debug.apk lib/arm64-v8a/libmain.so > /tmp/libmain.so
"$ANDROID_HOME"/ndk/*/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-objdump \
    -d /tmp/libmain.so | less    # find the function you changed
```

The APK's copy is stripped, so disassemble by address rather than by symbol.

Debug builds are signed with the standard debug key, so they install without
any further setup. Release builds are unsigned — wire up a `signingConfig` if
you want one.

## Configuration

Set these in `gradle.properties`, or pass them as `-P` flags:

| Property | Default | Meaning |
| --- | --- | --- |
| `tmc.viewWidth` | `320` | `MODE1_GBA_WIDTH` / `PORT_VIEW_CONTENT_WIDTH` / `VIEWPORT_WIDTH` |
| `tmc.viewHeight` | `240` | the same three names on the height axis |
| `tmc.baserom` | `baserom.gba` | ROM to extract from, relative to the repo root |

An optional `app/config.json` (not generated, absent by default) is staged
alongside the ROM and read by the engine as it would be on the desktop. It is
the practical way to change anything in there, since the app's private files
directory is awkward to reach on a device. Input bindings are the usual reason:
every button has a gamepad default (`kDefaults` in `port_runtime_config.cpp`)
covering the D-pad, face buttons, shoulders and start/select, but **the analog
sticks are not bound to anything** — add `SDL_AXIS:` entries if you want them.

The default is the expanded 320x240 viewport this milestone is about. The
regression gate covers the GBA-native size instead, which is worth remembering
when something looks wrong here — build it for comparison with:

```bash
cd android && ./gradlew assembleDebug -Ptmc.viewWidth=240 -Ptmc.viewHeight=160
```

Both axes are compile-time defines, so changing either forces a native rebuild.

The build is USA-only: `asset_extractor` hardcodes USA offsets in `xmake.lua`,
and the CMake build defines `USA` to match.

## How it fits together

**Native.** `app/src/main/cpp/CMakeLists.txt` builds `libmain.so` — the name
SDLActivity looks for — from the source list it **parses out of the `tmc_pc`
target in `xmake.lua`**. The list is not duplicated: a file added to the
desktop target joins this build with no second edit, and the parser fails the
configure step loudly if it ever stops matching xmake.lua's formatting. SDL3
is built from `libs/SDL` (a submodule pinned to `release-3.4.4`, matching the
version the desktop build resolves), and its `org.libsdl.app` Java glue is
compiled straight out of the same checkout so the two can never drift.

Compiler flags mirror the desktop target exactly, including its optimisation
split: C at `-O0`, C++ at `-O3`. That looks like an oversight in `xmake.lua`
but it is what the build the gate passes on actually compiles, and the decomp C
is the half carrying the load-bearing literals. `-DTMC_C_OPTIMIZATION=-O2`
raises it — nothing in this repo has measured the engine at anything else, on
any platform, so treat that as unverified.

**Assets.** `stageGameData` runs the same headless `asset_extractor` the
desktop build ships, against `baserom.gba`, into `app/build/gamedata-root/`.
Everything under there is packaged into the APK beneath `assets/gamedata/`.

The ROM is baked in alongside the extracted tree because the engine treats a
missing ROM as fatal (`port_rom.c`) even when every asset is present — the
extracted tree does not cover the assembled pointer tables. Nothing in
`app/build/` is checked in.

**Runtime.** The engine reaches the filesystem through relative-path `fopen`
throughout, and APK assets are not files. So `TmcActivity` mirrors
`assets/gamedata/` into the app's private files directory on first launch after
each install, and `port_main.c` `chdir()`s there before its first open. The
on-device layout ends up identical to a desktop install — `baserom.gba` next to
`assets/` — and every path in the engine works unchanged.

`TmcActivity` also repairs the asset fingerprint after staging. The engine
skips extraction when the ROM's size and mtime match what the extractor
recorded, and neither value survives the trip: the ROM gets a fresh mtime when
it is copied out of the APK, and the recorded one is in the *host* toolchain's
filesystem-clock epoch (libstdc++ counts from 2174, libc++ on the device from
1970). If that repair ever fails, the game still runs — it just re-extracts on
device on first launch, with a progress bar, reaching the same state slowly.

## Engine changes this build required

Five, all no-ops off Android:

- `port/port_main.c` includes `<SDL3/SDL_main.h>` under `__ANDROID__` so
  `main()` becomes the exported `SDL_main` that SDLActivity calls through JNI.
  Android-only on purpose: on Windows that same header would also rename
  `main()` and pull in SDL's `WinMain` shim, which is a real change to a build
  this port already ships.
- `port/port_main.c` `chdir()`s to the app's internal storage at the top of
  `main()`, as above.
- `port/port_main.c` adds `SDL_WINDOW_FULLSCREEN` to the window flags on
  Android. This is the only thing that hides the status bar: the activity's
  fullscreen theme does not survive `SDLActivity.onCreate`, which calls
  `setWindowStyle(false)` and adds `FLAG_FORCE_NOT_FULLSCREEN`. Asking SDL for
  a fullscreen window makes it call `setWindowStyle(true)`, which applies the
  immersive-sticky flags.
- `port/port_main.c` pipes stdout and stderr into logcat on a pump thread.
  Nothing else does: SDL leaves the process's stdio alone on Android, so every
  diagnostic the port prints would otherwise be written to nothing.
- `port/port_touch_controls.cpp` gained a `PORT_NO_TOUCH_CONTROLS` opt-out. Its
  implementation is `#ifdef __ANDROID__`, so an Android build would otherwise
  compile the on-screen overlay in. With the define set, every entry point
  becomes the same no-op the desktop build uses.

One change was **not** Android-only and applies everywhere:
`port/port_capture.c` line-buffers the `--record=` file. It had been fully
buffered and flushed only by an `atexit` handler, so a recording of a hang —
the only kind anyone makes — lost its whole buffer. On Android that was the
normal path (`SDL_main` returns to JNI rather than the process calling
`exit()`), which is how it was noticed, but the same footgun was live on
desktop and is what `record-bug.sh`'s "do NOT kill the process" warning was
about.

## What this deliberately does not do

- **No touch controls, in any form.** Not compiled, not reachable.
- **No launcher.** The `launcher` define (the tmc-Modern-Launcher UI and the
  in-game settings modal) is left unset, as on desktop.
- **`TMC_ANDROID_PORT` is not defined.** The upstream Android build used it to
  turn *off* the area / sprite-pointer / text JSON overrides
  (`port_asset_loader.cpp`) and to swap the embedded `sounds.json` source
  (`port_m4a_backend.cpp`). Since this build bakes in the full extracted tree,
  leaving it unset keeps the asset path byte-identical to the desktop build
  the regression gate covers. Turning it on would diverge from the only
  configuration anything here has verified.
- **`TMC_ANDROID_RUNTIME_DIR` is not set.** `PreferredAssetRoot()` falls back
  to the cwd on Android, and the `chdir()` above already puts that in the right
  place.
- **No ABI but arm64-v8a.** Each extra ABI multiplies the native build time.
- **No app icon.** The platform default is used.

## Diagnostics on a device

The port's traces and its capture/replay tooling both work here, which is what
made B16 findable. Three pieces:

**Its own log output reaches logcat.** SDL does not touch the process's stdio
on Android, so without this every `[AREA]`, `[sync]` and `[ASSET]` trace goes
to a descriptor pointing at nothing. It is line-buffered, so the last line
before a hang is the one you get.

```bash
adb logcat -c && adb logcat -s tmc:V > run.log
```

**Arguments come from a file**, since an Android app has no argv. Push
`args.txt` — one argument per line, `#` comments ignored — to the app's
external files directory, which `adb push` writes to without a `run-as` dance.
Anything else pushed alongside is staged into the working directory first, so a
recording and its save travel with it.

```bash
adb shell mkdir -p /sdcard/Android/data/org.tmc.port/files
printf -- '--script=bug.script\n' > args.txt
adb push args.txt bug.script /sdcard/Android/data/org.tmc.port/files/
```

**The `TMC_*` traces reach the device through `--env=NAME=VALUE`.** Every
diagnostic in this port is gated on `getenv`, and an Android app has no
environment — which used to mean the one platform where a bug reproduces was
the one platform where none of the instruments could be switched on. Put them
in `args.txt` like any other argument; each one echoes an `[env]` line into
logcat so you can see it took:

```
--env=TMC_STUCK_TRACE=1
--env=TMC_CAMTRACE=1
```

`TMC_STUCK_TRACE` is the one to reach for on a softlock: it reports when the
player has been in `PLAYER_ROOMTRANSITION` for 180 frames, with the direction,
collisions and position that decide whether he can leave the doorway. Set it to
a small number instead of 1 to make it fire on every ordinary doorway, which is
how you confirm it is running before you trust its silence.

**Recording works too**, and survives however the app ends:

```
--record=/sdcard/Android/data/org.tmc.port/files/device_bug.script
```

Clear `files/tmc.sav` first if you want the recording to start blank
(`adb shell run-as org.tmc.port rm -f files/tmc.sav`), then pull the script
afterwards — you can pull it while the game is still hung.

Running the *same* script on the device and on desktop, and diffing the traces,
is the comparison that identifies whether a symptom is the viewport or the
platform. It is worth reaching for early.

## Notes

- First launch after each install copies ~44 MB out of the APK before the
  window appears. Subsequent launches check a stamp file and skip it.
- Staging overwrites the files it owns rather than clearing the directory, so
  `tmc.sav` survives a reinstall. Uninstalling takes it with everything else.
- On first launch the engine also writes its `rom_data/` page cache — about
  2800 files and 11 MB — into the same directory. That is the desktop
  behaviour; nothing reads it back while a full ROM is present.
- `gradle/wrapper/gradle-wrapper.jar` is committed, unlike most binaries here,
  because the wrapper is useless without it — `./gradlew` on a fresh clone
  would need a system `gradle` to bootstrap the thing whose job is to remove
  that requirement. It is Gradle's own published 8.13 wrapper jar, sha256
  `81a82aaea5abcc8ff68b3dfcb58b3c3c429378efd98e7433460610fecd7ae45f`, which
  matches `services.gradle.org/distributions/gradle-8.13-wrapper.jar.sha256`.
  `gradlew.bat` is *not* committed — the root `.gitignore` catches `*.bat` and
  nothing here targets Windows. Regenerate either with
  `gradle wrapper --gradle-version 8.13 --distribution-type bin`.
- Toolchain versions are pinned in `app/build.gradle.kts`: AGP 8.13.2, Gradle
  8.13, NDK 27.3.13750724, compileSdk 36, minSdk 24. `libs/SDL` is pinned to
  `release-3.4.4` in `.gitmodules`.
- The regression gate in `CLAUDE.md` covers the desktop 240x160 build and says
  nothing about this one. Nothing here can substitute for playtesting on the
  device.
