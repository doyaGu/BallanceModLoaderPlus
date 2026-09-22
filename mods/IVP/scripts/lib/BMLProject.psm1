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

function Get-BMLProjectLayout {
    param(
        [string]$RepoRoot
    )

    if (-not $RepoRoot) {
        $RepoRoot = Get-BMLRepositoryRoot -AnchorPath $PSScriptRoot
    }
    $repo = [System.IO.Path]::GetFullPath($RepoRoot)
    $workspace = [System.IO.Path]::GetFullPath((Join-Path $repo '..\..'))
    if ((Test-Path -LiteralPath (Join-Path $workspace 'cmake\BMLMod.cmake') -PathType Leaf) -and
        (Test-Path -LiteralPath (Join-Path $workspace 'mods\IVP\CMakeLists.txt') -PathType Leaf)) {
        $buildRoot = Join-Path $workspace 'build-mods'
        $releaseBin = Join-Path $buildRoot 'mods\IVP\bin\RelWithDebInfo'
    } else {
        $buildRoot = [System.IO.Path]::GetFullPath((Join-Path $repo '..\build-ivp'))
        $releaseBin = Join-Path $buildRoot 'bin\RelWithDebInfo'
    }

    return [pscustomobject]@{
        RepoRoot = $repo
        BuildRoot = $buildRoot
        DefaultReleaseBin = $releaseBin
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

Export-ModuleMember -Function `
    Get-BMLRepositoryRoot, `
    Get-BMLProjectLayout, `
    Assert-BMLPath, `
    Get-BMLOptionalHash, `
    Get-BMLTextIfExists
