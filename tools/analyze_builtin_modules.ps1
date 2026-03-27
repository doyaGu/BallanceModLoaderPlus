<#
.SYNOPSIS
    Analyze built-in BML modules with real-game isolated runs and bootstrap checks.

.DESCRIPTION
    This script:
    1. Builds the selected configuration if needed
    2. Temporarily deploys BML.dll and ModLoader.dll to the real Ballance install
    3. Runs one full-stack baseline with all built-in modules plus BML_IntegrationTest
    4. Runs isolated real-game probe sessions for each built-in module with its minimal dependency set
    5. Captures bootstrap diagnostics, probe reports, stdout/stderr logs, and a JSON summary

.PARAMETER GameDir
    Path to the Ballance game directory.

.PARAMETER BuildDir
    Path to the CMake build directory.

.PARAMETER Config
    Build configuration. Default: Release

.PARAMETER TimeoutSeconds
    Max wait time for the full integration baseline report. Default: 120

.PARAMETER ProbeTimeoutSeconds
    Max wait time for each isolated probe report. Default: 90

.PARAMETER SkipBuild
    Skip the build step.

.PARAMETER NoRestore
    Do not restore BML.dll and ModLoader.dll after the run.

.PARAMETER OutputDir
    Directory for raw analysis artifacts. Default: <repo>\artifacts\builtin-module-health
#>

param(
    [string]$GameDir = "C:\Users\kakut\Games\Ballance",
    [string]$BuildDir = "",
    [string]$Config = "Release",
    [int]$TimeoutSeconds = 120,
    [int]$ProbeTimeoutSeconds = 90,
    [int]$ProbeMinDurationSeconds = 8,
    [int]$PauseBetweenRunsSeconds = 3,
    [switch]$SkipBuild,
    [switch]$NoRestore,
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) {
    $BuildDir = Join-Path $RepoRoot "build"
}
if (-not $OutputDir) {
    $OutputDir = Join-Path $RepoRoot "artifacts\builtin-module-health"
}

$Timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$RunOutputDir = Join-Path $OutputDir $Timestamp
$RunsDir = Join-Path $RunOutputDir "runs"

$BinDir = Join-Path $GameDir "Bin"
$BBDir = Join-Path $GameDir "BuildingBlocks"
$ModLoaderDir = Join-Path $GameDir "ModLoader"
$PlayerExe = Join-Path $BinDir "Player.exe"
$BackupDir = Join-Path $GameDir ".bml_builtin_analysis_backup"
$ProbeReportPath = Join-Path $ModLoaderDir "BuiltinModuleProbeReport.json"
$IntegrationReportPath = Join-Path $ModLoaderDir "IntegrationTestReport.json"

$BuildBML = Join-Path $BuildDir "bin\$Config\BML.dll"
$BuildModLoader = Join-Path $BuildDir "bin\$Config\ModLoader.dll"
$BuildDriver = Join-Path $BuildDir "bin\$Config\BMLCoreDriver.exe"
$BuildModsDir = Join-Path $BuildDir "Mods"

$BuiltinModules = @(
    [ordered]@{ Name = "BML_UI";         Id = "com.bml.ui";         Dependencies = @() },
    [ordered]@{ Name = "BML_Input";      Id = "com.bml.input";      Dependencies = @() },
    [ordered]@{ Name = "BML_Render";     Id = "com.bml.render";     Dependencies = @() },
    [ordered]@{ Name = "BML_Physics";    Id = "com.bml.physics";    Dependencies = @() },
    [ordered]@{ Name = "BML_ObjectLoad"; Id = "com.bml.objectload"; Dependencies = @() },
    [ordered]@{ Name = "BML_GameEvent";  Id = "com.bml.gameevent";  Dependencies = @("BML_ObjectLoad") },
    [ordered]@{ Name = "BML_Console";    Id = "com.bml.console";    Dependencies = @("BML_UI", "BML_Input") },
    [ordered]@{ Name = "BML_HUD";        Id = "com.bml.hud";        Dependencies = @("BML_UI", "BML_Input", "BML_Console", "BML_GameEvent") },
    [ordered]@{ Name = "BML_ModMenu";    Id = "com.bml.modmenu";    Dependencies = @("BML_UI", "BML_Input") },
    [ordered]@{ Name = "BML_MapMenu";    Id = "com.bml.mapmenu";    Dependencies = @("BML_UI", "BML_Input", "BML_Console", "BML_ObjectLoad") }
)

