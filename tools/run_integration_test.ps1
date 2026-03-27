<#
.SYNOPSIS
    Deploy, run, and collect in-game integration test results for BML.

.DESCRIPTION
    This script:
    1. Backs up existing BML.dll, ModLoader.dll, and module DLLs in the game directory
    2. Deploys freshly-built artifacts from the build tree
    3. Launches Player.exe
    4. Waits for IntegrationTestReport.json to appear
    5. Parses the report and outputs results
    6. Restores backups

.PARAMETER GameDir
    Path to the Ballance game installation. Default: C:\Users\kakut\Games\Ballance

.PARAMETER BuildDir
    Path to the CMake build directory. Default: <repo_root>\build

.PARAMETER Config
    Build configuration (Release/Debug/RelWithDebInfo). Default: Release

.PARAMETER TimeoutSeconds
    Max seconds to wait for the report file. Default: 120

.PARAMETER SkipBuild
    Skip the build step (assume artifacts are already built).

.PARAMETER NoRestore
    Don't restore backups after the test (useful for debugging).

.EXAMPLE
    .\tools\run_integration_test.ps1
    .\tools\run_integration_test.ps1 -GameDir "D:\Games\Ballance" -Config Debug
#>

param(
    [string]$GameDir = "C:\Users\kakut\Games\Ballance",
    [string]$BuildDir = "",
    [string]$Config = "Release",
    [int]$TimeoutSeconds = 120,
    [switch]$SkipBuild,
    [switch]$NoRestore
)

$ErrorActionPreference = "Stop"

# Resolve paths; script lives in <repo>/tools/
$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $RepoRoot) {
    $RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}
if (-not $BuildDir) {
    $BuildDir = Join-Path $RepoRoot "build"
}

$BinDir       = Join-Path $GameDir "Bin"
$BBDir        = Join-Path $GameDir "BuildingBlocks"
$ModsDir      = Join-Path $GameDir "Mods"
$ModLoaderDir = Join-Path $GameDir "ModLoader"
$ReportPath   = Join-Path $ModLoaderDir "IntegrationTestReport.json"
$BackupDir    = Join-Path $GameDir ".bml_test_backup"
$PlayerExe    = Join-Path $BinDir "Player.exe"

# Build artifacts
$BuildBML         = Join-Path $BuildDir "bin\$Config\BML.dll"
$BuildModLoader   = Join-Path $BuildDir "bin\$Config\ModLoader.dll"
$BuildModsDir     = Join-Path $BuildDir "Mods"

# Module list (all modules including integration test)
$Modules = @(
    "BML_Console", "BML_GameEvent", "BML_HUD", "BML_Input",
    "BML_MapMenu", "BML_ModMenu", "BML_ObjectLoad",
    "BML_Physics", "BML_Render", "BML_UI",
    "BML_IntegrationTest"
)

$ExpectedTests = @(
    "api_load",
    "module_context",
    "global_context",
    "runtime_version",
    "mod_id",
    "logger",
    "subscriptions",
    "engine_init_received",
    "extension_register",
    "extension_query",
    "extension_load_use",
    "extension_unload",
    "extension_unregister",
    "extension_query_peer",
    "config_roundtrip",
    "imc_self_pubsub_setup",
    "frame_preprocess",
    "frame_postprocess",
    "frame_prerender",
    "frame_postrender",
    "engine_play_received",
    "imc_self_pubsub_verify"
)

# ---------------------------------------------------------------------------
# Helper functions
# ---------------------------------------------------------------------------

function Write-Step($msg) {
    Write-Host ">> $msg" -ForegroundColor Cyan
}

function Write-Ok($msg) {
    Write-Host "   [OK] $msg" -ForegroundColor Green
}

function Write-Fail($msg) {
    Write-Host "   [FAIL] $msg" -ForegroundColor Red
}

function Write-Warn($msg) {
    Write-Host "   [WARN] $msg" -ForegroundColor Yellow
}

