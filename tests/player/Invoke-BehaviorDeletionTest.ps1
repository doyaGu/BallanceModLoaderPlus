# Checks native object deletion through public APIs, without the private
# Behavior test interface or fixture DLLs in the loader under test.
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
    $SubjectMod = Join-Path $releaseBin 'BehaviorDeletionTest.bmodp'
}
if (-not $ArtifactsDirectory) {
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $ArtifactsDirectory = Join-Path $layout.RepoRoot "build-dev\player-behavior-deletion-$timestamp"
}

foreach ($artifact in @($BuildDll, $DriverMod, $SubjectMod)) {
    if ([System.IO.Path]::GetFullPath($artifact) -match '(?i)(^|[\\/])Debug([\\/]|$)') {
        throw "Behavior Player acceptance requires Release or RelWithDebInfo native artifacts; Debug artifact: $artifact"
    }
}

$run = Invoke-BMLPlayerRun -BallanceRoot $BallanceRoot -LoaderDll $BuildDll `
    -Install @(
        @{ Source = $DriverMod
           Destination = 'ModLoader\Mods\PlayerFlowDriver.bmodp' },
        @{ Source = $SubjectMod
           Destination = 'ModLoader\Mods\BehaviorDeletionTest.bmodp' },
        @{ Source = (Join-Path $PSScriptRoot 'BehaviorAcceptance.cfg')
           Destination = 'ModLoader\Configs\BML.cfg' }
    ) -Environment @{ BML_PLAYER_RETURN_TO_MENU = '1' } `
    -ArtifactsDirectory $ArtifactsDirectory -PlayerWidth $PlayerWidth `
    -PlayerHeight $PlayerHeight -TimeoutSeconds $TimeoutSeconds

$flow = Get-BMLPlayerFlowChecks -Run $run -Probes @('BehaviorDeletionTest')
$checks = $flow.Checks
$checks['DeletionCompleted'] = $flow.Verdicts.Contains('BehaviorDeletionTest') -and
    $flow.Verdicts['BehaviorDeletionTest'].State -eq 'pass' -and
    $flow.Verdicts['BehaviorDeletionTest'].Started -and
    $flow.Verdicts['BehaviorDeletionTest'].Detail -eq 'complete'
$checks['ReturnedToMenu'] = $run.ModLoaderLog.Contains('Player return: requested=true') -and
    $run.ModLoaderLog.Contains('Player return: completed=true')
$checks['CleanDeletion'] = $run.ModLoaderLog -notmatch '\[(?:[^\]]+/)?ERROR\]|Exception'
$failedChecks = @($checks.GetEnumerator() | Where-Object { -not $_.Value } |
    ForEach-Object Key)
$result = [pscustomobject]@{
    Status = $(if ($failedChecks.Count -eq 0) { 'pass' } else { 'fail' })
    PlayerExitCode = $run.PlayerExitCode
    PlayerTimedOut = $run.TimedOut
    ArtifactsDirectory = $run.ArtifactsDirectory
    Flow = $flow.Flow
    Checks = [pscustomobject]$checks
    FailedChecks = $failedChecks
}

$result
if ($failedChecks.Count -gt 0) {
    throw "Behavior deletion Player test failed: $($failedChecks -join ', ')"
}
