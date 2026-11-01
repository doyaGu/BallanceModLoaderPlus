# Loads ordinary native and script ZIP packages in Player. Each Mod reads its
# bundled resource through the public SDK; the runner observes their results.
[CmdletBinding()]
param(
    [string]$BallanceRoot = $env:BML_BALLANCE_ROOT,
    [Parameter(Mandatory = $true)]
    [string]$BuildDll,
    [string]$DriverMod,
    [string]$TestMod,
    [ValidateSet('separate', 'mixed-reload', 'wrapped-reload', 'nested', 'search-reload', 'search-rollback')]
    [string]$Scenario = 'separate',
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
$search = $Scenario -in @('search-reload', 'search-rollback')
$probe = $(if ($search) { 'ScriptZipSearchPathsTest' } else { 'NativeZipResourcesTest' })
if (-not $DriverMod) {
    $DriverMod = Join-Path $releaseBin 'PlayerFlowDriver.bmodp'
}
if (-not $TestMod) {
    $TestMod = Join-Path $releaseBin "$probe.bmodp"
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
New-Item -ItemType Directory -Force (Join-Path $nativeSource '3D Entities\PH') | Out-Null
Set-Content -LiteralPath (Join-Path $nativeSource '3D Entities\PH\zip-native-only.dat') -Value 'native-resource'
Copy-Item -LiteralPath $TestMod -Destination (Join-Path $nativeSource "$probe.bmodp")
Copy-Item -LiteralPath (Join-Path $scriptSource 'Resources\probe.txt') `
    -Destination (Join-Path $nativeSource 'Resources\probe.txt')
$nativeZip = Join-Path $ArtifactsDirectory 'NativeZipResourcesTest.zip'
$scriptZip = Join-Path $ArtifactsDirectory 'ScriptResourcesTest.zip'
$reload = $Scenario -in @('mixed-reload', 'wrapped-reload')
if ($Scenario -eq 'wrapped-reload') {
    Copy-Item -LiteralPath $scriptSource -Destination (Join-Path $nativeSource 'Script') -Recurse
} elseif ($Scenario -eq 'mixed-reload') {
    foreach ($entry in Get-ChildItem -LiteralPath $scriptSource) {
        Copy-Item -LiteralPath $entry.FullName -Destination $nativeSource -Recurse -Force
    }
}
New-BMLZipFromDirectory -SourceDir $nativeSource -ZipPath $nativeZip
$install = @(
        @{ Source = $DriverMod; Destination = 'ModLoader\Mods\PlayerFlowDriver.bmodp' },
        @{ Source = $nativeZip; Destination = 'ModLoader\Mods\NativeZipResourcesTest.zip' },
        @{ Source = (Join-Path $PSScriptRoot 'BehaviorAcceptance.cfg')
           Destination = 'ModLoader\Configs\BML.cfg' }
)
if ($Scenario -eq 'nested') {
    # Unrelated containers can legitimately ship inner ZIPs with the same name.
    $outerA = Join-Path $nativeSource 'outer-a'
    $outerB = Join-Path $nativeSource 'outer-b'
    $other = Join-Path $nativeSource 'other'
    foreach ($directory in @($outerA, $outerB, (Join-Path $other 'Resources'))) {
        New-Item -ItemType Directory -Force $directory | Out-Null
    }
    Copy-Item -LiteralPath $nativeZip -Destination (Join-Path $outerA 'Shared.zip')
    Set-Content -LiteralPath (Join-Path $other 'Resources\probe.txt') -Value 'unrelated-resource'
    New-BMLZipFromDirectory -SourceDir $other -ZipPath (Join-Path $outerB 'Shared.zip')
    $zipA = Join-Path $ArtifactsDirectory 'A.zip'
    $zipB = Join-Path $ArtifactsDirectory 'B.zip'
    New-BMLZipFromDirectory -SourceDir $outerA -ZipPath $zipA
    New-BMLZipFromDirectory -SourceDir $outerB -ZipPath $zipB
    $install = @($install | Where-Object { $_.Source -ne $nativeZip })
    $install += @(
        @{ Source = $zipA; Destination = 'ModLoader\Mods\A.zip' },
        @{ Source = $zipB; Destination = 'ModLoader\Mods\B.zip' }
    )
}
$environment = @{}
if ($reload) {
    $environment['BML_TEST_NATIVE_ZIP_RELOAD'] = '1'
} else {
    if ($search) {
        $pathsSource = Join-Path $nativeSource 'paths-script'
        New-Item -ItemType Directory -Force $pathsSource | Out-Null
        $script = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'ScriptZipSearchPaths.mod.as')
        Set-Content -LiteralPath (Join-Path $pathsSource 'Paths.mod.as') -Value $script -Encoding utf8
        foreach ($directory in @('3D Entities\PH', 'Textures', 'Sounds')) {
            $target = Join-Path $pathsSource $directory
            New-Item -ItemType Directory -Force $target | Out-Null
            Set-Content -LiteralPath (Join-Path $target 'zip-before.dat') -Value 'zip-before'
        }
        New-BMLZipFromDirectory -SourceDir $pathsSource -ZipPath $scriptZip
        foreach ($directory in @('3D Entities\PH', 'Textures', 'Sounds')) {
            $target = Join-Path $pathsSource $directory
            Remove-Item -LiteralPath (Join-Path $target 'zip-before.dat')
            Set-Content -LiteralPath (Join-Path $target 'zip-after.dat') -Value 'zip-after'
        }
        if ($Scenario -eq 'search-rollback') {
            Set-Content -LiteralPath (Join-Path $pathsSource 'Paths.mod.as') `
                -Value $script.Replace('RejectReload = false', 'RejectReload = true') -Encoding utf8
            $environment['BML_TEST_ZIP_ROLLBACK'] = '1'
        }
        $replacement = Join-Path $ArtifactsDirectory 'ScriptResourcesReplacement.zip'
        New-BMLZipFromDirectory -SourceDir $pathsSource -ZipPath $replacement
        $environment['BML_TEST_ZIP_REPLACEMENT'] = [IO.Path]::GetFullPath($replacement)
    } else {
        New-BMLZipFromDirectory -SourceDir $scriptSource -ZipPath $scriptZip
    }
    $install += @{ Source = $scriptZip; Destination = 'ModLoader\Mods\ScriptResourcesTest.zip' }
}
$run = Invoke-BMLPlayerRun -BallanceRoot $BallanceRoot -LoaderDll $BuildDll `
    -Install $install -Environment $environment `
    -ArtifactsDirectory $ArtifactsDirectory -PlayerWidth 800 -PlayerHeight 600 -TimeoutSeconds 60

