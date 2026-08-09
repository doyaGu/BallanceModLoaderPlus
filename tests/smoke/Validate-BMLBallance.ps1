[CmdletBinding()]
param(
    [string]$BallanceRoot = $env:BML_BALLANCE_ROOT,

    [string]$BuildDll,

    [string]$CKAngelScriptDll,

    [string]$NativeImcSmokeMod,

    [switch]$LegacyNativeSmoke,

    [ValidateRange(1, 600)]
    [int]$PlayerSeconds = 30,

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
        ExpectedSha256 = '452039D194A9E9620E381EA1F29BE735DEC2EF38BAC8F75746558FE9F80127E9'
        LoadNeedle = 'Loading Mod DebugUtilities[Debug Utilities] v0.3.1'
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

    $process = Start-Process -FilePath $playerPath -WorkingDirectory (Split-Path -Parent $playerPath) -WindowStyle Hidden -PassThru
    $playerStarted = $true
    $deadline = (Get-Date).AddSeconds($PlayerSeconds)
    while (-not $process.HasExited -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 100
        $process.Refresh()
        if ($HotReloadStateSmoke -and -not $hotReloadStateSourcePatched) {
            $liveLogText = Get-BMLTextIfExists $modLoaderLog
            if ((Test-SmokeTextContains $liveLogText 'BML state reload smoke v1 ready') -and
                (Test-SmokeTextContains $liveLogText 'BML script mod summary:')) {
                Copy-Item -LiteralPath $stateReloadSmokeRuntimeReplacement -Destination $stateReloadSmokeRuntime -Force
                $hotReloadStateSourcePatched = $true
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
        Add-SmokeCheck $checks 'script-summary' (Test-SmokeTextContains $modLogText 'BML script mod summary: imc-facades') 'BML script mod summary: imc-facades'
        Add-SmokeCheck $checks 'script-imc-facades' (Test-SmokeTextContains $modLogText 'BML IMC facade smoke: runtime=true stream=true') 'BML IMC facade smoke: runtime=true stream=true'
        Add-SmokeCheck $checks 'script-imc-stream' (Test-SmokeTextContains $modLogText 'BML IMC stream poll: status=0') 'BML IMC stream poll: status=0'
        if ($SingleFileSmoke) {
            Add-SmokeCheck $checks 'single-file-script-package' (Test-SmokeTextContains $modLogText 'BML single-file script smoke loaded resource=true') 'BML single-file script smoke loaded resource=true'
        }
        if ($ZipSmoke) {
            Add-SmokeCheck $checks 'zip-script-package' (Test-SmokeTextContains $modLogText 'BML zip script smoke loaded resource=true') 'BML zip script smoke loaded resource=true'
        }
        if ($HotReloadStateSmoke) {
            Add-SmokeCheck $checks 'state-reload-source-patched' $hotReloadStateSourcePatched 'BML state reload smoke source patched'
            Add-SmokeCheck $checks 'state-reload-ready' (Test-SmokeTextContains $modLogText 'BML state reload smoke v1 ready') 'BML state reload smoke v1 ready'
            if ($HotReloadStateScenario -eq 'Success') {
                Add-SmokeCheck $checks 'state-reload-migrated' (Test-SmokeTextContains $modLogText 'BML state reload smoke v2 loaded migrated=true from=1.0.0 counter=1235 text=from-v1:migrated') 'BML state reload smoke v2 loaded migrated=true from=1.0.0 counter=1235 text=from-v1:migrated'
                Add-SmokeCheck $checks 'state-reload-committed' (Test-SmokeTextContains $modLogText 'Script mod bml.state.reload.smoke hot reload succeeded.') 'Script mod bml.state.reload.smoke hot reload succeeded.'
            } else {
                $reloadFailedNeedle = 'Script mod bml.state.reload.smoke hot reload failed:'
                Add-SmokeCheck $checks 'state-reload-rejected' (Test-SmokeTextContains $modLogText $reloadFailedNeedle) $reloadFailedNeedle
                Add-SmokeCheck $checks 'state-reload-old-runtime-kept' (Test-SmokeTextContainsAfter $modLogText 'BML state reload smoke v1 heartbeat' $reloadFailedNeedle) 'BML state reload smoke v1 heartbeat after failed reload'
                Add-SmokeCheck $checks 'state-reload-candidate-not-loaded' (-not (Test-SmokeTextContains $modLogText 'candidate should not load')) 'candidate should not load'
                if ($HotReloadStateScenario -eq 'CompileFailure') {
                    Add-SmokeCheck $checks 'state-reload-compile-failed' (Test-SmokeTextContains $modLogText 'phase=compile') 'phase=compile'
                } elseif ($HotReloadStateScenario -eq 'MigrateFailure') {
                    Add-SmokeCheck $checks 'state-reload-migrate-failed' (Test-SmokeTextContains $modLogText 'intentional state reload migrate failure smoke') 'intentional state reload migrate failure smoke'
                    Add-SmokeCheck $checks 'state-reload-rollback-success' (Test-SmokeTextContains $modLogText 'Reload failed; rolled back to previous runtime') 'Reload failed; rolled back to previous runtime'
                } elseif ($HotReloadStateScenario -eq 'RestoreFailure') {
                    Add-SmokeCheck $checks 'state-reload-restore-failed' (Test-SmokeTextContains $modLogText 'intentional state reload restore failure smoke') 'intentional state reload restore failure smoke'
                    Add-SmokeCheck $checks 'state-reload-rollback-success' (Test-SmokeTextContains $modLogText 'Reload failed; rolled back to previous runtime') 'Reload failed; rolled back to previous runtime'
                }
            }
        }
        Add-SmokeCheck $checks 'compile-diagnostic' (Test-SmokeTextContains $modLogText 'phase=compile') 'phase=compile'
        Add-SmokeCheck $checks 'runtime-diagnostic' (Test-SmokeTextContains $modLogText 'phase=callback') 'phase=callback'
        Add-SmokeCheck $checks 'script-imgui-stack-recovery' (Test-SmokeTextContains $modLogText 'Recovered mismatched ImGui stack after script callback') 'Recovered mismatched ImGui stack after script callback'
        Add-SmokeCheck $checks 'script-imgui-stack-recovery-silent' (-not (Test-SmokeTextContains $modLogText '[imgui-error] In window')) 'no raw ImGui recovery errors in ModLoader log'
        if (-not $HotReloadStateSmoke -and -not $NativeImcSmokeMod) {
            Add-SmokeCheck $checks 'shutdown-smoke' (Test-SmokeTextContains $modLogText 'BML shutdown smoke requesting exit') 'BML shutdown smoke requesting exit'
        }
    }
    if ($NativeImcSmokeMod) {
        Add-SmokeCheck $checks 'native-imc-interfaces' (Test-SmokeTextContains $modLogText 'BML native IMC smoke: runtime=true scene=true gameplay=true ui=true speedrun=true events=true') 'BML native IMC smoke: runtime=true scene=true gameplay=true ui=true speedrun=true events=true'
        Add-SmokeCheck $checks 'native-imc-exit-event' (Test-SmokeTextContains $modLogText 'BML native IMC smoke exit event: received=true passed=true') 'BML native IMC smoke exit event: received=true passed=true'
    }
    if ($LegacyNativeSmoke) {
        foreach ($fixture in $legacyNativeSmokeFixtures) {
            $fileStem = [System.IO.Path]::GetFileNameWithoutExtension($fixture.FileName)
            Add-SmokeCheck $checks "legacy-native-$($fixture.ModId)-loaded" (Test-SmokeTextContains $modLogText $fixture.LoadNeedle) $fixture.LoadNeedle

            $lifecycleFailure =
                (Test-SmokeTextContains $modLogText "Failed to load $fileStem.") -or
                (Test-SmokeTextContains $modLogText "Duplicate Mod: $($fixture.ModId)") -or
                (Test-SmokeTextContains $modLogText "Dependencies not satisfied for mod $($fixture.ModId)") -or
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
    PlayerStarted = $playerStarted
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
