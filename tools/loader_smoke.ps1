param(
    [string]$GameDir = "C:\Users\kakut\Games\Ballance",
    [string]$BuildConfig = "Release",
    [int]$RuntimeSeconds = 20,
    [string[]]$Cases = @("base5", "base5_hud", "base5_modmenu", "base5_mapmenu", "base5_ui_stack", "full_builtin")
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$BuildRoot = Join-Path $RepoRoot "build"
$ArtifactsRoot = Join-Path $RepoRoot ("artifacts\loader-smoke-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
$PlayerExe = Join-Path $GameDir "Bin\Player.exe"
$GameBml = Join-Path $GameDir "Bin\BML.dll"
$BuiltBml = Join-Path $BuildRoot ("bin\" + $BuildConfig + "\BML.dll")
$BuiltModsRoot = Join-Path $BuildRoot "Mods"

$BaseModules = @("BML_UI", "BML_Input", "BML_Console", "BML_ObjectLoad", "BML_Event")
$CaseMap = @{
    "base5" = $BaseModules
    "base5_hud" = $BaseModules + @("BML_HUD")
    "base5_modmenu" = $BaseModules + @("BML_ModMenu")
    "base5_mapmenu" = $BaseModules + @("BML_MapMenu")
    "base5_ui_stack" = $BaseModules + @("BML_HUD", "BML_ModMenu", "BML_MapMenu")
    "base5_ui" = $BaseModules
    "full_builtin" = @(
        "BML_UI",
        "BML_Input",
        "BML_Console",
        "BML_ObjectLoad",
        "BML_Event",
        "BML_HUD",
        "BML_ModMenu",
        "BML_MapMenu",
        "BML_Render",
        "BML_Physics"
    )
}

function New-CaseLayout {
    param(
        [string]$CaseName,
        [string[]]$Modules
    )

    $CaseRoot = Join-Path $ArtifactsRoot $CaseName
    $ModsDir = Join-Path $CaseRoot "Mods"
    $LogsDir = Join-Path $CaseRoot "logs"

    New-Item -ItemType Directory -Path $ModsDir -Force | Out-Null
    New-Item -ItemType Directory -Path $LogsDir -Force | Out-Null

    foreach ($Module in $Modules) {
        $Source = Join-Path $BuiltModsRoot ($Module + "\" + $BuildConfig)
        if (-not (Test-Path $Source)) {
            throw "Missing built module directory: $Source"
        }
        $Destination = Join-Path $ModsDir $Module
        New-Item -ItemType Directory -Path $Destination -Force | Out-Null
        Copy-Item (Join-Path $Source "*") $Destination -Recurse -Force
    }

    return @{
        CaseRoot = $CaseRoot
        ModsDir = $ModsDir
        LogsDir = $LogsDir
    }
}

if (-not (Test-Path $PlayerExe)) {
    throw "Player.exe not found: $PlayerExe"
}
if (-not (Test-Path $BuiltBml)) {
    throw "Built BML.dll not found: $BuiltBml"
}

New-Item -ItemType Directory -Path $ArtifactsRoot -Force | Out-Null
$BackupBml = Join-Path $ArtifactsRoot "BML.dll.backup"
Copy-Item $GameBml $BackupBml -Force
Copy-Item $BuiltBml $GameBml -Force

$Results = @()

try {
    foreach ($CaseName in $Cases) {
        if (-not $CaseMap.ContainsKey($CaseName)) {
            throw "Unknown case: $CaseName"
        }

        $Modules = [string[]]$CaseMap[$CaseName]
        $Layout = New-CaseLayout -CaseName $CaseName -Modules $Modules
        $Stdout = Join-Path $Layout.LogsDir "stdout.log"
        $Stderr = Join-Path $Layout.LogsDir "stderr.log"

        $env:BML_MODS_DIR = $Layout.ModsDir
        $Process = Start-Process -FilePath $PlayerExe `
            -WorkingDirectory (Split-Path $PlayerExe) `
            -RedirectStandardOutput $Stdout `
            -RedirectStandardError $Stderr `
            -PassThru

        Start-Sleep -Seconds $RuntimeSeconds

        if (-not $Process.HasExited) {
            Stop-Process -Id $Process.Id -Force
            Start-Sleep -Seconds 2
        }

        $StdoutText = if (Test-Path $Stdout) { Get-Content $Stdout -Raw } else { "" }
        $Loaded = $StdoutText -match "Modules loaded successfully"

        $Results += [pscustomobject]@{
            Case = $CaseName
            ModulesLoaded = $Loaded
            Modules = ($Modules -join ", ")
            Stdout = $Stdout
            Stderr = $Stderr
        }
    }
}
finally {
    Remove-Item Env:BML_MODS_DIR -ErrorAction SilentlyContinue
    if (Test-Path $BackupBml) {
        Copy-Item $BackupBml $GameBml -Force
    }
}

$SummaryPath = Join-Path $ArtifactsRoot "summary.json"
$Results | ConvertTo-Json -Depth 4 | Set-Content $SummaryPath
$Results | Format-Table -AutoSize
Write-Host "`nSummary: $SummaryPath"
