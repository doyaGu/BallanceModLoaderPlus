[CmdletBinding()]
param(
    [string]$BallanceRoot = $env:BML_BALLANCE_ROOT,

    [string]$BuildDll,

    [string]$NativeSmokeMod,

    [string]$CKAngelScriptDll,

    [ValidateRange(1, 600)]
    [int]$PlayerSeconds = 30,

    [switch]$SkipInstall,

    [switch]$SkipPlayer,

    [switch]$SkipSmokeInstall,

    [switch]$KeepInstalled,

    [switch]$SingleFileSmoke,

    [switch]$ZipSmoke
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot '..\..\scripts\lib\BMLProject.psm1') -Force

function Test-SmokeTextContains {
    param(
        [AllowNull()]
        [string]$Text,

        [Parameter(Mandatory = $true)]
        [string]$Needle
    )

    if ($null -eq $Text) {
        return $false
    }
    return $Text.IndexOf($Needle, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
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
        [string]$ModsDirectory
    )

    Remove-SmokeInstall -ModsDirectory $ModsDirectory

    foreach ($smoke in @(
        'BMLAngelScriptSmoke',
        'BMLAngelScriptCompileErrorSmoke',
        'BMLAngelScriptRuntimeErrorSmoke',
        'BMLAngelScriptShutdownSmoke'
    )) {
        Copy-BMLDirectoryFresh -SourceDir (Join-Path $ScriptSmokeRoot $smoke) -DestinationDir (Join-Path $ModsDirectory $smoke)
    }

    if ($SingleFileSmoke) {
        Install-SingleFileSmoke -SourceDirectory (Join-Path $ScriptSmokeRoot 'BMLAngelScriptSingleFileSmoke') -ModsDirectory $ModsDirectory
    }
    if ($ZipSmoke) {
        Install-ZipSmoke -SourceDirectory (Join-Path $ScriptSmokeRoot 'BMLAngelScriptZipSmoke') -ModsDirectory $ModsDirectory
    }

    Copy-Item -LiteralPath $NativeSmokeMod -Destination (Join-Path $ModsDirectory 'BMLNativeInteropSmoke.bmodp') -Force
}

if (-not $BallanceRoot) {
    throw 'Ballance root is required. Pass -BallanceRoot or set BML_BALLANCE_ROOT.'
}

$layout = Get-BMLProjectLayout
$scriptSmokeRoot = Join-Path $layout.RepoRoot 'tests\smoke\AngelScript'
$ballanceRootFull = [System.IO.Path]::GetFullPath($BallanceRoot)
$buildingBlocksDir = Join-Path $ballanceRootFull 'BuildingBlocks'
$modsDir = Join-Path $ballanceRootFull 'ModLoader\Mods'
$playerPath = Join-Path $ballanceRootFull 'Bin\Player.exe'
$installedDll = Join-Path $buildingBlocksDir 'BMLPlus.dll'
$installedAngelScriptDll = Join-Path $buildingBlocksDir 'AngelScript.dll'
$modLoaderLog = Join-Path $ballanceRootFull 'ModLoader\ModLoader.log'
$playerLog = Join-Path $ballanceRootFull 'Bin\Player.log'
$angelScriptLog = Join-Path $ballanceRootFull 'Bin\AngelScript.log'

if (-not $BuildDll) {
    $BuildDll = Join-Path $layout.DefaultReleaseBin 'BMLPlus.dll'
}
if (-not $NativeSmokeMod) {
    $NativeSmokeMod = Join-Path (Split-Path -Parent $BuildDll) 'BMLNativeInteropSmoke.bmodp'
}
Assert-BMLPath -Path $ballanceRootFull -Type Container
Assert-BMLPath -Path $buildingBlocksDir -Type Container
Assert-BMLPath -Path $modsDir -Type Container
Assert-BMLPath -Path $playerPath -Type Leaf
if (-not $SkipInstall) {
    Assert-BMLPath -Path $BuildDll -Type Leaf
}
if (-not $SkipSmokeInstall) {
    Assert-BMLPath -Path $NativeSmokeMod -Type Leaf
}
if ($CKAngelScriptDll) {
    Assert-BMLPath -Path $CKAngelScriptDll -Type Leaf
}

$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backupPath = $null
$sourceHash = Get-BMLOptionalHash $BuildDll
$installedHashBefore = Get-BMLOptionalHash $installedDll

