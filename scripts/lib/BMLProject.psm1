Set-StrictMode -Version Latest

function Get-BMLRepositoryRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$AnchorPath
    )

    $full = [System.IO.Path]::GetFullPath($AnchorPath)
    $directory = if (Test-Path -LiteralPath $full -PathType Leaf) {
        Split-Path -Parent $full
    } else {
        $full
    }

    while ($directory) {
        if ((Test-Path -LiteralPath (Join-Path $directory 'CMakeLists.txt')) -and
            (Test-Path -LiteralPath (Join-Path $directory 'src')) -and
            (Test-Path -LiteralPath (Join-Path $directory 'include'))) {
            return $directory
        }

        $parent = Split-Path -Parent $directory
        if ($parent -eq $directory) {
            break
        }
        $directory = $parent
    }

    throw "Could not locate repository root from: $AnchorPath"
}

function Resolve-BMLPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BasePath,

        [Parameter(Mandatory = $true)]
        [string]$RelativePath
    )

    return [System.IO.Path]::GetFullPath((Join-Path $BasePath $RelativePath))
}

function Get-BMLProjectLayout {
    param(
        [string]$RepoRoot
    )

    if (-not $RepoRoot) {
        $RepoRoot = Get-BMLRepositoryRoot -AnchorPath $PSScriptRoot
    }
    $repo = [System.IO.Path]::GetFullPath($RepoRoot)

    return [pscustomobject]@{
        RepoRoot = $repo
        ScriptsRoot = Join-Path $repo 'scripts'
        ToolsRoot = Join-Path $repo 'tools'
        DocsRoot = Join-Path $repo 'docs'
        TemplatesRoot = Join-Path $repo 'templates'
        RuntimeSourceRoot = Join-Path $repo 'packaging\runtime'
        DefaultReleaseBin = Join-Path $repo 'cmake-build-release\bin'
    }
}

function Assert-BMLPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [ValidateSet('Any', 'Container', 'Leaf')]
        [string]$Type = 'Any'
    )

    $pathType = switch ($Type) {
        'Container' { 'Container' }
        'Leaf' { 'Leaf' }
        default { $null }
    }

    $exists = if ($pathType) {
        Test-Path -LiteralPath $Path -PathType $pathType
    } else {
        Test-Path -LiteralPath $Path
    }

    if (-not $exists) {
        throw "Required path is missing: $Path"
    }
}

function New-BMLCleanDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Path | Out-Null
}

function Copy-BMLDirectoryContents {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceDir,

        [Parameter(Mandatory = $true)]
        [string]$DestinationDir
    )

    Assert-BMLPath -Path $SourceDir -Type Container
    New-Item -ItemType Directory -Path $DestinationDir -Force | Out-Null
    Copy-Item -Path (Join-Path $SourceDir '*') -Destination $DestinationDir -Recurse
}

function Copy-BMLDirectoryFresh {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceDir,

        [Parameter(Mandatory = $true)]
        [string]$DestinationDir
    )

    Assert-BMLPath -Path $SourceDir -Type Container
    if (Test-Path -LiteralPath $DestinationDir) {
        Remove-Item -LiteralPath $DestinationDir -Recurse -Force
    }
    Copy-Item -LiteralPath $SourceDir -Destination $DestinationDir -Recurse
}

function New-BMLZipFromDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceDir,

        [Parameter(Mandatory = $true)]
        [string]$ZipPath
    )

    Assert-BMLPath -Path $SourceDir -Type Container
    if (Test-Path -LiteralPath $ZipPath) {
        Remove-Item -LiteralPath $ZipPath -Force
    }

    $items = Get-ChildItem -LiteralPath $SourceDir -Force
    if (-not $items) {
        throw "Cannot package empty directory: $SourceDir"
    }

    Compress-Archive -Path (Join-Path $SourceDir '*') -DestinationPath $ZipPath -CompressionLevel Optimal
}

function Get-BMLOptionalHash {
    param([string]$Path)

    if (-not $Path -or -not (Test-Path -LiteralPath $Path)) {
        return $null
    }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

function Get-BMLTextIfExists {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return ''
    }
    return Get-Content -LiteralPath $Path -Raw -ErrorAction SilentlyContinue
}

function Get-BMLLogTail {
    param(
        [string]$Path,
        [int]$Lines = 80
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return ''
    }
    return (Get-Content -LiteralPath $Path -Tail $Lines -ErrorAction SilentlyContinue) -join "`n"
}

Export-ModuleMember -Function `
    Get-BMLRepositoryRoot, `
    Resolve-BMLPath, `
    Get-BMLProjectLayout, `
    Assert-BMLPath, `
    New-BMLCleanDirectory, `
    Copy-BMLDirectoryContents, `
    Copy-BMLDirectoryFresh, `
    New-BMLZipFromDirectory, `
    Get-BMLOptionalHash, `
    Get-BMLTextIfExists, `
    Get-BMLLogTail
