# Runs the shipped Player flow with the Level_01 route probe installed. The
# route itself is under review, so the probe reports SKIPPED and this runner
# only proves the driver flow and the input plumbing the route will need.
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
    [int]$TimeoutSeconds = 300
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
    $TestMod = Join-Path $releaseBin 'GameplayRouteTest.bmodp'
}
if (-not $ArtifactsDirectory) {
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $ArtifactsDirectory = Join-Path $layout.RepoRoot `
        "build-dev\player-gameplay-route-$timestamp"
}

$run = Invoke-BMLPlayerRun -BallanceRoot $BallanceRoot -LoaderDll $BuildDll `
    -Install @(
        @{ Source = $DriverMod
           Destination = 'ModLoader\Mods\PlayerFlowDriver.bmodp' },
        @{ Source = $TestMod
           Destination = 'ModLoader\Mods\GameplayRouteTest.bmodp' }
    ) `
    -ArtifactsDirectory $ArtifactsDirectory -PlayerWidth $PlayerWidth `
    -PlayerHeight $PlayerHeight -TimeoutSeconds $TimeoutSeconds

$flow = Get-BMLPlayerFlowChecks -Run $run -Probes @('GameplayRouteTest')
$checks = $flow.Checks
$probe = [regex]::Match($run.ModLoaderLog,
        'Gameplay route: status=(?<status>pass|fail|skip) reason=(?<reason>\S+) ' +
        'input_applied=(?<inputApplied>true|false) motion=(?<motion>true|false) ' +
        'route=(?<route>true|false) checkpoint=(?<checkpoint>true|false) ' +
        'extra_life=(?<extraLife>true|false) extra_points=(?<extraPoints>[0-9]+) ' +
        'waypoint=(?<waypoint>[0-9]+) commanded_travel=(?<travel>-?[0-9.]+)')
# The authored route is still under review, so a skipped verdict is the expected
# outcome. Once the probe turns the pilot back on, this check tightens to the
# passing route instead.
$checks['GameplayRoute'] = $probe.Success -and
    (($probe.Groups['status'].Value -eq 'skip' -and
      $probe.Groups['reason'].Value -eq 'route-under-review' -and
      $run.ModLoaderLog.Contains(
          'Gameplay pilot: skipped=true reason=route-under-review')) -or
     ($probe.Groups['status'].Value -eq 'pass' -and
      $probe.Groups['inputApplied'].Value -eq 'true' -and
      $probe.Groups['motion'].Value -eq 'true' -and
      $probe.Groups['route'].Value -eq 'true'))

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
    throw "Gameplay route test failed: $($failedChecks -join ', ')"
}
