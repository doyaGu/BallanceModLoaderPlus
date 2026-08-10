[CmdletBinding()]
param(
    [string]$Source = '.',

    [string]$Output,

    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'lib\BMLProject.psm1') -Force

$sourceFull = [System.IO.Path]::GetFullPath($Source)
Assert-BMLPath -Path $sourceFull -Type Container

if ([string]::IsNullOrWhiteSpace($Output)) {
    $sourceName = Split-Path -Leaf ($sourceFull.TrimEnd('\', '/'))
    if ([string]::IsNullOrWhiteSpace($sourceName)) {
        throw 'Cannot derive a package name from the source directory. Pass -Output explicitly.'
    }
    $Output = Join-Path $sourceFull "dist\$sourceName.zip"
}
$outputFull = [System.IO.Path]::GetFullPath($Output)

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
    $excludedDirectories = @('.git', '.hg', '.svn', '.idea', '.vscode', '__pycache__')
    $excludedFiles = @('.DS_Store', '.gitattributes', '.gitignore', 'as.predefined', 'Thumbs.db')
    $excludedExtensions = @('.code-workspace', '.pyc', '.pyo')

    foreach ($file in $files) {
        if (-not $file.FullName.StartsWith($sourcePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to pack file outside source directory: $($file.FullName)"
        }

        $relative = $file.FullName.Substring($sourcePrefix.Length)
        $segments = $relative -split '[\\/]'
        if ($segments[0] -ieq 'dist' -or
            ($excludedDirectories | Where-Object { $segments -contains $_ }) -or
            $excludedFiles -contains $file.Name -or
            $excludedExtensions -contains $file.Extension) {
            continue
        }
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
