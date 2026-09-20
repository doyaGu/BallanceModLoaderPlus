[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SourceRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-ContainsLiteral {
    param(
        [string]$Text,
        [string]$Expected,
        [string]$Message
    )

    if (-not $Text.Contains($Expected, [System.StringComparison]::Ordinal)) {
        throw $Message
    }
}

$workflowPath = Join-Path $SourceRoot '.github\workflows\build.yml'
$workflow = Get-Content -LiteralPath $workflowPath -Raw

Assert-ContainsLiteral `
    -Text $workflow `
    -Expected '${{ github.workspace }}\packages\*.manifest.json' `
    -Message 'Release CI must preserve the unsigned updater manifest next to the tested ZIP files.'

Assert-ContainsLiteral `
    -Text $workflow `
    -Expected 'BMLPlus-${{ github.ref_name }}-Unsigned-Release-Files' `
    -Message 'Tag CI must retain four ZIP files plus the unsigned updater manifest under a predictable name.'

if ($workflow.Contains('draft-release:', [System.StringComparison]::Ordinal) -or
    $workflow.Contains('gh release create', [System.StringComparison]::Ordinal) -or
    $workflow.Contains('${{ github.token }}', [System.StringComparison]::Ordinal)) {
    throw 'GitHub Actions must not create or publish the GitHub release.'
}

$runbook = Get-Content -LiteralPath (Join-Path $SourceRoot 'RELEASING.md') -Raw
Assert-ContainsLiteral `
    -Text $runbook `
    -Expected "Refusing to create a release with bot account" `
    -Message 'The maintainer runbook must reject bot-authenticated GitHub CLI sessions.'
Assert-ContainsLiteral `
    -Text $runbook `
    -Expected 'gh release create $version @releaseFiles' `
    -Message 'The maintainer runbook must create the GitHub release after local signing.'

foreach ($relative in @(
    'RELEASING.md',
    'scripts\Sign-BMLReleaseFiles.ps1',
    'scripts\Copy-BMLUpdaterFilesToPages.ps1'
)) {
    if (-not (Test-Path -LiteralPath (Join-Path $SourceRoot $relative) -PathType Leaf)) {
        throw "Required release file is missing: $relative"
    }
}
Write-Host 'Tagged-build release file checks passed.'
