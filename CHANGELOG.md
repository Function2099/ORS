# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-09-07

### Added

- Windows portable zip packing (`scripts/pack-windows.ps1`) and a GitHub Release workflow on `v*.*.*` tags
- Recording status shows dropped frames when the capture queue overruns
- Non-fatal `warningOccurred` when audio init fails, capture falls back to GDI, or MP4 uses software H.264

### Changed

- Public README (English and Traditional Chinese), contributing guide, code of conduct, and security policy for the open-source repository

### Fixed

- Pause no longer stretches the last video sample across the pause; mux timestamps skip paused time
- Stop no longer joins encoder finalize on the UI thread (new `Stopping` state)
- VFR matches oCam Fast: only unique desktop presents are encoded (no cursor-only frames); in-recording screenshots reuse the last mapped frame instead of waiting out a timeout
- VFR desktop presents are capped at the configured FPS instead of mapping every DXGI update (high-refresh displays)
- DXGI `ACCESS_LOST` rebuilds Desktop Duplication instead of aborting the recording
- Audio write failures stop the session instead of dropping samples silently
- Audio queue waits instead of dropping PCM when the encoder is briefly behind
- DXGI / WASAPI waits are ~16 ms so Stop is not stalled on a 200 ms acquire timeout

### Changed

- MP4 encoding enables Media Foundation low-latency mode and requests zero B-frames
- Default recording quality is `high` (was `very-high`)
- MP4 encode path prefers NV12 into IMFSinkWriter; GPU crop uses `CopySubresourceRegion`
- WASAPI capture uses event callback and QPC timestamps aligned to the video clock
- Main window is excluded from capture (`WDA_EXCLUDEFROMCAPTURE`)
- Screenshots work while recording or paused (copies the live capture frame; GDI fallback because DXGI Desktop Duplication is already in use)
- Settings **Time limit** page: enable a recording duration (minutes/seconds; pause time does not count) and choose what happens after a successful stop — do nothing, start a new recording, quit, shut down, or sleep (Windows)
- Settings **Watermark** page: enable an image overlay (PNG / JPG / BMP with alpha), opacity, X/Y position, and optional application to screenshots
- Settings **Performance** page: multi-core / encoder threads, DXGI vs GDI capture, and pipeline-layer queue depth
- Settings **Hotkeys** page: enable each action and capture a key (F9 record, F10 pause, F3 screenshot, F4 select region)
- Global Windows hotkeys via a dedicated message window; if F3/F4 are already taken (for example by oCam), ORS keeps the action working as Ctrl+F3 / Ctrl+F4

- Settings **Capture** page: include-cursor toggle and image format (PNG / JPG / BMP)
- Settings **Audio** page: system audio toggle, WASAPI microphone device list, left/right/stereo input source — mixed into one AAC track on the next recording
- Settings dialog uses the main window chrome (teal header, mode pills, chip buttons)
- Recording **General** page: cursor/always-on-top/tray toggles, FPS (1–240, default 60), quality, keyframe interval, resolution align, VFR/CFR — applied on the next recording (or immediately for window/tray chrome)
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
