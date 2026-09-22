# Runs the shipped Player flow with the state round-trip probe installed. The
# probe captures the retail ball mid-motion, lets the simulation diverge, then
# restores the captured state and checks the simulation carries on.
[CmdletBinding()]
param(
    [string]$BallanceRoot = $env:BML_BALLANCE_ROOT,

    [string]$BuildDll,

    [string]$DriverMod,

    [string]$IvpMod,

    [string]$TestMod,

    [string]$ArtifactsDirectory,

    [ValidateRange(0, 16384)]
    [int]$PlayerWidth = 800,

    [ValidateRange(0, 16384)]
    [int]$PlayerHeight = 600,

    [ValidateRange(10, 600)]
    [int]$TimeoutSeconds = 180
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
if (-not $IvpMod) {
    $IvpMod = Join-Path $releaseBin 'IVP.bmodp'
}
if (-not $TestMod) {
    $TestMod = Join-Path $releaseBin 'IvpBallStateRoundTripTest.bmodp'
}
if (-not $ArtifactsDirectory) {
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $ArtifactsDirectory = Join-Path $layout.BuildRoot `
        "player-ivp-ball-state-round-trip-$timestamp"
}

$run = Invoke-BMLPlayerRun -BallanceRoot $BallanceRoot -LoaderDll $BuildDll `
    -Install @(
        @{ Source = $DriverMod
           Destination = 'ModLoader\Mods\PlayerFlowDriver.bmodp' },
        @{ Source = $IvpMod
           Destination = 'ModLoader\Mods\IVP.bmodp' },
        @{ Source = $TestMod
           Destination = 'ModLoader\Mods\IvpBallStateRoundTripTest.bmodp' }
    ) `
    -ArtifactsDirectory $ArtifactsDirectory -PlayerWidth $PlayerWidth `
    -PlayerHeight $PlayerHeight -TimeoutSeconds $TimeoutSeconds

$flow = Get-BMLPlayerFlowChecks -Run $run -Probes @('IvpBallStateRoundTripTest')
$checks = $flow.Checks
$probe = [regex]::Match($run.ModLoaderLog,
        'Ball state round-trip: status=(?<status>pass|fail) reason=(?<reason>\S+) ' +
        'ball=(?<ball>\S+) mapped=(?<mapped>true|false) ' +
        'moving_capture=(?<movingCapture>true|false) ' +
        'divergence=(?<divergence>true|false) position=(?<position>true|false) ' +
        'rotation=(?<rotation>true|false) ' +
        'linear_velocity=(?<linearVelocity>true|false) ' +
        'angular_velocity=(?<angularVelocity>true|false) ' +
        'simulation_continued=(?<simulationContinued>true|false)')
$checks['BallStateRoundTrip'] = $probe.Success -and
    $probe.Groups['status'].Value -eq 'pass' -and
    $probe.Groups['reason'].Value -eq 'complete' -and
    $probe.Groups['mapped'].Value -eq 'true' -and
    $probe.Groups['movingCapture'].Value -eq 'true' -and
    $probe.Groups['divergence'].Value -eq 'true' -and
    $probe.Groups['position'].Value -eq 'true' -and
    $probe.Groups['rotation'].Value -eq 'true' -and
    $probe.Groups['linearVelocity'].Value -eq 'true' -and
    $probe.Groups['angularVelocity'].Value -eq 'true' -and
    $probe.Groups['simulationContinued'].Value -eq 'true'

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
    IvpModHash = Get-BMLOptionalHash $IvpMod
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
    throw "Ball state round-trip test failed: $($failedChecks -join ', ')"
}
