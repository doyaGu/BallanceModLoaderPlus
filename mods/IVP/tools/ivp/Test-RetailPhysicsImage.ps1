[CmdletBinding()]
param(
    [string]$BallanceRoot = $env:BML_BALLANCE_ROOT
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $BallanceRoot) {
    throw 'Pass -BallanceRoot or set BML_BALLANCE_ROOT.'
}

$contractPath = Join-Path $PSScriptRoot 'retail-contract.json'
$contract = Get-Content -Raw -LiteralPath $contractPath | ConvertFrom-Json
$imageContract = $contract.image

$dllPath = Join-Path ([IO.Path]::GetFullPath($BallanceRoot)) $imageContract.relative_path
if (-not (Test-Path -LiteralPath $dllPath -PathType Leaf)) {
    throw "Original physics_RT.dll not found: $dllPath"
}

$expectedSha256 = ([string]$imageContract.sha256).ToUpperInvariant()
$expectedTimestamp = [Convert]::ToUInt32([string]$imageContract.timestamp, 16)
$expectedImageSize = [Convert]::ToUInt32([string]$imageContract.image_size, 16)
$expectedMachine = [Convert]::ToUInt16([string]$imageContract.machine, 16)
$expectedOptionalMagic = [Convert]::ToUInt16([string]$imageContract.optional_magic, 16)
$image = [IO.File]::ReadAllBytes($dllPath)

function Read-U16([int]$Offset) {
    return [BitConverter]::ToUInt16($image, $Offset)
}

function Read-U32([int]$Offset) {
    return [BitConverter]::ToUInt32($image, $Offset)
}

$peOffset = [int](Read-U32 0x3C)
if ((Read-U32 $peOffset) -ne 0x00004550) {
    throw 'physics_RT.dll has no valid PE signature.'
}

$machine = Read-U16 ($peOffset + 4)
$sectionCount = Read-U16 ($peOffset + 6)
$timestamp = Read-U32 ($peOffset + 8)
$optionalSize = Read-U16 ($peOffset + 20)
$optionalOffset = $peOffset + 24
$optionalMagic = Read-U16 $optionalOffset
$imageSize = Read-U32 ($optionalOffset + 56)

if ($machine -ne $expectedMachine -or $optionalMagic -ne $expectedOptionalMagic) {
    throw 'physics_RT.dll is not the expected 32-bit x86 PE image.'
}
if ($timestamp -ne $expectedTimestamp -or $imageSize -ne $expectedImageSize) {
    throw ('PE identity mismatch: timestamp=0x{0:X8}, image-size=0x{1:X8}' -f
           $timestamp, $imageSize)
}

$sha256 = (Get-FileHash -LiteralPath $dllPath -Algorithm SHA256).Hash.ToUpperInvariant()
if ($sha256 -ne $expectedSha256) {
    throw "SHA-256 mismatch: $sha256"
}

$sections = @()
$sectionOffset = $optionalOffset + $optionalSize
for ($index = 0; $index -lt $sectionCount; ++$index) {
    $entry = $sectionOffset + 40 * $index
    $sections += [pscustomobject]@{
        VirtualSize = Read-U32 ($entry + 8)
        VirtualAddress = Read-U32 ($entry + 12)
        RawSize = Read-U32 ($entry + 16)
        RawOffset = Read-U32 ($entry + 20)
    }
}

function Convert-RvaToOffset([uint32]$Rva) {
    foreach ($section in $sections) {
        $span = [Math]::Max([uint32]$section.VirtualSize, [uint32]$section.RawSize)
        if ($Rva -ge $section.VirtualAddress -and
            ($Rva - $section.VirtualAddress) -lt $span) {
            return [int]($section.RawOffset + $Rva - $section.VirtualAddress)
        }
    }
    throw ('RVA 0x{0:X8} is outside all PE sections.' -f $Rva)
}

$anchors = @($imageContract.instruction_anchors | ForEach-Object {
    $anchorBytes = @([string]$_.bytes -split '\s+' | ForEach-Object {
        [Convert]::ToByte($_, 16)
    })
    @{
        Rva = [Convert]::ToUInt32([string]$_.rva, 16)
        Bytes = [byte[]]$anchorBytes
    }
})

foreach ($anchor in $anchors) {
    $offset = Convert-RvaToOffset $anchor.Rva
    for ($index = 0; $index -lt $anchor.Bytes.Length; ++$index) {
        if ($image[$offset + $index] -ne $anchor.Bytes[$index]) {
            throw ('Instruction anchor mismatch at RVA 0x{0:X8}+0x{1:X}' -f
                   $anchor.Rva, $index)
        }
    }
}

[pscustomobject]@{
    Path = $dllPath
    Architecture = 'x86'
    FileSize = $image.Length
    ImageSize = ('0x{0:X8}' -f $imageSize)
    Timestamp = ('0x{0:X8}' -f $timestamp)
    Sha256 = $sha256
    InstructionAnchors = $anchors.Count
    Compatible = $true
}
