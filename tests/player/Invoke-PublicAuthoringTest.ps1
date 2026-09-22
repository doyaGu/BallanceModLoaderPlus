# Runs one Native Mod through the public Command, DataShare, and Mod Menu C++
# facades in the real Ballance Player.
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
    $TestMod = Join-Path $releaseBin 'PublicAuthoringTest.bmodp'
}
if (-not $ArtifactsDirectory) {
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $ArtifactsDirectory = Join-Path $layout.RepoRoot `
        "build-dev\player-public-authoring-$timestamp"
}

$run = Invoke-BMLPlayerRun -BallanceRoot $BallanceRoot -LoaderDll $BuildDll `
    -Install @(
        @{ Source = $DriverMod
           Destination = 'ModLoader\Mods\PlayerFlowDriver.bmodp' },
        @{ Source = $TestMod
           Destination = 'ModLoader\Mods\PublicAuthoringTest.bmodp' }
    ) `
    -ArtifactsDirectory $ArtifactsDirectory -PlayerWidth $PlayerWidth `
    -PlayerHeight $PlayerHeight -TimeoutSeconds $TimeoutSeconds

$flow = Get-BMLPlayerFlowChecks -Run $run -Probes @('PublicAuthoringTest')
$checks = $flow.Checks
$log = $run.ModLoaderLog
$checks['PublicFacades'] = $log.Contains(
    'Public authoring load: status=pass command=true data_share=true page=true')
$checks['CommandDispatch'] = $log.Contains(
    'Public authoring command: argument=direct')
$checks['ExplicitCleanup'] = $log.Contains(
    'Public authoring unload: status=pass command=0 page=0')

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
    throw "Public authoring Player test failed: $($failedChecks -join ', ')"
}
