# Runs the shipped Player flow with the constraint probe installed. The probe
# anchors the retail ball with an IVP constraint, checks it holds under real
# input, then frees the translation axes and checks motion resumes.
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
    $TestMod = Join-Path $releaseBin 'IvpBallConstraintTest.bmodp'
}
if (-not $ArtifactsDirectory) {
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $ArtifactsDirectory = Join-Path $layout.BuildRoot `
        "player-ivp-ball-constraint-$timestamp"
}

$run = Invoke-BMLPlayerRun -BallanceRoot $BallanceRoot -LoaderDll $BuildDll `
    -Install @(
        @{ Source = $DriverMod
           Destination = 'ModLoader\Mods\PlayerFlowDriver.bmodp' },
        @{ Source = $IvpMod
           Destination = 'ModLoader\Mods\IVP.bmodp' },
        @{ Source = $TestMod
           Destination = 'ModLoader\Mods\IvpBallConstraintTest.bmodp' }
    ) `
    -ArtifactsDirectory $ArtifactsDirectory -PlayerWidth $PlayerWidth `
    -PlayerHeight $PlayerHeight -TimeoutSeconds $TimeoutSeconds

$flow = Get-BMLPlayerFlowChecks -Run $run -Probes @('IvpBallConstraintTest')
$checks = $flow.Checks
$probe = [regex]::Match($run.ModLoaderLog,
        'Ball constraint: status=(?<status>pass|fail) reason=(?<reason>\S+) ' +
        'ball=(?<ball>\S+) mapped=(?<mapped>true|false) ' +
        'created=(?<created>true|false) endpoints=(?<endpoints>true|false) ' +
        'cores=(?<cores>true|false) anchored=(?<anchored>true|false) ' +
        'axes_freed=(?<axesFreed>true|false) resumed=(?<resumed>true|false)')
$checks['BallConstraint'] = $probe.Success -and
    $probe.Groups['status'].Value -eq 'pass' -and
    $probe.Groups['reason'].Value -eq 'complete' -and
    $probe.Groups['mapped'].Value -eq 'true' -and
    $probe.Groups['created'].Value -eq 'true' -and
    $probe.Groups['endpoints'].Value -eq 'true' -and
    $probe.Groups['cores'].Value -eq 'true' -and
    $probe.Groups['anchored'].Value -eq 'true' -and
    $probe.Groups['axesFreed'].Value -eq 'true' -and
    $probe.Groups['resumed'].Value -eq 'true'

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
    throw "Ball constraint test failed: $($failedChecks -join ', ')"
}