if (-not $SkipInstall) {
    if (Test-Path -LiteralPath $installedDll) {
        $backupPath = "$installedDll.bak-$timestamp"
        Copy-Item -LiteralPath $installedDll -Destination $backupPath
    }
    Copy-Item -LiteralPath $BuildDll -Destination $installedDll -Force
}

if ($CKAngelScriptDll) {
    Copy-Item -LiteralPath $CKAngelScriptDll -Destination $installedAngelScriptDll -Force
}

if (-not $SkipSmokeInstall) {
    Install-SmokeMods -ScriptSmokeRoot $scriptSmokeRoot -ModsDirectory $modsDir
}

$playerExitCode = $null
$playerTimedOut = $false
$playerKilled = $false
$playerStarted = $false

if (-not $SkipPlayer) {
    foreach ($logPath in @($modLoaderLog, $playerLog, $angelScriptLog)) {
        if (Test-Path -LiteralPath $logPath) {
            Remove-Item -LiteralPath $logPath -Force
        }
    }

    $process = Start-Process -FilePath $playerPath -WorkingDirectory (Split-Path -Parent $playerPath) -PassThru
    $playerStarted = $true
    $deadline = (Get-Date).AddSeconds($PlayerSeconds)
    while (-not $process.HasExited -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 250
        $process.Refresh()
    }

    if (-not $process.HasExited) {
        $playerTimedOut = $true
        Stop-Process -Id $process.Id -Force
        $playerKilled = $true
        $process.WaitForExit()
    }

    $playerExitCode = $process.ExitCode
}

