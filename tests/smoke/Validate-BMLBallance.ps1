[CmdletBinding()]
param(
    [string]$BallanceRoot = $env:BML_BALLANCE_ROOT,

    [string]$BuildDll,

    [string]$CKAngelScriptDll,

    [string]$NativeImcSmokeMod,

    [switch]$LegacyNativeSmoke,

    [ValidateRange(1, 600)]
    [int]$PlayerSeconds = 30,

    [switch]$ShowPlayer,

    [switch]$SkipInstall,

    [switch]$SkipPlayer,

    [switch]$SkipSmokeInstall,

    [switch]$SkipScriptSmoke,

    [switch]$KeepInstalled,

    [switch]$SingleFileSmoke,

    [switch]$ZipSmoke,

    [switch]$HotReloadStateSmoke,

    [ValidateSet('Success', 'CompileFailure', 'MigrateFailure', 'RestoreFailure')]
    [string]$HotReloadStateScenario = 'Success'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot '..\..\scripts\lib\BMLProject.psm1') -Force

function Convert-SmokeText {
    param(
        [AllowNull()]
        [object]$Text
    )

    if ($null -eq $Text) {
        return ''
    }

    return [string]::Join("`n", @($Text))
}

function Test-SmokeTextContains {
    param(
        [AllowNull()]
        [object]$Text,

        [Parameter(Mandatory = $true)]
        [string]$Needle
    )

    $textString = Convert-SmokeText $Text
    if ($textString.Length -eq 0) {
        return $false
    }
    return $textString.IndexOf($Needle, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
}

function Test-SmokeTextMatches {
    param(
        [AllowNull()]
        [object]$Text,

        [Parameter(Mandatory = $true)]
        [string]$Pattern
    )

    $textString = Convert-SmokeText $Text
    if ($textString.Length -eq 0) {
        return $false
    }
    return [System.Text.RegularExpressions.Regex]::IsMatch(
        $textString,
        $Pattern,
        [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
}

function Test-SmokeTextContainsAfter {
    param(
        [AllowNull()]
        [object]$Text,

        [Parameter(Mandatory = $true)]
        [string]$Needle,

        [Parameter(Mandatory = $true)]
        [string]$AfterNeedle
    )

    $textString = Convert-SmokeText $Text
    if ($textString.Length -eq 0) {
        return $false
    }
    $afterIndex = $textString.IndexOf($AfterNeedle, [System.StringComparison]::OrdinalIgnoreCase)
    if ($afterIndex -lt 0) {
        return $false
    }

    $needleIndex = $textString.IndexOf($Needle, $afterIndex + $AfterNeedle.Length, [System.StringComparison]::OrdinalIgnoreCase)
    return $needleIndex -ge 0
}

function Add-SmokeCheck {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [System.Collections.Generic.List[object]]$Checks,

        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [bool]$Passed,

        [Parameter(Mandatory = $true)]
        [string]$Needle
    )

    $Checks.Add([pscustomobject]@{
        Name = $Name
        Passed = $Passed
        Needle = $Needle
    })
}

function Install-SingleFileSmoke {
    param(
        [string]$SourceDirectory,
        [string]$ModsDirectory
    )

    Assert-BMLPath -Path $SourceDirectory -Type Container
    $entry = Get-ChildItem -LiteralPath $SourceDirectory -File -Filter '*.mod.as' | Select-Object -First 1
    if (-not $entry) {
        throw "Single-file smoke source has no *.mod.as entry: $SourceDirectory"
    }

    $destinationEntry = Join-Path $ModsDirectory $entry.Name
    $stem = $entry.Name.Substring(0, $entry.Name.Length - '.mod.as'.Length)
    $destinationRoot = Join-Path $ModsDirectory $stem

    if (Test-Path -LiteralPath $destinationEntry) {
        Remove-Item -LiteralPath $destinationEntry -Force
    }
    if (Test-Path -LiteralPath $destinationRoot) {
        Remove-Item -LiteralPath $destinationRoot -Recurse -Force
    }

    Copy-Item -LiteralPath $entry.FullName -Destination $destinationEntry -Force

    $resources = Join-Path $SourceDirectory 'Resources'
    if (Test-Path -LiteralPath $resources) {
        New-Item -ItemType Directory -Path $destinationRoot -Force | Out-Null
        Copy-Item -LiteralPath $resources -Destination (Join-Path $destinationRoot 'Resources') -Recurse -Force
    }
}

function Install-ZipSmoke {
    param(
        [string]$SourceDirectory,
        [string]$ModsDirectory
    )

    $zipPath = Join-Path $ModsDirectory 'BMLAngelScriptZipSmoke.zip'
    $packScript = Join-Path $PSScriptRoot '..\..\scripts\Pack-BMLScriptMod.ps1'
    Assert-BMLPath -Path $packScript -Type Leaf | Out-Null
    & $packScript -Source $SourceDirectory -Output $zipPath -Force | Out-Null
}

function Remove-SmokeInstall {
    param([string]$ModsDirectory)

    $directories = @(
        'BMLAngelScriptSmoke',
        'BMLAngelScriptCompileErrorSmoke',
        'BMLAngelScriptRuntimeErrorSmoke',
        'BMLAngelScriptShutdownSmoke',
        'BMLAngelScriptStateReloadSmoke',
        'BMLAngelScriptSingleFileSmoke'
    )
    foreach ($directory in $directories) {
        $path = Join-Path $ModsDirectory $directory
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Recurse -Force
        }
    }

    foreach ($file in @('BMLAngelScriptSingleFileSmoke.mod.as', 'BMLAngelScriptZipSmoke.zip')) {
        $path = Join-Path $ModsDirectory $file
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Force
        }
    }
}

function Copy-FileWithRetry {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,

        [Parameter(Mandatory = $true)]
        [string]$Destination,

        [ValidateRange(1, 60)]
        [int]$Attempts = 10,

        [ValidateRange(1, 10000)]
        [int]$DelayMilliseconds = 500
    )

    for ($i = 1; $i -le $Attempts; ++$i) {
        try {
            Copy-Item -LiteralPath $Source -Destination $Destination -Force
            return
        } catch {
            if ($i -eq $Attempts) {
                throw
            }
            Start-Sleep -Milliseconds $DelayMilliseconds
        }
    }
}