function Get-ReportSnapshot($Path, $ExpectedTests) {
    if (-not (Test-Path $Path)) {
        return $null
    }

    $report = Get-Content -Raw $Path | ConvertFrom-Json
    $testNames = @{}
    foreach ($test in @($report.tests)) {
        if ($null -ne $test.name) {
            $testNames[$test.name] = $true
        }
    }

    $missing = @()
    foreach ($name in $ExpectedTests) {
        if (-not $testNames.ContainsKey($name)) {
            $missing += $name
        }
    }

    $summary = $report.summary
    $complete = $false
    if ($null -ne $summary -and $null -ne $summary.complete) {
        $complete = [bool]$summary.complete
    }

    $requiredComplete = $false
    if ($null -ne $summary -and $null -ne $summary.required_complete) {
        $requiredComplete = [bool]$summary.required_complete
    }

    [pscustomobject]@{
        Report = $report
        MissingTests = $missing
        Complete = $complete
        RequiredComplete = $requiredComplete
    }
}

# ---------------------------------------------------------------------------
# Step 0: Validate prerequisites
# ---------------------------------------------------------------------------

Write-Step "Validating prerequisites"

if (-not (Test-Path $PlayerExe)) {
    throw "Player.exe not found at $PlayerExe"
}
if (-not (Test-Path $BuildDir)) {
    throw "Build directory not found at $BuildDir. Run cmake --build first."
}

# Check that build artifacts exist
if (-not $SkipBuild) {
    Write-Step "Building project (config=$Config)"
    cmake --build $BuildDir --config $Config 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }
    Write-Ok "Build succeeded"
}

if (-not (Test-Path $BuildBML)) {
    throw "BML.dll not found at $BuildBML"
}
if (-not (Test-Path $BuildModLoader)) {
    throw "ModLoader.dll not found at $BuildModLoader"
}

$IntTestDll = Join-Path $BuildModsDir "BML_IntegrationTest\$Config\BML_IntegrationTest.dll"
if (-not (Test-Path $IntTestDll)) {
    throw "BML_IntegrationTest.dll not found at $IntTestDll"
}

Write-Ok "All build artifacts found"

# ---------------------------------------------------------------------------
# Step 1: Backup existing files
# ---------------------------------------------------------------------------

Write-Step "Backing up existing files to $BackupDir"

if (Test-Path $BackupDir) {
    Remove-Item -Recurse -Force $BackupDir
}
New-Item -ItemType Directory -Path $BackupDir -Force | Out-Null

# Backup BML.dll
$srcBml = Join-Path $BinDir "BML.dll"
if (Test-Path $srcBml) {
    Copy-Item $srcBml (Join-Path $BackupDir "BML.dll")
}

# Backup ModLoader.dll
$srcML = Join-Path $BBDir "ModLoader.dll"
if (Test-Path $srcML) {
    Copy-Item $srcML (Join-Path $BackupDir "ModLoader.dll")
}

# Backup module DLLs
$backupModsDir = Join-Path $BackupDir "Mods"
New-Item -ItemType Directory -Path $backupModsDir -Force | Out-Null
foreach ($mod in $Modules) {
    $modDir = Join-Path $ModsDir $mod
    if (Test-Path $modDir) {
        Copy-Item -Recurse $modDir (Join-Path $backupModsDir $mod)
    }
}

Write-Ok "Backup complete"

# ---------------------------------------------------------------------------
# Step 2: Deploy build artifacts
# ---------------------------------------------------------------------------

Write-Step "Deploying build artifacts"

# Deploy BML.dll
Copy-Item -Force $BuildBML $srcBml
Write-Ok "BML.dll -> $BinDir"

# Deploy ModLoader.dll
Copy-Item -Force $BuildModLoader $srcML
Write-Ok "ModLoader.dll -> $BBDir"

