[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^v\d+\.\d+\.\d+$')]
    [string]$Version,

    [Parameter(Mandatory = $true)]
    [string]$ReleaseDir,

    [Parameter(Mandatory = $true)]
    [string]$GhPagesCheckout,

    [string]$ExpectedBaseRef,

    [ValidatePattern('^[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+$')]
    [string]$Repository = 'doyaGu/BallanceModLoaderPlus'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'lib\BMLReleaseFiles.psm1') -Force

$release = [System.IO.Path]::GetFullPath($ReleaseDir)
$pages = [System.IO.Path]::GetFullPath($GhPagesCheckout)
Assert-BMLReleaseFiles -Directory $release -Version $Version -SignatureState Signed
Assert-BMLUpdaterManifestMatchesPackage -Directory $release -Version $Version
Assert-BMLStableJson -Directory $release -Version $Version -Repository $Repository

$gitDir = & git -C $pages rev-parse --git-dir 2>$null
if ($LASTEXITCODE -ne 0 -or -not $gitDir) {
    throw "GhPagesCheckout is not a Git checkout: $pages"
}
$head = (& git -C $pages rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or -not $head) {
    throw 'Unable to resolve the gh-pages checkout HEAD.'
}
if ($ExpectedBaseRef) {
    $expectedHead = (& git -C $pages rev-parse $ExpectedBaseRef).Trim()
    if ($LASTEXITCODE -ne 0 -or -not $expectedHead) {
        throw "Unable to resolve expected gh-pages base ref: $ExpectedBaseRef"
    }
    if ($head -cne $expectedHead) {
        throw "GhPagesCheckout HEAD $head does not match ${ExpectedBaseRef} at $expectedHead."
    }
} else {
    $branch = (& git -C $pages branch --show-current).Trim()
    if ($LASTEXITCODE -ne 0 -or $branch -cne 'gh-pages') {
        throw "GhPagesCheckout must be on branch gh-pages when ExpectedBaseRef is omitted, got '$branch'."
    }
}
$before = @(& git -C $pages status --short)
if ($LASTEXITCODE -ne 0) {
    throw 'Unable to inspect the gh-pages checkout.'
}
if ($before.Count -ne 0) {
    throw 'GhPagesCheckout must be clean before copying stable.json and stable.json.sig.'
}

$updates = Join-Path $pages 'updates'
New-Item -ItemType Directory -Path $updates -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $release 'stable.json') -Destination (Join-Path $updates 'stable.json') -Force
Copy-Item -LiteralPath (Join-Path $release 'stable.json.sig') -Destination (Join-Path $updates 'stable.json.sig') -Force

$indexPath = Join-Path $updates 'index.html'
if (-not (Test-Path -LiteralPath $indexPath -PathType Leaf)) {
    $index = '<!doctype html><meta charset="utf-8"><title>BML+ updater channel</title><h1>BML+ updater channel</h1><ul><li><a href="stable.json">stable.json</a></li><li><a href="stable.json.sig">stable.json.sig</a></li></ul>' + "`n"
    Write-BMLUtf8NoBomText -Path $indexPath -Text $index
}

$allowed = @(
    'updates/index.html',
    'updates/stable.json',
    'updates/stable.json.sig'
)
$after = @(& git -C $pages -c status.showUntrackedFiles=all status --short)
if ($LASTEXITCODE -ne 0) {
    throw 'Unable to inspect the copied updater files.'
}
foreach ($line in $after) {
    if ($line.Length -lt 4) {
        throw "Unexpected git status entry: $line"
    }
    $path = $line.Substring(3).Replace('\', '/')
    if ($path -notin $allowed) {
        throw "Copying updater files changed an unrelated path: $path"
    }
}

Write-Host "Copied stable.json and stable.json.sig to $updates"
Write-Host 'Review the diff, then commit and push gh-pages without --force.'
