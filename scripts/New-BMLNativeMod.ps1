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

function ConvertTo-CppString {
    param([string]$Value)

    return $Value.Replace('\', '\\').Replace('"', '\"').Replace("`r", '\r').Replace("`n", '\n').Replace("`t", '\t')
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
    $Description = "$Name native mod"
}

$className = ConvertTo-BMLModClassName -ModId $Id
if ([string]::IsNullOrWhiteSpace($Destination)) {
    $Destination = Join-Path (Get-Location) $className
}

$templateRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\templates\native-mod-template'))
$destinationFull = [System.IO.Path]::GetFullPath($Destination)
$templatePrefix = $templateRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
if ($destinationFull.StartsWith($templatePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Destination must not be inside the SDK template directory.'
}
if (Test-Path -LiteralPath $destinationFull) {
    throw "Destination already exists: $destinationFull"
}

Assert-BMLPath -Path (Join-Path $templateRoot 'CMakeLists.txt') -Type Leaf
Assert-BMLPath -Path (Join-Path $templateRoot 'src\HelloMod.cpp') -Type Leaf
Assert-BMLPath -Path (Join-Path $templateRoot 'README.md') -Type Leaf
Copy-BMLDirectoryContents -SourceDir $templateRoot -DestinationDir $destinationFull

$cmakePath = Join-Path $destinationFull 'CMakeLists.txt'
$sourcePath = Join-Path $destinationFull 'src\HelloMod.cpp'
$renamedSourcePath = Join-Path $destinationFull "src\$className.cpp"
$readmePath = Join-Path $destinationFull 'README.md'

$escapedId = ConvertTo-CppString $Id
$escapedName = ConvertTo-CppString $Name
$escapedAuthor = ConvertTo-CppString $Author
$escapedVersion = ConvertTo-CppString $Version
$escapedDescription = ConvertTo-CppString $Description
$cmakeVersion = ($Version -split '[-+]')[0]

$cmake = Get-Content -LiteralPath $cmakePath -Raw -Encoding UTF8
$cmake = $cmake.Replace('HelloMod', $className)
$cmake = $cmake.Replace('1.0.0', $cmakeVersion)

$source = Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8
$source = $source.Replace('HelloMod', $className)
$source = $source.Replace(('return "{0}";' -f $className), ('return "{0}";' -f $escapedId))
$source = $source.Replace('"Hello Mod"', ('"{0}"' -f $escapedName))
$source = $source.Replace('"Template"', ('"{0}"' -f $escapedAuthor))
$source = $source.Replace('"1.0.0"', ('"{0}"' -f $escapedVersion))
$source = $source.Replace('"Minimal example mod for BML+"', ('"{0}"' -f $escapedDescription))

$readme = Get-Content -LiteralPath $readmePath -Raw -Encoding UTF8
$readme = $readme.Replace('# BML+ Native Mod Template', "# $Name")
$readme = $readme.Replace('HelloMod', $className)

$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($cmakePath, $cmake, $utf8NoBom)
[System.IO.File]::WriteAllText($sourcePath, $source, $utf8NoBom)
[System.IO.File]::WriteAllText($readmePath, $readme, $utf8NoBom)
Move-Item -LiteralPath $sourcePath -Destination $renamedSourcePath

[pscustomobject]@{
    Id = $Id
    Name = $Name
    ClassName = $className
    Destination = $destinationFull
    Source = "src/$className.cpp"
}
