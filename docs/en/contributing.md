# Contributing to BML+

This guide is for contributors who build and change the BML+ loader itself. If
you want to create a mod against a released BML+ SDK, use the [native mod
guide](native-mod-api.md) or the [script mod guide](script-mod/index.md)
instead. Mod authors do not need to build this repository.

## Runtime flow

```text
BallancePlayer / Virtools CK2
            |
       BMLPlus.dll
            |
       ModManager              CK lifecycle and engine callbacks
            |
       ModContext              discovery, dependency order, ownership, services
        /         \
 native mods     script host   IMod callbacks / CKAngelScript callbacks
        \         /
       built-in services and generated IMC APIs
```

`ModManager` connects the Virtools manager lifecycle to BML+. `ModContext`
owns mod discovery, dependency order, callback dispatch, public services, and
shutdown. Native mods enter through the installed C++ ABI; script mods enter
through the CKAngelScript host. Built-in generated IMC Providers project
loader-owned behavior rather than implementing a second copy of it.

## Development environment

BML+ is a 32-bit Windows plugin for Virtools CK2. A core development build
requires:

- Visual Studio 2019 or newer with the C++ toolchain;
- CMake 3.14 or newer;
- Python 3.10 or newer;
- Virtools SDK 2.1;
- CKAngelScript API 6 or newer when script support is enabled; and
- all Git submodules from this repository.

Clone the repository with its submodules:

```powershell
git clone --recursive https://github.com/doyaGu/BallanceModLoaderPlus.git
cd BallanceModLoaderPlus
```

If the repository was cloned without `--recursive`, initialize the submodules
before configuring:

```powershell
git submodule update --init --recursive
```

## Configure, build, and test

Use an x86/Win32 build. A 64-bit binary cannot be loaded by Ballance Player and
cannot link against the Virtools SDK libraries.

The command below targets a multi-configuration Visual Studio generator. If
Visual Studio is not the default, pass `-G` with the installed generator name.
For Ninja, enter an x86 Native Tools environment, omit `-A Win32`, add
`-DCMAKE_BUILD_TYPE=Debug`, and use a separate build directory configured with
`-DCMAKE_BUILD_TYPE=Release` for the release build.

```powershell
cmake -S . -B build-dev `
  -A Win32 `
  -DVIRTOOLS_SDK_PATH="<path-to-Virtools-SDK-2.1>" `
  -DCKANGELSCRIPT_ROOT="<path-to-CKAngelScript>" `
  -DBML_ENABLE_ANGELSCRIPT=ON `
  -DBML_BUILD_TESTS=ON `
  -DCMAKE_INSTALL_PREFIX="<absolute-path-to-install-dev>"

cmake --build build-dev --config Debug
ctest --test-dir build-dev -C Debug --output-on-failure
```

The Debug DLL is written to `build-dev/bin/Debug/BMLPlus.dll` with Visual
Studio. A single-configuration generator writes it to
`build-dev/bin/BMLPlus.dll`. Build Release before packaging:

```powershell
cmake --build build-dev --config Release
ctest --test-dir build-dev -C Release --output-on-failure
```

To verify the installed SDK layout and its consumer helpers:

```powershell
cmake --build build-dev --config Release --target install
```

The CMake install contains `BMLConfig.cmake`, public headers, mod CMake helpers,
the IMC generator, and public Mod-author documentation. The release packager
adds the native/script templates and editor API files to that installed tree.

## Runtime validation

Unit and integration tests do not exercise Virtools or the real Player. Changes
to hooks, lifecycle order, rendering, input, CK object access, native mod
loading, or script hosting also require validation in the real Player. Choose
the validation level that observes the behavior being changed:

- `tests/smoke/Validate-BMLBallance.ps1` checks deployment, Mod loading,
  exported runtime surfaces, shutdown, and installation restoration. It is a
  process-integration smoke test, not proof of Building Block semantics.
- A focused Player scenario drives the actual game path and checks an observable
  world result. Use this level for behavior execution, physics epochs, graph
  changes, lifecycle ordering, rendering, or input behavior.

Set `BML_BALLANCE_ROOT` or pass `-BallanceRoot` to the smoke script:

```powershell
powershell -ExecutionPolicy Bypass `
  -File tests/smoke/Validate-BMLBallance.ps1 `
  -BallanceRoot "<Ballance-root>" `
  -BuildDll "build-dev/bin/Debug/BMLPlus.dll"
