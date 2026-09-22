[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^v\d+\.\d+\.\d+(?:-[0-9A-Za-z]+(?:\.[0-9A-Za-z]+)*)?$')]
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
Import-Module (Join-Path $PSScriptRoot 'lib\BMLReleaseFiles.psm1') -Force

function Copy-RequiredFile {
    param(
        [string]$Source,
        [string]$Destination
    )

    Assert-BMLPath -Path $Source -Type Leaf
    New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

function Get-InstalledModPackages {
    param([string]$InstallDir)

    $manifest = Join-Path $InstallDir 'share\BML\mods.txt'
    Assert-BMLPath -Path $manifest -Type Leaf
    $packages = @(Get-Content -LiteralPath $manifest | Where-Object { $_ -ne '' })
    if ($packages.Count -eq 0) {
        throw "Installed Mod manifest is empty: $manifest"
    }

    $expected = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    foreach ($package in $packages) {
        if ($package -cnotmatch '^[A-Za-z0-9][A-Za-z0-9_.-]*\.(bmodp|zip)$' -or
            -not $expected.Add($package)) {
            throw "Invalid or duplicate installed Mod package: $package"
        }
        Assert-BMLPath -Path (Join-Path $InstallDir "Mods\$package") -Type Leaf
    }

    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $InstallDir 'Mods') -File) {
        if (-not $expected.Contains($file.Name)) {
            throw "Installed Mod is absent from the manifest: $($file.FullName)"
        }
    }

    return $packages
}

function Assert-ModArchiveMatchesInstall {
    param(
        [string]$ArchivePath,
        [string]$InstallDir,
        [string]$EntryPrefix,
        [string[]]$Packages
    )

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        $expected = [System.Collections.Generic.HashSet[string]]::new(
            [System.StringComparer]::Ordinal)
        foreach ($name in $Packages) {
            [void]$expected.Add("$EntryPrefix$name")
        }
        $found = [System.Collections.Generic.HashSet[string]]::new(
            [System.StringComparer]::Ordinal)
        foreach ($entry in $archive.Entries) {
            if (-not $entry.FullName.StartsWith($EntryPrefix,
                    [System.StringComparison]::Ordinal) -or -not $entry.Name) {
                continue
            }
            if (-not $expected.Contains($entry.FullName) -or
                -not $found.Add($entry.FullName)) {
                throw "Unexpected or duplicate Mod in ${ArchivePath}: $($entry.FullName)"
            }

            $sourceHash = (Get-FileHash -LiteralPath (Join-Path $InstallDir "Mods\$($entry.Name)") -Algorithm SHA256).Hash
            $stream = $entry.Open()
            try {
                $sha = [System.Security.Cryptography.SHA256]::Create()
                try {
                    $archiveHash = [System.BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '')
                } finally {
                    $sha.Dispose()
                }
            } finally {
                $stream.Dispose()
            }
            if ($archiveHash -cne $sourceHash) {
                throw "Mod bytes differ from the installed build: $($entry.FullName)"
            }
        }
        if ($found.Count -ne $expected.Count) {
            $missing = @($expected | Where-Object { -not $found.Contains($_) })
            throw "Mod archive is missing packages: $($missing -join ', ')"
        }
    } finally {
        $archive.Dispose()
    }
}

