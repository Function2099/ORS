# Open Recording Software (ORS)

[English](README.md) | [繁體中文](README.zh-TW.md)

A lightweight, native **C++20 / Qt 6** screen recorder. No FFmpeg, no game-process injection.

[![CI](https://github.com/Function2099/ORS/actions/workflows/ci.yml/badge.svg)](https://github.com/Function2099/ORS/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows-0078D6.svg)](#requirements)
[![Status](https://img.shields.io/badge/status-0.1.0%20preview-yellow.svg)](#status)

## Status

**0.1.0 preview** — Windows only. Screen recording to **MP4** (H.264 + AAC) and silent **GIF** works today. Portable zip builds are unsigned (SmartScreen may warn). There is no installer.

The UI currently defaults to Traditional Chinese. English is available in Settings → Language. A later 1.0 release will default to English.

## Features

| Area | What works now |
|---|---|
| Capture | Full screen, primary/secondary monitor, presets, drag-select, custom size. DXGI Desktop Duplication (default) or GDI. Optional cursor overlay. Main window is excluded from the recording. |
| Recording | Start / pause / resume / stop. VFR (desktop presents) or CFR. Hardware H.264 via Media Foundation when the GPU supports it; software fallback otherwise. |
| Audio | System loopback and/or a chosen microphone, mixed into **one** AAC track. Mic source can be left, right, or stereo. |
| Still capture | PNG / JPG / BMP, including while recording or paused. |
| Output | **MP4** (H.264 + AAC) and **GIF** (custom Median Cut + LZW encoder, no audio). Files go to the Videos folder, or the path in Settings. |
| UI | Region overlay, settings (recording, audio, capture, GIF, hotkeys, save, time limit, watermark, performance, language), system tray, always-on-top. |
| Hotkeys | F9 record, F10 pause, F3 screenshot, F4 select region (each can be disabled). If a key is already taken, ORS stores `Ctrl+…` instead. |
| Extra | Image watermark, recording time limit (then none / restart / quit / shutdown / sleep), filename templates, dropped-frame counter. |

## Not implemented yet

These items appear in the UI, the source tree, or the [project plan](screen-recorder-plan.md), but they are **not usable** yet:

| Item | Notes |
|---|---|
| **Game capture** | The Game tab is a placeholder. Windows Graphics Capture and the window picker are not wired. |
| **Audio-only recording** | The Audio tab is a placeholder. WAV output is planned later. |
| **WMV** | Listed in the format menu; recording still only accepts MP4 and GIF. |
| **Other containers** | AVI, FLV, MKV, MOV, TS, VOB — stub files only. |
| **H.265** | Not offered. |
| **Dual audio tracks** | System sound and microphone are mixed into a single stereo track. |
| **macOS / Linux** | Capture and audio files exist as placeholders and are not built. |
| **Installer / signed releases** | Portable zip only; no NSIS/Inno and no Authenticode signature. |
| **Multi-monitor spanning** | One recording captures the display with the largest overlap, not a stitched desktop. |

Out of scope for v1.0: live annotation, scheduled recording, video editing, cloud upload, and live streaming (RTMP).

The full phase list is in [`screen-recorder-plan.md`](screen-recorder-plan.md) (Traditional Chinese) and [`docs/architecture.md`](docs/architecture.md).

## Download

Windows 64-bit portable zip: [Releases](https://github.com/Function2099/ORS/releases)

1. Download `ORS-x.y.z-win64.zip` and extract the **whole** folder.
2. Run `ORS.exe` inside that folder (keep the Qt DLL folders next to it).
3. If SmartScreen says the publisher is unknown and you downloaded from this repository’s Releases page, open **More info** → **Run anyway**.
4. If the app will not start because of a missing `VCRUNTIME` DLL, install the [Visual C++ Redistributable (x64)](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist).

To publish a zip from source: build, then `powershell -File scripts/pack-windows.ps1`. Pushing a `v*.*.*` tag runs the same pack on GitHub Actions and attaches the zip to the release.

## Requirements

- Windows 10 version 1903 or later, or Windows 11
- GPU with DXGI 1.2+ (hardware encode needs a working vendor driver)
- To **build**: Visual Studio 2022 or 2026 with Desktop development with C++, CMake 3.20+, Ninja, Qt 6.8.3 MSVC 2022 64-bit

MinGW Qt kits cannot be mixed with the MSVC compiler.

## Build

Details: [`docs/building.md`](docs/building.md). From an **x64 Native Tools** prompt:

```powershell
$env:CMAKE_PREFIX_PATH = "C:/Qt/6.8.3/msvc2022_64"
cmake --preset windows-release
cmake --build ignore/build
```

The GUI is `ignore/build/ORS.exe`. A post-build step runs `windeployqt` so Qt DLLs sit next to the executable.

```powershell
ctest --test-dir ignore/build --output-on-failure
```

Build trees belong under [`ignore/`](ignore/README.md) (gitignored except that README). Do not commit compiler output.

## Usage

1. Run `ignore/build/ORS.exe`.
2. Choose a region (or leave the overlay on the primary monitor).
3. Pick **H.264 + AAC (.MP4)** or **GIF**.
4. Press **Record** (or F9). Pause with F10; screenshot with F3.
5. Files are written to the system Videos folder unless you change **Settings → Save**.

Settings are stored at `%APPDATA%/ORS/config.json`.

## Documentation

- [Building](docs/building.md)
- [Architecture](docs/architecture.md)
- [Project plan](screen-recorder-plan.md) (Traditional Chinese, includes the original roadmap)
- [Changelog](CHANGELOG.md)

## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) and the [Code of Conduct](CODE_OF_CONDUCT.md). Open an issue before large changes. Bug reports and well-scoped pull requests are welcome.

## Security

Do not file public issues for vulnerabilities. See [SECURITY.md](SECURITY.md).

ORS does not inject into games. Protected / DRM video and some exclusive-fullscreen titles may capture as a black frame. Anti-cheat software can still restrict screen capture; that is a platform limitation, not a supported workaround.

## License

ORS source is **MIT** — see [LICENSE](LICENSE).

Qt 6 is **dynamically linked** under LGPL v3. See [NOTICE](NOTICE) for third-party terms.
