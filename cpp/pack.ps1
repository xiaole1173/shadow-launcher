<#
.SYNOPSIS
    Shadow Launcher - One-click packager (flat structure)
    Copies build artifacts, runs windeployqt, bundles EasyTier, cleans up, compresses.
.NOTES
    i18n .qm files embedded in QRC via qt_add_translations; no manual deploy needed.
    EasyTier binaries go to bin/ subdirectory.
    SHADOW_DEV=1 auto-detected: clears env var and triggers Release rebuild before packaging.
#>
$ErrorActionPreference = "Stop"

$ProjectRoot = "D:\latest-code\cpp"
$BuildDir    = "$ProjectRoot\build\Release"
$QtDir       = "C:\Qt\6.8.3\msvc2022_64"
$Windeployqt = "$QtDir\bin\windeployqt.exe"
$DistDir     = "$ProjectRoot\dist\ShadowLauncher"
$OneMB       = 1048576

$BuildDate  = Get-Date -Format "yyyy-MM-dd HH:mm"

# Extract version from CMakeLists.txt
$VersionTag = "v0.0.0"
$cmakeFile = "$ProjectRoot\CMakeLists.txt"
if (Test-Path $cmakeFile) {
    $match = Select-String -Path $cmakeFile -Pattern 'SHADOW_DISPLAY_VERSION="([^"]+)"' | Select-Object -First 1
    if ($match) {
        $VersionTag = $match.Matches.Groups[1].Value
    }
}

Write-Host "`n  ========================================" -ForegroundColor Cyan
Write-Host "   Shadow Launcher Packager (flat)" -ForegroundColor Cyan
Write-Host "   $VersionTag" -ForegroundColor Cyan
Write-Host "  ========================================`n" -ForegroundColor Cyan

# ---- Preflight ----
Write-Host "[0/5] Preflight checks..." -ForegroundColor Yellow

if (-not (Test-Path "$BuildDir\ShadowLauncher.exe")) {
    Write-Host "  FAIL: ShadowLauncher.exe not found - build first!" -ForegroundColor Red
    exit 1
}

$exeTime = (Get-Item "$BuildDir\ShadowLauncher.exe").LastWriteTime.ToString("yyyy-MM-dd HH:mm")
Write-Host "       exe built : $exeTime" -ForegroundColor Gray

# Auto-handle dev mode
if ($env:SHADOW_DEV -eq "1") {
    Write-Host "  WARN: SHADOW_DEV=1 detected - clearing and rebuilding Release..." -ForegroundColor Yellow
    Remove-Item Env:\SHADOW_DEV -ErrorAction SilentlyContinue
    Push-Location $ProjectRoot
    cmake --build build --target ShadowLauncher --config Release 2>&1 | ForEach-Object {
        if ($_ -match "error|FAILED|fatal") { Write-Host "  $_" -ForegroundColor Red }
    }
    Pop-Location
    if (-not (Test-Path "$BuildDir\ShadowLauncher.exe")) {
        Write-Host "  FAIL: Release rebuild failed" -ForegroundColor Red
        exit 1
    }
    $exeTime = (Get-Item "$BuildDir\ShadowLauncher.exe").LastWriteTime.ToString("yyyy-MM-dd HH:mm")
    Write-Host "       rebuild done : $exeTime" -ForegroundColor Green
}

$binOk = $true
@("easytier-core.exe", "easytier-cli.exe", "wintun.dll") | ForEach-Object {
    if (-not (Test-Path "$ProjectRoot\build\Release\bin\$_")) {
        Write-Host "  WARN: Missing bin/$_ - multiplayer may break" -ForegroundColor Yellow
        $binOk = $false
    }
}
if ($binOk) { Write-Host "       EasyTier   : OK" -ForegroundColor Gray }
Write-Host ""

# ---- Step 1: Clean ----
Write-Host "[1/5] Cleaning old dist..." -ForegroundColor Yellow
if (Test-Path $DistDir) { Remove-Item -Recurse -Force $DistDir }
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
Write-Host ""

# ---- Step 2: Copy exe + updater ----
Write-Host "[2/5] Copying binary files..." -ForegroundColor Yellow
Copy-Item "$BuildDir\ShadowLauncher.exe" $DistDir
$up = "$BuildDir\SLUpdater.exe"
if (Test-Path $up) { Copy-Item $up $DistDir; Write-Host "       SLUpdater.exe" -ForegroundColor Gray }
else { Write-Host "       WARN: SLUpdater.exe not found" -ForegroundColor Yellow }
Write-Host ""

