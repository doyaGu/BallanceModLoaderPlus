Set-StrictMode -Version Latest

function Get-BMLReleaseFileNames {
    param(
        [Parameter(Mandatory = $true)]
        [ValidatePattern('^v\d+\.\d+\.\d+(?:-[0-9A-Za-z]+(?:\.[0-9A-Za-z]+)*)?$')]
        [string]$Version,

        [Parameter(Mandatory = $true)]
        [ValidateSet('Unsigned', 'Signed')]
        [string]$SignatureState
    )

    if ($SignatureState -eq 'Signed' -and $Version -notmatch '^v\d+\.\d+\.\d+$') {
        throw "Prerelease packages cannot be treated as signed stable release files: $Version"
    }

    $unsigned = @(
        "BMLPlus-$Version.zip",
        "BMLPlus-Update-$Version.zip",
        "BMLPlus-Update-$Version.manifest.json",
        "BMLPlus-SDK-$Version-Release.zip",
        "BMLPlus-SDK-$Version-Debug.zip"
    )
    if ($SignatureState -eq 'Unsigned') {
        return $unsigned
    }

    return @(
        $unsigned
        "BMLPlus-Update-$Version.manifest.json.sig"
        'stable.json'
        'stable.json.sig'
        'SHA256SUMS.txt'
    )
}

function Get-BMLFileSha256 {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required file is missing: $Path"
    }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Assert-BMLProductionRuntime {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Production runtime is missing: $Path"
    }

    $contents = [System.Text.Encoding]::ASCII.GetString(
        [System.IO.File]::ReadAllBytes([System.IO.Path]::GetFullPath($Path)))
    $privateInterfaceIds = @('bml.test.behavior')
    foreach ($interfaceId in $privateInterfaceIds) {
        if ($contents.Contains($interfaceId, [System.StringComparison]::Ordinal)) {
            throw "Production runtime contains private test interface: $interfaceId"
        }
    }
}

function Assert-BMLReleaseFiles {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Directory,

        [Parameter(Mandatory = $true)]
        [ValidatePattern('^v\d+\.\d+\.\d+(?:-[0-9A-Za-z]+(?:\.[0-9A-Za-z]+)*)?$')]
        [string]$Version,

        [Parameter(Mandatory = $true)]
        [ValidateSet('Unsigned', 'Signed')]
        [string]$SignatureState
    )

    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) {
        throw "Release directory is missing: $Directory"
    }

    $expected = @(Get-BMLReleaseFileNames -Version $Version -SignatureState $SignatureState)
    $entries = @(Get-ChildItem -LiteralPath $Directory -Force)
    $actual = @($entries | Where-Object { -not $_.PSIsContainer } | ForEach-Object { $_.Name })
    $expectedSet = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    $expectedSet.UnionWith([string[]]$expected)
    $actualSet = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    $actualSet.UnionWith([string[]]$actual)
    $missing = @($expected | Where-Object { -not $actualSet.Contains($_) })
    $unexpected = @($actual | Where-Object { -not $expectedSet.Contains($_) })
    $unexpected += @($entries | Where-Object { $_.PSIsContainer } | ForEach-Object { "$($_.Name)/" })
    if ($missing.Count -gt 0 -or $unexpected.Count -gt 0) {
        $parts = @()
        if ($missing.Count -gt 0) {
            $parts += "missing: $($missing -join ', ')"
        }
        if ($unexpected.Count -gt 0) {
            $parts += "unexpected: $($unexpected -join ', ')"
        }
        throw "The $SignatureState release directory has the wrong files in ${Directory}: $($parts -join '; ')"
    }

    if ($SignatureState -eq 'Signed') {
        Assert-BMLP1363SignatureFile -Path (Join-Path $Directory "BMLPlus-Update-$Version.manifest.json.sig")
        Assert-BMLP1363SignatureFile -Path (Join-Path $Directory 'stable.json.sig')
    }
}

function Assert-BMLP1363SignatureFile {
    param([string]$Path)

    try {
        $signature = [System.Convert]::FromBase64String(
            [System.IO.File]::ReadAllText($Path, [System.Text.Encoding]::ASCII))
    } catch {
        throw "Signature file is not valid Base64: $Path"
    }
    if ($signature.Length -ne 64) {
        throw "Signature file must contain a 64-byte ECDSA P-256 P1363 signature: $Path"
    }
}

