[CmdletBinding()]
param(
    [string]$Database = "build-dev/physics_RT-analysis.i64",
    [string]$RetailDll,
    [string]$Manifest = "build-dev/physics_RT-symbols.tsv",
    [string]$ReferenceRoot = $env:IVP_REFERENCE_ROOT
)

$ErrorActionPreference = "Stop"
if (-not $RetailDll -and $env:BML_BALLANCE_ROOT) {
    $RetailDll = Join-Path $env:BML_BALLANCE_ROOT 'BuildingBlocks/physics_RT.dll'
}
if (-not $RetailDll) {
    throw 'Pass -RetailDll or set BML_BALLANCE_ROOT.'
}
if (-not $ReferenceRoot -or -not [System.IO.Path]::IsPathFullyQualified($ReferenceRoot)) {
    throw 'Pass an absolute -ReferenceRoot or set IVP_REFERENCE_ROOT to an absolute path.'
}
$expectedDllHash = "E72E4AFCFA5C33A7D3D27776137F8C997B3C52D89D8A8A4745F1CA21E45893EC"
$workspace = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "../..")).Path
$buildRoot = (Resolve-Path -LiteralPath (Join-Path $workspace "build-dev")).Path
$databasePath = (Resolve-Path -LiteralPath (Join-Path $workspace $Database)).Path
$retailDllPath = (Resolve-Path -LiteralPath $RetailDll).Path
$manifestPath = [System.IO.Path]::GetFullPath((Join-Path $workspace $Manifest))
$runnerPath = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "Run-IvpIdbScript.py")).Path
$applyPath = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "Apply-BallanceIvpIdbCorrections.py")).Path
$publicAuditPath = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "Audit-IvpPublicInterface.py")).Path
$referenceRoot = (Resolve-Path -LiteralPath $ReferenceRoot).Path
$publicEvidencePath = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "public-api-evidence.tsv")).Path

if ([System.IO.Path]::GetDirectoryName($databasePath) -ne $buildRoot) {
    throw "Database must be a direct child of build-dev: $databasePath"
}
if ([System.IO.Path]::GetDirectoryName($manifestPath) -ne $buildRoot) {
    throw "Manifest must be a direct child of build-dev: $manifestPath"
}
$actualDllHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $retailDllPath).Hash
if ($actualDllHash -ne $expectedDllHash) {
    throw "Unexpected retail physics_RT.dll SHA-256: $actualDllHash"
}
if ((Get-Process ida, idat -ErrorAction SilentlyContinue | Measure-Object).Count -ne 0) {
    throw "Close IDA before updating the canonical database"
}

$transactionPath = Join-Path $buildRoot (".physics_RT-analysis.transaction-{0}.i64" -f $PID)
$transactionManifestPath = Join-Path $buildRoot (".physics_RT-symbols.transaction-{0}.tsv" -f $PID)
$transactionMethodsPath = Join-Path $buildRoot (".ivp-methods.transaction-{0}.tsv" -f $PID)
if (Test-Path -LiteralPath $transactionPath) {
    throw "Transaction path already exists: $transactionPath"
}
if (Test-Path -LiteralPath $transactionManifestPath) {
    throw "Transaction manifest path already exists: $transactionManifestPath"
}
if (Test-Path -LiteralPath $transactionMethodsPath) {
    throw "Transaction methods path already exists: $transactionMethodsPath"
}