function Install-SmokeMods {
    param(
        [string]$ScriptSmokeRoot,
        [string]$ModsDirectory,
        [bool]$InstallShutdownSmoke
    )

    Remove-SmokeInstall -ModsDirectory $ModsDirectory

    foreach ($smoke in @(
        'BMLAngelScriptSmoke',
        'BMLAngelScriptCompileErrorSmoke',
        'BMLAngelScriptRuntimeErrorSmoke'
    )) {
        Copy-BMLDirectoryFresh -SourceDir (Join-Path $ScriptSmokeRoot $smoke) -DestinationDir (Join-Path $ModsDirectory $smoke)
    }

    if ($HotReloadStateSmoke) {
        Copy-BMLDirectoryFresh -SourceDir (Join-Path $ScriptSmokeRoot 'BMLAngelScriptStateReloadSmoke') -DestinationDir (Join-Path $ModsDirectory 'BMLAngelScriptStateReloadSmoke')
    } elseif ($InstallShutdownSmoke) {
        Copy-BMLDirectoryFresh -SourceDir (Join-Path $ScriptSmokeRoot 'BMLAngelScriptShutdownSmoke') -DestinationDir (Join-Path $ModsDirectory 'BMLAngelScriptShutdownSmoke')
    }

    if ($SingleFileSmoke) {
        Install-SingleFileSmoke -SourceDirectory (Join-Path $ScriptSmokeRoot 'BMLAngelScriptSingleFileSmoke') -ModsDirectory $ModsDirectory
    }
    if ($ZipSmoke) {
        Install-ZipSmoke -SourceDirectory (Join-Path $ScriptSmokeRoot 'BMLAngelScriptZipSmoke') -ModsDirectory $ModsDirectory
    }

}

if (-not $BallanceRoot) {
    throw 'Ballance root is required. Pass -BallanceRoot or set BML_BALLANCE_ROOT.'
}
if ($SkipScriptSmoke -and ($SingleFileSmoke -or $ZipSmoke -or $HotReloadStateSmoke)) {
    throw '-SingleFileSmoke, -ZipSmoke, and -HotReloadStateSmoke require script smoke tests.'
}
if ($SkipScriptSmoke -and -not $SkipPlayer -and -not $NativeImcSmokeMod) {
    throw '-SkipScriptSmoke requires -NativeImcSmokeMod when Player is started.'
}
if (-not $HotReloadStateSmoke -and $HotReloadStateScenario -ne 'Success') {
    throw '-HotReloadStateScenario requires -HotReloadStateSmoke.'
}

$layout = Get-BMLProjectLayout
$scriptSmokeRoot = Join-Path $layout.RepoRoot 'tests\smoke\AngelScript'
$legacyNativeSmokeRoot = Join-Path $layout.RepoRoot 'packaging\runtime\ModLoader\Mods'
$legacyNativeSmokeFixtures = @(
    [pscustomobject]@{
        FileName = 'CameraUtilities.bmodp'
        ModId = 'CameraUtilities'
        ExpectedSha256 = '671D3A217D0E581B877FC9E2FD198030940D94F7615E839ED3CC7200411249C7'
        LoadNeedle = 'Loading Mod CameraUtilities[Camera Utilities] v0.3.0'
    },
    [pscustomobject]@{
        FileName = 'DebugUtilities.bmodp'
        ModId = 'DebugUtilities'
        ExpectedSha256 = 'B5CB4C2EC69CF3EC26D0BFF46B5F1B446303F4893E3C3EFA3E90CF96D7A1D954'
        LoadNeedle = 'Loading Mod DebugUtilities[Debug Utilities] v0.3.2'
    },
    [pscustomobject]@{
        FileName = 'TravelMode.bmodp'
        ModId = 'TravelMode'
        ExpectedSha256 = '353023505C04BAE008EFB8FB80B0247BF4B4CF4A9D82BE65EA8DFCB9F367C4BF'
        LoadNeedle = 'Loading Mod TravelMode[Travel Mode] v0.3.0'
    }
)
$ballanceRootFull = [System.IO.Path]::GetFullPath($BallanceRoot)
$buildingBlocksDir = Join-Path $ballanceRootFull 'BuildingBlocks'
$modsDir = Join-Path $ballanceRootFull 'ModLoader\Mods'
$playerPath = Join-Path $ballanceRootFull 'Bin\Player.exe'
$installedDll = Join-Path $buildingBlocksDir 'BMLPlus.dll'
$installedAngelScriptDll = Join-Path $buildingBlocksDir 'AngelScript.dll'
$installedNativeImcSmokeMod = Join-Path $modsDir 'BMLNativeImcSmoke.bmodp'
$retiredNativeInteropSmokeMod = Join-Path $modsDir 'BMLNativeInteropSmoke.bmodp'
$modLoaderLog = Join-Path $ballanceRootFull 'ModLoader\ModLoader.log'
$playerLog = Join-Path $ballanceRootFull 'Bin\Player.log'
$angelScriptLog = Join-Path $ballanceRootFull 'Bin\AngelScript.log'
$compileErrorSmokeRuntime = Join-Path $modsDir 'BMLAngelScriptCompileErrorSmoke\runtime.as'
$compileErrorSmokeRecovery = Join-Path $modsDir 'BMLAngelScriptCompileErrorSmoke\runtime.recovery.txt'
$stateReloadSmokeRuntime = Join-Path $modsDir 'BMLAngelScriptStateReloadSmoke\runtime.as'
$stateReloadSmokeRuntimeV2 = Join-Path $modsDir 'BMLAngelScriptStateReloadSmoke\runtime.v2.txt'
$stateReloadSmokeRuntimeReplacement = switch ($HotReloadStateScenario) {
    'CompileFailure' { Join-Path $modsDir 'BMLAngelScriptStateReloadSmoke\runtime.compile-fail.txt' }
    'MigrateFailure' { Join-Path $modsDir 'BMLAngelScriptStateReloadSmoke\runtime.migrate-fail.txt' }
    'RestoreFailure' { Join-Path $modsDir 'BMLAngelScriptStateReloadSmoke\runtime.restore-fail.txt' }
    default { $stateReloadSmokeRuntimeV2 }
}

