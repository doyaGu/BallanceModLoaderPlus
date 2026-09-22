[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^v\d+\.\d+\.\d+$')]
    [string]$Version,

    [Parameter(Mandatory = $true)]
    [string]$InputDir,

    [Parameter(Mandatory = $true)]
    [string]$OutputDir,

    [Parameter(Mandatory = $true)]
    [string]$SigningCngKeyName,

    [ValidatePattern('^[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+$')]
    [string]$Repository = 'doyaGu/BallanceModLoaderPlus',

    [string]$PreviousChannelPath,

    [string]$PreviousChannelSignaturePath,

    [switch]$AllowEmptyRevocations
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'lib\BMLReleaseFiles.psm1') -Force

$input = [System.IO.Path]::GetFullPath($InputDir)
$output = [System.IO.Path]::GetFullPath($OutputDir)
if ($input -eq $output) {
    throw 'InputDir and OutputDir must differ so the CI ZIP files and updater manifest remain unchanged.'
}
$inputPrefix = $input.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
if ($output.StartsWith($inputPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'OutputDir must not be inside InputDir because signing must not modify the CI output directory.'
}

Assert-BMLReleaseFiles -Directory $input -Version $Version -SignatureState Unsigned
Assert-BMLUpdaterManifestMatchesPackage -Directory $input -Version $Version

if ($PreviousChannelPath -and $AllowEmptyRevocations) {
    throw 'Use PreviousChannelPath or AllowEmptyRevocations, not both.'
}
if ($PreviousChannelSignaturePath -and -not $PreviousChannelPath) {
    throw 'PreviousChannelSignaturePath requires PreviousChannelPath.'
}
$revokedVersions = @()
$revokedManifestHashes = @()
if ($PreviousChannelPath) {
    if (-not (Test-Path -LiteralPath $PreviousChannelPath -PathType Leaf)) {
        throw "Previous channel file is missing: $PreviousChannelPath"
    }
    if (-not $PreviousChannelSignaturePath) {
        $PreviousChannelSignaturePath = "$PreviousChannelPath.sig"
    }
    Assert-BMLCngSignature `
        -InputPath $PreviousChannelPath `
        -SignaturePath $PreviousChannelSignaturePath `
        -CngKeyName $SigningCngKeyName
    try {
        $previousChannel = Get-Content -LiteralPath $PreviousChannelPath -Raw | ConvertFrom-Json
    } catch {
        throw "Previous channel file is not valid JSON: $($_.Exception.Message)"
    }
    foreach ($property in @('schemaVersion', 'revokedVersions', 'revokedManifestHashes')) {
        if ($property -notin $previousChannel.PSObject.Properties.Name) {
            throw "Previous channel file is missing required property: $property"
        }
    }
    if ($previousChannel.schemaVersion -ne 1) {
        throw "Previous channel schemaVersion must be 1, got '$($previousChannel.schemaVersion)'."
    }
    $revokedVersions = @($previousChannel.revokedVersions)
    $revokedManifestHashes = @($previousChannel.revokedManifestHashes)
} elseif (-not $AllowEmptyRevocations) {
    throw 'PreviousChannelPath is required to preserve revocations. Use AllowEmptyRevocations only when creating the first channel.'
}
if ($Version -in $revokedVersions) {
    throw "Cannot publish revoked version: $Version"
}
$manifestHash = Get-BMLFileSha256 -Path (Join-Path $input "BMLPlus-Update-$Version.manifest.json")
if ($manifestHash -in @($revokedManifestHashes | ForEach-Object { $_.ToLowerInvariant() })) {
    throw "Cannot publish updater manifest whose hash is revoked: $manifestHash"
}

if (Test-Path -LiteralPath $output) {
    if (@(Get-ChildItem -LiteralPath $output -Force).Count -ne 0) {
        throw "OutputDir must not contain existing files: $output"
    }
} else {
    New-Item -ItemType Directory -Path $output | Out-Null
}

foreach ($name in Get-BMLReleaseFileNames -Version $Version -SignatureState Unsigned) {
    Copy-Item -LiteralPath (Join-Path $input $name) -Destination (Join-Path $output $name)
}

$manifestName = "BMLPlus-Update-$Version.manifest.json"
Write-BMLCngSignature `
    -InputPath (Join-Path $output $manifestName) `
    -SignaturePath (Join-Path $output "$manifestName.sig") `
    -CngKeyName $SigningCngKeyName

$channelPath = Join-Path $output 'stable.json'
Write-BMLUpdaterChannel `
    -Path $channelPath `
    -Version $Version `
    -Repository $Repository `
    -RevokedVersions $revokedVersions `
    -RevokedManifestHashes $revokedManifestHashes
Write-BMLCngSignature `
    -InputPath $channelPath `
    -SignaturePath "$channelPath.sig" `
    -CngKeyName $SigningCngKeyName

Write-BMLSha256Sums -Directory $output
Assert-BMLReleaseFiles -Directory $output -Version $Version -SignatureState Signed
Assert-BMLUpdaterManifestMatchesPackage -Directory $output -Version $Version
Assert-BMLStableJson -Directory $output -Version $Version -Repository $Repository

foreach ($name in Get-BMLReleaseFileNames -Version $Version -SignatureState Signed) {
    Write-Host "Signed release file ready: $(Join-Path $output $name)"
}
