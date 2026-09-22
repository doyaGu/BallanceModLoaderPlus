[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SourceRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $SourceRoot 'scripts\lib\BMLReleaseFiles.psm1') -Force
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

function Get-StreamHash {
    param([System.IO.Stream]$Stream)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($sha.ComputeHash($Stream))).Replace('-', '').ToLowerInvariant()
    } finally {
        $sha.Dispose()
    }
}

function New-TestZip {
    param([string]$Path, [hashtable]$Files)

    $archive = [System.IO.Compression.ZipFile]::Open($Path, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in @($Files.Keys | Sort-Object)) {
            $entry = $archive.CreateEntry($name)
            $stream = $entry.Open()
            try {
                $bytes = [System.Text.Encoding]::UTF8.GetBytes([string]$Files[$name])
                $stream.Write($bytes, 0, $bytes.Length)
            } finally {
                $stream.Dispose()
            }
        }
    } finally {
        $archive.Dispose()
    }
}

function Get-TestManifestFiles {
    param([string]$ZipPath)

    $files = @()
    $archive = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)
    try {
        foreach ($entry in $archive.Entries) {
            if (-not $entry.Name) {
                continue
            }
            $stream = $entry.Open()
            try {
                $hash = Get-StreamHash -Stream $stream
            } finally {
                $stream.Dispose()
            }
            $files += [ordered]@{
                path = $entry.FullName
                sha256 = $hash
                size = $entry.Length
            }
        }
    } finally {
        $archive.Dispose()
    }
    return $files
}

function Assert-SignatureValid {
    param([string]$InputPath, [string]$SignaturePath, [System.Security.Cryptography.CngKey]$Key)

    $ecdsa = [System.Security.Cryptography.ECDsaCng]::new($Key)
    try {
        $bytes = [System.IO.File]::ReadAllBytes($InputPath)
        $sha = [System.Security.Cryptography.SHA256]::Create()
        try {
            $hash = $sha.ComputeHash($bytes)
        } finally {
            $sha.Dispose()
        }
        $signature = [System.Convert]::FromBase64String(
            [System.IO.File]::ReadAllText($SignaturePath, [System.Text.Encoding]::ASCII))
        Assert-True -Condition ($ecdsa.VerifyHash($hash, $signature)) -Message "Invalid signature: $SignaturePath"
    } finally {
        $ecdsa.Dispose()
    }
}

$version = 'v1.2.3'
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("bml-release-test-" + [guid]::NewGuid().ToString('N'))
$unsigned = Join-Path $tempRoot 'unsigned'
$signed = Join-Path $tempRoot 'signed'
$pages = Join-Path $tempRoot 'pages'
$previousChannelPath = Join-Path $tempRoot 'previous-stable.json'
$keyName = "BMLReleaseTest-" + [guid]::NewGuid().ToString('N')
$key = $null

