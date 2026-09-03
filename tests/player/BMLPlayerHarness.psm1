Set-StrictMode -Version Latest

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

Export-ModuleMember -Function Save-PlayerWindow, Test-ImageDimensions,
    Confirm-PlayerSetupDialog, Copy-TestFile, Start-BMLPlayerProcess,
    New-BMLTutorialExitState, Step-BMLTutorialExit, Reset-BMLTutorialExitKey
