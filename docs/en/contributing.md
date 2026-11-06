# Build and contribute to BML+

Mod authors normally use an installed SDK and the [Mod workflow](modding.md).
This page is for changing the Loader, its SDK, bindings or tools.

## Prerequisites and engine selection

Use CMake 3.25+, C++20, Python 3.10+ and SDL3 3.4.8+ development files. Initialize
BML+'s submodules. Use the matching Ballanced `sdl` revision and submodules;
never mix original Virtools headers/libraries into this build.

```sh
git submodule update --init --recursive
cmake -S . -B build/sdl-release -DBML_RUNTIME=sdl \
  -DBALLANCED_ROOT=/path/to/Ballanced \
  -DCKANGELSCRIPT_ROOT=/path/to/CKAngelScript \
  -DCMAKE_BUILD_TYPE=Release -DBML_BUILD_MODS=ON -DBML_BUILD_TESTS=ON
cmake --build build/sdl-release --config Release --parallel
```

`BALLANCED_ROOT` defaults to a sibling `Ballanced` checkout. Existing CK2/VxMath
CMake targets are reused; otherwise matching source subdirectories are added or
installed packages are found. To select installed packages rather than that
sibling tree, pass `-DBALLANCED_ROOT=` and `-DCMAKE_PREFIX_PATH=/path/to/engine-sdk`.
SDL3 must also be discoverable through CMake.

Scripts are enabled by default. The default `BML_BUILD_SCRIPT_RUNTIME=ON` builds
the matching CKAngelScript plugin from `CKANGELSCRIPT_ROOT` against the same
engine targets. For the script-free build use a separate directory and
`-DBML_ENABLE_ANGELSCRIPT=OFF`. Supplying only headers is not a runtime deployment.

A compiler/architecture/configuration change needs a new build directory. The
branch accepts only `BML_RUNTIME=sdl` and dynamic hosts; it rejects static host
integration and old `VIRTOOLS_SDK_PATH` cache settings. On a multi-config generator
keep `--config Release` and `ctest -C Release`; on Windows choose the architecture
of the Player, not an unconditional `-A Win32`.

## Install the right component

```sh
cmake --install build/sdl-release --config Release --prefix /path/to/sdk
cmake --install build/sdl-release --config Release \
  --prefix /path/to/disposable-game --component BMLRuntime
cmake --install build/sdl-release --config Release \
  --prefix /path/to/disposable-game --component BMLMods
```

The first command creates the development installation. `BMLRuntime` installs
only the runtime component into an already prepared matching Player tree;
`BMLMods` installs the optional in-tree Mods. Build those Mods with
`BML_BUILD_MODS=ON`. Player supplies CK2, VxMath and SDL. On macOS the runtime
stage must contain the matching host SDL for load-command normalization/signing;
use a disposable copy of a complete Player installation, not SDK libraries copied
into a game. Packaging uses the component install manifest to select owned files.

## Actual runtime flow and owners

```text
Ballanced SDL Player / CK2
  ModManager: CK lifecycle, ImGui frame, late PostProcess, render notification
    ModContext: composes services and borrows CK managers
      ModLoader: ModRecord identities, dependencies, instances and invocation gates
        NativeMod: C definition/instance/callbacks
        ScriptMod: CKAngelScript runtime and per-Mod context projection
        built-in Mods: composed registrations and local callbacks
```

The CK engine performs its default physics scheduling. BML's late PostProcess
runs the Behavior safe point, then its normal service/Mod work, then completes
deferred input cleanup. It is inside `CKContext::Process`, before that function
returns. Render is a separate operation; a pending skip is consumed by the next
Render. BML does not add a Player dependency on the Loader.

`IMod`, `IBML`, `IMessageReceiver`, `IConfig`, `IProperty`, `ICommand` and `ILogger`
have been removed, including internal implementations. `ModRecord` is a composed
registration, not a replacement inherited Mod interface. Public C++
`Mod::Instance` virtual calls stay inside each Mod module.

