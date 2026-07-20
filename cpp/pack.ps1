<#
.SYNOPSIS
    Shadow Launcher - One-click packager
    Copies build artifacts, runs windeployqt, bundles EasyTier, cleans up, compresses.
.NOTES
    i18n .qm files embedded in QRC via qt_add_translations; no manual deploy needed.
    EasyTier binaries go to bin/ subdirectory (matches easytier_process.cpp lookup path).
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
Write-Host "   Shadow Launcher Packager" -ForegroundColor Cyan
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

# Auto-handle dev mode: unset env var + rebuild as Release
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
    if (-not (Test-Path "$ProjectRoot\bin\$_")) {
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

# ---- Step 3: windeployqt ----
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
    Write-Host "       windeployqt done" -ForegroundColor Gray
}
finally {
    $ErrorActionPreference = $prevEAP
    Pop-Location
}
Write-Host ""

# ---- Step 4: Extra resources ----
Write-Host "[4/5] Copying extra resources..." -ForegroundColor Yellow

# 4a. MSVC CRT DLLs (manual copy from VS redist — windeployqt skips them)
$crtBase = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC"
$crtDir = Get-ChildItem "$crtBase\*\x64\Microsoft.VC143.CRT" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
if ($crtDir) {
    @("vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll", "concrt140.dll") | ForEach-Object {
        $src = Join-Path $crtDir.FullName $_
        if (Test-Path $src) {
            Copy-Item $src "$DistDir\$_" -Force
            Write-Host "       $_" -ForegroundColor Gray
        } else {
            Write-Host "       WARN: $_ not found in redist" -ForegroundColor Yellow
        }
    }
} else {
    Write-Host "       WARN: MSVC redist not found at $crtBase — app may crash on clean Windows" -ForegroundColor Yellow
}

# 4b. Skins (offline login head textures)
if (Test-Path "$ProjectRoot\skins") {
    New-Item -ItemType Directory -Force -Path "$DistDir\skins" | Out-Null
    Copy-Item "$ProjectRoot\skins\*" "$DistDir\skins\" -Force
    $skinFiles = (Get-ChildItem "$DistDir\skins" -File).Count
    Write-Host "       skins/ : $skinFiles files" -ForegroundColor Gray
    Get-ChildItem "$DistDir\skins" -File | ForEach-Object {
        Write-Host "         $($_.Name)" -ForegroundColor DarkGray
    }
} else {
    Write-Host "       WARN: skins folder not found at $ProjectRoot\skins" -ForegroundColor Yellow
}

# 4b. versions.json
if (Test-Path "$ProjectRoot\package\versions.json") {
    Copy-Item "$ProjectRoot\package\versions.json" "$DistDir\versions.json" -Force
    Write-Host "       versions.json" -ForegroundColor Gray
}

# 4c. EasyTier + WireGuard runtime
if (Test-Path "$ProjectRoot\bin") {
    New-Item -ItemType Directory -Force -Path "$DistDir\bin" | Out-Null
    Copy-Item "$ProjectRoot\bin\*" "$DistDir\bin\" -Recurse -Force
    $binFiles = (Get-ChildItem "$DistDir\bin" -File).Count
    $binSizeMB = [math]::Round((Get-ChildItem "$DistDir\bin" -Recurse -File | Measure-Object -Property Length -Sum).Sum / $OneMB, 1)
    Write-Host "       bin/ : $binFiles files, $binSizeMB MB" -ForegroundColor Gray
    Get-ChildItem "$DistDir\bin" -File | ForEach-Object {
        Write-Host "         $($_.Name)" -ForegroundColor DarkGray
    }
}

# 4d. Build info
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
        Remove-Item $_.FullName -Force -ErrorAction SilentlyContinue
        $removed++
    }
}
Write-Host "       removed $removed junk files" -ForegroundColor Gray

# ---- Summary ----
Write-Host ""
Write-Host "  ========================================" -ForegroundColor Green
Write-Host "   Pack Complete" -ForegroundColor Green
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

# ---- 7-Zip ----
$SevenZip = "C:\Program Files\7-Zip\7z.exe"
if (Test-Path $SevenZip) {
    Write-Host "  Compressing with 7-Zip..." -ForegroundColor Yellow
    $archive = "$ProjectRoot\dist\ShadowLauncher_$VersionTag.7z"
    & $SevenZip a -mx9 -mmt=on $archive $DistDir 2>&1 | Select-Object -Last 1
    $archiveSizeMB = [math]::Round((Get-Item $archive).Length / $OneMB, 1)
    Write-Host "  Archive: $archive  ($archiveSizeMB MB)" -ForegroundColor Green
} else {
    Write-Host "  Tip: install 7-Zip for smaller archives" -ForegroundColor Gray
}

Write-Host ""
