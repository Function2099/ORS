# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Screenshot from the toolbar: DXGI still of the current region, saved as PNG / JPG / BMP
- Settings **Capture** page: include-cursor toggle and image format (PNG / JPG / BMP)
- Settings **Audio** page: system audio toggle, WASAPI microphone device list, left/right/stereo input source — mixed into one AAC track on the next recording
- Settings dialog uses the main window chrome (teal header, mode pills, chip buttons)
- Recording **General** page: cursor/always-on-top/tray toggles, FPS, quality, keyframe interval, resolution align, VFR/CFR — applied on the next recording (or immediately for window/tray chrome)
- Settings dialog now has all ten category pages (Recording, Audio, Capture, GIF, Hotkeys, Save, Time limit, Watermark, Performance, Language); unimplemented pages show a placeholder
- Settings uses a left-hand accordion menu (four groups, collapsed by default) with the page content on the right

### Changed

- Default recording name is `錄製_YYYY_MM_DD_HH_NN_SS_ms.mp4` instead of `ORS_yyyyMMdd_HHmmss.mp4`

### Added

- Phase 1 core recording: DXGI Desktop Duplication + WASAPI + Media Foundation MP4
- Start/stop from the main window, elapsed timer, output to the Videos folder
- `MFEncoder` packed BGRA copy and `Mp4Muxer` via IMFSinkWriter (H.264 + AAC)
- Catch2 test for NV12 color conversion

- Phase 0 bootstrap: CMake + Qt 6 Widgets executable, Ninja presets, `windeployqt` deploy
- Main window shell: original ORS chrome (teal header, mode pills, record CTA, status meta)
- Settings dialog with JSON config at `%APPDATA%/ORS/config.json`
- Core contracts: `VideoFrame`, `AudioFrame`, `EncodedPacket`, `ICapture`, `IAudioCapture`, `IEncoder`, `IMuxer`
- `RecordingSession` state machine (start stays Idle until Phase 1)
- Bounded `FrameQueue` and Catch2 tests
- Qt Linguist `ors_zh_TW` / `ors_en` embedded translations
- Windows CI: install Qt 6.8.3 MSVC, build, `ctest`
- `docs/building.md` and `docs/architecture.md`