# ---- Step 3: windeployqt (flat to dist root) ----
Write-Host "[3/5] Running windeployqt..." -ForegroundColor Yellow
Push-Location $DistDir
$prevEAP = $ErrorActionPreference
$ErrorActionPreference = "Continue"
try {
    & $Windeployqt "ShadowLauncher.exe" `
        --qmldir "$ProjectRoot\qml" `
        --no-translations `
        --no-opengl-sw 2>&1 | ForEach-Object {
            if ($_ -match "Warning|error") { Write-Host "  $_" -ForegroundColor DarkGray }
        }
    Write-Host "       windeployqt done (flat)" -ForegroundColor Gray
}
finally {
    $ErrorActionPreference = $prevEAP
    Pop-Location
}
Write-Host ""

# ---- Step 4: Extra resources ----
Write-Host "[4/5] Copying extra resources..." -ForegroundColor Yellow

# 4a. MSVC CRT DLLs
# ⚠ 2026-08-12：漏 msvcp140_2.dll 曾导致内测报"找不到 MSVCP140_2.dll"
# （Qt6Gui.dll / Qt6Quick.dll 依赖它，exe 同目录必须携带）
# 重写：逐文件三源查找（build dir → VS redist → System32）——旧逻辑是
# "build dir 找到任意一个就整体跳过 redist"（$crtFound -eq 0 判断），
# 导致 build 里有 3 个 CRT 时 msvcp140_2.dll 永远漏拷（实锤：08-12 打包
# 日志无 msvcp140_2.dll 行）。System32 的版本为系统 VC++ redist 安装
# （14.5x，向后兼容 Qt 6.8.3 msvc2022 所需 14.3x+），可作最终兑底。
$crtDlls = @("vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll", "concrt140.dll")
$crtBase = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC"
$crtDir = Get-ChildItem "$crtBase\*\x64\Microsoft.VC143.CRT" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
foreach ($dll in $crtDlls) {
    $src = "$BuildDir\$dll"
    $from = ""
    if (Test-Path $src) {
        $from = "build dir"
    } elseif ($crtDir -and (Test-Path (Join-Path $crtDir.FullName $dll))) {
        $src = Join-Path $crtDir.FullName $dll
        $from = "VS redist"
    } else {
        $sysSrc = Join-Path $env:WINDIR "System32\$dll"
        if (Test-Path $sysSrc) {
            $src = $sysSrc
            $from = "System32"
        }
    }
    if ($from) {
        Copy-Item $src "$DistDir\$dll" -Force
        Write-Host "       $dll (from $from)" -ForegroundColor Gray
    } else {
        Write-Host "       WARN: $dll not found (build/redist/System32)" -ForegroundColor Yellow
    }
}

# 4b. Skins
if (Test-Path "$ProjectRoot\skins") {
    New-Item -ItemType Directory -Force -Path "$DistDir\skins" | Out-Null
    Copy-Item "$ProjectRoot\skins\*" "$DistDir\skins\" -Force
    $skinFiles = (Get-ChildItem "$DistDir\skins" -File).Count
    Write-Host "       skins/ : $skinFiles files" -ForegroundColor Gray
} else {
    Write-Host "       WARN: skins folder not found" -ForegroundColor Yellow
}

# 4c. versions.json (optional)
if (Test-Path "$ProjectRoot\package\versions.json") {
    Copy-Item "$ProjectRoot\package\versions.json" "$DistDir\versions.json" -Force
    Write-Host "       versions.json" -ForegroundColor Gray
}

# 4d. EasyTier — to root bin/
if (Test-Path "$ProjectRoot\build\Release\bin") {
    New-Item -ItemType Directory -Force -Path "$DistDir\bin" | Out-Null
    # Exclude *.bak (official-2.6.4 backups) — never ship them
    Get-ChildItem "$ProjectRoot\build\Release\bin\*" -File | Where-Object { $_.Extension -ne ".bak" } | Copy-Item -Destination "$DistDir\bin\" -Force
    $binFiles = (Get-ChildItem "$DistDir\bin" -File).Count
    $binSizeMB = [math]::Round((Get-ChildItem "$DistDir\bin" -Recurse -File | Measure-Object -Property Length -Sum).Sum / $OneMB, 1)
    Write-Host "       bin/ : $binFiles files, $binSizeMB MB" -ForegroundColor Gray
}

# 4f. wv2login.exe (WebView2 embedded-login helper, separate process)
$wv2 = "$ProjectRoot\thirdparty\wv2login\bin\x64\Release\wv2login.exe"
if (Test-Path $wv2) {
    Copy-Item $wv2 "$DistDir\wv2login.exe" -Force
    Copy-Item "$ProjectRoot\thirdparty\wv2login\bin\x64\Release\WebView2Loader.dll" "$DistDir\WebView2Loader.dll" -Force
    Write-Host "       wv2login.exe + WebView2Loader.dll" -ForegroundColor Gray
} else {
    Write-Host "       WARN: wv2login.exe not built - embedded login will fail!" -ForegroundColor Yellow
}

# 4g. QuickControls2 只留 Basic 风格（其余 5 套 + FluentWinUI3 冗余）
# 2026-08-11：只留 Basic 风格安全——main_release.cpp 已强制 QT_QUICK_CONTROLS_STYLE=Basic
# （Qt 6.7+ 默认解析 native Windows 风格且依赖 Fusion 等，不指定会报 module is not installed）
$keepQuickDlls = @("Qt6QuickControls2.dll", "Qt6QuickControls2Impl.dll", "Qt6QuickControls2Basic.dll", "Qt6QuickControls2BasicStyleImpl.dll")
Get-ChildItem "$DistDir\Qt6QuickControls2*.dll" -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -notin $keepQuickDlls } | Remove-Item -Force -ErrorAction SilentlyContinue
$qmlControlsDir = "$DistDir\qml\QtQuick\Controls"
if (Test-Path $qmlControlsDir) {
    Get-ChildItem $qmlControlsDir -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -notin @("Basic", "impl") } | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
}
# Dialogs quickimpl 下的冗余风格 qml（+Fusion/+Imagine/+Material/+Universal，项目全用 Basic）
$qmlDialogsDir = "$DistDir\qml\QtQuick\Dialogs\quickimpl\qml"
if (Test-Path $qmlDialogsDir) {
    Get-ChildItem $qmlDialogsDir -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "+*" } | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
}
Write-Host "       QuickControls2: 只留 Basic 风格 (-12MB)" -ForegroundColor Gray

# 4h. qmltooling 调试插件（发布不需要）
if (Test-Path "$DistDir\qmltooling") {
    Remove-Item "$DistDir\qmltooling" -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "       qmltooling: 调试插件已删 (-1MB)" -ForegroundColor Gray
}

# 4e. compat.json
Write-Host "[4e] Generating compat.json..." -ForegroundColor Yellow
$exePath = "$DistDir\ShadowLauncher.exe"
if (Test-Path $exePath) {
    $sha256 = (Get-FileHash -Path $exePath -Algorithm SHA256).Hash.ToLower()
    $compat = @{
        version           = $VersionTag
        update_mode       = "force_full"   # 全量更新：启动器删除除 .minecraft/logs/java_cache/agreement_consent.txt 外所有内容后覆盖
        force_reason      = ""
        qt_version        = "6.8.3"
        resource_epoch    = 1
        exe_sha256        = $sha256
        full_sha256       = ""
        build_date        = $BuildDate
        release_notes_url = "https://gitee.com/xiaole1173/shadow-launcher/releases"
    }
    $compatJson = $compat | ConvertTo-Json
    $compatJson | Out-File -FilePath "$DistDir\compat.json" -Encoding utf8 -Force
    Write-Host "       compat.json ($sha256)" -ForegroundColor Gray
} else {
    Write-Host "       WARN: ShadowLauncher.exe not found" -ForegroundColor Yellow
}
$etIncluded = if (Test-Path "$DistDir\bin\easytier-core.exe") { "included" } else { "MISSING" }
$buildInfo = "Shadow Launcher`r`n  Packed   : $BuildDate`r`n  Tag      : $VersionTag`r`n  Qt       : 6.8.3 (msvc2022_64)`r`n  EasyTier : $etIncluded`r`n  i18n     : embedded in QRC zh_CN zh_HK zh_TW`r`n"
$buildInfo | Out-File -FilePath "$DistDir\build_info.txt" -Encoding utf8
Write-Host "       build_info.txt" -ForegroundColor Gray
Write-Host ""

# ---- Step 5: Cleanup ----
Write-Host "[5/5] Cleaning junk..." -ForegroundColor Yellow

$junkPatterns = @(
    "*.log", "*.pdb", "*.ilk", "*.exp",
    "bootstrap.exe", "bootstrapc.exe", "cefsimple_copy.exe", "libcef.lib",
    "*.qmltypes",
    "launcher_profiles.json", "init_result.txt", "agreement_consent.txt",
    ".shadow_beta_key",
    "shadow_launcher_*.log", "neoforge-*.log",
    "installer.log", "debug.log"
)

$removed = 0
foreach ($pat in $junkPatterns) {
    Get-ChildItem $DistDir -Recurse -Filter $pat -ErrorAction SilentlyContinue | ForEach-Object {
        if ($_.FullName -match "\\.minecraft\\") { return }
        Remove-Item $_.FullName -Force -ErrorAction SilentlyContinue
        $removed++
    }
}
Write-Host "       removed $removed junk files" -ForegroundColor Gray

# ---- Summary ----
Write-Host ""
Write-Host "  ========================================" -ForegroundColor Green
Write-Host "   Pack Complete (flat)" -ForegroundColor Green
Write-Host "  ========================================" -ForegroundColor Green
Write-Host ""

$totalSizeMB  = [math]::Round((Get-ChildItem $DistDir -Recurse -File | Measure-Object -Property Length -Sum).Sum / $OneMB, 0)
$fileCount    = (Get-ChildItem $DistDir -Recurse -File).Count
$dllCount     = (Get-ChildItem $DistDir -Recurse -Filter "*.dll" -File).Count
$binDirSizeMB = if (Test-Path "$DistDir\bin") {
    [math]::Round((Get-ChildItem "$DistDir\bin" -Recurse -File | Measure-Object -Property Length -Sum).Sum / $OneMB, 1)
} else { 0 }

Write-Host "  Output : $DistDir" -ForegroundColor White
Write-Host "  Size   : $totalSizeMB MB  (bin: $binDirSizeMB MB)" -ForegroundColor White
Write-Host "  Files  : $fileCount  (DLLs: $dllCount)" -ForegroundColor White
Write-Host ""

# ---- 7-Zip：ZIP 格式（2026-08-11 起）----
# 全量更新包 .zip：启动器内置 miniz 解压（不需要用户装 7-Zip）。
# compat.json 排除出包（它引用 zip 的 SHA256，压缩后单独生成，发布时作为独立 asset 上传）。
$SevenZip = "C:\Program Files\7-Zip\7z.exe"
if (Test-Path $SevenZip) {
    Write-Host "  Compressing with 7-Zip (zip)..." -ForegroundColor Yellow
    $archive = "$ProjectRoot\dist\ShadowLauncher_$VersionTag.zip"
    # 包内必须含 ShadowLauncher.exe（全量更新/首次安装依赖；v0.4.2 曾用 -x! 排除导致全量更新后无主程序）
    & $SevenZip a -tzip -mx9 -mmt=on $archive "$DistDir\*" "-x!compat.json" 2>&1 | Select-Object -Last 1
    $archiveSizeMB = [math]::Round((Get-Item $archive).Length / $OneMB, 1)
    Write-Host "  Archive: $archive  ($archiveSizeMB MB)" -ForegroundColor Green

    # compat.json 在压缩后生成/更新：full_sha256 = zip 哈希（force_full 模式校验用）
    $zipSha = (Get-FileHash -Path $archive -Algorithm SHA256).Hash.ToLower()
    if (Test-Path "$DistDir\compat.json") {
        $compatObj = Get-Content "$DistDir\compat.json" -Raw | ConvertFrom-Json
        $compatObj.full_sha256 = $zipSha
        $compatObj | ConvertTo-Json | Out-File "$DistDir\compat.json" -Encoding utf8 -Force
        Write-Host "  compat.json full_sha256 = $($zipSha.Substring(0,16))..." -ForegroundColor Green
    } else {
        Write-Host "  WARN: compat.json 不存在，full_sha256 未填写" -ForegroundColor Yellow
    }
} else {
    Write-Host "  Tip: install 7-Zip for smaller archives" -ForegroundColor Gray
}

Write-Host ""