$flow = Get-BMLPlayerFlowChecks -Run $run -Probes @($probe)
$checks = $flow.Checks
$checks['NativeResourceOnLoad'] = $run.ModLoaderLog.Contains('Native ZIP resource: phase=load readable=true')
$checks['NativeResourceOnUnload'] = $run.ModLoaderLog.Contains('Native ZIP resource: phase=unload readable=true')
if ($search) {
    $mode = $(if ($Scenario -eq 'search-rollback') { 'rollback' } else { 'reload' })
    $checks['CandidateSearchPaths'] = $run.ModLoaderLog.Contains('ZIP search paths: candidate_readable=true')
    $checks['FinalSearchPaths'] = $run.ModLoaderLog.Contains("ZIP search paths: mode=$mode resources=true stable_count=true")
} else {
    $checks['ScriptResourceOnLoad'] = $run.ModLoaderLog.Contains('BML zip script smoke loaded resource=true')
}
if ($reload) {
    $checks['NativeResourceAfterTwoReloads'] = $run.ModLoaderLog.Contains('Native ZIP reload: round=2 readable=true')
    $checks['ScriptResourceAfterTwoReloads'] =
        [regex]::Matches($run.ModLoaderLog, 'BML zip script smoke loaded resource=true').Count -eq 3
}
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
