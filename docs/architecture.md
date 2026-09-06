# Architecture

ORS is a native C++20 / Qt 6 Widgets screen recorder. Phase 1 delivers a working **screen → H.264/AAC MP4** pipeline on Windows. Interfaces from Phase 0 stay stable; later phases add region overlay, WMV, and WGC without changing those signatures.

## Dependency direction

```
ui  →  core  →  capture / audio / encode / mux
```

`mux` must not depend on `ui`. The Qt UI thread only receives session signals; it does not block on I/O after recording has started. `RecordingSession::start()` waits briefly for capture/muxer setup, then returns.

## Modules

| Module | Path | Phase 1 | Later |
|---|---|---|---|
| UI | `src/ui/` | Start/stop, timer, output folder | Region overlay (2), window picker (4) |
| Config | `src/core/Config.*` | JSON read/write | Extra settings fields stay additive |
| Session | `src/core/RecordingSession.*` | Capture + audio + encode/mux workers | Pause UI (2), game/audio tabs (4, 9) |
| Frame queue | `src/core/FrameQueue.h` | Bounded drop-oldest + `waitPop` | Same |
| Capture | `src/capture/` | `CaptureWin` DXGI Desktop Duplication | WGC (4) |
| Audio | `src/audio/` | `AudioCaptureWin` WASAPI loopback and/or mic, mixed to one stereo track | Dual-track mux (later) |
| Encode | `src/encode/` | `MFEncoder` packed BGRA | Dedicated H.264 MFT if needed |
| Mux | `src/mux/` | `Mp4Muxer` IMFSinkWriter H.264 + AAC; `GifEncoder` Median Cut + LZW GIF89a | WMV (3), other custom muxers (5–7) |

Windows-only sources (`CaptureWin`, `AudioCaptureWin`, `Mp4Muxer`) are compiled only on `WIN32`. `GifEncoder` is portable C++ and is included on all platforms. Mac/Linux placeholders stay in the tree but are excluded from the Windows target.

## Recording state machine

```
Idle ──start──▶ Recording ──pause──▶ Paused ──resume──▶ Recording
  ▲                  │                                      │
  └───────stop───────┴──────────────────stop─────────────────┘
```

`RecordingSession` exposes `start(RecordingRequest)` / `pause` / `resume` / `stop` and signals `stateChanged`, `errorOccurred`, `recordingFinished`.

Phase 1 UI wires start/stop. While recording or paused, the main window keeps its size and chrome; the action row shows only Stop / Pause / Screenshot, with current file size and free disk space on the right (refreshed every `recording.storageUpdateSeconds`). Pause/resume remain available on the session API and the Pause button. When `timeLimit.enabled` is true, the main-window elapsed timer (pause time excluded) calls `stop()` at the configured duration and then runs `timeLimit.action` only if the file was saved.

## Frames and packets

- `VideoFrame`: CPU buffer (`BGRA8` from DXGI, timestamp, width/height/stride, bytes).
- `AudioFrame`: PCM s16 stereo payload plus sample rate.
- `EncodedPacket`: packed BGRA video (pre-H.264) or PCM audio consumed by `IMuxer`.

`FrameQueue<T>` is a thread-safe bounded deque (default capacity 4). When full it drops the oldest frame and increments `dropped()`. `waitPop` blocks with a timeout; `wake` unblocks waiters on stop.

## Capture / encode / mux contracts

- `ICapture::start` / `stop` / `grab(VideoFrame&)` — `grab` runs on the capture thread.
- `IAudioCapture` — same pattern with `AudioFrame`.
- `IEncoder::open` / `encode` / `flush` / `close`.
- `IMuxer::open` / `writeVideo` / `writeAudio` / `finalize`.

Phase 1 uses **IMFSinkWriter** to compress packed BGRA (`MFVideoFormat_RGB32`) to H.264 and PCM to AAC while writing MP4. `MFEncoder` copies frames to a tightly packed even-sized buffer.

## Config (`%APPDATA%/ORS/config.json`)

Schema version `1`. Later phases add keys; they do not rename existing ones.

