[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$contractSyncPath = Join-Path $PSScriptRoot 'sync_retail_contract.py'
$coveragePath = Join-Path $PSScriptRoot 'physics-rt-api-coverage.tsv'
$indirectCoveragePath = Join-Path $PSScriptRoot 'physics-rt-indirect-api-coverage.tsv'
$callsPath = Join-Path $repositoryRoot 'include\BML\IVP\detail\AddressEntries.inc'
$manifestPath = Join-Path $repositoryRoot 'src\IVP\generated\IvpSymbols.inc'

$python = Get-Command python -ErrorAction Stop
& $python.Source $contractSyncPath --check
if ($LASTEXITCODE -ne 0) {
    throw 'The IVP retail-contract generated views are stale.'
}

$rows = @(Import-Csv -LiteralPath $coveragePath -Delimiter "`t")
if ($rows.Count -ne 681) {
    throw "Expected 681 tracked retail IVP targets, found $($rows.Count)."
}

$duplicateRvas = @($rows | Group-Object rva | Where-Object Count -ne 1)
if ($duplicateRvas.Count -ne 0) {
    throw "Coverage list contains duplicate RVAs: $($duplicateRvas.Name -join ', ')"
}

$allowedDispositions = @('typed-wrapper', 'reconstructed-inline', 'adapter-internal')
$callsText = [IO.File]::ReadAllText($callsPath)
$manifestText = [IO.File]::ReadAllText($manifestPath)
$unnamedIdaRvas = @(
    '00009990', '00009C40', '0000BD00', '000129C0', '00014200',
    '00017790', '00017CF0', '000183C0', '00018710', '000197F0', '00019950',
    '00030180', '000109E0', '00010B70', '0000C200',
    '0002DC70', '0002DD00', '0002DDB0',
    '00028950', '00028960', '0002A210', '0002A2E0', '0002A380',
    '0002A430', '0002A4A0', '0002A4D0', '0002A500', '0002A540',
    '0002A5C0', '0002A6C0', '0002A760', '0002A790', '0002A7C0',
    '0002A800', '0002A880', '0002A920'
)

foreach ($row in $rows) {
    if ($row.rva -notmatch '^[0-9A-F]{8}$') {
        throw "Invalid RVA '$($row.rva)' for $($row.symbol)."
    }
    if ($allowedDispositions -notcontains $row.disposition) {
        throw "Invalid disposition '$($row.disposition)' at RVA $($row.rva)."
    }

    $trimmedRva = $row.rva.TrimStart('0')
    if ($trimmedRva.Length -eq 0) { $trimmedRva = '0' }
    if ($manifestText -notmatch "(?i)0x0*$trimmedRva" + 'u' -and
        $unnamedIdaRvas -notcontains $row.rva) {
        throw "RVA 0x$($row.rva) is absent from the checked-in IDA symbol manifest."
    }

    if ($row.disposition -eq 'adapter-internal') {
        if ($row.public_header -or $row.probe -or $row.address_id) {
            throw "Internal RVA 0x$($row.rva) must not claim a public wrapper."
        }
        continue
    }

    $headerPath = Join-Path $repositoryRoot ($row.public_header -replace '/', '\')
    if (-not (Test-Path -LiteralPath $headerPath -PathType Leaf)) {
        throw "Public header not found for RVA 0x$($row.rva): $headerPath"
    }
    $headerText = [IO.File]::ReadAllText($headerPath)
    if (-not $headerText.Contains($row.probe)) {
        throw "Probe '$($row.probe)' not found in $($row.public_header)."
    }

    if ($row.disposition -eq 'typed-wrapper') {
        if (-not $row.address_id) {
            throw "Typed wrapper at RVA 0x$($row.rva) has no Address id."
        }
        $escapedAddress = [regex]::Escape($row.address_id)
        if ($callsText -notmatch "(?m)^\s*$escapedAddress\s*=\s*0x$($row.rva)u,") {
            throw "Address::$($row.address_id) does not map to RVA 0x$($row.rva)."
        }
        if ($headerText -notmatch "Address::$escapedAddress\b") {
            throw "Address::$($row.address_id) is not used by $($row.public_header)."
        }
    } elseif ($row.address_id) {
        throw "Reconstructed inline API at RVA 0x$($row.rva) must not claim a DLL call."
    }
}

$counts = @{}
foreach ($group in ($rows | Group-Object disposition)) {
    $counts[$group.Name] = $group.Count
}
if ($counts['typed-wrapper'] -ne 680 -or
    $counts.ContainsKey('reconstructed-inline') -or
    $counts['adapter-internal'] -ne 1) {
    throw ('Unexpected disposition totals: typed={0}, inline={1}, internal={2}' -f
           $counts['typed-wrapper'], 0,
           $counts['adapter-internal'])
}

$addressRvas = @{}
foreach ($match in [regex]::Matches(
             $callsText,
             '(?m)^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*0x([0-9A-Fa-f]+)u,')) {
    $addressRvas[$match.Groups[1].Value] =
        $match.Groups[2].Value.PadLeft(8, '0').ToUpperInvariant()
}
$publicAddressIds = [Collections.Generic.HashSet[string]]::new(
    [StringComparer]::Ordinal)
foreach ($header in Get-ChildItem (Join-Path $repositoryRoot 'include\BML\IVP') `
                                  -Recurse -Filter '*.h') {
    foreach ($match in [regex]::Matches(
                 [IO.File]::ReadAllText($header.FullName),
                 '\bAddress::([A-Za-z_][A-Za-z0-9_]*)\b')) {
        [void]$publicAddressIds.Add($match.Groups[1].Value)
    }
}
$publicRvas = [Collections.Generic.HashSet[string]]::new(
    [StringComparer]::Ordinal)
foreach ($addressId in $publicAddressIds) {
    if (-not $addressRvas.ContainsKey($addressId)) {
        throw "Public Address id is absent from AddressEntries.inc: $addressId"
    }
    [void]$publicRvas.Add($addressRvas[$addressId])
}
$typedRvas = [Collections.Generic.HashSet[string]]::new(
    [StringComparer]::Ordinal)
foreach ($row in $rows | Where-Object disposition -eq 'typed-wrapper') {
    [void]$typedRvas.Add($row.rva)
}
$missingPublicRvas = @($publicRvas | Where-Object { -not $typedRvas.Contains($_) })
$staleTypedRvas = @($typedRvas | Where-Object { -not $publicRvas.Contains($_) })
if ($missingPublicRvas.Count -ne 0 -or $staleTypedRvas.Count -ne 0) {
    throw ('Public Address reverse closure differs: missing={0}; stale={1}' -f
           ($missingPublicRvas -join ', '), ($staleTypedRvas -join ', '))
}

$indirectRows = @(Import-Csv -LiteralPath $indirectCoveragePath -Delimiter "`t")
if ($indirectRows.Count -ne 14) {
    throw "Expected 14 indirect IVP call sites, found $($indirectRows.Count)."
}
$duplicateCallsites = @(
    $indirectRows | Group-Object callsite_rva | Where-Object Count -ne 1
)
if ($duplicateCallsites.Count -ne 0) {
    throw ('Indirect coverage contains duplicate call sites: {0}' -f
           ($duplicateCallsites.Name -join ', '))
}
foreach ($row in $indirectRows) {
    if ($row.callsite_rva -notmatch '^[0-9A-F]{8}$') {
        throw "Invalid indirect call-site RVA '$($row.callsite_rva)'."
    }
    $headerPath = Join-Path $repositoryRoot ($row.public_header -replace '/', '\')
    if (-not (Test-Path -LiteralPath $headerPath -PathType Leaf)) {
        throw "Indirect-call public header not found: $headerPath"
    }
    if (-not [IO.File]::ReadAllText($headerPath).Contains($row.probe)) {
        throw "Indirect-call probe '$($row.probe)' not found in $($row.public_header)."
    }
}

[pscustomobject]@{
    DependencyTargets = $rows.Count
    PublicAddressIds = $publicAddressIds.Count
    PublicAddressRvas = $publicRvas.Count
    IndirectCallsites = $indirectRows.Count
    TypedWrappers = $counts['typed-wrapper']
    ReconstructedInline = 0
    AdapterInternal = $counts['adapter-internal']
    Pending = 0
    Complete = $true
}
