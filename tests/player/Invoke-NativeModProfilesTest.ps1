# Runs the projects produced by bml.py in the real Ballance
# Player. The projects themselves come from NativeModProfilesIntegrationTest.
[CmdletBinding()]
param(
    [string]$BallanceRoot = $env:BML_BALLANCE_ROOT,

    [string]$BuildDll,

    [string]$DriverMod,

    [string]$ProfileBuildRoot,

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
    $BuildDll = Join-Path $layout.RepoRoot 'build-dev\bin\RelWithDebInfo\BMLPlus.dll'
}
if (-not $DriverMod) {
    $DriverMod = Join-Path $layout.RepoRoot `
        'build-dev\bin\RelWithDebInfo\PlayerFlowDriver.bmodp'
}
if (-not $ProfileBuildRoot) {
    $ProfileBuildRoot = Join-Path $layout.RepoRoot `
        'build-dev\tests\native-mod-profiles-RelWithDebInfo'
}
if (-not $ArtifactsDirectory) {
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $ArtifactsDirectory = Join-Path $layout.RepoRoot `
        "build-dev\player-native-profiles-$timestamp"
}

$basicMod = Join-Path $ProfileBuildRoot 'basic-install\Mods\BasicMod.bmodp'
$providerMod = Join-Path $ProfileBuildRoot `
    'provider-install\Mods\ValueProviderMod.bmodp'
$consumerMod = Join-Path $ProfileBuildRoot `
    'consumer-install\Mods\ValueConsumerMod.bmodp'
$imcMod = Join-Path $ProfileBuildRoot 'imc-install\Mods\RemoteApiMod.bmodp'

$run = Invoke-BMLPlayerRun -BallanceRoot $BallanceRoot -LoaderDll $BuildDll `
    -Install @(
        @{ Source = $DriverMod
           Destination = 'ModLoader\Mods\PlayerFlowDriver.bmodp' },
        @{ Source = $basicMod
           Destination = 'ModLoader\Mods\BasicMod.bmodp' },
        @{ Source = $providerMod
           Destination = 'ModLoader\Mods\ValueProviderMod.bmodp' },
        @{ Source = $consumerMod
           Destination = 'ModLoader\Mods\ValueConsumerMod.bmodp' },
        @{ Source = $imcMod
           Destination = 'ModLoader\Mods\RemoteApiMod.bmodp' }
    ) `
    -ArtifactsDirectory $ArtifactsDirectory -PlayerWidth $PlayerWidth `
    -PlayerHeight $PlayerHeight -TimeoutSeconds $TimeoutSeconds

$flow = Get-BMLPlayerFlowChecks -Run $run -Probes @('__native_profile_no_probe__')
$checks = $flow.Checks
$checks.Remove('ProbesDiscovered')
$checks.Remove('Probe___native_profile_no_probe__')
$checks['NoUnexpectedProbes'] = $null -ne $flow.Flow -and $flow.Flow.Probes -eq 0
$log = $run.ModLoaderLog

$checks['BasicCommandCleanup'] = $log.Contains(
    'Native profile basic: command_cleanup_status=0')
$checks['InterfacePublished'] = $log.Contains(
    'Native profile interface provider: publication_status=0')
$checks['InterfaceConsumed'] = $log.Contains(
    'Native profile interface consumer: open_status=0 read_status=0 value=42')
$checks['InterfaceReset'] = $log.Contains(
    'Native profile interface consumer: reset=true')
$checks['InterfaceCleaned'] = $log.Contains(
    'Native profile interface provider: cleanup_status=0')
$checks['ImcStarted'] = $log.Contains(
    'Native profile IMC provider: start_status=0')
$checks['ImcCleaned'] = $log.Contains(
    'Native profile IMC provider: cleanup_status=0')

$providerLoadIndex = $log.IndexOf(
    'Native profile interface provider: publication_status=0')
$consumerLoadIndex = $log.IndexOf(
    'Native profile interface consumer: open_status=0 read_status=0 value=42')
$consumerUnloadIndex = $log.IndexOf(
    'Native profile interface consumer: reset=true')
$providerUnloadIndex = $log.IndexOf(
    'Native profile interface provider: cleanup_status=0')
$checks['InterfaceLifecycleOrder'] = $providerLoadIndex -ge 0 -and
    $consumerLoadIndex -gt $providerLoadIndex -and
    $consumerUnloadIndex -gt $consumerLoadIndex -and
    $providerUnloadIndex -gt $consumerUnloadIndex

$failedChecks = @($checks.GetEnumerator() | Where-Object { -not $_.Value } |
    ForEach-Object Key)
$result = [pscustomobject]@{
    Status = $(if ($failedChecks.Count -eq 0) { 'pass' } else { 'fail' })
    BallanceRoot = $run.BallanceRoot
    PlayerExitCode = $run.PlayerExitCode
    PlayerTimedOut = $run.TimedOut
    ArtifactsDirectory = $run.ArtifactsDirectory
    Screenshot = $run.Screenshot
    Trace = $run.Trace
    Flow = $flow.Flow
    Checks = [pscustomobject]$checks
    FailedChecks = $failedChecks
}

$result
if ($failedChecks.Count -gt 0) {
    throw "Native Mod profiles Player test failed: $($failedChecks -join ', ')"
}
