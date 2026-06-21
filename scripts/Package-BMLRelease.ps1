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

    [string]$UpdaterBaseUrl,

    [ValidateSet('stable', 'beta')]
    [string]$UpdaterDefaultChannel = 'stable',

    [switch]$IncludeAngelScript,

    [string]$SigningCngKeyName,

    [switch]$SkipUpdateSigning
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

function Write-UpdaterBootstrapReadme {
    param(
        [string]$DestinationDir,
        [string]$Version
    )

    New-Item -ItemType Directory -Path $DestinationDir -Force | Out-Null
@"
BML+ Updater

Bin\Updater.exe updates BML+ runtime files only. It does not install, remove, or
change mods and configs.

Use BMLPlus-$Version.zip for manual installation. BMLPlus-Update-$Version.zip is
only for Bin\Updater.exe apply-local/verify-local and is not a manual install
package.

This release does not self-update Bin\Updater.exe. If an old updater reports
detached signature verification failure, install the latest manual package once
to bootstrap Bin\Updater.exe, then use updater packages afterwards.
"@ | Set-Content -Path (Join-Path $DestinationDir 'Updater-README.txt') -Encoding UTF8
}

function Get-RelativeZipPath {
    param(
        [string]$BaseDir,
        [string]$Path
    )

    $baseUri = [System.Uri](([System.IO.Path]::GetFullPath($BaseDir).TrimEnd('\') + '\'))
    $pathUri = [System.Uri]([System.IO.Path]::GetFullPath($Path))
    return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($pathUri).ToString())
}

function Get-BMLVersionHeaderFullVersion {
    param([string]$VersionHeaderPath)

    Assert-BMLPath -Path $VersionHeaderPath -Type Leaf
    $match = Select-String -LiteralPath $VersionHeaderPath -Pattern '^\s*#define\s+BML_VERSION_FULL\s+"([^"]+)"\s*$' | Select-Object -First 1
    if (-not $match) {
        throw "Unable to read BML_VERSION_FULL from $VersionHeaderPath"
    }
    return $match.Matches[0].Groups[1].Value
}

function Assert-BMLBinaryVersionMatchesHeader {
    param(
        [string]$BinaryPath,
        [string]$VersionHeaderPath,
        [string]$Label
    )

    Assert-BMLPath -Path $BinaryPath -Type Leaf
    $expected = Get-BMLVersionHeaderFullVersion -VersionHeaderPath $VersionHeaderPath
    $versionInfo = (Get-Item -LiteralPath $BinaryPath).VersionInfo
    $actual = if ($versionInfo.ProductVersion) { $versionInfo.ProductVersion } else { $versionInfo.FileVersion }
    if ($actual -ne $expected) {
        throw "$Label version resource mismatch: binary has '$actual', Version.h has '$expected'. Rebuild the target before packaging."
    }
}

function Test-UpdaterManagedPath {
    param([string]$RelativePath)

    $lower = $RelativePath.ToLowerInvariant()
    if ($lower -eq 'bin/updater.exe') {
        return $false
    }
    if ($lower.StartsWith('modloader/mods/') -or $lower.StartsWith('modloader/configs/')) {
        return $false
    }
    if ($RelativePath.Contains('\') -or $RelativePath.Contains(':') -or $RelativePath.StartsWith('/')) {
        return $false
    }
    foreach ($segment in $RelativePath.Split('/')) {
        if (-not $segment -or $segment -eq '.' -or $segment -eq '..' -or $segment.EndsWith('.') -or $segment.EndsWith(' ')) {
            return $false
        }
    }
    return $true
}

function New-UpdaterManifestObject {
    param(
        [string]$Version,
        [string]$PackagePath,
        [string]$PackageStage
    )

    $managedFiles = @()
    foreach ($file in Get-ChildItem -LiteralPath $PackageStage -File -Recurse) {
        $relative = Get-RelativeZipPath -BaseDir $PackageStage -Path $file.FullName
        if (-not (Test-UpdaterManagedPath -RelativePath $relative)) {
            throw "Updater package contains forbidden managed path: $relative"
        }
        $managedFiles += [pscustomobject]@{
            path = $relative
            sha256 = ((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant())
            size = $file.Length
        }
    }

    return [ordered]@{
        schemaVersion = 1
        version = $Version
        package = [ordered]@{
            fileName = [System.IO.Path]::GetFileName($PackagePath)
            sha256 = ((Get-FileHash -LiteralPath $PackagePath -Algorithm SHA256).Hash.ToLowerInvariant())
        }
        managedFiles = $managedFiles
        preserve = @()
        removeFiles = @()
    }
}

function Write-UpdaterManifestSignature {
    param(
        [string]$ManifestPath,
        [string]$SignaturePath,
        [string]$CngKeyName
    )

    if (-not $CngKeyName) {
        throw 'Updater manifest signing requires -SigningCngKeyName, or pass -SkipUpdateSigning for local unsigned packaging.'
    }

    $key = [System.Security.Cryptography.CngKey]::Open($CngKeyName)
    $ecdsa = [System.Security.Cryptography.ECDsaCng]::new($key)
    try {
        $bytes = [System.IO.File]::ReadAllBytes($ManifestPath)
        $sha = [System.Security.Cryptography.SHA256]::Create()
        $hash = $sha.ComputeHash($bytes)
        $signature = $ecdsa.SignHash($hash)
        if ($signature.Length -ne 64) {
            throw "CNG ECDSA signature must be IEEE P1363 r||s format; got $($signature.Length) bytes."
        }
        [System.IO.File]::WriteAllText($SignaturePath, [System.Convert]::ToBase64String($signature), [System.Text.Encoding]::ASCII)
    } finally {
        $ecdsa.Dispose()
        $key.Dispose()
    }
}

function Write-Utf8NoBomText {
    param(
        [string]$Path,
        [string]$Text
    )

    $encoding = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText($Path, $Text, $encoding)
}

function Write-UpdaterSourcesJson {
    param(
        [string]$DestinationDir,
        [string]$BaseUrl,
        [string]$DefaultChannel
    )

    if (-not $BaseUrl) {
        return
    }

    $normalized = $BaseUrl.TrimEnd('/')
    if (-not $normalized.StartsWith('https://')) {
        throw '-UpdaterBaseUrl must use HTTPS.'
    }
    if ($normalized -match '\s') {
        throw '-UpdaterBaseUrl must not contain whitespace.'
    }

    New-Item -ItemType Directory -Path $DestinationDir -Force | Out-Null
    $sourceObject = [ordered]@{
        schemaVersion = 1
        baseUrl = $normalized
        defaultChannel = $DefaultChannel
    }
    Write-Utf8NoBomText -Path (Join-Path $DestinationDir 'sources.json') -Text (($sourceObject | ConvertTo-Json -Depth 3) + "`n")
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

    $binDll = Join-Path $RootDir 'bin\AngelScript.dll'
    if (Test-Path -LiteralPath $binDll -PathType Leaf) {
        return $binDll
    }

    throw "Required CKAngelScript runtime is missing: $rootDll, $buildingBlocksDll, or $binDll"
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
$scriptSdkDocs = $scriptDocs
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
    $nativeTemplate
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
    (Join-Path $releaseBin 'Updater.exe'),
    (Join-Path $debugBin 'BMLPlus.pdb'),
    (Join-Path $runtimeSource 'ModLoader\Configs\BML.cfg'),
    (Join-Path $runtimeSource 'ModLoader\Fonts\unifont.otf'),
    (Join-Path $runtimeSource 'ModLoader\Mods\CameraUtilities.bmodp'),
    (Join-Path $runtimeSource 'ModLoader\Mods\DebugUtilities.bmodp'),
    (Join-Path $runtimeSource 'ModLoader\Mods\TravelMode.bmodp')
)) {
    Assert-BMLPath -Path $path -Type Leaf
}

Assert-BMLBinaryVersionMatchesHeader `
    -BinaryPath (Join-Path $releaseBin 'BMLPlus.dll') `
    -VersionHeaderPath (Join-Path $releaseInstall 'include\BML\Version.h') `
    -Label 'Release BMLPlus.dll'
Assert-BMLBinaryVersionMatchesHeader `
    -BinaryPath (Join-Path $debugInstall 'bin\BMLPlus.dll') `
    -VersionHeaderPath (Join-Path $debugInstall 'include\BML\Version.h') `
    -Label 'Debug BMLPlus.dll'

if ($IncludeAngelScript) {
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
Copy-RequiredFile -Source (Join-Path $releaseBin 'Updater.exe') -Destination (Join-Path $runtimeStage 'Bin\Updater.exe')
Write-UpdaterBootstrapReadme -DestinationDir (Join-Path $runtimeStage 'Bin') -Version $Version
Write-UpdaterSourcesJson -DestinationDir (Join-Path $runtimeStage 'ModLoader\Updater') -BaseUrl $UpdaterBaseUrl -DefaultChannel $UpdaterDefaultChannel
Copy-RequiredFile -Source (Join-Path $layout.RepoRoot 'LICENSE') -Destination (Join-Path $runtimeStage 'LICENSE')
Copy-RequiredFile -Source (Join-Path $layout.RepoRoot 'README.md') -Destination (Join-Path $runtimeStage 'README.md')
Copy-RequiredFile -Source (Join-Path $layout.RepoRoot 'README_zh-CN.md') -Destination (Join-Path $runtimeStage 'README_zh-CN.md')

if ($IncludeAngelScript) {
    Copy-RequiredFile -Source $ckasRuntimeDll -Destination (Join-Path $runtimeStage 'BuildingBlocks\AngelScript.dll')
}

New-BMLZipFromDirectory -SourceDir $runtimeStage -ZipPath (Join-Path $output "BMLPlus-$Version.zip")

$updaterStage = Join-Path $stageRoot 'updater-runtime'
Copy-BMLDirectoryFresh -SourceDir $runtimeStage -DestinationDir $updaterStage
foreach ($forbidden in @(
    (Join-Path $updaterStage 'Bin\Updater.exe'),
    (Join-Path $updaterStage 'Bin\Updater-README.txt'),
    (Join-Path $updaterStage 'ModLoader\Updater'),
    (Join-Path $updaterStage 'ModLoader\Mods'),
    (Join-Path $updaterStage 'ModLoader\Configs')
)) {
    if (Test-Path -LiteralPath $forbidden) {
        Remove-Item -LiteralPath $forbidden -Recurse -Force
    }
}

$updaterZip = Join-Path $output "BMLPlus-Update-$Version.zip"
New-BMLZipFromDirectory -SourceDir $updaterStage -ZipPath $updaterZip
$updaterManifest = Join-Path $output "BMLPlus-Update-$Version.manifest.json"
$manifestObject = New-UpdaterManifestObject -Version $Version -PackagePath $updaterZip -PackageStage $updaterStage
Write-Utf8NoBomText -Path $updaterManifest -Text (($manifestObject | ConvertTo-Json -Depth 6) + "`n")
$updaterManifestSignature = "$updaterManifest.sig"
if ($SkipUpdateSigning) {
    Write-Warning "Skipping updater manifest signature: $updaterManifestSignature"
} else {
    Write-UpdaterManifestSignature -ManifestPath $updaterManifest -SignaturePath $updaterManifestSignature -CngKeyName $SigningCngKeyName
}

$releaseSdkStage = Join-Path $stageRoot 'sdk-release'
New-BMLCleanDirectory $releaseSdkStage
Copy-BMLDirectoryContents -SourceDir $releaseInstall -DestinationDir $releaseSdkStage
Copy-RequiredFile -Source (Join-Path $layout.TemplatesRoot 'README.md') -Destination (Join-Path $releaseSdkStage 'templates\README.md')
Copy-BMLDirectoryContents -SourceDir $nativeTemplate -DestinationDir (Join-Path $releaseSdkStage 'templates\native-mod-template')

if ($IncludeAngelScript) {
    Copy-CKAngelScriptHeaders -DestinationIncludeDir (Join-Path $releaseSdkStage 'include')
    Copy-BMLDirectoryContents -SourceDir $scriptTemplate -DestinationDir (Join-Path $releaseSdkStage 'templates\script-mod-template')
    Copy-DocumentationFiles -DestinationDir (Join-Path $releaseSdkStage 'docs\scripting') -Files $scriptSdkDocs
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
