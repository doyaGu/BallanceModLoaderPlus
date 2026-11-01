# Loads ordinary native and script ZIP packages in Player. Each Mod reads its
# bundled resource through the public SDK; the runner observes their results.
[CmdletBinding()]
param(
    [string]$BallanceRoot = $env:BML_BALLANCE_ROOT,
    [Parameter(Mandatory = $true)]
    [string]$BuildDll,
    [string]$DriverMod,
    [string]$TestMod,
    [string]$ArtifactsDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '..\..\scripts\lib\BMLProject.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'BMLPlayerHarness.psm1') -Force

if (-not $BallanceRoot) {
    throw 'Pass -BallanceRoot or set BML_BALLANCE_ROOT.'
}
$releaseBin = Split-Path -Parent ([IO.Path]::GetFullPath($BuildDll))
if (-not $DriverMod) {
    $DriverMod = Join-Path $releaseBin 'PlayerFlowDriver.bmodp'
}
if (-not $TestMod) {
    $TestMod = Join-Path $releaseBin 'NativeZipResourcesTest.bmodp'
}
if (-not $ArtifactsDirectory) {
    $ArtifactsDirectory = Join-Path (Get-BMLProjectLayout).RepoRoot `
        "build-dev\player-native-zip-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
}
foreach ($path in @($BuildDll, $DriverMod, $TestMod)) {
    Assert-BMLPath -Path $path -Type Leaf
}

$scriptSource = Join-Path $PSScriptRoot '..\smoke\AngelScript\BMLAngelScriptZipSmoke'
$nativeSource = Join-Path $ArtifactsDirectory "native-package-$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Force (Join-Path $nativeSource 'Resources') | Out-Null
Copy-Item -LiteralPath $TestMod -Destination (Join-Path $nativeSource 'NativeZipResourcesTest.bmodp')
Copy-Item -LiteralPath (Join-Path $scriptSource 'Resources\probe.txt') `
    -Destination (Join-Path $nativeSource 'Resources\probe.txt')
$nativeZip = Join-Path $ArtifactsDirectory 'NativeZipResourcesTest.zip'
$scriptZip = Join-Path $ArtifactsDirectory 'ScriptResourcesTest.zip'
New-BMLZipFromDirectory -SourceDir $nativeSource -ZipPath $nativeZip
New-BMLZipFromDirectory -SourceDir $scriptSource -ZipPath $scriptZip

$run = Invoke-BMLPlayerRun -BallanceRoot $BallanceRoot -LoaderDll $BuildDll `
    -Install @(
        @{ Source = $DriverMod; Destination = 'ModLoader\Mods\PlayerFlowDriver.bmodp' },
        @{ Source = $nativeZip; Destination = 'ModLoader\Mods\NativeZipResourcesTest.zip' },
        @{ Source = $scriptZip; Destination = 'ModLoader\Mods\ScriptResourcesTest.zip' },
        @{ Source = (Join-Path $PSScriptRoot 'BehaviorAcceptance.cfg')
           Destination = 'ModLoader\Configs\BML.cfg' }
    ) -ArtifactsDirectory $ArtifactsDirectory -PlayerWidth 800 -PlayerHeight 600 -TimeoutSeconds 60

$flow = Get-BMLPlayerFlowChecks -Run $run -Probes @('NativeZipResourcesTest')
$checks = $flow.Checks
$checks['NativeResourceOnLoad'] = $run.ModLoaderLog.Contains('Native ZIP resource: phase=load readable=true')
$checks['NativeResourceOnUnload'] = $run.ModLoaderLog.Contains('Native ZIP resource: phase=unload readable=true')
$checks['ScriptResourceOnLoad'] = $run.ModLoaderLog.Contains('BML zip script smoke loaded resource=true')
$failed = @($checks.GetEnumerator() | Where-Object { -not $_.Value } | ForEach-Object Key)
[pscustomobject]@{
    Status = $(if ($failed.Count -eq 0) { 'pass' } else { 'fail' })
    ArtifactsDirectory = $run.ArtifactsDirectory
    Trace = $run.Trace
    Checks = [pscustomobject]$checks
    FailedChecks = $failed
}
if ($failed.Count -gt 0) {
    throw "Native ZIP resource test failed: $($failed -join ', ')"
}