function Test-BMLSafeArchivePath {
    param([string]$Path)

    if (-not $Path -or $Path.Contains('\') -or $Path.Contains(':') -or $Path.StartsWith('/')) {
        return $false
    }
    foreach ($segment in $Path.Split('/')) {
        if (-not $segment -or $segment -eq '.' -or $segment -eq '..' -or
            $segment.EndsWith('.') -or $segment.EndsWith(' ')) {
            return $false
        }
    }
    return $true
}

function Test-BMLUpdaterManagedPath {
    param([string]$Path)

    if (-not (Test-BMLSafeArchivePath -Path $Path)) {
        return $false
    }
    $lower = $Path.ToLowerInvariant()
    if ($lower -eq 'bin/updater.exe' -or
        $lower.StartsWith('modloader/updater/') -or
        $lower.StartsWith('modloader/mods/') -or
        $lower.StartsWith('modloader/configs/')) {
        return $false
    }
    return $true
}

function Get-BMLStreamSha256 {
    param([System.IO.Stream]$Stream)

    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($sha.ComputeHash($Stream))).Replace('-', '').ToLowerInvariant()
    } finally {
        $sha.Dispose()
    }
}

function Assert-BMLUpdaterManifestMatchesPackage {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Directory,

        [Parameter(Mandatory = $true)]
        [ValidatePattern('^v\d+\.\d+\.\d+(?:-[0-9A-Za-z]+(?:\.[0-9A-Za-z]+)*)?$')]
        [string]$Version
    )

    $packageName = "BMLPlus-Update-$Version.zip"
    $manifestName = "BMLPlus-Update-$Version.manifest.json"
    $packagePath = Join-Path $Directory $packageName
    $manifestPath = Join-Path $Directory $manifestName
    foreach ($path in @($packagePath, $manifestPath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required updater file is missing: $path"
        }
    }

    try {
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    } catch {
        throw "Updater manifest is not valid JSON: $($_.Exception.Message)"
    }

    if ($manifest.schemaVersion -ne 1) {
        throw "Updater manifest schemaVersion must be 1, got '$($manifest.schemaVersion)'."
    }
    if ($manifest.version -cne $Version) {
        throw "Updater manifest version '$($manifest.version)' does not match '$Version'."
    }
    if ($manifest.package.fileName -cne $packageName) {
        throw "Updater manifest package '$($manifest.package.fileName)' does not match '$packageName'."
    }

    $packageHash = Get-BMLFileSha256 -Path $packagePath
    if ($manifest.package.sha256 -cne $packageHash) {
        throw "Updater package hash does not match $manifestName."
    }

    $manifestFiles = [System.Collections.Generic.Dictionary[string, object]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($file in @($manifest.managedFiles)) {
        $path = [string]$file.path
        if (-not (Test-BMLUpdaterManagedPath -Path $path)) {
            throw "Updater manifest contains an unsafe or unmanaged path: $path"
        }
        if (-not $manifestFiles.TryAdd($path, $file)) {
            throw "Updater manifest contains a duplicate path: $path"
        }
    }

    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::OpenRead($packagePath)
    try {
        $archivePaths = [System.Collections.Generic.HashSet[string]]::new(
            [System.StringComparer]::Ordinal)
        foreach ($entry in $archive.Entries) {
            if (-not $entry.Name) {
                continue
            }
            $path = $entry.FullName.Replace('\', '/')
            if (-not (Test-BMLUpdaterManagedPath -Path $path)) {
                throw "Updater ZIP contains an unsafe or unmanaged path: $path"
            }
            if (-not $archivePaths.Add($path)) {
                throw "Updater ZIP contains a duplicate path: $path"
            }

            $manifestFile = $null
            if (-not $manifestFiles.TryGetValue($path, [ref]$manifestFile)) {
                throw "Updater ZIP entry is missing from the manifest: $path"
            }
            if ([long]$manifestFile.size -ne [long]$entry.Length) {
                throw "Updater ZIP entry size does not match the manifest: $path"
            }

            $stream = $entry.Open()
            try {
                $entryHash = Get-BMLStreamSha256 -Stream $stream
            } finally {
                $stream.Dispose()
            }
            if ([string]$manifestFile.sha256 -cne $entryHash) {
                throw "Updater ZIP entry hash does not match the manifest: $path"
            }
        }

        if ($archivePaths.Count -ne $manifestFiles.Count) {
            $missing = @($manifestFiles.Keys | Where-Object { -not $archivePaths.Contains($_) })
            throw "Updater manifest contains files absent from the ZIP: $($missing -join ', ')"
        }
    } finally {
        $archive.Dispose()
    }
}

function Write-BMLUtf8NoBomText {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Text
    )

    $encoding = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText($Path, $Text, $encoding)
}

