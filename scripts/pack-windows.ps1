# Pack a portable Windows zip: ORS.exe + Qt runtime (not the CMake tree).
# Run from a Developer prompt after a Release build.
#
#   powershell -File scripts/pack-windows.ps1
#   powershell -File scripts/pack-windows.ps1 -Version 0.1.0 -BuildDir ignore/build

[CmdletBinding()]
param(
    [string]$BuildDir = "",
    [string]$OutRoot = "",
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $repoRoot

if (-not $BuildDir) { $BuildDir = Join-Path $repoRoot "ignore/build" }
if (-not $OutRoot) { $OutRoot = Join-Path $repoRoot "ignore/out" }
if (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $repoRoot $BuildDir
}
if (-not [System.IO.Path]::IsPathRooted($OutRoot)) {
    $OutRoot = Join-Path $repoRoot $OutRoot
}

if (-not $Version) {
    $cmake = Get-Content (Join-Path $repoRoot "CMakeLists.txt") -Raw
    if ($cmake -match 'project\(\s*ORS\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
        $Version = $Matches[1]
    } else {
        throw "Could not read project version from CMakeLists.txt; pass -Version"
    }
}

$exeCandidates = @(
    (Join-Path $BuildDir "ORS.exe"),
    (Join-Path $BuildDir "Release/ORS.exe"),
    (Join-Path $BuildDir "RelWithDebInfo/ORS.exe")
)
$exePath = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exePath) {
    throw "ORS.exe not found under $BuildDir. Build Release first."
}
$binDir = Split-Path -Parent $exePath

$name = "ORS-$Version-win64"
$stage = Join-Path $OutRoot $name
$zipPath = Join-Path $OutRoot "$name.zip"

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
New-Item -ItemType Directory -Path $stage | Out-Null

Copy-Item $exePath (Join-Path $stage "ORS.exe")

Get-ChildItem $binDir -Filter "Qt6*.dll" -File | Where-Object {
    $_.Name -notmatch 'd\.dll$'
} | ForEach-Object { Copy-Item $_.FullName $stage }

foreach ($extra in @("opengl32sw.dll", "dxcompiler.dll", "dxil.dll", "vc_redist.x64.exe")) {
    $src = Join-Path $binDir $extra
    if (Test-Path $src) {
        Copy-Item $src $stage
    }
}

foreach ($dir in @(
        "platforms",
        "styles",
        "imageformats",
        "iconengines",
        "tls",
        "generic",
        "networkinformation"
    )) {
    $src = Join-Path $binDir $dir
    if (Test-Path $src) {
        Copy-Item $src (Join-Path $stage $dir) -Recurse
    }
}

$qwindows = Join-Path $stage "platforms/qwindows.dll"
if (-not (Test-Path $qwindows)) {
    throw "Missing platforms/qwindows.dll — windeployqt did not run, or the build dir is incomplete."
}

foreach ($doc in @("LICENSE", "NOTICE")) {
    $src = Join-Path $repoRoot $doc
    if (Test-Path $src) {
        Copy-Item $src $stage
    }
}

$readMe = @"
Open Recording Software (ORS) $Version — Windows x64 portable build

Extract this whole folder. Run ORS.exe from inside it.
Do not copy ORS.exe somewhere else by itself; the Qt DLL folders must stay next to it.

This build is not Authenticode-signed. Windows SmartScreen may say the publisher is
unknown. If you downloaded it from https://github.com/Function2099/ORS/releases ,
open More info and choose Run anyway.

If ORS.exe will not start (VCRUNTIME / MSVCP missing), install Microsoft Visual C++
Redistributable 2015-2022 x64, or run vc_redist.x64.exe from this folder if present.

Requires Windows 10 version 1903 or later, or Windows 11 (64-bit).

---

請解壓整個資料夾，在資料夾內執行 ORS.exe。
不要只把 ORS.exe 單獨移走，Qt 的 DLL 與 platforms 等子資料夾必須跟 exe 放在一起。

此版本尚未程式碼簽章。若 Windows 顯示「未知發行者」，且檔案來自
https://github.com/Function2099/ORS/releases ，可選「其他資訊」→「仍要執行」。

若因缺少 VCRUNTIME 而無法啟動，請安裝 Visual C++ 可轉散發套件（x64），
或執行本資料夾內的 vc_redist.x64.exe（若有）。
"@
Set-Content -Path (Join-Path $stage "README.txt") -Value $readMe -Encoding utf8

New-Item -ItemType Directory -Path $OutRoot -Force | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($stage, $zipPath)

$hash = (Get-FileHash -Algorithm SHA256 $zipPath).Hash.ToLowerInvariant()
Set-Content -Path "$zipPath.sha256" -Value "$hash  $name.zip" -Encoding ascii

Write-Host "Packed $zipPath"
Write-Host "SHA256 $hash"
Write-Host "Folder $stage"
