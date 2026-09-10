# Create mods for BML+

Use a released BML+ SDK to create mods. Building the BML+ repository is only
necessary when changing the loader, SDK, script host, or bundled services.

If you are unsure which route to choose, start with a script mod. It has the
shortest build-free edit, reload, and diagnosis loop. Move to native code only
when a concrete requirement needs native hooks, native memory, caller-thread
RPC handlers, or a performance-critical loop.

## Choose a development route

| Route | Use it when | Main trade-off |
| --- | --- | --- |
| Script mod | You want quick edit/test cycles, commands, config, UI, gameplay scripting, CKAngelScript engine access, or a generated IMC Client/Provider without C++. | IMC callbacks run on the game thread; scripts should not own unsafe hooks or performance-critical loops. |
| Native mod | You need C++20, direct Virtools integration, native hooks, caller-thread IMC execution, or tight control over hot-path work. | Requires an MSVC-compatible Win32 build and explicit DLL ABI and lifetime discipline. |
| Native plugin with a CKAngelScript extension | Scripts must directly use plugin-specific native objects or engine primitives that cannot be represented as IMC records. | The native plugin must register and maintain that script API through CKAngelScript. |

Do not reproduce CKAngelScript's scene, behavior, component, message, or async
APIs in a new BML wrapper. Use CKAngelScript for CK/Vx work and BML+ for mod
identity, lifecycle, configuration, commands, loader UI, and mod-level services.

## Start a script mod

1. Create a Script Mod Project in your normal source workspace:

   ```bat
   "<BML-SDK>\scripts\bml.cmd" new script yourname.my-mod
   ```

   The command infers a readable name and author, creates a valid class and
   entry filename, and adds the project-local Developer Workflow.

2. Enter the generated directory and run `.\bml run`. The first run asks for
   the Ballance folder, verifies the Script Mod runtime, deploys a managed copy,
   starts Player, and prints only this Mod's new log lines.
3. Keep `bml run` open while editing. Each saved source change is synchronized
   to the managed copy so Player's normal hot reload remains active.
4. Run `.\bml pack` to create `dist/<project>.zip`. Test that zip without the
   managed development copy installed.

An existing directory Script Mod can adopt the same commands without rewriting
its source: `"<BML-SDK>\scripts\bml.cmd" init --project "C:\path\to\mod"`.
Move an existing single-file Mod into its own directory first; Player loads that
directory with the same runtime behavior.