function Remove-PackageStagingDirectory {
    param([string]$Path, [string]$OutputDir)

    $target = [System.IO.Path]::GetFullPath($Path)
    $expected = [System.IO.Path]::GetFullPath((Join-Path $OutputDir '_zip-contents'))
    if (-not [string]::Equals($target, $expected,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove a path outside the package staging directory: $target"
    }

    for ($attempt = 0; $attempt -lt 10; ++$attempt) {
        try {
            Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction Stop
            return
        } catch {
            if (-not (Test-Path -LiteralPath $target)) {
                return
            }
            if ($attempt -eq 9) {
                throw
            }
            Start-Sleep -Milliseconds 250
        }
    }
}

function Write-RequiredAngelScriptReadme {
    param([string]$DestinationDir)

    New-Item -ItemType Directory -Path $DestinationDir -Force | Out-Null
@"
BML+ script mods require CKAngelScript.

This package installs the matching AngelScript.dll into BuildingBlocks next to
BMLPlus.dll. Keep the two DLLs together when deploying this release. BML script
support in this release requires CKAngelScript API v6, including source-section
loading, object-handle arguments, script-array access,
module import binding, module bytecode, transactional replacement, module graph,
and module fingerprint features.
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

function Get-BMLVersionHeaderVersion {
    param([string]$VersionHeaderPath)

    Assert-BMLPath -Path $VersionHeaderPath -Type Leaf
    $match = Select-String -LiteralPath $VersionHeaderPath -Pattern '^\s*#define\s+BML_VERSION\s+"([^"]+)"\s*$' | Select-Object -First 1
    if (-not $match) {
        throw "Unable to read BML_VERSION from $VersionHeaderPath"
    }
    return $match.Matches[0].Groups[1].Value
}

function Assert-BMLSdkDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SdkDir,

        [switch]$RequireAngelScript
    )

    foreach ($relative in @(
        'README.md',
        'README_zh-CN.md',
        'include\BML\BMLAll.h',
        'include\BML\Imc.h',
        'include\BML\Interface.h',
        'include\BML\Runtime.h',
        'include\BML\Speedrun.h',
        'include\BML\Bui.h',
        'include\BML\Gui\Gui.h',
        'include\BML\Guids\Hooks.h',
        'lib\cmake\BML\BMLConfig.cmake',
        'lib\cmake\BML\BMLTargets.cmake',
        'lib\cmake\BML\BMLMod.cmake',
        'lib\cmake\BML\BMLImc.cmake',
        'lib\cmake\BML\FindVirtoolsSDK.cmake',
        'share\BML\tools\imc_codegen.py',
        'share\BML\tools\interface_codegen.py',
        'share\BML\docs\en\modding.md',
        'share\BML\docs\en\native-mod-api.md',
        'share\BML\docs\en\imc-author-guide.md',
        'share\BML\docs\en\imc.md',
        'share\BML\docs\zh-CN\modding.md',
        'share\BML\docs\zh-CN\native-mod-api.md',
        'share\BML\docs\zh-CN\imc-author-guide.md',
        'share\BML\docs\zh-CN\imc.md',
        'templates\README.md',
        'templates\native-mod-template\CMakeLists.txt',
        'templates\native-mod-template\README.md',
        'templates\native-mod-template\src\HelloMod.cpp',
        'templates\native-interface-provider-template\CMakeLists.txt',
        'templates\native-interface-provider-template\README.md',
        'templates\native-interface-provider-template\api\value.bml-interface',
        'templates\native-interface-provider-template\api\value.bml-interface.lock',
        'templates\native-interface-provider-template\src\HelloMod.cpp',
        'templates\native-interface-consumer-template\CMakeLists.txt',
        'templates\native-interface-consumer-template\README.md',
        'templates\native-interface-consumer-template\src\HelloMod.cpp',
        'templates\native-imc-provider-template\CMakeLists.txt',
        'templates\native-imc-provider-template\README.md',
        'templates\native-imc-provider-template\api\service.imc',
        'templates\native-imc-provider-template\api\service.imc.lock',
        'templates\native-imc-provider-template\src\HelloMod.cpp',
        'scripts\bml.cmd',
        'scripts\bml.py',
        'scripts\lib\BMLProject.psm1'
    )) {
        Assert-BMLPath -Path (Join-Path $SdkDir $relative) -Type Leaf
    }

    if ($RequireAngelScript) {
        foreach ($relative in @(
            'include\CKAngelScript.h',
            'include\angelscript.h',
            'templates\script-mod-template\HelloScript.mod.as',
            'templates\script-mod-template\README.md',
            'examples\script-mod\README.md',
            'examples\script-mod\README_zh-CN.md',
            'examples\script-mod\command-config\CommandConfig.mod.as',
            'examples\script-mod\input-ui\InputUi.mod.as',
            'examples\script-mod\game-state\GameState.mod.as',
            'share\BML\docs\en\script-mod\index.md',
            'share\BML\docs\en\script-mod\api.md',
            'share\BML\docs\zh-CN\api.md',
            'share\BML\docs\zh-CN\script-mod-tutorial\README.md',
            'docs\api\as.predefined',
            'docs\api\bml-script-mod-api.as',
            'docs\api\bml-imgui-api.as'
        )) {
            Assert-BMLPath -Path (Join-Path $SdkDir $relative) -Type Leaf
        }
    } else {
        foreach ($relative in @(
            'templates\script-mod-template',
            'examples\script-mod',
            'share\BML\docs\en\script-mod',
            'share\BML\docs\zh-CN\script-mod-tutorial',
            'docs\api\as.predefined'
        )) {
            if (Test-Path -LiteralPath (Join-Path $SdkDir $relative)) {
                throw "SDK directory contains AngelScript author files but script support was not requested: $relative"
            }
        }
    }

    foreach ($relative in @(
        'include\BML\Core.h',
        'include\BML\Import.h',
        'include\BML\Interop.h',
        'include\BML\DataBox.h',
        'include\BML\RefCount.h',
        'include\BML\Timer.h'
    )) {
        $forbidden = Join-Path $SdkDir $relative
        if (Test-Path -LiteralPath $forbidden) {
            throw "SDK directory contains a removed or internal header: $relative"
        }
    }

    foreach ($relative in @('docs\en', 'docs\zh-CN')) {
        $duplicateDocs = Join-Path $SdkDir $relative
        if (Test-Path -LiteralPath $duplicateDocs) {
            throw "SDK directory contains duplicate author documentation: $relative"
        }
    }

    $bmlTargets = Get-Content -LiteralPath (Join-Path $SdkDir 'lib\cmake\BML\BMLTargets.cmake') -Raw
    foreach ($dependency in @('VirtoolsSDK::CK2', 'VirtoolsSDK::VxMath')) {
        if (-not $bmlTargets.Contains($dependency)) {
            throw "SDK target export does not use the required namespaced dependency: $dependency"
        }
    }

    foreach ($file in Get-ChildItem -LiteralPath $SdkDir -File -Recurse -Force) {
        $relative = (Get-RelativeZipPath -BaseDir $SdkDir -Path $file.FullName).ToLowerInvariant()
        if ($relative -match '(^|/)__pycache__(/|$)' -or $relative -match '\.py[co]$') {
            throw "SDK directory contains a Python cache file: $relative"
        }
        if ($relative -match '^include/(gtest|gmock)/' -or
            $relative -match '^lib/(gtest|gmock)(_main)?\.(lib|a)$' -or
            $relative -match '^lib/cmake/gtest/' -or
            $relative -match '^lib/pkgconfig/(gtest|gmock)') {
            throw "SDK directory contains GoogleTest development files: $relative"
        }
    }
}

