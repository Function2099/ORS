# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Screenshots work while recording or paused (copies the live capture frame; GDI fallback because DXGI Desktop Duplication is already in use)
- Settings **Time limit** page: enable a recording duration (minutes/seconds; pause time does not count) and choose what happens after a successful stop — do nothing, start a new recording, quit, shut down, or sleep (Windows)
- Settings **Watermark** page: enable an image overlay (PNG / JPG / BMP with alpha), opacity, X/Y position, and optional application to screenshots
- Settings **Performance** page: multi-core / encoder threads, DXGI vs GDI capture, and pipeline-layer queue depth
- Settings **Hotkeys** page: enable each action and capture a key (F9 record, F10 pause, F3 screenshot, F4 select region)
- Global Windows hotkeys via a dedicated message window; if F3/F4 are already taken (for example by oCam), ORS keeps the action working as Ctrl+F3 / Ctrl+F4

- Settings **Capture** page: include-cursor toggle and image format (PNG / JPG / BMP)
- Settings **Audio** page: system audio toggle, WASAPI microphone device list, left/right/stereo input source — mixed into one AAC track on the next recording
- Settings dialog uses the main window chrome (teal header, mode pills, chip buttons)
- Recording **General** page: cursor/always-on-top/tray toggles, FPS, quality, keyframe interval, resolution align, VFR/CFR — applied on the next recording (or immediately for window/tray chrome)
- Settings **GIF** page: include-cursor toggle and FPS (1–60), stored as `gif.*` and used when the output format is GIF
- Toolbar output format menu includes **GIF (.GIF)**; recording writes a silent GIF89a file via a custom Median Cut + LZW encoder
- Settings dialog now has all ten category pages (Recording, Audio, Capture, GIF, Hotkeys, Save, Time limit, Watermark, Performance, Language)
- Settings uses a left-hand accordion menu (four groups, collapsed by default) with the page content on the right

### Fixed

- Toolbar screenshots saved as fully transparent PNGs: DXGI Desktop Duplication leaves alpha at 0, which `QImage` then writes as empty. Frames are now forced opaque, empty DXGI presents are skipped, and still capture falls back to GDI if the desktop is idle.
- Variable-frame-rate recordings hitching despite a 60 FPS setting: DXGI waited only 1/FPS then dropped the slot, and each MP4 sample was tagged 1/FPS even when the next unique frame arrived much later. Capture now waits for the real desktop present (oCam-style VFR), sample duration is the gap to the next frame, and the cursor overlay copies only the cursor rectangle.
- Recording FPS spin box allowed 1–120 and GIF 1–50; both now clamp to 1–60.
- While recording, the action row keeps the same window size and shows only Stop / Pause / Screenshot, with current file size and free disk space on the right. The storage readout refreshes every 5 seconds by default (`recording.storageUpdateSeconds`, integer, minimum 1).

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
