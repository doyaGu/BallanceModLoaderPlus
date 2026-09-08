Set-StrictMode -Version Latest

Import-Module (Join-Path $PSScriptRoot '..\..\scripts\lib\BMLProject.psm1')

# Shared machinery for the Ballance Player acceptance runners. Every runner
# starts the real game, accepts the FullScreen Setup dialog, captures window
# screenshots and drives the tutorial exit key the same way, so those pieces
# live here instead of being duplicated per runner.

if (-not ('BMLPlayerDialog' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class BMLPlayerDialog {
    private delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);

    [StructLayout(LayoutKind.Sequential)]
    public struct Rect {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Point {
        public int X;
        public int Y;
    }

    [DllImport("user32.dll")]
    public static extern IntPtr GetDlgItem(IntPtr dialog, int controlId);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr window, out Rect rect);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr window, out Rect rect);

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr window, ref Point point);

    [DllImport("user32.dll")]
    public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc callback,
                                            IntPtr parameter);

    [DllImport("user32.dll")]
    private static extern bool EnumChildWindows(IntPtr parent,
                                                 EnumWindowsProc callback,
                                                 IntPtr parameter);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr window,
                                                        out uint processId);

    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr window);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowText(IntPtr window, char[] text,
                                            int capacity);

    public static IntPtr FindVisibleWindow(int processId, string title) {
        IntPtr found = IntPtr.Zero;
        long smallestArea = long.MaxValue;
        Action<IntPtr> consider = delegate(IntPtr window) {
            uint owner;
            if (!IsWindowVisible(window) ||
                GetWindowThreadProcessId(window, out owner) == 0 ||
                owner != (uint)processId)
                return;
            char[] text = new char[256];
            int length = GetWindowText(window, text, text.Length);
            if (length <= 0 ||
                !string.Equals(new string(text, 0, length), title,
                               StringComparison.Ordinal))
                return;
            Rect rect;
            if (!GetWindowRect(window, out rect))
                return;
            long area = (long)(rect.Right - rect.Left) *
                        (rect.Bottom - rect.Top);
            if (area > 0 && area < smallestArea) {
                found = window;
                smallestArea = area;
            }
        };
        EnumWindows(delegate(IntPtr window, IntPtr parameter) {
            consider(window);
            EnumChildWindows(window,
                delegate(IntPtr child, IntPtr childParameter) {
                    consider(child);
                    return true;
                }, IntPtr.Zero);
            return true;
        }, IntPtr.Zero);
        return found;
    }

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr window, uint message,
                                             IntPtr wParam, IntPtr lParam);
}
'@
}