function Assert-BMLBinaryVersionMatchesHeader {
    param(
        [string]$BinaryPath,
        [string]$VersionHeaderPath,
        [string]$Label
    )

    Assert-BMLPath -Path $BinaryPath -Type Leaf
    $expected = Get-BMLVersionHeaderVersion -VersionHeaderPath $VersionHeaderPath
    $versionInfo = (Get-Item -LiteralPath $BinaryPath).VersionInfo
    $actual = if ($versionInfo.ProductVersion) { $versionInfo.ProductVersion } else { $versionInfo.FileVersion }
    $matchesRelease = $actual -eq $expected -or $actual.StartsWith("$expected+", [System.StringComparison]::Ordinal)
    if (-not $matchesRelease) {
        throw "$Label release version mismatch: binary has '$actual', Version.h has '$expected'. Rebuild the target before packaging."
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
        [string]$PackageDirectory
    )

    $managedFiles = @()
    foreach ($file in Get-ChildItem -LiteralPath $PackageDirectory -File -Recurse) {
        $relative = Get-RelativeZipPath -BaseDir $PackageDirectory -Path $file.FullName
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

    Write-BMLCngSignature `
        -InputPath $ManifestPath `
        -SignaturePath $SignaturePath `
        -CngKeyName $CngKeyName
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

function Get-CKAngelScriptHeaderApiVersion {
    param([string]$HeaderPath)

    Assert-BMLPath -Path $HeaderPath -Type Leaf
    $match = Select-String -LiteralPath $HeaderPath -Pattern '^\s*#define\s+CKAS_API_VERSION\s+(\d+)\s*$' | Select-Object -First 1
    if (-not $match) {
        throw "Unable to read CKAS_API_VERSION from $HeaderPath"
    }
    return [int]$match.Matches[0].Groups[1].Value
}

function Assert-CKAngelScriptHeaderFeature {
    param(
        [string]$HeaderPath,
        [string]$Feature
    )

    $match = Select-String -LiteralPath $HeaderPath -SimpleMatch $Feature | Select-Object -First 1
    if (-not $match) {
        throw "CKAngelScript header is missing required BML script feature $Feature. Use the matching CKAngelScript runtime and headers."
    }
}

function Assert-CKAngelScriptRuntimeCompatible {
    param([string]$RootDir)

    $headerPath = Join-Path $RootDir 'include\CKAngelScript.h'
    $apiVersion = Get-CKAngelScriptHeaderApiVersion -HeaderPath $headerPath
    if ($apiVersion -lt 6) {
        throw "CKAngelScript API version $apiVersion is too old. BML script support requires API version 6 or newer."
    }

    foreach ($feature in @(
        'CKAS_FEATURE_OBJECT_TYPE_NAMESPACE',
        'CKAS_FEATURE_OBJECT_METHOD_CONTEXT_ACCESS',
        'CKAS_FEATURE_SCRIPT_ARRAY_ACCESS',
        'CKAS_FEATURE_SOURCE_SECTIONS',
        'CKAS_FEATURE_OBJECT_HANDLE_ARGS',
        'CKAS_FEATURE_MODULE_IMPORTS',
        'CKAS_FEATURE_MODULE_BYTECODE',
        'CKAS_FEATURE_MODULE_REPLACE_TRANSACTION',
        'CKAS_FEATURE_MODULE_GRAPH',
        'CKAS_FEATURE_MODULE_FINGERPRINT'
    )) {
        Assert-CKAngelScriptHeaderFeature -HeaderPath $headerPath -Feature $feature
    }
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
$zipContentsRoot = Join-Path $output '_zip-contents'

if ($ckasRuntime -and -not $IncludeAngelScript) {
    throw '-CKAngelScriptRuntimeDir requires -IncludeAngelScript.'
}
if ($IncludeAngelScript -and -not $ckasRuntime) {
    throw '-IncludeAngelScript requires -CKAngelScriptRuntimeDir.'
}

foreach ($path in @(
    $releaseInstall,
    $debugInstall,
    $releaseBin,
    $debugBin,
    $runtimeSource,
    (Join-Path $layout.RepoRoot 'LICENSE'),
    (Join-Path $layout.RepoRoot 'README.md'),
    (Join-Path $layout.RepoRoot 'README_zh-CN.md')
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
    (Join-Path $runtimeSource 'ModLoader\Fonts\unifont.otf')
)) {
    Assert-BMLPath -Path $path -Type Leaf
}

$modPackages = @(Get-InstalledModPackages -InstallDir $releaseInstall)
$debugModPackages = @(Get-InstalledModPackages -InstallDir $debugInstall)
if (($modPackages -join '|') -cne ($debugModPackages -join '|')) {
    throw 'Release and Debug installations contain different Mod packages.'
}
$defaultMods = @('CameraUtilities.bmodp', 'DebugUtilities.bmodp', 'TravelMode.bmodp')
foreach ($name in $defaultMods) {
    if ($name -notin $modPackages) {
        throw "Default Mod is absent from the installed Mod manifest: $name"
    }
}
$sourceMods = Join-Path $runtimeSource 'ModLoader\Mods'
if ((Test-Path -LiteralPath $sourceMods) -and
    @(Get-ChildItem -LiteralPath $sourceMods -Recurse -File).Count -gt 0) {
    throw "Runtime source must not contain prebuilt Mods: $sourceMods"
}

foreach ($path in @(
    (Join-Path $releaseInstall 'bin\BMLPlus.dll'),
    (Join-Path $releaseBin 'BMLPlus.dll'),
    (Join-Path $debugInstall 'bin\BMLPlus.dll')
)) {
    Assert-BMLProductionRuntime -Path $path
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
    if ($ckasRuntime) {
        Assert-BMLPath -Path $ckasRuntimeDll -Type Leaf
        Assert-BMLPath -Path (Join-Path $ckasRuntime 'include\CKAngelScript.h') -Type Leaf
        Assert-BMLPath -Path (Join-Path $ckasRuntime 'include\angelscript.h') -Type Leaf
        Assert-CKAngelScriptRuntimeCompatible -RootDir $ckasRuntime
    }
}

New-BMLCleanDirectory $output
New-BMLCleanDirectory $zipContentsRoot

$runtimeFiles = Join-Path $zipContentsRoot 'runtime'
New-BMLCleanDirectory $runtimeFiles
Copy-BMLDirectoryContents -SourceDir $runtimeSource -DestinationDir $runtimeFiles
foreach ($name in $defaultMods) {
    Copy-RequiredFile -Source (Join-Path $releaseInstall "Mods\$name") `
        -Destination (Join-Path $runtimeFiles "ModLoader\Mods\$name")
}
Copy-RequiredFile -Source (Join-Path $releaseBin 'BMLPlus.dll') -Destination (Join-Path $runtimeFiles 'BuildingBlocks\BMLPlus.dll')
Copy-RequiredFile -Source (Join-Path $releaseBin 'Updater.exe') -Destination (Join-Path $runtimeFiles 'Bin\Updater.exe')
Write-UpdaterBootstrapReadme -DestinationDir (Join-Path $runtimeFiles 'Bin') -Version $Version
Write-UpdaterSourcesJson -DestinationDir (Join-Path $runtimeFiles 'ModLoader\Updater') -BaseUrl $UpdaterBaseUrl -DefaultChannel $UpdaterDefaultChannel
Copy-RequiredFile -Source (Join-Path $layout.RepoRoot 'LICENSE') -Destination (Join-Path $runtimeFiles 'LICENSE')
Copy-RequiredFile -Source (Join-Path $layout.RepoRoot 'README.md') -Destination (Join-Path $runtimeFiles 'README.md')
Copy-RequiredFile -Source (Join-Path $layout.RepoRoot 'README_zh-CN.md') -Destination (Join-Path $runtimeFiles 'README_zh-CN.md')

if ($IncludeAngelScript) {
    Copy-RequiredFile -Source $ckasRuntimeDll -Destination (Join-Path $runtimeFiles 'BuildingBlocks\AngelScript.dll')
}

New-BMLZipFromDirectory -SourceDir $runtimeFiles -ZipPath (Join-Path $output "BMLPlus-$Version.zip")

$modFiles = Join-Path $zipContentsRoot 'mods'
New-BMLCleanDirectory $modFiles
Copy-RequiredFile -Source (Join-Path $layout.RepoRoot 'mods\LICENSE') `
    -Destination (Join-Path $modFiles 'Mods-LICENSE.txt')
foreach ($name in $modPackages) {
    Copy-RequiredFile -Source (Join-Path $releaseInstall "Mods\$name") `
        -Destination (Join-Path $modFiles "ModLoader\Mods\$name")
}
New-BMLZipFromDirectory -SourceDir $modFiles -ZipPath (Join-Path $output "BMLPlus-Mods-$Version.zip")

