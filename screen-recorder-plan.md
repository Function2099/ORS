# Open Recording Software (ORS) — 專案計畫書

> **版本：** v3.1（在 v2 原生 C/C++ 架構基礎上，補齊開源發布、錄製管線、風險與里程碑規劃；決策已全部確認）  
> **軟體全名：** Open Recording Software  
> **簡稱：** ORS  
> **發布形式：** GitHub 正式開源軟體（完全自研程式碼，不複製或破解既有軟體）

---

## 0. 版本變更說明

| 版本 | 重點 |
|---|---|
| v1 | Electron + FFmpeg（執行檔呼叫） |
| v2 | 改為全原生 C/C++；捨棄 FFmpeg 與 DLL Hook，改用 Windows 原生 API + WGC |
| **v3** | 定名 **ORS**；補齊開源發布策略、錄製管線架構、設定持久化、測試/CI、風險與限制、里程碑對照 |
| **v3.1** | 確認 MIT 授權、Catch2、JSON、`open-recording-software` repo；語言策略：開發期繁中、發布預設英文 |

---

## 1. 專案願景與目標

### 1.1 願景
打造一款**輕量、高效、完全開源**的跨平台螢幕錄影軟體，功能對標 oCam 的核心錄製體驗，但以現代 Windows 原生 API 實作，避免 FFmpeg 依賴與遊戲 Hook 帶來的體積、授權與反作弊風險。

### 1.2 成功指標（可量測）

| 指標 | 目標（Windows Phase 1–4 完成時） |
|---|---|
| 安裝/解壓後體積 | ≤ 50 MB（不含 Qt 執行時若採動態連結則另計） |
| 1080p60 全螢幕錄製 CPU 佔用 | 硬體編碼路徑下，額外 CPU < 15%（中高階 CPU 基準） |
| 錄製延遲（擷取→檔案可播放） | 停止後 3 秒內完成封裝收尾 |
| 功能覆蓋 | 螢幕錄製 + 遊戲錄製 + 音訊錄製 + MP4/WMV 輸出 |
| 開源就緒 | 可從乾淨 clone 在 Windows 上建置並執行 |

### 1.3 設計原則
1. **原生優先**：能用 OS 內建 API 就不引入第三方函式庫
2. **介面抽象、實作分平台**：`ICapture` / `IAudioCapture` / `IMuxer` 三層抽象
3. **先可用、再完整**：Phase 1–4 先交付日常可用的 MP4 工具，格式擴充與跨平台移植分階段進行
4. **開源友善**：目錄結構、建置腳本、授權聲明從第一天就對齊 GitHub 發布標準

### 1.4 語言與在地化（i18n）策略

| 階段 | UI 語言 | 說明 |
|---|---|---|
| **開發期（Phase 0–4）** | **繁體中文** | 開發者自用、迭代快；所有字串仍須包在 `tr()` 內 |
| **正式發布（v1.0.0 起）** | **英文為預設** | `en_US` 為預設語系；繁中作為首批翻譯之一 |
| **架構** | Qt Linguist | 從 Phase 0 建立 `resources/i18n/ors_zh_TW.ts`、`ors_en.ts`；建置時用 `lrelease` 產生 `.qm` |

開發期介面可以先顯示中文，但**不寫死裸字串**——一律 `tr("錄製")` 這種形式，之後切換預設語言只需調整載入的 `.qm`，不必重構 UI。

---

## 2. 平台與技術選型