if (-not $BuildDll) {
    $BuildDll = Join-Path $layout.DefaultReleaseBin 'BMLPlus.dll'
}
Assert-BMLPath -Path $ballanceRootFull -Type Container
Assert-BMLPath -Path $buildingBlocksDir -Type Container
Assert-BMLPath -Path $modsDir -Type Container
Assert-BMLPath -Path $playerPath -Type Leaf
if (-not $SkipInstall) {
    Assert-BMLPath -Path $BuildDll -Type Leaf
}
if ($NativeImcSmokeMod) {
    Assert-BMLPath -Path $NativeImcSmokeMod -Type Leaf
}
if ($LegacyNativeSmoke) {
    foreach ($fixture in $legacyNativeSmokeFixtures) {
        $fixturePath = Join-Path $legacyNativeSmokeRoot $fixture.FileName
        Assert-BMLPath -Path $fixturePath -Type Leaf
        $fixtureHash = Get-BMLOptionalHash $fixturePath
        if (-not [string]::Equals($fixtureHash, $fixture.ExpectedSha256,
                [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Legacy native smoke fixture hash changed: $($fixture.FileName). Expected $($fixture.ExpectedSha256), got $fixtureHash."
        }
    }
}
if ($CKAngelScriptDll) {
    Assert-BMLPath -Path $CKAngelScriptDll -Type Leaf
}

$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backupPath = $null
$angelScriptBackupPath = $null
$nativeImcSmokeBackupPath = $null
$retiredNativeInteropSmokeBackupPath = $null
$legacyNativeSmokeInstall = [System.Collections.Generic.List[object]]::new()
$process = $null
$playerExitCode = $null
$playerTimedOut = $false
$playerKilled = $false
$playerStarted = $false
$compileErrorRecoverySourcePatched = $false
$hotReloadStateSourcePatched = $false
$sourceHash = Get-BMLOptionalHash $BuildDll
$installedHashBefore = Get-BMLOptionalHash $installedDll
$installedAngelScriptHashBefore = Get-BMLOptionalHash $installedAngelScriptDll

$restoreState = [pscustomobject]@{ Completed = $false }
$restoreInstall = {
    if ($restoreState.Completed) {
        return
    }

    try {
        if ($null -ne $process) {
            try {
                $process.Refresh()
                if (-not $process.HasExited) {
                    Stop-Process -Id $process.Id -Force
                    $process.WaitForExit()
                }
            } catch {
                # The process may have exited between Refresh and Stop-Process.
            }
        }

        if (-not $KeepInstalled -and -not $SkipInstall) {
            if ($backupPath -and (Test-Path -LiteralPath $backupPath)) {
                Copy-FileWithRetry -Source $backupPath -Destination $installedDll
                Remove-Item -LiteralPath $backupPath -Force
            } elseif (Test-Path -LiteralPath $installedDll) {
                Remove-Item -LiteralPath $installedDll -Force
            }
        }
        if (-not $KeepInstalled -and $CKAngelScriptDll) {
            if ($angelScriptBackupPath -and (Test-Path -LiteralPath $angelScriptBackupPath)) {
                Copy-FileWithRetry -Source $angelScriptBackupPath -Destination $installedAngelScriptDll
                Remove-Item -LiteralPath $angelScriptBackupPath -Force
            } elseif (Test-Path -LiteralPath $installedAngelScriptDll) {
                Remove-Item -LiteralPath $installedAngelScriptDll -Force
            }
        }
        if (-not $KeepInstalled -and -not $SkipSmokeInstall -and -not $SkipScriptSmoke) {
            Remove-SmokeInstall -ModsDirectory $modsDir
        }
        if ($retiredNativeInteropSmokeBackupPath -and
            (Test-Path -LiteralPath $retiredNativeInteropSmokeBackupPath)) {
            Copy-FileWithRetry -Source $retiredNativeInteropSmokeBackupPath -Destination $retiredNativeInteropSmokeMod
            Remove-Item -LiteralPath $retiredNativeInteropSmokeBackupPath -Force
        }
        if (-not $KeepInstalled -and -not $SkipSmokeInstall -and $NativeImcSmokeMod) {
            if ($nativeImcSmokeBackupPath -and (Test-Path -LiteralPath $nativeImcSmokeBackupPath)) {
                Copy-FileWithRetry -Source $nativeImcSmokeBackupPath -Destination $installedNativeImcSmokeMod
                Remove-Item -LiteralPath $nativeImcSmokeBackupPath -Force
            } elseif (Test-Path -LiteralPath $installedNativeImcSmokeMod) {
                Remove-Item -LiteralPath $installedNativeImcSmokeMod -Force
            }
        }
        if (-not $KeepInstalled -and -not $SkipSmokeInstall -and $LegacyNativeSmoke) {
            foreach ($installedFixture in $legacyNativeSmokeInstall) {
                if ($installedFixture.BackupPath -and (Test-Path -LiteralPath $installedFixture.BackupPath)) {
                    Copy-FileWithRetry -Source $installedFixture.BackupPath -Destination $installedFixture.Destination
                    Remove-Item -LiteralPath $installedFixture.BackupPath -Force
                } elseif (Test-Path -LiteralPath $installedFixture.Destination) {
                    Remove-Item -LiteralPath $installedFixture.Destination -Force
                }
            }
        }
    } finally {
        $restoreState.Completed = $true
    }
}

trap {
    $failure = $_
    & $restoreInstall
    throw $failure
}

if (-not $SkipInstall) {
    if (Test-Path -LiteralPath $installedDll) {
        $backupPath = "$installedDll.bak-$timestamp"
        Copy-Item -LiteralPath $installedDll -Destination $backupPath
    }
    Copy-Item -LiteralPath $BuildDll -Destination $installedDll -Force
}

if ($CKAngelScriptDll) {
    if (Test-Path -LiteralPath $installedAngelScriptDll) {
        $angelScriptBackupPath = "$installedAngelScriptDll.bak-$timestamp"
        Copy-Item -LiteralPath $installedAngelScriptDll -Destination $angelScriptBackupPath
    }
    Copy-Item -LiteralPath $CKAngelScriptDll -Destination $installedAngelScriptDll -Force
}

if (-not $SkipSmokeInstall) {
    if (Test-Path -LiteralPath $retiredNativeInteropSmokeMod) {
        $retiredNativeInteropSmokeBackupPath = "$retiredNativeInteropSmokeMod.bak-$timestamp"
        Copy-Item -LiteralPath $retiredNativeInteropSmokeMod -Destination $retiredNativeInteropSmokeBackupPath
        Remove-Item -LiteralPath $retiredNativeInteropSmokeMod -Force
    }

    if (-not $SkipScriptSmoke) {
        Install-SmokeMods -ScriptSmokeRoot $scriptSmokeRoot -ModsDirectory $modsDir `
            -InstallShutdownSmoke (-not [bool]$NativeImcSmokeMod)
    }

    if ($NativeImcSmokeMod) {
        if (Test-Path -LiteralPath $installedNativeImcSmokeMod) {
            $nativeImcSmokeBackupPath = "$installedNativeImcSmokeMod.bak-$timestamp"
            Copy-Item -LiteralPath $installedNativeImcSmokeMod -Destination $nativeImcSmokeBackupPath
        }
        Copy-Item -LiteralPath $NativeImcSmokeMod -Destination $installedNativeImcSmokeMod -Force
    }

    if ($LegacyNativeSmoke) {
        foreach ($fixture in $legacyNativeSmokeFixtures) {
            $source = Join-Path $legacyNativeSmokeRoot $fixture.FileName
            $destination = Join-Path $modsDir $fixture.FileName
            $fixtureBackupPath = $null
            if (Test-Path -LiteralPath $destination) {
                $fixtureBackupPath = "$destination.bak-$timestamp"
                Copy-Item -LiteralPath $destination -Destination $fixtureBackupPath
            }
            $legacyNativeSmokeInstall.Add([pscustomobject]@{
                FileName = $fixture.FileName
                ModId = $fixture.ModId
                Source = $source
                Destination = $destination
                BackupPath = $fixtureBackupPath
                SourceHash = Get-BMLOptionalHash $source
                ExpectedSha256 = $fixture.ExpectedSha256
                InstalledHashBefore = Get-BMLOptionalHash $destination
            })
            Copy-Item -LiteralPath $source -Destination $destination -Force
        }
    }
}

if (-not $SkipPlayer) {
    foreach ($logPath in @($modLoaderLog, $playerLog, $angelScriptLog)) {
        if (Test-Path -LiteralPath $logPath) {
            Remove-Item -LiteralPath $logPath -Force
        }
    }

    $playerArguments = @('--width', '800', '--height', '600', '--bpp', '32')
    $startPlayer = @{
        FilePath = $playerPath
        ArgumentList = $playerArguments
        WorkingDirectory = Split-Path -Parent $playerPath
        PassThru = $true
    }
    if (-not $ShowPlayer) {
        $startPlayer.WindowStyle = 'Hidden'
    }
    $process = Start-Process @startPlayer
    $playerStarted = $true
    $deadline = (Get-Date).AddSeconds($PlayerSeconds)
    while (-not $process.HasExited -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 100
        $process.Refresh()
        if ($HotReloadStateSmoke -and -not $hotReloadStateSourcePatched) {
            $liveLogText = Get-BMLTextIfExists $modLoaderLog
            if ((Test-SmokeTextContains $liveLogText 'BML state reload smoke v1 ready') -and
                (Test-SmokeTextContains $liveLogText 'BML state reload services: v1 timer=valid command=valid datashare=valid') -and
                (Test-SmokeTextContains $liveLogText 'BML state reload hook: v1=valid') -and
                (Test-SmokeTextContains $liveLogText 'BML state reload timer callback: v1') -and
                (Test-SmokeTextContains $liveLogText 'BML state reload command callback: v1') -and
                (Test-SmokeTextContains $liveLogText 'BML script mod summary:')) {
                Copy-Item -LiteralPath $stateReloadSmokeRuntimeReplacement -Destination $stateReloadSmokeRuntime -Force
                $hotReloadStateSourcePatched = $true
                Copy-Item -LiteralPath $compileErrorSmokeRecovery -Destination $compileErrorSmokeRuntime -Force
                $compileErrorRecoverySourcePatched = $true
            }
        }
    }

    if (-not $process.HasExited) {
        $playerTimedOut = $true
        Stop-Process -Id $process.Id -Force
        $playerKilled = $true
        $process.WaitForExit()
    }

    $playerExitCode = $process.ExitCode
}

$modLogText = Convert-SmokeText (Get-BMLTextIfExists $modLoaderLog)
$playerLogText = Convert-SmokeText (Get-BMLTextIfExists $playerLog)
$checks = [System.Collections.Generic.List[object]]::new()
if (-not $SkipPlayer) {
    Add-SmokeCheck $checks 'player-postprocess-clean' (-not (Test-SmokeTextContains $playerLogText 'Error : PostProcess')) 'Player.log must not contain Error : PostProcess'
    Add-SmokeCheck $checks 'mod-load-clean' (-not (Test-SmokeTextContains $modLogText 'Failed to load ')) 'ModLoader.log must not contain native mod load failures'
    if (-not $SkipScriptSmoke) {
        Add-SmokeCheck $checks 'bindings' (Test-SmokeTextContains $modLogText 'Registered BML AngelScript bindings') 'Registered BML AngelScript bindings'
        Add-SmokeCheck $checks 'bindings-unregister-clean' (-not (Test-SmokeTextContains $modLogText 'Failed to unregister BML AngelScript bindings')) 'BML AngelScript bindings must unregister cleanly'
        Add-SmokeCheck $checks 'script-summary' (Test-SmokeTextContains $modLogText 'BML script mod summary: capabilities') 'BML script mod summary: capabilities'
        Add-SmokeCheck $checks 'script-capabilities' (Test-SmokeTextContains $modLogText 'BML capability smoke: runtime=true') 'BML capability smoke: runtime=true'
        Add-SmokeCheck $checks 'script-raw-handle-validity' (Test-SmokeTextContains $modLogText 'BML raw handle validity smoke: live=true deleted=true') 'BML raw handle validity smoke: live=true deleted=true'
        Add-SmokeCheck $checks 'script-event-callback' (Test-SmokeTextContains $modLogText 'BML script event callback: exit_game') 'BML script event callback: exit_game'
        if ($SingleFileSmoke) {
            Add-SmokeCheck $checks 'single-file-script-package' (Test-SmokeTextContains $modLogText 'BML single-file script smoke loaded resource=true') 'BML single-file script smoke loaded resource=true'
        }
        if ($ZipSmoke) {
            Add-SmokeCheck $checks 'zip-script-package' (Test-SmokeTextContains $modLogText 'BML zip script smoke loaded resource=true') 'BML zip script smoke loaded resource=true'
        }
        if ($HotReloadStateSmoke) {
            Add-SmokeCheck $checks 'failed-placeholder-recovery-source-patched' $compileErrorRecoverySourcePatched 'BML compile error smoke source patched for recovery'
            Add-SmokeCheck $checks 'failed-placeholder-initial-compile-failed' (Test-SmokeTextContains $modLogText 'Script mod script:BMLAngelScriptCompileErrorSmoke failed: phase=compile') 'initial failed placeholder compile diagnostic'
            Add-SmokeCheck $checks 'failed-placeholder-recovery-phase' (Test-SmokeTextContains $modLogText 'BML failed placeholder recovery phase=valid id=bml.compile.error.smoke') 'BML failed placeholder recovery phase=valid id=bml.compile.error.smoke'
            Add-SmokeCheck $checks 'failed-placeholder-recovery-committed' (Test-SmokeTextContains $modLogText 'Script mod bml.compile.error.smoke hot reload succeeded.') 'Script mod bml.compile.error.smoke hot reload succeeded.'
            Add-SmokeCheck $checks 'failed-placeholder-recovery-shutdown' (Test-SmokeTextContains $modLogText 'BML failed placeholder recovery shutdown=valid') 'BML failed placeholder recovery shutdown=valid'
            Add-SmokeCheck $checks 'failed-placeholder-recovery-phase-valid' (-not (Test-SmokeTextContains $modLogText 'BML failed placeholder recovery phase=unexpected') -and
                -not (Test-SmokeTextContains $modLogText 'BML failed placeholder recovery shutdown=unexpected')) 'no unexpected failed placeholder recovery phase'
            Add-SmokeCheck $checks 'state-reload-source-patched' $hotReloadStateSourcePatched 'BML state reload smoke source patched'
            Add-SmokeCheck $checks 'state-reload-ready' (Test-SmokeTextContains $modLogText 'BML state reload smoke v1 ready') 'BML state reload smoke v1 ready'
            Add-SmokeCheck $checks 'state-reload-initial-phase' (Test-SmokeTextContains $modLogText 'BML state reload phase: v1 load=initial') 'BML state reload phase: v1 load=initial'
            Add-SmokeCheck $checks 'state-reload-phase-valid' (-not (Test-SmokeTextContains $modLogText 'BML state reload phase: v1 load=unexpected') -and
                -not (Test-SmokeTextContains $modLogText 'BML state reload phase: v1 unload=unexpected') -and
                -not (Test-SmokeTextContains $modLogText 'BML state reload phase: v2 load=unexpected') -and
                -not (Test-SmokeTextContains $modLogText 'BML state reload phase: v2 unload=unexpected')) 'no unexpected BML state reload phase'
            Add-SmokeCheck $checks 'state-hook-phase-valid' (-not (Test-SmokeTextMatches $modLogText 'BML state hook phase:[^\r\n]*=unexpected')) 'no unexpected BML state hook phase'
            Add-SmokeCheck $checks 'state-cleanup-phase-valid' (-not (Test-SmokeTextMatches $modLogText 'BML failed candidate cleanup phase:[^\r\n]*=unexpected')) 'no unexpected BML failed candidate cleanup phase'
            if ($HotReloadStateScenario -eq 'Success') {
                $v2Services = 'BML state reload services: v2 timer=valid command=valid datashare=valid'
                Add-SmokeCheck $checks 'state-reload-migrated' (Test-SmokeTextContains $modLogText 'BML state reload smoke v2 loaded migrated=true from=1.0.0 counter=1235 text=from-v1:migrated') 'BML state reload smoke v2 loaded migrated=true from=1.0.0 counter=1235 text=from-v1:migrated'
                Add-SmokeCheck $checks 'state-reload-committed' (Test-SmokeTextContains $modLogText 'Script mod bml.state.reload.smoke hot reload succeeded.') 'Script mod bml.state.reload.smoke hot reload succeeded.'
                Add-SmokeCheck $checks 'state-reload-old-unload-phase' (Test-SmokeTextContains $modLogText 'BML state reload phase: v1 unload=reload') 'BML state reload phase: v1 unload=reload'
                Add-SmokeCheck $checks 'state-reload-new-load-phase' (Test-SmokeTextContains $modLogText 'BML state reload phase: v2 load=reload') 'BML state reload phase: v2 load=reload'
                Add-SmokeCheck $checks 'state-reload-shutdown-phase' (Test-SmokeTextContains $modLogText 'BML state reload phase: v2 unload=shutdown') 'BML state reload phase: v2 unload=shutdown'
                Add-SmokeCheck $checks 'state-hook-save-phase' (Test-SmokeTextContains $modLogText 'BML state hook phase: v1 save=valid') 'BML state hook phase: v1 save=valid'
                Add-SmokeCheck $checks 'state-hook-migrate-phase' (Test-SmokeTextContains $modLogText 'BML state hook phase: v2 migrate=valid') 'BML state hook phase: v2 migrate=valid'
                Add-SmokeCheck $checks 'state-hook-restore-phase' (Test-SmokeTextContains $modLogText 'BML state hook phase: v2 restore=valid') 'BML state hook phase: v2 restore=valid'
                Add-SmokeCheck $checks 'state-reload-no-failed-cleanup' (-not (Test-SmokeTextContains $modLogText 'BML failed candidate cleanup phase:')) 'successful reload does not clean a failed candidate'
                Add-SmokeCheck $checks 'state-reload-v1-services-ready' (Test-SmokeTextContains $modLogText 'BML state reload services: v1 timer=valid command=valid datashare=valid') 'BML state reload services: v1 timer=valid command=valid datashare=valid'
                Add-SmokeCheck $checks 'state-reload-v1-timer-ran' (Test-SmokeTextContains $modLogText 'BML state reload timer callback: v1') 'BML state reload timer callback: v1'
                Add-SmokeCheck $checks 'state-reload-v1-command-ran' (Test-SmokeTextContains $modLogText 'BML state reload command callback: v1') 'BML state reload command callback: v1'
                Add-SmokeCheck $checks 'state-reload-v2-services-ready' (Test-SmokeTextContains $modLogText $v2Services) $v2Services
                Add-SmokeCheck $checks 'state-reload-v2-timer-ran' (Test-SmokeTextContainsAfter $modLogText 'BML state reload timer callback: v2' $v2Services) 'BML state reload timer callback: v2 after v2 service registration'
                Add-SmokeCheck $checks 'state-reload-v2-command-ran' (Test-SmokeTextContainsAfter $modLogText 'BML state reload command callback: v2' $v2Services) 'BML state reload command callback: v2 after v2 service registration'
                Add-SmokeCheck $checks 'state-reload-old-timer-stopped' (-not (Test-SmokeTextContainsAfter $modLogText 'BML state reload timer callback: v1' $v2Services)) 'no v1 timer callback after v2 service registration'
                Add-SmokeCheck $checks 'state-reload-old-command-stopped' (-not (Test-SmokeTextContainsAfter $modLogText 'BML state reload command callback: v1' $v2Services)) 'no v1 command callback after v2 service registration'
                Add-SmokeCheck $checks 'state-reload-v2-datashare-published' (Test-SmokeTextContainsAfter $modLogText 'BML state reload datashare publish: v2=valid' $v2Services) 'BML state reload datashare publish: v2=valid after v2 service registration'
                Add-SmokeCheck $checks 'state-reload-v2-datashare-received' (Test-SmokeTextContainsAfter $modLogText 'BML state reload datashare callback: v2=valid' $v2Services) 'BML state reload datashare callback: v2=valid after v2 service registration'
                Add-SmokeCheck $checks 'state-reload-old-datashare-stopped' (-not (Test-SmokeTextContains $modLogText 'BML state reload datashare callback: v1')) 'no v1 datashare callback after replacement'
                Add-SmokeCheck $checks 'state-reload-v1-hook-ready' (Test-SmokeTextContains $modLogText 'BML state reload hook: v1=valid') 'BML state reload hook: v1=valid'
                Add-SmokeCheck $checks 'state-reload-v2-hook-replaced' (Test-SmokeTextContainsAfter $modLogText 'BML state reload hook: v2=valid' 'BML state reload phase: v1 unload=reload') 'BML state reload hook: v2=valid after v1 unload'
                Add-SmokeCheck $checks 'state-reload-hooks-detached' (-not (Test-SmokeTextContains $modLogText 'BML state reload hook callback:')) 'detached reload hooks do not execute'
            } else {
                $reloadFailedNeedle = 'Script mod bml.state.reload.smoke hot reload failed:'
                Add-SmokeCheck $checks 'state-reload-rejected' (Test-SmokeTextContains $modLogText $reloadFailedNeedle) $reloadFailedNeedle
                Add-SmokeCheck $checks 'state-reload-old-runtime-kept' (Test-SmokeTextContainsAfter $modLogText 'BML state reload smoke v1 heartbeat' $reloadFailedNeedle) 'BML state reload smoke v1 heartbeat after failed reload'
                Add-SmokeCheck $checks 'state-reload-candidate-not-loaded' (-not (Test-SmokeTextContains $modLogText 'candidate should not load')) 'candidate should not load'
                Add-SmokeCheck $checks 'state-reload-shutdown-phase' (Test-SmokeTextContains $modLogText 'BML state reload phase: v1 unload=shutdown') 'BML state reload phase: v1 unload=shutdown'
                if ($HotReloadStateScenario -eq 'CompileFailure') {
                    Add-SmokeCheck $checks 'state-reload-compile-failed' (Test-SmokeTextContains $modLogText 'phase=compile') 'phase=compile'
                    Add-SmokeCheck $checks 'state-reload-compile-kept-runtime-active' (-not (Test-SmokeTextContains $modLogText 'BML state reload phase: v1 unload=reload') -and
                        -not (Test-SmokeTextContains $modLogText 'BML state reload phase: v1 load=rollback')) 'compile rejection does not deactivate the live runtime'
                    Add-SmokeCheck $checks 'state-reload-compile-skipped-state-hooks' (-not (Test-SmokeTextContains $modLogText 'BML state hook phase:')) 'compile rejection does not run state hooks'
                    Add-SmokeCheck $checks 'state-reload-compile-skipped-cleanup' (-not (Test-SmokeTextContains $modLogText 'BML failed candidate cleanup phase:')) 'compile rejection has no candidate runtime to clean'
                } elseif ($HotReloadStateScenario -eq 'MigrateFailure') {
                    Add-SmokeCheck $checks 'state-reload-mutation-blocked' (Test-SmokeTextContains $modLogText 'CKContext::CreateObject is not available during hot reload migrate-state') 'CKContext::CreateObject is not available during hot reload migrate-state'
                    Add-SmokeCheck $checks 'state-reload-mutation-not-leaked' (-not (Test-SmokeTextContains $modLogText 'BML state reload mutation leaked into the live world')) 'no state-reload mutation leak'
                    Add-SmokeCheck $checks 'state-reload-rollback-success' (Test-SmokeTextContains $modLogText 'Reload failed; rolled back to previous runtime') 'Reload failed; rolled back to previous runtime'
                    Add-SmokeCheck $checks 'state-reload-failed-candidate-unload-phase' (Test-SmokeTextContains $modLogText 'BML state reload phase: v1 unload=reload') 'BML state reload phase: v1 unload=reload'
                    Add-SmokeCheck $checks 'state-reload-rollback-load-phase' (Test-SmokeTextContains $modLogText 'BML state reload phase: v1 load=rollback') 'BML state reload phase: v1 load=rollback'
                    Add-SmokeCheck $checks 'state-hook-save-phase' (Test-SmokeTextContains $modLogText 'BML state hook phase: v1 save=valid') 'BML state hook phase: v1 save=valid'
                    Add-SmokeCheck $checks 'state-hook-migrate-phase' (Test-SmokeTextContains $modLogText 'BML state hook phase: migrate-fail migrate=valid') 'BML state hook phase: migrate-fail migrate=valid'
                    Add-SmokeCheck $checks 'state-hook-rollback-restore-phase' (Test-SmokeTextContains $modLogText 'BML state hook phase: v1 restore=valid') 'BML state hook phase: v1 restore=valid'
                    Add-SmokeCheck $checks 'state-reload-migrate-cleanup-phase' (Test-SmokeTextContains $modLogText 'BML failed candidate cleanup phase: migrate=valid') 'BML failed candidate cleanup phase: migrate=valid'
                    Add-SmokeCheck $checks 'state-reload-rollback-hook-reinstalled' (Test-SmokeTextContainsAfter $modLogText 'BML state reload hook: v1=valid' 'BML state reload phase: v1 unload=reload') 'BML state reload hook: v1=valid after v1 reload unload'
                } elseif ($HotReloadStateScenario -eq 'RestoreFailure') {
                    Add-SmokeCheck $checks 'state-reload-restore-failed' (Test-SmokeTextContains $modLogText 'intentional state reload restore failure smoke') 'intentional state reload restore failure smoke'
                    Add-SmokeCheck $checks 'state-reload-rollback-success' (Test-SmokeTextContains $modLogText 'Reload failed; rolled back to previous runtime') 'Reload failed; rolled back to previous runtime'
                    Add-SmokeCheck $checks 'state-reload-failed-candidate-unload-phase' (Test-SmokeTextContains $modLogText 'BML state reload phase: v1 unload=reload') 'BML state reload phase: v1 unload=reload'
                    Add-SmokeCheck $checks 'state-reload-rollback-load-phase' (Test-SmokeTextContains $modLogText 'BML state reload phase: v1 load=rollback') 'BML state reload phase: v1 load=rollback'
                    Add-SmokeCheck $checks 'state-hook-save-phase' (Test-SmokeTextContains $modLogText 'BML state hook phase: v1 save=valid') 'BML state hook phase: v1 save=valid'
                    Add-SmokeCheck $checks 'state-hook-migrate-phase' (Test-SmokeTextContains $modLogText 'BML state hook phase: restore-fail migrate=valid') 'BML state hook phase: restore-fail migrate=valid'
                    Add-SmokeCheck $checks 'state-hook-restore-phase' (Test-SmokeTextContains $modLogText 'BML state hook phase: restore-fail restore=valid') 'BML state hook phase: restore-fail restore=valid'
                    Add-SmokeCheck $checks 'state-hook-rollback-restore-phase' (Test-SmokeTextContains $modLogText 'BML state hook phase: v1 restore=valid') 'BML state hook phase: v1 restore=valid'
                    Add-SmokeCheck $checks 'state-reload-restore-cleanup-phase' (Test-SmokeTextContains $modLogText 'BML failed candidate cleanup phase: restore=valid') 'BML failed candidate cleanup phase: restore=valid'
                    Add-SmokeCheck $checks 'state-reload-rollback-hook-reinstalled' (Test-SmokeTextContainsAfter $modLogText 'BML state reload hook: v1=valid' 'BML state reload phase: v1 unload=reload') 'BML state reload hook: v1=valid after v1 reload unload'
                }
            }
        }
        Add-SmokeCheck $checks 'compile-diagnostic' (Test-SmokeTextContains $modLogText 'phase=compile') 'phase=compile'
        Add-SmokeCheck $checks 'runtime-diagnostic' (Test-SmokeTextContains $modLogText 'phase=callback') 'phase=callback'
        Add-SmokeCheck $checks 'script-async-rejected' (Test-SmokeTextContains $modLogText 'Async work is not available in the current script host phase.') 'Async work is not available in the current script host phase.'
        Add-SmokeCheck $checks 'script-imgui-stack-recovery' (Test-SmokeTextContains $modLogText 'Recovered mismatched ImGui stack after script callback') 'Recovered mismatched ImGui stack after script callback'
        Add-SmokeCheck $checks 'script-imgui-stack-recovery-silent' (-not (Test-SmokeTextContains $modLogText '[imgui-error] In window')) 'no raw ImGui recovery errors in ModLoader log'
        if (-not $HotReloadStateSmoke -and -not $NativeImcSmokeMod) {
            Add-SmokeCheck $checks 'shutdown-smoke' (Test-SmokeTextContains $modLogText 'BML shutdown smoke requesting exit') 'BML shutdown smoke requesting exit'
        }
    }
    if ($NativeImcSmokeMod) {
        if (-not $SkipScriptSmoke) {
            $scriptGameplaySnapshotPattern = 'BML gameplay snapshot: status=0 count=[1-9][0-9]* values=true'
            Add-SmokeCheck $checks 'script-gameplay-snapshot' (Test-SmokeTextMatches $modLogText $scriptGameplaySnapshotPattern) $scriptGameplaySnapshotPattern
        }
        Add-SmokeCheck $checks 'native-imc-interfaces' (Test-SmokeTextContains $modLogText 'BML native IMC smoke: runtime=true scene=true gameplay=true ui=true speedrun=true imc=true') 'BML native IMC smoke: runtime=true scene=true gameplay=true ui=true speedrun=true imc=true'
        Add-SmokeCheck $checks 'native-exit-callback' (Test-SmokeTextContains $modLogText 'BML native IMC smoke exit callback: received=true passed=true') 'BML native IMC smoke exit callback: received=true passed=true'
        Add-SmokeCheck $checks 'native-imc-unload' (Test-SmokeTextContains $modLogText 'BML native IMC smoke unloaded') 'BML native IMC smoke unloaded'
    }
    if ($LegacyNativeSmoke) {
        foreach ($fixture in $legacyNativeSmokeFixtures) {
            $fileStem = [System.IO.Path]::GetFileNameWithoutExtension($fixture.FileName)
            Add-SmokeCheck $checks "legacy-native-$($fixture.ModId)-loaded" (Test-SmokeTextContains $modLogText $fixture.LoadNeedle) $fixture.LoadNeedle

            $lifecycleFailure =
                (Test-SmokeTextContains $modLogText "Failed to load $fileStem.") -or
                (Test-SmokeTextContains $modLogText "Duplicate Mod: $($fixture.ModId)") -or
                (Test-SmokeTextContains $modLogText "Cannot initialize Mod $($fixture.ModId):") -or
                (Test-SmokeTextContains $modLogText "Exception in mod $($fixture.ModId) unload callback") -or
                (Test-SmokeTextContains $modLogText "Failed to unload mod $($fixture.ModId).")
            Add-SmokeCheck $checks "legacy-native-$($fixture.ModId)-lifecycle" (-not $lifecycleFailure) "no load, duplicate, dependency, unload callback, or unload failure for $($fixture.ModId)"
        }
    }
    Add-SmokeCheck $checks 'goodbye' (Test-SmokeTextContains $modLogText 'Goodbye!') 'Goodbye!'
}

$failedChecks = @($checks | Where-Object { -not $_.Passed })
$hasGoodbye = Test-SmokeTextContains $modLogText 'Goodbye!'
$shutdownAnomaly = $playerStarted -and $null -ne $playerExitCode -and $playerExitCode -ne 0 -and $hasGoodbye

$status = 'ok'
if ($playerTimedOut -or $failedChecks.Count -gt 0) {
    $status = 'failed'
} elseif ($shutdownAnomaly) {
    $status = 'shutdown_anomaly'
}

& $restoreInstall

$result = [pscustomobject]@{
    Status = $status
    BallanceRoot = $ballanceRootFull
    BuildDll = [System.IO.Path]::GetFullPath($BuildDll)
    InstalledDll = $installedDll
    BackupPath = $backupPath
    SourceHash = $sourceHash
    InstalledHashBefore = $installedHashBefore
    InstalledHashAfter = Get-BMLOptionalHash $installedDll
    InstalledAngelScriptDll = $installedAngelScriptDll
    AngelScriptBackupPath = $angelScriptBackupPath
    InstalledAngelScriptHashBefore = $installedAngelScriptHashBefore
    InstalledAngelScriptHashAfter = Get-BMLOptionalHash $installedAngelScriptDll
    CKAngelScriptDll = $CKAngelScriptDll
    NativeImcSmokeMod = $NativeImcSmokeMod
    InstalledNativeImcSmokeMod = $installedNativeImcSmokeMod
    NativeImcSmokeBackupPath = $nativeImcSmokeBackupPath
    RetiredNativeInteropSmokeMod = $retiredNativeInteropSmokeMod
    RetiredNativeInteropSmokeBackupPath = $retiredNativeInteropSmokeBackupPath
    LegacyNativeSmoke = [bool]$LegacyNativeSmoke
    LegacyNativeSmokeInstall = @($legacyNativeSmokeInstall | ForEach-Object {
        [pscustomobject]@{
            FileName = $_.FileName
            ModId = $_.ModId
            Source = $_.Source
            Destination = $_.Destination
            BackupPath = $_.BackupPath
            SourceHash = $_.SourceHash
            ExpectedSha256 = $_.ExpectedSha256
            InstalledHashBefore = $_.InstalledHashBefore
            InstalledHashAfter = Get-BMLOptionalHash $_.Destination
        }
    })
    SkipScriptSmoke = [bool]$SkipScriptSmoke
    SingleFileSmoke = [bool]$SingleFileSmoke
    ZipSmoke = [bool]$ZipSmoke
    HotReloadStateSmoke = [bool]$HotReloadStateSmoke
    HotReloadStateScenario = $HotReloadStateScenario
    CompileErrorRecoverySourcePatched = $compileErrorRecoverySourcePatched
    PlayerStarted = $playerStarted
    ShowPlayer = [bool]$ShowPlayer
    PlayerExitCode = $playerExitCode
    PlayerTimedOut = $playerTimedOut
    PlayerKilled = $playerKilled
    ShutdownAnomaly = $shutdownAnomaly
    Checks = $checks
    FailedChecks = $failedChecks
    Logs = [pscustomobject]@{
        ModLoader = $modLoaderLog
        Player = $playerLog
        AngelScript = $angelScriptLog
        ModLoaderTail = Get-BMLLogTail $modLoaderLog
        PlayerTail = Get-BMLLogTail $playerLog
        AngelScriptTail = Get-BMLLogTail $angelScriptLog
    }
}

$result

if ($status -eq 'failed') {
    $missing = ($failedChecks | ForEach-Object { $_.Name }) -join ', '
    throw "BML Ballance validation failed. Missing checks: $missing"
}