```

Close any existing Player process before replacing a loaded DLL. The script
backs up the installed loader, installs smoke assets, starts Player, validates
the logs, and restores the previous installation unless `-KeepInstalled` is
specified.

Player acceptance is split into one driver Mod and one probe Mod per subject.
`PlayerFlowDriver` owns only the shipped flow: the menu graph into Level 01, the
tutorial exit handshake, the frame captures, and the exit. It knows no subject
under test. Every verdict comes from a probe Mod whose DLL exports
`BMLPlayerProbeRead`; the driver discovers the loaded probes, waits for each one
to report, and fails the run when a probe fails. A probe that reads gameplay
state also exports `BMLPlayerProbeStart`, so the driver decides when it may
touch the ball, the camera, or input. Which subjects a run covers is therefore
decided by which probes the runner installs, not by the driver.

The ExecuteBB probe physicalizes a body it owns itself, so it proves the
ExecuteBB physics operations without depending on the retail ball:

```powershell
cmake --build build-dev --config RelWithDebInfo --target BML `
  PlayerFlowDriver ExecuteBBTest
powershell -ExecutionPolicy Bypass `
  -File tests/player/Invoke-ExecuteBBTest.ps1 `
  -BallanceRoot "<Ballance-root>" `
  -BuildDll "build-dev/bin/RelWithDebInfo/BMLPlus.dll"
```

Every runner shows the Player window by default and restores the installed
loader, the test Mods, and the logs when it finishes.

The Behavior Runtime probes cover the runtime semantics fixture, the transport
seam, Patches, the published Plan and Hook facade, and the script hook a script
Mod retires. They install together with their two Virtools fixture plugins:

```powershell
cmake --build build-dev --config RelWithDebInfo --target BML `
  PlayerFlowDriver BehaviorRuntimeSemanticsTest BehaviorTransportTest `
  BehaviorPatchTest BehaviorFacadeTest BehaviorScriptHookTest `
  BehaviorLifecycleFixture BehaviorTransportFixture
powershell -ExecutionPolicy Bypass `
  -File tests/player/Invoke-BehaviorAcceptanceTest.ps1 `
  -BallanceRoot "<Ballance-root>" `
  -BuildDll "build-dev/bin/RelWithDebInfo/BMLPlus.dll"