$ExpectedIntegrationTests = @(
    "api_load",
    "module_context",
    "global_context",
    "runtime_version",
    "mod_id",
    "logger",
    "subscriptions",
    "engine_init_received",
    "extension_register",
    "extension_query",
    "extension_load_use",
    "extension_unload",
    "extension_unregister",
    "extension_query_peer",
    "config_roundtrip",
    "imc_self_pubsub_setup",
    "frame_preprocess",
    "frame_postprocess",
    "frame_prerender",
    "frame_prerender_payload",
    "frame_postrender",
    "frame_postrender_payload",
    "frame_postspriterender",
    "frame_postspriterender_payload",
    "engine_play_received",
    "imc_self_pubsub_verify"
)

$BuiltinModuleByName = @{}
foreach ($Module in $BuiltinModules) {
    $BuiltinModuleByName[$Module.Name] = $Module
}

function Write-Step($Message) {
    Write-Host ">> $Message" -ForegroundColor Cyan
}

function Write-Ok($Message) {
    Write-Host "   [OK] $Message" -ForegroundColor Green
}

function Write-Warn($Message) {
    Write-Host "   [WARN] $Message" -ForegroundColor Yellow
}

function Write-Fail($Message) {
    Write-Host "   [FAIL] $Message" -ForegroundColor Red
}

function Ensure-Path($Path) {
    if (-not (Test-Path $Path)) {
        throw "Path not found: $Path"
    }
}

function Copy-ModuleArtifact {
    param(
        [Parameter(Mandatory = $true)][string]$ModuleName,
        [Parameter(Mandatory = $true)][string]$DestinationModsDir
    )

    $SourceDir = Join-Path $BuildModsDir "$ModuleName\$Config"
    if (-not (Test-Path $SourceDir)) {
        throw "Module build output not found for $ModuleName at $SourceDir"
    }

    $DestinationDir = Join-Path $DestinationModsDir $ModuleName
    New-Item -ItemType Directory -Force -Path $DestinationDir | Out-Null

    $Dll = Get-ChildItem -Path $SourceDir -Filter "*.dll" | Select-Object -First 1
    if (-not $Dll) {
        throw "No DLL found for $ModuleName in $SourceDir"
    }

    Copy-Item -Force $Dll.FullName (Join-Path $DestinationDir $Dll.Name)
    $Toml = Join-Path $SourceDir "mod.toml"
    if (Test-Path $Toml) {
        Copy-Item -Force $Toml (Join-Path $DestinationDir "mod.toml")
    }
}

function New-RunModsDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$RunName,
        [Parameter(Mandatory = $true)][string[]]$ModuleNames
    )

    $RunDir = Join-Path $RunsDir $RunName
    $ModsDir = Join-Path $RunDir "Mods"
    if (Test-Path $RunDir) {
        Remove-Item -Recurse -Force $RunDir
    }
    New-Item -ItemType Directory -Force -Path $ModsDir | Out-Null

    foreach ($ModuleName in $ModuleNames) {
        Copy-ModuleArtifact -ModuleName $ModuleName -DestinationModsDir $ModsDir
    }

    return [pscustomobject]@{
        RunDir = $RunDir
        ModsDir = $ModsDir
    }
}