# Deploy modules
foreach ($mod in $Modules) {
    $buildModDir = Join-Path $BuildModsDir "$mod\$Config"
    $destModDir  = Join-Path $ModsDir $mod

    if (-not (Test-Path $buildModDir)) {
        Write-Warn "Build output not found for $mod at $buildModDir, skipping"
        continue
    }

    if (-not (Test-Path $destModDir)) {
        New-Item -ItemType Directory -Path $destModDir -Force | Out-Null
    }

    # Copy DLL and mod.toml
    $dllFile = Get-ChildItem -Path $buildModDir -Filter "*.dll" | Select-Object -First 1
    if ($dllFile) {
        Copy-Item -Force $dllFile.FullName (Join-Path $destModDir $dllFile.Name)
    }
    $tomlFile = Join-Path $buildModDir "mod.toml"
    if (Test-Path $tomlFile) {
        Copy-Item -Force $tomlFile (Join-Path $destModDir "mod.toml")
    }
}
Write-Ok "All modules deployed"

# ---------------------------------------------------------------------------
# Step 3: Remove stale report
# ---------------------------------------------------------------------------

if (Test-Path $ReportPath) {
    Remove-Item -Force $ReportPath
}

# ---------------------------------------------------------------------------
# Step 4: Launch game
# ---------------------------------------------------------------------------

