# Open Recording Software (ORS)

[English](README.md) | [繁體中文](README.zh-TW.md)

輕量、原生 **C++20 / Qt 6** 螢幕錄影軟體。不依賴 FFmpeg，也不對遊戲行程做注入。

[![CI](https://github.com/Function2099/ORS/actions/workflows/ci.yml/badge.svg)](https://github.com/Function2099/ORS/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows-0078D6.svg)](#系統需求)
[![Status](https://img.shields.io/badge/status-0.1.0%20preview-yellow.svg)](#現況)

## 現況

**0.1.0 預覽版** — 目前只有 Windows。螢幕錄製輸出 **MP4**（H.264 + AAC）與無聲 **GIF** 已可使用。提供未簽署的免安裝 zip（SmartScreen 可能會警告）。沒有安裝檔。

介面目前預設繁體中文。可在「設定 → 語言」切換英文。之後 1.0 正式版會改以英文為預設。

## 已實作功能

| 類別 | 說明 |
|---|---|
| 擷取 | 全螢幕、主要/第二顯示器、常用解析度、拖曳選區、自訂大小。預設 DXGI Desktop Duplication，可改 GDI。可含游標。主視窗不會被錄進去。 |
| 錄製 | 開始/暫停/繼續/停止。VFR（等桌面真正更新）或 CFR。有硬體編碼器時走 Media Foundation H.264，否則軟體備援。 |
| 音訊 | 系統音迴路與/或指定麥克風，**混成一軌** AAC。麥克風可選左、右或立體聲。 |
| 截圖 | PNG/JPG/BMP，錄影中或暫停時也可截。 |
| 輸出 | **MP4**（H.264 + AAC）與 **GIF**（自製 Median Cut + LZW，無音訊）。預設存到系統「影片」資料夾，或設定裡的路徑。 |
| 介面 | 錄製框、設定（錄製、聲音、擷取、GIF、快捷鍵、儲存、時間限制、浮水印、效能、語言）、系統匣、視窗置頂。 |
| 快捷鍵 | F9 錄製、F10 暫停、F3 截圖、F4 選區（可個別關閉）。若按鍵已被其他程式佔用，會改存成 `Ctrl+…`。 |
| 其他 | 圖片浮水印、錄製時長限制（到時可無事/重錄/結束程式/關機/睡眠）、檔名模板、丟幀計數。 |

## 尚未實作

下列項目可能出現在介面、原始碼目錄或[專案計畫](screen-recorder-plan.md)裡，但**現在還不能用**：

| 項目 | 說明 |
|---|---|
| **遊戲錄製** | 「遊戲」分頁只是佔位。Windows Graphics Capture 與視窗挑選器尚未接上。 |
| **僅音訊錄製** | 「音訊」分頁只是佔位。之後計畫輸出 WAV。 |
| **WMV** | 輸出格式選單有此項，但錄製仍只接受 MP4 與 GIF。 |
| **其他容器** | AVI、FLV、MKV、MOV、TS、VOB 目前只有空檔。 |
| **H.265** | 尚未提供。 |
| **雙音軌** | 系統音與麥克風會混成單一立體聲軌，不是分開兩軌。 |
| **macOS/Linux** | 擷取與音訊檔案只是預留，不會被編進 Windows 目標。 |
| **安裝檔/簽署發布** | 只有免安裝 zip，沒有 NSIS/Inno，也尚未程式碼簽章。 |
| **多螢幕拼成單一畫面** | 一次錄製只抓重疊面積最大的那一台顯示器。 |

v1.0 範圍外：錄影中即時標註、排程錄影、剪輯後製、雲端上傳、直播推流（RTMP）。

完整階段規劃見 [`screen-recorder-plan.md`](screen-recorder-plan.md) 與 [`docs/architecture.md`](docs/architecture.md)。

## 下載

Windows 64 位元免安裝包：[Releases](https://github.com/Function2099/ORS/releases)

1. 下載 `ORS-x.y.z-win64.zip`，解壓**整個**資料夾。
2. 在該資料夾內執行 `ORS.exe`（Qt 的 DLL 與子資料夾要跟 exe 放在一起）。
3. 若 SmartScreen 顯示未知發行者，且檔案來自本倉庫的 Releases，選「其他資訊」→「仍要執行」。
4. 若因缺少 `VCRUNTIME` 而無法啟動，請安裝 [Visual C++ 可轉散發套件（x64）](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)。

從原始碼打包：編譯後執行 `powershell -File scripts/pack-windows.ps1`。推送 `v*.*.*` 標籤時，GitHub Actions 會自動打包並附在 Release 上。

## 系統需求

- Windows 10 1903 以上，或 Windows 11
- 支援 DXGI 1.2+ 的 GPU（硬體編碼需要對應廠商驅動）
- **編譯**需要：Visual Studio 2022 或 2026（C++ 桌面開發）、CMake 3.20+、Ninja、Qt 6.8.3 MSVC 2022 64-bit

MinGW 的 Qt 套件不能與 MSVC 編譯器混用。

## 編譯

詳細步驟見 [`docs/building.md`](docs/building.md)。請在 **x64 Native Tools** 終端機執行：

```powershell
$env:CMAKE_PREFIX_PATH = "C:/Qt/6.8.3/msvc2022_64"
cmake --preset windows-release
cmake --build ignore/build
```

執行檔為 `ignore/build/ORS.exe`。編譯後會跑 `windeployqt`，把 Qt DLL 放到執行檔旁邊。

```powershell
ctest --test-dir ignore/build --output-on-failure
```

建置產物放在 [`ignore/`](ignore/README.md)（除該 README 外皆不進 Git）。請不要提交編譯輸出。

## 使用方式

1. 執行 `ignore/build/ORS.exe`。
2. 選擇錄製範圍（或沿用主要顯示器上的錄製框）。
3. 選擇 **H.264 + AAC (.MP4)** 或 **GIF**。
4. 按「錄製」（或 F9）。F10 暫停；F3 截圖。
5. 檔案會寫入系統「影片」資料夾，除非你在「設定 → 儲存」改過路徑。

設定檔位於 `%APPDATA%/ORS/config.json`。

## 文件

- [建置說明](docs/building.md)
- [架構](docs/architecture.md)
- [專案計畫](screen-recorder-plan.md)
- [變更紀錄](CHANGELOG.md)

## 貢獻

請先閱讀 [CONTRIBUTING.md](CONTRIBUTING.md) 與 [行為準則](CODE_OF_CONDUCT.md)。較大的改動請先開 Issue。歡迎回報缺陷與範圍清楚的 Pull Request。

## 安全性

請勿用公開 Issue 回報漏洞，見 [SECURITY.md](SECURITY.md)。

ORS 不會注入遊戲行程。受 DRM 保護的畫面、以及部分獨佔全螢幕遊戲，可能錄成黑畫面。反作弊軟體仍可能限制螢幕擷取；這是平台限制，不是支援的繞過手段。

## 授權

ORS 原始碼為 **MIT**，見 [LICENSE](LICENSE)。

Qt 6 以 **動態連結** 使用，授權為 LGPL v3。第三方條款見 [NOTICE](NOTICE)。