| 項目 | 決定 | 理由 |
|---|---|---|
| 目標平台 | Windows / macOS / Linux（**Windows 優先**） | 跨平台為長期目標，先集中火力做好 Windows |
| 開發語言 | **C / C++（C++20）** | 極致錄製效能、體積貼近原生等級 |
| GUI 框架 | **Qt 6（C++，Widgets）** | 跨平台成熟度最高；LGPL 動態連結可合規開源 |
| 建置系統 | **CMake 3.20+** | Qt 官方推薦、利於 CI 多平台建置 |
| 畫面擷取（Windows，桌面/區域） | **DXGI Desktop Duplication API** | 微軟官方高效能桌面擷取 |
| 畫面擷取（Windows，遊戲/視窗） | **Windows Graphics Capture API (WGC)** | 抓合成後畫面，不需注入遊戲 process；`CreateFromWindow` / `CreateFromMonitor` |
| 音訊擷取（Windows） | **WASAPI**（含 Loopback 錄系統音） | 原生音訊，支援麥克風與系統音效 |
| 視訊編碼 | **Media Foundation Transform (MFT)** | 透過驅動呼叫 NVENC / QuickSync / AMF，無需廠商 SDK |
| 封裝輸出（MP4/WMV） | **IMFSinkWriter** | Windows 原生封裝 |
| 封裝輸出（其他格式） | **自製 Muxer**（MKV/MOV/AVI/FLV/TS/VOB） | 練習底層格式、最小化依賴 |
| GIF 輸出 | **自製 GIF 編碼器**（調色盤量化 + LZW） | 與視訊 codec 獨立 |
| ~~FFmpeg~~ | **完全移除** | 無 GPL/LGPL 疑慮、體積最小化 |
| ~~遊戲錄製 DLL Hook~~ | **廢棄，改用 WGC** | 避免反作弊誤判；與螢幕錄製共用 capture 抽象層 |

### 2.1 跨平台後期對照（Phase 10–11）

| 模組 | macOS | Linux |
|---|---|---|
| 畫面擷取 | ScreenCaptureKit | PipeWire（首選）/ X11（備援） |
| 音訊擷取 | CoreAudio | PulseAudio / PipeWire |
| 視訊編碼 | VideoToolbox | VAAPI（硬體）/ 軟體編碼備援 |
| 封裝 | AVFoundation / 自製 muxer 共用層 | 自製 muxer 共用層 |

---

## 3. 核心功能規格（對標 oCam「螢幕錄製」工具列）

參考 oCam 介面，共 6 個工具列按鈕 + 錄製控制：

### 3.1 錄製（Record）
一鍵開始/停止錄製目前設定的來源與區域；錄製中需有明顯狀態指示（工具列圖示、計時器、可選系統匣提示）。

### 3.2 擷取（Capture）
單張截圖（非影片），存成 PNG/JPG，與錄影共用同一組來源/區域設定。

### 3.3 錄製區域（Recording Region）— 下拉選單
- 預設尺寸：Youtube 子選單（720p/1080p/1440p/4K/8K）、其他常用（640×360、800×450、1024×576、640×480、800×600、1024×768）
- 全螢幕 (F) / 主要顯示器 (P) / 第二顯示器 (S)
- 選擇區域…(A)：滑鼠拖曳框選
- 自訂大小…(C)：手動輸入寬高
- 新增錄製區域 / 編輯錄製區域列表：常用區域存成清單快速切換

### 3.4 開啟（Open）
開啟輸出資料夾（使用目前設定的輸出路徑）。

### 3.5 編解碼器（Codec）— 下拉選單
- **預設**：自動選擇 — H.264 + AAC (.MP4)（Media Foundation Sink Writer）
- **容器格式**：MP4、WMV（原生）／MKV、M4V、MOV、FLV、AVI、TS、VOB（自製 muxer）
- **GIF 動畫輸出**（自製編碼器）
- **進階（可延後）**：H.265 選項、音訊 bitrate、視訊 bitrate/CBR/VBR
- **硬體編碼不可用時**：自動降級軟體 MFT 或提示使用者調整解析度/幀率

### 3.6 聲音（Sound）— 下拉選單
- 錄製系統聲音（開關；Windows: WASAPI Loopback）
- 麥克風裝置選擇（列舉系統音訊裝置）
- 不錄製麥克風
- 系統音與麥克風混音策略：以時間戳對齊後由 muxer 寫入雙音軌或混成單音軌（**Phase 3 先實作雙音軌，混音可延後**）

### 3.7 錄製控制
- 開始 / 暫停 / 繼續 / 停止
- 全域快捷鍵：預設 **F9** 開始/停止、**F10** 暫停/繼續
- 快捷鍵註冊失敗時**必須跳出提示**（不可靜默失敗）
- 錄製計時顯示

### 3.8 儲存
- 預設輸出：**系統「影片」資料夾**（`SHGetKnownFolderPath(FOLDERID_Videos)`，不寫死路徑）
- 檔名規則：`ORS_YYYYMMDD_HHMMSS.mp4`（可在設定中調整模板）
- 可在設定裡自訂路徑；錄完可選「自動開啟資料夾」

