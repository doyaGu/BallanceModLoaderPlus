# BML Script Mod Validation

This checklist covers the current BML Script Mod Platform smoke matrix.

## Automated Ballance Validation

Preferred release validation is the repository test script:

```powershell
tests/smoke/Validate-BMLBallance.ps1 `
  -BallanceRoot $env:BML_BALLANCE_ROOT `
  -BuildDll cmake-build-release/bin/BMLPlus.dll `
  -PlayerSeconds 30 `
  -KeepInstalled
```

`-BallanceRoot` is required unless `BML_BALLANCE_ROOT` is set.

The script backs up the installed `BuildingBlocks/BMLPlus.dll`, installs the
new DLL, installs the script smoke directories and `BMLNativeInteropSmoke.bmodp`,
starts `Bin/Player.exe`, collects
`ModLoader/ModLoader.log`, `Bin/Player.log`, and `Bin/AngelScript.log`, then
returns a structured result object.

Use `-SingleFileSmoke` to also install a root-level `*.mod.as` smoke with a
sibling resource directory. Use `-ZipSmoke` to package and install a `.zip`
script smoke. These use distinct mod ids and can run alongside the directory
smokes and the native `.bmodp` smoke.

If Player exits with a non-zero code after `Goodbye!` was written, the script
reports `Status = shutdown_anomaly`. Treat that separately from script facade
regressions. Missing smoke log lines, timeout, or missing `Goodbye!` are
validation failures.

## Build Matrix

Run from the repository root with an x86 Visual Studio developer shell:

```powershell
cmake -S . -B cmake-build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBML_ENABLE_ANGELSCRIPT=ON -DCKANGELSCRIPT_ROOT=$env:CKANGELSCRIPT_ROOT
cmake --build cmake-build-release --config Release

cmake -S . -B cmake-build-release-noas -G Ninja -DCMAKE_BUILD_TYPE=Release -DBML_ENABLE_ANGELSCRIPT=OFF
cmake --build cmake-build-release-noas --config Release
```

Both builds should produce `bin/BMLPlus.dll`.

## Entry Validation

Script entry validation happens in the same path users run: BML scans installed
single-file, directory, and zip script mods, CKAngelScript compiles the entry,
and metadata reflection validates `[bml.mod]`, dependencies, and exports during
Player/load smoke. Smoke directories should contain exactly one top-level
`*.mod.as` entry file; single-file smokes are installed directly under
`ModLoader/Mods`, and zip smokes are produced through `scripts/Pack-BMLScriptMod.ps1`.
Compile/runtime negative smokes intentionally fail inside Player.

## Player Smoke Matrix

Use `-BallanceRoot` or `BML_BALLANCE_ROOT` to select the manual Player test directory.

When copying smoke directories, remove the destination directory first. Copying a directory onto an existing directory can create a nested stale smoke directory and invalidate the test.

