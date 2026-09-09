# Runs the generated native and AngelScript facades from one .imc contract in
# the original Ballance Player. RPC and topic traffic must work both ways.
[CmdletBinding()]
param(
    [string]$BallanceRoot = $env:BML_BALLANCE_ROOT,
    [string]$BuildDll,
    [string]$DriverMod,
    [string]$NativeMod,
    [string]$AngelScriptDll = $env:BML_CKANGELSCRIPT_DLL,
    [string]$ScriptMod,
    [string]$ScriptFacade,
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
if (-not $NativeMod) {
    $NativeMod = Join-Path $releaseBin 'ScriptImcInteropTest.bmodp'
}
if (-not $AngelScriptDll) {
    $worksRoot = Split-Path -Parent (Split-Path -Parent $layout.RepoRoot)
    $AngelScriptDll = Join-Path $worksRoot `
        'Virtools\CKAngelScript\build\src\Release\AngelScript.dll'
}
if (-not $ScriptMod) {
    $ScriptMod = Join-Path $PSScriptRoot 'ScriptImcInterop.mod.as'
}
if (-not $ScriptFacade) {
    $ScriptFacade = Join-Path $layout.RepoRoot `
        'build-dev\tests\player\script-imc\test_scriptinterop_imc.as'
}
if (-not $ArtifactsDirectory) {
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $ArtifactsDirectory = Join-Path $layout.RepoRoot `
        "build-dev\player-script-imc-$timestamp"
}

$nativeArtifacts = @($BuildDll, $DriverMod, $NativeMod, $AngelScriptDll)
$debugArtifact = $nativeArtifacts | Where-Object {
    [System.IO.Path]::GetFullPath($_) -match '(?i)(^|[\\/])Debug([\\/]|$)'
} | Select-Object -First 1
if ($debugArtifact) {
    throw "Script IMC Player test requires Release or RelWithDebInfo artifacts; Debug artifact: $debugArtifact"
}

# A single-file script Mod reads companion sources from a directory matching
# its stem. The harness restores every file; this runner only creates/removes
# the empty container when the install did not already have one.
$scriptResourceDirectory = Join-Path ([System.IO.Path]::GetFullPath($BallanceRoot)) `
    'ModLoader\Mods\ScriptImcInterop'
$createdResourceDirectory = -not (Test-Path -LiteralPath $scriptResourceDirectory)
if ($createdResourceDirectory) {
    New-Item -ItemType Directory -Path $scriptResourceDirectory | Out-Null
}

try {
    $run = Invoke-BMLPlayerRun -BallanceRoot $BallanceRoot -LoaderDll $BuildDll `
        -Install @(
            @{ Source = $DriverMod
               Destination = 'ModLoader\Mods\PlayerFlowDriver.bmodp' },
            @{ Source = $NativeMod
               Destination = 'ModLoader\Mods\ScriptImcInteropTest.bmodp' },
            @{ Source = $AngelScriptDll
               Destination = 'BuildingBlocks\AngelScript.dll' },
            @{ Source = $ScriptMod
               Destination = 'ModLoader\Mods\ScriptImcInterop.mod.as' },
            @{ Source = $ScriptFacade
               Destination = 'ModLoader\Mods\ScriptImcInterop\test_scriptinterop_imc.as' }
        ) `
        -ArtifactsDirectory $ArtifactsDirectory -PlayerWidth $PlayerWidth `
        -PlayerHeight $PlayerHeight -TimeoutSeconds $TimeoutSeconds
} finally {
    if ($createdResourceDirectory -and
        (Test-Path -LiteralPath $scriptResourceDirectory)) {
        Remove-Item -LiteralPath $scriptResourceDirectory
    }
}

$flow = Get-BMLPlayerFlowChecks -Run $run -Probes @('ScriptImcInteropTest')
$checks = $flow.Checks
$log = $run.ModLoaderLog
$checks['BindingsRegistered'] = $log.Contains('Registered BML AngelScript bindings') -and
    -not $log.Contains('BML AngelScript bindings unavailable') -and
    -not $log.Contains('Failed to register BML AngelScript')
$checks['ScriptLoaded'] = $log.Contains('BML script mod summary: loaded=1 failed=0') -and
    -not $log.Contains('Script mod bml.script.imc.interop failed:')
$checks['NativeToScript'] = $log.Contains(
    'Script IMC native interop: status=pass rpc_client=true rpc_provider=true topic_subscriber=true topic_publisher=true')
$checks['ScriptToNative'] = $log.Contains(
    'Script IMC interop: status=pass rpc_client=true rpc_provider=true topic_subscriber=true topic_publisher=true script_loopback=true handles=true')
$checks['ProviderClosed'] = $log.Contains(
    'Script IMC interop unload: provider_closed=true subscription_cancelled=true')
$checks['InstallRestored'] = $run.InstallRestored

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
    NativeModHash = Get-BMLOptionalHash $NativeMod
    AngelScriptHash = Get-BMLOptionalHash $AngelScriptDll
    ScriptModHash = Get-BMLOptionalHash $ScriptMod
    ScriptFacadeHash = Get-BMLOptionalHash $ScriptFacade
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
    throw "Script IMC interop test failed: $($failedChecks -join ', ')"
}