---

## 4. 頂部分頁：螢幕錄製 / 遊戲錄製 / 音訊錄製

| 分頁 | 擷取方式 | 預設差異 |
|---|---|---|
| **螢幕錄製** | DXGI Desktop Duplication（桌面/區域） | 預設錄系統音 + 可選麥克風 |
| **遊戲錄製** | WGC `CreateFromWindow`（`WindowPicker` 選取） | 預設不錄麥克風、幀率預設 60 |
| **音訊錄製** | 無畫面，僅音訊管線 | 輸出 WAV/MP3（MP3 需評估是否自製或 Phase 12+ 再議） |

三個分頁共用同一套 **capture 抽象層 + encode/mux pipeline**，不是獨立子專案。

---

## 5. 不在第一版（v1.0.0）範圍內

- 錄影中即時繪圖/標註
- 自動排程錄影
- 影片剪輯/後製
- 快捷鍵自訂 UI（先用固定預設鍵）
- 多螢幕同時錄製
- 雲端上傳/分享
- 直播推流（RTMP 等）

---

## 6. 錄製管線架構（核心技術設計）

### 6.1 執行緒模型

```
┌─────────────┐    ┌──────────────┐    ┌─────────────┐    ┌──────────┐
│ Capture     │───▶│ Frame Queue  │───▶│ Encode      │───▶│ Muxer    │──▶ 檔案
│ Thread      │    │ (bounded)    │    │ Thread      │    │ Thread   │
└─────────────┘    └──────────────┘    └─────────────┘    └──────────┘
       ▲                                        ▲
┌─────────────┐    ┌──────────────┐           │
│ Audio       │───▶│ Audio Queue  │───────────┘
│ Capture     │    │ (bounded)    │
└─────────────┘    └──────────────┘
```

- **Capture Thread**：DXGI / WGC 拉 frame；音訊獨立 callback 或 thread
- **Frame Queue**：有界環形緩衝（建議 3–5 幀）；滿了丟最舊幀並記錄 dropped frame 計數
- **Encode Thread**：MF MFT 吃 NV12/BGRA，輸出 H.264/H.265 NAL units
- **Muxer Thread**：IMFSinkWriter 或自製 muxer 寫入；負責 A/V 時間戳對齊
- **UI Thread（Qt 主執行緒）**：只收狀態 signal，不做阻塞 I/O

### 6.2 錄製狀態機

```
Idle ──start──▶ Recording ──pause──▶ Paused ──resume──▶ Recording
  ▲                  │                                      │
  └───────stop───────┴──────────────────stop─────────────────┘
```

- 狀態轉換必須原子化；停止時依序：停止擷取 → flush encode → finalize mux → 回報檔案路徑
- 暫停期間不推 frame 進 queue，但保留 session 設定

### 6.3 A/V 同步策略
- 視訊時間戳：以 `QueryPerformanceCounter` 相對於錄製開始時間
- 音訊時間戳：WASAPI capture position 換算為同一時鐘
- Muxer 寫入時允許 ±1 幀的 jitter；長時間錄製每 30 秒記錄 drift 指標到 log

### 6.4 設定持久化
- 格式：**JSON**（`%APPDATA%/ORS/config.json` 或跨平台等效路徑）
- 內容：輸出路徑、最後使用的區域預設、codec 選擇、音訊裝置 ID、分頁偏好
- Phase 1 可硬編碼預設值 + 簡單 JSON；Phase 2 補 SettingsDialog 完整編輯

---

## 7. 專案架構與目錄結構