function Resolve-ModuleSet {
    param(
        [Parameter(Mandatory = $true)][string]$ModuleName
    )

    $resolved = New-Object System.Collections.Generic.List[string]
    $visiting = New-Object System.Collections.Generic.HashSet[string]
    $visited = New-Object System.Collections.Generic.HashSet[string]

    function Visit-Module([string]$Name) {
        if ($visited.Contains($Name)) {
            return
        }

        if (-not $BuiltinModuleByName.ContainsKey($Name)) {
            throw "Unknown builtin module dependency: $Name"
        }

        if ($visiting.Contains($Name)) {
            throw "Circular builtin module dependency detected at $Name"
        }

        [void]$visiting.Add($Name)
        $ModuleInfo = $BuiltinModuleByName[$Name]
        foreach ($DependencyName in @($ModuleInfo.Dependencies)) {
            Visit-Module $DependencyName
        }
        [void]$visiting.Remove($Name)
        [void]$visited.Add($Name)
        [void]$resolved.Add($Name)
    }

    Visit-Module $ModuleName
    return @($resolved.ToArray())
}

function Parse-DriverLoadOrder {
    param([string]$StdoutPath)

    $LoadOrder = @()
    $Warnings = @()
    if (-not (Test-Path $StdoutPath)) {
        return [pscustomobject]@{ LoadOrder = $LoadOrder; Warnings = $Warnings }
    }

    foreach ($Line in Get-Content $StdoutPath) {
        if ($Line -match "Load order \(\d+\):\s*(.+)$") {
            $LoadOrder = ($Matches[1] -split ",") | ForEach-Object { $_.Trim() } | Where-Object { $_ }
        }
        if ($Line -like "Dependency warnings*" -or $Line -like "  - mod=*") {
            $Warnings += $Line
        }
    }

    return [pscustomobject]@{
        LoadOrder = $LoadOrder
        Warnings = $Warnings
    }
}

