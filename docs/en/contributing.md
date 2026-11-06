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
       ModContext              private runtime services and CK state
            |
       ModLoader               discovery, ownership, callbacks, lifecycle
        /         \
 native mods     script host   bml.mod C tables / CKAngelScript callbacks
        \         /
       built-in loader features
```

`ModManager` connects the Virtools manager lifecycle to BML+. `ModContext`
is the stable `IBML` object and owns runtime services. Its private `ModLoader`
owns Mod discovery, dependency order, callback dispatch, and Mod shutdown.
Native mods enter through versioned C tables and module-local C++ wrappers; script mods enter
through the CKAngelScript host. Mods may use IMC to publish their own interfaces;
the loader does not publish a generated IMC interface.

## Development environment

This branch targets Ballanced's SDL CK2 and VxMath, using C++20, CMake 3.25+
and Python 3.10+. Initialize the BML+ submodules. Use the matching Ballanced
`sdl` sources or installed CK2/VxMath and SDL3 development packages. AngelScript
support additionally uses the matching CKAngelScript source tree or SDK.

```sh
git submodule update --init --recursive
cmake -S . -B build-sdl -DBML_RUNTIME=sdl -DBML_BUILD_MODS=ON \
  -DBALLANCED_ROOT=/path/to/Ballanced
cmake --build build-sdl --config Release
cmake --install build-sdl --config Release --prefix /path/to/sdk
```

Use a separate build directory for each compiler, architecture and configuration.
`BML_ENABLE_ANGELSCRIPT=OFF` builds the script-free variant. Dynamic hosts are
implemented; static-host integration is pending.

## Runtime validation

Run the SDL component tests in `tests/ck2/sdl`,
installed-SDK Player regression in `tests/player/sdl`, and the focused
SDK tests in `tests/sdk`. Updater tests live in `tests/updater/sdl`.
Component tests use SDL dummy video; native Player tests require a working
desktop and matching game assets. Unexecuted targets remain unverified.

## Find the owner of a change

| Change | Owner | Minimum focused validation |
| --- | --- | --- |
| Plugin entry, Hook Block registration, or engine interception | `src/BML.cpp`, `src/Behavior/Blocks/HookBlock.*`, `src/Hooks/` | SDL build plus the affected real Player scenario |
| Building Block configuration, execution, or graph insertion | `src/Behavior/Block.*`, `src/Behavior/Runtime.*`, `include/BML/Behavior/Blocks/`, `src/Api/ExecuteBB.cpp` | Focused Behavior and ABI tests, SDL build, and affected Player test |
| CK lifecycle and callback timing | `src/Loader/ModManager.*` | Focused lifecycle tests and Player smoke test |
| Mod discovery, dependency order, callback dispatch, or Mod shutdown | `src/Loader/ModLoader.*` | Relevant loader/dependency tests and native/script smoke coverage |
| IBML services or CK runtime state | `src/Loader/ModContext.*` | Relevant service tests and native/script smoke coverage |
| HUD, menus, command bar, or built-in behavior | `src/Mods/BMLMod.*`, `src/HUD/`, `src/Console/`, `src/CustomMaps/`, `src/ModMenu/`, `src/Gameplay/`, `src/UI/` | Focused UI/service tests and Player visual/input smoke test |
| Legacy native SDK or CMake consumer behavior | `include/BML/`, `cmake/` | ABI/compile tests, template configure/build, and installed SDK check |
| IMC runtime | `src/Imc/ImcApi.cpp`, `src/Imc/ImcRuntime.*` | IMC runtime/compatibility tests and native IMC smoke test |
| Built-in interface struct or the reads behind it | `include/BML/Interface.h`, `src/Api/Interfaces.cpp`, `src/Api/BuiltinCapabilities.*` | Focused interface tests, the C ABI compile test, and native smoke test |
| Opaque CK object references at the C/script seam | `include/BML/Types.h`, `src/Api/ObjectRefs.*` | `ObjectRefsTest`, C ABI/IMC compile tests, and a Player lifecycle test when deletion timing changes |
| IMC code generator or its sample interface | `tools/imc_codegen.py`, `tests/codegen/imc/` | Generator check, compatibility test, and review of interface, lock, and header together |
| Script discovery, binding, execution, or reload | `src/AngelScript/`, `docs/api/` | Focused script tests, API stub check, and script-capable Player smoke test |
| Public docs or release layout | `docs/`, `src/CMakeLists.txt`, `scripts/package_sdl_update.py` | Both strict MkDocs builds, CMake install, and SDK stage validation |

Start with `ModManager.cpp`, then `Mod.h/.hpp`, `ModLoader.h`, NativeMod
and `ModContext.h`. The old IMod/IBML declarations describe private adapters. Follow
`ModLoader.cpp` for Mod lifecycle and `ModContext.cpp` for runtime services.

## Public interface rules

SDL public authoring uses the C tables and module-local C++ wrappers. Preserve
released table prefixes, fixed-width fields, cdecl, ownership and version rules.
IMod/IBML remain private adapters on this branch; they are not another exported
Mod authoring route. The original Windows branch keeps its own frozen ABI.

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

The loader publishes no `.imc` interface of its own; the generator is for Mods.
`tests/codegen/imc/test.sample.imc` checks the generator, lock, and committed
header. The `.imc` files under `tests/smoke/` and `tests/player/` exercise runtime
interop; the provider template has its own starter interface. Runtime test
headers are generated in the build tree.

Do not edit a generated header by hand. Change the matching `.imc` file and run
the generator. For the sample interface that is:

```powershell
python tools/imc_codegen.py `
  --update-lock `
  --out-dir tests/codegen/imc/generated `
  --input tests/codegen/imc/test.sample.imc
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

- Build Debug and Release for the selected SDL platform.
- Run the relevant focused tests and the full test suite.
- Run the appropriate real Player validation for runtime-facing changes. A
  smoke run is sufficient only when process integration is the behavior under
  test; game semantics require a focused Player scenario.
- Verify generated IMC headers and locks when an interface changed.
- Build both documentation sites with strict warnings.
- Confirm that legacy native headers and exports were not changed accidentally.
- Review the final diff for unrelated local files before staging it.

C/C++ changes use the root `.clang-format`: four-space indentation, unindented
access labels, one data member per declaration, and named parameters in public
C tables. Format changed code; keep third-party sources and unrelated pending
work out of formatting patches. Use descriptive names for callback plumbing.