if (-not ('BMLPlayerInput' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class BMLPlayerInput {
    [DllImport("user32.dll")]
    private static extern void keybd_event(byte virtualKey, byte scanCode,
                                            uint flags, UIntPtr extraInfo);
    [DllImport("user32.dll")]
    private static extern uint MapVirtualKey(uint code, uint mapType);

    private static uint Flags(byte virtualKey, bool released) {
        uint flags = released ? 2u : 0u;
        if (virtualKey >= 0x21 && virtualKey <= 0x28)
            flags |= 1u;
        return flags;
    }

    public static void KeyDown(byte virtualKey) {
        keybd_event(virtualKey, (byte)MapVirtualKey(virtualKey, 0),
                    Flags(virtualKey, false), UIntPtr.Zero);
    }

    public static void KeyUp(byte virtualKey) {
        keybd_event(virtualKey, (byte)MapVirtualKey(virtualKey, 0),
                    Flags(virtualKey, true), UIntPtr.Zero);
    }
}
'@
}

# The tutorial exit key. Level 01 parks gameplay input until the shipped
# tutorial has consumed it, so every runner has to press it.
$script:TutorialExitVirtualKey = 0x51

function Save-PlayerWindow {
    param(
        [Parameter(Mandatory = $true)]
        [System.Diagnostics.Process]$Process,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $previousDpi = [BMLPlayerDialog]::SetThreadDpiAwarenessContext(
        [IntPtr](-4))
    try {
        $Process.Refresh()
        $window = [BMLPlayerDialog]::FindVisibleWindow($Process.Id, 'Ballance')
        if ($window -eq [IntPtr]::Zero) {
            $window = $Process.MainWindowHandle
        }
        if ($window -eq [IntPtr]::Zero) {
            return $false
        }
        $rect = New-Object BMLPlayerDialog+Rect
        $origin = New-Object BMLPlayerDialog+Point
        if (-not [BMLPlayerDialog]::GetClientRect($window, [ref]$rect) -or
            -not [BMLPlayerDialog]::ClientToScreen($window, [ref]$origin)) {
            return $false
        }
        $width = $rect.Right
        $height = $rect.Bottom
        if ($width -le 0 -or $height -le 0) {
            return $false
        }

        Add-Type -AssemblyName System.Drawing
        $bitmap = [System.Drawing.Bitmap]::new($width, $height)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.CopyFromScreen($origin.X, $origin.Y, 0, 0,
                [System.Drawing.Size]::new($width, $height))
            $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
        } finally {
            $graphics.Dispose()
            $bitmap.Dispose()
        }
        return (Test-Path -LiteralPath $Path) -and
            (Get-Item -LiteralPath $Path).Length -gt 0
    } finally {
        if ($previousDpi -ne [IntPtr]::Zero) {
            [void][BMLPlayerDialog]::SetThreadDpiAwarenessContext($previousDpi)
        }
    }
}

function Test-ImageDimensions {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][int]$Width,
        [Parameter(Mandatory = $true)][int]$Height
    )
    if (-not (Test-Path -LiteralPath $Path)) {
        return $false
    }
    Add-Type -AssemblyName System.Drawing
    $image = [System.Drawing.Image]::FromFile($Path)
    try {
        return $image.Width -eq $Width -and $image.Height -eq $Height
    } finally {
        $image.Dispose()
    }
}

function Confirm-PlayerSetupDialog {
    param(
        [Parameter(Mandatory = $true)]
        [IntPtr]$Dialog
    )

    $wmCommand = 0x0111
    $lbGetCurSel = 0x0188
    $lbSetCurSel = 0x0186
    $driverId = 1007
    $screenModeId = 1008
    $driver = [BMLPlayerDialog]::GetDlgItem($Dialog, $driverId)
    $screenMode = [BMLPlayerDialog]::GetDlgItem($Dialog, $screenModeId)
    if ($driver -eq [IntPtr]::Zero -or $screenMode -eq [IntPtr]::Zero) {
        return $false
    }

    if ([BMLPlayerDialog]::SendMessage($driver, $lbGetCurSel,
                                      [IntPtr]::Zero, [IntPtr]::Zero).ToInt64() -lt 0) {
        [void][BMLPlayerDialog]::SendMessage($driver, $lbSetCurSel,
                                             [IntPtr]::Zero, [IntPtr]::Zero)
        $selectionChanged = [IntPtr]((1 -shl 16) -bor $driverId)
        [void][BMLPlayerDialog]::SendMessage($Dialog, $wmCommand,
                                             $selectionChanged, $driver)
    }
    if ([BMLPlayerDialog]::SendMessage($screenMode, $lbGetCurSel,
                                      [IntPtr]::Zero, [IntPtr]::Zero).ToInt64() -lt 0) {
        [void][BMLPlayerDialog]::SendMessage($screenMode, $lbSetCurSel,
                                             [IntPtr]::Zero, [IntPtr]::Zero)
    }
    if ([BMLPlayerDialog]::SendMessage($screenMode, $lbGetCurSel,
                                      [IntPtr]::Zero, [IntPtr]::Zero).ToInt64() -lt 0) {
        return $false
    }

    [void][BMLPlayerDialog]::SendMessage($Dialog, $wmCommand,
                                         [IntPtr]1, [IntPtr]::Zero)
    return $true
}

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