```
open-recording-software/                # GitHub repo 根目錄（repo 名：open-recording-software）
├── CMakeLists.txt
├── LICENSE
├── README.md
├── CHANGELOG.md
├── CONTRIBUTING.md
├── .gitignore                          # 主要忽略 ignore/**（僅保留 ignore/README.md）
├── ignore/                             # 本機專用，不上 Git（建置產物、本機筆記等）
│   ├── README.md                       # 唯一會提交的說明檔
│   ├── build/                          # CMake：cmake -B ignore/build -S .
│   ├── out/                            # 可選：安裝/打包輸出
│   └── local/                          # 可選：個人草稿、本機設定
├── .github/
│   ├── workflows/
│   │   └── ci.yml                      # Windows 建置；後期擴 macOS/Linux
│   ├── ISSUE_TEMPLATE/
│   └── PULL_REQUEST_TEMPLATE.md
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── RecordingSession.cpp/.h     # 狀態機 + 管線編排
│   │   ├── FrameQueue.cpp/.h
│   │   └── Config.cpp/.h               # JSON 設定讀寫
│   ├── ui/
│   │   ├── MainWindow.cpp/.h
│   │   ├── RegionOverlay.cpp/.h
│   │   ├── WindowPicker.cpp/.h
│   │   └── SettingsDialog.cpp/.h
│   ├── capture/
│   │   ├── ICapture.h
│   │   ├── CaptureWin.cpp/.h           # DXGI + WGC
│   │   ├── CaptureMac.cpp/.h           # 後期
│   │   └── CaptureLinux.cpp/.h         # 後期
│   ├── audio/
│   │   ├── IAudioCapture.h
│   │   ├── AudioCaptureWin.cpp/.h
│   │   ├── AudioCaptureMac.cpp/.h
│   │   └── AudioCaptureLinux.cpp/.h
│   ├── encode/
│   │   └── MFEncoder.cpp/.h
│   └── mux/
│       ├── IMuxer.h
│       ├── Mp4Muxer.cpp/.h             # IMFSinkWriter 封裝
│       ├── WmvMuxer.cpp/.h             # IMFSinkWriter 封裝
│       ├── AviMuxer.cpp/.h
│       ├── FlvMuxer.cpp/.h
│       ├── MkvMuxer.cpp/.h
│       ├── MovMuxer.cpp/.h
│       ├── TsMuxer.cpp/.h
│       ├── VobMuxer.cpp/.h
│       └── GifEncoder.cpp/.h
├── resources/
│   ├── icons/
│   ├── i18n/
│   │   ├── ors_en.ts                   # 英文（發布預設語系）
│   │   └── ors_zh_TW.ts                # 繁中（開發期主要語系）
│   └── ors.qrc
├── docs/
│   ├── architecture.md                 # 管線與模組說明（開源友善）
│   ├── building.md                     # 建置教學
│   └── formats/                        # 自製 muxer 規格筆記
└── tests/
    ├── muxer/                          # muxer 單元測試
    └── integration/                    # 錄製 smoke test（可選）
```

**模組依賴方向（單向）：** `ui` → `core` → `capture` / `audio` / `encode` / `mux`。禁止 mux 反向依賴 UI。

**本機產物約定：** 不需要進 Git 的內容一律放在 `ignore/`（例如 `ignore/build`）。`.gitignore` 以忽略整個 `ignore/**` 為主，不分散列出一堆副檔名規則。

---

## 8. 自製 Muxer 實作順序

| 順序 | 格式 | 難度 | 備註 |
|---|---|---|---|
| 1 | AVI | 低 | RIFF chunk，最佳練手起點 |
| 2 | FLV | 中低 | Tag-based，規格完整 |
| 3 | MKV | 中 | EBML/Matroska |
| 4 | MOV | 中 | 與 MP4 box 結構相近 |
| 5 | TS | 高 | PAT/PMT、188-byte packets |
| 6 | VOB | 高 | MPEG-PS，可與 TS 對照學習 |
| 7 | GIF | 獨立 | Median Cut 等調色盤量化 + LZW |

---

## 9. 開發階段與里程碑

### 9.1 Phase 規劃

| Phase | 內容 | 目標版本 |
|---|---|---|
| **1** | DXGI 全螢幕 + WASAPI 麥克風 + MF 編碼 + MP4 輸出 + 開始/停止 | `0.1.0-alpha` |
| **2** | 區域框選、暫停/繼續、截圖、錄製區域列表、設定 JSON | `0.2.0-alpha` |
| **3** | WASAPI Loopback 系統音、WMV 輸出 | `0.3.0-beta` |
| **4** | 遊戲錄製分頁（WGC + WindowPicker） | `0.4.0-beta` |
| **5** | 自製 Muxer：AVI → FLV | `0.5.0` |
| **6** | 自製 Muxer：MKV → MOV | `0.6.0` |
| **7** | 自製 Muxer：TS → VOB | `0.7.0` |
| **8** | GIF 編碼器 | `0.8.0` |
| **9** | 全域快捷鍵、音訊錄製分頁 | `0.9.0` |
| **10** | macOS 移植 | `1.0.0`（或 `1.1.0`） |
| **11** | Linux 移植 | `1.x` |
| **12** | 安裝檔打包（NSIS/Inno Setup）、簽章 | Release |