- AngelScript-enabled normal path:
  - Copy `cmake-build-release/bin/BMLPlus.dll` to `BuildingBlocks/BMLPlus.dll`.
  - Copy `tests/smoke/AngelScript/BMLAngelScriptSmoke` to `ModLoader/Mods/BMLAngelScriptSmoke`.
  - Start `Bin/Player.exe`.
  - Expected log includes `Registered BML AngelScript bindings`, export round trips, command hooks, load events, render/process, and physics hooks.
  - Expected log includes `BML mod root valid: true`, `BML mod path valid: true`, and `BML mod path escape blocked: true`.
  - Expected log includes `BML ctx runtime: paused=false playing=false cheat=false sr=0 hs=0` during startup smoke.
  - Expected log includes `BML time/input api: timeOk=true frameOk=true keyboard=true mouse=true escUp=true keyName=true mousePosOk=true mouseRelOk=true`.
  - Expected log includes `BML ctx time/input api: timeOk=true frameOk=true keyboard=true mouse=true escUp=true keyName=true input=true`.
  - Expected log includes `BML object api null defaults: valid=false id=0 nameEmpty=true class=0 childCount=0`.
  - Expected log includes `BML ctx borrowed manager api: ck=true renderContext=true attr=true behavior=true collision=true input=true message=true path=true parameter=true render=true sound=true time=true`.
  - Expected log includes `BML ctx object api null defaults: valid=false id=0 nameEmpty=true class=0 childCount=0 missingArray=true`.
  - Expected log includes `BML hud/timer api: flags=7 title=true fps=true srHud=true currentContext=true srTimeOk=true` with default HUD settings.
  - Expected log includes `BML ctx directories: game=true loader=true config=true invalid=true`.
  - Expected log includes `BML mod resource helpers: file=true dir=true read=true escapeRead=true missingRead=true`.
  - With `-SingleFileSmoke`, expected log includes `BML single-file script smoke loaded resource=true`.
  - With `-ZipSmoke`, expected log includes `BML zip script smoke loaded resource=true`.
  - Expected log includes `BML mod registry query: count=4 first=BML selfByIndex=true invalid=true` when normal smoke and shutdown smoke are the only script Mods present.
  - Expected log includes `BML ctx mod registry query: count=4 first=BML selfByIndex=true selfFind=true invalid=true` under the same Mod set.
  - Expected log includes `BML modref metadata: author=BML+ desc=true bmlVersion=0.3.12 parts=0.3.12`.
  - Expected log includes `BML modref dependencies: check=1 count=2 first=BML@0.3.12 firstOptional=false second=bml.optional.missing.smoke@9.9.9 secondOptional=true invalid=true`.
  - Expected log includes `BML script export registry: count=7 sum=true indexedCall=true overloadedAny=-703 overloadedInt=0 overloadedCall=0 overloadedResult=15 invalid=true`.
  - Expected log includes `BML native modref metadata: author=<native-mod-authors> desc=true bmlVersion=0.3.12`.
  - Expected log includes `BML native export registry: count=6 sum=true indexedCall=true invalid=true`.
  - Expected log includes `BML command query: count=9 first=bml echo=true missing=false echoCheat=false`.
  - Expected log includes `BML command details valid: true`.
  - Expected log includes `BML menu/message api: exercised=true`, plus `echo bml-command-smoke` and `echo bml-command-global-smoke`.
  - Expected log includes `BML config: greeting=hello enabled=true answer=42 scale=2.5 category=true key=true`.
  - Expected log includes `BML config mismatch fallback: -33`.
  - Expected log includes `BML typed datashare: bool=true int=42 float=3.5`.
  - Expected log includes `BML typed datashare mismatch fallback: -77`.
  - Expected log includes `BML datashare delegate request: immediate=true pending=true bool=true object=true invalid=true`.
  - Expected log includes `BML script timer registration: once=true loop=true callbackOnce=true callbackLoop=true callbackDelegate=true invalid=true` and a `cancelledState` field.
  - Expected log includes `BML script command delegate registration: global=true method=true invalid=true`.
  - Expected log includes `BML native command delegate completion smoke command=true` and `BML native command method delegate completion smoke command=true`.
  - Expected log includes `BML native mod state BML kind=1 state=2 smoke kind=2 state=2`.
  - Expected log includes `BML native export lifecycle smoke first=0 duplicate=-10 unregister=0 validBefore=1 validAfter=0 staleCall=-709 callback=false`.
  - Expected log includes `BML native export hardening smoke findEx=0 ambiguous=-703 explicit=0 mismatch=-705 badSig=-704 missingTarget=-700 exception=-708`.
  - Expected log includes `BML native export hardening script tryEcho=0`, `voidString=-3`, and `constants=true`.
  - Expected log includes `BML native export NativeFrameReport status=0` and `enumOk=true`.
  - Expected shutdown summary includes `BML script timer summary: once=true loop=true callbackOnce=true callbackLoop=true callbackDelegate=true cancelled=false` plus `command=true`, `commandDelegate=true`, `commandDelegateCompletion=true`, `commandDelegateMethod=true`, `commandDelegateMethodCompletion=true`, `dataShareDelegate=true`, and `selfCommandCalls=1`.
  - On shutdown, expected log includes `Removed 1 native export(s) owned by unloaded Mod BML`.
  - Expected log does not include `forbidden symbol` or `Failed to register BML AngelScript declarations`.

- AngelScript-disabled build:
  - Copy `cmake-build-release-noas/bin/BMLPlus.dll` to `BuildingBlocks/BMLPlus.dll`.
  - Leave script smoke files in `ModLoader/Mods`.
  - Start `Bin/Player.exe`.
  - Expected log includes native BML loading and no script-mod execution.

- Missing `AngelScript.dll`:
  - Copy AngelScript-enabled `BMLPlus.dll`.
  - Temporarily rename `BuildingBlocks/AngelScript.dll`.
  - Start `Bin/Player.exe`, then restore the DLL.
  - Expected log marks script mods failed with `phase=ckas-host message=AngelScript.dll is not loaded; script mods are unavailable.`

- Compile-error negative smoke:
  - Temporarily copy `tests/smoke/AngelScript/BMLAngelScriptCompileErrorSmoke` to `ModLoader/Mods`.
  - Expected log includes `phase=compile` and CKAS compile diagnostics.
  - Because BML does not parse AngelScript source, compile failure before metadata reflection uses synthetic id `script:BMLAngelScriptCompileErrorSmoke`.
  - Normal smoke should continue running and should be able to read `id=script:BMLAngelScriptCompileErrorSmoke kind=2 state=3` through `BML::ModRef`.
  - Native BML smoke should log diagnostics for the synthetic failed mod if it checks this case.
  - Remove the temporary negative smoke directory after the test.

- Runtime-error negative smoke:
  - Temporarily copy `tests/smoke/AngelScript/BMLAngelScriptRuntimeErrorSmoke` to `ModLoader/Mods`.
  - Expected log includes `phase=callback`, the thrown message, and a stack trace.
  - Normal smoke should continue running and should be able to read `id=bml.runtime.error.smoke kind=2 state=3` and the failed mod diagnostic through `BML::ModRef`.
  - Native BML smoke should log `BML native failed mod diagnostic id=bml.runtime.error.smoke status=0`.
  - Remove the temporary negative smoke directory after the test.

- Shutdown smoke:
  - Temporarily copy `tests/smoke/AngelScript/BMLAngelScriptShutdownSmoke` to `ModLoader/Mods`.
  - Expected log includes `BML shutdown smoke requesting exit`.
  - Expected shutdown log includes `Removed 1 native export(s) owned by unloaded Mod BML`, proving BML force-removes native exports left by a Mod after `OnUnload`.
  - Remove the temporary shutdown smoke directory after the test.

## Safety Checks

Before and after Player tests:

```powershell
Get-Item (Join-Path $env:BML_BALLANCE_ROOT 'base.cmo') | Format-List FullName,Length,LastWriteTime
```

Script mod tests must not modify `base.cmo`.