$updaterFiles = Join-Path $zipContentsRoot 'updater-runtime'
Copy-BMLDirectoryFresh -SourceDir $runtimeFiles -DestinationDir $updaterFiles
foreach ($forbidden in @(
    (Join-Path $updaterFiles 'Bin\Updater.exe'),
    (Join-Path $updaterFiles 'Bin\Updater-README.txt'),
    (Join-Path $updaterFiles 'ModLoader\Updater'),
    (Join-Path $updaterFiles 'ModLoader\Mods'),
    (Join-Path $updaterFiles 'ModLoader\Configs')
)) {
    if (Test-Path -LiteralPath $forbidden) {
        Remove-Item -LiteralPath $forbidden -Recurse -Force
    }
}

$updaterZip = Join-Path $output "BMLPlus-Update-$Version.zip"
New-BMLZipFromDirectory -SourceDir $updaterFiles -ZipPath $updaterZip
$updaterManifest = Join-Path $output "BMLPlus-Update-$Version.manifest.json"
$manifestObject = New-UpdaterManifestObject -Version $Version -PackagePath $updaterZip -PackageDirectory $updaterFiles
Write-Utf8NoBomText -Path $updaterManifest -Text (($manifestObject | ConvertTo-Json -Depth 6) + "`n")
$updaterManifestSignature = "$updaterManifest.sig"
if ($SkipUpdateSigning) {
    Write-Warning "Skipping updater manifest signature: $updaterManifestSignature"
} else {
    Write-UpdaterManifestSignature -ManifestPath $updaterManifest -SignaturePath $updaterManifestSignature -CngKeyName $SigningCngKeyName
}