# Starts the Player and brings its window to the front, answering the
# FullScreen Setup dialog when the install is configured to show it. The
# Process is always handed back, so the caller can stop it even when the
# dialog could not be answered.
function Start-BMLPlayerProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PlayerPath,

        [Parameter(Mandatory = $true)]
        [string[]]$ArgumentList,

        [Parameter(Mandatory = $true)]
        $WindowShell
    )

    $process = Start-Process -FilePath $PlayerPath `
        -ArgumentList $ArgumentList `
        -WorkingDirectory (Split-Path -Parent $PlayerPath) -PassThru
    $activated = $false
    $setupAccepted = $false
    $setupFailed = $false
    for ($attempt = 0; $attempt -lt 100 -and -not $process.HasExited; ++$attempt) {
        Start-Sleep -Milliseconds 100
        $process.Refresh()
        if ($process.MainWindowHandle -ne [IntPtr]::Zero -and
            $WindowShell.AppActivate($process.Id)) {
            if ($process.MainWindowTitle -eq 'FullScreen Setup') {
                if (-not (Confirm-PlayerSetupDialog $process.MainWindowHandle)) {
                    $setupFailed = $true
                    break
                }
                $setupAccepted = $true
                continue
            }
            $activated = $true
            break
        }
    }

    return [pscustomobject]@{
        Process = $process
        WindowActivated = $activated
        SetupDialogAccepted = $setupAccepted
        SetupDialogFailed = $setupFailed
    }
}

function New-BMLTutorialExitState {
    return [pscustomobject]@{
        Stage = 'waiting-for-exit-listener'
        StageAt = Get-Date
        Injected = $false
    }
}

# Advances the tutorial exit handshake once per poll. Control readiness gates
# on the shipped tutorial consuming its exit key, so the runner presses it and
# keeps pressing it while the listener stays up.
function Step-BMLTutorialExit {
    param(
        [Parameter(Mandatory = $true)]
        $State,

        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$LiveLog,

        [Parameter(Mandatory = $true)]
        $WindowShell,

        [Parameter(Mandatory = $true)]
        [int]$ProcessId,

        [datetime]$Now = (Get-Date)
    )

    if ($State.Stage -eq 'waiting-for-exit-listener' -and
        $LiveLog.Contains('Gameplay tutorial: exit_ready=true')) {
        $State.Stage = 'exit-listener-settle'
        $State.StageAt = $Now
    } elseif ($State.Stage -eq 'exit-listener-settle' -and
              ($Now - $State.StageAt).TotalMilliseconds -ge 250) {
        [void]$WindowShell.AppActivate($ProcessId)
        [BMLPlayerInput]::KeyDown($script:TutorialExitVirtualKey)
        Start-Sleep -Milliseconds 120
        [BMLPlayerInput]::KeyUp($script:TutorialExitVirtualKey)
        $State.Injected = $true
        $State.Stage = 'waiting-for-tutorial-exit'
        $State.StageAt = Get-Date
    } elseif ($State.Stage -eq 'waiting-for-tutorial-exit' -and
              $LiveLog.Contains('Gameplay tutorial: exited=true by_input=true')) {
        $State.Stage = 'waiting-for-control'
        $State.StageAt = $Now
    } elseif ($State.Stage -eq 'waiting-for-tutorial-exit' -and
              ($Now - $State.StageAt).TotalSeconds -ge 2) {
        [void]$WindowShell.AppActivate($ProcessId)
        [BMLPlayerInput]::KeyDown($script:TutorialExitVirtualKey)
        Start-Sleep -Milliseconds 120
        [BMLPlayerInput]::KeyUp($script:TutorialExitVirtualKey)
        $State.StageAt = Get-Date
    } elseif ($State.Stage -eq 'waiting-for-control' -and
              $LiveLog.Contains('Gameplay input: ready=true')) {
        $State.Stage = 'ball-navigation-ready'
        $State.StageAt = $Now
    }
}

# Releases the injected key so a failed run cannot leave it held down.
function Reset-BMLTutorialExitKey {
    [BMLPlayerInput]::KeyUp($script:TutorialExitVirtualKey)
}

