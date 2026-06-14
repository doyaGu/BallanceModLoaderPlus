[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Source,

    [Parameter(Mandatory = $true)]
    [string]$Output,

    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'lib\BMLProject.psm1') -Force

$sourceFull = [System.IO.Path]::GetFullPath($Source)
$outputFull = [System.IO.Path]::GetFullPath($Output)

Assert-BMLPath -Path $sourceFull -Type Container

if ([System.IO.Path]::GetExtension($outputFull) -ne '.zip') {
    throw 'Output must be a .zip script package. .bmodp is reserved for native mods.'
}

$entries = @(Get-ChildItem -LiteralPath $sourceFull -File -Filter '*.mod.as')
if ($entries.Count -ne 1) {
    throw "Source must contain exactly one top-level *.mod.as entry; found $($entries.Count)."
}

$outputDir = Split-Path -Parent $outputFull
if ($outputDir -and -not (Test-Path -LiteralPath $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

if (Test-Path -LiteralPath $outputFull) {
    if (-not $Force) {
        throw "Output already exists: $outputFull. Pass -Force to replace it."
    }
    Remove-Item -LiteralPath $outputFull -Force
}

$files = @(Get-ChildItem -LiteralPath $sourceFull -File -Recurse -Force)
if ($files.Count -eq 0) {
    throw "Source directory is empty: $sourceFull"
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$zip = [System.IO.Compression.ZipFile]::Open($outputFull, [System.IO.Compression.ZipArchiveMode]::Create)
try {
    $sourcePrefix = $sourceFull.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    foreach ($file in $files) {
        if (-not $file.FullName.StartsWith($sourcePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to pack file outside source directory: $($file.FullName)"
        }

        $relative = $file.FullName.Substring($sourcePrefix.Length)
        $entryName = $relative.Replace('\', '/')
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $zip,
            $file.FullName,
            $entryName,
            [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally {
    $zip.Dispose()
}

[pscustomobject]@{
    Source = $sourceFull
    Output = $outputFull
    Entry = $entries[0].Name
}
