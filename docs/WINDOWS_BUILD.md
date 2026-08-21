# Building on Windows (MSYS2 / MinGW-w64)

The README documents the MSVC + vcpkg route. This file documents the MSYS2 route,
which is what the current Windows dev machine uses: it needs no Visual Studio C++
workload, all dependencies are prebuilt packages, and it uses the same GCC/libstdc++
combination as the Linux build, so portability breakage shows up on both platforms.

## 1. Toolchain

```powershell
winget install --id MSYS2.MSYS2
```

Then, from an MSYS2 UCRT64 shell (`C:\msys64\ucrt64.exe`):

```bash
pacman -Syu                     # run twice: the first pass updates msys2-runtime and exits
pacman -S --needed \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-pkgconf \
    mingw-w64-ucrt-x86_64-SDL2 \
    mingw-w64-ucrt-x86_64-bullet \
    mingw-w64-ucrt-x86_64-openal \
    mingw-w64-ucrt-x86_64-fmt \
    mingw-w64-ucrt-x86_64-glm \
    mingw-w64-ucrt-x86_64-zlib \
    mingw-w64-ucrt-x86_64-gdb \
    git
```

## 2. Build

```bash
cd /c/path/to/donut
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

## 3. Run

`build/bin/donut.exe` links against the UCRT64 DLLs, so `C:\msys64\ucrt64\bin` must be
on `PATH`, and the working directory must be the project root (asset paths are relative).
`run.cmd` does both:

```
run.cmd
```

## Symlinks

The repo tracks `shaders` -> `assets/shaders` and `audio` -> `art/audio` as git symlinks.
Windows only materialises those as symlinks with Developer Mode or admin rights; without
it git writes them out as plain text files and the engine cannot find the shaders.

Replace them with directory junctions (no privileges needed) and hide the swap from git:

```powershell
cd <project root>
del shaders, audio
mklink /J shaders assets\shaders
mklink /J audio   art\audio
git update-index --skip-worktree shaders audio
```

## Game assets

See [ASSETS.md](ASSETS.md) for the layout. On Windows, the InstallShield CABs on the CDs
are not readable by 7-Zip; build [unshield](https://github.com/twogood/unshield) instead
(from the UCRT64 shell, `-DBUILD_STATIC=ON` is required — the shared build does not link
under MinGW):

```bash
git clone --depth 1 https://github.com/twogood/unshield.git
cmake -S unshield -B unshield/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_STATIC=ON -DBUILD_TESTING=OFF
cmake --build unshield/build
```

Then, with the three CD images extracted:

```bash
# data1.hdr (CD1) is the index for the whole data*.cab set, so collect every
# cab next to it before extracting
7z x "Hit & Run 1.iso" -ocd1 "data*.cab" "data*.hdr" "*.rcf" "movies"
7z x "Hit & Run 2.iso" -ocd1 "data*.cab"
7z x "Hit & Run 3.iso" -ocd1 "data*.cab"
cd cd1 && unshield -d ../files/game x data1.cab
```

The full extraction (~1.3 GB) lives in `files/` (gitignored). What the engine reads is
copied into the tree:

| From                       | To                  |
| -------------------------- | ------------------- |
| `files/game/Art/chars/*.cho` | `art/chars/`      |
| `files/game/Art/frontend/`   | `art/frontend/`   |
| `files/game/Art/missions/level01/`, `generic/` | `art/missions/` |
| `files/game/Sound/*.rcf`, `Music*/*.rcf`, CD1 `dialogf.rcf` | `art/audio/` |

All four destinations are gitignored — the level/car/character P3Ds already committed
under `art/` are a curated subset and stay as they are.
