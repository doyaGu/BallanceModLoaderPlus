[CmdletBinding()]
param(
    [string]$BallanceRoot = $env:BML_BALLANCE_ROOT,

    [string]$BuildDll,

    [string]$TestMod,

    [ValidateRange(10, 600)]
    [int]$TimeoutSeconds = 120
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot '..\..\scripts\lib\BMLProject.psm1') -Force

function Copy-TestFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,

        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    for ($attempt = 1; $attempt -le 10; ++$attempt) {
        try {
            Copy-Item -LiteralPath $Source -Destination $Destination -Force
            return
        } catch {
            if ($attempt -eq 10) {
                throw
            }
            Start-Sleep -Milliseconds 500
        }
    }
}

if (-not $BallanceRoot) {
    throw 'Ballance root is required. Pass -BallanceRoot or set BML_BALLANCE_ROOT.'
}

$layout = Get-BMLProjectLayout
if (-not $BuildDll) {
    $BuildDll = Join-Path $layout.DefaultReleaseBin 'BMLPlus.dll'
}
if (-not $TestMod) {
    $TestMod = Join-Path $layout.RepoRoot 'build-dev\bin\RelWithDebInfo\ExecuteBBTest.bmodp'
}

$ballanceRootFull = [System.IO.Path]::GetFullPath($BallanceRoot)
$playerPath = Join-Path $ballanceRootFull 'Bin\Player.exe'
$installedDll = Join-Path $ballanceRootFull 'BuildingBlocks\BMLPlus.dll'
$installedTestMod = Join-Path $ballanceRootFull 'ModLoader\Mods\ExecuteBBTest.bmodp'
$modLoaderLog = Join-Path $ballanceRootFull 'ModLoader\ModLoader.log'
$playerLog = Join-Path $ballanceRootFull 'Bin\Player.log'

foreach ($path in @($ballanceRootFull, $playerPath, $BuildDll, $TestMod)) {
    Assert-BMLPath -Path $path -Type $(if ($path -eq $ballanceRootFull) { 'Container' } else { 'Leaf' })
}

$targetPlayer = [System.IO.Path]::GetFullPath($playerPath)
$runningTarget = @(Get-Process -Name Player -ErrorAction SilentlyContinue | Where-Object {
    try {
        [System.IO.Path]::GetFullPath($_.Path) -eq $targetPlayer
    } catch {
        $false
    }
})
if ($runningTarget.Count -gt 0) {
    throw "Ballance Player is already running from $targetPlayer. Close it before running the test."
}

