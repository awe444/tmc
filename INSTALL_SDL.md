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

PR #1 of the SDL-port roadmap is implemented: the binary opens a window,
accepts keyboard / X-Input gamepad input, opens a silent audio device,
and runs an empty 59.7274 Hz frame loop. The actual game logic is not
yet linked in. See [docs/sdl_port.md](docs/sdl_port.md) for the full
roadmap and current status.
