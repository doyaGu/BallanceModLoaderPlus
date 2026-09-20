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

$buildYaml = Get-Content -LiteralPath (Join-Path $SourceRoot '.github\workflows\build.yml') -Raw
$unpinnedActions = @(
    Select-String `
        -Path (Join-Path $SourceRoot '.github\workflows\*.yml') `
        -Pattern '^\s*uses:\s+[^\s#]+@v\d' `
)
if ($unpinnedActions.Count -ne 0) {
    throw "GitHub Actions must use commit SHAs: $($unpinnedActions.Line -join ', ')"
}
Assert-ContainsLiteral `
    -Text $buildYaml `
    -Expected 'VIRTOOLS_SDK_REF: 175bcb12d829c687359b90615be53524cdba0132' `
    -Message 'The Virtools SDK checkout must use the reviewed commit SHA.'
Assert-ContainsLiteral `
    -Text $buildYaml `
    -Expected 'CKANGELSCRIPT_RELEASE_SHA256:' `
    -Message 'The CKAngelScript ZIP download must be checked against a fixed SHA-256 digest.'

Write-Host 'GitHub Actions, Virtools SDK, and CKAngelScript versions are fixed.'
