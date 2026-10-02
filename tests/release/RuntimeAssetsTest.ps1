[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SourceRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $SourceRoot 'scripts\lib\BMLProject.psm1') -Force
Add-Type -AssemblyName System.IO.Compression.FileSystem

$tempDirectory = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
$tempRoot = Join-Path $tempDirectory ('BMLRuntimeAssetsTest-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempRoot | Out-Null
try {
    $runtimeSource = Join-Path $SourceRoot 'packaging\runtime'
    $runtimeFiles = Join-Path $tempRoot 'runtime'
    Copy-BMLDirectoryContents -SourceDir $runtimeSource -DestinationDir $runtimeFiles
    $archivePath = Join-Path $tempRoot 'runtime.zip'
    New-BMLZipFromDirectory -SourceDir $runtimeFiles -ZipPath $archivePath
    $archive = [System.IO.Compression.ZipFile]::OpenRead($archivePath)
    try {
        foreach ($entry in $archive.Entries) {
            if ($entry.FullName.StartsWith('ModLoader/Configs/', [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "User configuration must not be packaged: $($entry.FullName)"
            }
        }
        $fontSources = @(Get-ChildItem -LiteralPath (Join-Path $runtimeSource 'ModLoader\Fonts') -File)
        if ($fontSources.Count -eq 0) {
            throw 'Runtime source has no bundled font assets.'
        }
        foreach ($font in $fontSources) {
            $entry = $archive.GetEntry('ModLoader/Fonts/' + $font.Name)
            if (-not $entry) {
                throw "Missing bundled font asset: $($font.Name)"
            }
            $stream = $entry.Open()
            $sha = [System.Security.Cryptography.SHA256]::Create()
            try {
                $actual = [System.BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '')
                $expected = (Get-FileHash -LiteralPath $font.FullName -Algorithm SHA256).Hash
                if ($actual -cne $expected) {
                    throw "Bundled font asset differs from source: $($font.Name)"
                }
            } finally {
                $sha.Dispose()
                $stream.Dispose()
            }
        }
    } finally {
        $archive.Dispose()
    }
    Write-Host 'Runtime assets exclude user configurations and retain all bundled font assets.'
} finally {
    $resolvedRoot = [System.IO.Path]::GetFullPath($tempRoot)
    $expectedParent = $tempDirectory.TrimEnd('\', '/')
    if ([System.IO.Path]::GetDirectoryName($resolvedRoot) -cne $expectedParent -or
        -not [System.IO.Path]::GetFileName($resolvedRoot).StartsWith('BMLRuntimeAssetsTest-')) {
        throw 'Refusing to clean an unexpected temporary directory.'
    }
    Remove-Item -LiteralPath $resolvedRoot -Recurse -Force
}
