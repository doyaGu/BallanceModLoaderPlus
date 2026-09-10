# Runs a real native provider and consumer pair in Ballance Player. The test
# covers cross-DLL lookup, provider authentication, explicit unregister, and
# loader-owned cleanup before the provider DLL is released.
[CmdletBinding()]
param(
    [string]$BallanceRoot = $env:BML_BALLANCE_ROOT,

    [string]$BuildDll,

    [string]$DriverMod,

    [string]$ProviderMod,

    [string]$ConsumerMod,

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
if (-not $ProviderMod) {
    $ProviderMod = Join-Path $releaseBin 'InterfaceProviderTest.bmodp'
}
if (-not $ConsumerMod) {
    $ConsumerMod = Join-Path $releaseBin 'InterfaceConsumerTest.bmodp'
}
if (-not $ArtifactsDirectory) {
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $ArtifactsDirectory = Join-Path $layout.RepoRoot `
        "build-dev\player-interface-provider-$timestamp"
}

$run = Invoke-BMLPlayerRun -BallanceRoot $BallanceRoot -LoaderDll $BuildDll `
    -Install @(
        @{ Source = $DriverMod
           Destination = 'ModLoader\Mods\PlayerFlowDriver.bmodp' },
        @{ Source = $ProviderMod
           Destination = 'ModLoader\Mods\InterfaceProviderTest.bmodp' },
        @{ Source = $ConsumerMod
           Destination = 'ModLoader\Mods\InterfaceConsumerTest.bmodp' }
    ) `
    -ArtifactsDirectory $ArtifactsDirectory -PlayerWidth $PlayerWidth `
    -PlayerHeight $PlayerHeight -TimeoutSeconds $TimeoutSeconds

$flow = Get-BMLPlayerFlowChecks -Run $run -Probes @('InterfaceConsumerTest')
$checks = $flow.Checks
$log = $run.ModLoaderLog

$providerLoad = [regex]::Match($log,
    'Interface provider load: status=(?<status>pass|fail) registered=(?<registered>true|false) ' +
    'callable=(?<callable>true|false) duplicate_rejected=(?<duplicate>true|false) ' +
    'owner_rejected=(?<owner>true|false) builtin_rejected=(?<builtin>true|false) ' +
    'stack_rejected=(?<stack>true|false) heap_rejected=(?<heap>true|false) ' +
    'thread_rejected=(?<thread>true|false) explicit_cycle=(?<explicit>true|false)')
$consumerLoad = [regex]::Match($log,
    'Interface consumer load: status=(?<status>pass|fail) callable=(?<callable>true|false) ' +
    'value=(?<value>-?[0-9]+) version_mismatch=(?<version>true|false) ' +
    'missing=(?<missing>true|false) explicit_missing=(?<explicit>true|false) ' +
    'unregister_denied=(?<unregister>true|false) spoof_denied=(?<spoof>true|false)')
$consumerUnload = [regex]::Match($log,
    'Interface consumer unload: status=(?<status>pass|fail) ' +
    'provider_present=(?<provider>true|false) cleanup_present=(?<cleanup>true|false) ' +
    'value=(?<value>-?[0-9]+)')
$providerUnload = [regex]::Match($log,
    'Interface provider unload: status=(?<status>pass|fail) ' +
    'cleanup_present=(?<cleanup>true|false) explicit_removed=(?<explicit>true|false)')
$providerDestroy = [regex]::Match($log,
    'Interface provider destroy: status=(?<status>pass|fail) ' +
    'primary_removed=(?<primary>true|false) cleanup_removed=(?<cleanup>true|false) ' +
    'primary_code=(?<primaryCode>-?[0-9]+) cleanup_code=(?<cleanupCode>-?[0-9]+)')

$checks['ProviderRegistration'] = $providerLoad.Success -and
    $providerLoad.Groups['status'].Value -eq 'pass' -and
    $providerLoad.Groups['registered'].Value -eq 'true' -and
    $providerLoad.Groups['callable'].Value -eq 'true' -and
    $providerLoad.Groups['duplicate'].Value -eq 'true' -and
    $providerLoad.Groups['owner'].Value -eq 'true' -and
    $providerLoad.Groups['builtin'].Value -eq 'true' -and
    $providerLoad.Groups['stack'].Value -eq 'true' -and
    $providerLoad.Groups['heap'].Value -eq 'true' -and
    $providerLoad.Groups['thread'].Value -eq 'true' -and
    $providerLoad.Groups['explicit'].Value -eq 'true'
$checks['ConsumerBinding'] = $consumerLoad.Success -and
    $consumerLoad.Groups['status'].Value -eq 'pass' -and
    $consumerLoad.Groups['callable'].Value -eq 'true' -and
    $consumerLoad.Groups['value'].Value -eq '42' -and
    $consumerLoad.Groups['version'].Value -eq 'true' -and
    $consumerLoad.Groups['missing'].Value -eq 'true' -and
    $consumerLoad.Groups['explicit'].Value -eq 'true' -and
    $consumerLoad.Groups['unregister'].Value -eq 'true' -and
    $consumerLoad.Groups['spoof'].Value -eq 'true'
$checks['DependencyUnloadOrder'] = $consumerUnload.Success -and
    $consumerUnload.Groups['status'].Value -eq 'pass' -and
    $consumerUnload.Groups['provider'].Value -eq 'true' -and
    $consumerUnload.Groups['cleanup'].Value -eq 'true' -and
    $consumerUnload.Groups['value'].Value -eq '42'
$checks['ExplicitUnregister'] = $providerUnload.Success -and
    $providerUnload.Groups['status'].Value -eq 'pass' -and
    $providerUnload.Groups['cleanup'].Value -eq 'true' -and
    $providerUnload.Groups['explicit'].Value -eq 'true'
$checks['AutomaticProviderCleanup'] = $providerDestroy.Success -and
    $providerDestroy.Groups['status'].Value -eq 'pass' -and
    $providerDestroy.Groups['primary'].Value -eq 'true' -and
    $providerDestroy.Groups['cleanup'].Value -eq 'true'

$providerLoadIndex = $log.IndexOf('Interface provider load:')
$consumerLoadIndex = $log.IndexOf('Interface consumer load:')
$consumerUnloadIndex = $log.IndexOf('Interface consumer unload:')
$providerUnloadIndex = $log.IndexOf('Interface provider unload:')
$providerDestroyIndex = $log.IndexOf('Interface provider destroy:')
$checks['LifecycleLogOrder'] = $providerLoadIndex -ge 0 -and
    $consumerLoadIndex -gt $providerLoadIndex -and
    $consumerUnloadIndex -gt $consumerLoadIndex -and
    $providerUnloadIndex -gt $consumerUnloadIndex -and
    $providerDestroyIndex -gt $providerUnloadIndex

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
    ProviderModHash = Get-BMLOptionalHash $ProviderMod
    ConsumerModHash = Get-BMLOptionalHash $ConsumerMod
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
    throw "Interface provider test failed: $($failedChecks -join ', ')"
}