$modLogText = Get-BMLTextIfExists $modLoaderLog
$checks = [System.Collections.Generic.List[object]]::new()
if (-not $SkipPlayer) {
    $scriptCoreCapabilityExpected = 'BML core capability facade smoke: hud=<mode> srTimeOk=true rawMessage=0 rawHandle=true'
    $scriptCoreCapabilityPassed = [regex]::IsMatch(
        $modLogText,
        'BML core capability facade smoke: hud=-?\d+ srTimeOk=true rawMessage=0 rawHandle=true',
        [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    Add-SmokeCheck $checks 'bindings' (Test-SmokeTextContains $modLogText 'Registered BML AngelScript bindings') 'Registered BML AngelScript bindings'
    Add-SmokeCheck $checks 'script-summary' (Test-SmokeTextContains $modLogText 'BML script mod summary:') 'BML script mod summary:'
    Add-SmokeCheck $checks 'script-datashare-request' (Test-SmokeTextContains $modLogText 'BML datashare request: immediate=') 'BML datashare request: immediate='
    $scriptDataShareDelegatePassed =
        (Test-SmokeTextContains $modLogText 'BML datashare delegate immediate callback key=AngelScriptSmokeDelegateImmediate exists=true value=delegate-ready') -and
        (Test-SmokeTextContains $modLogText 'BML datashare delegate pending callback key=AngelScriptSmokeDelegatePending exists=true value=88') -and
        (Test-SmokeTextContains $modLogText 'BML datashare delegate bool callback key=AngelScriptSmokeDelegateBool exists=true value=true') -and
        (Test-SmokeTextContains $modLogText 'BML datashare delegate method callback key=AngelScriptSmokeDelegateObject exists=true value=object-ready hits=1') -and
        (Test-SmokeTextContains $modLogText 'dataShareDelegate=true')
    Add-SmokeCheck $checks 'script-datashare-delegate' $scriptDataShareDelegatePassed 'BML datashare delegate callbacks and summary dataShareDelegate=true'
    Add-SmokeCheck $checks 'script-object-ownership' (Test-SmokeTextContains $modLogText 'BML script object ownership: transientTimer=true heldTimer=true transientCommand=true heldCommand=true transientDataShare=true heldDataShare=true') 'BML script object ownership: transientTimer=true heldTimer=true transientCommand=true heldCommand=true transientDataShare=true heldDataShare=true'
    Add-SmokeCheck $checks 'script-command-delegate' (Test-SmokeTextContains $modLogText 'BML script command delegate registration: global=true method=true invalid=true') 'BML script command delegate registration: global=true method=true invalid=true'
    Add-SmokeCheck $checks 'command-completion-script' (Test-SmokeTextContains $modLogText 'BML script command completion callback') 'BML script command completion callback'
    Add-SmokeCheck $checks 'command-completion-native' (Test-SmokeTextContains $modLogText 'BML native command completion smoke') 'BML native command completion smoke'
    Add-SmokeCheck $checks 'command-completion-native-delegate' (Test-SmokeTextContains $modLogText 'BML native command delegate completion smoke command=true') 'BML native command delegate completion smoke command=true'
    Add-SmokeCheck $checks 'command-completion-native-method-delegate' (Test-SmokeTextContains $modLogText 'BML native command method delegate completion smoke command=true') 'BML native command method delegate completion smoke command=true'
    Add-SmokeCheck $checks 'script-ui-facade' (Test-SmokeTextContains $modLogText 'BML UI facade smoke:') 'BML UI facade smoke:'
    Add-SmokeCheck $checks 'script-core-capability-facade' $scriptCoreCapabilityPassed $scriptCoreCapabilityExpected
    Add-SmokeCheck $checks 'script-input-invalid-key' (Test-SmokeTextContains $modLogText 'keyboardInvalidDefaults=true') 'keyboardInvalidDefaults=true'
    Add-SmokeCheck $checks 'script-imgui-advanced' (Test-SmokeTextContains $modLogText 'BML ImGui advanced smoke:') 'BML ImGui advanced smoke:'
    Add-SmokeCheck $checks 'native-export-lifecycle' (Test-SmokeTextContains $modLogText 'BML native export lifecycle smoke') 'BML native export lifecycle smoke'
    Add-SmokeCheck $checks 'native-core-capability' (Test-SmokeTextContains $modLogText 'BML native core capability smoke get=0') 'BML native core capability smoke get=0'
    Add-SmokeCheck $checks 'native-core-capability-set' (Test-SmokeTextContains $modLogText 'set=0 rawMessage=0') 'set=0 rawMessage=0'
    Add-SmokeCheck $checks 'native-export-hardening' (Test-SmokeTextContains $modLogText 'BML native export hardening smoke findEx=0 ambiguous=-703 explicit=0 mismatch=-705 badSig=-704 missingTarget=-700 exception=-708') 'BML native export hardening smoke findEx=0 ambiguous=-703 explicit=0 mismatch=-705 badSig=-704 missingTarget=-700 exception=-708'
    Add-SmokeCheck $checks 'script-export-hardening' (Test-SmokeTextContains $modLogText 'BML native export hardening script tryEcho=0') 'BML native export hardening script tryEcho=0'
    Add-SmokeCheck $checks 'script-export-hardening-callstring' (Test-SmokeTextContains $modLogText 'voidString=-3') 'voidString=-3'
    Add-SmokeCheck $checks 'script-export-hardening-constants' (Test-SmokeTextContains $modLogText 'constants=true') 'constants=true'
    if ($SingleFileSmoke) {
        Add-SmokeCheck $checks 'single-file-script-package' (Test-SmokeTextContains $modLogText 'BML single-file script smoke loaded resource=true') 'BML single-file script smoke loaded resource=true'
    }
    if ($ZipSmoke) {
        Add-SmokeCheck $checks 'zip-script-package' (Test-SmokeTextContains $modLogText 'BML zip script smoke loaded resource=true') 'BML zip script smoke loaded resource=true'
    }
    Add-SmokeCheck $checks 'compile-diagnostic' (Test-SmokeTextContains $modLogText 'phase=compile') 'phase=compile'
    Add-SmokeCheck $checks 'runtime-diagnostic' (Test-SmokeTextContains $modLogText 'phase=callback') 'phase=callback'
    Add-SmokeCheck $checks 'goodbye' (Test-SmokeTextContains $modLogText 'Goodbye!') 'Goodbye!'
    Add-SmokeCheck $checks 'shutdown-smoke' (Test-SmokeTextContains $modLogText 'BML shutdown smoke requesting exit') 'BML shutdown smoke requesting exit'
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

if (-not $KeepInstalled -and -not $SkipInstall -and $backupPath -and (Test-Path -LiteralPath $backupPath)) {
    Copy-FileWithRetry -Source $backupPath -Destination $installedDll
}

$result = [pscustomobject]@{
    Status = $status
    BallanceRoot = $ballanceRootFull
    BuildDll = [System.IO.Path]::GetFullPath($BuildDll)
    InstalledDll = $installedDll
    BackupPath = $backupPath
    SourceHash = $sourceHash
    InstalledHashBefore = $installedHashBefore
    InstalledHashAfter = Get-BMLOptionalHash $installedDll
    NativeSmokeMod = $NativeSmokeMod
    CKAngelScriptDll = $CKAngelScriptDll
    SingleFileSmoke = [bool]$SingleFileSmoke
    ZipSmoke = [bool]$ZipSmoke
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
