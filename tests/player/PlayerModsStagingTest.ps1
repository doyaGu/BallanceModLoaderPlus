[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SourceRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$modulePath = Join-Path $SourceRoot 'tests\player\BMLPlayerHarness.psm1'
$module = Import-Module $modulePath -Force -PassThru
$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) `
    "bml-player-mods-$([guid]::NewGuid().ToString('N'))"
$modsDirectory = Join-Path $testRoot 'ModLoader\Mods'
$nestedDirectory = Join-Path $modsDirectory 'Library'
$state = $null

try {
    New-Item -ItemType Directory -Path $nestedDirectory -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $modsDirectory 'Existing.bmodp') `
        -Value 'native' -NoNewline
    Set-Content -LiteralPath (Join-Path $nestedDirectory 'Existing.mod.as') `
        -Value 'script' -NoNewline

    $state = & $module {
        param($Path)
        Start-BMLPlayerModsStaging -ModsDirectory $Path
    } $modsDirectory

    if (-not (Test-Path -LiteralPath $modsDirectory -PathType Container) -or
        @(Get-ChildItem -LiteralPath $modsDirectory -Force).Count -ne 0) {
        throw 'Player Mods staging did not provide an empty Mods directory.'
    }

    Set-Content -LiteralPath (Join-Path $modsDirectory 'Test.bmodp') `
        -Value 'test' -NoNewline
    & $module {
        param($Stage)
        Restore-BMLPlayerModsStaging -State $Stage
    } $state
    & $module {
        param($Stage)
        Restore-BMLPlayerModsStaging -State $Stage
    } $state
    $state = $null

    if ((Get-Content -LiteralPath (Join-Path $modsDirectory 'Existing.bmodp') `
            -Raw) -cne 'native' -or
        (Get-Content -LiteralPath (Join-Path $nestedDirectory 'Existing.mod.as') `
            -Raw) -cne 'script') {
        throw 'Player Mods staging did not restore the original files.'
    }
    if (Test-Path -LiteralPath (Join-Path $modsDirectory 'Test.bmodp')) {
        throw 'Player Mods staging left a test Mod in the restored directory.'
    }
    if (@(Get-ChildItem -LiteralPath (Split-Path -Parent $modsDirectory) `
            -Filter 'Mods.test-bak-*' -Force).Count -ne 0) {
        throw 'Player Mods staging left a backup directory behind.'
    }

    Remove-Item -LiteralPath $modsDirectory -Recurse -Force
    $state = & $module {
        param($Path)
        Start-BMLPlayerModsStaging -ModsDirectory $Path
    } $modsDirectory
    & $module {
        param($Stage)
        Restore-BMLPlayerModsStaging -State $Stage
    } $state
    $state = $null

    if (Test-Path -LiteralPath $modsDirectory) {
        throw 'Player Mods staging created a directory that did not exist before the test.'
    }
} finally {
    if ($null -ne $state) {
        & $module {
            param($Stage)
            Restore-BMLPlayerModsStaging -State $Stage
        } $state
    }
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}
