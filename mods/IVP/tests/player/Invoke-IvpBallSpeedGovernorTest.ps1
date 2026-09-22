# Runs the shipped Player flow with the speed governor probe installed. The
# probe drives the retail ball through the shipped Ball Navigation script and
# checks the IVP speed limit against real gameplay motion.
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
    $TestMod = Join-Path $releaseBin 'IvpBallSpeedGovernorTest.bmodp'
}
if (-not $ArtifactsDirectory) {
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $ArtifactsDirectory = Join-Path $layout.BuildRoot `
        "player-ivp-ball-speed-governor-$timestamp"
}

$run = Invoke-BMLPlayerRun -BallanceRoot $BallanceRoot -LoaderDll $BuildDll `
    -Install @(
        @{ Source = $DriverMod
           Destination = 'ModLoader\Mods\PlayerFlowDriver.bmodp' },
        @{ Source = $IvpMod
           Destination = 'ModLoader\Mods\IVP.bmodp' },
        @{ Source = $TestMod
           Destination = 'ModLoader\Mods\IvpBallSpeedGovernorTest.bmodp' }
    ) `
    -ArtifactsDirectory $ArtifactsDirectory -PlayerWidth $PlayerWidth `
    -PlayerHeight $PlayerHeight -TimeoutSeconds $TimeoutSeconds

$flow = Get-BMLPlayerFlowChecks -Run $run -Probes @('IvpBallSpeedGovernorTest')
$checks = $flow.Checks
$probe = [regex]::Match($run.ModLoaderLog,
        'Ball speed governor: status=(?<status>pass|fail) reason=(?<reason>\S+) ' +
        'ball=(?<ball>\S+) mapped=(?<mapped>true|false) ' +
        'rolling=(?<rolling>true|false) ' +
        'limit_applied=(?<limitApplied>true|false) ' +
        'limit_respected=(?<limitRespected>true|false) ' +
        'angular_coupled=(?<angularCoupled>true|false) ' +
        'controllable=(?<controllable>true|false) ' +
        'released_acceleration=(?<releasedAcceleration>true|false)')
$checks['BallSpeedGovernor'] = $probe.Success -and
    $probe.Groups['status'].Value -eq 'pass' -and
    $probe.Groups['reason'].Value -eq 'complete' -and
    $probe.Groups['mapped'].Value -eq 'true' -and
    $probe.Groups['rolling'].Value -eq 'true' -and
    $probe.Groups['limitApplied'].Value -eq 'true' -and
    $probe.Groups['limitRespected'].Value -eq 'true' -and
    $probe.Groups['angularCoupled'].Value -eq 'true' -and
    $probe.Groups['controllable'].Value -eq 'true' -and
    $probe.Groups['releasedAcceleration'].Value -eq 'true'

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
    throw "Ball speed governor test failed: $($failedChecks -join ', ')"
}