function Write-BMLCngSignature {
    param(
        [Parameter(Mandatory = $true)]
        [string]$InputPath,

        [Parameter(Mandatory = $true)]
        [string]$SignaturePath,

        [Parameter(Mandatory = $true)]
        [string]$CngKeyName
    )

    if (-not (Test-Path -LiteralPath $InputPath -PathType Leaf)) {
        throw "Cannot sign missing file: $InputPath"
    }

    $key = [System.Security.Cryptography.CngKey]::Open($CngKeyName)
    try {
        if ($key.AlgorithmGroup -ne [System.Security.Cryptography.CngAlgorithmGroup]::ECDsa) {
            throw "CNG key '$CngKeyName' is not an ECDSA key."
        }
        $ecdsa = [System.Security.Cryptography.ECDsaCng]::new($key)
        try {
            if ($ecdsa.KeySize -ne 256) {
                throw "CNG key '$CngKeyName' must be ECDSA P-256, got $($ecdsa.KeySize) bits."
            }
            $bytes = [System.IO.File]::ReadAllBytes($InputPath)
            $sha = [System.Security.Cryptography.SHA256]::Create()
            try {
                $signature = $ecdsa.SignHash($sha.ComputeHash($bytes))
            } finally {
                $sha.Dispose()
            }
            if ($signature.Length -ne 64) {
                throw "CNG ECDSA signature must be IEEE P1363 r||s format; got $($signature.Length) bytes."
            }
            [System.IO.File]::WriteAllText(
                $SignaturePath,
                [System.Convert]::ToBase64String($signature),
                [System.Text.Encoding]::ASCII)
        } finally {
            $ecdsa.Dispose()
        }
    } finally {
        $key.Dispose()
    }
}

function Assert-BMLCngSignature {
    param(
        [Parameter(Mandatory = $true)]
        [string]$InputPath,

        [Parameter(Mandatory = $true)]
        [string]$SignaturePath,

        [Parameter(Mandatory = $true)]
        [string]$CngKeyName
    )

    if (-not (Test-Path -LiteralPath $InputPath -PathType Leaf)) {
        throw "Signed file is missing: $InputPath"
    }
    Assert-BMLP1363SignatureFile -Path $SignaturePath
    $signature = [System.Convert]::FromBase64String(
        [System.IO.File]::ReadAllText($SignaturePath, [System.Text.Encoding]::ASCII))
    $key = [System.Security.Cryptography.CngKey]::Open($CngKeyName)
    try {
        if ($key.AlgorithmGroup -ne [System.Security.Cryptography.CngAlgorithmGroup]::ECDsa) {
            throw "CNG key '$CngKeyName' is not an ECDSA key."
        }
        $ecdsa = [System.Security.Cryptography.ECDsaCng]::new($key)
        try {
            if ($ecdsa.KeySize -ne 256) {
                throw "CNG key '$CngKeyName' must be ECDSA P-256, got $($ecdsa.KeySize) bits."
            }
            $bytes = [System.IO.File]::ReadAllBytes($InputPath)
            $sha = [System.Security.Cryptography.SHA256]::Create()
            try {
                $valid = $ecdsa.VerifyHash($sha.ComputeHash($bytes), $signature)
            } finally {
                $sha.Dispose()
            }
            if (-not $valid) {
                throw "Signature does not match the file and CNG key: $SignaturePath"
            }
        } finally {
            $ecdsa.Dispose()
        }
    } finally {
        $key.Dispose()
    }
}

function Write-BMLUpdaterChannel {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [ValidatePattern('^v\d+\.\d+\.\d+$')]
        [string]$Version,

        [Parameter(Mandatory = $true)]
        [ValidatePattern('^[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+$')]
        [string]$Repository,

        [string[]]$RevokedVersions = @(),

        [string[]]$RevokedManifestHashes = @()
    )

    foreach ($revokedVersion in $RevokedVersions) {
        if ($revokedVersion -notmatch '^v\d+\.\d+\.\d+$') {
            throw "Invalid revoked version: $revokedVersion"
        }
    }
    foreach ($revokedHash in $RevokedManifestHashes) {
        if ($revokedHash -notmatch '^[0-9a-fA-F]{64}$') {
            throw "Invalid revoked manifest hash: $revokedHash"
        }
    }

    $baseUrl = "https://github.com/$Repository/releases/download/$Version"
    $channel = [ordered]@{
        schemaVersion = 1
        version = $Version
        packageUrl = "$baseUrl/BMLPlus-Update-$Version.zip"
        manifestUrl = "$baseUrl/BMLPlus-Update-$Version.manifest.json"
        revokedVersions = @($RevokedVersions | Sort-Object -Unique)
        revokedManifestHashes = @($RevokedManifestHashes | ForEach-Object { $_.ToLowerInvariant() } | Sort-Object -Unique)
    }
    Write-BMLUtf8NoBomText -Path $Path -Text (($channel | ConvertTo-Json -Depth 6) + "`n")
}

