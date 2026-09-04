# Architecture

ORS is a native C++20 / Qt 6 Widgets screen recorder. Phase 0 locks **module boundaries, frame types, interfaces, config schema, and the recording state machine**. Capture, encode, and mux implementations are filled in later phases without changing these signatures.

## Dependency direction

```
ui  →  core  →  capture / audio / encode / mux
```

`mux` must not depend on `ui`. The Qt UI thread only receives session signals; it does not block on I/O.

## Modules

| Module | Path | Phase 0 | Later |
|---|---|---|---|
| UI | `src/ui/` | `MainWindow` shell, `SettingsDialog` | Region overlay (2), window picker (4) |
| Config | `src/core/Config.*` | JSON read/write | Extra settings fields stay additive |
| Session | `src/core/RecordingSession.*` | State machine, no workers | Wire capture/encode/mux threads (1+) |
| Frame queue | `src/core/FrameQueue.h` | Bounded drop-oldest queue | Used by capture → encode |
| Capture | `src/capture/ICapture.h` | Interface only | `CaptureWin` DXGI (1), WGC (4) |
| Audio | `src/audio/IAudioCapture.h` | Interface only | WASAPI mic (1), loopback (3) |
| Encode | `src/encode/IEncoder.h` | Interface only | `MFEncoder` (1) |
| Mux | `src/mux/IMuxer.h` | Interface only | `Mp4Muxer` (1), `WmvMuxer` (3), custom muxers (5–8) |

Windows-only sources (`CaptureWin`, `AudioCaptureWin`, `MFEncoder`, `Mp4Muxer`, …) are **not compiled** in Phase 0. Mac/Linux placeholders stay in the tree but are excluded from the Windows target.

## Recording state machine

```
Idle ──start──▶ Recording ──pause──▶ Paused ──resume──▶ Recording
  ▲                  │                                      │
  └───────stop───────┴──────────────────stop─────────────────┘
```

`RecordingSession` exposes `start` / `pause` / `resume` / `stop` and signals `stateChanged`, `errorOccurred`, `recordingFinished`.

Phase 0 `start()` does **not** spawn capture threads. It emits `errorOccurred("擷取管線尚未接上")` and stays `Idle`. Phase 1 implements workers behind the same methods.

## Frames and packets

- `VideoFrame`: CPU buffer (`BGRA8` / `NV12`, timestamp, width/height/stride, bytes). Phase 1 may add an optional D3D11 texture without changing `FrameQueue`.
- `AudioFrame`: PCM payload plus sample rate / channels / bit depth.
- `EncodedPacket`: encoder output consumed by `IMuxer`.

`FrameQueue<T>` is a thread-safe bounded deque (default capacity 4). When full it drops the oldest frame and increments `dropped()`.

## Capture / encode / mux contracts

- `ICapture::start` / `stop` / `grab(VideoFrame&)` — `grab` runs on the capture thread.
- `IAudioCapture` — same pattern with `AudioFrame`.
- `IEncoder::open` / `encode` / `flush` / `close`.
- `IMuxer::open` / `writeVideo` / `writeAudio` / `finalize`.

## Config (`%APPDATA%/ORS/config.json`)

Schema version `1`. Later phases add keys; they do not rename existing ones.

| Key | Default | Notes |
|---|---|---|
| `ui.language` | `zh_TW` | Development default; release will switch to `en_US` |
| `ui.lastTab` | `screen` | `screen` / `game` / `audio` |
| `output.directory` | `""` | Empty → system Videos folder (`QStandardPaths::MoviesLocation`) |
| `output.filenameTemplate` | `ORS_yyyyMMdd_HHmmss` | Qt date format |
| `region.preset` | `monitor-primary` | Phase 2 fills region UI |
| `region.customWidth` / `customHeight` | 1920 / 1080 | |
| `region.saved[]` | `[]` | `{name, width, height}` |
| `codec.container` / `video` / `audio` | `mp4` / `h264` / `aac` | |
| `audio.system` / `microphoneId` | `true` / `""` | |
| `hotkeys.toggleRecord` / `togglePause` | `F9` / `F10` | Registered in Phase 9 |

## Pipeline (Phase 1+)

```
Capture thread ──▶ FrameQueue ──▶ Encode thread ──▶ Muxer thread ──▶ file
Audio capture  ──▶ AudioQueue ──┘
```

UI stays on the Qt main thread.

## i18n

Source strings are Traditional Chinese wrapped in `tr()`. `resources/i18n/ors_zh_TW.ts` and `ors_en.ts` are compiled with `lrelease` and embedded under `:/i18n/`. Phase 0–4 load `ors_zh_TW.qm` from `ui.language`.