function Invoke-BootstrapDriver {
    param(
        [Parameter(Mandatory = $true)][string]$RunName,
        [Parameter(Mandatory = $true)][string]$ModsDir
    )

    $DriverStdout = Join-Path $RunsDir "$RunName.driver.stdout.log"
    $DriverStderr = Join-Path $RunsDir "$RunName.driver.stderr.log"
    if (Test-Path $DriverStdout) { Remove-Item -Force $DriverStdout }
    if (Test-Path $DriverStderr) { Remove-Item -Force $DriverStderr }

    $DriverProcess = Start-Process -FilePath $BuildDriver `
        -ArgumentList @("--mods", $ModsDir, "--list") `
        -WorkingDirectory (Split-Path -Parent $BuildDriver) `
        -PassThru `
        -RedirectStandardOutput $DriverStdout `
        -RedirectStandardError $DriverStderr
    $null = $DriverProcess.WaitForExit()

    $Parsed = Parse-DriverLoadOrder -StdoutPath $DriverStdout
    return [pscustomobject]@{
        ExitCode = $DriverProcess.ExitCode
        StdoutPath = $DriverStdout
        StderrPath = $DriverStderr
        LoadOrder = $Parsed.LoadOrder
        DependencyWarnings = $Parsed.Warnings
    }
}

function Set-ProcessEnv {
    param([hashtable]$Values)

    $Previous = @{}
    foreach ($Key in $Values.Keys) {
        $Previous[$Key] = [Environment]::GetEnvironmentVariable($Key, "Process")
        [Environment]::SetEnvironmentVariable($Key, $Values[$Key], "Process")
    }
    return $Previous
}

function Restore-ProcessEnv {
    param([hashtable]$Values)

    foreach ($Key in $Values.Keys) {
        [Environment]::SetEnvironmentVariable($Key, $Values[$Key], "Process")
    }
}

function Wait-ForJsonReport {
    param(
        [Parameter(Mandatory = $true)][string]$ReportPath,
        [Parameter(Mandatory = $true)][datetime]$Deadline,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process
    )

    $Report = $null
    while ((Get-Date) -lt $Deadline) {
        if (Test-Path $ReportPath) {
            try {
                $Report = Get-Content -Raw $ReportPath | ConvertFrom-Json
                return $Report
            } catch {
            }
        }

        if ($Process.HasExited) {
            break
        }

        Start-Sleep -Milliseconds 500
    }

    if (Test-Path $ReportPath) {
        try {
            return Get-Content -Raw $ReportPath | ConvertFrom-Json
        } catch {
        }
    }

    return $null
}

function Wait-ForIntegrationReport {
    param(
        [Parameter(Mandatory = $true)][datetime]$Deadline,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process
    )

    $Snapshot = $null
    while ((Get-Date) -lt $Deadline) {
        if (Test-Path $IntegrationReportPath) {
            try {
                $Report = Get-Content -Raw $IntegrationReportPath | ConvertFrom-Json
                $TestNames = @{}
                foreach ($Test in @($Report.tests)) {
                    if ($null -ne $Test.name) {
                        $TestNames[$Test.name] = $true
                    }
                }

                $Missing = @()
                foreach ($Name in $ExpectedIntegrationTests) {
                    if (-not $TestNames.ContainsKey($Name)) {
                        $Missing += $Name
                    }
                }

                $Summary = $Report.summary
                $Complete = ($null -ne $Summary.complete) -and [bool]$Summary.complete
                $RequiredComplete = ($null -ne $Summary.required_complete) -and [bool]$Summary.required_complete

                $Snapshot = [pscustomobject]@{
                    Report = $Report
                    Missing = $Missing
                    Complete = $Complete
                    RequiredComplete = $RequiredComplete
                }

                if ($Complete -and $RequiredComplete -and $Missing.Count -eq 0) {
                    return $Snapshot
                }
            } catch {
            }
        }

        if ($Process.HasExited) {
            break
        }

        Start-Sleep -Milliseconds 500
    }

    return $Snapshot
}

function Invoke-PlayerRun {
    param(
        [Parameter(Mandatory = $true)][string]$RunName,
        [Parameter(Mandatory = $true)][string]$ModsDir,
        [Parameter(Mandatory = $true)][int]$WaitSeconds,
        [Parameter(Mandatory = $true)][hashtable]$Environment,
        [Parameter(Mandatory = $true)][ValidateSet("integration", "probe")] [string]$Mode
    )

    $StdoutPath = Join-Path $RunsDir "$RunName.stdout.log"
    $StderrPath = Join-Path $RunsDir "$RunName.stderr.log"
    $ReportCopyPath = if ($Mode -eq "integration") {
        Join-Path $RunsDir "$RunName.integration-report.json"
    } else {
        Join-Path $RunsDir "$RunName.probe-report.json"
    }
    if (Test-Path $StdoutPath) { Remove-Item -Force $StdoutPath }
    if (Test-Path $StderrPath) { Remove-Item -Force $StderrPath }
    if (Test-Path $ReportCopyPath) { Remove-Item -Force $ReportCopyPath }

    if ($Mode -eq "integration" -and (Test-Path $IntegrationReportPath)) {
        Remove-Item -Force $IntegrationReportPath
    }
    if ($Mode -eq "probe" -and (Test-Path $ProbeReportPath)) {
        Remove-Item -Force $ProbeReportPath
    }

    $PreviousEnv = Set-ProcessEnv -Values $Environment
    try {
        $Process = Start-Process -FilePath $PlayerExe `
            -WorkingDirectory $BinDir `
            -PassThru `
            -RedirectStandardOutput $StdoutPath `
            -RedirectStandardError $StderrPath

        $Deadline = (Get-Date).AddSeconds($WaitSeconds)
        if ($Mode -eq "integration") {
            $Report = Wait-ForIntegrationReport -Deadline $Deadline -Process $Process
        } else {
            $Report = Wait-ForJsonReport -ReportPath $ProbeReportPath -Deadline $Deadline -Process $Process
        }

        if ($null -ne $Report) {
            if ($Mode -eq "integration" -and (Test-Path $IntegrationReportPath)) {
                Copy-Item -Force $IntegrationReportPath $ReportCopyPath
            }
            if ($Mode -eq "probe" -and (Test-Path $ProbeReportPath)) {
                Copy-Item -Force $ProbeReportPath $ReportCopyPath
            }
            Start-Sleep -Seconds 1
        }

        if (-not $Process.HasExited) {
            Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
            Start-Sleep -Seconds 1
        }

        return [pscustomobject]@{
            Report = $Report
            ReportCopyPath = $ReportCopyPath
            StdoutPath = $StdoutPath
            StderrPath = $StderrPath
            ExitCode = if ($Process.HasExited) { $Process.ExitCode } else { $null }
        }
    } finally {
        Restore-ProcessEnv -Values $PreviousEnv
    }
}

function Get-ProbeAssessment {
    param(
        [Parameter(Mandatory = $true)]$Module,
        [Parameter(Mandatory = $true)]$ProbeReport
    )

    $Loaded = @($ProbeReport.loaded_modules) -contains $Module.Id
    $Configs = @()
    if ($null -ne $ProbeReport.configs.PSObject.Properties[$Module.Id]) {
        $Configs = @($ProbeReport.configs.($Module.Id))
    }

    $Result = [ordered]@{
        loaded = $Loaded
        satisfied = $false
        signals = @()
        missing = @()
    }

    if (-not $Loaded) {
        $Result.missing += "module_not_loaded"
        return [pscustomobject]$Result
    }

    switch ($Module.Id) {
        "com.bml.ui" {
            if ($ProbeReport.flags.ui_extension_loaded) { $Result.signals += "ui_extension_loaded" } else { $Result.missing += "ui_extension_loaded" }
            if ($ProbeReport.flags.ui_imgui_context_live) { $Result.signals += "imgui_context_live" } else { $Result.missing += "imgui_context_live" }
            if ($ProbeReport.counts.ui_draw -gt 0) { $Result.signals += "ui_draw_events" } else { $Result.missing += "ui_draw_events" }
        }
        "com.bml.input" {
            if ($ProbeReport.flags.input_extension_loaded) { $Result.signals += "input_extension_loaded" } else { $Result.missing += "input_extension_loaded" }
        }
        "com.bml.render" {
            if ($ProbeReport.counts.pre_render -gt 0) { $Result.signals += "pre_render_events" } else { $Result.missing += "pre_render_events" }
            if ($ProbeReport.counts.post_render -gt 0) { $Result.signals += "post_render_events" } else { $Result.missing += "post_render_events" }
        }
        "com.bml.physics" {
            if ($ProbeReport.counts.physics_physicalize -gt 0 -or $ProbeReport.counts.physics_unphysicalize -gt 0) {
                $Result.signals += "physics_topic_activity"
            } else {
                $Result.missing += "physics_topic_activity"
            }
        }
        "com.bml.objectload" {
            if ($ProbeReport.counts.objectload_script -gt 0) { $Result.signals += "objectload_script_events" } else { $Result.missing += "objectload_script_events" }
        }
        "com.bml.gameevent" {
            if ($ProbeReport.counts.menu_post_start -gt 0) { $Result.signals += "menu_post_start_event" } else { $Result.missing += "menu_post_start_event" }
        }
        "com.bml.console" {
            if ($ProbeReport.flags.console_extension_loaded) { $Result.signals += "console_extension_loaded" } else { $Result.missing += "console_extension_loaded" }
            if ([int]$ProbeReport.command_results.help -eq 0) { $Result.signals += "help_command_ok" } else { $Result.missing += "help_command_ok" }
            if ($ProbeReport.flags.console_window_visible) { $Result.signals += "console_window_visible" } else { $Result.missing += "console_window_visible" }
        }
        "com.bml.hud" {
            if ($Configs.Count -gt 0) { $Result.signals += "hud_config_initialized" } else { $Result.missing += "hud_config_initialized" }
            if ([int]$ProbeReport.command_results.hud_show -eq 0) { $Result.signals += "hud_command_ok" } else { $Result.missing += "hud_command_ok" }
        }
        "com.bml.modmenu" {
            if ($ProbeReport.flags.modmenu_window_visible) { $Result.signals += "modmenu_window_visible" } else { $Result.missing += "modmenu_window_visible" }
        }
        "com.bml.mapmenu" {
            if ($Configs.Count -gt 0) { $Result.signals += "mapmenu_config_initialized" } else { $Result.missing += "mapmenu_config_initialized" }
            if ([int]$ProbeReport.command_results.mapmenu_open -eq 0 -or [int]$ProbeReport.command_results.mapmenu_refresh -eq 0) {
                $Result.signals += "mapmenu_command_ok"
            } else {
                $Result.missing += "mapmenu_command_ok"
            }
            if ($ProbeReport.flags.mapmenu_window_visible -or $ProbeReport.flags.mapmenu_button_visible) {
                $Result.signals += "mapmenu_ui_visible"
            }
        }
    }

    $Result.satisfied = ($Result.missing.Count -eq 0)
    return [pscustomobject]$Result
}

function Backup-And-DeployCore {
    if (Test-Path $BackupDir) {
        Remove-Item -Recurse -Force $BackupDir
    }
    New-Item -ItemType Directory -Force -Path $BackupDir | Out-Null

    $GameBML = Join-Path $BinDir "BML.dll"
    $GameModLoader = Join-Path $BBDir "ModLoader.dll"
    if (Test-Path $GameBML) {
        Copy-Item -Force $GameBML (Join-Path $BackupDir "BML.dll")
    }
    if (Test-Path $GameModLoader) {
        Copy-Item -Force $GameModLoader (Join-Path $BackupDir "ModLoader.dll")
    }

    Copy-Item -Force $BuildBML $GameBML
    Copy-Item -Force $BuildModLoader $GameModLoader
}

function Restore-Core {
    $GameBML = Join-Path $BinDir "BML.dll"
    $GameModLoader = Join-Path $BBDir "ModLoader.dll"
    $BackupBML = Join-Path $BackupDir "BML.dll"
    $BackupModLoader = Join-Path $BackupDir "ModLoader.dll"

    if (Test-Path $BackupBML) {
        Copy-Item -Force $BackupBML $GameBML
    }
    if (Test-Path $BackupModLoader) {
        Copy-Item -Force $BackupModLoader $GameModLoader
    }
    if (Test-Path $BackupDir) {
        Remove-Item -Recurse -Force $BackupDir
    }
}

Write-Step "Preparing analysis workspace"
New-Item -ItemType Directory -Force -Path $RunsDir | Out-Null

Write-Step "Validating prerequisites"
Ensure-Path $PlayerExe
Ensure-Path $BuildDir

if (-not $SkipBuild) {
    Write-Step "Building project (config=$Config)"
    cmake --build $BuildDir --config $Config 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }
    Write-Ok "Build succeeded"
}

Ensure-Path $BuildBML
Ensure-Path $BuildModLoader
Ensure-Path $BuildDriver
Ensure-Path $BuildModsDir

foreach ($Module in $BuiltinModules) {
    Ensure-Path (Join-Path $BuildModsDir "$($Module.Name)\$Config")
}
Ensure-Path (Join-Path $BuildModsDir "BML_IntegrationTest\$Config")

Write-Step "Deploying core binaries to the real game install"
Backup-And-DeployCore
Write-Ok "Core binaries deployed"

$Analysis = [ordered]@{
    generated_at = (Get-Date).ToString("s")
    game_dir = $GameDir
    build_dir = $BuildDir
    config = $Config
    output_dir = $RunOutputDir
    baseline = $null
    modules = @()
}

try {
    Write-Step "Running full-stack baseline"
    $BaselineModules = $BuiltinModules.Name + @("BML_IntegrationTest")
    $BaselineLayout = New-RunModsDirectory -RunName "baseline" -ModuleNames $BaselineModules
    $BaselineDriver = Invoke-BootstrapDriver -RunName "baseline" -ModsDir $BaselineLayout.ModsDir
    $BaselinePlayer = Invoke-PlayerRun -RunName "baseline" `
        -ModsDir $BaselineLayout.ModsDir `
        -WaitSeconds $TimeoutSeconds `
        -Environment @{ BML_MODS_DIR = $BaselineLayout.ModsDir; BML_INTTEST_MODE = $null; BML_PROBE_TARGET = $null } `
        -Mode "integration"

    $Analysis.baseline = [ordered]@{
        modules = $BaselineModules
        driver = $BaselineDriver
        runtime = $BaselinePlayer
    }

    if ($null -eq $BaselinePlayer.Report) {
        Write-Warn "Full baseline did not produce a complete integration report"
    } else {
        Write-Ok "Full baseline report captured"
    }

    foreach ($Module in $BuiltinModules) {
        $RunName = $Module.Name.ToLowerInvariant()
        $ResolvedModules = @(Resolve-ModuleSet -ModuleName $Module.Name)
        $ModuleSet = @($ResolvedModules) + @("BML_IntegrationTest") | Select-Object -Unique
        Write-Step "Running isolated probe for $($Module.Name) with modules: $($ModuleSet -join ', ')"

        $Layout = New-RunModsDirectory -RunName $RunName -ModuleNames $ModuleSet
        $Driver = Invoke-BootstrapDriver -RunName $RunName -ModsDir $Layout.ModsDir
        $Player = Invoke-PlayerRun -RunName $RunName `
            -ModsDir $Layout.ModsDir `
            -WaitSeconds $ProbeTimeoutSeconds `
            -Environment @{
                BML_MODS_DIR = $Layout.ModsDir
                BML_INTTEST_MODE = "probe"
                BML_PROBE_TARGET = $Module.Id
                BML_PROBE_MIN_DURATION_MS = [string]($ProbeMinDurationSeconds * 1000)
            } `
            -Mode "probe"

        $Assessment = $null
        if ($null -ne $Player.Report) {
            $Assessment = Get-ProbeAssessment -Module $Module -ProbeReport $Player.Report
        }

        $Analysis.modules += [ordered]@{
            name = $Module.Name
            id = $Module.Id
            dependencies = $Module.Dependencies
            resolved_dependencies = $ResolvedModules | Where-Object { $_ -ne $Module.Name }
            enabled_modules = $ModuleSet
            driver = $Driver
            runtime = $Player
            assessment = $Assessment
        }

        if ($null -eq $Player.Report) {
            Write-Warn "$($Module.Name): probe report missing"
        } elseif ($Assessment.satisfied) {
            Write-Ok "$($Module.Name): required runtime signals satisfied"
        } else {
            Write-Warn "$($Module.Name): missing signals: $($Assessment.missing -join ', ')"
        }

        Start-Sleep -Seconds $PauseBetweenRunsSeconds
    }

    $SummaryPath = Join-Path $RunOutputDir "builtin-module-health.json"
    $Analysis | ConvertTo-Json -Depth 10 | Set-Content -Path $SummaryPath -Encoding UTF8
    Write-Ok "Raw analysis summary written to $SummaryPath"
}
finally {
    if (-not $NoRestore) {
        Write-Step "Restoring core binaries"
        Restore-Core
        Write-Ok "Core binaries restored"
    } else {
        Write-Warn "Skipping restore (--NoRestore). Backup remains at $BackupDir"
    }
}
