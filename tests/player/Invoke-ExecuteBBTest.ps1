[CmdletBinding()]
param(
    [string]$BallanceRoot = $env:BML_BALLANCE_ROOT,

    [string]$BuildDll,

    [string]$TestMod,

    [string]$RuntimeSemanticsMod,

    [string]$FixtureDll,

    [string]$TransportMod,

    [string]$PatchMod,

    [string]$TransportFixture,

    [string]$ScriptMod,

    [string]$ArtifactsDirectory,

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
$releaseBin = Split-Path -Parent ([System.IO.Path]::GetFullPath($BuildDll))
if (-not $TestMod) {
    $TestMod = Join-Path $releaseBin 'ExecuteBBTest.bmodp'
}
if (-not $RuntimeSemanticsMod) {
    $RuntimeSemanticsMod = Join-Path $releaseBin 'BehaviorRuntimeSemanticsTest.bmodp'
}
if (-not $FixtureDll) {
    $FixtureDll = Join-Path $releaseBin 'BehaviorLifecycleFixture.dll'
}
if (-not $TransportMod) {
    $TransportMod = Join-Path $releaseBin 'BehaviorTransportTest.bmodp'
}
if (-not $PatchMod) {
    $PatchMod = Join-Path $releaseBin 'BehaviorPatchTest.bmodp'
}
if (-not $TransportFixture) {
    $TransportFixture = Join-Path $releaseBin 'BehaviorTransportFixture.dll'
}
if (-not $ScriptMod) {
    $ScriptMod = Join-Path $PSScriptRoot 'BehaviorLifecycleScript.mod.as'
}

$ballanceRootFull = [System.IO.Path]::GetFullPath($BallanceRoot)
$playerPath = Join-Path $ballanceRootFull 'Bin\Player.exe'
$installedDll = Join-Path $ballanceRootFull 'BuildingBlocks\BMLPlus.dll'
$installedTestMod = Join-Path $ballanceRootFull 'ModLoader\Mods\ExecuteBBTest.bmodp'
$installedRuntimeSemanticsMod = Join-Path $ballanceRootFull 'ModLoader\Mods\BehaviorRuntimeSemanticsTest.bmodp'
$installedFixture = Join-Path $ballanceRootFull 'BuildingBlocks\BehaviorLifecycleFixture.dll'
$installedTransportMod = Join-Path $ballanceRootFull 'ModLoader\Mods\BehaviorTransportTest.bmodp'
$installedPatchMod = Join-Path $ballanceRootFull 'ModLoader\Mods\BehaviorPatchTest.bmodp'
$installedTransportFixture = Join-Path $ballanceRootFull 'BuildingBlocks\BehaviorTransportFixture.dll'
$installedScriptMod = Join-Path $ballanceRootFull 'ModLoader\Mods\BehaviorLifecycleScript.mod.as'
$modLoaderLog = Join-Path $ballanceRootFull 'ModLoader\ModLoader.log'
$playerLog = Join-Path $ballanceRootFull 'Bin\Player.log'

foreach ($path in @($ballanceRootFull, $playerPath, $BuildDll, $TestMod,
                     $RuntimeSemanticsMod, $FixtureDll, $TransportMod, $PatchMod,
                     $TransportFixture,
                     $ScriptMod)) {
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
if (-not $ArtifactsDirectory) {
    $ArtifactsDirectory = Join-Path $layout.RepoRoot "build-dev\player-lifecycle-$timestamp"
}
$artifactsDirectoryFull = [System.IO.Path]::GetFullPath($ArtifactsDirectory)
New-Item -ItemType Directory -Path $artifactsDirectoryFull -Force | Out-Null
$screenshotPath = Join-Path $artifactsDirectoryFull 'Player-window.png'
$framePath = Join-Path $artifactsDirectoryFull 'Player-frame.bmp'
$tracePath = Join-Path $artifactsDirectoryFull 'ModLoader-trace.log'
$playerTracePath = Join-Path $artifactsDirectoryFull 'Player-trace.log'
$artifacts = @(
    [pscustomobject]@{ Path = $installedDll; Backup = "$installedDll.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $installedTestMod; Backup = "$installedTestMod.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $installedRuntimeSemanticsMod; Backup = "$installedRuntimeSemanticsMod.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $installedFixture; Backup = "$installedFixture.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $installedTransportMod; Backup = "$installedTransportMod.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $installedPatchMod; Backup = "$installedPatchMod.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $installedTransportFixture; Backup = "$installedTransportFixture.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $installedScriptMod; Backup = "$installedScriptMod.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $modLoaderLog; Backup = "$modLoaderLog.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $playerLog; Backup = "$playerLog.test-bak-$timestamp" }
)
$process = $null
$playerExitCode = $null
$timedOut = $false
$testLog = ''
$playerRunLog = ''
$restored = $false
$screenshotCaptured = $false
$playerWindowActivated = $false
$windowShell = $null
$sourceHash = Get-BMLOptionalHash $BuildDll
$testModHash = Get-BMLOptionalHash $TestMod
$runtimeSemanticsModHash = Get-BMLOptionalHash $RuntimeSemanticsMod
$installedHashBefore = Get-BMLOptionalHash $installedDll
$installedTestModHashBefore = Get-BMLOptionalHash $installedTestMod
$installedRuntimeSemanticsModHashBefore = Get-BMLOptionalHash $installedRuntimeSemanticsMod
$installedFixtureHashBefore = Get-BMLOptionalHash $installedFixture
$installedTransportModHashBefore = Get-BMLOptionalHash $installedTransportMod
$installedPatchModHashBefore = Get-BMLOptionalHash $installedPatchMod
$installedTransportFixtureHashBefore = Get-BMLOptionalHash $installedTransportFixture
$installedScriptModHashBefore = Get-BMLOptionalHash $installedScriptMod
$modLoaderLogHashBefore = Get-BMLOptionalHash $modLoaderLog
$playerLogHashBefore = Get-BMLOptionalHash $playerLog
$previousFramePath = [Environment]::GetEnvironmentVariable(
    'BML_PLAYER_FRAME_PATH', 'Process')

try {
    [Environment]::SetEnvironmentVariable(
        'BML_PLAYER_FRAME_PATH', $framePath, 'Process')
    foreach ($artifact in $artifacts) {
        if (Test-Path -LiteralPath $artifact.Path) {
            Copy-TestFile -Source $artifact.Path -Destination $artifact.Backup
        }
    }

    Copy-TestFile -Source $BuildDll -Destination $installedDll
    Copy-TestFile -Source $TestMod -Destination $installedTestMod
    Copy-TestFile -Source $RuntimeSemanticsMod -Destination $installedRuntimeSemanticsMod
    Copy-TestFile -Source $FixtureDll -Destination $installedFixture
    Copy-TestFile -Source $TransportMod -Destination $installedTransportMod
    Copy-TestFile -Source $PatchMod -Destination $installedPatchMod
    Copy-TestFile -Source $TransportFixture -Destination $installedTransportFixture
    Copy-TestFile -Source $ScriptMod -Destination $installedScriptMod
    foreach ($logPath in @($modLoaderLog, $playerLog)) {
        if (Test-Path -LiteralPath $logPath) {
            Remove-Item -LiteralPath $logPath -Force
        }
    }

    $process = Start-Process -FilePath $playerPath `
        -WorkingDirectory (Split-Path -Parent $playerPath) -PassThru

    $windowShell = New-Object -ComObject WScript.Shell
    for ($attempt = 0; $attempt -lt 100 -and -not $process.HasExited; ++$attempt) {
        Start-Sleep -Milliseconds 100
        $process.Refresh()
        if ($process.MainWindowHandle -ne [IntPtr]::Zero -and
            $windowShell.AppActivate($process.Id)) {
            $playerWindowActivated = $true
            break
        }
    }

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
    if (Test-Path -LiteralPath $framePath) {
        Add-Type -AssemblyName System.Drawing
        $frame = [System.Drawing.Image]::FromFile($framePath)
        try {
            $frame.Save($screenshotPath, [System.Drawing.Imaging.ImageFormat]::Png)
        } finally {
            $frame.Dispose()
        }
        $screenshotCaptured = (Test-Path -LiteralPath $screenshotPath) -and
            (Get-Item -LiteralPath $screenshotPath).Length -gt 0
    }
    Set-Content -LiteralPath $tracePath -Value $testLog -Encoding UTF8
    Set-Content -LiteralPath $playerTracePath -Value $playerRunLog -Encoding UTF8
} finally {
    [Environment]::SetEnvironmentVariable(
        'BML_PLAYER_FRAME_PATH', $previousFramePath, 'Process')
    if ($null -ne $windowShell) {
        [Runtime.InteropServices.Marshal]::ReleaseComObject($windowShell) |
            Out-Null
    }
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

$outcomePattern = 'ExecuteBB test: status=(?<status>pass|fail) reason=(?<reason>\S+) x0=(?<x0>-?[0-9.]+) push_start=(?<pushStart>-?[0-9.]+) pushed=(?<pushed>-?[0-9.]+) pulled=(?<pulled>-?[0-9.]+) released=(?<released>-?[0-9.]+) physicalize_event=(?<physicalize>true|false) unphysicalize_event=(?<unphysicalize>true|false) menu_opened=(?<menuOpened>true|false) level_chosen=(?<levelChosen>true|false) control_ready=(?<controlReady>true|false) runtime_probe=(?<runtimeProbe>true|false) lifecycle_probe=(?<lifecycleProbe>true|false) additive_edit=(?<additiveEdit>true|false) script_hook_retirement=(?<scriptHookRetirement>true|false) runtime_detail=(?<runtimeDetail>\S+) frames=(?<frames>[0-9]+)'
$outcome = [regex]::Match($testLog, $outcomePattern)
$transportPattern = 'Behavior transport: status=(?<status>pass|fail) reason=(?<reason>\S+) transport=(?<transport>true|false) wire=(?<wire>true|false) object_ref=(?<objectRef>true|false) session_after_reset=(?<session>true|false) catalog=(?<catalog>true|false) detached=(?<detached>true|false) inspect=(?<inspect>true|false) watch=(?<watch>true|false)'
$transport = [regex]::Match($testLog, $transportPattern)
$patchPattern = 'Behavior patch: status=(?<status>pass|fail) reason=(?<reason>\S+) module=(?<module>true|false) apply=(?<apply>true|false) execute=(?<execute>true|false) close=(?<close>true|false) restore=(?<restore>true|false) reset=(?<reset>true|false) deletion=(?<deletion>true|false) retirement=(?<retirement>true|false)'
$patch = [regex]::Match($testLog, $patchPattern)
$runtimeSemanticsPattern = 'Behavior runtime semantics: status=(?<status>pass|fail) lifecycle=(?<lifecycle>true|false) additive_edit=(?<additiveEdit>true|false) detail=(?<detail>\S+)'
$runtimeSemantics = [regex]::Match($testLog, $runtimeSemanticsPattern)
$postStartIndex = $testLog.IndexOf('On Message PostStartMenu')
$preLoadIndex = $testLog.IndexOf('On Message PreLoadLevel')
$postLoadIndex = $testLog.IndexOf('On Message PostLoadLevel')
$startLevelIndex = $testLog.IndexOf('On Message StartLevel')
$naturalLevelFlow = $postStartIndex -ge 0 -and $preLoadIndex -gt $postStartIndex -and
                    $postLoadIndex -gt $preLoadIndex -and $startLevelIndex -gt $postLoadIndex
$checks = [ordered]@{
    PlayerWindowVisible = $playerWindowActivated
    MenuFlow = $outcome.Success -and
        $outcome.Groups['menuOpened'].Value -eq 'true' -and
        $outcome.Groups['levelChosen'].Value -eq 'true'
    TestPassed = $outcome.Success -and $outcome.Groups['status'].Value -eq 'pass'
    RuntimeProbe = $outcome.Success -and
        $outcome.Groups['runtimeProbe'].Value -eq 'true' -and
        $outcome.Groups['runtimeDetail'].Value -eq 'complete'
    RuntimeSemanticsFixture = $runtimeSemantics.Success -and
        $runtimeSemantics.Groups['status'].Value -eq 'pass' -and
        $runtimeSemantics.Groups['lifecycle'].Value -eq 'true' -and
        $runtimeSemantics.Groups['additiveEdit'].Value -eq 'true' -and
        $runtimeSemantics.Groups['detail'].Value -eq 'complete'
    LifecycleProbe = $outcome.Success -and
        $outcome.Groups['lifecycleProbe'].Value -eq 'true'
    AdditiveEditProbe = $outcome.Success -and
        $outcome.Groups['additiveEdit'].Value -eq 'true'
    ScriptHookRetirement = $outcome.Success -and
        $outcome.Groups['scriptHookRetirement'].Value -eq 'true' -and
        $testLog.Contains('ScriptHookRetirement installed=true') -and
        ([regex]::Matches($testLog, 'ScriptHookRetirement callback=1 uninstall=true').Count -eq 1) -and
        $testLog.Contains('ScriptHookRetirement retired=true callbacks=1')
    BehaviorTransportProbe = $transport.Success -and
        $transport.Groups['status'].Value -eq 'pass' -and
        $transport.Groups['transport'].Value -eq 'true' -and
        $transport.Groups['session'].Value -eq 'true' -and
        $transport.Groups['catalog'].Value -eq 'true'
    BehaviorPatch = $patch.Success -and
        $patch.Groups['status'].Value -eq 'pass' -and
        $patch.Groups['module'].Value -eq 'true' -and
        $patch.Groups['apply'].Value -eq 'true' -and
        $patch.Groups['execute'].Value -eq 'true' -and
        $patch.Groups['close'].Value -eq 'true' -and
        $patch.Groups['restore'].Value -eq 'true' -and
        $patch.Groups['reset'].Value -eq 'true' -and
        $patch.Groups['deletion'].Value -eq 'true' -and
        $patch.Groups['retirement'].Value -eq 'true'
    BehaviorInspectProbe = $transport.Success -and
        $transport.Groups['inspect'].Value -eq 'true' -and
        $testLog -match
            'Behavior inspect: status=pass graph=Gameplay_Events nodes=\d+ links=\d+ template_nodes=53 template_links=60 delay_1=true delay_2=true pending=unknown live=true'
    BehaviorDetachedProbe = $transport.Success -and
        $transport.Groups['detached'].Value -eq 'true'
    BehaviorWatchProbe = $transport.Success -and
        $transport.Groups['watch'].Value -eq 'true' -and
        $testLog.Contains(
            'Behavior watch: status=pass sampled=true events=2 exact_unavailable=true graph_endpoints=true')
    OutcomeWire = $transport.Success -and
        $transport.Groups['wire'].Value -eq 'true'
    CaptureTimeObjectRef = $transport.Success -and
        $transport.Groups['objectRef'].Value -eq 'true'
    ExitCallback = $testLog.Contains('ExecuteBB test exit: status=pass')
    CleanExecuteBB = -not $testLog.Contains('ExecuteBB::')
    CleanPostProcess = -not $playerRunLog.Contains('Error : PostProcess')
    CleanModLoad = -not $testLog.Contains('Failed to load ')
    CleanShutdown = $testLog.Contains('Goodbye!')
    NaturalLevelFlow = $naturalLevelFlow
    ScreenshotCaptured = $screenshotCaptured -and
        $testLog.Contains('Player frame: captured=true') -and
        (Test-Path -LiteralPath $screenshotPath) -and
        (Get-Item -LiteralPath $screenshotPath).Length -gt 0
    PlayerExited = -not $timedOut -and $playerExitCode -eq 0
    InstallRestored = $restored -and
        (Get-BMLOptionalHash $installedDll) -eq $installedHashBefore -and
        (Get-BMLOptionalHash $installedTestMod) -eq $installedTestModHashBefore -and
        (Get-BMLOptionalHash $installedRuntimeSemanticsMod) -eq $installedRuntimeSemanticsModHashBefore -and
        (Get-BMLOptionalHash $installedFixture) -eq $installedFixtureHashBefore -and
        (Get-BMLOptionalHash $installedTransportMod) -eq $installedTransportModHashBefore -and
        (Get-BMLOptionalHash $installedPatchMod) -eq $installedPatchModHashBefore -and
        (Get-BMLOptionalHash $installedTransportFixture) -eq $installedTransportFixtureHashBefore -and
        (Get-BMLOptionalHash $installedScriptMod) -eq $installedScriptModHashBefore -and
        (Get-BMLOptionalHash $modLoaderLog) -eq $modLoaderLogHashBefore -and
        (Get-BMLOptionalHash $playerLog) -eq $playerLogHashBefore
}
$failedChecks = @($checks.GetEnumerator() | Where-Object { -not $_.Value } | ForEach-Object Key)

$result = [pscustomobject]@{
    Status = $(if ($failedChecks.Count -eq 0) { 'pass' } else { 'fail' })
    BallanceRoot = $ballanceRootFull
    Visible = $playerWindowActivated
    PlayerExitCode = $playerExitCode
    PlayerTimedOut = $timedOut
    SourceHash = $sourceHash
    TestModHash = $testModHash
    RuntimeSemanticsModHash = $runtimeSemanticsModHash
    FixtureHash = Get-BMLOptionalHash $FixtureDll
    TransportModHash = Get-BMLOptionalHash $TransportMod
    PatchModHash = Get-BMLOptionalHash $PatchMod
    TransportFixtureHash = Get-BMLOptionalHash $TransportFixture
    ScriptModHash = Get-BMLOptionalHash $ScriptMod
    ArtifactsDirectory = $artifactsDirectoryFull
    Screenshot = $screenshotPath
    Trace = $tracePath
    InstalledHashBefore = $installedHashBefore
    InstalledHashAfter = Get-BMLOptionalHash $installedDll
    InstalledTestModHashBefore = $installedTestModHashBefore
    InstalledTestModHashAfter = Get-BMLOptionalHash $installedTestMod
    InstalledRuntimeSemanticsModHashBefore = $installedRuntimeSemanticsModHashBefore
    InstalledRuntimeSemanticsModHashAfter = Get-BMLOptionalHash $installedRuntimeSemanticsMod
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
            LifecycleProbe = $outcome.Groups['lifecycleProbe'].Value -eq 'true'
            AdditiveEditProbe = $outcome.Groups['additiveEdit'].Value -eq 'true'
            ScriptHookRetirement = $outcome.Groups['scriptHookRetirement'].Value -eq 'true'
            RuntimeDetail = $outcome.Groups['runtimeDetail'].Value
            Frames = [int]$outcome.Groups['frames'].Value
        }
    } else { $null })
}

$result
if ($failedChecks.Count -gt 0) {
    throw "ExecuteBB test failed: $($failedChecks -join ', ')"
}
