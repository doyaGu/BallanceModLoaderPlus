param(
    [Parameter(Mandatory = $true)]
    [string]$IdaSymbolsPath,

    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $PSScriptRoot '..\..\src\IVP\generated\IvpSymbols.inc'
}

$inputFull = [IO.Path]::GetFullPath($IdaSymbolsPath)
$outputFull = [IO.Path]::GetFullPath($OutputPath)
if (-not (Test-Path -LiteralPath $inputFull -PathType Leaf)) {
    throw "IDA symbol export not found: $inputFull"
}

$records = [Collections.Generic.List[object]]::new()
foreach ($row in Import-Csv -LiteralPath $inputFull -Delimiter "`t" -Header Name, Rva) {
    if ([string]::IsNullOrWhiteSpace($row.Name) -or
        $row.Rva -notmatch '^0x[0-9A-Fa-f]+$') {
        throw "Invalid IDA symbol row: $($row.Name) $($row.Rva)"
    }

    $rva = [Convert]::ToUInt32($row.Rva.Substring(2), 16)
    $flags = 0x00000001 -bor 0x00000002 # function | IDA-named
    if ($row.Name.StartsWith('?')) {
        $flags = $flags -bor 0x00000004 # decorated
    } else {
        $flags = $flags -bor 0x00000010 # manually readable IDA name
    }
    if ($row.Name -match 'IVP_' -or
        $row.Name -match '(^|\?)(ivp|p|qh)_') {
        $flags = $flags -bor 0x00000008 # IVP engine or bundled utility
    }

    $records.Add([pscustomobject]@{
        Name = [string]$row.Name
        Rva = $rva
        Flags = [uint32]$flags
    })
}

$records.Sort([Comparison[object]]{
    param($left, $right)
    return [StringComparer]::Ordinal.Compare($left.Name, $right.Name)
})

for ($index = 1; $index -lt $records.Count; ++$index) {
    if ([StringComparer]::Ordinal.Equals($records[$index - 1].Name,
                                         $records[$index].Name)) {
        throw "Duplicate IDA name: $($records[$index].Name)"
    }
}

$lines = [Collections.Generic.List[string]]::new()
$lines.Add('// Generated only from names and RVAs in the original DLL IDA database.')
$lines.Add("// Input: IDA name/RVA export ($([IO.Path]::GetFileName($inputFull)))")
foreach ($record in $records) {
    $escaped = $record.Name.Replace('\', '\\').Replace('"', '\"')
    $lines.Add(('{{"{0}", 0x{1:X}u, 0x{2:X8}u}},' -f
                $escaped, $record.Rva, $record.Flags))
}

$directory = Split-Path -Parent $outputFull
New-Item -ItemType Directory -Path $directory -Force | Out-Null
[IO.File]::WriteAllLines($outputFull, $lines, [Text.UTF8Encoding]::new($false))
Write-Output "Generated $($records.Count) original-DLL IDA symbols at $outputFull"
