# Runs the shipped Player flow with the ExecuteBB probe installed. The probe
# physicalizes a body it owns itself, so it proves the ExecuteBB physics
# operations without depending on the retail ball.
[CmdletBinding()]
param(
    [string]$BallanceRoot = $env:BML_BALLANCE_ROOT,

    [string]$BuildDll,

    [string]$DriverMod,

    [string]$TestMod,

    [string]$ArtifactsDirectory,

    [ValidateRange(0, 16384)]
    [int]$PlayerWidth = 800,

    [ValidateRange(0, 16384)]
    [int]$PlayerHeight = 600,

    [ValidateRange(10, 600)]
    [int]$TimeoutSeconds = 120
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot '..\..\scripts\lib\BMLProject.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'BMLPlayerHarness.psm1') -Force

if (-not $BallanceRoot) {
    throw 'Ballance root is required. Pass -BallanceRoot or set BML_BALLANCE_ROOT.'
}

$layout = Get-BMLProjectLayout
if (-not $BuildDll) {
    $BuildDll = Join-Path $layout.DefaultReleaseBin 'BMLPlus.dll'
}
$releaseBin = Split-Path -Parent ([System.IO.Path]::GetFullPath($BuildDll))
if (-not $DriverMod) {
    $DriverMod = Join-Path $releaseBin 'PlayerFlowDriver.bmodp'
}
if (-not $TestMod) {
    $TestMod = Join-Path $releaseBin 'ExecuteBBTest.bmodp'
}
if (-not $ArtifactsDirectory) {
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $ArtifactsDirectory = Join-Path $layout.RepoRoot `
        "build-dev\player-executebb-$timestamp"
}

$run = Invoke-BMLPlayerRun -BallanceRoot $BallanceRoot -LoaderDll $BuildDll `
    -Install @(
        @{ Source = $DriverMod
           Destination = 'ModLoader\Mods\PlayerFlowDriver.bmodp' },
        @{ Source = $TestMod
           Destination = 'ModLoader\Mods\ExecuteBBTest.bmodp' }
    ) `
    -ArtifactsDirectory $ArtifactsDirectory -PlayerWidth $PlayerWidth `
    -PlayerHeight $PlayerHeight -TimeoutSeconds $TimeoutSeconds

$flow = Get-BMLPlayerFlowChecks -Run $run -Probes @('ExecuteBBTest')
$checks = $flow.Checks
$probe = [regex]::Match($run.ModLoaderLog,
        'ExecuteBB probe: status=(?<status>pass|fail) reason=(?<reason>\S+) ' +
        'x0=(?<x0>-?[0-9.]+) push_start=(?<pushStart>-?[0-9.]+) ' +
        'pushed=(?<pushed>-?[0-9.]+) pulled=(?<pulled>-?[0-9.]+) ' +
        'released=(?<released>-?[0-9.]+) ' +
        'physicalize_event=(?<physicalize>true|false) ' +
        'unphysicalize_event=(?<unphysicalize>true|false)')
$checks['ExecuteBBOperations'] = $probe.Success -and
    $probe.Groups['status'].Value -eq 'pass' -and
    $probe.Groups['reason'].Value -eq 'completed' -and
    $probe.Groups['physicalize'].Value -eq 'true' -and
    $probe.Groups['unphysicalize'].Value -eq 'true'
# The probe drives the operations through the published helpers, so the
# loader must never fall back to its own ExecuteBB diagnostics.
$checks['CleanExecuteBB'] = -not $run.ModLoaderLog.Contains('ExecuteBB::')

$failedChecks = @($checks.GetEnumerator() | Where-Object { -not $_.Value } |
    ForEach-Object Key)

$result = [pscustomobject]@{
    Status = $(if ($failedChecks.Count -eq 0) { 'pass' } else { 'fail' })
    BallanceRoot = $run.BallanceRoot
    Visible = $run.WindowActivated
    SetupDialogAccepted = $run.SetupDialogAccepted
    PlayerExitCode = $run.PlayerExitCode
    PlayerTimedOut = $run.TimedOut
    SourceHash = Get-BMLOptionalHash $BuildDll
    DriverModHash = Get-BMLOptionalHash $DriverMod
    TestModHash = Get-BMLOptionalHash $TestMod
    ArtifactsDirectory = $run.ArtifactsDirectory
    Screenshot = $run.Screenshot
    TutorialScreenshot = $run.TutorialScreenshot
    Trace = $run.Trace
    PlayerTrace = $run.PlayerTrace
    Flow = $flow.Flow
    Probes = $flow.Verdicts
    Checks = [pscustomobject]$checks
    FailedChecks = $failedChecks
}

$result
if ($failedChecks.Count -gt 0) {
    throw "ExecuteBB test failed: $($failedChecks -join ', ')"
}
