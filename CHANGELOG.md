# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Phase 0 bootstrap: CMake + Qt 6 Widgets executable, Ninja presets, `windeployqt` deploy
- Main window shell: original ORS chrome (teal header, mode pills, record CTA, status meta)
- Settings dialog with JSON config at `%APPDATA%/ORS/config.json`
- Core contracts: `VideoFrame`, `AudioFrame`, `EncodedPacket`, `ICapture`, `IAudioCapture`, `IEncoder`, `IMuxer`
- `RecordingSession` state machine (start stays Idle until Phase 1)
- Bounded `FrameQueue` and Catch2 tests
- Qt Linguist `ors_zh_TW` / `ors_en` embedded translations
- Windows CI: install Qt 6.8.3 MSVC, build, `ctest`
- `docs/building.md` and `docs/architecture.md`
