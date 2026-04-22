# Building the SDL Port

For the GBA ROM build see [INSTALL.md](INSTALL.md).

This file is a quick-start guide for the host SDL port. The full design
document is in [docs/sdl_port.md](docs/sdl_port.md).

## Linux (Debian/Ubuntu)

```sh
sudo apt install cmake ninja-build libsdl2-dev
cmake --preset sdl-release
cmake --build --preset sdl-release
./build/sdl-release/tmc_sdl
```

## macOS

```sh
brew install cmake ninja sdl2
cmake --preset sdl-release
cmake --build --preset sdl-release
./build/sdl-release/tmc_sdl
```

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

**Building with real game assets is not yet supported.** The SDL build
does not ingest `baserom.gba` or the `tools/asset_processor` extraction
output, so the framebuffer stays at the rasterizer's cleared backdrop:
the Nintendo / Capcom logos, title-screen art, sprites, and audio
samples never load. EEPROM-backed save data still round-trips
correctly. See [docs/sdl_port.md](docs/sdl_port.md#game-assets--baseromgba)
for the full "Game assets / `baserom.gba`" expectations and
[docs/sdl_port.md](docs/sdl_port.md) for the roadmap.

If you want the visually faithful, audio-on game today, build the
matching GBA ROM (see [INSTALL.md](INSTALL.md)) and run it under your
GBA emulator of choice. The SDL port and the ROM build coexist and do
not interfere with each other.

If you only want to exercise the SDL platform layer in isolation (a
blank window paced at 59.7274 Hz with input + silent audio, and no
game logic linked in), pass `-DTMC_LINK_GAME_SOURCES=OFF` at configure
time.
