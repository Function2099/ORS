# Building ORS

Windows desktop build for Phase 1. Later phases keep the same CMake entry points.

## Requirements

- Windows 10 1903+ or Windows 11
- Visual Studio 2022 or 2026 with **Desktop development with C++**
- CMake 3.20+ (Qt ships 3.30 under `C:\Qt\Tools\CMake_64\bin`)
- Ninja (Qt ships it under `C:\Qt\Tools\Ninja`)
- Qt 6.8.3 **MSVC 2022 64-bit** kit: `C:\Qt\6.8.3\msvc2022_64`

MinGW Qt kits cannot be mixed with the MSVC compiler.

## Environment

From an **x64 Native Tools** prompt (so `cl.exe` is on `PATH`):

```powershell
$env:CMAKE_PREFIX_PATH = "C:/Qt/6.8.3/msvc2022_64"
```

If CMake/Ninja are not already on `PATH`:

```powershell
$env:Path = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:Path
```

## Configure and build

Build trees belong under `ignore/` (gitignored):

```powershell
cmake --preset windows-release
cmake --build ignore/build
ctest --test-dir ignore/build --output-on-failure
```

Equivalent without presets:

```powershell
cmake -B ignore/build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build ignore/build
```

The `ORS.exe` output is `ignore/build/ORS.exe` (Ninja). A `POST_BUILD` step runs `windeployqt` so Qt DLLs sit next to the executable.

Visual Studio multi-config generators place the binary at `ignore/build/Release/ORS.exe`.

## CMake targets

| Target | Purpose |
|---|---|
| `ORS` | GUI application |
| `ors_tests` | Catch2 tests (`Config`, `FrameQueue`) |

Phase 1 links Windows libraries (`dxgi`, `d3d11`, `mf`, `mfplat`, `mfreadwrite`, `mfuuid`, `ole32`) for capture/encode/mux.
