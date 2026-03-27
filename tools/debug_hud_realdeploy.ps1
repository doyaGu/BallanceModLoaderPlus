param(
    [string]$GameDir = "C:\Users\kakut\Games\Ballance",
    [string]$Config = "Release",
    [int]$RuntimeSeconds = 18,
    [string[]]$Modules = @("BML_UI", "BML_Input", "BML_Console", "BML_ObjectLoad", "BML_GameEvent", "BML_HUD"),
    [switch]$BuildFirst,
    [string[]]$BuildTargets = @(),
    [switch]$PreserveDeployment,
    [string]$InputScenario = ""
)

$ErrorActionPreference = "Stop"

function Normalize-ListParameter {
    param(
        [string[]]$Values
    )

    $normalized = @()
    foreach ($Value in $Values) {
        if ([string]::IsNullOrWhiteSpace($Value)) {
            continue
        }

        foreach ($Item in ($Value -split "\s*,\s*")) {
            if (-not [string]::IsNullOrWhiteSpace($Item)) {
                $normalized += $Item
            }
        }
    }

    return $normalized
}

$Modules = Normalize-ListParameter $Modules
$BuildTargets = Normalize-ListParameter $BuildTargets

$RepoRoot = Split-Path -Parent $PSScriptRoot
$ArtifactRoot = Join-Path $RepoRoot ("artifacts\hud-realdeploy-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
$BackupRoot = Join-Path $ArtifactRoot "backup"
$GameBin = Join-Path $GameDir "Bin"
$GameBB = Join-Path $GameDir "BuildingBlocks"
$GameMods = Join-Path $GameDir "Mods"
$GameLoader = Join-Path $GameDir "ModLoader"
$PlayerExe = Join-Path $GameBin "Player.exe"

$BuiltBml = Join-Path $RepoRoot ("build\bin\" + $Config + "\BML.dll")
$BuiltModLoader = Join-Path $RepoRoot ("build\bin\" + $Config + "\ModLoader.dll")
$BuiltModsRoot = Join-Path $RepoRoot "build\Mods"

$DstBml = Join-Path $GameBin "BML.dll"
$DstModLoader = Join-Path $GameBB "ModLoader.dll"
$Stdout = Join-Path $ArtifactRoot "stdout.log"
$Stderr = Join-Path $ArtifactRoot "stderr.log"
$Screenshot = Join-Path $ArtifactRoot "window.png"

function Backup-IfExists {
    param(
        [string]$Source,
        [string]$Backup
    )

    if (Test-Path $Source) {
        New-Item -ItemType Directory -Path (Split-Path $Backup) -Force | Out-Null
        Copy-Item $Source $Backup -Recurse -Force
        return $true
    }

    return $false
}

function Test-SameFile {
    param(
        [string]$Left,
        [string]$Right
    )

    if (-not (Test-Path $Left) -or -not (Test-Path $Right)) {
        return $false
    }

    $leftInfo = Get-Item $Left
    $rightInfo = Get-Item $Right
    if ($leftInfo.Length -ne $rightInfo.Length) {
        return $false
    }

    return (Get-FileHash $Left -Algorithm SHA256).Hash -eq (Get-FileHash $Right -Algorithm SHA256).Hash
}

function Restore-OrRemove {
    param(
        [string]$Target,
        [string]$Backup,
        [bool]$HadOriginal
    )

    if ($HadOriginal -and (Test-Path $Backup)) {
        if (Test-Path $Target) {
            Remove-Item $Target -Recurse -Force
        }
        Copy-Item $Backup $Target -Recurse -Force
    } else {
        Remove-Item $Target -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Wait-MainWindow {
    param(
        [System.Diagnostics.Process]$Process,
        [int]$TimeoutSeconds = 15
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $Process.Refresh()
        if ($Process.MainWindowHandle -ne [IntPtr]::Zero) {
            return $Process.MainWindowHandle
        }
        Start-Sleep -Milliseconds 500
    }

    return [IntPtr]::Zero
}

function Save-WindowScreenshot {
    param(
        [IntPtr]$WindowHandle,
        [string]$OutputPath
    )

    if ($WindowHandle -eq [IntPtr]::Zero) {
        return $false
    }

    Add-Type -AssemblyName System.Drawing
    Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class BmlWindowCapture {
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }
}
"@

    $rect = New-Object BmlWindowCapture+RECT
    if (-not [BmlWindowCapture]::GetWindowRect($WindowHandle, [ref]$rect)) {
        return $false
    }

    $width = [Math]::Max(1, $rect.Right - $rect.Left)
    $height = [Math]::Max(1, $rect.Bottom - $rect.Top)

    $bitmap = New-Object System.Drawing.Bitmap $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
    $bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose()
    $bitmap.Dispose()
    return $true
}

function Initialize-InputScenario {
    param(
        [string]$Scenario,
        [string]$LoaderDir
    )

    if ([string]::IsNullOrWhiteSpace($Scenario)) {
        return
    }

    switch ($Scenario.ToLowerInvariant()) {
        "console-basics" {
            $historyPath = Join-Path $LoaderDir "CommandBar.history"
            $esc = [char]27
            $historyLines = @(
                "echo ${esc}[31mANSI TEST${esc}[0m",
                "help"
            )
            Set-Content -Path $historyPath -Value $historyLines -Encoding utf8
        }
        default {
            throw "Unknown input scenario: $Scenario"
        }
    }
}

function Invoke-InputScenario {
    param(
        [string]$Scenario,
        [System.Diagnostics.Process]$Process,
        [IntPtr]$WindowHandle
    )

    if ([string]::IsNullOrWhiteSpace($Scenario)) {
        return
    }

    Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class BmlForegroundWindow {
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [StructLayout(LayoutKind.Sequential)]
    public struct INPUT {
        public uint type;
        public InputUnion U;
    }

    [StructLayout(LayoutKind.Explicit)]
    public struct InputUnion {
        [FieldOffset(0)]
        public KEYBDINPUT ki;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct KEYBDINPUT {
        public ushort wVk;
        public ushort wScan;
        public uint dwFlags;
        public uint time;
        public IntPtr dwExtraInfo;
    }

    [DllImport("user32.dll", SetLastError=true)]
    public static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);
}
"@

    if ($WindowHandle -eq [IntPtr]::Zero) {
        throw "Main window was not created for scenario '$Scenario'"
    }

    [BmlForegroundWindow]::ShowWindow($WindowHandle, 9) | Out-Null
    [BmlForegroundWindow]::SetForegroundWindow($WindowHandle) | Out-Null

    Start-Sleep -Seconds 4

    function Send-Key {
        param(
            [UInt16]$VirtualKey
        )

        $inputs = @(
            (New-Object BmlForegroundWindow+INPUT),
            (New-Object BmlForegroundWindow+INPUT)
        )
        $inputs[0].type = 1
        $inputs[0].U.ki.wVk = $VirtualKey
        $inputs[1].type = 1
        $inputs[1].U.ki.wVk = $VirtualKey
        $inputs[1].U.ki.dwFlags = 0x0002
        [void][BmlForegroundWindow]::SendInput([uint32]$inputs.Count, $inputs, [Runtime.InteropServices.Marshal]::SizeOf([type][BmlForegroundWindow+INPUT]))
    }

    function Send-TextChar {
        param(
            [char]$Character
        )

        switch ($Character) {
            '/' { Send-Key 0xBF; return }
            'h' { Send-Key 0x48; return }
            default { throw "Unsupported scenario character: $Character" }
        }
    }

    switch ($Scenario.ToLowerInvariant()) {
        "console-basics" {
            Send-TextChar '/'
            Start-Sleep -Milliseconds 700
            Send-Key 0x26
            Start-Sleep -Milliseconds 400
            Send-Key 0x0D

            Start-Sleep -Milliseconds 1200
            Send-TextChar '/'
            Start-Sleep -Milliseconds 600
            Send-TextChar 'h'
            Start-Sleep -Milliseconds 400
            Send-Key 0x09
            Start-Sleep -Milliseconds 700
            Send-Key 0x27
            Start-Sleep -Milliseconds 400
            Send-Key 0x0D

            Start-Sleep -Milliseconds 1200
            Send-TextChar '/'
            Start-Sleep -Milliseconds 600
            Send-Key 0x26
            Start-Sleep -Milliseconds 400
            Send-Key 0x0D
        }
    }
}

if (-not (Test-Path $PlayerExe)) {
    throw "Player.exe not found at $PlayerExe"
}
if (-not (Test-Path $BuiltBml)) {
    throw "Built BML.dll not found at $BuiltBml"
}
if (-not (Test-Path $BuiltModLoader)) {
    throw "Built ModLoader.dll not found at $BuiltModLoader"
}

if ($BuildFirst) {
    if (-not $BuildTargets -or $BuildTargets.Count -eq 0) {
        $BuildTargets = $Modules | Sort-Object -Unique
    }

    & cmake --build (Join-Path $RepoRoot "build") --config $Config --target @BuildTargets
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed for targets: $($BuildTargets -join ', ')"
    }
}

New-Item -ItemType Directory -Path $ArtifactRoot, $BackupRoot, $GameMods -Force | Out-Null

$HadBml = $false
if ((Test-Path $DstBml) -and -not (Test-SameFile -Left $DstBml -Right $BuiltBml)) {
    $HadBml = Backup-IfExists -Source $DstBml -Backup (Join-Path $BackupRoot "Bin\BML.dll")
}

$HadModLoader = $false
if ((Test-Path $DstModLoader) -and -not (Test-SameFile -Left $DstModLoader -Right $BuiltModLoader)) {
    $HadModLoader = Backup-IfExists -Source $DstModLoader -Backup (Join-Path $BackupRoot "BuildingBlocks\ModLoader.dll")
}
$HadMods = Backup-IfExists -Source $GameMods -Backup (Join-Path $BackupRoot "Mods")

try {
    Copy-Item $BuiltBml $DstBml -Force
    Copy-Item $BuiltModLoader $DstModLoader -Force

    Remove-Item (Join-Path $GameMods "*") -Recurse -Force -ErrorAction SilentlyContinue
    foreach ($Module in $Modules) {
        $SourceDir = Join-Path $BuiltModsRoot ($Module + "\" + $Config)
        if (-not (Test-Path $SourceDir)) {
            throw "Missing built module: $SourceDir"
        }

        $DestDir = Join-Path $GameMods $Module
        New-Item -ItemType Directory -Path $DestDir -Force | Out-Null
        Copy-Item (Join-Path $SourceDir "*") $DestDir -Recurse -Force
    }

    Remove-Item $Stdout, $Stderr, $Screenshot -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $GameLoader "stdout.log"), (Join-Path $GameLoader "stderr.log") -Force -ErrorAction SilentlyContinue
    Initialize-InputScenario -Scenario $InputScenario -LoaderDir $GameLoader

    $Process = Start-Process -FilePath $PlayerExe `
        -WorkingDirectory $GameBin `
        -RedirectStandardOutput $Stdout `
        -RedirectStandardError $Stderr `
        -PassThru

    $WindowHandle = Wait-MainWindow -Process $Process -TimeoutSeconds 20
    Invoke-InputScenario -Scenario $InputScenario -Process $Process -WindowHandle $WindowHandle
    Start-Sleep -Seconds $RuntimeSeconds
    Save-WindowScreenshot -WindowHandle $WindowHandle -OutputPath $Screenshot | Out-Null

    if (-not $Process.HasExited) {
        Stop-Process -Id $Process.Id -Force
        Start-Sleep -Seconds 2
    }

    foreach ($Log in Get-ChildItem -Path $GameMods -Recurse -Filter "*.log" -ErrorAction SilentlyContinue) {
        $Relative = $Log.FullName.Substring($GameMods.Length).TrimStart("\")
        $Dest = Join-Path $ArtifactRoot ("Mods\" + $Relative)
        New-Item -ItemType Directory -Path (Split-Path $Dest) -Force | Out-Null
        Copy-Item $Log.FullName $Dest -Force
    }

    $GameStdout = Join-Path $GameLoader "stdout.log"
    $GameStderr = Join-Path $GameLoader "stderr.log"
    if (Test-Path $GameStdout) {
        Copy-Item $GameStdout (Join-Path $ArtifactRoot "game-stdout.log") -Force
    }
    if (Test-Path $GameStderr) {
        Copy-Item $GameStderr (Join-Path $ArtifactRoot "game-stderr.log") -Force
    }
}
finally {
    if (-not $PreserveDeployment) {
        Restore-OrRemove -Target $DstBml -Backup (Join-Path $BackupRoot "Bin\BML.dll") -HadOriginal $HadBml
        Restore-OrRemove -Target $DstModLoader -Backup (Join-Path $BackupRoot "BuildingBlocks\ModLoader.dll") -HadOriginal $HadModLoader
        Restore-OrRemove -Target $GameMods -Backup (Join-Path $BackupRoot "Mods") -HadOriginal $HadMods
    }
}

Write-Output $ArtifactRoot
