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
New-Item -ItemType Directory -Force -Path "$DistDir\launcher" | Out-Null
$up = "$BuildDir\SLUpdater.exe"
if (Test-Path $up) { Copy-Item $up "$DistDir\launcher\"; Write-Host "       SLUpdater.exe" -ForegroundColor Gray }
else { Write-Host "       WARN: SLUpdater.exe not found" -ForegroundColor Yellow }
# qt.conf — tells Qt to look for plugins/qml in launcher/ subdir
Copy-Item "$ProjectRoot\qt.conf" "$DistDir\qt.conf" -Force
Write-Host "       qt.conf" -ForegroundColor Gray
Write-Host ""
# ---- Step 3: windeployqt (deploy to launcher/ subdir) ----
Write-Host "[3/5] Running windeployqt..." -ForegroundColor Yellow
Push-Location $DistDir
$prevEAP = $ErrorActionPreference
$ErrorActionPreference = "Continue"
try {
    & $Windeployqt "ShadowLauncher.exe" `
        --dir launcher `
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

# 4a. MSVC CRT DLLs — check build dir first (user-placed), then VS redist
$crtDlls = @("vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll", "concrt140.dll")
$crtFound = 0
foreach ($dll in $crtDlls) {
    $src = "$BuildDir\$dll"
    if (Test-Path $src) {
        Copy-Item $src "$DistDir\launcher\$dll" -Force
        Write-Host "       $dll (from build dir)" -ForegroundColor Gray
        $crtFound++
    }
}
if ($crtFound -eq 0) {
    # Fallback: try VS redist
    $crtBase = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC"
    $crtDir = Get-ChildItem "$crtBase\*\x64\Microsoft.VC143.CRT" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($crtDir) {
        foreach ($dll in $crtDlls) {
            $src = Join-Path $crtDir.FullName $dll
            if (Test-Path $src) {
                Copy-Item $src "$DistDir\launcher\$dll" -Force
                Write-Host "       $dll (from VS redist)" -ForegroundColor Gray
                $crtFound++
            }
        }
    }
}
if ($crtFound -eq 0) {
    Write-Host "       WARN: VC++ CRT DLLs not found — app may crash on clean Windows" -ForegroundColor Yellow
    Write-Host "         Place them in $BuildDir or install VS redist" -ForegroundColor DarkGray
}

# 4b. Skins (offline login head textures) — to launcher/skins
if (Test-Path "$ProjectRoot\skins") {
    New-Item -ItemType Directory -Force -Path "$DistDir\launcher\skins" | Out-Null
    Copy-Item "$ProjectRoot\skins\*" "$DistDir\launcher\skins\" -Force
    $skinFiles = (Get-ChildItem "$DistDir\launcher\skins" -File).Count
    Write-Host "       skins/ : $skinFiles files" -ForegroundColor Gray
} else {
    Write-Host "       WARN: skins folder not found at $ProjectRoot\skins" -ForegroundColor Yellow
}

# 4c. versions.json (optional)
if (Test-Path "$ProjectRoot\package\versions.json") {
    Copy-Item "$ProjectRoot\package\versions.json" "$DistDir\launcher\versions.json" -Force
    Write-Host "       versions.json" -ForegroundColor Gray
} else {
    Write-Host "       versions.json: not found (optional, no package/ folder)" -ForegroundColor DarkGray
}

# 4d. EasyTier — to launcher/bin
if (Test-Path "$ProjectRoot\build\Release\bin") {
    New-Item -ItemType Directory -Force -Path "$DistDir\launcher\bin" | Out-Null
    Copy-Item "$ProjectRoot\build\Release\bin\*" "$DistDir\launcher\bin\" -Recurse -Force
    $binFiles = (Get-ChildItem "$DistDir\launcher\bin" -File).Count
    $binSizeMB = [math]::Round((Get-ChildItem "$DistDir\launcher\bin" -Recurse -File | Measure-Object -Property Length -Sum).Sum / $OneMB, 1)
    Write-Host "       bin/ : $binFiles files, $binSizeMB MB" -ForegroundColor Gray
}

# 4e. compat.json (update system metadata)
Write-Host ""
Write-Host "[4e] Generating compat.json..." -ForegroundColor Yellow
$exePath = "$DistDir\ShadowLauncher.exe"
if (Test-Path $exePath) {
    $sha256 = (Get-FileHash -Path $exePath -Algorithm SHA256).Hash.ToLower()
    $compat = @{
        version           = $VersionTag
        update_mode       = "exe"
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
    Write-Host "       WARN: ShadowLauncher.exe not found in dist, skip compat.json" -ForegroundColor Yellow
}
$etIncluded = if (Test-Path "$DistDir\launcher\bin\easytier-core.exe") { "included" } else { "MISSING" }
$buildInfo = "Shadow Launcher`r`n  Packed   : $BuildDate`r`n  Tag      : $VersionTag`r`n  Qt       : 6.8.3 (msvc2022_64)`r`n  EasyTier : $etIncluded`r`n  i18n     : embedded in QRC zh_CN zh_HK zh_TW`r`n"
$buildInfo | Out-File -FilePath "$DistDir\launcher\build_info.txt" -Encoding utf8
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
        # Don't delete inside .minecraft (user data)
        if ($_.FullName -match "\\.minecraft\\") { return }
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
$binDirSizeMB = if (Test-Path "$DistDir\launcher\bin") {
    [math]::Round((Get-ChildItem "$DistDir\launcher\bin" -Recurse -File | Measure-Object -Property Length -Sum).Sum / $OneMB, 1)
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
    & $SevenZip a -mx9 -mmt=on $archive $DistDir `
        -x!ShadowLauncher.exe 2>&1 | Select-Object -Last 1
    $archiveSizeMB = [math]::Round((Get-Item $archive).Length / $OneMB, 1)
    Write-Host "  Archive: $archive  ($archiveSizeMB MB)" -ForegroundColor Green
} else {
    Write-Host "  Tip: install 7-Zip for smaller archives" -ForegroundColor Gray
}

Write-Host ""
