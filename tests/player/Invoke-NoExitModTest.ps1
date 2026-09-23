# Confirms that a native Mod without BMLExit receives OnUnload and does not
# prevent Player from exiting. The loader must not delete it across the DLL.
[CmdletBinding()]
param(
    [string]$BallanceRoot = $env:BML_BALLANCE_ROOT,
    [string]$BuildDll,
    [string]$DriverMod,
    [string]$SubjectMod,
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
if (-not $SubjectMod) {
    $SubjectMod = Join-Path $releaseBin 'NoExitModTest.bmodp'
}
if (-not $ArtifactsDirectory) {
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $ArtifactsDirectory = Join-Path $layout.RepoRoot "build-dev\player-no-exit-$timestamp"
}

$run = Invoke-BMLPlayerRun -BallanceRoot $BallanceRoot -LoaderDll $BuildDll `
    -Install @(
        @{ Source = $DriverMod
           Destination = 'ModLoader\Mods\PlayerFlowDriver.bmodp' },
        @{ Source = $SubjectMod
           Destination = 'ModLoader\Mods\NoExitModTest.bmodp' }
    ) -ArtifactsDirectory $ArtifactsDirectory -PlayerWidth $PlayerWidth `
    -PlayerHeight $PlayerHeight -TimeoutSeconds $TimeoutSeconds

$flow = Get-BMLPlayerFlowChecks -Run $run -Probes @('__no_exit_no_probe__')
$checks = $flow.Checks
$checks.Remove('ProbesDiscovered')
$checks.Remove('Probe___no_exit_no_probe__')
$checks['NoUnexpectedProbes'] = $null -ne $flow.Flow -and $flow.Flow.Probes -eq 0

$log = $run.ModLoaderLog
$loadIndex = $log.IndexOf('No-exit Mod: OnLoad')
$unloadIndex = $log.IndexOf('No-exit Mod: OnUnload')
$warningIndex = $log.IndexOf('Native Mod NoExitModTest does not export BMLExit')
$checks['NoExitTeardownOrder'] = $loadIndex -ge 0 -and
    $unloadIndex -gt $loadIndex -and $warningIndex -gt $unloadIndex

$failedChecks = @($checks.GetEnumerator() | Where-Object { -not $_.Value } |
    ForEach-Object Key)
$result = [pscustomobject]@{
    Status = $(if ($failedChecks.Count -eq 0) { 'pass' } else { 'fail' })
    PlayerExitCode = $run.PlayerExitCode
    PlayerTimedOut = $run.TimedOut
    ArtifactsDirectory = $run.ArtifactsDirectory
    Checks = [pscustomobject]$checks
    FailedChecks = $failedChecks
}

$result
if ($failedChecks.Count -gt 0) {
    throw "No-exit Mod Player test failed: $($failedChecks -join ', ')"
}