$releaseSdkFiles = Join-Path $zipContentsRoot 'sdk-release'
New-BMLCleanDirectory $releaseSdkFiles
Copy-BMLDirectoryContents -SourceDir $releaseInstall -DestinationDir $releaseSdkFiles

if ($IncludeAngelScript) {
    Copy-CKAngelScriptHeaders -DestinationIncludeDir (Join-Path $releaseSdkFiles 'include')
}

Assert-BMLSdkDirectory -SdkDir $releaseSdkFiles -RequireAngelScript:$IncludeAngelScript
New-BMLZipFromDirectory -SourceDir $releaseSdkFiles -ZipPath (Join-Path $output "BMLPlus-SDK-$Version-Release.zip")

$debugSdkFiles = Join-Path $zipContentsRoot 'sdk-debug'
New-BMLCleanDirectory $debugSdkFiles
Copy-BMLDirectoryContents -SourceDir $debugInstall -DestinationDir $debugSdkFiles
Copy-RequiredFile -Source (Join-Path $debugBin 'BMLPlus.pdb') -Destination (Join-Path $debugSdkFiles 'bin\BMLPlus.pdb')
if ($IncludeAngelScript) {
    Copy-CKAngelScriptHeaders -DestinationIncludeDir (Join-Path $debugSdkFiles 'include')
}