# Runs one Player acceptance flow end to end: install into the real game, start
# the Player, drive the tutorial exit, capture the artifacts, then put the
# install back the way it was. Every runner shares this, so a runner only has to
# say which probes to install and which live-log markers deserve a window
# screenshot.
#
# Install entries are @{ Source = <built file>; Destination = <path relative to
# the Ballance root> }. Remove entries are Ballance-relative paths that must not
# be present for this run; they are backed up and restored like everything else.
function Invoke-BMLPlayerRun {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BallanceRoot,

        [Parameter(Mandatory = $true)]
        [string]$LoaderDll,

        [Parameter(Mandatory = $true)]
        [object[]]$Install,

        [string[]]$Remove = @(),

        [hashtable]$Environment = @{},

        [Parameter(Mandatory = $true)]
        [string]$ArtifactsDirectory,

        [Parameter(Mandatory = $true)]
        [int]$PlayerWidth,

        [Parameter(Mandatory = $true)]
        [int]$PlayerHeight,

        [Parameter(Mandatory = $true)]
        [int]$TimeoutSeconds,

        # Name -> live ModLoader.log marker. The window is captured to
        # <name>.png the first poll after the marker shows up.
        [System.Collections.IDictionary]$WindowCaptures = @{}
    )

    $ballanceRootFull = [System.IO.Path]::GetFullPath($BallanceRoot)
    $playerPath = Join-Path $ballanceRootFull 'Bin\Player.exe'
    $installedLoader = Join-Path $ballanceRootFull 'BuildingBlocks\BMLPlus.dll'
    $modLoaderLog = Join-Path $ballanceRootFull 'ModLoader\ModLoader.log'
    $playerLog = Join-Path $ballanceRootFull 'Bin\Player.log'
    $playerConfig = Join-Path $ballanceRootFull 'Bin\Player.ini'

    Assert-BMLPath -Path $ballanceRootFull -Type 'Container'
    Assert-BMLPath -Path $playerPath -Type 'Leaf'
    Assert-BMLPath -Path $LoaderDll -Type 'Leaf'
    $plan = @(foreach ($entry in $Install) {
        Assert-BMLPath -Path $entry.Source -Type 'Leaf'
        [pscustomobject]@{
            Source = [System.IO.Path]::GetFullPath($entry.Source)
            Path = Join-Path $ballanceRootFull $entry.Destination
        }
    })

    # A second Player would fight over the files this run replaces.
    $targetPlayer = [System.IO.Path]::GetFullPath($playerPath)
    $runningTarget = @(Get-Process -Name Player -ErrorAction SilentlyContinue |
        Where-Object {
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

    $artifactsDirectoryFull = [System.IO.Path]::GetFullPath($ArtifactsDirectory)
    New-Item -ItemType Directory -Path $artifactsDirectoryFull -Force |
        Out-Null
    $screenshotPath = Join-Path $artifactsDirectoryFull 'Player-window.png'
    $framePath = Join-Path $artifactsDirectoryFull 'Player-frame.bmp'
    $tutorialScreenshotPath = Join-Path $artifactsDirectoryFull 'Tutorial-window.png'
    $tutorialFramePath = Join-Path $artifactsDirectoryFull 'Tutorial-frame.bmp'
    $tracePath = Join-Path $artifactsDirectoryFull 'ModLoader-trace.log'
    $playerTracePath = Join-Path $artifactsDirectoryFull 'Player-trace.log'

    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $touched = @($installedLoader) + @($plan | ForEach-Object Path) +
        @(foreach ($relative in $Remove) {
            Join-Path $ballanceRootFull $relative
        }) + @($modLoaderLog, $playerLog, $playerConfig)
    $artifacts = @(foreach ($path in $touched) {
        [pscustomobject]@{
            Path = $path
            Backup = "$path.test-bak-$timestamp"
            HashBefore = Get-BMLOptionalHash $path
        }
    })

    # The driver always writes the two frames, so the frame protocol belongs
    # here. Runner-supplied variables cover whatever their probes read.
    $variables = [ordered]@{
        BML_PLAYER_FRAME_PATH = $framePath
        BML_PLAYER_TUTORIAL_FRAME_PATH = $tutorialFramePath
    }
    foreach ($name in $Environment.Keys) {
        $variables[$name] = $Environment[$name]
    }
    $previousVariables = @{}
    foreach ($name in $variables.Keys) {
        $previousVariables[$name] =
            [Environment]::GetEnvironmentVariable($name, 'Process')
    }

    $captures = [ordered]@{}
    foreach ($name in $WindowCaptures.Keys) {
        $captures[$name] = [pscustomobject]@{
            Marker = $WindowCaptures[$name]
            Path = Join-Path $artifactsDirectoryFull "$name.png"
            Captured = $false
        }
    }

    $process = $null
    $playerExitCode = $null
    $timedOut = $false
    $modLoaderText = ''
    $playerText = ''
    $restored = $false
    $screenshotCaptured = $false
    $tutorialScreenshotCaptured = $false
    $windowActivated = $false
    $setupDialogAccepted = $false
    $tutorialExit = New-BMLTutorialExitState
    $windowShell = $null

    try {
        foreach ($name in $variables.Keys) {
            [Environment]::SetEnvironmentVariable($name, $variables[$name],
                                                 'Process')
        }
        foreach ($artifact in $artifacts) {
            if (Test-Path -LiteralPath $artifact.Path) {
                Copy-TestFile -Source $artifact.Path `
                    -Destination $artifact.Backup
            }
        }

        Copy-TestFile -Source $LoaderDll -Destination $installedLoader
        foreach ($entry in $plan) {
            Copy-TestFile -Source $entry.Source -Destination $entry.Path
        }
        foreach ($relative in $Remove) {
            $path = Join-Path $ballanceRootFull $relative
            if (Test-Path -LiteralPath $path) {
                Remove-Item -LiteralPath $path -Force
            }
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
        $windowActivated = $started.WindowActivated
        $setupDialogAccepted = $started.SetupDialogAccepted
        if ($started.SetupDialogFailed) {
            throw 'Player FullScreen Setup dialog did not expose a selectable render mode.'
        }

        $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
        while (-not $process.HasExited -and (Get-Date) -lt $deadline) {
            Start-Sleep -Milliseconds 100
            $process.Refresh()
            $liveLog = [string]::Join("`n", @(Get-BMLTextIfExists $modLoaderLog))
            foreach ($name in $captures.Keys) {
                $capture = $captures[$name]
                if ($capture.Captured -or
                    -not $liveLog.Contains($capture.Marker)) {
                    continue
                }
                [void]$windowShell.AppActivate($process.Id)
                Start-Sleep -Milliseconds 150
                $capture.Captured = Save-PlayerWindow -Process $process `
                    -Path $capture.Path
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
        $modLoaderText = [string]::Join("`n", @(Get-BMLTextIfExists $modLoaderLog))
        $playerText = [string]::Join("`n", @(Get-BMLTextIfExists $playerLog))
        $screenshotCaptured = Convert-PlayerFrame -FramePath $framePath `
            -Destination $screenshotPath
        $tutorialScreenshotCaptured = Convert-PlayerFrame `
            -FramePath $tutorialFramePath -Destination $tutorialScreenshotPath
        Set-Content -LiteralPath $tracePath -Value $modLoaderText -Encoding UTF8
        Set-Content -LiteralPath $playerTracePath -Value $playerText `
            -Encoding UTF8
    } finally {
        Reset-BMLTutorialExitKey
        foreach ($name in $variables.Keys) {
            [Environment]::SetEnvironmentVariable(
                $name, $previousVariables[$name], 'Process')
        }
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
                Copy-TestFile -Source $artifact.Backup `
                    -Destination $artifact.Path
                Remove-Item -LiteralPath $artifact.Backup -Force
            } elseif (Test-Path -LiteralPath $artifact.Path) {
                Remove-Item -LiteralPath $artifact.Path -Force
            }
        }
        $restored = $true
    }

    $installRestored = $restored
    foreach ($artifact in $artifacts) {
        if ((Get-BMLOptionalHash $artifact.Path) -ne $artifact.HashBefore) {
            $installRestored = $false
        }
    }

    return [pscustomobject]@{
        BallanceRoot = $ballanceRootFull
        PlayerWidth = $PlayerWidth
        PlayerHeight = $PlayerHeight
        WindowActivated = $windowActivated
        SetupDialogAccepted = $setupDialogAccepted
        PlayerExitCode = $playerExitCode
        TimedOut = $timedOut
        TutorialExitInjected = $tutorialExit.Injected
        ModLoaderLog = $modLoaderText
        PlayerLog = $playerText
        Trace = $tracePath
        PlayerTrace = $playerTracePath
        ArtifactsDirectory = $artifactsDirectoryFull
        Screenshot = $screenshotPath
        ScreenshotCaptured = $screenshotCaptured
        TutorialScreenshot = $tutorialScreenshotPath
        TutorialScreenshotCaptured = $tutorialScreenshotCaptured
        Captures = [pscustomobject]$captures
        InstallRestored = $installRestored
        LoaderHash = Get-BMLOptionalHash $LoaderDll
        Installed = @($plan | ForEach-Object {
            [pscustomobject]@{
                Path = $_.Path
                Hash = Get-BMLOptionalHash $_.Source
            }
        })
    }
}

# The driver writes the level frame as a back-buffer .bmp, which is the only
# proof the level was really on screen. The .png beside it is what a reviewer
# opens.
function Convert-PlayerFrame {
    param(
        [Parameter(Mandatory = $true)][string]$FramePath,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    if (-not (Test-Path -LiteralPath $FramePath)) {
        return $false
    }
    Add-Type -AssemblyName System.Drawing
    $frame = [System.Drawing.Image]::FromFile($FramePath)
    try {
        $frame.Save($Destination, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $frame.Dispose()
    }
    return (Test-Path -LiteralPath $Destination) -and
        (Get-Item -LiteralPath $Destination).Length -gt 0
}

# The checks every runner shares. They cover the driver flow and the one verdict
# each probe publishes; subject-specific log lines stay in the runner.
function Get-BMLPlayerFlowChecks {
    param(
        [Parameter(Mandatory = $true)]
        $Run,

        # The probe Mod ids this run installed. Each one has to be discovered
        # and has to report pass or skip.
        [Parameter(Mandatory = $true)]
        [string[]]$Probes
    )

    $log = $Run.ModLoaderLog
    $flowPattern = 'Player flow: status=(?<status>pass|fail) reason=(?<reason>\S+) ' +
        'menu_opened=(?<menuOpened>true|false) level_chosen=(?<levelChosen>true|false) ' +
        'control_ready=(?<controlReady>true|false) tutorial_declared=(?<tutorialDeclared>true|false) ' +
        'tutorial_listener=(?<tutorialListener>true|false) tutorial_exited=(?<tutorialExited>true|false) ' +
        'tutorial_by_input=(?<tutorialByInput>true|false) probes=(?<probes>[0-9]+) ' +
        'passed=(?<passed>[0-9]+) skipped=(?<skipped>[0-9]+) failed=(?<failed>[0-9]+) ' +
        'pending=(?<pending>[0-9]+) frames=(?<frames>[0-9]+)'
    $flow = [regex]::Match($log, $flowPattern)

    $verdicts = [ordered]@{}
    foreach ($match in [regex]::Matches($log,
            'Player probe: mod=(?<mod>\S+) state=(?<state>pass|fail|skip|pending) started=(?<started>true|false) detail=(?<detail>\S*)')) {
        $verdicts[$match.Groups['mod'].Value] = [pscustomobject]@{
            State = $match.Groups['state'].Value
            Started = $match.Groups['started'].Value -eq 'true'
            Detail = $match.Groups['detail'].Value
        }
    }

    $postStartIndex = $log.IndexOf('On Message PostStartMenu')
    $preLoadIndex = $log.IndexOf('On Message PreLoadLevel')
    $postLoadIndex = $log.IndexOf('On Message PostLoadLevel')
    $startLevelIndex = $log.IndexOf('On Message StartLevel')

    $checks = [ordered]@{
        PlayerWindowVisible = $Run.WindowActivated
        MenuFlow = $flow.Success -and
            $flow.Groups['menuOpened'].Value -eq 'true' -and
            $flow.Groups['levelChosen'].Value -eq 'true'
        FlowPassed = $flow.Success -and $flow.Groups['status'].Value -eq 'pass'
        BallNavigationFlow = $flow.Success -and
            $Run.TutorialExitInjected -and
            $log.Contains(
                'Gameplay tutorial keys: count=2 exit_declared=true continue=28 exit=16') -and
            $log.Contains('Gameplay input: tutorial_exit=true key=16') -and
            $log.Contains('Gameplay input: ready=true') -and
            $flow.Groups['tutorialDeclared'].Value -eq 'true' -and
            $flow.Groups['tutorialListener'].Value -eq 'true' -and
            $flow.Groups['tutorialExited'].Value -eq 'true' -and
            $flow.Groups['tutorialByInput'].Value -eq 'true' -and
            $flow.Groups['controlReady'].Value -eq 'true'
        ProbesDiscovered = $flow.Success -and
            [int]$flow.Groups['probes'].Value -eq $Probes.Count -and
            [int]$flow.Groups['pending'].Value -eq 0
    }
    foreach ($probe in $Probes) {
        $checks["Probe_$probe"] = $verdicts.Contains($probe) -and
            ($verdicts[$probe].State -eq 'pass' -or
             $verdicts[$probe].State -eq 'skip')
    }
    $shared = [ordered]@{
        CleanPostProcess = -not $Run.PlayerLog.Contains('Error : PostProcess')
        CleanModLoad = -not $log.Contains('Failed to load ')
        CleanShutdown = $log.Contains('Goodbye!') -and
            -not $log.Contains('Failed to retire Behavior') -and
            -not $log.Contains('Failed to unload mod') -and
            -not $log.Contains('Failed to leave the current Behavior Plan world')
        NaturalLevelFlow = $postStartIndex -ge 0 -and
            $preLoadIndex -gt $postStartIndex -and
            $postLoadIndex -gt $preLoadIndex -and
            $startLevelIndex -gt $postLoadIndex
        ScreenshotCaptured = $Run.ScreenshotCaptured -and
            $log.Contains('Player frame: captured=true') -and
            (Test-ImageDimensions -Path $Run.Screenshot `
                -Width $Run.PlayerWidth -Height $Run.PlayerHeight)
        TutorialScreenshotCaptured = $Run.TutorialScreenshotCaptured -and
            $log.Contains('Tutorial frame: captured=true') -and
            (Test-ImageDimensions -Path $Run.TutorialScreenshot `
                -Width $Run.PlayerWidth -Height $Run.PlayerHeight)
        ExitCallback = $log.Contains('Player flow exit: status=pass')
        PlayerExited = -not $Run.TimedOut -and $Run.PlayerExitCode -eq 0
        InstallRestored = $Run.InstallRestored
    }
    foreach ($name in $shared.Keys) {
        $checks[$name] = $shared[$name]
    }

    return [pscustomobject]@{
        Checks = $checks
        Flow = $(if ($flow.Success) {
            [pscustomobject]@{
                Status = $flow.Groups['status'].Value
                Reason = $flow.Groups['reason'].Value
                Probes = [int]$flow.Groups['probes'].Value
                Passed = [int]$flow.Groups['passed'].Value
                Skipped = [int]$flow.Groups['skipped'].Value
                Failed = [int]$flow.Groups['failed'].Value
                Pending = [int]$flow.Groups['pending'].Value
                Frames = [int]$flow.Groups['frames'].Value
            }
        } else { $null })
        Verdicts = $verdicts
    }
}

Export-ModuleMember -Function Save-PlayerWindow, Test-ImageDimensions,
    Confirm-PlayerSetupDialog, Copy-TestFile, Start-BMLPlayerProcess,
    New-BMLTutorialExitState, Step-BMLTutorialExit, Reset-BMLTutorialExitKey,
    Invoke-BMLPlayerRun, Convert-PlayerFrame, Get-BMLPlayerFlowChecks
