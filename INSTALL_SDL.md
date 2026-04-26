# Building the SDL Port

For the GBA ROM build see [INSTALL.md](INSTALL.md).

This file is a quick-start guide for the host SDL port. The full design
document is in [docs/sdl_port.md](docs/sdl_port.md).

## Linux (Debian/Ubuntu)

```sh
sudo apt install cmake ninja-build libsdl2-dev python3
cmake --preset sdl-release
cmake --build --preset sdl-release
./build/sdl-release/tmc_sdl
```

## macOS

```sh
brew install cmake ninja sdl2 python3
cmake --preset sdl-release
cmake --build --preset sdl-release
./build/sdl-release/tmc_sdl
```

## Real graphics (your `baserom.gba`)

The default preset has no ROM path, so `LoadGfxGroup` never unpacks tiles
and the window stays on the rasterizer backdrop. To point CMake at **your
own** matching retail `baserom.gba` (16,777,216 bytes) so configure runs
`tools/port/gen_host_assets.py` and links generated gfx / palette / frame
tables, set **`TMC_BASEROM`** to an **absolute** path, then use the
`-baserom` presets (separate build dirs so stub and asset builds do not
share a cache):

```sh
export TMC_BASEROM=/absolute/path/to/baserom.gba
cmake --preset sdl-release-baserom
cmake --build --preset sdl-release-baserom
./build/sdl-release-baserom/tmc_sdl
```

One-shot without exporting the variable:

```sh
TMC_BASEROM=/absolute/path/to/baserom.gba cmake --preset sdl-release-baserom
cmake --build --preset sdl-release-baserom
```

To exercise the same asset pipeline **without** a retail ROM (still a
blank screen, but validates `gen_host_assets` and linking):

```sh
python3 tools/port/gen_host_assets.py make-placeholder-baserom --out /tmp/placeholder_baserom.gba
TMC_BASEROM=/tmp/placeholder_baserom.gba cmake --preset sdl-release-baserom
cmake --build --preset sdl-release-baserom
```

Details, `TMC_GAME_VERSION`, and limitations (sounds, asm-only data, etc.) are in
[docs/sdl_port.md](docs/sdl_port.md#game-assets--baseromgba).

## Windows

With vcpkg:

```pwsh
vcpkg install sdl2:x64-windows
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
build\Release\tmc_sdl.exe
```

With MSYS2 (mingw64):

```sh
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-SDL2
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/tmc_sdl.exe
```

## No system SDL2?

Add `-DTMC_USE_FETCHCONTENT_SDL=ON` to the `cmake` invocation. SDL2 will
be downloaded and statically linked.

## What the port currently does

The default build links the real game source into `tmc_sdl` and boots
into `src/main.c::AgbMain`. The binary opens a window, accepts
keyboard / X-Input gamepad input, opens a silent audio device, and
runs the GBA game-state machine through `HandleNintendoCapcomLogos ->
HandleTitlescreen` into the title-screen idle.

**Without `-DTMC_BASEROM=...`**, the framebuffer stays at the rasterizer
backdrop: logos and title art do not load. **With `TMC_BASEROM`** set to
your matching retail ROM at configure time, the host build extracts gfx
and palette groups and you get real visuals in the SDL window (see the
**Real graphics** section above). Audio remains silent until the m4a mixer
work lands; m4a / sample tables may still be stubbed — see
[docs/sdl_port.md](docs/sdl_port.md#game-assets--baseromgba).

For a bit-exact GBA cartridge image, build the matching ROM
([INSTALL.md](INSTALL.md)) and use an emulator. The SDL port and the ROM
build coexist.

If you only want to exercise the SDL platform layer in isolation (a
blank window paced at 59.7274 Hz with input + silent audio, and no
game logic linked in), pass `-DTMC_LINK_GAME_SOURCES=OFF` at configure
time.
