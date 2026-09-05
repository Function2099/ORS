# Open Recording Software (ORS)

A lightweight, high-performance, open-source cross-platform screen recorder.

## Status

Phase 1 (Core recording): Windows screen capture to H.264/AAC MP4. Press Record to start, Stop to finish. Files go to the system Videos folder (or the path in Settings).

## Build

See [docs/building.md](docs/building.md). Short version (VS x64 developer prompt):

```powershell
$env:CMAKE_PREFIX_PATH = "C:/Qt/6.8.3/msvc2022_64"
cmake --preset windows-release
cmake --build ignore/build
```

The GUI is `ignore/build/ORS.exe` after `windeployqt`.

## Local-only files

Put build outputs and personal scratch files under [`ignore/`](ignore/README.md) (gitignored except that README).

## Features (Phase 1)

- Screen recording (DXGI Desktop Duplication) to H.264/AAC MP4
- Region screenshot to PNG, JPG, or BMP
- Optional system audio (WASAPI loopback)
- Hardware-accelerated encoding via Media Foundation when available

Later phases:

- Game/window recording (Windows Graphics Capture)
- Dedicated audio-only recording
- WMV and custom muxers

## Documentation

- [Project plan](screen-recorder-plan.md) (Traditional Chinese)
- [Building](docs/building.md)
- [Architecture](docs/architecture.md)

## License

MIT — see [LICENSE](LICENSE). Qt is dynamically linked under LGPL v3; see [NOTICE](NOTICE).