Write-Step "Launching Player.exe"
$StdoutLog = Join-Path $ModLoaderDir "stdout.log"
$StderrLog = Join-Path $ModLoaderDir "stderr.log"
$proc = Start-Process -FilePath $PlayerExe -WorkingDirectory $BinDir -PassThru `
    -RedirectStandardOutput $StdoutLog -RedirectStandardError $StderrLog

Write-Host "   PID: $($proc.Id)" -ForegroundColor Gray
Write-Host "   stdout -> $StdoutLog" -ForegroundColor Gray
Write-Host "   Waiting up to ${TimeoutSeconds}s for report..." -ForegroundColor Gray

# ---------------------------------------------------------------------------
# Step 5: Wait for report
# ---------------------------------------------------------------------------

$deadline = (Get-Date).AddSeconds($TimeoutSeconds)
$reportFound = $false
$reportReady = $false
$reportSnapshot = $null
$reportParseError = $null

while ((Get-Date) -lt $deadline) {
    if (Test-Path $ReportPath) {
        $reportFound = $true
        try {
            $snapshot = Get-ReportSnapshot -Path $ReportPath -ExpectedTests $ExpectedTests
            $reportSnapshot = $snapshot
            $reportParseError = $null

            if ($snapshot.Complete -and $snapshot.RequiredComplete -and $snapshot.MissingTests.Count -eq 0) {
                # Give a moment for the file to be fully written
                Start-Sleep -Milliseconds 500
                $reportReady = $true
                break
            }
        } catch {
            $reportParseError = $_.Exception.Message
        }
    }

    # Check if process exited early
    if ($proc.HasExited) {
        Write-Warn "Player.exe exited (code $($proc.ExitCode)) before a complete report was written"
        break
    }

    Start-Sleep -Seconds 1
}

# Kill game process if still running
if (-not $proc.HasExited) {
    Write-Step "Terminating Player.exe"
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
}

# ---------------------------------------------------------------------------
# Step 6: Parse and display results
# ---------------------------------------------------------------------------

$exitCode = 0

if (Test-Path $ReportPath) {
    try {
        $reportSnapshot = Get-ReportSnapshot -Path $ReportPath -ExpectedTests $ExpectedTests
    } catch {
        $reportParseError = $_.Exception.Message
    }
}

if (-not $reportReady) {
    if (-not $reportFound -and -not (Test-Path $ReportPath)) {
        Write-Fail "Report file not found at $ReportPath"
    } else {
        Write-Fail "Complete integration report was not produced"
    }
    Write-Host ""
    Write-Host "Possible causes:" -ForegroundColor Yellow
    Write-Host "  - The game exited before the final integration report was written"
    Write-Host "  - The report was only partially written or contained missing test items"
    Write-Host "  - ModLoader.dll failed to bootstrap"
    Write-Host ""
    if ($reportParseError) {
        Write-Host "Last report parse error:" -ForegroundColor Yellow
        Write-Host "  $reportParseError"
        Write-Host ""
    }
    if ($reportSnapshot) {
        Write-Host "Report status:" -ForegroundColor Yellow
        Write-Host "  complete=$($reportSnapshot.Complete) required_complete=$($reportSnapshot.RequiredComplete)"
        if ($reportSnapshot.MissingTests.Count -gt 0) {
            Write-Host "  missing tests: $($reportSnapshot.MissingTests -join ', ')"
        }
        Write-Host ""
    }
    Write-Host "Recent stdout.log:" -ForegroundColor Yellow
    if (Test-Path $StdoutLog) {
        Get-Content $StdoutLog | Select-Object -Last 50
    }
    if (Test-Path $StderrLog) {
        Write-Host ""
        Write-Host "Recent stderr.log:" -ForegroundColor Yellow
        Get-Content $StderrLog | Select-Object -Last 50
    }
    $exitCode = 2
} else {
    Write-Step "Parsing test report"
    $report = $reportSnapshot.Report

    Write-Host ""
    Write-Host "  BML Integration Test Report" -ForegroundColor White
    Write-Host "  Version:  $($report.version)" -ForegroundColor Gray
    Write-Host "  Duration: $([math]::Round($report.duration_ms, 0)) ms" -ForegroundColor Gray
    Write-Host ""

    $passCount = 0
    $failCount = 0

    foreach ($test in $report.tests) {
        if ($test.passed) {
            $passCount++
            $status = "PASS"
            $color = "Green"
        } else {
            $failCount++
            $status = "FAIL"
            $color = "Red"
        }

        $detail = ""
        if ($test.detail) {
            $detail = " ($($test.detail))"
        }
        Write-Host "  [$status] $($test.name)$detail" -ForegroundColor $color
    }

    Write-Host ""
    Write-Host "  Summary: $passCount passed, $failCount failed, $($report.summary.total) total" -ForegroundColor $(if ($failCount -eq 0) { "Green" } else { "Red" })
    Write-Host "  Completion: complete=$($report.summary.complete) required_complete=$($report.summary.required_complete) expected_total=$($report.summary.expected_total)" -ForegroundColor Gray
    Write-Host ""

    if ($failCount -gt 0 -or -not $report.summary.complete -or -not $report.summary.required_complete) {
        $exitCode = 1
    }
}

# ---------------------------------------------------------------------------
# Step 7: Restore backups
# ---------------------------------------------------------------------------

if (-not $NoRestore) {
    Write-Step "Restoring backups"

    # Restore BML.dll
    $bkBml = Join-Path $BackupDir "BML.dll"
    if (Test-Path $bkBml) {
        Copy-Item -Force $bkBml $srcBml
    }

    # Restore ModLoader.dll
    $bkML = Join-Path $BackupDir "ModLoader.dll"
    if (Test-Path $bkML) {
        Copy-Item -Force $bkML $srcML
    }

    # Restore modules
    foreach ($mod in $Modules) {
        $bkMod = Join-Path $backupModsDir $mod
        $destMod = Join-Path $ModsDir $mod

        if ($mod -eq "BML_IntegrationTest") {
            # Remove test module from game (it wasn't there before)
            if (Test-Path $destMod) {
                Remove-Item -Recurse -Force $destMod
            }
        } elseif (Test-Path $bkMod) {
            if (Test-Path $destMod) {
                Remove-Item -Recurse -Force $destMod
            }
            Copy-Item -Recurse $bkMod $destMod
        }
    }

    # Cleanup backup dir
    Remove-Item -Recurse -Force $BackupDir
    Write-Ok "Backups restored"
} else {
    Write-Warn "Skipping restore (--NoRestore). Backups at: $BackupDir"
}

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------

if ($exitCode -eq 0) {
    Write-Host "Integration test PASSED" -ForegroundColor Green
} elseif ($exitCode -eq 1) {
    Write-Host "Integration test FAILED (some tests failed)" -ForegroundColor Red
} else {
    Write-Host "Integration test ERROR (report not generated)" -ForegroundColor Red
}

exit $exitCode