function Assert-BMLStableJson {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Directory,

        [Parameter(Mandatory = $true)]
        [ValidatePattern('^v\d+\.\d+\.\d+$')]
        [string]$Version,

        [Parameter(Mandatory = $true)]
        [ValidatePattern('^[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+$')]
        [string]$Repository
    )

    $channelPath = Join-Path $Directory 'stable.json'
    try {
        $channel = Get-Content -LiteralPath $channelPath -Raw | ConvertFrom-Json
    } catch {
        throw "stable.json is not valid JSON: $($_.Exception.Message)"
    }
    foreach ($property in @(
        'schemaVersion',
        'version',
        'packageUrl',
        'manifestUrl',
        'revokedVersions',
        'revokedManifestHashes'
    )) {
        if ($property -notin $channel.PSObject.Properties.Name) {
            throw "stable.json is missing required property: $property"
        }
    }
    if ($channel.schemaVersion -ne 1) {
        throw "stable.json schemaVersion must be 1, got '$($channel.schemaVersion)'."
    }
    if ($channel.version -cne $Version) {
        throw "stable.json version '$($channel.version)' does not match '$Version'."
    }

    $baseUrl = "https://github.com/$Repository/releases/download/$Version"
    $expectedPackageUrl = "$baseUrl/BMLPlus-Update-$Version.zip"
    $expectedManifestUrl = "$baseUrl/BMLPlus-Update-$Version.manifest.json"
    if ($channel.packageUrl -cne $expectedPackageUrl) {
        throw "stable.json packageUrl does not match the versioned GitHub release file URL."
    }
    if ($channel.manifestUrl -cne $expectedManifestUrl) {
        throw "stable.json manifestUrl does not match the versioned GitHub release file URL."
    }

    foreach ($revokedVersion in @($channel.revokedVersions)) {
        if ([string]$revokedVersion -notmatch '^v\d+\.\d+\.\d+$') {
            throw "stable.json contains an invalid revoked version: $revokedVersion"
        }
    }
    foreach ($revokedHash in @($channel.revokedManifestHashes)) {
        if ([string]$revokedHash -notmatch '^[0-9a-f]{64}$') {
            throw "stable.json contains an invalid revoked manifest hash: $revokedHash"
        }
    }
    if ($Version -in @($channel.revokedVersions)) {
        throw "stable.json points to revoked version: $Version"
    }
    $manifestHash = Get-BMLFileSha256 -Path (Join-Path $Directory "BMLPlus-Update-$Version.manifest.json")
    if ($manifestHash -in @($channel.revokedManifestHashes)) {
        throw "stable.json points to a revoked manifest hash: $manifestHash"
    }

    Assert-BMLP1363SignatureFile -Path (Join-Path $Directory 'stable.json.sig')
}

function Write-BMLSha256Sums {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Directory,

        [string]$FileName = 'SHA256SUMS.txt'
    )

    $checksumPath = Join-Path $Directory $FileName
    $lines = @(
        Get-ChildItem -LiteralPath $Directory -File |
            Where-Object { $_.Name -cne $FileName } |
            Sort-Object Name |
            ForEach-Object {
                "$(Get-BMLFileSha256 -Path $_.FullName)  $($_.Name)"
            }
    )
    Write-BMLUtf8NoBomText -Path $checksumPath -Text (($lines -join "`n") + "`n")
}

Export-ModuleMember -Function `
    Get-BMLReleaseFileNames, `
    Get-BMLFileSha256, `
    Assert-BMLProductionRuntime, `
    Assert-BMLReleaseFiles, `
    Assert-BMLUpdaterManifestMatchesPackage, `
    Write-BMLUtf8NoBomText, `
    Write-BMLCngSignature, `
    Assert-BMLCngSignature, `
    Write-BMLUpdaterChannel, `
    Assert-BMLStableJson, `
    Write-BMLSha256Sums