> **v1.0.0 定義建議：** Phase 1–4 完成 + 基本文件 + CI 綠燈 + 授權/NOTICE 齊全。自製 muxer 與跨平台可作為 1.x 增量功能。

### 9.2 GitHub Milestone 對照

| Milestone | 包含 Phase | 說明 |
|---|---|---|
| `M0: Bootstrap` | 建置腳本、空殼 UI、CI | 可編譯、可執行空視窗 |
| `M1: Core Recording` | Phase 1–2 | 日常可用螢幕錄製 |
| `M2: Audio & Formats` | Phase 3 | 系統音 + WMV |
| `M3: Game Capture` | Phase 4 | WGC 遊戲分頁 |
| `M4: Custom Muxers` | Phase 5–8 | 格式擴充 |
| `M5: Cross-platform` | Phase 10–11 | macOS / Linux |

---

## 10. 開源發布策略（GitHub）

### 10.1 授權（已確認）

| 元件 | 授權 | 說明 |
|---|---|---|
| ORS 自有程式碼 | **MIT** | 根目錄放 `LICENSE`（MIT 全文） |
| Qt | **LGPL v3（動態連結）** | 使用 `find_package(Qt6)` 動態連結；`NOTICE` 中標註 Qt 源碼取得方式 |
| Windows SDK API | 系統授權 | 無額外限制 |

第三方字體/圖示若有使用，一併記錄於 `NOTICE`。

### 10.2 版本號策略（SemVer）
- `0.x.y`：開發期；API 可變
- `1.0.0`：Windows 功能完整（Phase 1–4）且文件齊備
- `MAJOR`：破壞性設定格式或管線 API 變更

### 10.3 首次開倉 Checklist
- [ ] `README.md`：專案簡介、截圖/GIF、建置步驟、系統需求、授權
- [ ] `LICENSE` + `NOTICE`
- [ ] `CONTRIBUTING.md`：分支策略、commit 風格、PR 流程
- [ ] `CHANGELOG.md`：Keep a Changelog 格式
- [ ] GitHub Actions：`windows-latest` 上 CMake 建置
- [ ] Release：附 `ORS-x.y.z-win64.zip` 或安裝檔
- [ ] Issue 標籤：`bug` `enhancement` `platform:windows` `good first issue` `muxer`

### 10.4 分支策略
- `main`：穩定可發布
- `develop`（可選）：整合中功能
- Feature branch：`feature/phase1-dxgi-capture` 等

### 10.5 CI 最低要求（Phase 0 即建立）
```yaml
# 概念：push/PR 時在 Windows 上
# cmake -B ignore/build -S . -DCMAKE_PREFIX_PATH=...
# cmake --build ignore/build --config Release
# ctest --test-dir ignore/build（有測試時）
```

---

## 11. 測試策略

| 層級 | 範圍 | 工具 |
|---|---|---|
| 單元測試 | muxer 寫入/讀回、config JSON 解析 | **Catch2** |
| 整合測試 | 錄製 5 秒 → 驗證 MP4 可開、有視訊軌 | 手動 / CI smoke（可選） |
| 手動測試矩陣 | 雙顯示器、獨顯/集顯、不同解析度、遊戲視窗 | 測試清單文件化 |

Phase 1 不強求自動化整合測試；Phase 5 起 muxer 應有單元測試覆蓋。

---

## 12. 已知風險與限制

