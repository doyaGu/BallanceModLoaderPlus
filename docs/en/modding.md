# Create mods for BML+

Use a released BML+ SDK to create mods. Building the BML+ repository is only
necessary when changing the loader, SDK, script host, or bundled services.

If you are unsure which route to choose, start with a script mod. It has the
shortest build-free edit, reload, and diagnosis loop. Move to native code only
when a concrete requirement needs native hooks, native memory, a generated IMC
Provider, or a performance-critical loop.

## Choose a development route

| Route | Use it when | Main trade-off |
| --- | --- | --- |
| Script mod | You want quick edit/test cycles, commands, config, UI, gameplay scripting, or CKAngelScript engine access without a C++ build. | Cannot provide a custom IMC Provider and should not own unsafe hooks or performance-critical native loops. |
| Native mod | You need C++20, direct Virtools integration, native hooks, generated IMC providers, or tight control over hot-path work. | Requires an MSVC-compatible Win32 build and explicit DLL ABI and lifetime discipline. |
| Native plugin with a CKAngelScript extension | A native service owns the unsafe or performance-sensitive work, but scripts need a small typed control surface. | The native plugin must register and maintain that script API through CKAngelScript. |

Do not reproduce CKAngelScript's scene, behavior, component, message, or async
APIs in a new BML wrapper. Use CKAngelScript for CK/Vx work and BML+ for mod
identity, lifecycle, configuration, commands, loader UI, and mod-level services.

## Start a script mod

1. Open PowerShell in `ModLoader/Mods` and create a Mod from the SDK template:

   ```powershell
   & "<BML-SDK>/scripts/New-BMLScriptMod.ps1" `
     -Id "yourname.my-mod" -Name "My Mod" -Author "Your Name"
   ```

   The command creates the destination directory, a valid class and entry
   filename, and personalized metadata. You can also copy
   `templates/script-mod-template` manually.

2. Open the generated directory and README.
3. Confirm that the matching `BuildingBlocks/AngelScript.dll` is installed.
4. Start Player without editing the generated source. Confirm both the in-game
   greeting and its load line in `ModLoader/ModLoader.log`.
5. Keep the generated id stable; changing it later creates a different Mod and
   requires a Player restart.
6. Keep Player open. Saving source in the loaded directory triggers automatic
   hot reload; only new entries, id changes, and dependency changes require a
   restart.
7. From the Mod directory, run `scripts/Pack-BMLScriptMod.ps1`; it writes
   `dist/<directory-name>.zip`. Test that zip without the development copy
   installed.

Read the [script mod guide](https://doyagu.github.io/BallanceModLoaderPlus/script-mod/),
then use its API reference for exact declarations. A script-capable SDK installs
the same pages under `share/BML/docs/en/script-mod`.

## Start a native mod

1. Open PowerShell in the directory where you keep source projects and create
   a Mod from the SDK template:

   ```powershell
   & "<BML-SDK>/scripts/New-BMLNativeMod.ps1" `
     -Id "yourname.my-mod" -Name "My Mod" -Author "Your Name"
   ```

   The command keeps the CMake target, C++ class, source filename, and metadata
   consistent. You can also copy `templates/native-mod-template` manually.

2. Open the generated README. Run `cmake --help`, select an installed Visual
   Studio generator explicitly, configure its Win32 target, and point
   `CMAKE_PREFIX_PATH` at the extracted BML+ SDK. Keep the source and build
   paths short, for example `C:\Mods\MyMod`, to avoid MSBuild file-tracking
   failures in deeply nested directories.
3. Point `VIRTOOLS_SDK_PATH` at Virtools SDK 2.1.
4. Build `RelWithDebInfo` and install the Mod into `ModLoader/Mods`. A native
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
5. Start that Ballance installation's Player, wait until the main menu is
   visible, and confirm the generated Mod's in-game greeting, load line in
   `ModLoader/ModLoader.log`, and sample command. The BML+ version banner alone
   only proves that the loader initialized.
6. Build Release and test the exact artifact you intend to publish.

The SDK CMake entry point is:

```cmake
find_package(BML CONFIG REQUIRED)
bml_add_mod(MyMod MyMod.cpp)
bml_install_mod(MyMod)
```

Read the [native mod API overview](native-mod-api.md) before adding ownership,
callbacks, UI, or cross-mod services.

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
| Typed request/response calls, asynchronous results, Topics, versioned data, or high throughput | A generated IMC interface implemented by a native mod |
| A built-in BML+ runtime, gameplay, event, UI, or speedrun service | The existing typed BML+ API for the selected language |
| Communication among CKAngelScript runtime scripts or components | CKAngelScript `Message` or `Async` where their execution model fits |

Do not invent a JSON message format or hand-write field identifiers. Define a
`.imc` interface, let `bml_add_imc_interface` generate its C++ binding, and keep
the schema lock with the interface. See [Inter-mod communication](imc.md) and
[Create a typed IMC API](imc-author-guide.md).

Script mods can consume BML+'s typed script APIs and DataShare. A custom IMC
Provider remains native. If scripts must use a native service, expose a small
typed CKAngelScript extension owned by that native plugin.

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