Read the [script mod guide](https://doyagu.github.io/BallanceModLoaderPlus/script-mod/),
then use its API reference for exact declarations. A script-capable SDK installs
the same pages under `share/BML/docs/en/script-mod`.

## Start a native mod

1. Open a terminal in the directory where you keep source projects and create
   a Mod from the SDK template:

   ```bat
   "<BML-SDK>\scripts\bml.cmd" new native yourname.my-mod
   ```

   The command creates a working Mod, chooses its readable name from the id, and
   uses your Git name as the author. You do not need to edit CMake settings.

2. Enter the generated directory and run one development command:

   ```bat
   .\bml run
   ```

   The first run asks for the Virtools SDK and Ballance folders. It then selects
   the newest installed Visual Studio generator, configures Win32,
   builds and deploys `RelWithDebInfo`, starts Player, and prints this Mod's new
   log lines after Player exits. The two paths and selected generator are cached
   in ignored `.bml/settings.json`; later runs need only `.\bml run`. The
   project-local tool is Python 3.10+; `bml.cmd` is only its Windows launcher.
3. Edit the source file printed by `new`, then run `.\bml run` again.
4. The native
   Mod must link the same MSVC runtime as the loader it runs in, because the
   native interface passes C++ objects across the DLL boundary. The runtime in
   `BMLPlus-<version>.zip` is built against the Release MSVC runtime, so a Debug
   `.bmodp` is not ABI-compatible with it; `RelWithDebInfo` provides debug
   information while using the compatible runtime. `bml_add_mod` pins the Mod's
   runtime to the SDK you configured against and fails the configure step on a
   conflicting `CMAKE_MSVC_RUNTIME_LIBRARY`, so the two cannot drift apart
   silently.
   `BMLPlus-SDK-<version>-Debug.zip` is the supported exception. It contains a
   Debug `bin/BMLPlus.dll` and its `.pdb`, so a Debug Mod is valid as long as
   you also copy that Debug loader over `BuildingBlocks/BMLPlus.dll`. Keep the
   loader and every installed native Mod on one side of that line, and go back
   to the Release loader before testing what you publish.
5. Before publishing, run `.\bml run --configuration Release`, test that exact
   artifact, then run `.\bml pack`; it copies the Release `.bmodp` into `dist`.
   Use the generated README's manual CMake commands only for CI or diagnosing
   the build itself.

### Use it with an existing native Mod

An existing CMake Mod does not need to be regenerated or rewritten. Run this
once from the extracted SDK:

```bat
"<BML-SDK>\scripts\bml.cmd" init owner.existing-mod --project "C:\path\to\mod"
```

`init` reads the target from `bml_add_mod`, reads the version from `project`, and
adds only `bml.mod.json`, `bml.py`, `bml.cmd`, and ignored local settings. It
does not change `CMakeLists.txt` or source files. If the target cannot be
inferred, pass `--target`; if the installed filename differs from the target,
also pass `--artifact`.

Afterward, `.\bml run` is available, but the existing manual CMake workflow
remains fully supported. Adopting the helper is optional.

### Advanced native starting points

Ignore this section for a normal Mod. These options exist only when two Mods
must call each other:

| Need | Creation option |
| --- | --- |
| Let another native Mod call a small C++ API | `--profile interface-provider` |
| Call that API from a second native Mod | `--profile interface-consumer --provider-id "owner.provider"` |
| Expose a message-based API that isolates callers from your C++ binary | `--profile imc-provider` |

Run `bml help --verbose` to see the complete commands. A provider edits its
file under `api/`, then runs `.\bml interface update` after an intentional
compatible API change. BML+ owns the generated headers.

The SDK CMake entry point is:

```cmake
find_package(BML CONFIG REQUIRED)
bml_add_mod(MyMod MyMod.cpp)
bml_install_mod(MyMod)
```

Read the [native mod API overview](native-mod-api.md) before adding ownership,
callbacks, UI, or cross-mod services, and
[Which native API to use](native-api-routes.md) when a capability has more than
one spelling.

## Shared authoring rules

- Keep the mod id stable. Other mods use it for dependencies and service
  ownership.
- Declare dependencies before load rather than discovering required peers in a
  per-frame callback.
- Treat borrowed CK objects as non-owning and revalidate them after level or
  object changes.
- Keep per-tick, render, engine-hook, and synchronous RPC work bounded.
- Log one clear startup line during development and test the release package in
  a clean `ModLoader/Mods` directory.
- Document the required BML+, CKAngelScript, native plugin, and dependency
  versions in the mod README.

## Choose a communication mechanism

| Need | Use |
| --- | --- |
| A small named scalar or byte value in the same process | DataShare |
| Typed request/response calls, asynchronous results, Topics, or versioned data between native and/or script mods | A generated IMC interface; either language may consume or provide it |
| A built-in BML+ runtime, gameplay, event, UI, or speedrun service | The existing typed BML+ API for the selected language |
| Communication among CKAngelScript runtime scripts or components | CKAngelScript `Message` or `Async` where their execution model fits |

Do not invent a JSON message format or hand-write field identifiers. Define a
`.imc` interface, let `bml_target_imc_api` generate its C++ and/or AngelScript
binding, and keep the schema lock with the interface. See [Inter-mod communication](imc.md) and
[Create a typed IMC API](imc-author-guide.md).

Use a CKAngelScript extension only when a script must directly borrow a
plugin-specific native object or invoke an engine primitive that cannot be
expressed as typed IMC data. Routine native/script service boundaries should
use one generated IMC contract instead of maintaining a second hand-written API.

## Performance and ownership

BML callbacks and synchronous IMC handlers normally run on the game thread.
Cache stable lookup results as ids or revalidating references, move preparation
out of hot callbacks, bound Topic capacities, and monitor dropped messages.

Use a native mod when an operation must patch engine internals, execute a
high-frequency loop, control native memory, or provide a high-throughput
service. Keep policy, configuration, and infrequent control calls in script
when that split makes the mod easier to develop.

## Publish

- Native mods normally publish a `.bmodp` package and must export both
  `BMLEntry(IBML*)` and `BMLExit(IMod*)`.
- Script mods publish a single `*.mod.as` file or a zip containing exactly one
  entry. `.bmodp` is native-only.
- Test the distributed artifact, not only the working directory or a
  `RelWithDebInfo` build.
- Include dependencies, supported versions, installation, configuration, and a
  useful failure-reporting path in the mod README.