try {
    New-Item -ItemType Directory -Path $unsigned, $signed, $pages | Out-Null

    foreach ($name in @(
        "BMLPlus-$version.zip",
        "BMLPlus-Mods-$version.zip",
        "BMLPlus-SDK-$version-Release.zip",
        "BMLPlus-SDK-$version-Debug.zip"
    )) {
        New-TestZip -Path (Join-Path $unsigned $name) -Files @{ 'fixture.txt' = $name }
    }

    $updaterName = "BMLPlus-Update-$version.zip"
    $updaterPath = Join-Path $unsigned $updaterName
    New-TestZip -Path $updaterPath -Files @{
        'BuildingBlocks/BMLPlus.dll' = 'release binary'
        'LICENSE' = 'license text'
    }
    $manifestName = "BMLPlus-Update-$version.manifest.json"
    $manifest = [ordered]@{
        schemaVersion = 1
        version = $version
        package = [ordered]@{
            fileName = $updaterName
            sha256 = Get-BMLFileSha256 -Path $updaterPath
        }
        managedFiles = @(Get-TestManifestFiles -ZipPath $updaterPath)
        preserve = @()
        removeFiles = @()
    }
    Write-BMLUtf8NoBomText `
        -Path (Join-Path $unsigned $manifestName) `
        -Text (($manifest | ConvertTo-Json -Depth 6) + "`n")

    $prereleaseVersion = 'v1.2.3-alpha.1'
    $prerelease = Join-Path $tempRoot 'prerelease'
    New-Item -ItemType Directory -Path $prerelease | Out-Null
    foreach ($file in Get-ChildItem -LiteralPath $unsigned -File) {
        $prereleaseName = $file.Name.Replace($version, $prereleaseVersion)
        Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $prerelease $prereleaseName)
    }
    $prereleaseUpdaterName = "BMLPlus-Update-$prereleaseVersion.zip"
    $prereleaseManifestName = "BMLPlus-Update-$prereleaseVersion.manifest.json"
    $prereleaseManifest = Get-Content `
        -LiteralPath (Join-Path $prerelease $prereleaseManifestName) -Raw | ConvertFrom-Json
    $prereleaseManifest.version = $prereleaseVersion
    $prereleaseManifest.package.fileName = $prereleaseUpdaterName
    Write-BMLUtf8NoBomText `
        -Path (Join-Path $prerelease $prereleaseManifestName) `
        -Text (($prereleaseManifest | ConvertTo-Json -Depth 6) + "`n")
    Assert-BMLReleaseFiles `
        -Directory $prerelease -Version $prereleaseVersion -SignatureState Unsigned
    Assert-BMLUpdaterManifestMatchesPackage `
        -Directory $prerelease -Version $prereleaseVersion

    $signedPrereleaseRejected = $false
    try {
        Get-BMLReleaseFileNames `
            -Version $prereleaseVersion -SignatureState Signed | Out-Null
    } catch {
        $signedPrereleaseRejected = $_.Exception.Message.Contains(
            'cannot be treated as signed stable release files',
            [System.StringComparison]::Ordinal)
    }
    Assert-True `
        -Condition $signedPrereleaseRejected `
        -Message 'Prerelease packages must remain outside the signed stable release flow.'

    $zipHashes = @{}
    foreach ($zip in Get-ChildItem -LiteralPath $unsigned -Filter '*.zip') {
        $zipHashes[$zip.Name] = Get-BMLFileSha256 -Path $zip.FullName
    }

    $key = [System.Security.Cryptography.CngKey]::Create(
        [System.Security.Cryptography.CngAlgorithm]::EcdsaP256,
        $keyName)
    $missingPreviousRejected = $false
    try {
        & (Join-Path $SourceRoot 'scripts\Sign-BMLReleaseFiles.ps1') `
            -Version $version `
            -InputDir $unsigned `
            -OutputDir (Join-Path $tempRoot 'missing-previous') `
            -SigningCngKeyName $keyName `
            -Repository 'example/BML'
    } catch {
        $missingPreviousRejected = $_.Exception.Message.Contains(
            'PreviousChannelPath',
            [System.StringComparison]::Ordinal)
    }
    Assert-True `
        -Condition $missingPreviousRejected `
        -Message 'The signing command must require the previous stable.json unless empty revocation lists are explicitly acknowledged.'

    $previousChannel = [ordered]@{
        schemaVersion = 1
        version = 'v1.2.2'
        packageUrl = 'https://example.invalid/previous.zip'
        manifestUrl = 'https://example.invalid/previous.manifest.json'
        revokedVersions = @('v1.0.0')
        revokedManifestHashes = @('AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA')
    }
    Write-BMLUtf8NoBomText `
        -Path $previousChannelPath `
        -Text (($previousChannel | ConvertTo-Json -Depth 6) + "`n")
    Write-BMLCngSignature `
        -InputPath $previousChannelPath `
        -SignaturePath "$previousChannelPath.sig" `
        -CngKeyName $keyName

    $alteredPreviousPath = Join-Path $tempRoot 'altered-previous-stable.json'
    Copy-Item -LiteralPath $previousChannelPath -Destination $alteredPreviousPath
    Copy-Item -LiteralPath "$previousChannelPath.sig" -Destination "$alteredPreviousPath.sig"
    $alteredPreviousText = [System.IO.File]::ReadAllText($alteredPreviousPath).Replace('v1.2.2', 'v1.2.1')
    Write-BMLUtf8NoBomText -Path $alteredPreviousPath -Text $alteredPreviousText
    $alteredPreviousRejected = $false
    try {
        & (Join-Path $SourceRoot 'scripts\Sign-BMLReleaseFiles.ps1') `
            -Version $version `
            -InputDir $unsigned `
            -OutputDir (Join-Path $tempRoot 'altered-previous-output') `
            -SigningCngKeyName $keyName `
            -Repository 'example/BML' `
            -PreviousChannelPath $alteredPreviousPath
    } catch {
        $alteredPreviousRejected = $_.Exception.Message.Contains(
            'Signature does not match',
            [System.StringComparison]::Ordinal)
    }
    Assert-True `
        -Condition $alteredPreviousRejected `
        -Message 'Signing must reject a modified previous stable.json.'

    & (Join-Path $SourceRoot 'scripts\Sign-BMLReleaseFiles.ps1') `
        -Version $version `
        -InputDir $unsigned `
        -OutputDir $signed `
        -SigningCngKeyName $keyName `
        -Repository 'example/BML' `
        -PreviousChannelPath $previousChannelPath

    Assert-BMLReleaseFiles -Directory $signed -Version $version -SignatureState Signed
    Assert-BMLUpdaterManifestMatchesPackage -Directory $signed -Version $version
    foreach ($entry in $zipHashes.GetEnumerator()) {
        $actual = Get-BMLFileSha256 -Path (Join-Path $signed $entry.Key)
        Assert-True -Condition ($actual -ceq $entry.Value) -Message "Signing changed CI ZIP bytes: $($entry.Key)"
    }

    Assert-SignatureValid `
        -InputPath (Join-Path $signed $manifestName) `
        -SignaturePath (Join-Path $signed "$manifestName.sig") `
        -Key $key
    Assert-SignatureValid `
        -InputPath (Join-Path $signed 'stable.json') `
        -SignaturePath (Join-Path $signed 'stable.json.sig') `
        -Key $key

    $channel = Get-Content -LiteralPath (Join-Path $signed 'stable.json') -Raw | ConvertFrom-Json
    Assert-True `
        -Condition ($channel.packageUrl -ceq "https://github.com/example/BML/releases/download/$version/$updaterName") `
        -Message 'stable.json packageUrl is not versioned and immutable.'
    Assert-True `
        -Condition (@($channel.revokedVersions).Count -eq 1 -and $channel.revokedVersions[0] -ceq 'v1.0.0') `
        -Message 'The new stable.json dropped a previously revoked version.'
    Assert-True `
        -Condition (@($channel.revokedManifestHashes).Count -eq 1 -and $channel.revokedManifestHashes[0] -ceq (('a' * 64) -join '')) `
        -Message 'The new stable.json dropped a previously revoked manifest hash.'

    $checksums = @(Get-Content -LiteralPath (Join-Path $signed 'SHA256SUMS.txt'))
    Assert-True -Condition ($checksums.Count -eq 9) -Message 'SHA256SUMS.txt must cover the other nine signed release files.'

    & git -C $pages init --initial-branch=gh-pages | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw 'Unable to create gh-pages test checkout.'
    }
    & git -C $pages config user.name 'BML Release Test'
    & git -C $pages config user.email 'release-test@example.invalid'
    New-Item -ItemType Directory -Path (Join-Path $pages 'styles') | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $pages 'index.html'), 'documentation root')
    [System.IO.File]::WriteAllText((Join-Path $pages 'styles\site.css'), 'documentation CSS')
    & git -C $pages add --all
    & git -C $pages commit -m 'Test documentation site' | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw 'Unable to commit the gh-pages test fixture.'
    }
    $siteHash = Get-BMLFileSha256 -Path (Join-Path $pages 'index.html')

    & (Join-Path $SourceRoot 'scripts\Copy-BMLUpdaterFilesToPages.ps1') `
        -Version $version `
        -ReleaseDir $signed `
        -GhPagesCheckout $pages `
        -Repository 'example/BML'

    Assert-True `
        -Condition ((Get-BMLFileSha256 -Path (Join-Path $pages 'index.html')) -ceq $siteHash) `
        -Message 'Copying stable.json files changed the documentation site.'
    foreach ($relative in @('updates\index.html', 'updates\stable.json', 'updates\stable.json.sig')) {
        Assert-True `
            -Condition (Test-Path -LiteralPath (Join-Path $pages $relative) -PathType Leaf) `
            -Message "Missing copied updater file: $relative"
    }

    $bad = Join-Path $tempRoot 'bad'
    Copy-Item -LiteralPath $unsigned -Destination $bad -Recurse
    $stream = [System.IO.File]::Open((Join-Path $bad $updaterName), [System.IO.FileMode]::Append)
    try {
        $stream.WriteByte(0)
    } finally {
        $stream.Dispose()
    }
    $rejected = $false
    try {
        Assert-BMLUpdaterManifestMatchesPackage -Directory $bad -Version $version
    } catch {
        $rejected = $_.Exception.Message.Contains('package hash', [System.StringComparison]::OrdinalIgnoreCase)
    }
    Assert-True -Condition $rejected -Message 'Signing must reject an updater ZIP that differs from its manifest.'

    $forbidden = Join-Path $tempRoot 'forbidden'
    Copy-Item -LiteralPath $unsigned -Destination $forbidden -Recurse
    $forbiddenUpdaterPath = Join-Path $forbidden $updaterName
    [System.IO.File]::Delete($forbiddenUpdaterPath)
    New-TestZip -Path $forbiddenUpdaterPath -Files @{
        'ModLoader/Configs/BML.cfg' = 'must not be updater-managed'
    }
    $forbiddenManifest = [ordered]@{
        schemaVersion = 1
        version = $version
        package = [ordered]@{
            fileName = $updaterName
            sha256 = Get-BMLFileSha256 -Path $forbiddenUpdaterPath
        }
        managedFiles = @(Get-TestManifestFiles -ZipPath $forbiddenUpdaterPath)
        preserve = @()
        removeFiles = @()
    }
    Write-BMLUtf8NoBomText `
        -Path (Join-Path $forbidden $manifestName) `
        -Text (($forbiddenManifest | ConvertTo-Json -Depth 6) + "`n")
    $forbiddenRejected = $false
    try {
        Assert-BMLUpdaterManifestMatchesPackage -Directory $forbidden -Version $version
    } catch {
        $forbiddenRejected = $_.Exception.Message.Contains(
            'unmanaged path',
            [System.StringComparison]::OrdinalIgnoreCase)
    }
    Assert-True `
        -Condition $forbiddenRejected `
        -Message 'Signing must reject updater ZIP files that manage user configuration.'

    Write-Host 'Release signing and gh-pages file-copy checks passed.'
} finally {
    if ($key) {
        try {
            $key.Delete()
        } finally {
            $key.Dispose()
        }
    }
    $resolvedTempRoot = [System.IO.Path]::GetFullPath($tempRoot)
    $systemTemp = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
    if ($resolvedTempRoot.StartsWith($systemTemp, [System.StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $resolvedTempRoot)) {
        Remove-Item -LiteralPath $resolvedTempRoot -Recurse -Force
    }
}