Assert-BMLSdkDirectory -SdkDir $debugSdkFiles -RequireAngelScript:$IncludeAngelScript
New-BMLZipFromDirectory -SourceDir $debugSdkFiles -ZipPath (Join-Path $output "BMLPlus-SDK-$Version-Debug.zip")

Assert-ModArchiveMatchesInstall -ArchivePath (Join-Path $output "BMLPlus-$Version.zip") `
    -InstallDir $releaseInstall -EntryPrefix 'ModLoader/Mods/' -Packages $defaultMods
Assert-ModArchiveMatchesInstall -ArchivePath (Join-Path $output "BMLPlus-Mods-$Version.zip") `
    -InstallDir $releaseInstall -EntryPrefix 'ModLoader/Mods/' -Packages $modPackages
Assert-ModArchiveMatchesInstall -ArchivePath (Join-Path $output "BMLPlus-SDK-$Version-Release.zip") `
    -InstallDir $releaseInstall -EntryPrefix 'Mods/' -Packages $modPackages
Assert-ModArchiveMatchesInstall -ArchivePath (Join-Path $output "BMLPlus-SDK-$Version-Debug.zip") `
    -InstallDir $debugInstall -EntryPrefix 'Mods/' -Packages $debugModPackages

Remove-PackageStagingDirectory -Path $zipContentsRoot -OutputDir $output

Get-ChildItem -LiteralPath $output -Filter '*.zip' | ForEach-Object {
    Write-Host "Created $($_.FullName)"
}
