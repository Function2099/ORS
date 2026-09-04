# Open Recording Software (ORS)

A lightweight, high-performance, open-source cross-platform screen recorder.

## Status

Phase 0 (Bootstrap): Windows shell builds and runs. Recording is not implemented yet.

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

## Features (planned)

- Screen recording (DXGI Desktop Duplication)
- Game/window recording (Windows Graphics Capture)
- Audio recording
- Hardware-accelerated H.264 encoding (Media Foundation)
- MP4/WMV output; additional formats via custom muxers

## Documentation

- [Project plan](screen-recorder-plan.md) (Traditional Chinese)
- [Building](docs/building.md)
- [Architecture](docs/architecture.md)

## License

MIT — see [LICENSE](LICENSE). Qt is dynamically linked under LGPL v3; see [NOTICE](NOTICE).
