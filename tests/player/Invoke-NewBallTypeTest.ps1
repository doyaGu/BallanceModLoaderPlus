# Runs Level 01 with BallSticky selected through AllLevel.StartBall. The test
# exercises NewBallType's public registration path and its Behavior-authored
# Gameplay patch without directly replacing CurrentLevel or activating an
# internal Building Block.
[CmdletBinding()]
param(
    [string]$BallanceRoot = $env:BML_BALLANCE_ROOT,

    [string]$BuildDll,

    [string]$DriverMod,

    [string]$TestMod,

    [string]$BallAssetsRoot,

    [string]$BmlConfig,

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
    $TestMod = Join-Path $releaseBin 'NewBallTypeTest.bmodp'
}
if (-not $BallAssetsRoot) {
    $BallAssetsRoot = Join-Path (Split-Path -Parent $layout.RepoRoot) `
        'BallanceMods\BallSticky\3D Entities'
}
if (-not $BmlConfig) {
    $BmlConfig = Join-Path $PSScriptRoot 'BehaviorAcceptance.cfg'
}
if (-not $ArtifactsDirectory) {
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $ArtifactsDirectory = Join-Path $layout.RepoRoot `
        "build-dev\player-new-ball-$timestamp"
}

$ball = Join-Path $BallAssetsRoot 'Ball_Sticky.nmo'
$physicsBall = Join-Path $BallAssetsRoot 'PH\P_Ball_Sticky.nmo'
$transformer = Join-Path $BallAssetsRoot 'PH\P_Trafo_Sticky.nmo'
foreach ($required in @($BuildDll, $DriverMod, $TestMod, $BmlConfig,
                         $ball, $physicsBall, $transformer)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "New-ball acceptance input is missing: $required"
    }
}

$nativeArtifacts = @($BuildDll, $DriverMod, $TestMod)
$debugArtifacts = @($nativeArtifacts | Where-Object {
    [System.IO.Path]::GetFullPath($_) -match '(?i)(^|[\\/])Debug([\\/]|$)'
})
if ($debugArtifacts.Count -gt 0) {
    throw "New-ball Player acceptance requires Release or RelWithDebInfo artifacts: $($debugArtifacts[0])"
}

$install = @(
    @{ Source = $DriverMod
       Destination = 'ModLoader\Mods\PlayerFlowDriver.bmodp' },
    @{ Source = $TestMod
       Destination = 'ModLoader\Mods\NewBallTypeTest.bmodp' },
    @{ Source = $ball
       Destination = '3D Entities\Ball_Sticky.nmo' },
    @{ Source = $physicsBall
       Destination = '3D Entities\PH\P_Ball_Sticky.nmo' },
    @{ Source = $transformer
       Destination = '3D Entities\PH\P_Trafo_Sticky.nmo' },
    @{ Source = $BmlConfig
       Destination = 'ModLoader\Configs\BML.cfg' }
)

$run = Invoke-BMLPlayerRun -BallanceRoot $BallanceRoot -LoaderDll $BuildDll `
    -Install $install `
    -Environment @{ BML_PLAYER_DISABLE_ANGELSCRIPT = '1' } `
    -WindowCaptures ([ordered]@{
        'BallSticky' = 'New ball type: status=pass '
    }) `
    -ArtifactsDirectory $ArtifactsDirectory -PlayerWidth $PlayerWidth `
    -PlayerHeight $PlayerHeight -TimeoutSeconds $TimeoutSeconds

$log = $run.ModLoaderLog
$flow = Get-BMLPlayerFlowChecks -Run $run -Probes @('NewBallTypeTest')
$checks = $flow.Checks
$newBall = [regex]::Match(
    $log,
    'New ball type: status=(?<status>pass|fail) reason=(?<reason>\S+) ' +
    'level=(?<level>true|false) active_ball=(?<ball>\S+) ' +
    'ball_table=(?<ballTable>true|false) ' +
    'module_table=(?<moduleTable>true|false) ' +
    'module_group=(?<moduleGroup>true|false) graph=(?<graph>true|false)')
$checks['NewBallType'] = $newBall.Success -and
    $newBall.Groups['status'].Value -eq 'pass' -and
    $newBall.Groups['level'].Value -eq 'true' -and
    $newBall.Groups['ball'].Value -eq 'Ball_Sticky' -and
    $newBall.Groups['ballTable'].Value -eq 'true' -and
    $newBall.Groups['moduleTable'].Value -eq 'true' -and
    $newBall.Groups['moduleGroup'].Value -eq 'true' -and
    $newBall.Groups['graph'].Value -eq 'true'
$checks['BallStickyVisible'] = $run.Captures.BallSticky.Captured -and
    (Test-ImageDimensions -Path $run.Captures.BallSticky.Path `
        -Width $PlayerWidth -Height $PlayerHeight)
$failedChecks = @($checks.GetEnumerator() | Where-Object { -not $_.Value } |
    ForEach-Object Key)
$result = [pscustomobject]@{
    Status = $(if ($failedChecks.Count -eq 0) { 'pass' } else { 'fail' })
    BallanceRoot = $run.BallanceRoot
    Visible = $run.WindowActivated
    PlayerExitCode = $run.PlayerExitCode
    PlayerTimedOut = $run.TimedOut
    SourceHash = Get-BMLOptionalHash $BuildDll
    TestModHash = Get-BMLOptionalHash $TestMod
    BallAssetHash = Get-BMLOptionalHash $ball
    PhysicsBallHash = Get-BMLOptionalHash $physicsBall
    TransformerHash = Get-BMLOptionalHash $transformer
    ArtifactsDirectory = $run.ArtifactsDirectory
    Screenshot = $run.Screenshot
    BallStickyScreenshot = $run.Captures.BallSticky.Path
    Trace = $run.Trace
    PlayerTrace = $run.PlayerTrace
    Flow = $flow.Flow
    Probes = $flow.Verdicts
    Checks = [pscustomobject]$checks
    FailedChecks = $failedChecks
}

$result
if ($failedChecks.Count -gt 0) {
    throw "New-ball acceptance test failed: $($failedChecks -join ', ')"
}
