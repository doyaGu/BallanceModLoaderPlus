[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Id,

    [Parameter(Mandatory = $true)]
    [string]$Name,

    [Parameter(Mandatory = $true)]
    [string]$Author,

    [ValidatePattern('^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$')]
    [string]$Version = '1.0.0',

    [string]$Description,

    [string]$Destination
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'lib\BMLProject.psm1') -Force

function ConvertTo-AngelScriptString {
    param([string]$Value)

    return $Value.Replace('\', '\\').Replace('"', '\"').Replace("`r", '\r').Replace("`n", '\n').Replace("`t", '\t')
}

function ConvertTo-ScriptClassName {
    param([string]$ModId)

    $leaf = ($ModId -split '\.')[-1]
    $className = ''
    foreach ($part in @($leaf -split '[^A-Za-z0-9]+' | Where-Object { $_ })) {
        $className += $part.Substring(0, 1).ToUpperInvariant()
        if ($part.Length -gt 1) {
            $className += $part.Substring(1)
        }
    }

    if ($className -notmatch '^[A-Za-z_]') {
        $className = "Mod$className"
    }
    if (-not $className.EndsWith('Mod', [System.StringComparison]::Ordinal)) {
        $className += 'Mod'
    }
    return $className
}

if ($Id -cnotmatch '^[a-z][a-z0-9_-]*(?:\.[a-z0-9][a-z0-9_-]*)+$') {
    throw "Id must be a lowercase owner-prefixed value such as 'yourname.my-mod'."
}
if ([string]::IsNullOrWhiteSpace($Name)) {
    throw 'Name must not be empty.'
}
if ($Name -match '[\r\n]') {
    throw 'Name must fit on one line.'
}
if ([string]::IsNullOrWhiteSpace($Author)) {
    throw 'Author must not be empty.'
}
if ([string]::IsNullOrWhiteSpace($Description)) {
    $Description = "$Name script mod"
}

$className = ConvertTo-ScriptClassName -ModId $Id
if ([string]::IsNullOrWhiteSpace($Destination)) {
    $Destination = Join-Path (Get-Location) $className
}

$templateRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\templates\script-mod-template'))
$destinationFull = [System.IO.Path]::GetFullPath($Destination)
$templatePrefix = $templateRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
if ($destinationFull.StartsWith($templatePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Destination must not be inside the SDK template directory.'
}
if (Test-Path -LiteralPath $destinationFull) {
    throw "Destination already exists: $destinationFull"
}

Assert-BMLPath -Path (Join-Path $templateRoot 'HelloScript.mod.as') -Type Leaf
Assert-BMLPath -Path (Join-Path $templateRoot 'README.md') -Type Leaf
Copy-BMLDirectoryContents -SourceDir $templateRoot -DestinationDir $destinationFull

$sourcePath = Join-Path $destinationFull 'HelloScript.mod.as'
$entryPath = Join-Path $destinationFull "$className.mod.as"
$readmePath = Join-Path $destinationFull 'README.md'

$escapedId = ConvertTo-AngelScriptString $Id
$escapedName = ConvertTo-AngelScriptString $Name
$escapedAuthor = ConvertTo-AngelScriptString $Author
$escapedVersion = ConvertTo-AngelScriptString $Version
$escapedDescription = ConvertTo-AngelScriptString $Description

$source = Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8
$source = $source.Replace('example.hello.script', $escapedId)
$source = $source.Replace('Hello Script', $escapedName)
$source = $source.Replace('Your Name', $escapedAuthor)
$source = $source.Replace('1.0.0', $escapedVersion)
$source = $source.Replace('Minimal BML+ script mod', $escapedDescription)
$source = $source.Replace('HelloScript', $className)

$readme = Get-Content -LiteralPath $readmePath -Raw -Encoding UTF8
$readme = $readme.Replace('# BML+ Script Mod Template', "# $Name")
$readme = $readme.Replace('example.hello.script', $Id)
$readme = $readme.Replace('Hello Script', $Name)
$readme = $readme.Replace('HelloScript', $className)

$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($sourcePath, $source, $utf8NoBom)
[System.IO.File]::WriteAllText($readmePath, $readme, $utf8NoBom)
Move-Item -LiteralPath $sourcePath -Destination $entryPath

[pscustomobject]@{
    Id = $Id
    Name = $Name
    ClassName = $className
    Destination = $destinationFull
    Entry = [System.IO.Path]::GetFileName($entryPath)
}