```

Every probe path defaults to the directory holding the loader DLL, so one
`-BuildDll` is enough. Add `-DisableAngelScript` when the install has no
`AngelScript.dll`; the script hook probe then reports SKIPPED instead of
failing.

All runners share `tests/player/BMLPlayerHarness.psm1`. `Invoke-BMLPlayerRun`
owns backing up and installing the touched files, starting Player, answering the
FullScreen Setup dialog, capturing window screenshots, driving the tutorial exit
key, and restoring the installation. `Get-BMLPlayerFlowChecks` turns the driver
line and the probe verdicts into the checks every runner shares. Change that
module when startup, the flow, or the probe protocol changes, not the individual
runners.

## Find the owner of a change

| Change | Owner | Minimum focused validation |
| --- | --- | --- |
| Plugin entry, Hook Block registration, or engine interception | `src/BML.cpp`, `src/Behavior/HookBlock.*`, `src/Hooks/` | Win32 build plus the affected real Player scenario |
| Building Block Prototype configuration, execution, or graph insertion | `src/Behavior/Runtime.*`, the affected `src/Behavior/<BuildingBlock>.*` module, `src/Api/ExecuteBB.cpp` | Dependency test, exported ABI test, Win32 build, and affected Player test |
| CK lifecycle and callback timing | `src/Loader/ModManager.*` | Focused lifecycle tests and Player smoke test |
| Mod discovery, dependency order, services, or shutdown | `src/Loader/ModContext.*` | Relevant loader/dependency tests and native/script smoke coverage |
| HUD, menus, command bar, or built-in behavior | `src/Mods/BMLMod.*`, `src/HUD/`, `src/Console/`, `src/CustomMaps/`, `src/Gameplay/`, `src/UI/` | Focused UI/service tests and Player visual/input smoke test |
| Legacy native SDK or CMake consumer behavior | `include/BML/`, `cmake/` | ABI/compile tests, template configure/build, and installed SDK check |
| IMC runtime | `src/Imc/ImcApi.cpp`, `src/Imc/ImcRuntime.*` | IMC runtime/compatibility tests and native IMC smoke test |
| Built-in interface struct or the reads behind it | `include/BML/Interface.h`, `src/Api/Interfaces.cpp`, `src/Api/BuiltinCapabilities.*` | Focused interface tests, the C ABI compile test, and native smoke test |
| Opaque CK object references at the C/script seam | `include/BML/Types.h`, `src/Api/ObjectRefs.*` | `ObjectRefsTest`, C ABI/IMC compile tests, and a Player lifecycle test when deletion timing changes |
| IMC code generator or its sample interface | `tools/imc_codegen.py`, `tests/imc/` | Generator check, compatibility test, and review of interface, lock, and header together |
| Script discovery, binding, execution, or reload | `src/AngelScript/`, `docs/api/` | Focused script tests, API stub check, and script-capable Player smoke test |
| Public docs or release layout | `docs/`, `src/CMakeLists.txt`, `scripts/Package-BMLRelease.ps1` | Both strict MkDocs builds, CMake install, and SDK stage validation |

Start with `ModManager.cpp`, then read the declarations in `IMod.h`,
`IMessageReceiver.h`, `IBML.h`, and `ModContext.h`. Use function-level searches
to enter `ModContext.cpp`; reading that file from top to bottom is not a useful
introduction.

## Public interface rules

The legacy native C++ interfaces, including `IBML`, `IMod`, and
`IMessageReceiver`, cross the DLL boundary and are ABI-frozen for the current
release line. Do not change virtual function signatures or order, object
layout, ownership rules, or types passed across that boundary.

The `BML_*` C APIs and IMC use explicit handles, status codes, and allocation
functions. Preserve their documented ownership and compatibility rules. A new
loader capability belongs in a versioned interface struct reached through
`BML_GetInterface`; a service one Mod publishes for other Mods normally belongs
in a generated IMC interface. A provider interface registered through BML is
reserved for native base Mods that need process-local pointers and can enforce
a required Mod dependency; never introduce an ad hoc C++ ABI.

Script APIs are public source interfaces. A binding change must update the
script API reference, author documentation, and runtime smoke coverage in the
same change.

## Generated interfaces

The loader publishes no `.imc` interface of its own; the generator is an
authoring tool for Mods that publish theirs. Two `.imc` files live in this
repository, both as tests. `tests/imc/test.sample.imc` keeps the generator, its
lock format, and its committed output under test.
`tests/smoke/smoke.native.imc` belongs to the native smoke Mod, which publishes
it and consumes it again in the Player so the loader's IMC exports stay
exercised at runtime; `bml_target_imc_api` generates its header into the build
tree, so only the `.imc` and its lock are committed.

Do not edit a generated header by hand. Change the matching `.imc` file and run
the generator. For the sample interface that is:

```powershell
python tools/imc_codegen.py `
  --update-lock `
  --out-dir tests/imc/generated `
  --input tests/imc/test.sample.imc
```

Review and commit the `.imc`, `.imc.lock`, and generated header together. The
lock owns stable field and endpoint identities; do not edit its numbers by
hand. A normal BML+ build runs the generator in check mode over the sample and
fails when its committed binding is stale. The smoke interface is regenerated by
the build itself, and a lock that no longer matches its `.imc` fails that build
with the `bml_update_imc_locks` target to run.

Public Mod-author documentation is installed from CMake into
`share/BML/docs/<language>`. The release packager copies that installed tree;
do not add a second source-document copy step to the packaging script. Editor
API stubs remain under `docs/api` because they are tool inputs rather than
reader documentation.

## Change discipline

- Keep game-thread callbacks short. Do not block in `OnProcess`, `OnRender`,
  engine hooks, or game-thread IMC handlers.
- Treat borrowed CK objects as non-owning. Revalidate stored object references
  after object deletion, level changes, and CK reset.
- Do not unload a mod DLL while one of its callbacks is running.
- Use generated IMC bindings instead of hand-written payload codecs.
- Add behavior tests for observable rules. Avoid tests that only search source
  files for implementation text.
- Update English and Simplified Chinese public documentation together when the
  same public behavior is documented in both languages.

Commit titles use short imperative English, for example:

```text
Fix native mod instance cleanup
Document native mod build requirements
Reject malformed event payloads
```

Keep unrelated behavior, documentation, generated output, and cleanup changes
in separate commits when they can be reviewed independently.

## Before submitting a change

- Build Debug and Release as Win32.
- Run the relevant focused tests and the full test suite.
- Run the appropriate real Player validation for runtime-facing changes. A
  smoke run is sufficient only when process integration is the behavior under
  test; game semantics require a focused Player scenario.
- Verify generated IMC headers and locks when an interface changed.
- Build both documentation sites with strict warnings.
- Confirm that legacy native headers and exports were not changed accidentally.
- Review the final diff for unrelated local files before staging it.