| 風險 | 影響 | 緩解 |
|---|---|---|
| **WGC 無法擷取受 DRM 保護內容** | Netflix 等黑畫面 | UI 提示「受保護內容無法錄製」 |
| **少數全螢幕獨佔遊戲** | 黑畫面或擷取失敗 | 文件說明；建議改視窗化/邊框視窗模式 |
| **無硬體編碼器** | CPU 飆高、掉幀 | 自動降級軟體編碼或降低解析度；設定頁顯示目前編碼器 |
| **反作弊軟體** | 極少數仍可能限制螢幕擷取 | 文件免責；不採用 DLL 注入 |
| **自製 muxer 相容性** | 部分播放器不支援 | 每種格式附驗證過的播放器清單 |
| **Qt 部署體積** | 超過 oCam 體積 | 動態連結 + `windeployqt`；Release strip |

---

## 13. 系統需求（初版）

### Windows（Phase 1–4）
- **OS**：Windows 10 1903+（WGC 需此版本以上）或 Windows 11
- **GPU**：支援 DXGI 1.2+；硬體編碼需對應廠商驅動
- **RAM**：4 GB 以上
- **磁碟**：錄製輸出空間視使用者而定
- **開發環境**：Visual Studio 2019/2022、CMake 3.20+、Qt 6.5+

---

## 14. 已確認決策

### 14.1 技術與功能（v2 延續）
- [x] 遊戲錄製：WGC，併入主專案 capture 模組
- [x] 多格式輸出：捨棄 FFmpeg；MP4/WMV 走 MF，其餘自製 muxer
- [x] 開發順序：Phase 1–3 先做 MP4/WMV 完整版
- [x] 快捷鍵：F9 / F10 預設；衝突必須提示
- [x] 輸出資料夾：`FOLDERID_Videos`，不寫死路徑
- [x] 建置：CMake（見第 15 章入門）
- [x] **v1.0.0 定義**：Phase 1–4 完成 + 文件 + CI 綠燈；自製 muxer 與跨平台歸入 1.x

### 14.2 開源與工程（v3.1 確認）
- [x] **授權**：MIT
- [x] **測試框架**：Catch2
- [x] **設定格式**：JSON（`%APPDATA%/ORS/config.json`）
- [x] **GitHub repo 名稱**：`open-recording-software`
- [x] **UI 語言**：開發期繁中顯示；正式發布預設英文；全程使用 Qt `tr()` + Linguist
- [x] **音訊錄製（Phase 9）**：先輸出 **WAV**；AAC 延後（可走 MF）

---

## 15. CMake 入門說明

CMake 是「產生建置檔案的工具」——撰寫 `CMakeLists.txt` 描述專案，CMake 產生 Visual Studio / Ninja 等建置環境後再編譯。

### 15.1 基本概念
- **`CMakeLists.txt`**：描述原始碼、連結函式庫、輸出目標
- **`add_executable()`**：宣告執行檔
- **`target_link_libraries()`**：連結 Qt、Windows SDK（`dxgi` `d3d11` `mfplat` `mfreadwrite` `mfuuid` 等）
- **`find_package(Qt6)`**：尋找已安裝的 Qt

### 15.2 最小範例
```cmake
cmake_minimum_required(VERSION 3.20)
project(ORS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(ORS
    src/main.cpp
    src/ui/MainWindow.cpp
)

target_link_libraries(ORS PRIVATE
    Qt6::Widgets
    dxgi d3d11 mfplat mfreadwrite mfuuid
)
```

### 15.3 Windows 建置流程
```powershell
cmake -B ignore/build -S . -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2019_64"
cmake --build ignore/build --config Release
```

### 15.4 本專案 CMake 重點
- 用 `if(WIN32)` / `if(APPLE)` / `if(UNIX AND NOT APPLE)` 分平台編譯
- Phase 1 先 **Windows only**，避免過早複雜化
- 整合 `windeployqt` 做 Release 部署

---

## 16. 下一步行動

1. **Plan 模式**：依 Phase 0 → Phase 1 拆解 issue 與任務順序
2. **Phase 0（Bootstrap）**：CMake + 空殼 Qt 視窗 + CI + LICENSE/README 骨架
3. **Phase 1**：實作 `ICapture` → `CaptureWin`(DXGI) → `MFEncoder` → `Mp4Muxer` → 可錄製 MP4

---

*本計畫書 v3.1：所有主要決策已確認完畢。可進入 Cursor Plan 模式，從 Phase 0 Bootstrap 開始。*
