# Runs the shipped Player flow with every Behavior probe installed. The probes
# cover the runtime semantics fixture, the transport seam, Patches and the
# published Plan and Hook facade, plus the script hook a script Mod retires.
[CmdletBinding()]
param(
    [string]$BallanceRoot = $env:BML_BALLANCE_ROOT,

    [string]$BuildDll,

    [string]$DriverMod,

    [string]$RuntimeSemanticsMod,

    [string]$TransportMod,

    [string]$PatchMod,

    [string]$FacadeMod,

    [string]$ScriptHookMod,

    [string]$FixtureDll,

    [string]$TransportFixture,

    [string]$ScriptMod,

    [switch]$DisableAngelScript,

    [string]$ArtifactsDirectory,

    [ValidateRange(0, 16384)]
    [int]$PlayerWidth = 800,

    [ValidateRange(0, 16384)]
    [int]$PlayerHeight = 600,

    [ValidateRange(10, 600)]
    [int]$TimeoutSeconds = 120
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot '..\..\scripts\lib\BMLProject.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'BMLPlayerHarness.psm1') -Force

if (-not $BallanceRoot) {
    throw 'Ballance root is required. Pass -BallanceRoot or set BML_BALLANCE_ROOT.'
}

$layout = Get-BMLProjectLayout
if (-not $BuildDll) {
    $BuildDll = Join-Path $layout.DefaultReleaseBin 'BMLPlus.dll'
}
$releaseBin = Split-Path -Parent ([System.IO.Path]::GetFullPath($BuildDll))
if (-not $DriverMod) {
    $DriverMod = Join-Path $releaseBin 'PlayerFlowDriver.bmodp'
}
if (-not $RuntimeSemanticsMod) {
    $RuntimeSemanticsMod = Join-Path $releaseBin 'BehaviorRuntimeSemanticsTest.bmodp'
}
if (-not $TransportMod) {
    $TransportMod = Join-Path $releaseBin 'BehaviorTransportTest.bmodp'
}
if (-not $PatchMod) {
    $PatchMod = Join-Path $releaseBin 'BehaviorPatchTest.bmodp'
}
if (-not $FacadeMod) {
    $FacadeMod = Join-Path $releaseBin 'BehaviorFacadeTest.bmodp'
}
if (-not $ScriptHookMod) {
    $ScriptHookMod = Join-Path $releaseBin 'BehaviorScriptHookTest.bmodp'
}
if (-not $FixtureDll) {
    $FixtureDll = Join-Path $releaseBin 'BehaviorLifecycleFixture.dll'
}
if (-not $TransportFixture) {
    $TransportFixture = Join-Path $releaseBin 'BehaviorTransportFixture.dll'
}
if (-not $ScriptMod) {
    $ScriptMod = Join-Path $PSScriptRoot 'BehaviorLifecycleScript.mod.as'
}
if (-not $ArtifactsDirectory) {
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $ArtifactsDirectory = Join-Path $layout.RepoRoot `
        "build-dev\player-behavior-acceptance-$timestamp"
}

$scriptModRelative = 'ModLoader\Mods\BehaviorLifecycleScript.mod.as'
$install = @(
    @{ Source = $DriverMod
       Destination = 'ModLoader\Mods\PlayerFlowDriver.bmodp' },
    @{ Source = $RuntimeSemanticsMod
       Destination = 'ModLoader\Mods\BehaviorRuntimeSemanticsTest.bmodp' },
    @{ Source = $TransportMod
       Destination = 'ModLoader\Mods\BehaviorTransportTest.bmodp' },
    @{ Source = $PatchMod
       Destination = 'ModLoader\Mods\BehaviorPatchTest.bmodp' },
    @{ Source = $FacadeMod
       Destination = 'ModLoader\Mods\BehaviorFacadeTest.bmodp' },
    @{ Source = $ScriptHookMod
       Destination = 'ModLoader\Mods\BehaviorScriptHookTest.bmodp' },
    @{ Source = $FixtureDll
       Destination = 'BuildingBlocks\BehaviorLifecycleFixture.dll' },
    @{ Source = $TransportFixture
       Destination = 'BuildingBlocks\BehaviorTransportFixture.dll' }
)
$remove = @()
if ($DisableAngelScript) {
    # This install has no AngelScript runtime, so the script Mod would only
    # produce a load failure. The script hook probe skips itself to match.
    $remove += $scriptModRelative
} else {
    $install += @{ Source = $ScriptMod; Destination = $scriptModRelative }
}

$run = Invoke-BMLPlayerRun -BallanceRoot $BallanceRoot -LoaderDll $BuildDll `
    -Install $install -Remove $remove `
    -Environment @{
        BML_PLAYER_DISABLE_ANGELSCRIPT =
            $(if ($DisableAngelScript) { '1' } else { $null })
    } `
    -WindowCaptures ([ordered]@{
        'BehaviorPatch-baseline' = 'Behavior patch visual: stage=baseline '
        'BehaviorPatch-active' = 'Behavior patch visual: stage=active '
        'BehaviorPatch-restored' = 'Behavior patch visual: stage=restored '
        'BehaviorRuntime' = 'Behavior runtime visual: stage=active '
    }) `
    -ArtifactsDirectory $ArtifactsDirectory -PlayerWidth $PlayerWidth `
    -PlayerHeight $PlayerHeight -TimeoutSeconds $TimeoutSeconds

$log = $run.ModLoaderLog
$flow = Get-BMLPlayerFlowChecks -Run $run -Probes @(
    'BehaviorRuntimeSemanticsTest', 'BehaviorTransportTest',
    'BehaviorPatchTest', 'BehaviorFacadeTest', 'BehaviorScriptHookTest')
$checks = $flow.Checks

$runtimeSemantics = [regex]::Match($log,
    'Behavior runtime semantics: status=(?<status>pass|fail) ' +
    'lifecycle=(?<lifecycle>true|false) additive_edit=(?<additiveEdit>true|false) ' +
    'relations=(?<relations>true|false) physics_force=(?<physicsForce>true|false) ' +
    'hook_error=(?<hookError>true|false) message=(?<message>true|false) ' +
    'visual=(?<visual>true|false) detail=(?<detail>\S+)')
$transport = [regex]::Match($log,
    'Behavior transport: status=(?<status>pass|fail) reason=(?<reason>\S+) ' +
    'transport=(?<transport>true|false) wire=(?<wire>true|false) ' +
    'object_ref=(?<objectRef>true|false) session_after_reset=(?<session>true|false) ' +
    'catalog=(?<catalog>true|false) detached=(?<detached>true|false) ' +
    'inspect=(?<inspect>true|false) watch=(?<watch>true|false)')
$patch = [regex]::Match($log,
    'Behavior patch: status=(?<status>pass|fail) reason=(?<reason>\S+) ' +
    'module=(?<module>true|false) visual=(?<visual>true|false) ' +
    'durable=(?<durable>true|false) relations=(?<relations>true|false) ' +
    'apply=(?<apply>true|false) execute=(?<execute>true|false) ' +
    'close=(?<close>true|false) restore=(?<restore>true|false) ' +
    'reset=(?<reset>true|false) deletion=(?<deletion>true|false) ' +
    'retirement=(?<retirement>true|false) hooks=(?<hooks>true|false) ' +
    'graph_changed=(?<graphChanged>true|false) ' +
    'callback_close=(?<callbackClose>true|false) ' +
    'teardown_reentry=(?<teardownReentry>true|false)')
$facade = [regex]::Match($log,
    'Behavior plan: status=(?<status>pass|fail) reason=(?<reason>\S+) ' +
    'submit=(?<submit>true|false) install=(?<install>true|false) ' +
    'hooks=(?<hooks>true|false) close=(?<close>true|false) ' +
    'release=(?<release>true|false) taps=(?<taps>[0-9]+) ' +
    'afters=(?<afters>[0-9]+) frames=(?<frames>[0-9]+)')
$facadeSelfClose = [regex]::Match($log,
    'Behavior self-close: status=(?<status>pass|fail) calls=(?<calls>[0-9]+) ' +
    'closing=(?<closing>true|false)')
$facadePatch = [regex]::Match($log,
    'Behavior graph patch: status=(?<status>pass|fail) reason=(?<reason>\S+) ' +
    'apply=(?<apply>true|false) close=(?<close>true|false)')
$facadeReplacement = [regex]::Match($log,
    'Behavior node replacement: status=(?<status>pass|fail)')
$facadeRemoval = [regex]::Match($log,
    'Behavior node removal: status=(?<status>pass|fail) ' +
    'lifecycle=(?<lifecycle>true|false) restore=(?<restore>true|false) ' +
    'pending=(?<pending>true|false) ' +
    'active_peer=(?<activePeer>true|false) ' +
    'isolation=(?<isolation>true|false)')
$facadeIdentity = [regex]::Match($log,
    'Behavior identity: status=(?<status>pass|fail) attach=(?<attach>true|false) ' +
    'continuation=(?<continuation>true|false) ' +
    'identity=(?<identity>true|false) befores=(?<befores>[0-9]+)')
$authoredScript = [regex]::Match($log,
    'Behavior authored script: status=(?<status>pass|fail) ' +
    'atomic=(?<atomic>true|false) ' +
    'create=(?<create>true|false) edit=(?<edit>true|false) ' +
    'operation=(?<operation>true|false) ' +
    'activity=(?<activity>true|false) close=(?<close>true|false)')
$gameplayPatch = [regex]::Match($log,
    'Behavior gameplay patch: status=(?<status>pass|fail) ' +
    'scripts=(?<scripts>[0-9]+) realtime=(?<realtime>true|false) ' +
    'delta=(?<delta>true|false)')
$scriptHook = [regex]::Match($log,
    'Behavior script hook: status=(?<status>pass|fail) reason=(?<reason>\S+) ' +
    'installed=(?<installed>true|false) frames=(?<frames>[0-9]+)')

$checks['RuntimeSemanticsFixture'] = $runtimeSemantics.Success -and
    $runtimeSemantics.Groups['status'].Value -eq 'pass' -and
    $runtimeSemantics.Groups['lifecycle'].Value -eq 'true' -and
    $runtimeSemantics.Groups['additiveEdit'].Value -eq 'true' -and
    $runtimeSemantics.Groups['relations'].Value -eq 'true' -and
    $runtimeSemantics.Groups['physicsForce'].Value -eq 'true' -and
    $runtimeSemantics.Groups['hookError'].Value -eq 'true' -and
    $runtimeSemantics.Groups['message'].Value -eq 'true' -and
    $runtimeSemantics.Groups['visual'].Value -eq 'true' -and
    $runtimeSemantics.Groups['detail'].Value -eq 'complete'
$checks['BehaviorRuntimeVisual'] = $run.Captures.'BehaviorRuntime'.Captured -and
    (Test-ImageDimensions -Path $run.Captures.'BehaviorRuntime'.Path `
        -Width $PlayerWidth -Height $PlayerHeight) -and
    $log.Contains(
        'Behavior runtime visual: stage=active call=true start=true pulse=true')
$checks['BehaviorTransportProbe'] = $transport.Success -and
    $transport.Groups['status'].Value -eq 'pass' -and
    $transport.Groups['transport'].Value -eq 'true' -and
    $transport.Groups['session'].Value -eq 'true' -and
    $transport.Groups['catalog'].Value -eq 'true'
$checks['BehaviorPatch'] = $patch.Success -and
    $patch.Groups['status'].Value -eq 'pass' -and
    $patch.Groups['module'].Value -eq 'true' -and
    $patch.Groups['visual'].Value -eq 'true' -and
    $patch.Groups['durable'].Value -eq 'true' -and
    $patch.Groups['relations'].Value -eq 'true' -and
    $patch.Groups['apply'].Value -eq 'true' -and
    $patch.Groups['execute'].Value -eq 'true' -and
    $patch.Groups['close'].Value -eq 'true' -and
    $patch.Groups['restore'].Value -eq 'true' -and
    $patch.Groups['reset'].Value -eq 'true' -and
    $patch.Groups['deletion'].Value -eq 'true' -and
    $patch.Groups['retirement'].Value -eq 'true' -and
    $patch.Groups['hooks'].Value -eq 'true' -and
    $patch.Groups['graphChanged'].Value -eq 'true' -and
    $patch.Groups['callbackClose'].Value -eq 'true' -and
    $patch.Groups['teardownReentry'].Value -eq 'true' -and
    $log.Contains(
        'Behavior patch graph changed: status=pass rejected=-1 error=27 readmitted=0 state=2') -and
    $log -match
        'Behavior patch callback close: status=pass first=-9 second=-9 state=3 nodes=2 links=3 routed=true calls=\d+' -and
    $log.Contains(
        'Behavior patch callback restore: status=pass stale=true nodes=1 links=2') -and
    $log.Contains(
        'Behavior patch teardown reentry: status=pass outer=0 stale=true queued=true self=-9 sibling=-9 state=3 sibling_state=3 calls=2 nodes=2 links=3 detach=2 delete=2 restored=true')
$checks['BehaviorPlanFacade'] = $facade.Success -and
    $facade.Groups['status'].Value -eq 'pass' -and
    $facade.Groups['submit'].Value -eq 'true' -and
    $facade.Groups['install'].Value -eq 'true' -and
    $facade.Groups['hooks'].Value -eq 'true' -and
    $facade.Groups['close'].Value -eq 'true' -and
    $facade.Groups['release'].Value -eq 'true' -and
    [int]$facade.Groups['taps'].Value -ge 2 -and
    [int]$facade.Groups['afters'].Value -ge 1
$checks['BehaviorHookSelfClose'] = $facadeSelfClose.Success -and
    $facadeSelfClose.Groups['status'].Value -eq 'pass' -and
    [int]$facadeSelfClose.Groups['calls'].Value -eq 1 -and
    $facadeSelfClose.Groups['closing'].Value -eq 'true'
$checks['BehaviorGraphPatchFacade'] = $facadePatch.Success -and
    $facadePatch.Groups['status'].Value -eq 'pass' -and
    $facadePatch.Groups['apply'].Value -eq 'true' -and
    $facadePatch.Groups['close'].Value -eq 'true'
$checks['BehaviorNodeReplacementFacade'] = $facadeReplacement.Success -and
    $facadeReplacement.Groups['status'].Value -eq 'pass'
$checks['BehaviorNodeRemovalFacade'] = $facadeRemoval.Success -and
    $facadeRemoval.Groups['status'].Value -eq 'pass' -and
    $facadeRemoval.Groups['lifecycle'].Value -eq 'true' -and
    $facadeRemoval.Groups['restore'].Value -eq 'true' -and
    $facadeRemoval.Groups['pending'].Value -eq 'true' -and
    $facadeRemoval.Groups['activePeer'].Value -eq 'true' -and
    $facadeRemoval.Groups['isolation'].Value -eq 'true'
$checks['BehaviorIdentityFacade'] = $facadeIdentity.Success -and
    $facadeIdentity.Groups['status'].Value -eq 'pass' -and
    $facadeIdentity.Groups['attach'].Value -eq 'true' -and
    $facadeIdentity.Groups['continuation'].Value -eq 'true' -and
    $facadeIdentity.Groups['identity'].Value -eq 'true' -and
    [int]$facadeIdentity.Groups['befores'].Value -ge 1
$checks['BehaviorAuthoredScript'] = $authoredScript.Success -and
    $authoredScript.Groups['status'].Value -eq 'pass' -and
    $authoredScript.Groups['atomic'].Value -eq 'true' -and
    $authoredScript.Groups['create'].Value -eq 'true' -and
    $authoredScript.Groups['edit'].Value -eq 'true' -and
    $authoredScript.Groups['operation'].Value -eq 'true' -and
    $authoredScript.Groups['activity'].Value -eq 'true' -and
    $authoredScript.Groups['close'].Value -eq 'true'
$checks['BehaviorGameplayMigration'] = $gameplayPatch.Success -and
    $gameplayPatch.Groups['status'].Value -eq 'pass' -and
    [int]$gameplayPatch.Groups['scripts'].Value -eq 2 -and
    $gameplayPatch.Groups['realtime'].Value -eq 'true' -and
    $gameplayPatch.Groups['delta'].Value -eq 'true'
$checks['BehaviorPatchVisual'] =
    $run.Captures.'BehaviorPatch-baseline'.Captured -and
    $run.Captures.'BehaviorPatch-active'.Captured -and
    $run.Captures.'BehaviorPatch-restored'.Captured -and
    (Test-ImageDimensions -Path $run.Captures.'BehaviorPatch-baseline'.Path `
        -Width $PlayerWidth -Height $PlayerHeight) -and
    (Test-ImageDimensions -Path $run.Captures.'BehaviorPatch-active'.Path `
        -Width $PlayerWidth -Height $PlayerHeight) -and
    (Test-ImageDimensions -Path $run.Captures.'BehaviorPatch-restored'.Path `
        -Width $PlayerWidth -Height $PlayerHeight) -and
    $log.Contains('Behavior patch visual: stage=baseline ') -and
    $log.Contains('Behavior patch visual: stage=active ') -and
    $log.Contains('Behavior patch visual: stage=restored ')
$checks['BehaviorInspectProbe'] = $transport.Success -and
    $transport.Groups['inspect'].Value -eq 'true' -and
    $log -match
        'Behavior inspect: status=pass graph=Gameplay_Events nodes=\d+ links=\d+ template_nodes=53 template_links=60 delay_1=true delay_2=true pending=unknown live=true'
$checks['BehaviorDetachedProbe'] = $transport.Success -and
    $transport.Groups['detached'].Value -eq 'true'
$checks['BehaviorWatchProbe'] = $transport.Success -and
    $transport.Groups['watch'].Value -eq 'true' -and
    $log.Contains(
        'Behavior watch: status=pass sampled=true events=2 callback_failure=true graph_endpoints=true layout_target=true layout_events=1') -and
    $log.Contains('Behavior layout target: changed=true using=true ') -and
    $log -match
        'Behavior layout watch event: sequence=1 kind=2 before=\d+ after=\d+ status=pass'
$checks['FrameWire'] = $transport.Success -and
    $transport.Groups['wire'].Value -eq 'true'
$checks['CaptureTimeObjectRef'] = $transport.Success -and
    $transport.Groups['objectRef'].Value -eq 'true'
# Without the AngelScript runtime there is no script Mod to insert the block,
# so the probe declares the skip instead of reporting a hook it never saw.
$checks['ScriptHookRetirement'] = $(if ($DisableAngelScript) {
        $log.Contains(
            'ScriptHook retirement: skipped=true reason=angelscript-disabled')
    } else {
        $scriptHook.Success -and
        $scriptHook.Groups['status'].Value -eq 'pass' -and
        $scriptHook.Groups['installed'].Value -eq 'true' -and
        $log.Contains('ScriptHookRetirement installed=true') -and
        ([regex]::Matches($log,
            'ScriptHookRetirement callback=1 uninstall=true').Count -eq 1) -and
        $log.Contains('ScriptHookRetirement retired=true callbacks=1')
    })
$checks['CleanExecuteBB'] = -not $log.Contains('ExecuteBB::')

$failedChecks = @($checks.GetEnumerator() | Where-Object { -not $_.Value } |
    ForEach-Object Key)

$result = [pscustomobject]@{
    Status = $(if ($failedChecks.Count -eq 0) { 'pass' } else { 'fail' })
    BallanceRoot = $run.BallanceRoot
    Visible = $run.WindowActivated
    AngelScriptDisabled = [bool]$DisableAngelScript
    SetupDialogAccepted = $run.SetupDialogAccepted
    PlayerExitCode = $run.PlayerExitCode
    PlayerTimedOut = $run.TimedOut
    SourceHash = Get-BMLOptionalHash $BuildDll
    DriverModHash = Get-BMLOptionalHash $DriverMod
    RuntimeSemanticsModHash = Get-BMLOptionalHash $RuntimeSemanticsMod
    TransportModHash = Get-BMLOptionalHash $TransportMod
    PatchModHash = Get-BMLOptionalHash $PatchMod
    FacadeModHash = Get-BMLOptionalHash $FacadeMod
    ScriptHookModHash = Get-BMLOptionalHash $ScriptHookMod
    FixtureHash = Get-BMLOptionalHash $FixtureDll
    TransportFixtureHash = Get-BMLOptionalHash $TransportFixture
    ScriptModHash = Get-BMLOptionalHash $ScriptMod
    ArtifactsDirectory = $run.ArtifactsDirectory
    Screenshot = $run.Screenshot
    TutorialScreenshot = $run.TutorialScreenshot
    Captures = $run.Captures
    Trace = $run.Trace
    PlayerTrace = $run.PlayerTrace
    Flow = $flow.Flow
    Probes = $flow.Verdicts
    Checks = [pscustomobject]$checks
    FailedChecks = $failedChecks
}

$result
if ($failedChecks.Count -gt 0) {
    throw "Behavior acceptance test failed: $($failedChecks -join ', ')"
}