$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$artifacts = @(
    [pscustomobject]@{ Path = $installedDll; Backup = "$installedDll.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $installedTestMod; Backup = "$installedTestMod.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $modLoaderLog; Backup = "$modLoaderLog.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $playerLog; Backup = "$playerLog.test-bak-$timestamp" }
)
$process = $null
$playerExitCode = $null
$timedOut = $false
$testLog = ''
$playerRunLog = ''
$restored = $false
$sourceHash = Get-BMLOptionalHash $BuildDll
$testModHash = Get-BMLOptionalHash $TestMod
$installedHashBefore = Get-BMLOptionalHash $installedDll
$installedTestModHashBefore = Get-BMLOptionalHash $installedTestMod
$modLoaderLogHashBefore = Get-BMLOptionalHash $modLoaderLog
$playerLogHashBefore = Get-BMLOptionalHash $playerLog

try {
    foreach ($artifact in $artifacts) {
        if (Test-Path -LiteralPath $artifact.Path) {
            Copy-TestFile -Source $artifact.Path -Destination $artifact.Backup
        }
    }

    Copy-TestFile -Source $BuildDll -Destination $installedDll
    Copy-TestFile -Source $TestMod -Destination $installedTestMod
    foreach ($logPath in @($modLoaderLog, $playerLog)) {
        if (Test-Path -LiteralPath $logPath) {
            Remove-Item -LiteralPath $logPath -Force
        }
    }

    $process = Start-Process -FilePath $playerPath `
        -WorkingDirectory (Split-Path -Parent $playerPath) -PassThru

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while (-not $process.HasExited -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 100
        $process.Refresh()
    }
    if (-not $process.HasExited) {
        $timedOut = $true
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
    $playerExitCode = $process.ExitCode
    $testLog = [string]::Join("`n", @(Get-BMLTextIfExists $modLoaderLog))
    $playerRunLog = [string]::Join("`n", @(Get-BMLTextIfExists $playerLog))
} finally {
    if ($null -ne $process) {
        try {
            $process.Refresh()
            if (-not $process.HasExited) {
                Stop-Process -Id $process.Id -Force
                $process.WaitForExit()
            }
        } catch {
        }
    }

    foreach ($artifact in $artifacts) {
        if (Test-Path -LiteralPath $artifact.Backup) {
            Copy-TestFile -Source $artifact.Backup -Destination $artifact.Path
            Remove-Item -LiteralPath $artifact.Backup -Force
        } else {
            if (Test-Path -LiteralPath $artifact.Path) {
                Remove-Item -LiteralPath $artifact.Path -Force
            }
        }
    }
    $restored = $true
}

$outcomePattern = 'ExecuteBB test: status=(?<status>pass|fail) reason=(?<reason>\S+) x0=(?<x0>-?[0-9.]+) push_start=(?<pushStart>-?[0-9.]+) pushed=(?<pushed>-?[0-9.]+) pulled=(?<pulled>-?[0-9.]+) released=(?<released>-?[0-9.]+) physicalize_event=(?<physicalize>true|false) unphysicalize_event=(?<unphysicalize>true|false) menu_opened=(?<menuOpened>true|false) level_chosen=(?<levelChosen>true|false) control_ready=(?<controlReady>true|false) runtime_probe=(?<runtimeProbe>true|false) runtime_detail=(?<runtimeDetail>\S+) frames=(?<frames>[0-9]+)'
$outcome = [regex]::Match($testLog, $outcomePattern)
$postStartIndex = $testLog.IndexOf('On Message PostStartMenu')
$preLoadIndex = $testLog.IndexOf('On Message PreLoadLevel')
$postLoadIndex = $testLog.IndexOf('On Message PostLoadLevel')
$startLevelIndex = $testLog.IndexOf('On Message StartLevel')
$naturalLevelFlow = $postStartIndex -ge 0 -and $preLoadIndex -gt $postStartIndex -and
                    $postLoadIndex -gt $preLoadIndex -and $startLevelIndex -gt $postLoadIndex
$checks = [ordered]@{
    MenuFlow = $outcome.Success -and
        $outcome.Groups['menuOpened'].Value -eq 'true' -and
        $outcome.Groups['levelChosen'].Value -eq 'true'
    TestPassed = $outcome.Success -and $outcome.Groups['status'].Value -eq 'pass'
    RuntimeProbe = $outcome.Success -and
        $outcome.Groups['runtimeProbe'].Value -eq 'true' -and
        $outcome.Groups['runtimeDetail'].Value -eq 'complete'
    ExitCallback = $testLog.Contains('ExecuteBB test exit: status=pass')
    CleanExecuteBB = -not $testLog.Contains('ExecuteBB::')
    CleanPostProcess = -not $playerRunLog.Contains('Error : PostProcess')
    CleanModLoad = -not $testLog.Contains('Failed to load ')
    CleanShutdown = $testLog.Contains('Goodbye!')
    NaturalLevelFlow = $naturalLevelFlow
    PlayerExited = -not $timedOut -and $playerExitCode -eq 0
    InstallRestored = $restored -and
        (Get-BMLOptionalHash $installedDll) -eq $installedHashBefore -and
        (Get-BMLOptionalHash $installedTestMod) -eq $installedTestModHashBefore -and
        (Get-BMLOptionalHash $modLoaderLog) -eq $modLoaderLogHashBefore -and
        (Get-BMLOptionalHash $playerLog) -eq $playerLogHashBefore
}
$failedChecks = @($checks.GetEnumerator() | Where-Object { -not $_.Value } | ForEach-Object Key)

$result = [pscustomobject]@{
    Status = $(if ($failedChecks.Count -eq 0) { 'pass' } else { 'fail' })
    BallanceRoot = $ballanceRootFull
    Visible = $true
    PlayerExitCode = $playerExitCode
    PlayerTimedOut = $timedOut
    SourceHash = $sourceHash
    TestModHash = $testModHash
    InstalledHashBefore = $installedHashBefore
    InstalledHashAfter = Get-BMLOptionalHash $installedDll
    InstalledTestModHashBefore = $installedTestModHashBefore
    InstalledTestModHashAfter = Get-BMLOptionalHash $installedTestMod
    Checks = [pscustomobject]$checks
    FailedChecks = $failedChecks
    Outcome = $(if ($outcome.Success) {
        [pscustomobject]@{
            Reason = $outcome.Groups['reason'].Value
            InitialX = [float]$outcome.Groups['x0'].Value
            PushStartX = [float]$outcome.Groups['pushStart'].Value
            PushedX = [float]$outcome.Groups['pushed'].Value
            PulledX = [float]$outcome.Groups['pulled'].Value
            ReleasedX = [float]$outcome.Groups['released'].Value
            PhysicalizeEvent = $outcome.Groups['physicalize'].Value -eq 'true'
            UnphysicalizeEvent = $outcome.Groups['unphysicalize'].Value -eq 'true'
            LevelMenuOpened = $outcome.Groups['menuOpened'].Value -eq 'true'
            LevelChosen = $outcome.Groups['levelChosen'].Value -eq 'true'
            ControlReady = $outcome.Groups['controlReady'].Value -eq 'true'
            RuntimeProbe = $outcome.Groups['runtimeProbe'].Value -eq 'true'
            RuntimeDetail = $outcome.Groups['runtimeDetail'].Value
            Frames = [int]$outcome.Groups['frames'].Value
        }
    } else { $null })
}

$result
if ($failedChecks.Count -gt 0) {
    throw "ExecuteBB test failed: $($failedChecks -join ', ')"
}