| Change | Source owner | Focused checks |
| --- | --- | --- |
| CK plugin entry / manager lifecycle | `src/BML.cpp`, `src/Loader/ModManager.*` | Build and real Player start/stop |
| Discovery, dependencies, unload/callback ownership | `src/Loader/ModLoader.*`, `NativeMod.*`, `ModRecord.*` | Loader/registry tests and Player retirement/reload |
| Runtime composition and borrowing | `src/Loader/ModContext.*` | Service tests, script on/off, shutdown |
| Versioned C records and module-local C++ SDK | `include/BML/`, `src/Api/`, `cmake/` | C/C++ layout, installed/relocated SDK consumers |
| Behavior graph/execution/world lifetime | `src/Behavior/`, `src/Api/Behavior/` | Behavior and object-reference tests |
| Script VM, projections, convenience engine calls | `src/AngelScript/` | Binding/reference checks, script and Player tests |
| Input interception and render requests | `src/Hooks/` | Input phases, borrowing, Process/Render separation |
| Commands, config, timers, IMC, DataShare | Their named directories under `src/` | Module contracts and callback cleanup |
| Fonts, console, menu and HUD | `src/UI/`, `Console/`, `ModMenu/`, `HUD/` | Headless tests plus real visual/input acceptance |
| SDK/CLI/update tools | `scripts/`, `tools/`, `tests/sdk`, `tests/updater/sdl` | CLI/package/ABI and signed transaction fixtures |

`ScriptEngineCalls` is private and compiled only with scripting. It runs existing
engine convenience operations in the built-in Mod's Behavior Session; per-Mod
Behavior authoring keeps its own ownership. RefCount is private to DataShare.
There is no `ExecuteBBAdapter`, `ScriptFacadeAccess` or old I* compatibility layer.

## Validation

```sh
ctest --test-dir build/sdl-release -C Release --output-on-failure --parallel 6
cmake -S tests/sdk -B build/sdk-contracts
ctest --test-dir build/sdk-contracts --output-on-failure
```

The standalone SDK contracts need no engine. The full suite also builds installed
SDK consumers when CK2/VxMath installation is available. Run the script-free
configuration separately. Process-management and updater tests are serialized
because they inspect running Player processes.

Real [Player acceptance](https://github.com/doyaGu/BallanceModLoaderPlus/tree/sdl/tests/player/sdl)
requires a desktop and matching game assets. Its runner uses copies and compares
original/staged Mod hashes; it must not patch a Mod to repair SDK relocation.
Use `--benchmark-api` for API timings, not as a functional pass/fail threshold.
[Validation status](validation.md) records executed targets and remaining work.

## Public changes and generated files

Preserve C table prefixes, calling conventions, record ownership, borrowed
lifetimes and explicit failure codes. SDK C++ values and callbacks remain
module-local. Engine/ImGui opt-ins additionally require their C++ dependency ABI.
Internal refactors must not change event names or callback order implicitly.

Change an IMC `.imc` or native interface schema, update its adjacent lock with the
provided generator/workflow, and review source, lock and generated files together.
Do not hand-edit generated codecs. `bml_update_imc_locks` updates CMake-managed
IMC locks; `bml interface update` updates an author project’s interface lock.

For documentation use the existing requirements and both configurations:

```sh
python -m pip install -r requirements-docs.txt
python -m mkdocs build --strict --config-file mkdocs.yml
python -m mkdocs build --strict --config-file mkdocs.zh-CN.yml
```

Keep paired guides, examples, API declarations and install layout consistent.
Native snippets use the installed SDK; script snippets use the actual registered
properties and methods. Do not package API completion stubs as runtime scripts.
Historical design records are not the current authoring contract.

Documentation source mapping, API regeneration and example checks are described
in [the documentation maintenance notes](https://github.com/doyaGu/BallanceModLoaderPlus/blob/sdl/docs/README.md).