function Invoke-IdaScript {
    param(
        [Parameter(Mandatory = $true)][string]$Script,
        [string[]]$Expected = @(),
        [string[]]$ScriptArgument = @(),
        [switch]$ExportManifest,
        [switch]$AutoAnalysis
    )

    $arguments = @(
        "-u", $runnerPath,
        "--database", $transactionPath,
        "--script", $Script
    )
    if ($ExportManifest) {
        $arguments += @("--export", $transactionManifestPath)
    }
    if ($AutoAnalysis) {
        $arguments += "--auto-analysis"
    }
    foreach ($argument in $ScriptArgument) {
        $arguments += @("--script-argument", $argument)
    }

    $output = @()
    $exitCode = 0
    $joined = ""
    $maximumAttempts = 4
    for ($attempt = 1; $attempt -le $maximumAttempts; ++$attempt) {
        $output = @(& python @arguments 2>&1)
        $exitCode = $LASTEXITCODE
        $joined = $output -join "`n"
        if ($exitCode -eq 0) {
            break
        }

        # IDA 9.4 occasionally leaves a just-closed compressed database locked
        # for the next short-lived idapro process (OPEN_RC 4). It has also once
        # opened a fresh transaction copy with a transient zero image base. The
        # correction script checks the image base before making any edit, so
        # these two exact failures are safe to retry; every other guard refusal
        # remains fatal on the first attempt.
        $retryableOpenFailure =
            $exitCode -eq 3 -and $joined -match '(?m)^OPEN_RC\s+[1-9][0-9]*\b'
        $retryableUninitializedImage =
            $exitCode -eq 2 -and
            $joined -match '(?m)^REFUSED\s+unexpected image base\s*$'
        if ((-not $retryableOpenFailure -and
             -not $retryableUninitializedImage) -or
            $attempt -eq $maximumAttempts) {
            break
        }

        $delayMilliseconds = 250 * $attempt * $attempt
        Write-Host ("IDA_RETRY`t{0}`t{1}`t{2}ms" -f
            $attempt, [System.IO.Path]::GetFileName($Script),
            $delayMilliseconds)
        Start-Sleep -Milliseconds $delayMilliseconds
    }
    if ($exitCode -ne 0) {
        $output | ForEach-Object { Write-Host $_ }
        throw "IDA script failed with exit code ${exitCode}: $Script"
    }
    foreach ($pattern in $Expected) {
        if ($joined -notmatch $pattern) {
            $output | ForEach-Object { Write-Host $_ }
            throw "IDA audit did not emit expected result '$pattern': $Script"
        }
    }
    $output |
        Where-Object { "$_" -match "^(OPEN_RC|SAVED|EXPORTED|NAMED_IVP_FUNCTIONS|DIRECT_TYPES|PUBLIC_ADDRESS_|PUBLIC_EXACT_TYPES|CONSTRUCTORS|COMPLETE_DESTRUCTORS|DELETING_DESTRUCTORS|OWNERSHIP_COMMENTS|CONSTRUCTOR_LIFETIME_COMMENTS|LONG_DOUBLE_MEMBERS|IVP_VTABLE_|MISSING_TYPES|UNRESOLVED_IDS|UNRESOLVED_NAMES|PROBLEM|PROBLEMS)" } |
        ForEach-Object { Write-Host $_ }
}

function Remove-IdaWorkingSet {
    param([Parameter(Mandatory = $true)][string]$IdbPath)

    $directory = [System.IO.Path]::GetDirectoryName($IdbPath)
    if ($directory -ne $buildRoot) {
        throw "Refusing to clean IDA files outside build-dev: $IdbPath"
    }
    $stem = [System.IO.Path]::GetFileNameWithoutExtension($IdbPath)
    foreach ($extension in @('.i64', '.id0', '.id1', '.id2', '.nam', '.til')) {
        $candidate = Join-Path $directory ($stem + $extension)
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            Remove-Item -LiteralPath $candidate -Force
        }
    }
}

