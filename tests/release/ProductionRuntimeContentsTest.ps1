[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RuntimeDll
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot '..\..\scripts\lib\BMLReleaseFiles.psm1') -Force

Assert-BMLProductionRuntime -Path $RuntimeDll
Write-Output 'Production runtime contains no private test interfaces.'
