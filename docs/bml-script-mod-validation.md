# BML Script Mod Validation

This checklist covers the current BML Script Mod Platform and its typed
IMC-backed facade smoke matrix.

## Automated Ballance validation

Run the repository smoke script:

    tests/smoke/Validate-BMLBallance.ps1 \
      -BallanceRoot $env:BML_BALLANCE_ROOT \
      -BuildDll cmake-build-release/bin/BMLPlus.dll \
      -PlayerSeconds 30 \
      -KeepInstalled

BallanceRoot is required unless BML_BALLANCE_ROOT is set. The script backs up
the installed DLL, installs the current DLL and smoke packages, starts Player,
collects ModLoader, Player, and AngelScript logs, and returns a structured
result. A non-zero exit after Goodbye! is reported as shutdown_anomaly rather
than as an IMC failure.

Use SingleFileSmoke or ZipSmoke to exercise those package forms. Use
HotReloadStateSmoke to exercise script-runtime migration:

    tests/smoke/Validate-BMLBallance.ps1 \
      -BallanceRoot $env:BML_BALLANCE_ROOT \
      -BuildDll cmake-build-release/bin/BMLPlus.dll \
      -CKAngelScriptDll $env:CKANGELSCRIPT_ROOT/build-ci-release/src/Release/AngelScript.dll \
      -HotReloadStateSmoke \
      -HotReloadStateScenario Success

CompileFailure, MigrateFailure, and RestoreFailure scenarios must reject the
candidate and leave the previous runtime processing.

## Build matrix

Run from the repository root with an x86 Visual Studio developer shell:

    cmake -S . -B cmake-build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBML_ENABLE_ANGELSCRIPT=ON -DCKANGELSCRIPT_ROOT=$env:CKANGELSCRIPT_ROOT
    cmake --build cmake-build-release --config Release

    cmake -S . -B cmake-build-release-noas -G Ninja -DCMAKE_BUILD_TYPE=Release -DBML_ENABLE_ANGELSCRIPT=OFF
    cmake --build cmake-build-release-noas --config Release

Both builds must produce bin/BMLPlus.dll.

## Current Player smoke API

The AngelScript-enabled normal path installs BMLAngelScriptSmoke. It must show
all of the following in ModLoader.log:

- Registered BML AngelScript bindings.
- BML script mod summary: imc-facades.
- BML IMC facade smoke: runtime=true stream=true.
- A successful BML IMC stream poll.
- Compile and callback diagnostics from their negative smokes, contained ImGui
  recovery, and Goodbye! from the shutdown smoke.

The optional package checks are:

- SingleFileSmoke: BML single-file script smoke loaded resource=true.
- ZipSmoke: BML zip script smoke loaded resource=true.
- HotReloadStateSmoke: the state candidate is installed; a successful
  candidate migrates state, while a failed candidate is rejected and the old
  runtime keeps running.

For an AngelScript-disabled build, install the no-AS DLL, leave script smoke
files present, and verify native BML loads without script-mod execution. If
AngelScript.dll is absent, expect the ckas-host diagnostic that script mods
are unavailable.

The old Record/Registry, export-registry, ExportRef, and CallFrame smoke
assertions are deliberately gone. They are not public APIs.

## Entry and safety checks

Entry validation happens on the user path: BML scans single-file, directory,
and zip script packages; CKAngelScript compiles their entry; and metadata
reflection validates bml.mod and dependencies. A smoke directory has exactly
one top-level .mod.as entry file. Before copying a smoke directory, remove its
destination to avoid nesting stale files.

Before and after Player tests, confirm that base.cmo was unchanged:

    Get-Item (Join-Path $env:BML_BALLANCE_ROOT 'base.cmo') | Format-List FullName,Length,LastWriteTime

Script-mod tests must not modify base.cmo.