try {
    # Generate the exact-public method set before opening IDA. Auditing a
    # checked-in or previous-run TSV here would let newly recovered IDB names
    # evade the same transaction that introduced them.
    $publicAuditArguments = @(
        $publicAuditPath,
        "--repo-root", $workspace,
        "--reference-root", $referenceRoot,
        "--evidence-ledger", $publicEvidencePath,
        "--format", "methods-tsv"
    )
    $publicAuditOutput = @(& python @publicAuditArguments)
    if ($LASTEXITCODE -ne 0) {
        throw "Public IVP method audit generation failed with exit code $LASTEXITCODE"
    }
    [IO.File]::WriteAllLines(
        $transactionMethodsPath,
        [string[]]$publicAuditOutput,
        [Text.UTF8Encoding]::new($true))

    Copy-Item -LiteralPath $databasePath -Destination $transactionPath

    Invoke-IdaScript -Script $applyPath -Expected @("EXPORTED\s+1600", "SAVED\s+") -ExportManifest -AutoAnalysis

    $audits = @(
        @{ Name = "Audit-IvpIdbNamedFunctionTypes.py"; Expected = @("NAMED_IVP_FUNCTIONS\s+1044", "MISSING_TYPES\s+0") },
        @{ Name = "Audit-IvpIdbDirectTypes.py"; Expected = @("DIRECT_TYPES\s+681", "MISSING_TYPES\s+0") },
        @{ Name = "Audit-IvpIdbPublicAddressTypes.py"; Expected = @("PUBLIC_ADDRESS_IDS\s+681", "PUBLIC_ADDRESS_RVAS\s+680", "UNRESOLVED_IDS\s+0", "MISSING_TYPES\s+0", "PUBLIC_ADDRESS_ABI_PROBLEMS\s+0") },
        @{ Name = "Audit-IvpIdbPublicTypes.py"; Expected = @("PUBLIC_EXACT_TYPES\s+409", "MISSING_TYPES\s+0", "UNRESOLVED_NAMES\s+0"); ScriptArgument = @($transactionMethodsPath) },
        @{ Name = "Audit-IvpIdbConstructors.py"; Expected = @("CONSTRUCTORS\s+93", "PROBLEMS\s+0") },
        @{ Name = "Audit-IvpIdbCompleteDestructors.py"; Expected = @("COMPLETE_DESTRUCTORS\s+69", "PROBLEMS\s+0") },
        @{ Name = "Audit-IvpIdbDeletingDestructors.py"; Expected = @("DELETING_DESTRUCTORS\s+47", "PROBLEMS\s+0") },
        @{ Name = "Audit-IvpIdbOwnershipComments.py"; Expected = @("OWNERSHIP_COMMENTS\s+10", "PROBLEMS\s+0") },
        @{ Name = "Audit-IvpIdbConstructorLifetimeComments.py"; Expected = @("CONSTRUCTOR_LIFETIME_COMMENTS\s+19", "PROBLEMS\s+0") },
        @{ Name = "Audit-IvpIdbDirectDestructorComments.py"; Expected = @("DIRECT_DESTRUCTOR_COMMENTS\s+14", "PROBLEMS\s+0") },
        @{ Name = "Audit-IvpIdbLongDouble.py"; Expected = @("LONG_DOUBLE_MEMBERS\s+0") },
        @{ Name = "Audit-IvpIdbBinary64Alignment.py"; Expected = @("BINARY64_UDT_OWNERS\s+57", "BINARY64_EXACT_LAYOUTS\s+7", "ENVIRONMENT_RETAIL_LAYOUT\s+1", "PROBLEMS\s+0") },
        @{ Name = "Audit-IvpIdbBitfields.py"; Expected = @("BITFIELD_IDB_OWNERS\s+11", "BITFIELD_IDB_STORAGE_FIELDS\s+23", "PROBLEMS\s+0") },
        @{ Name = "Audit-IvpIdbVtables.py"; Expected = @("IVP_VTABLE_ADDRESS_POINTS\s+75", "IVP_VTABLE_EXACT_TARGET_TABLES\s+18", "PROBLEMS\s+0") }
    )
    foreach ($audit in $audits) {
        $auditPath = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot $audit.Name)).Path
        $scriptArguments = @()
        if ($audit.ContainsKey("ScriptArgument")) {
            $scriptArguments = $audit.ScriptArgument
        }
        Invoke-IdaScript -Script $auditPath -Expected $audit.Expected `
            -ScriptArgument $scriptArguments
    }

    # PowerShell's same-volume Move-Item replacement works on the .NET
    # Framework host used by IDA and retains no backup. Until this line the
    # canonical IDB is untouched.
    Move-Item -LiteralPath $transactionManifestPath -Destination $manifestPath -Force
    Move-Item -LiteralPath $transactionPath -Destination $databasePath -Force
    Write-Host "COMMITTED`t$databasePath"
    Write-Host "SHA256`t$((Get-FileHash -Algorithm SHA256 -LiteralPath $databasePath).Hash)"
}
finally {
    Remove-IdaWorkingSet -IdbPath $transactionPath
    if (Test-Path -LiteralPath $transactionManifestPath) {
        Remove-Item -LiteralPath $transactionManifestPath -Force
    }
    if (Test-Path -LiteralPath $transactionMethodsPath) {
        Remove-Item -LiteralPath $transactionMethodsPath -Force
    }
}
