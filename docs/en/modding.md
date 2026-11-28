# Create a Mod

Use an SDL SDK matching your Player. Keep the SDK outside the game and do not
build against BML's private `src/` headers. Start with a Script Mod when the
feature fits its APIs; use Native code for native libraries, engine/ImGui
extensions or a measured performance need.

| Route | Entry | Build/deployment |
| --- | --- | --- |
| Script | `[bml.mod]` class and `BML::ModContext` callbacks | Matching CKAngelScript runtime; `.mod.as`, directory or ZIP |
| Native C++20 | `BML::Mod::Instance` + `BML_DEFINE_MOD` | Versioned C entry; `.bmodp` + ABI sidecar |
| Native C11 | `BML_GetModInterface` in `Mod.h` | Same C protocol and sidecar |
| Native/script service | Generated IMC RPC and Topics | `.imc`, reviewed lock, generated language bindings |
| Native provider C table | `Interface.hpp`, declared provider dependency | Process-local ABI and provider lifetime |

`IMod`, `IBML`, `IConfig`, `IProperty`, `ICommand`, `ILogger` and
`IMessageReceiver` are removed. Old SDL sources use the [migration guide](native-mod-migration.md).

## A single workflow on every platform

Python 3.10+ is the common entry. These commands work without shell-specific
line continuation:

```text
python /path/to/sdk/scripts/bml.py new script example.hello
python /path/to/sdk/scripts/bml.py new native example.native
```

Run one creation command, enter its generated project directory, then use:

```text
python bml.py build --bml-sdk /path/to/sdk
python bml.py run --bml-sdk /path/to/sdk --ballance-root /path/to/game
python bml.py pack
```

Linux/macOS also provide executable `scripts/bml` and project `./bml`; Windows
provides `scripts/bml.cmd` and project `bml.cmd`. Replace `/path/to/...` with your
own paths and quote paths containing spaces. Native projects need CMake and a
C++20 compiler. Script projects do not need a native compiler.

The workflow remembers local choices under `.bml/`. `build` prepares the artifact
without deploying or starting Player; `run` prepares, deploys a managed copy and
starts Player. Script `run` continues synchronizing saved edits for hot reload.
A native binary is rebuilt/redeployed for a later Player run, not hot-replaced
inside the running process. `pack` writes the publishable archive in `dist`.

`--configuration` chooses a supported SDK configuration; use the SDK's matching
configuration. `--package-root` appends dependency prefixes; `--generator` selects
a CMake generator. Core C ABI Mods do not need CK2/ImGui extension headers. Add
matching dependency development packages when opting into those extensions.

The game root contains `base.cmo` and either `Bin/Player` or `Player`; Windows
uses `Player.exe`. Supply that root through the retained `--ballance-root` option,
not the `Bin` directory. Native deployment checks the SDK/game ABI descriptors.

## Existing projects

An existing project can adopt the workflow without replacing its source/CMake:

```text
python /path/to/sdk/scripts/bml.py init example.existing --project /path/to/project
```

For a Script directory add `--kind script` when it cannot be inferred. Move a
single-file Script Mod into its own development directory before adopting the
helper. Generated project files include `bml.mod.json`, `bml.py`, `bml`, `bml.cmd`
and ignored local settings. Use `--target` or `--artifact` when Native target or
filename inference is ambiguous. SDK creation refuses to overwrite a project.

## Manual CMake remains supported

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyMod LANGUAGES CXX)
find_package(BML CONFIG REQUIRED)
bml_add_mod(MyMod src/MyMod.cpp)
bml_install_mod(MyMod)
```

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/sdk -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix /path/to/game/ModLoader
```

`bml_add_mod` links the core `BML::BML`, chooses a module for the target OS and
emits the required ABI sidecar. Add `BML::Engine` and/or `BML::ImGui` explicitly
for direct CK/Vx/ImGui use. Engine/ImGui imports and their compiler, standard
library, runtime, configuration and SDL must match Player. Reuse Player's SDL;
SDK CMake performs any macOS load-command/signature fixup at build time.

## Samples and advanced profiles

The SDK installs `examples/native-mod`, `native-c-mod`, `native-interface-provider`,
`native-interface-consumer` and, when scripting is enabled, `script-mod`.
The basic C++ example uses `ctx.Config()`, `ctx.Input()`, `ctx.Timers()` and
`ctx.Commands()`. Service views are non-owning. Registration members express
ownership; inspect Result values or use `Take()` to report failures through the
module-local callback barrier.

```text
python /path/to/sdk/scripts/bml.py new native example.provider --profile interface-provider
python /path/to/sdk/scripts/bml.py new native example.consumer --profile interface-consumer --provider-id example.provider
python /path/to/sdk/scripts/bml.py new native example.imc --profile imc-provider
```

For native `.bml-interface` schemas, use `python bml.py interface update` for
intentional compatible changes; `interface check` validates the lock. IMC
`.imc` locks instead use the configured `bml_update_imc_locks` CMake target. Use [IMC](imc-author-guide.md)
for ordinary typed Native/Script services, a native C table for process-local
infrastructure, and a CKAngelScript extension only when scripts need native types
that cannot be represented by the ordinary services.

## Package and verify

Native CLI `pack` archives the module and sidecar only. Add a project-owned
resource packaging step for native assets (the in-tree BallSticky/BMLModuls
packages demonstrate this). Script `pack` includes sources and resources.

- Native: keep `Name.bmodp` and `Name.bmodp.abi.json` together, or ZIP both.
- Script: `.mod.as`, a directory entry or ZIP. Do not include editor API stubs,
  `.bml`, compiler output, local paths or credentials.
- Resources: exact case, relative portable paths and UTF-8 text. Prefer `/` in
  paths; do not assume a case-insensitive host or a two-byte `wchar_t`.
- Test the packed artifact with the managed development copy removed. Verify
  required/optional dependencies, input/UI ownership, cleanup, world reset and
  reloading failures. [Support status](validation.md) is separate from a build pass.

See [Native API](native-mod-api.md), [Script Mods](script-mod/index.md),
[Behavior](behavior-authoring.md) and [API routes](native-api-routes.md).
