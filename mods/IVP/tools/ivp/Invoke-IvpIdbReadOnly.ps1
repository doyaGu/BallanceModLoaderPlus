[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Script,
    [string[]]$ScriptArgument = @(),
    [string]$Database = "build-dev/physics_RT-analysis.i64"
)

$ErrorActionPreference = "Stop"
$workspace = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "../..")).Path
$buildRoot = (Resolve-Path -LiteralPath (Join-Path $workspace "build-dev")).Path
$databasePath = (Resolve-Path -LiteralPath (Join-Path $workspace $Database)).Path
$scriptPath = (Resolve-Path -LiteralPath (Join-Path $workspace $Script)).Path
$runnerPath = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "Run-IvpIdbScript.py")).Path

if ([System.IO.Path]::GetDirectoryName($databasePath) -ne $buildRoot) {
    throw "Database must be a direct child of build-dev: $databasePath"
}
if ((Get-Process ida, idat -ErrorAction SilentlyContinue | Measure-Object).Count -ne 0) {
    throw "Close IDA before inspecting the canonical database"
}

$inspectionPath = Join-Path $buildRoot (".physics_RT-analysis.readonly-{0}.i64" -f $PID)
if (Test-Path -LiteralPath $inspectionPath) {
    throw "Inspection path already exists: $inspectionPath"
}

function Remove-IdaWorkingSet {
    param([Parameter(Mandatory = $true)][string]$IdbPath)

    $directory = [System.IO.Path]::GetDirectoryName($IdbPath)
    if ($directory -ne $buildRoot) {
        throw "Refusing to clean IDA files outside build-dev: $IdbPath"
    }
    $stem = [System.IO.Path]::GetFileNameWithoutExtension($IdbPath)
    foreach ($extension in @('.i64', '.id0', '.id1', '.id2', '.nam', '.til')) {
        $candidate = Join-Path $directory ($stem + $extension)
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            Remove-Item -LiteralPath $candidate -Force
        }
    }
}

try {
    Copy-Item -LiteralPath $databasePath -Destination $inspectionPath
    $arguments = @(
        "-u", $runnerPath,
        "--database", $inspectionPath,
        "--script", $scriptPath
    )
    foreach ($argument in $ScriptArgument) {
        $arguments += @("--script-argument", $argument)
    }
    $exitCode = 0
    $maximumAttempts = 4
    for ($attempt = 1; $attempt -le $maximumAttempts; ++$attempt) {
        $output = @(& python @arguments 2>&1)
        $exitCode = $LASTEXITCODE
        $joined = $output -join "`n"
        if ($exitCode -eq 0) {
            $output | ForEach-Object { Write-Host $_ }
            break
        }
        $retryableOpenFailure =
            $exitCode -eq 3 -and $joined -match '(?m)^OPEN_RC\s+[1-9][0-9]*\b'
        if (-not $retryableOpenFailure -or $attempt -eq $maximumAttempts) {
            $output | ForEach-Object { Write-Host $_ }
            break
        }
        $delayMilliseconds = 250 * $attempt * $attempt
        Write-Host ("IDA_RETRY`t{0}`t{1}`t{2}ms" -f
            $attempt, [System.IO.Path]::GetFileName($scriptPath),
            $delayMilliseconds)
        Start-Sleep -Milliseconds $delayMilliseconds
    }
    if ($exitCode -ne 0) {
        throw "IDA inspection failed with exit code $exitCode"
    }
}
finally {
    Remove-IdaWorkingSet -IdbPath $inspectionPath
}
