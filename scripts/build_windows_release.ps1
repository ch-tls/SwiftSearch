<#
.SYNOPSIS
    SwiftSearch — Windows Release Build + Package Script

.DESCRIPTION
    Configures, builds, tests, and packages SwiftSearch for Windows (MSVC).
    Produces NSIS installer and ZIP archive via CPack.

.PARAMETER BuildDir
    Build output directory (default: "cmake-build-release")

.PARAMETER Config
    Build configuration (default: "Release")

.PARAMETER SkipTests
    Skip running unit tests after build

.PARAMETER SkipPackage
    Skip CPack packaging step

.EXAMPLE
    .\scripts\build_windows_release.ps1

.EXAMPLE
    .\scripts\build_windows_release.ps1 -SkipTests -SkipPackage

.NOTES
    Prerequisites:
      - Visual Studio 2022 (with C++ workload)
      - CMake >= 3.16
      - Qt6 (Core, Gui, Widgets, Sql, Concurrent)
      - NSIS (for .exe installer packaging)
#>

param(
    [string]$BuildDir = "cmake-build-release",
    [string]$Config = "Release",
    [switch]$SkipTests,
    [switch]$SkipPackage
)

$ErrorActionPreference = "Stop"
$ProjectDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$BuildPath = Join-Path $ProjectDir $BuildDir
$Jobs = $env:NUMBER_OF_PROCESSORS

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " SwiftSearch — Windows Release Build" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Build dir : $BuildPath"
Write-Host " Config    : $Config"
Write-Host " Jobs      : $Jobs"
Write-Host " Tests     : $(if ($SkipTests) { 'NO' } else { 'YES' })"
Write-Host " Package   : $(if ($SkipPackage) { 'NO' } else { 'YES' })"
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# ── Configure ──
Write-Host "[1/4] Configuring..." -ForegroundColor Yellow
cmake -B "$BuildPath" -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_BUILD_TYPE="$Config" `
  -DBUILD_TESTS=ON `
  -DSWIFTSEARCH_WERROR=OFF

if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

# ── Build ──
Write-Host ""
Write-Host "[2/4] Building ($Jobs jobs)..." -ForegroundColor Yellow
cmake --build "$BuildPath" --config "$Config" -j $Jobs

if ($LASTEXITCODE -ne 0) { throw "Build failed" }

# ── Test ──
if (-not $SkipTests) {
    Write-Host ""
    Write-Host "[3/4] Running tests..." -ForegroundColor Yellow
    ctest --test-dir "$BuildPath" -C "$Config" --output-on-failure

    if ($LASTEXITCODE -ne 0) { throw "Tests failed" }
}

# ── Package ──
if (-not $SkipPackage) {
    Write-Host ""
    Write-Host "[4/4] Packaging (NSIS + ZIP)..." -ForegroundColor Yellow
    Push-Location "$BuildPath"

    $archiveName = "SwiftSearch-$Config-windows-x64"
    cpack -G NSIS -C "$Config" 2>&1
    cpack -G ZIP -C "$Config" 2>&1

    Pop-Location

    Write-Host "  -> $BuildPath/SwiftSearch-*-win64.exe" -ForegroundColor Green
    Write-Host "  -> $BuildPath/SwiftSearch-*-win64.zip" -ForegroundColor Green
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Release build complete." -ForegroundColor Green
Write-Host " Binary: $BuildPath\src\$Config\SwiftSearch.exe" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
