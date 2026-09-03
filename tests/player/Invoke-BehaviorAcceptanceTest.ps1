[CmdletBinding()]
param(
    [string]$BallanceRoot = $env:BML_BALLANCE_ROOT,

    [string]$BuildDll,

    [string]$TestMod,

    [string]$RuntimeSemanticsMod,

    [string]$FixtureDll,

    [string]$TransportMod,

    [string]$PatchMod,

    [string]$FacadeMod,

    [string]$TransportFixture,

    [string]$ScriptMod,

    [switch]$DisableAngelScript,

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
if (-not $TestMod) {
    $TestMod = Join-Path $releaseBin 'BehaviorAcceptanceTest.bmodp'
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
if (-not $FacadeMod) {
    $FacadeMod = Join-Path $releaseBin 'BehaviorFacadeTest.bmodp'
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
$installedTestMod = Join-Path $ballanceRootFull 'ModLoader\Mods\BehaviorAcceptanceTest.bmodp'
$installedRuntimeSemanticsMod = Join-Path $ballanceRootFull 'ModLoader\Mods\BehaviorRuntimeSemanticsTest.bmodp'
$installedFixture = Join-Path $ballanceRootFull 'BuildingBlocks\BehaviorLifecycleFixture.dll'
$installedTransportMod = Join-Path $ballanceRootFull 'ModLoader\Mods\BehaviorTransportTest.bmodp'
$installedPatchMod = Join-Path $ballanceRootFull 'ModLoader\Mods\BehaviorPatchTest.bmodp'
$installedFacadeMod = Join-Path $ballanceRootFull 'ModLoader\Mods\BehaviorFacadeTest.bmodp'
$installedTransportFixture = Join-Path $ballanceRootFull 'BuildingBlocks\BehaviorTransportFixture.dll'
$installedScriptMod = Join-Path $ballanceRootFull 'ModLoader\Mods\BehaviorLifecycleScript.mod.as'
$modLoaderLog = Join-Path $ballanceRootFull 'ModLoader\ModLoader.log'
$playerLog = Join-Path $ballanceRootFull 'Bin\Player.log'
$playerConfig = Join-Path $ballanceRootFull 'Bin\Player.ini'

$requiredPaths = @($ballanceRootFull, $playerPath, $BuildDll, $TestMod,
                   $RuntimeSemanticsMod, $FixtureDll, $TransportMod,
                   $PatchMod, $FacadeMod, $TransportFixture)
if (-not $DisableAngelScript) {
    $requiredPaths += $ScriptMod
}
foreach ($path in $requiredPaths) {
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

if ($PlayerWidth -eq 0 -or $PlayerHeight -eq 0) {
    Add-Type -AssemblyName System.Windows.Forms
    $primaryScreen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    if ($PlayerWidth -eq 0) {
        $PlayerWidth = $primaryScreen.Width
    }
    if ($PlayerHeight -eq 0) {
        $PlayerHeight = $primaryScreen.Height
    }
}
$playerArguments = @('--width', $PlayerWidth.ToString(),
                     '--height', $PlayerHeight.ToString(), '--bpp', '32')

$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if (-not $ArtifactsDirectory) {
    $ArtifactsDirectory = Join-Path $layout.RepoRoot `
        "build-dev\player-behavior-acceptance-$timestamp"
}
$artifactsDirectoryFull = [System.IO.Path]::GetFullPath($ArtifactsDirectory)
New-Item -ItemType Directory -Path $artifactsDirectoryFull -Force | Out-Null
$screenshotPath = Join-Path $artifactsDirectoryFull 'Player-window.png'
$framePath = Join-Path $artifactsDirectoryFull 'Player-frame.bmp'
$tutorialScreenshotPath = Join-Path $artifactsDirectoryFull 'Tutorial-window.png'
$tutorialFramePath = Join-Path $artifactsDirectoryFull 'Tutorial-frame.bmp'
$patchBaselineScreenshotPath = Join-Path $artifactsDirectoryFull 'BehaviorPatch-baseline.png'
$patchActiveScreenshotPath = Join-Path $artifactsDirectoryFull 'BehaviorPatch-active.png'
$patchRestoredScreenshotPath = Join-Path $artifactsDirectoryFull 'BehaviorPatch-restored.png'
$runtimeScreenshotPath = Join-Path $artifactsDirectoryFull 'BehaviorRuntime.png'
$tracePath = Join-Path $artifactsDirectoryFull 'ModLoader-trace.log'
$playerTracePath = Join-Path $artifactsDirectoryFull 'Player-trace.log'
$artifacts = @(
    [pscustomobject]@{ Path = $installedDll; Backup = "$installedDll.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $installedTestMod; Backup = "$installedTestMod.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $installedRuntimeSemanticsMod; Backup = "$installedRuntimeSemanticsMod.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $installedFixture; Backup = "$installedFixture.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $installedTransportMod; Backup = "$installedTransportMod.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $installedPatchMod; Backup = "$installedPatchMod.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $installedFacadeMod; Backup = "$installedFacadeMod.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $installedTransportFixture; Backup = "$installedTransportFixture.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $installedScriptMod; Backup = "$installedScriptMod.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $modLoaderLog; Backup = "$modLoaderLog.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $playerLog; Backup = "$playerLog.test-bak-$timestamp" },
    [pscustomobject]@{ Path = $playerConfig; Backup = "$playerConfig.test-bak-$timestamp" }
)
$process = $null
$playerExitCode = $null
$timedOut = $false
$testLog = ''
$playerRunLog = ''
$restored = $false
$screenshotCaptured = $false
$tutorialScreenshotCaptured = $false
$patchBaselineCaptured = $false
$patchActiveCaptured = $false
$patchRestoredCaptured = $false
$runtimeCaptured = $false
$playerWindowActivated = $false
$setupDialogAccepted = $false
$tutorialExit = New-BMLTutorialExitState
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
$installedFacadeModHashBefore = Get-BMLOptionalHash $installedFacadeMod
$installedTransportFixtureHashBefore = Get-BMLOptionalHash $installedTransportFixture
$installedScriptModHashBefore = Get-BMLOptionalHash $installedScriptMod
$modLoaderLogHashBefore = Get-BMLOptionalHash $modLoaderLog
$playerLogHashBefore = Get-BMLOptionalHash $playerLog
$playerConfigHashBefore = Get-BMLOptionalHash $playerConfig
$previousFramePath = [Environment]::GetEnvironmentVariable(
    'BML_PLAYER_FRAME_PATH', 'Process')
$previousTutorialFramePath = [Environment]::GetEnvironmentVariable(
    'BML_PLAYER_TUTORIAL_FRAME_PATH', 'Process')
$previousDisableAngelScript = [Environment]::GetEnvironmentVariable(
    'BML_PLAYER_DISABLE_ANGELSCRIPT', 'Process')

try {
    [Environment]::SetEnvironmentVariable(
        'BML_PLAYER_FRAME_PATH', $framePath, 'Process')
    [Environment]::SetEnvironmentVariable(
        'BML_PLAYER_TUTORIAL_FRAME_PATH', $tutorialFramePath, 'Process')
    [Environment]::SetEnvironmentVariable(
        'BML_PLAYER_DISABLE_ANGELSCRIPT',
        $(if ($DisableAngelScript) { '1' } else { $null }),
        'Process')
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
    Copy-TestFile -Source $FacadeMod -Destination $installedFacadeMod
    Copy-TestFile -Source $TransportFixture -Destination $installedTransportFixture
    if ($DisableAngelScript) {
        if (Test-Path -LiteralPath $installedScriptMod) {
            Remove-Item -LiteralPath $installedScriptMod -Force
        }
    } else {
        Copy-TestFile -Source $ScriptMod -Destination $installedScriptMod
    }
    foreach ($logPath in @($modLoaderLog, $playerLog)) {
        if (Test-Path -LiteralPath $logPath) {
            Remove-Item -LiteralPath $logPath -Force
        }
    }

    $windowShell = New-Object -ComObject WScript.Shell
    $started = Start-BMLPlayerProcess -PlayerPath $playerPath `
        -ArgumentList $playerArguments -WindowShell $windowShell
    $process = $started.Process
    $playerWindowActivated = $started.WindowActivated
    $setupDialogAccepted = $started.SetupDialogAccepted
    if ($started.SetupDialogFailed) {
        throw 'Player FullScreen Setup dialog did not expose a selectable render mode.'
    }

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while (-not $process.HasExited -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 100
        $process.Refresh()
        $liveLog = [string]::Join("`n", @(Get-BMLTextIfExists $modLoaderLog))
        if (-not $patchBaselineCaptured -and
            $liveLog.Contains('Behavior patch visual: stage=baseline ')) {
            [void]$windowShell.AppActivate($process.Id)
            Start-Sleep -Milliseconds 150
            $patchBaselineCaptured = Save-PlayerWindow `
                -Process $process -Path $patchBaselineScreenshotPath
        }
        if (-not $patchActiveCaptured -and
            $liveLog.Contains('Behavior patch visual: stage=active ')) {
            [void]$windowShell.AppActivate($process.Id)
            Start-Sleep -Milliseconds 150
            $patchActiveCaptured = Save-PlayerWindow `
                -Process $process -Path $patchActiveScreenshotPath
        }
        if (-not $patchRestoredCaptured -and
            $liveLog.Contains('Behavior patch visual: stage=restored ')) {
            [void]$windowShell.AppActivate($process.Id)
            Start-Sleep -Milliseconds 150
            $patchRestoredCaptured = Save-PlayerWindow `
                -Process $process -Path $patchRestoredScreenshotPath
        }
        if (-not $runtimeCaptured -and
            $liveLog.Contains('Behavior runtime visual: stage=active ')) {
            [void]$windowShell.AppActivate($process.Id)
            Start-Sleep -Milliseconds 150
            $runtimeCaptured = Save-PlayerWindow `
                -Process $process -Path $runtimeScreenshotPath
        }
        Step-BMLTutorialExit -State $tutorialExit -LiveLog $liveLog `
            -WindowShell $windowShell -ProcessId $process.Id
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
    if (Test-Path -LiteralPath $tutorialFramePath) {
        Add-Type -AssemblyName System.Drawing
        $tutorialFrame = [System.Drawing.Image]::FromFile($tutorialFramePath)
        try {
            $tutorialFrame.Save(
                $tutorialScreenshotPath,
                [System.Drawing.Imaging.ImageFormat]::Png)
        } finally {
            $tutorialFrame.Dispose()
        }
        $tutorialScreenshotCaptured =
            (Test-Path -LiteralPath $tutorialScreenshotPath) -and
            (Get-Item -LiteralPath $tutorialScreenshotPath).Length -gt 0
    }
    Set-Content -LiteralPath $tracePath -Value $testLog -Encoding UTF8
    Set-Content -LiteralPath $playerTracePath -Value $playerRunLog -Encoding UTF8
} finally {
    Reset-BMLTutorialExitKey
    [Environment]::SetEnvironmentVariable(
        'BML_PLAYER_FRAME_PATH', $previousFramePath, 'Process')
    [Environment]::SetEnvironmentVariable(
        'BML_PLAYER_TUTORIAL_FRAME_PATH', $previousTutorialFramePath, 'Process')
    [Environment]::SetEnvironmentVariable(
        'BML_PLAYER_DISABLE_ANGELSCRIPT', $previousDisableAngelScript,
        'Process')
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

$outcomePattern = 'Behavior acceptance: status=(?<status>pass|fail) reason=(?<reason>\S+) menu_opened=(?<menuOpened>true|false) level_chosen=(?<levelChosen>true|false) control_ready=(?<controlReady>true|false) runtime_probe=(?<runtimeProbe>true|false) lifecycle_probe=(?<lifecycleProbe>true|false) additive_edit=(?<additiveEdit>true|false) script_hook_retirement=(?<scriptHookRetirement>true|false) tutorial_declared=(?<tutorialDeclared>true|false) tutorial_listener=(?<tutorialListener>true|false) tutorial_exited=(?<tutorialExited>true|false) tutorial_by_input=(?<tutorialByInput>true|false) runtime_detail=(?<runtimeDetail>\S+) frames=(?<frames>[0-9]+)'
$outcome = [regex]::Match($testLog, $outcomePattern)
$transportPattern = 'Behavior transport: status=(?<status>pass|fail) reason=(?<reason>\S+) transport=(?<transport>true|false) wire=(?<wire>true|false) object_ref=(?<objectRef>true|false) session_after_reset=(?<session>true|false) catalog=(?<catalog>true|false) detached=(?<detached>true|false) inspect=(?<inspect>true|false) watch=(?<watch>true|false)'
$transport = [regex]::Match($testLog, $transportPattern)
$patchPattern = 'Behavior patch: status=(?<status>pass|fail) reason=(?<reason>\S+) module=(?<module>true|false) visual=(?<visual>true|false) durable=(?<durable>true|false) relations=(?<relations>true|false) apply=(?<apply>true|false) execute=(?<execute>true|false) close=(?<close>true|false) restore=(?<restore>true|false) reset=(?<reset>true|false) deletion=(?<deletion>true|false) retirement=(?<retirement>true|false) hooks=(?<hooks>true|false) graph_changed=(?<graphChanged>true|false) callback_close=(?<callbackClose>true|false)'
$patch = [regex]::Match($testLog, $patchPattern)
$facadePattern = 'Behavior plan: status=(?<status>pass|fail) reason=(?<reason>\S+) submit=(?<submit>true|false) install=(?<install>true|false) hooks=(?<hooks>true|false) close=(?<close>true|false) release=(?<release>true|false) taps=(?<taps>[0-9]+) afters=(?<afters>[0-9]+) frames=(?<frames>[0-9]+)'
$facade = [regex]::Match($testLog, $facadePattern)
$facadeSelfClosePattern = 'Behavior self-close: status=(?<status>pass|fail) calls=(?<calls>[0-9]+) closing=(?<closing>true|false)'
$facadeSelfClose = [regex]::Match($testLog, $facadeSelfClosePattern)
$facadePatchPattern = 'Behavior graph patch: status=(?<status>pass|fail) reason=(?<reason>\S+) apply=(?<apply>true|false) close=(?<close>true|false)'
$facadePatch = [regex]::Match($testLog, $facadePatchPattern)
$runtimeSemanticsPattern = 'Behavior runtime semantics: status=(?<status>pass|fail) lifecycle=(?<lifecycle>true|false) additive_edit=(?<additiveEdit>true|false) relations=(?<relations>true|false) physics_force=(?<physicsForce>true|false) hook_error=(?<hookError>true|false) message=(?<message>true|false) visual=(?<visual>true|false) detail=(?<detail>\S+)'
$runtimeSemantics = [regex]::Match($testLog, $runtimeSemanticsPattern)
$postStartIndex = $testLog.IndexOf('On Message PostStartMenu')
$preLoadIndex = $testLog.IndexOf('On Message PreLoadLevel')
$postLoadIndex = $testLog.IndexOf('On Message PostLoadLevel')
$startLevelIndex = $testLog.IndexOf('On Message StartLevel')
$naturalLevelFlow = $postStartIndex -ge 0 -and $preLoadIndex -gt $postStartIndex -and
                    $postLoadIndex -gt $preLoadIndex -and $startLevelIndex -gt $postLoadIndex
$installRestored = $restored -and
    (Get-BMLOptionalHash $installedDll) -eq $installedHashBefore -and
    (Get-BMLOptionalHash $installedTestMod) -eq $installedTestModHashBefore -and
    (Get-BMLOptionalHash $installedRuntimeSemanticsMod) -eq $installedRuntimeSemanticsModHashBefore -and
    (Get-BMLOptionalHash $installedFixture) -eq $installedFixtureHashBefore -and
    (Get-BMLOptionalHash $installedTransportMod) -eq $installedTransportModHashBefore -and
    (Get-BMLOptionalHash $installedPatchMod) -eq $installedPatchModHashBefore -and
    (Get-BMLOptionalHash $installedFacadeMod) -eq $installedFacadeModHashBefore -and
    (Get-BMLOptionalHash $installedTransportFixture) -eq $installedTransportFixtureHashBefore -and
    (Get-BMLOptionalHash $installedScriptMod) -eq $installedScriptModHashBefore -and
    (Get-BMLOptionalHash $modLoaderLog) -eq $modLoaderLogHashBefore -and
    (Get-BMLOptionalHash $playerLog) -eq $playerLogHashBefore -and
    (Get-BMLOptionalHash $playerConfig) -eq $playerConfigHashBefore

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
        $runtimeSemantics.Groups['relations'].Value -eq 'true' -and
        $runtimeSemantics.Groups['physicsForce'].Value -eq 'true' -and
        $runtimeSemantics.Groups['hookError'].Value -eq 'true' -and
        $runtimeSemantics.Groups['message'].Value -eq 'true' -and
        $runtimeSemantics.Groups['visual'].Value -eq 'true' -and
        $runtimeSemantics.Groups['detail'].Value -eq 'complete'
    BehaviorRuntimeVisual = $runtimeCaptured -and
        $(Test-ImageDimensions -Path $runtimeScreenshotPath `
            -Width $PlayerWidth -Height $PlayerHeight) -and
        $testLog.Contains(
            'Behavior runtime visual: stage=active call=true start=true pulse=true')
    LifecycleProbe = $outcome.Success -and
        $outcome.Groups['lifecycleProbe'].Value -eq 'true'
    AdditiveEditProbe = $outcome.Success -and
        $outcome.Groups['additiveEdit'].Value -eq 'true'
    BallNavigationFlow = $outcome.Success -and
        $tutorialExit.Injected -and
        $testLog.Contains(
            'Gameplay tutorial keys: count=2 exit_declared=true continue=28 exit=16') -and
        $testLog.Contains('Gameplay input: tutorial_exit=true key=16') -and
        $testLog.Contains('Gameplay input: ready=true') -and
        $outcome.Groups['tutorialDeclared'].Value -eq 'true' -and
        $outcome.Groups['tutorialListener'].Value -eq 'true' -and
        $outcome.Groups['tutorialExited'].Value -eq 'true' -and
        $outcome.Groups['tutorialByInput'].Value -eq 'true' -and
        $outcome.Groups['controlReady'].Value -eq 'true'
    ScriptHookRetirement = $DisableAngelScript -or
        ($outcome.Success -and
         $outcome.Groups['scriptHookRetirement'].Value -eq 'true' -and
         $testLog.Contains('ScriptHookRetirement installed=true') -and
         ([regex]::Matches($testLog, 'ScriptHookRetirement callback=1 uninstall=true').Count -eq 1) -and
         $testLog.Contains('ScriptHookRetirement retired=true callbacks=1'))
    BehaviorTransportProbe = $transport.Success -and
        $transport.Groups['status'].Value -eq 'pass' -and
        $transport.Groups['transport'].Value -eq 'true' -and
        $transport.Groups['session'].Value -eq 'true' -and
        $transport.Groups['catalog'].Value -eq 'true'
    BehaviorPatch = $patch.Success -and
        $patch.Groups['status'].Value -eq 'pass' -and
        $patch.Groups['module'].Value -eq 'true' -and
        $patch.Groups['visual'].Value -eq 'true' -and
        $patch.Groups['durable'].Value -eq 'true' -and
        $patch.Groups['relations'].Value -eq 'true' -and
        $patch.Groups['apply'].Value -eq 'true' -and
        $patch.Groups['execute'].Value -eq 'true' -and
        $patch.Groups['close'].Value -eq 'true' -and
        $patch.Groups['restore'].Value -eq 'true' -and
        $patch.Groups['reset'].Value -eq 'true' -and
        $patch.Groups['deletion'].Value -eq 'true' -and
        $patch.Groups['retirement'].Value -eq 'true' -and
        $patch.Groups['hooks'].Value -eq 'true' -and
        $patch.Groups['graphChanged'].Value -eq 'true' -and
        $patch.Groups['callbackClose'].Value -eq 'true' -and
        $testLog.Contains(
            'Behavior patch graph changed: status=pass rejected=-1 error=27 readmitted=0 state=2') -and
        $testLog -match
            'Behavior patch callback close: status=pass first=-9 second=-9 state=3 nodes=2 links=3 routed=true calls=\d+' -and
        $testLog.Contains(
            'Behavior patch callback restore: status=pass stale=true nodes=1 links=2')
    BehaviorPlanFacade = $facade.Success -and
        $facade.Groups['status'].Value -eq 'pass' -and
        $facade.Groups['submit'].Value -eq 'true' -and
        $facade.Groups['install'].Value -eq 'true' -and
        $facade.Groups['hooks'].Value -eq 'true' -and
        $facade.Groups['close'].Value -eq 'true' -and
        $facade.Groups['release'].Value -eq 'true' -and
        [int]$facade.Groups['taps'].Value -ge 2 -and
        [int]$facade.Groups['afters'].Value -ge 1
    BehaviorHookSelfClose = $facadeSelfClose.Success -and
        $facadeSelfClose.Groups['status'].Value -eq 'pass' -and
        [int]$facadeSelfClose.Groups['calls'].Value -eq 1 -and
        $facadeSelfClose.Groups['closing'].Value -eq 'true'
    BehaviorGraphPatchFacade = $facadePatch.Success -and
        $facadePatch.Groups['status'].Value -eq 'pass' -and
        $facadePatch.Groups['apply'].Value -eq 'true' -and
        $facadePatch.Groups['close'].Value -eq 'true'
    BehaviorPatchVisual = $patchBaselineCaptured -and
        $patchActiveCaptured -and $patchRestoredCaptured -and
        $(Test-ImageDimensions -Path $patchBaselineScreenshotPath `
            -Width $PlayerWidth -Height $PlayerHeight) -and
        $(Test-ImageDimensions -Path $patchActiveScreenshotPath `
            -Width $PlayerWidth -Height $PlayerHeight) -and
        $(Test-ImageDimensions -Path $patchRestoredScreenshotPath `
            -Width $PlayerWidth -Height $PlayerHeight) -and
        $testLog.Contains('Behavior patch visual: stage=baseline ') -and
        $testLog.Contains('Behavior patch visual: stage=active ') -and
        $testLog.Contains('Behavior patch visual: stage=restored ')
    BehaviorInspectProbe = $transport.Success -and
        $transport.Groups['inspect'].Value -eq 'true' -and
        $testLog -match
            'Behavior inspect: status=pass graph=Gameplay_Events nodes=\d+ links=\d+ template_nodes=53 template_links=60 delay_1=true delay_2=true pending=unknown live=true'
    BehaviorDetachedProbe = $transport.Success -and
        $transport.Groups['detached'].Value -eq 'true'
    BehaviorWatchProbe = $transport.Success -and
        $transport.Groups['watch'].Value -eq 'true' -and
        $testLog.Contains(
            'Behavior watch: status=pass sampled=true events=2 callback_failure=true graph_endpoints=true layout_target=true layout_events=1') -and
        $testLog.Contains('Behavior layout target: changed=true using=true ') -and
        $testLog -match
            'Behavior layout watch event: sequence=1 kind=2 before=\d+ after=\d+ status=pass'
    FrameWire = $transport.Success -and
        $transport.Groups['wire'].Value -eq 'true'
    CaptureTimeObjectRef = $transport.Success -and
        $transport.Groups['objectRef'].Value -eq 'true'
    ExitCallback = $testLog.Contains('Behavior acceptance exit: status=pass')
    CleanExecuteBB = -not $testLog.Contains('ExecuteBB::')
    CleanPostProcess = -not $playerRunLog.Contains('Error : PostProcess')
    CleanModLoad = -not $testLog.Contains('Failed to load ')
    CleanShutdown = $testLog.Contains('Goodbye!')
    NaturalLevelFlow = $naturalLevelFlow
    ScreenshotCaptured = $screenshotCaptured -and
        $testLog.Contains('Player frame: captured=true') -and
        (Test-Path -LiteralPath $screenshotPath) -and
        (Get-Item -LiteralPath $screenshotPath).Length -gt 0
    TutorialScreenshotCaptured = $tutorialScreenshotCaptured -and
        $testLog.Contains('Tutorial frame: captured=true') -and
        (Test-Path -LiteralPath $tutorialScreenshotPath) -and
        (Get-Item -LiteralPath $tutorialScreenshotPath).Length -gt 0
    PlayerExited = -not $timedOut -and $playerExitCode -eq 0
    InstallRestored = $installRestored
}
$failedChecks = @($checks.GetEnumerator() | Where-Object { -not $_.Value } | ForEach-Object Key)

$result = [pscustomobject]@{
    Status = $(if ($failedChecks.Count -eq 0) { 'pass' } else { 'fail' })
    BallanceRoot = $ballanceRootFull
    Visible = $playerWindowActivated
    AngelScriptDisabled = [bool]$DisableAngelScript
    SetupDialogAccepted = $setupDialogAccepted
    PlayerExitCode = $playerExitCode
    PlayerTimedOut = $timedOut
    SourceHash = $sourceHash
    TestModHash = $testModHash
    RuntimeSemanticsModHash = $runtimeSemanticsModHash
    FixtureHash = Get-BMLOptionalHash $FixtureDll
    TransportModHash = Get-BMLOptionalHash $TransportMod
    FacadeModHash = Get-BMLOptionalHash $FacadeMod
    TransportFixtureHash = Get-BMLOptionalHash $TransportFixture
    ScriptModHash = Get-BMLOptionalHash $ScriptMod
    ArtifactsDirectory = $artifactsDirectoryFull
    Screenshot = $screenshotPath
    TutorialScreenshot = $tutorialScreenshotPath
    BehaviorPatchBaselineScreenshot = $patchBaselineScreenshotPath
    BehaviorPatchActiveScreenshot = $patchActiveScreenshotPath
    BehaviorPatchRestoredScreenshot = $patchRestoredScreenshotPath
    BehaviorRuntimeScreenshot = $runtimeScreenshotPath
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
            LevelMenuOpened = $outcome.Groups['menuOpened'].Value -eq 'true'
            LevelChosen = $outcome.Groups['levelChosen'].Value -eq 'true'
            ControlReady = $outcome.Groups['controlReady'].Value -eq 'true'
            TutorialExitDeclared = $outcome.Groups['tutorialDeclared'].Value -eq 'true'
            TutorialExitListener = $outcome.Groups['tutorialListener'].Value -eq 'true'
            TutorialExited = $outcome.Groups['tutorialExited'].Value -eq 'true'
            TutorialExitedByInput = $outcome.Groups['tutorialByInput'].Value -eq 'true'
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
    throw "Behavior acceptance test failed: $($failedChecks -join ', ')"
}
