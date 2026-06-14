[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^v\d+\.\d+\.\d+$')]
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

    [string]$RuntimeSourceDir,

    [string]$CKAngelScriptRuntimeDir,

    [switch]$IncludeAngelScript
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'lib\BMLProject.psm1') -Force

function Copy-RequiredFile {
    param(
        [string]$Source,
        [string]$Destination
    )

    Assert-BMLPath -Path $Source -Type Leaf
    New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

function Copy-DocumentationFiles {
    param(
        [string]$DestinationDir,
        [string[]]$Files
    )

    New-Item -ItemType Directory -Path $DestinationDir -Force | Out-Null
    foreach ($file in $Files) {
        Copy-RequiredFile -Source (Join-Path $layout.DocsRoot $file) -Destination (Join-Path $DestinationDir $file)
    }
}

function Write-RequiredAngelScriptReadme {
    param([string]$DestinationDir)

    New-Item -ItemType Directory -Path $DestinationDir -Force | Out-Null
@"
BML+ script mods require CKAngelScript.

This package installs the matching AngelScript.dll into BuildingBlocks next to
BMLPlus.dll. Keep the two DLLs together when deploying this release.
"@ | Set-Content -Path (Join-Path $DestinationDir 'CKAngelScript-README.txt') -Encoding UTF8
}

function Get-CKAngelScriptRuntimeDll {
    param([string]$RootDir)

    $rootDll = Join-Path $RootDir 'AngelScript.dll'
    if (Test-Path -LiteralPath $rootDll -PathType Leaf) {
        return $rootDll
    }

    $buildingBlocksDll = Join-Path $RootDir 'BuildingBlocks\AngelScript.dll'
    if (Test-Path -LiteralPath $buildingBlocksDll -PathType Leaf) {
        return $buildingBlocksDll
    }

    throw "Required CKAngelScript runtime is missing: $rootDll or $buildingBlocksDll"
}

function Copy-CKAngelScriptHeaders {
    param([string]$DestinationIncludeDir)

    Copy-RequiredFile -Source (Join-Path $ckasRuntime 'include\CKAngelScript.h') -Destination (Join-Path $DestinationIncludeDir 'CKAngelScript.h')
    Copy-RequiredFile -Source (Join-Path $ckasRuntime 'include\angelscript.h') -Destination (Join-Path $DestinationIncludeDir 'angelscript.h')
}

$layout = Get-BMLProjectLayout
$releaseInstall = [System.IO.Path]::GetFullPath($ReleaseInstallDir)
$debugInstall = [System.IO.Path]::GetFullPath($DebugInstallDir)
$releaseBin = [System.IO.Path]::GetFullPath($ReleaseBinaryDir)
$debugBin = [System.IO.Path]::GetFullPath($DebugBinaryDir)
$runtimeSource = if ($RuntimeSourceDir) {
    [System.IO.Path]::GetFullPath($RuntimeSourceDir)
} else {
    $layout.RuntimeSourceRoot
}
$ckasRuntime = if ($CKAngelScriptRuntimeDir) { [System.IO.Path]::GetFullPath($CKAngelScriptRuntimeDir) } else { $null }
$ckasRuntimeDll = if ($ckasRuntime) { Get-CKAngelScriptRuntimeDll -RootDir $ckasRuntime } else { $null }
$output = [System.IO.Path]::GetFullPath($OutputDir)
$stageRoot = Join-Path $output '_stage'

if ($ckasRuntime -and -not $IncludeAngelScript) {
    throw '-CKAngelScriptRuntimeDir requires -IncludeAngelScript.'
}
if ($IncludeAngelScript -and -not $ckasRuntime) {
    throw '-IncludeAngelScript requires -CKAngelScriptRuntimeDir.'
}

$scriptDocs = @(
    'bml-script-mod-author-guide.md',
    'bml-script-v1-contract.md',
    'bml-script-mod-api.as',
    'bml-script-facade-coverage.md'
)
$scriptSdkDocs = $scriptDocs + @('bml-script-mod-validation.md')
$nativeTemplate = Join-Path $layout.TemplatesRoot 'native-mod-template'
$scriptTemplate = Join-Path $layout.TemplatesRoot 'script-mod-template'

foreach ($path in @(
    $releaseInstall,
    $debugInstall,
    $releaseBin,
    $debugBin,
    $runtimeSource,
    (Join-Path $layout.RepoRoot 'LICENSE'),
    (Join-Path $layout.RepoRoot 'README.md'),
    (Join-Path $layout.RepoRoot 'README_zh-CN.md'),
    (Join-Path $layout.RepoRoot 'include\BML\Interop.h'),
    (Join-Path $layout.TemplatesRoot 'README.md'),
    $nativeTemplate,
    (Join-Path $layout.NativeInteropSmokeRoot 'BMLNativeInteropSmokeMod.cpp')
)) {
    Assert-BMLPath -Path $path
}

foreach ($path in @(
    (Join-Path $releaseInstall 'bin\BMLPlus.dll'),
    (Join-Path $releaseInstall 'include\BML\Version.h'),
    (Join-Path $releaseInstall 'lib\BMLPlus.lib'),
    (Join-Path $releaseInstall 'lib\cmake\BML\BMLTargets-release.cmake'),
    (Join-Path $debugInstall 'bin\BMLPlus.dll'),
    (Join-Path $debugInstall 'include\BML\Version.h'),
    (Join-Path $debugInstall 'lib\BMLPlus.lib'),
    (Join-Path $debugInstall 'lib\cmake\BML\BMLTargets-debug.cmake'),
    (Join-Path $releaseBin 'BMLPlus.dll'),
    (Join-Path $releaseBin 'BMLNativeInteropSmoke.bmodp'),
    (Join-Path $debugBin 'BMLPlus.pdb'),
    (Join-Path $runtimeSource 'ModLoader\Configs\BML.cfg'),
    (Join-Path $runtimeSource 'ModLoader\Fonts\unifont.otf'),
    (Join-Path $runtimeSource 'ModLoader\Mods\CameraUtilities.bmodp'),
    (Join-Path $runtimeSource 'ModLoader\Mods\DebugUtilities.bmodp'),
    (Join-Path $runtimeSource 'ModLoader\Mods\TravelMode.bmodp')
)) {
    Assert-BMLPath -Path $path -Type Leaf
}

if ($IncludeAngelScript) {
    Assert-BMLPath -Path (Join-Path $layout.ScriptsRoot 'Validate-BMLBallance.ps1') -Type Leaf
    Assert-BMLPath -Path (Join-Path $layout.ScriptsRoot 'Pack-BMLScriptMod.ps1') -Type Leaf
    foreach ($doc in $scriptSdkDocs) {
        Assert-BMLPath -Path (Join-Path $layout.DocsRoot $doc) -Type Leaf
    }
    Assert-BMLPath -Path $scriptTemplate -Type Container
    if ($ckasRuntime) {
        Assert-BMLPath -Path $ckasRuntimeDll -Type Leaf
        Assert-BMLPath -Path (Join-Path $ckasRuntime 'include\CKAngelScript.h') -Type Leaf
        Assert-BMLPath -Path (Join-Path $ckasRuntime 'include\angelscript.h') -Type Leaf
    }
}

New-BMLCleanDirectory $output
New-BMLCleanDirectory $stageRoot

$runtimeStage = Join-Path $stageRoot 'runtime'
New-BMLCleanDirectory $runtimeStage
Copy-Item -Path (Join-Path $runtimeSource '*') -Destination $runtimeStage -Recurse
Copy-RequiredFile -Source (Join-Path $releaseBin 'BMLPlus.dll') -Destination (Join-Path $runtimeStage 'BuildingBlocks\BMLPlus.dll')
Copy-RequiredFile -Source (Join-Path $layout.RepoRoot 'LICENSE') -Destination (Join-Path $runtimeStage 'LICENSE')
Copy-RequiredFile -Source (Join-Path $layout.RepoRoot 'README.md') -Destination (Join-Path $runtimeStage 'README.md')
Copy-RequiredFile -Source (Join-Path $layout.RepoRoot 'README_zh-CN.md') -Destination (Join-Path $runtimeStage 'README_zh-CN.md')
Copy-RequiredFile -Source (Join-Path $releaseBin 'BMLNativeInteropSmoke.bmodp') -Destination (Join-Path $runtimeStage 'Examples\BMLNativeInteropSmoke.bmodp')
Copy-RequiredFile -Source (Join-Path $layout.TemplatesRoot 'README.md') -Destination (Join-Path $runtimeStage 'Templates\README.md')
Copy-BMLDirectoryContents -SourceDir $nativeTemplate -DestinationDir (Join-Path $runtimeStage 'Templates\native-mod-template')

if ($IncludeAngelScript) {
    Copy-DocumentationFiles -DestinationDir (Join-Path $runtimeStage 'Docs\Scripting') -Files $scriptDocs
    Copy-BMLDirectoryContents -SourceDir $scriptTemplate -DestinationDir (Join-Path $runtimeStage 'Templates\script-mod-template')
    Copy-RequiredFile -Source $ckasRuntimeDll -Destination (Join-Path $runtimeStage 'BuildingBlocks\AngelScript.dll')
    Write-RequiredAngelScriptReadme -DestinationDir (Join-Path $runtimeStage 'Docs\Scripting')
}

New-BMLZipFromDirectory -SourceDir $runtimeStage -ZipPath (Join-Path $output "BMLPlus-$Version.zip")

$releaseSdkStage = Join-Path $stageRoot 'sdk-release'
New-BMLCleanDirectory $releaseSdkStage
Copy-BMLDirectoryContents -SourceDir $releaseInstall -DestinationDir $releaseSdkStage
Copy-RequiredFile -Source (Join-Path $layout.TemplatesRoot 'README.md') -Destination (Join-Path $releaseSdkStage 'templates\README.md')
Copy-BMLDirectoryContents -SourceDir $nativeTemplate -DestinationDir (Join-Path $releaseSdkStage 'templates\native-mod-template')
Copy-RequiredFile -Source (Join-Path $releaseBin 'BMLNativeInteropSmoke.bmodp') -Destination (Join-Path $releaseSdkStage 'examples\native\BMLNativeInteropSmoke.bmodp')
Copy-RequiredFile -Source (Join-Path $layout.NativeInteropSmokeRoot 'BMLNativeInteropSmokeMod.cpp') -Destination (Join-Path $releaseSdkStage 'examples\native\BMLNativeInteropSmokeMod.cpp')

if ($IncludeAngelScript) {
    Copy-CKAngelScriptHeaders -DestinationIncludeDir (Join-Path $releaseSdkStage 'include')
    Copy-BMLDirectoryContents -SourceDir $scriptTemplate -DestinationDir (Join-Path $releaseSdkStage 'templates\script-mod-template')
    Copy-DocumentationFiles -DestinationDir (Join-Path $releaseSdkStage 'docs\scripting') -Files $scriptSdkDocs
    Copy-RequiredFile -Source (Join-Path $layout.ScriptsRoot 'Validate-BMLBallance.ps1') -Destination (Join-Path $releaseSdkStage 'scripts\Validate-BMLBallance.ps1')
    Copy-RequiredFile -Source (Join-Path $layout.ScriptsRoot 'Pack-BMLScriptMod.ps1') -Destination (Join-Path $releaseSdkStage 'scripts\Pack-BMLScriptMod.ps1')
    Copy-BMLDirectoryContents -SourceDir (Join-Path $layout.ScriptsRoot 'lib') -DestinationDir (Join-Path $releaseSdkStage 'scripts\lib')
}

New-BMLZipFromDirectory -SourceDir $releaseSdkStage -ZipPath (Join-Path $output "BMLPlus-SDK-$Version-Release.zip")

$debugStage = Join-Path $stageRoot 'sdk-debug'
New-BMLCleanDirectory $debugStage
Copy-BMLDirectoryContents -SourceDir $debugInstall -DestinationDir $debugStage
Copy-RequiredFile -Source (Join-Path $debugBin 'BMLPlus.pdb') -Destination (Join-Path $debugStage 'bin\BMLPlus.pdb')
foreach ($directory in @('docs', 'tools', 'examples')) {
    $source = Join-Path $releaseSdkStage $directory
    if (Test-Path -LiteralPath $source) {
        Copy-BMLDirectoryContents -SourceDir $source -DestinationDir (Join-Path $debugStage $directory)
    }
}
if ($IncludeAngelScript) {
    Copy-CKAngelScriptHeaders -DestinationIncludeDir (Join-Path $debugStage 'include')
}

New-BMLZipFromDirectory -SourceDir $debugStage -ZipPath (Join-Path $output "BMLPlus-SDK-$Version-Debug.zip")

Remove-Item -LiteralPath $stageRoot -Recurse -Force

Get-ChildItem -LiteralPath $output -Filter '*.zip' | ForEach-Object {
    Write-Host "Created $($_.FullName)"
}
