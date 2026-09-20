[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SourceRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-ContainsLiteral {
    param([string]$Text, [string]$Expected, [string]$Message)
    if (-not $Text.Contains($Expected, [System.StringComparison]::Ordinal)) {
        throw $Message
    }
}

$docsYaml = Get-Content -LiteralPath (Join-Path $SourceRoot '.github\workflows\docs.yml') -Raw
$docsYaml = $docsYaml.Replace("`r`n", "`n").Replace("`r", "`n")
Assert-ContainsLiteral `
    -Text $docsYaml `
    -Expected '! -name "updates"' `
    -Message 'Publishing MkDocs files must preserve the existing /updates directory.'
Assert-ContainsLiteral `
    -Text $docsYaml `
    -Expected 'Documentation output must not claim the reserved /updates namespace.' `
    -Message 'MkDocs output must not contain an /updates directory.'
Assert-ContainsLiteral `
    -Text $docsYaml `
    -Expected "permissions:`n  contents: read" `
    -Message 'The MkDocs build job must have read-only repository permissions.'
Assert-ContainsLiteral `
    -Text $docsYaml `
    -Expected "deploy:`n    if:" `
    -Message 'Only the gh-pages deployment job may request write permission.'

Write-Host 'MkDocs deployment preserves the three /updates files.'