| Key | Default | Notes |
|---|---|---|
| `ui.language` | `zh_TW` | Development default; release will switch to `en_US` |
| `ui.lastTab` | `screen` | `screen` / `game` / `audio` |
| `output.directory` | `""` | Empty → system Videos folder (`QStandardPaths::MoviesLocation`) |
| `output.filenameTemplate` | `<Prefix>_<YYYY_MM_DD_HH_NN_SS_Z>` | Tokens: `<Prefix>`, `<Name>`, `<User>`, `<DisplayName>`, `<Date>`, `#` / `##` / `###`; Qt date format still works if there are no `<>` / `#` tokens |
| `output.filenamePrefix` | `錄製` | Expands `<Prefix>` |
| `output.filenameStartNumber` | `1` | Used by `<#>` / collision suffixes |
| `region.preset` | `monitor-primary` | Phase 2 fills region UI |
| `region.customWidth` / `customHeight` | 1920 / 1080 | |
| `region.saved[]` | `[]` | `{name, width, height}` |
| `codec.container` / `video` / `audio` | `mp4` / `h264` / `aac` | `mp4` records H.264/AAC; `gif` records a silent GIF; WMV is not wired yet |
| `audio.system` / `microphoneId` / `microphoneInputSource` | `true` / `""` / `stereo` | System loopback and/or selected mic mixed to one stereo track; mic source is `left`, `right`, or `stereo` |
| `hotkeys.toggleRecord` / `togglePause` / `captureStill` / `selectTarget` | `F9` / `F10` / `F3` / `F4` | Global `RegisterHotKey` on a message-only window; each has a matching `*Enabled` flag (default `true`). Bare keys already taken by another app (for example oCam's F3/F4) are stored as `Ctrl+…` |
| `recording.includeCursor` | `true` | DXGI frames composite the cursor with `GetCursorInfo` / `DrawIconEx` |
| `recording.alwaysOnTop` | `false` | Main window stays on top |
| `recording.useTrayIcon` / `hideWhenMinimized` / `hideOnStartup` | `true` / `false` / `false` | System tray; minimize or start hidden when the tray is available |
| `recording.frameRate` | `60` | Capture / encode FPS, clamped to 1–60 |
| `recording.quality` | `very-high` | `very-high` / `high` / `medium` / `low` / `custom` |
| `recording.customBitrateKbps` | `12000` | Used when quality is `custom` |
| `recording.keyframeInterval` | `5` | Seconds between keyframes; GOP frames = interval × FPS |
| `recording.resolutionAlign` | `8x4` | `8x4` / `2x2` / `16x16` |
| `recording.frameRateMode` | `vfr` | `vfr` waits for desktop presents and holds each unique frame until the next; `cfr` repeats the last frame on a 1/FPS clock |
| `recording.storageUpdateSeconds` | `5` | How often the recording chrome refreshes file size / free disk space; integer seconds, clamped to 1–999 |
| `capture.includeCursor` | `true` | Screenshot cursor overlay (independent of recording) |
| `capture.imageFormat` | `png` | `png` / `jpg` / `bmp`; `jpeg` loads as `jpg` |
| `gif.includeCursor` | `true` | Cursor overlay when `codec.container` is `gif` |
| `gif.frameRate` | `10` | GIF capture FPS, clamped to 1–60 |
| `timeLimit.enabled` | `false` | When true, recording stops after the configured duration |
| `timeLimit.minutes` / `seconds` | `10` / `0` | Clamped to 0–999 minutes and 0–59 seconds; duration is at least 1 second. Pause time does not count |
| `timeLimit.action` | `none` | After a successful time-limit stop: `none`, `restart`, `quit`, `shutdown`, or `sleep` |
| `watermark.enabled` | `false` | When true, overlay the image on recorded frames |
| `watermark.imagePath` / `opacity` / `x` / `y` | `""` / `100` / `10` / `10` | PNG/JPG/BMP with alpha; opacity 1–100; position relative to the capture region |
| `watermark.applyToCapture` | `true` | Also overlay on toolbar screenshots |
| `performance.useMultiCore` | `true` | When false, encoder threads are forced to 1 |
| `performance.encoderThreads` | `0` | `0` = auto (MF default); otherwise 1–16 via `CODECAPI_AVEncNumWorkerThreads` (hardware encoders may ignore it) |
| `performance.captureMode` | `dxgi` | `dxgi` Desktop Duplication or `gdi` BitBlt |
| `performance.pipelineLayers` | `3` | `3` / `2` / `0` maps to video queue capacity 4 / 2 / 1 |

## Pipeline (Phase 1)

```
Capture thread ──▶ FrameQueue ──▶ Encode/mux thread ──▶ MP4 or GIF file
Audio capture  ──▶ AudioQueue ──┘   (skipped for GIF)
```

- Capture: DXGI Desktop Duplication (default) or GDI BitBlt when `performance.captureMode` is `gdi`, on the monitor that overlaps the selected region; CPU crop to even width/height. Optional cursor overlay (drawn into a cursor-sized DIB, not a full-frame copy). `cfr` reuses the last frame on a 1/FPS deadline so the encoder keeps a steady rate; `vfr` waits up to 200 ms for the next desktop present (same idea as oCam “Variable Frame Rate (Fast)”) and only emits when the desktop or cursor updates. GDI has no update event, so it always captures at the target FPS. MP4 sample duration is the timestamp gap to the next unique frame, not a fixed 1/FPS slot. GIF recording forces CFR and uses `gif.frameRate` / `gif.includeCursor`. Optional watermark overlay after grab. The toolbar screenshot hides the region overlay and writes PNG / JPG / BMP with `QImage`. While idle it waits for a real desktop present (`grabStill`; skips empty DXGI frames, forces opaque alpha, falls back to GDI if idle). While recording or paused it copies the live capture frame (`snapshotFrame`) so DXGI is not duplicated, then falls back to GDI.
- Audio: WASAPI loopback when `audio.system` is true and/or the selected microphone when `audio.microphoneId` is set. The two streams are mixed into one stereo PCM track (mic left/right/stereo mapping applied first). Audio init failure falls back to video-only; one audio source failing does not drop the other. GIF output has no audio track.
- Encode/mux: one worker packs BGRA. MP4 writes via IMFSinkWriter (hardware transforms when available); GOP size follows `recording.keyframeInterval`. Queue depth follows `performance.pipelineLayers`. GIF uses a custom Median Cut + LZW encoder (`GifEncoder`).
- UI stays on the Qt main thread after start returns.

Known Phase 1 limits: no multi-monitor spanning in one file (the output with the largest overlap is captured), game/audio tabs and WMV are not wired yet. Hardware H.264 encoders may ignore GOP size and encoder thread count. GIF does not resize frames.

## i18n

Source strings are Traditional Chinese wrapped in `tr()`. `resources/i18n/ors_zh_TW.ts` and `ors_en.ts` are compiled with `lrelease` and embedded under `:/i18n/`. Phase 0–4 load `ors_zh_TW.qm` from `ui.language`.
