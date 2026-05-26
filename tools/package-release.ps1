param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [Parameter(Mandatory = $true)]
    [string]$ReleaseInstallDir,

    [Parameter(Mandatory = $true)]
    [string]$DebugInstallDir,

    [Parameter(Mandatory = $true)]
    [string]$ReleaseBinaryDir,

    [Parameter(Mandatory = $true)]
    [string]$DebugBinaryDir,

    [Parameter(Mandatory = $true)]
    [string]$OutputDir,

    [string]$RuntimeSourceDir
)

$ErrorActionPreference = 'Stop'

if (-not $RuntimeSourceDir) {
    $RuntimeSourceDir = Join-Path $PSScriptRoot '..\packaging\runtime'
}

function Resolve-RepoPath {
    param([string]$Path)

    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\$Path"))
}

function Assert-PathExists {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Required path is missing: $Path"
    }
}

function New-CleanDirectory {
    param([string]$Path)

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Path | Out-Null
}

function New-ZipFromDirectory {
    param(
        [string]$SourceDir,
        [string]$ZipPath
    )

    if (Test-Path -LiteralPath $ZipPath) {
        Remove-Item -LiteralPath $ZipPath -Force
    }

    $items = Get-ChildItem -LiteralPath $SourceDir -Force
    if (-not $items) {
        throw "Cannot package empty directory: $SourceDir"
    }

    Compress-Archive -Path (Join-Path $SourceDir '*') -DestinationPath $ZipPath -CompressionLevel Optimal
}

if ($Version -notmatch '^v\d+\.\d+\.\d+$') {
    throw "Version must be a tag in vX.Y.Z format: $Version"
}

$repoRoot = Resolve-RepoPath '.'
$releaseInstallDir = [System.IO.Path]::GetFullPath($ReleaseInstallDir)
$debugInstallDir = [System.IO.Path]::GetFullPath($DebugInstallDir)
$releaseBinaryDir = [System.IO.Path]::GetFullPath($ReleaseBinaryDir)
$debugBinaryDir = [System.IO.Path]::GetFullPath($DebugBinaryDir)
$runtimeSourceDir = [System.IO.Path]::GetFullPath($RuntimeSourceDir)
$outputDir = [System.IO.Path]::GetFullPath($OutputDir)
$stageRoot = Join-Path $outputDir '_stage'

Assert-PathExists $releaseInstallDir
Assert-PathExists $debugInstallDir
Assert-PathExists $releaseBinaryDir
Assert-PathExists $debugBinaryDir
Assert-PathExists $runtimeSourceDir

Assert-PathExists (Join-Path $releaseInstallDir 'bin\BMLPlus.dll')
Assert-PathExists (Join-Path $releaseInstallDir 'include\BML\Version.h')
Assert-PathExists (Join-Path $releaseInstallDir 'lib\BMLPlus.lib')
Assert-PathExists (Join-Path $releaseInstallDir 'lib\cmake\BML\BMLTargets-release.cmake')

Assert-PathExists (Join-Path $debugInstallDir 'bin\BMLPlus.dll')
Assert-PathExists (Join-Path $debugInstallDir 'include\BML\Version.h')
Assert-PathExists (Join-Path $debugInstallDir 'lib\BMLPlus.lib')
Assert-PathExists (Join-Path $debugInstallDir 'lib\cmake\BML\BMLTargets-debug.cmake')

Assert-PathExists (Join-Path $releaseBinaryDir 'BMLPlus.dll')
Assert-PathExists (Join-Path $debugBinaryDir 'BMLPlus.pdb')
Assert-PathExists (Join-Path $runtimeSourceDir 'ModLoader\Configs\BML.cfg')
Assert-PathExists (Join-Path $runtimeSourceDir 'ModLoader\Fonts\unifont.otf')
Assert-PathExists (Join-Path $runtimeSourceDir 'ModLoader\Mods\CameraUtilities.bmodp')
Assert-PathExists (Join-Path $runtimeSourceDir 'ModLoader\Mods\DebugUtilities.bmodp')
Assert-PathExists (Join-Path $runtimeSourceDir 'ModLoader\Mods\TravelMode.bmodp')
Assert-PathExists (Join-Path $repoRoot 'LICENSE')
Assert-PathExists (Join-Path $repoRoot 'README.md')
Assert-PathExists (Join-Path $repoRoot 'README_zh-CN.md')

New-CleanDirectory $outputDir
New-CleanDirectory $stageRoot

$runtimeStage = Join-Path $stageRoot 'runtime'
New-CleanDirectory $runtimeStage
Copy-Item -Path (Join-Path $runtimeSourceDir '*') -Destination $runtimeStage -Recurse
New-Item -ItemType Directory -Path (Join-Path $runtimeStage 'BuildingBlocks') | Out-Null
Copy-Item -LiteralPath (Join-Path $releaseBinaryDir 'BMLPlus.dll') `
    -Destination (Join-Path $runtimeStage 'BuildingBlocks\BMLPlus.dll')
Copy-Item -LiteralPath (Join-Path $repoRoot 'LICENSE') -Destination $runtimeStage
Copy-Item -LiteralPath (Join-Path $repoRoot 'README.md') -Destination $runtimeStage
Copy-Item -LiteralPath (Join-Path $repoRoot 'README_zh-CN.md') -Destination $runtimeStage

New-ZipFromDirectory -SourceDir $runtimeStage -ZipPath (Join-Path $outputDir "BMLPlus-$Version.zip")
New-ZipFromDirectory -SourceDir $releaseInstallDir -ZipPath (Join-Path $outputDir "BMLPlus-SDK-$Version-Release.zip")

$debugStage = Join-Path $stageRoot 'sdk-debug'
Copy-Item -LiteralPath $debugInstallDir -Destination $debugStage -Recurse
Copy-Item -LiteralPath (Join-Path $debugBinaryDir 'BMLPlus.pdb') -Destination (Join-Path $debugStage 'bin\BMLPlus.pdb')
New-ZipFromDirectory -SourceDir $debugStage -ZipPath (Join-Path $outputDir "BMLPlus-SDK-$Version-Debug.zip")

Remove-Item -LiteralPath $stageRoot -Recurse -Force

Get-ChildItem -LiteralPath $outputDir -Filter '*.zip' | ForEach-Object {
    Write-Host "Created $($_.FullName)"
}
