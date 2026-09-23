# Confirms that a failed BMLEntry cannot leave callbacks into its unloaded DLL.
[CmdletBinding()]
param(
    [string]$BallanceRoot = $env:BML_BALLANCE_ROOT,
    [string]$BuildDll,
    [string]$DriverMod,
    [string]$SubjectMod,
    [string]$InspectorMod,
    [string]$ArtifactsDirectory,
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
    $SubjectMod = Join-Path $releaseBin 'RejectedEntryTest.bmodp'
}
if (-not $InspectorMod) {
    $InspectorMod = Join-Path $releaseBin 'RejectedEntryInspectorTest.bmodp'
}
if (-not $ArtifactsDirectory) {
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $ArtifactsDirectory = Join-Path $layout.RepoRoot "build-dev\player-rejected-entry-$timestamp"
}

$run = Invoke-BMLPlayerRun -BallanceRoot $BallanceRoot -LoaderDll $BuildDll `
    -Install @(
        @{ Source = $DriverMod
           Destination = 'ModLoader\Mods\PlayerFlowDriver.bmodp' },
        @{ Source = $SubjectMod
           Destination = 'ModLoader\Mods\RejectedEntryTest.bmodp' },
        @{ Source = $InspectorMod
           Destination = 'ModLoader\Mods\RejectedEntryInspectorTest.bmodp' }
    ) -ArtifactsDirectory $ArtifactsDirectory -PlayerWidth 800 `
    -PlayerHeight 600 -TimeoutSeconds $TimeoutSeconds

$flow = Get-BMLPlayerFlowChecks -Run $run -Probes @('__rejected_entry_no_probe__')
$checks = $flow.Checks
$checks.Remove('ProbesDiscovered')
$checks.Remove('Probe___rejected_entry_no_probe__')
$checks['NoUnexpectedProbes'] = $null -ne $flow.Flow -and $flow.Flow.Probes -eq 0
$checks['RejectedDllUnloaded'] = $run.ModLoaderLog.Contains('Rejected entry DLL: unloaded')
$checks['RejectedCommandRemoved'] = $run.ModLoaderLog.Contains('Rejected entry command: absent')
$checks['CallbackSelfUnregister'] = $run.ModLoaderLog.Contains('Callback self-unregister: succeeded')
$checks['CallbackReleasedAfterExecution'] = $run.ModLoaderLog.Contains('Callback command release after execution: yes')
$checks['StrictAliasConflict'] = $run.ModLoaderLog.Contains('Callback alias conflict: rejected')

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
    throw "Rejected entry Player test failed: $($failedChecks -join ', ')"
}
