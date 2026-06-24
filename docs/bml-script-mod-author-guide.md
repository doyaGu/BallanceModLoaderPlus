# BML Script Mod v1 Author Guide

This guide is for people who want to publish a mod that BML+ loads from an
AngelScript file. It starts with the authoring workflow, then moves into the
API reference. `docs/bml-script-mod-api.as` is only an editor/static-checking
stub; do not package it as runtime script code.

## Who This Is For

You can use script mods without knowing the native BML C++ API first. You need
basic C-like syntax and enough AngelScript to read handles such as `T@`,
references such as `const T &in`, and class methods. The AngelScript notes
below explain the parts that matter most for BML.

If you already write native BML mods, read this as the script-facing shape of
the same mod concepts: metadata replaces the native plugin descriptor, fixed
callbacks replace virtual methods, and `BML::ModContext` is the per-mod service
object.

If you already use CKAngelScript, read this as the BML-owned layer on top of
CKAS. Scene objects, behavior graphs, components, raw CK/Vx bindings, messages,
async work, and plugin extension namespaces still belong to CKAngelScript and
its docs. BML adds mod identity, dependencies, resources, config, commands,
logging, UI hooks, exports, DataShare, and load diagnostics.

If you are exposing APIs from a native plugin, this guide tells script authors
how they will consume those APIs. Use the CKAngelScript registration docs for
the native binding work.

## How To Read This Guide

- New script author: read `First Mod`, `Before You Start`, `Entry Rules`,
  `AngelScript Notes`, `Callbacks`, and `The ModContext Object`.
- Native BML author: start at `What BML Adds To CKAngelScript`, then read
  `Callbacks`, `Logger And Config`, `Command And Completion`, `Timer`, and
  `DataShare`.
- CKAngelScript author: start at `What BML Adds To CKAngelScript`, then use the
  task table below to find the BML service you need.
- API lookup: jump to the reference section that owns the feature. The guide is
  not meant to be read straight through on the first pass.

## First Mod

Start from `templates/script-mod-template` or create a single file named
`HelloScript.mod.as` under `ModLoader/Mods`. The filename ending matters:
`*.mod.as` is how BML finds script mod entries.

The smallest useful mod logs one line when it loads:

```angelscript
[bml.mod(id: "example.hello", name: "Hello Script", version: "1.0.0",
         author: "You", description: "Minimal BML script mod",
         bml: "0.3.12")]
class HelloMod {
  void OnLoad(const BML::ModContext &in ctx) {
    BML::Logger@ logger = ctx.BorrowLogger();
    if (logger !is null)
      logger.Info("Hello from BML script");
  }
}
```

Run Ballance with a script-capable BML+ build and check the BML log. If the
line appears, the mod entry compiled, metadata was accepted, and the `OnLoad`
callback ran. From there, add one feature at a time: a config value, a command,
a timer, a DataShare request, a UI draw callback, or CKAngelScript scene work.

After editing an already loaded script source, use `script reload <id>` or
`script reload` for all script mods. If a reload fails, BML keeps the previous
runtime when it can and records the full reason in `ModLoader.log`, `script
logs`, and `script diag <id>`. When a script fails, read those diagnostics
before guessing from symptoms in game.

Keep smoke scripts and author examples separate. The smoke scripts under
`tests/smoke/AngelScript` intentionally exercise edge cases and regression
coverage; they are not minimal examples.

## Before You Start

You need:

- A script-capable BML+ build. `BML_ENABLE_ANGELSCRIPT` controls whether the
  build supports script mods.
- The matching `AngelScript.dll` next to `BMLPlus.dll` in `BuildingBlocks` when
  distributing a script-capable release.
- `docs/bml-script-mod-api.as` in your editor for completion and static checks.
  Do not include this stub from the script at runtime.
- CKAngelScript API docs when the mod touches Virtools objects, behavior
  graphs, components, messages, async work, or raw CK/Vx SDK bindings. Start
  with CKAS `docs/api-inventory.md`, `docs/scene-interop.md`,
  `docs/sdk-bindings.md`, `docs/messaging.md`, and `docs/async.md`.
- Matching CKAngelScript and AngelScript headers only when building a native
  plugin that also exposes script APIs.

To verify that the runtime is present, load Ballance with BML+ and check the
BML log. Missing or incompatible CKAngelScript is reported as a `ckas-host`
diagnostic, and script mods stay unavailable without breaking native-only BML
features.

## Package Shapes

v1 supports three package forms under `ModLoader/Mods`:

- A single `Foo.mod.as` file.
- A directory containing exactly one `*.mod.as` entry.
- A `.zip` package containing exactly one script mod entry.

The entry file must be valid AngelScript. BML does not parse source text; it
compiles the entry through CKAngelScript and reads declaration metadata from
CKAS metadata reflection.

Directory shape:

```text
ModLoader/Mods/HelloScript/
  Hello.mod.as
  runtime.as
  Resources/...
```

Single-file shape:

```text
ModLoader/Mods/HelloScript.mod.as
ModLoader/Mods/HelloScript/
  Resources/...
```

Zip package shape:

```text
HelloScript.zip
  Hello.mod.as
  Resources/...
  ConfigDefaults/...
  README.md
```

Larger directory or zip mod:

```text
MyScriptMod/
  MyScript.mod.as       # the only BML entry
  libs/                 # ordinary AngelScript files included by the entry
  Resources/            # textures, NMO/CMO/VMO assets, text data
  ConfigDefaults/       # optional user-facing defaults you copy/read yourself
  README.md
```

The entry file is the BML mod boundary. Keep metadata, the main class, and the
public `[bml.export]` methods easy to find there. Put implementation helpers in
other files only when your CKAngelScript setup includes them during module
build; do not assume BML will scan arbitrary files as separate mod entries.
Keep helper files inside the owning mod directory, for example
`MyScriptMod/libs/*.as`. Do not use a global `ModLoader/Mods/libs` directory as
a shared script library root; hot reload tracks a single mod's entry and
resource root, not arbitrary global include paths.

`.bmodp` is reserved for native DLL mods. Do not use `.bmodp` for script mods.

## Task Map

| I want to... | Read | Use |
| --- | --- | --- |
| Give my script a mod id, version, author, dependencies, or exports | `Mod Metadata` | `[bml.mod]`, `[bml.require]`, `[bml.optional]`, `[bml.export]` |
| Run code when the mod loads, each tick, during render, or when BML/game events happen | `Callbacks` | `OnLoad`, `OnProcess`, `OnRender`, `OnGameEvent`, event-specific callbacks |
| Log messages or store user settings | `Logger And Config` | `ctx.BorrowLogger()`, `ctx.BorrowConfig()` |
| Add commands or command completion | `Command And Completion` | `BML::Command`, `BML::CommandDefinition`, `BML::CommandCompletion` |
| Delay or repeat work | `Timer` | `BML::Timer`, `ctx.AddTimer()` |
| Draw BML/ImGui UI | `UI`, `Advanced ImGui` | `OnRender`, `ctx.Draw*`, `BML::ImGui::*` |
| Call another mod or expose functions to other mods | `ExportRef And CallFrame` | `ModRef.FindExport()`, `ExportRef.Call*`, `CallFrame` |
| Exchange typed data with native mods or scripts | `DataShare` | `BML::DataShareRequest`, `ctx.RequestDataShare*` |
| Hook an existing behavior graph path | `CK, Physics, And Text Helpers` | CKAS `Behavior`/`BB` lookup plus `ctx.InsertHookBlock*` or `BML::Hook::*` |
| Work with Virtools objects, behavior graphs, or CKDataArray/CKMesh/CKTexture | `What BML Adds To CKAngelScript`, CKAS docs | CKAS `Scene`, `Behavior`, `BB`, `Param`, raw CK/Vx SDK bindings where needed |

## Concepts At A Glance

| Term | Meaning |
| --- | --- |
| Script mod entry | The one `*.mod.as` file that BML compiles as the mod boundary. |
| Main mod class | The AngelScript class marked with `[bml.mod]`. BML creates and calls this class. |
| Metadata | AngelScript attributes such as `[bml.mod]` and `[bml.require]`. These replace a separate manifest file. |
| Callback | A method with an exact name/signature, such as `OnLoad` or `OnRender`, that BML calls at a known point. |
| `BML::ModContext` | The per-mod service object passed into callbacks. Start here for logging, config, commands, resources, timers, DataShare, exports, and BML UI. |
| Borrowed handle | A handle returned for immediate use, usually to CK/Virtools state. Do not keep it as long-term identity unless that section says it is safe. |
| CKAS ref | A CKAngelScript reference object such as an `ObjectRef@`-derived handle. Prefer this for long-lived CK object identity. |
| Export | A script method that other mods can find and call through BML interop. |

## Entry Rules

- The entry filename can be any name ending in `*.mod.as`.
- A directory or zip script package must contain exactly one entry.
- Multiple single-file entries directly under `Mods` are separate script mods.
- The entry is AngelScript source. `[bml.mod]`, `[bml.require]`,
  `[bml.optional]`, and `[bml.export]` are AngelScript metadata, not a separate
  manifest DSL.
- BML does not use a source lexer/parser to infer namespace, class name, or
  exports. Those facts come from CKAngelScript metadata reflection.
- The `[bml.mod]` class may live in any AngelScript namespace. BML records the
  namespace and class name reported by CKAS metadata reflection.

## AngelScript Notes

BML script mods are ordinary AngelScript classes hosted by CKAngelScript. These
language rules matter when reading this guide:

- `T@` is a handle. Use `is null` / `!is null` for null and identity checks.
- Use `@field = @handle` or `@field = expr` when rebinding a handle member. A
  plain assignment without handle syntax can call object assignment instead.
- Script classes are reference types. Passing `HelloCommand()` to
  `RegisterCommand` creates a script object that BML retains on success.
- Value classes such as `BML::ObjectLoadOptions`, `BML::CommandDefinition`, and
  event objects are copied by value. They are safe to store unless they return
  borrowed handles through a method.
- Declarations with `@+` in `bml-script-mod-api.as` mean BML retains the script
  object or delegate when registration succeeds. You still write ordinary
  script code; the marker documents the native ownership transfer.
- BML fixed callbacks run with CKAS no-suspend semantics. Do not call an API
  that suspends from `OnLoad`, `OnProcess`, `OnRender`, command callbacks,
  timer callbacks, DataShare callbacks, or export calls.

Keep interface method signatures exact when implementing `BML::Command`,
`BML::Timer`, and `BML::DataShareRequest`. A missing `const`, `&in`, return
type, or method name means BML cannot bind the script object to the expected
callback.

## What BML Adds To CKAngelScript

BML script mods run inside CKAngelScript, but a BML script mod is only one
script shape: an AngelScript module/class that BML gives mod identity, load
order, callbacks, resources, dependencies, exports, commands, config, logging,
timers, DataShare, and BML UI helpers. It is not a replacement for every CKAS
runtime surface.

The useful surfaces are layered:

- CKAngelScript runtime scripts are long-lived modules discovered from `Scripts`
  roots. They use `[script]` / `[script.depends]`, `ScriptContext`, `Runtime::*`,
  `Message::*`, and `Async::*`.
- `AngelScript Component` is a Building Block that attaches one script class to
  one behavior instance. It uses `CKBehaviorContext`, component metadata,
  injected fields, `BBConfig@`, `BBSlot@`, `BehaviorRef@`, and `ParamTypeInfo@`.
- CKAngelScript scene/behavior APIs such as `Scene`, `Behavior`, `BB`, and
  `Param` cover Virtools object identity, scene membership, behavior graph
  search/editing, runtime Building Block instances, parameter values, sources,
  and operations.
- Raw CK/Vx SDK bindings are available when the high-level CKAS API does not
  cover a direct engine call. Resolve raw CK pointers near the operation; do
  not store them as durable script identity.
- `NativePointer`, `NativeBuffer`, DynCall/DynLoad/DynCallback, and writable
  native memory are advanced escape hatches for proven SDK/FFI work, not the
  normal way to design mod interoperability.
- Other native plugins may expose script APIs by registering CKAngelScript
  engine extensions. Treat those APIs as plugin-owned surfaces, not as BML
  facade methods.

Do not measure script mod capability only by the BML facade. For Virtools scene
objects, behavior graphs, Building Blocks, parameters, CKDataArray, CKMesh,
CKTexture, cameras, render contexts, and math types, prefer CKAngelScript's
high-level API first, then raw CK/Vx SDK bindings when a direct SDK operation
is needed. Use BML APIs when the operation is about BML mod semantics or BML
services.

`[bml.require]` and `[bml.optional]` are BML mod dependencies. CKAngelScript
runtime scripts also have their own `[script.depends]` metadata, but that is
not the BML mod dependency graph.

## Choosing The Right Script Surface

| Goal | Best surface | Reason |
| --- | --- | --- |
| Publish a mod with BML identity, dependency ordering, config, commands, logs, HUD/menu UI, resources, exports, or DataShare | BML script mod | These are BML-owned mod semantics. |
| Patch or inspect a level object, data array, material, mesh, texture, camera, or scene membership | BML script mod plus CKAS `Scene`/raw CK APIs, or a CKAS runtime script when no BML mod identity is needed | BML can orchestrate the mod; CKAS should own Virtools object lookup and identity. |
| Add per-object or per-behavior logic inside an existing behavior graph | `AngelScript Component` | It naturally receives `CKBehaviorContext` and editor/component metadata injection. |
| Search, edit, or drive behavior graphs and Building Blocks | CKAS `Behavior`, `BB`, and `Param`, often from a component; a BML script mod can coordinate by borrowing `CKContext@` where CKAS overloads allow it | The graph and parameter model is CKAS-owned, not a BML facade clone. |
| Communicate between scripts/components or schedule frame-based work | CKAS `Message` and `Async`; use BML exports/DataShare only for stable mod-visible contracts | CKAS owns script runtime communication, while BML owns mod-level interop. |
| Hook engine internals, replace performance-critical loops, patch renderer/input internals, or expose a native service | Native plugin plus a guarded CKAS engine extension; script controls policy/config | Native code owns unsafe lifetime and performance boundaries. |

BML callbacks receive `BML::ModContext`, not `ScriptContext` or
`CKBehaviorContext`. Use `ctx.BorrowCKContext()` for CKAS helpers that explicitly
accept `CKContext@`. Runtime scripts should pass `ScriptContext`; AngelScript
Components should pass `CKBehaviorContext`. Do not fake one context as another.
If a missing context blocks a real use case, add a small native/CKAS extension at
the owner boundary instead of smuggling raw engine objects through BML.

## Common Project Shapes

- Pure BML script mod: good for commands, config-driven toggles, HUD/menu
  changes, DataShare, exports, resource loading, and light level scripting.
- BML orchestrator plus CKAS components: BML owns mod configuration and public
  interop; components live on behavior instances and execute per-object logic.
- Native core plus script policy: native code owns hooks, renderer/input patches,
  or high-frequency loops; AngelScript exposes safe knobs, scripts, and
  callbacks through a CKAS extension.
- Script service bus: CKAS `Message`/`Async` connects runtime scripts and
  components; BML exports/DataShare expose the stable mod-facing service.

## Mod Metadata

`bml.mod` declares the main script mod class. It is required and must appear
exactly once.

```angelscript
[bml.mod(id: "example.core", name: "Example Core", version: "1.2.0",
         author: "You", description: "Example", bml: "0.3.12")]
class ExampleCore {
}
```

Required arguments are `id`, `name`, and `version`. Optional arguments are
`author`, `description`, and `bml`. The `bml` value is the minimum BML+ version
the script expects, not the script's own version. Unknown `bml.*` metadata tags
are metadata errors. Non-BML metadata is ignored by BML and remains available to
other CKAngelScript tooling.

Use a stable reverse-DNS-style or owner-prefixed id such as
`author.example.mod`. Changing the id creates a different mod from BML's point
of view and breaks dependency/export lookups that target the old id.

Dependencies are also metadata. They participate in BML's dependency graph;
scripts cannot mutate dependencies at runtime.

```angelscript
[bml.require(id: "other.mod", version: "1.0.0")]
[bml.optional(id: "debug.helper", version: "0.1.0")]
[bml.mod(id: "example.dep", name: "Example With Deps", version: "1.0.0",
         author: "You", description: "Dependency example", bml: "0.3.12")]
class ExampleWithDeps {
}
```

`bml.require` means the dependency must be present and satisfy the optional
minimum version. `bml.optional` records a soft dependency that can be queried
through `ModRef`, but it does not block loading when absent. Dependency metadata
does not replace CKAngelScript `[script.depends]`; that belongs to CKAS runtime
scripts.

Exports are methods marked with `[bml.export]`. The public signature supports
the scalar types used by the typed `ExportRef.Call*` helpers and the wider
`CallFrame` value set documented later in this guide. Parameter names are not
part of the contract.

```angelscript
class ExportExample {
  [bml.export(name: "Add", signature: "int(int, int)")]
  int Add(int a, int b) {
    return a + b;
  }

  [bml.export(name: "Greeting", signature: "string(string)")]
  string Greeting(const string &in name) {
    return "Hello " + name;
  }
}
```

If the `name` argument is omitted, the method name is used. The export
signature is inferred from the compiled method declaration. Duplicate
`name + signature` pairs are rejected; overloading the same export name with
different supported signatures is allowed, but callers must pass the signature
when lookup would otherwise be ambiguous.

## Hot Reload

Hot reload replaces the script runtime for a mod that BML already discovered at
startup. It is meant for edit/test cycles, not for changing the mod graph while
the game is running.

Commands:

```text
script reload              # reload all script mods
script reload <id>         # reload one script mod
script reload <id> --dry-run
script reload <id> --force-exports
script diag <id>
script logs error
```

Expected behavior:

- A compile or metadata failure keeps the old runtime active when one exists.
  The in-game message only says reload failed; use `script diag <id>` or the
  Logs tab for the structured compiler messages.
- BML captures an in-memory source snapshot at the start of each reload attempt.
  Validation and commit use that same candidate runtime, so saving the script
  again while reload is running cannot switch the committed source underneath
  the validator.
- `--dry-run` does not call `SaveState`, `MigrateState`, `RestoreState`, or
  candidate `OnLoad`. It validates compilation, metadata, compatibility, and
  required state hook declarations only.
- If the mod failed during initial startup, BML keeps a failed placeholder. Fix
  the file, then run `script reload` or `script reload <placeholder-id>` to
  recover it. The placeholder can promote to the real mod id when that id does
  not conflict.
- Dropping a brand-new `*.mod.as` file into `ModLoader/Mods` after startup does
  not load a new mod. Restart Player to discover new mod registry nodes.
- Changing a successfully loaded mod id requires restart. Changing dependency
  declarations, adding required dependencies, or requiring a new dependency
  graph node is rejected because BML does not reorder the graph at runtime.
- Hot reload does not cascade into dependent mods. If a dependent mod now
  requires a version that the candidate no longer satisfies, reload is rejected.
  Restart or reload the affected dependent mods explicitly.
- Existing exports are compatibility promises. Normal reload requires every old
  export `name + signature` to remain present; new exports are allowed. Use
  `--force-exports` only for manual reloads where you know callers can tolerate
  stale/missing exports.
- Old Timer, Command, DataShare request, callback, and export handles become
  invalid or stale after replacement. New registrations from the new `OnLoad`
  are the only active resources.
- A script mod can detect reload lifecycle callbacks through `ModContext`:
  `ctx.IsReloading` is true only while BML is calling script lifecycle methods
  for hot reload. `ctx.ReloadPhase` is one of `BML::RELOAD_UNLOAD`,
  `BML::RELOAD_LOAD`, `BML::RELOAD_ROLLBACK`, `BML::RELOAD_RECOVERY`, or
  `BML::RELOAD_CLEANUP`. State migration hooks report the finer
  `BML::RELOAD_SAVE_STATE`, `BML::RELOAD_MIGRATE_STATE`, and
  `BML::RELOAD_RESTORE_STATE` phases. Normal startup and normal game shutdown
  use `BML::RELOAD_NONE`.
- Use `ctx.ReloadPhase` in `OnUnload` to distinguish final shutdown from old
  runtime replacement. Use `BML::RELOAD_ROLLBACK` in `OnLoad` to avoid assuming
  a failed candidate has left the game world untouched.
- Script object fields are not preserved automatically. To preserve them, add
  hot reload state hooks:

```angelscript
int counter = 0;
string mode = "idle";

void SaveState(BML::StateBag@ state) {
  state.SetInt("counter", counter);
  state.SetString("mode", mode);
}

void MigrateState(const string &in fromVersion, BML::StateBag@ state) {
  if (fromVersion == "1.0.0" && state.Has("oldMode")) {
    state.SetString("mode", state.GetString("oldMode", "idle"));
    state.Remove("oldMode");
  }
}

void RestoreState(BML::StateBag@ state) {
  counter = state.GetInt("counter", counter);
  mode = state.GetString("mode", mode);
}
```

  BML calls `SaveState` on the old runtime before `OnUnload`. If state was
  saved, the old runtime must declare `RestoreState(BML::StateBag@)` so rollback
  can restore the old script object without running migration code. The
  candidate must declare either `RestoreState(BML::StateBag@)` or
  `MigrateState(const string &in fromVersion, BML::StateBag@)`, otherwise reload
  is rejected before the old runtime is unloaded. On the candidate, BML calls
  `MigrateState` first, then `RestoreState`, then `OnLoad`. Rollback uses a
  clone of the original state bag and calls only old `RestoreState` before
  rollback `OnLoad`.
- `StateBag` is intentionally limited to `bool`, `int`, `float`, and `string`.
  Do not store AngelScript objects, callbacks, `ModRef`, `ExportRef`, CK handles,
  timers, commands, or DataShare requests in it; those resources are rebound
  through the normal `OnLoad` path.
- A `StateBag` passed by BML to `SaveState`, `MigrateState`, or `RestoreState`
  reports `state.IsReloadState == true` and is transient. BML enables it only
  while it is calling that hook. If a script stores that `StateBag@` in a field
  and tries to use it later, the script-visible API behaves as empty and ignores
  mutations. A script-created `BML::StateBag()` is just a normal script-owned
  primitive/string container and is not a hot reload migration object.
- State hooks are restricted phases. They may copy primitive/string values
  through `StateBag`, read context information, and log diagnostics. They must
  not register timers, commands, hooks, DataShare requests, or irreversible
  content; they must not execute commands, write DataShare/config values, mutate
  input state, call `ExportRef`/native exports, or change CK/game-world objects.
  Rebuild resources in `OnLoad` after pure state has been restored.
- Rollback restores only resources BML owns: callbacks, exports, timers,
  commands, DataShare requests, and script runtime handles. It cannot undo
  changes your script already made to the game world through CKAS Scene/BB APIs,
  raw CK/Vx calls, or another plugin. If a feature mutates world state during
  `OnLoad`, make it explicitly reversible or require a restart after failure.

```angelscript
void OnUnload(const BML::ModContext &in ctx) {
  if (ctx.ReloadPhase == BML::RELOAD_UNLOAD) {
    // Hot reload is replacing this runtime. Clean BML/CK resources that the
    // script owns, but do not treat this as final game shutdown.
    return;
  }

  // Normal unload or game shutdown.
}

void OnLoad(const BML::ModContext &in ctx) {
  if (ctx.ReloadPhase == BML::RELOAD_ROLLBACK) {
    // The new candidate failed and BML restored the old runtime. World state
    // touched by the failed candidate may still need manual repair or restart.
  }
}
```

## Callbacks

Old `OnPre*` / `OnPost*` overload families are not part of the v1 contract.
Only these fixed signatures are recognized:

```angelscript
void OnLoad(const BML::ModContext &in ctx);
void OnUnload(const BML::ModContext &in ctx);
void OnProcess(const BML::ModContext &in ctx);
void OnRender(const BML::ModContext &in ctx, const BML::RenderEvent &in event);
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event);
void OnCheatEnabled(const BML::ModContext &in ctx, const BML::CheatEvent &in event);
void OnLoadObject(const BML::ModContext &in ctx, const BML::LoadObjectEvent &in event);
void OnLoadScript(const BML::ModContext &in ctx, const BML::LoadScriptEvent &in event);
void OnCommandEvent(const BML::ModContext &in ctx, const BML::CommandEvent &in event);
void OnModifyConfig(const BML::ModContext &in ctx, const BML::ConfigEvent &in event);
void OnPhysicalize(const BML::ModContext &in ctx, const BML::PhysicalizeEvent &in event);
void OnUnphysicalize(const BML::ModContext &in ctx, const BML::ObjectEvent &in event);
```

`OnGameEvent` uses the `BML::GameEvent` enum for no-payload game events.
`CommandEvent.Phase` distinguishes pre-execute, post-execute, and completion.
`OnModifyConfig` exposes the changed category/key and a `ConfigProperty@`
borrowed through the normal OOP config facade.
Event objects copy scalar/string/list payloads, so storing an event object keeps
only that snapshot. Handles returned by `Borrow*` methods are still borrowed CK
handles and should be used immediately.

### Callback And Event Reference

| Callback | When to use it | Payload notes |
| --- | --- | --- |
| `OnLoad` | Initialize config, commands, timers, DataShare requests, exports, and content registration. | No event object. Failures here fail the script mod load. |
| `OnUnload` | Cancel optional work, unregister commands explicitly when desired, and clear script-side state. | BML also cleans up script-owned resources. |
| `OnProcess` | Light per-tick orchestration. | Avoid expensive scene scans; cache durable ids/refs and revalidate near use. |
| `OnRender` | BML UI and ImGui drawing. | `RenderEvent.Flags` is a snapshot of the render flags. Frame-scope handles from ImGui must not be stored. |
| `OnGameEvent` | Respond to BML/game lifecycle events such as level load, reset, pause, finish, and ball/nav state. | The payload is a `BML::GameEvent` enum. Use `BML::GetGameEventName(event)` for logs. |
| `OnCheatEnabled` | Track cheat state changes. | `CheatEvent.Enabled` is a snapshot. |
| `OnLoadObject` | Observe Object Load operations. | Object ids and load options are copied. `BorrowObject` and `BorrowMasterObject` resolve borrowed CK handles on demand. |
| `OnLoadScript` | Observe loaded Virtools behavior scripts. | `ScriptId` is stable as an id snapshot; `BorrowScript()` is a borrowed `CKBehavior@`. |
| `OnCommandEvent` | Observe command execution phases globally for this mod. | `CommandName`, args, `ArgsText`, phase flags, and cheat flag are snapshots. |
| `OnModifyConfig` | React to config edits. | Category/key/type are copied. `BorrowProperty()` returns a BML `ConfigProperty` wrapper and same-key reentry is suppressed. |
| `OnPhysicalize` | Observe physics physicalization requests. | Target id/name, scalar settings, mesh ids, ball centers/radii are copied. Borrowed target/mesh handles may be null later. |
| `OnUnphysicalize` | Observe physics unphysicalization requests. | Target id/name are copied. `BorrowTarget()` is a borrowed CK handle. |

Use event snapshots for logging, decisions, and delayed work. For delayed CK
object work, store `CK_ID` values or CKAS `ObjectRef@`-derived handles and
resolve raw CK handles only when the operation runs.

## The ModContext Object

`ModContext` is the main per-mod facade. Prefer the callback parameter over
global helper functions when both exist.

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
  BML::Logger@ logger = ctx.BorrowLogger();
  if (logger !is null)
    logger.Info("id=" + ctx.ModId);

  BML::Config@ config = ctx.BorrowConfig();
  if (config !is null) {
    BML::ConfigProperty@ greeting = config.GetProperty("General", "Greeting");
    if (greeting !is null)
      greeting.SetString("hello");
  }

  ctx.RegisterCommand(HelloCommand());
  ctx.AddTimer(HeartbeatTimer());
}
```

Common capabilities:

- Per-mod `Logger` and `Config` objects.
- Resource path resolution.
- BML/game state, time, input, HUD, menu, and message helpers.
- Mod registry queries such as `FindMod`, `GetModCount`, and `GetMod`.
- Script-owned Timer and Command registration.
- Typed DataShare read/request helpers.
- Typed export lookup and calls through `ModRef`, `ExportRef`, and `CallFrame`.

`BorrowLogger()` and `BorrowConfig()` return BML service wrappers that may be
stored as handles, but every call still revalidates the owning script mod. CK
and manager handles returned by other `Borrow*` APIs are escape hatches; use
them during the current callback and do not store them in long-lived script
fields.

## Resources And Paths

Use `ModContext` path helpers for files owned by the script mod:

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
  string root = ctx.GetModRootUtf8();
  string text = ctx.ReadModTextFileUtf8("Resources/readme.txt", "");
  if (ctx.ModFileExistsUtf8("Resources/settings.txt"))
    ctx.BorrowLogger().Info("resource root=" + root + " text=" + text);
}
```

`ResolveModPathUtf8(relativePath)` resolves a path inside the mod root.
`ModFileExistsUtf8`, `ModDirectoryExistsUtf8`, and `ReadModTextFileUtf8` are
guarded for script mod resources. Use `BML::Path` for pure path manipulation:
`Combine`, `Normalize`, `FileName`, `Extension`, `RemoveExtension`,
`IsAbsolute`, `IsRelative`, `Exists`, `IsFile`, and `IsDirectory`.

Directory constants such as `DIR_GAME`, `DIR_LOADER`, `DIR_CONFIG`, and
`DIR_TEMP` are available through `GetDirectoryUtf8(type)` when a script needs a
known BML path. Prefer mod-relative paths for assets so zip, directory, and
single-file packages behave the same.

## Logger And Config

`Logger`, `Config`, and `ConfigProperty` are BML service wrappers. They are
script handles with `IsValid`; each call resolves the owning mod again. It is
fine to store them as handles, but still check `IsValid` when work can run after
unload or failure.

```angelscript
class Settings {
  BML::ConfigProperty@ greeting;
  BML::ConfigProperty@ enabled;

  void Load(const BML::ModContext &in ctx) {
    BML::Config@ config = ctx.BorrowConfig();
    if (config is null)
      return;

    @greeting = config.GetProperty("General", "Greeting");
    @enabled = config.GetProperty("General", "Enabled");
    if (greeting !is null) {
      greeting.SetDefaultString("hello");
      greeting.SetComment("Text printed by the hello command.");
    }
    if (enabled !is null)
      enabled.SetDefaultBoolean(true);
    config.SetCategoryComment("General", "Basic script mod settings.");
  }
}
```

`GetProperty(category, key)` creates or returns the mod's property facade. Use
typed getters with defaults: `GetString`, `GetBoolean`, `GetInteger`,
`GetFloat`, and `GetKey`. Use typed setters and default setters for values.
`OnModifyConfig` fires when a property changes; same-property recursive edits
are suppressed to prevent accidental infinite loops.

## Input, Game State, And HUD

Input lives on `InputHook`, reached through `ctx.BorrowInputManager()`.
`InputHook` is a borrowed BML facade over the native input hook; use it during
callbacks and do not store it as durable state.

```angelscript
void OnProcess(const BML::ModContext &in ctx) {
  BML::InputHook@ input = ctx.BorrowInputManager();
  if (input !is null && input.IsKeyPressed(CKKEY_F5)) {
    ctx.SendIngameMessage("F5 pressed by script");
  }
}
```

Useful `ModContext` state and HUD helpers include:

- `IsInGame`, `IsInLevel`, `IsPaused`, `IsPlaying`, and `IsCheatEnabled`.
- `EnableCheat`, `ExitGame`, `SendIngameMessage`, `ClearIngameMessages`,
  `ExecuteCommand`, and `SkipRenderForNextTick`.
- `GetSRScore`, `GetHSScore`, `GetHUD`, `SetHUD`, `ShowTitle`, `ShowFPS`,
  `ShowSRTimer`, `StartSRTimer`, `PauseSRTimer`, `ResetSRTimer`, and
  `GetSRTime`.
- `OpenModsMenu`, `CloseModsMenu`, `OpenMapMenu`, and `CloseMapMenu`.

`ModContext` keeps these methods for compatibility, but new code should prefer
the mod-to-mod capability facade when it does not need per-mod state:
`BML::UI::SendMessage`, `BML::UI::ClearMessages`,
`BML::Menu::OpenModsMenu`, `BML::Menu::CloseModsMenu`,
`BML::Menu::OpenMapMenu`, `BML::Menu::CloseMapMenu`, and
`BML::HUD::*`. These functions are backed by the same Interop exports that
native mods can import from the built-in `BML` mod, so scripts and native mods
use the same public capability contract.

Guard mutating calls with state checks when they depend on a loaded level. BML
returns defaults or no-ops when the underlying runtime is unavailable, but a
script is easier to debug when the state check is explicit.

## Content Registration

`RegisterBallType`, `RegisterFloorType`, and the `RegisterModule` overloads
mirror BML's native content registration surface for common Ballance modding
tasks. Call them from `OnLoad`; late registration returns `false`.

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
  BML::BallTypeDefinition ball;
  ball.BallId = "example.ball";
  ball.BallName = "Example Ball";
  ball.BallFile = "Resources/example_ball.nmo";
  ball.ObjectName = "Example_Ball";
  ball.Friction = 0.4f;
  ball.Elasticity = 0.2f;
  ball.Mass = 1.0f;
  ball.Radius = 2.0f;
  if (!ctx.RegisterBallType(ball))
    ctx.BorrowLogger().Warn("failed to register ball type");
}
```

Use `BallTypeDefinition`, `FloorTypeDefinition`, `ModuleBallDefinition`,
`ModuleConvexDefinition`, `TrafoDefinition`, and `ModuleDefinition` as
value-only declarations. Keep asset paths mod-relative where possible.

## CK, Physics, And Text Helpers

For general Virtools work, start with CKAngelScript's own APIs:

- `Scene::*` for lookup, creation, scene membership, safe object refs, and
  guarded destruction.
- `Behavior::*`, `BB::*`, and `Param::*` for behavior graph search/editing,
  Building Block spawning/stepping, parameters, sources, and operations.
- Raw CK/Vx SDK bindings for the final engine call when a high-level helper
  does not cover the operation.

Use `ctx.Borrow*` for BML-owned convenience access to CK managers, named CK
object lookup, and per-mod BML services. CK and manager handles returned by
those calls are borrowed from Virtools. They are valid only while the owning
level object still exists. When CKAS `ObjectRef@` / `Entity3DRef@` /
`BehaviorRef@` handles fit the job, prefer them for durable script-side identity
and reacquire raw CK handles only near the operation.

`BML::CK` contains stateless helpers for borrowed CK handles. Null handles
return defaults or no-op. It is a convenience layer, not the complete CKAS
Virtools API:

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
  CKDataArray@ array = ctx.BorrowDataArrayByName("Some_Array");
  int column = BML::CK::FindColumn(array, "Name");
  string value = BML::CK::GetString(array, 0, column, "");

  BML::ObjectLoadOptions options;
  options.File = "3D Entities\\Example.nmo";
  BML::ObjectLoadResult@ loaded = BML::CK::LoadObject(options);
  CKObject@ main = loaded is null ? null : loaded.BorrowMainObject();
}
```

For durable scene identity, prefer CKAngelScript refs:

```angelscript
Entity3DRef@ ball;

void OnLoad(const BML::ModContext &in ctx) {
  CKContext@ ck = ctx.BorrowCKContext();
  @ball = Scene::FindEntity3D(ck, "Ball");
}

void OnProcess(const BML::ModContext &in ctx) {
  if (ball is null || !ball.valid)
    return;

  CK3dEntity@ entity = ball.Entity3D();
  if (entity !is null)
    entity.Translate(VxVector(0.0f, 0.0f, 0.1f));
}
```

Use CKAS `Scene::*` for lookup, creation, scene membership, stale-id
protection, and dynamic destruction. Use raw CK/Vx methods only near the actual
operation. Use CKAS `Behavior`, `BB`, and `Param` when the task is behavior
graph or parameter work. Use CKAS `Message` for communication with runtime
scripts or components.

CKAS `Async` is available to runtime scripts and components, but BML fixed
callbacks are no-suspend. Do not call `Await` from a BML callback. If a BML mod
needs delayed work, use BML timers or delegate the suspending work to a CKAS
runtime script/component and communicate through `Message` or BML exports.

### Hook Block

Use BML Hook Block when a script mod needs a stable callback point inside an
existing Virtools behavior graph. BML owns the native Hook Block instance,
retains the AngelScript delegate, and removes or restores its inserted links on
mod unload. CKAS still owns graph discovery: use CKAS `Behavior`, `BB`, and
`Param` helpers to find the owner script and Building Blocks you want to hook.

```angelscript
BML::HookBlockRef@ hook;

CKBehavior@ FindSubBehaviorByName(CKBehavior@ owner, const string &in name) {
  if (owner is null)
    return null;

  const int count = owner.GetSubBehaviorCount();
  for (int i = 0; i < count; ++i) {
    CKBehavior@ child = owner.GetSubBehavior(i);
    if (child !is null && child.GetName() == name)
      return child;
  }

  return null;
}

int OnBallHook(const BML::ModContext &in ctx,
               const BML::HookBlockEvent &in event) {
  BML::Logger@ log = ctx.BorrowLogger();
  if (log !is null)
    log.Info("Hooked " + event.BlockName);

  return CKBR_OK;
}

void OnLoad(const BML::ModContext &in ctx) {
  CKBehavior@ owner = ctx.BorrowScriptByName("Gameplay_Ingame");
  CKBehavior@ source = FindSubBehaviorByName(owner, "Some_Behavior");
  if (owner is null || source is null)
    return;

  @hook = ctx.InsertHookBlockAfter(owner, source, OnBallHook, "example hook");
  if (hook is null)
    ctx.BorrowLogger().Warn("hook install failed");
}
```

`CreateHookBlock` creates an unattached Hook Block under an owner behavior
script. `InsertHookBlockAfter`, `InsertHookBlockBefore`, and
`InsertHookBlockBetween` are convenience helpers for the common one-input,
one-output graph patches. If a graph has multiple matching links, BML patches
the first matching link; find a more specific source/target with CKAS when the
graph is ambiguous.

By default, the Hook Block activates all outputs after the callback, matching
the native Hook Block behavior. Set `HookBlockRef.AutoActivateOutputs = false`
when the script should choose the outgoing branch itself:

```angelscript
int RouteHook(const BML::ModContext &in ctx,
              const BML::HookBlockEvent &in event) {
  if (ShouldContinue())
    event.ActivateOutput(0);
  return CKBR_OK;
}

void OnLoad(const BML::ModContext &in ctx) {
  @hook = ctx.InsertHookBlockBetween(owner, from, to, RouteHook, "route");
  if (hook !is null)
    hook.AutoActivateOutputs = false;
}
```

Store `HookBlockRef@` only when the script may later disable or uninstall the
hook. Otherwise, it is safe to ignore the returned handle; BML still owns the
installed hook until the script mod unloads. Do not store `HookBlockEvent`
borrowed `CKBehavior@` handles across callbacks; store CKAS refs or durable ids
when later work needs to find the same graph objects.

`BML::Physics` wraps runtime `ExecuteBB` physics actions. These helpers return
`false` when BML is not in a loaded level or the target is null. It does not
expose PhysicsRT private manager types such as `CKIpionManager`.

```angelscript
void MakeBallPhysical(const BML::ModContext &in ctx, CK3dEntity@ target) {
  BML::PhysicalizeDefinition def;
  def.Fixed = false;
  def.Friction = 0.4f;
  def.Elasticity = 0.2f;
  def.Mass = 1.0f;
  def.CollisionGroup = "Ball";
  def.EnableCollision = true;

  if (!BML::Physics::PhysicalizeBall(target, def, VxVector(0.0f, 0.0f, 0.0f), 2.0f))
    ctx.BorrowLogger().Warn("physicalize failed");
}
```

`BML::Text` creates a `2D Text` behavior under an owner script and returns a
borrowed `CKBehavior@`. Pass materials as explicit borrowed arguments when
needed; keep `Text2DDefinition` value-only.

```angelscript
CKBehavior@ CreateHudText(const BML::ModContext &in ctx,
                          CKBehavior@ owner,
                          CK2dEntity@ target) {
  BML::Text2DDefinition text;
  text.Font = BML::FONT_GAME_NORMAL;
  text.Text = "Hello";
  text.Align = 0;
  text.Offset = Vx2DVector(0.0f, 0.0f);
  CKBehavior@ behavior = BML::Text::Create2DText(owner, target, text);
  if (behavior is null)
    ctx.BorrowLogger().Warn("2D Text creation failed");
  return behavior;
}
```

## UI

`BML::UI` contains two groups. `SendMessage` and `ClearMessages` are core
capabilities and may be called from normal callbacks. The BML-style menu
controls below are render-time Bui helpers; use them from `OnRender`. Drawing
and input calls outside a render callback are safe no-ops or return default
values. The facade intentionally does not expose `Bui::Window`, `Bui::Page`,
raw draw lists, raw text buffers, resource initialization, or script/menu
transition internals.

```angelscript
bool enabled = true;
int count = 3;
string search = "";

void OnRender(const BML::ModContext &in ctx, const BML::RenderEvent &in event) {
  BML::UI::Title("Example Script");
  BML::UI::WrappedText("Use BML::UI for simple menu controls.", 360.0f);

  BML::UI::SetCursorCoord(0.4f, 0.35f);
  if (BML::UI::MainButton("Click"))
    ctx.BorrowLogger().Info("clicked");

  BML::UI::YesNoButton("Enabled", enabled);
  BML::UI::InputIntButton("Count", count);
  BML::UI::SearchBar(search);
}
```

## Advanced ImGui

`ImGui` is the lower-level frame-scope API for scripts that need custom debug
or tooling UI. It is generated from BML's cimgui headers and documented in
`docs/bml-imgui-api.as`; the generation report is
`src/AngelScript/generated/BMLImGuiAngelScriptBindings.report.md`.

Use it only from `OnRender`. BML guards calls outside an active ImGui frame so
they return default values or no-op, but those calls are still script errors in
practice. Borrowed handles such as `ImDrawList@`, `ImGuiIO@`, and `ImGuiStyle@`
are valid only during the current frame and must not be stored.

The generated API intentionally omits context/platform lifecycle, allocators,
raw callbacks, raw `void*`, internal debug helpers, and native resource
lifecycle. Prefer `BML::UI` for ordinary menu buttons and page navigation; use
`ImGui` for advanced windows, tables, trees, controls, images, and frame-local
draw list work.

```angelscript
ImVec4 accent(0.2f, 0.4f, 0.9f, 1.0f);
int choice = 0;
string filter = "";

void OnRender(const BML::ModContext &in ctx, const BML::RenderEvent &in event) {
  if (ImGui::Begin("Advanced ImGui")) {
    ImGui::TextUnformatted("Custom script UI");
    ImGui::InputText("Filter", filter, 128);
    ImGui::ComboText("Mode", choice, "Normal\nDebug\nVerbose");
    ImGui::ColorPicker4("Accent", accent);

    ImDrawList@ draw = ImGui::GetForegroundDrawList();
    if (draw !is null)
      draw.AddQuad(ImVec2(16, 16), ImVec2(96, 16), ImVec2(96, 96), ImVec2(16, 96), 0xffffffff);
  }
  ImGui::End();
}
```

## ExportRef And CallFrame

Use `ModContext.FindMod`, `GetModCount`, and `GetMod(index)` to inspect native
and script mods known to BML. `ModRef` is a facade handle, not a native `IMod*`.
It exposes metadata, state, dependencies, diagnostics, and exports.

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
  BML::ModRef@ other = ctx.FindMod("example.provider");
  if (other is null || !other.IsValid) {
    ctx.BorrowLogger().Warn("provider missing");
    return;
  }

  if (other.IsFailed)
    ctx.BorrowLogger().Warn(other.Id + " failed: " + other.Diagnostic);
}
```

The built-in `BML` mod publishes UI/HUD/menu capabilities through the same
Interop registry. Dynamic callers may use nameless descriptors:

```angelscript
BML::ModRef@ bml = ctx.FindMod("BML");
BML::ExportRef@ sendMessage;
if (bml !is null && bml.TryFindExport("ui.message.add", sendMessage, "void(string)") == BML::ERROR_OK) {
  BML::CallFrame@ frame = BML::CallFrame();
  frame.SetString(0, "Hello from Interop");
  sendMessage.Call(frame);
}
```

Use metadata dependencies when load order matters. Use `FindMod` and state
checks when the relationship is optional at runtime. `CheckDependencies()`
returns BML status codes; `GetDependency*` methods expose the reflected
dependency list so scripts can explain missing optional features to users.

For mod-to-mod communication, choose the smallest stable contract:

- Use exports when the caller needs a request/response function call with typed
  arguments and a status code.
- Use DataShare when the data is state-like and can be read later by name.
- Use CKAS `Message` when the other participant is a CKAS runtime script or
  component rather than a BML mod.

Prefer typed export calls:

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
  BML::ModRef@ other = ctx.FindMod("example.provider");
  if (other is null || !other.IsValid)
    return;

  // The signature can be omitted when the export name is unique.
  BML::ExportRef@ sum;
  int lookup = other.TryFindExport("Add", sum);
  if (lookup == BML::ERROR_OK && sum !is null && sum.IsValid) {
    BML::CallFrame@ frame = BML::CallFrame();
    frame.SetInt(0, 2);
    frame.SetInt(1, 40);
    int status = sum.Call(frame);
    int value = 0;
    if (status == 0)
      frame.GetResultInt(value);
    ctx.BorrowLogger().Info("sum status=" + status + " value=" + value);
  } else if (lookup == BML::ERROR_INTEROP_EXPORT_AMBIGUOUS) {
    ctx.BorrowLogger().Warn("Add has multiple signatures; pass the signature explicitly.");
  }
}
```

Use `CallFrame` for dynamic argument lists:

```angelscript
BML::CallFrame@ frame = BML::CallFrame();
frame.SetString(0, "Ballance");
array<int> values = {1, 2, 3};
frame.SetArray(1, values);
int status = exportRef.Call(frame);
string result;
if (status == 0)
  frame.GetResultString(result);
```

`CallFrame` exposes `ArgCount`, `GetArgType(index)`, `ClearArg(index)`,
`ResultType`, and `ClearResult()` for reusable dynamic frames. Type values are
the `BML::CallValueType` enum: empty, bool, int, float, string, scalar arrays,
buffer, and object id. Native Interop stores copies of array, string-array, and
buffer values; native callers can also use the `Borrow*` CallFrame APIs for
zero-copy read views that stay valid only until the next frame mutation, call,
clear, or destroy. Native code that is already descriptor-driven should prefer
the generic `BML_CallFrame_SetValue`, `BML_CallFrame_BorrowValue`,
`BML_CallFrame_SetResultValue`, and `BML_CallFrame_BorrowResultValue` APIs.
They cover scalar values, scalar arrays, `array<uint8>` buffers, string arrays,
and object ids. Object values cross Interop as `CK_ID` identity; `CKObject@`
handles are resolved only while reading/writing the frame or dispatching a
script call.

Each `ExportRef.Call*` and `ExportRef.Call(frame)` clears the frame result
before dispatch. A failed call or `void` export leaves
`ResultType == BML::CALL_VALUE_EMPTY`; do not reuse an old result after a
non-zero status.

`ExportRef` is generation-checked. After owner unload or export unregister,
old handles do not call stale runtime state and return
`BML::ERROR_INTEROP_HANDLE_STALE`. `TryFindExport` returns
`BML::ERROR_INTEROP_TARGET_NOT_FOUND`, `ERROR_INTEROP_TARGET_FAILED`,
`ERROR_INTEROP_EXPORT_NOT_FOUND`, `ERROR_INTEROP_EXPORT_AMBIGUOUS`,
`ERROR_INTEROP_BAD_SIGNATURE`, or `ERROR_INTEROP_SIGNATURE_MISMATCH` for lookup
failures.

Interop signatures support:

- scalars: `void`, `bool`, `int`, `float`, `string`, `const string &in`
- array params: `const array<bool/int/float/string> &in`
- buffer params: `const array<uint8> &in`
- object params: `CKObject@`
- array returns: `array<bool/int/float/string>@`
- buffer returns: `array<uint8>@`
- object returns: `CKObject@`

`array<uint8>` maps to the native buffer APIs. CKAngelScript reflection may
print arrays as `T[]`; BML treats `T[]@` and `const T[]&in` as equivalent to
`array<T>@` and `const array<T> &in`. Nested arrays, dictionaries,
script class handles, arbitrary `T@`, raw pointers, `&out`, and `&inout` are not
valid Interop signatures. Signatureless lookup is valid only when an export name
is unique; when a mod exports the same name with multiple signatures, pass the
explicit signature. Explicit lookup compares compiled type descriptors, so
parameter names, function names, nameless descriptors such as `void(string)`,
and equivalent string forms do not affect resolution. Info APIs still return
the public signature text that the provider registered.

## Timer

Timers are owned by the script mod. BML cancels them during mod unload. For
simple delayed or repeated work, prefer callback timers:

```angelscript
void SayReady(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
  ctx.BorrowLogger().Info("ready");
}

bool Heartbeat(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
  ctx.BorrowLogger().Info("heartbeat " + event.CompletedIterations);
  return event.CompletedIterations < 5;
}

void OnLoad(const BML::ModContext &in ctx) {
  BML::TimerRef@ once = ctx.SetTimeout(1000.0f, SayReady, "ready");
  BML::TimerRef@ loop = ctx.SetInterval(1000.0f, Heartbeat, "heartbeat");
}
```

`SetTimeout`/`SetInterval` use millisecond delays. `SetTimeoutTicks` and
`SetIntervalTicks` use BML tick delays. A timeout callback uses
`BML::TimerCallback` and returns `void`; an interval callback uses
`BML::TimerLoopCallback` and returns `bool`, where `false` stops the timer.
BML retains the function/delegate until completion, cancellation, or mod unload.
To bind a method with object state, create an AngelScript delegate:

```angelscript
class Pulse {
  int count = 0;
  bool Tick(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
    count++;
    return count < 5;
  }
}

void OnLoad(const BML::ModContext &in ctx) {
  Pulse pulse;
  BML::TimerLoopCallback@ cb = BML::TimerLoopCallback(pulse.Tick);
  ctx.SetIntervalTicks(1, cb, "pulse");
}
```

For timers that need object state, start-paused, repeat-count, or priority
configuration, implement `BML::Timer`. Registration stores a script object and
caches its `Tick` method at registration. This mirrors the native `Timer` model:
provide either `DelayTicks` or `DelayMs`, then optional `Name`, `Loop`,
`RepeatCount`, `Priority`, and `StartPaused` getters.

```angelscript
class HeartbeatTimer : BML::Timer {
  string get_Name() const { return "heartbeat"; }
  float get_DelayMs() const { return 1000.0f; }
  bool get_Loop() const { return true; }

  bool Tick(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
    ctx.BorrowLogger().Info("heartbeat " + event.CompletedIterations);
    return event.CompletedIterations < 5;
  }
}

void OnLoad(const BML::ModContext &in ctx) {
  BML::TimerRef@ heartbeat = ctx.AddTimer(HeartbeatTimer());
}
```

`TimerRef` supports `IsValid`, `Pause`, `Resume`, `Cancel`, `State`,
`CompletedIterations`, `RemainingIterations`, and `Progress`.

The API stub shows `Timer@+`, command/DataShare callback `@+`, `Command@+`,
and `DataShareRequest@+`. That is an AngelScript/native ownership contract:
BML retains the object or delegate when registration succeeds and releases it on
unregister, completion, cancellation, or mod unload. Script authors still pass
normal script objects, including temporaries such as `ctx.AddTimer(HeartbeatTimer())`.

## Command And Completion

Commands are script objects. Implement the `BML::Command` interface and
register an instance. BML owns the command object until unregister or mod
unload. The interface only requires `Name` and `Execute`; optional metadata
getters and `Complete` are detected at registration.

Lightweight commands can instead use `CommandDefinition` plus function or method
delegates. `CommandDefinition.Enabled` defaults to `true`; the other fields
default to an empty string or `false`. Duplicate names or aliases fail and return
`null`. Self-unregister is delayed until the callback returns.

```angelscript
class HelloCommand : BML::Command {
  string get_Name() const { return "hello"; }
  string get_Alias() const { return "h"; }
  string get_Description() const { return "Print hello"; }

  void Execute(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
    ctx.BorrowLogger().Info("hello " + event.ArgsText);
  }

  void Complete(const BML::ModContext &in ctx,
                const BML::CommandEvent &in event,
                BML::CommandCompletion &inout completions) {
    completions.Add("world");
    completions.Add("ballance");
  }
}

void OnLoad(const BML::ModContext &in ctx) {
  BML::CommandRef@ command = ctx.RegisterCommand(HelloCommand());
  if (command is null || !command.IsValid)
    ctx.BorrowLogger().Warn("failed to register hello command");
}
```

```angelscript
void HelloExecute(const BML::ModContext &in ctx,
                  const BML::CommandEvent &in event) {
  ctx.BorrowLogger().Info("hello " + event.ArgsText);
}

void HelloComplete(const BML::ModContext &in ctx,
                   const BML::CommandEvent &in event,
                   BML::CommandCompletion &inout completions) {
  completions.Add("world");
}

void OnLoad(const BML::ModContext &in ctx) {
  BML::CommandDefinition def;
  def.Name = "hello";
  def.Alias = "h";
  def.Description = "Print hello";
  BML::CommandRef@ command = ctx.RegisterCommand(def, HelloExecute, HelloComplete);
}
```

## DataShare

Synchronous typed read:

```angelscript
bool enabled = BML::DataShareGetBool("feature.enabled", false, "BML");
int size = BML::DataShareSizeOf("feature.enabled", "BML");
```

Typed request objects become inert after completion, cancellation, or mod
unload. Implement `BML::DataShareRequest`, then pass the object to
`RequestDataShare`. Script DataShare callbacks are queued onto the BML main
thread safe point; they do not run inside `RequestDataShare`, `DataShareSet*`,
or native DataShare trigger call stacks. If the value is already available, the
returned ref can remain valid until the queued callback is drained on a later
process tick, then becomes invalid after the one-shot callback completes.
Reload or unload cancels queued callbacks that have not run yet. The delegate
overload has the same lifecycle rules and uses the current default DataShare
namespace when `name` is empty.

```angelscript
class GreetingRequest : BML::DataShareRequest {
  string get_Key() const { return "remote.greeting"; }
  int get_Type() const { return BML::DATASHARE_STRING; }
  string get_Name() const { return "OtherMod"; } // optional, defaults to "BML"

  void Receive(const BML::ModContext &in ctx, const BML::DataShareEvent &in event) {
    ctx.BorrowLogger().Info(event.Exists ? event.StringValue : "missing " + event.Key);
  }
}

void OnLoad(const BML::ModContext &in ctx) {
  BML::DataShareRequestRef@ request = ctx.RequestDataShare(GreetingRequest());
  if (request is null)
    ctx.BorrowLogger().Warn("failed to request remote.greeting");
}
```

```angelscript
void ReceiveGreeting(const BML::ModContext &in ctx,
                     const BML::DataShareEvent &in event) {
  ctx.BorrowLogger().Info(event.Exists ? event.StringValue : "missing " + event.Key);
}

void OnLoad(const BML::ModContext &in ctx) {
  BML::DataShareRequestRef@ request =
      ctx.RequestDataShare("remote.greeting", BML::DATASHARE_STRING, ReceiveGreeting);
}
```

## Common Recipes

### Toggle A Feature With Config And Command

```angelscript
bool enabled = true;
BML::ConfigProperty@ enabledProp;

void OnLoad(const BML::ModContext &in ctx) {
  BML::Config@ config = ctx.BorrowConfig();
  if (config !is null) {
    @enabledProp = config.GetProperty("General", "Enabled");
    if (enabledProp !is null) {
      enabledProp.SetDefaultBoolean(true);
      enabled = enabledProp.GetBoolean(true);
    }
  }

  BML::CommandDefinition def;
  def.Name = "toggle-example";
  def.Description = "Toggle the example feature";
  ctx.RegisterCommand(def, ToggleExample);
}

void ToggleExample(const BML::ModContext &in ctx,
                   const BML::CommandEvent &in event) {
  enabled = !enabled;
  if (enabledProp !is null && enabledProp.IsValid)
    enabledProp.SetBoolean(enabled);
  ctx.SendIngameMessage(enabled ? "Example enabled" : "Example disabled");
}

void OnModifyConfig(const BML::ModContext &in ctx,
                    const BML::ConfigEvent &in event) {
  if (event.Category == "General" && event.Key == "Enabled") {
    BML::ConfigProperty@ prop = event.BorrowProperty();
    enabled = prop !is null ? prop.GetBoolean(enabled) : enabled;
  }
}
```

### Delay Work Until The Next Tick

Use this when a callback fires before the scene object you need is ready, or
when a command should defer a heavy scan:

```angelscript
void RunLater(const BML::ModContext &in ctx,
              const BML::TimerEvent &in event) {
  CKContext@ ck = ctx.BorrowCKContext();
  Entity3DRef@ ball = Scene::FindEntity3D(ck, "Ball");
  if (ball !is null && ball.valid)
    ctx.BorrowLogger().Info("Ball id=" + ball.Id());
}

void OnLoad(const BML::ModContext &in ctx) {
  ctx.SetTimeoutTicks(1, RunLater, "find-ball");
}
```

### Expose A Small Service To Other Mods

```angelscript
[bml.export(name: "IsFeatureEnabled", signature: "bool()")]
bool IsFeatureEnabled() {
  return enabled;
}

[bml.export(name: "SetFeatureEnabled", signature: "void(bool)")]
void SetFeatureEnabled(bool value) {
  enabled = value;
}
```

Callers should use `TryFindExport` for optional integration and inspect the
returned status before calling.

## Diagnostics

Script failures are recorded as structured diagnostics:

```text
phase=compile message=...
phase=callback message=...
phase=export-lookup message=...
```

You can inspect diagnostics from:

- The BML Mod menu: script mods show loaded/failed state and details.
- Script code: `ModRef.Diagnostic`.
- Native interop: `BML_GetModDiagnostic(id, ...)`.

Common phases:

- `ckas-host`: `AngelScript.dll` is missing, or the installed CKAS build lacks
  the required namespace, object-method context, or generic script-array object
  access features.
- `compile`: AngelScript compilation failed.
- `metadata`: `bml.mod`, dependency, or export metadata is invalid.
- `callback`: a fixed callback failed at runtime.
- `export-lookup` / `export-call`: export missing, signature mismatch, or call failed.

Debugging order:

1. Check the first diagnostic phase. Fix `ckas-host`, `compile`, and
   `metadata` errors before looking at runtime logic.
2. Check the exact callback name in the diagnostic. A callback failure marks the
   script mod failed, so later callbacks may not run.
3. Log ids, names, and status codes, not raw handle addresses.
4. When a borrowed CK handle is null, log the durable id/name snapshot or CKAS
   ref `Error()` instead.
5. Reproduce with a small script package before moving the same logic into a
   larger mod.

For local validation, use the script-capable release package and copy the mod
into a clean `ModLoader/Mods` directory. The repository smoke tests are useful
for BML development, but an author should still verify the script in Player
with the same CKAngelScript runtime that will ship with the mod.

## Lifetime Rules

- Script callbacks use `NO_SUSPEND`; suspension is an error.
- Event objects are snapshots of scalar/string/list payloads. Event `Borrow*`
  methods return borrowed CK handles that may become null or stale after the
  callback, except `ConfigEvent.BorrowProperty()` which returns a BML
  `ConfigProperty` wrapper that revalidates the owning mod and property.
- `CommandCompletion` and borrowed CK object/manager handles are callback-scope
  only. CKAS `ObjectRef@`-derived handles are the preferred long-lived identity
  for CK objects; raw pointers resolved from them are still short-lived.
- `ModContext` is a per-mod facade. Store durable IDs/refs instead of borrowed
  CK handles when work crosses callbacks or frames.
- Timer, Command, DataShareRequest, and HookBlock resources are owned by the
  script mod and are cleaned up on mod unload and hot reload replacement.
- Export handles are generation-checked and become invalid when the owner or
  export disappears.
- Hot reload keeps the mod registry slot stable, but it does not discover new
  mods, rebuild dependency graph nodes, cascade dependent reloads, or undo
  script-authored game-world side effects.

## Do Not

- Do not rely on old callback overloads.
- Do not store borrowed CK objects or CK managers in long-lived fields.
- Do not access raw CKAngelScript engine/context/module/function state. This
  does not forbid ordinary CKAngelScript script APIs such as `Scene`, `Behavior`,
  `BB`, `Param`, or registered CK/Vx SDK bindings.
- Do not use `NativePointer`, `DynCall`, or writable native memory as a normal
  substitute for a missing plugin API. If a script needs stable native behavior,
  add a guarded CKAngelScript engine extension in the owning native plugin.
- Do not mutate dependencies at runtime; use metadata.
- Do not write a separate manifest DSL. The entry must be valid AngelScript.
- Do not package script mods as `.bmodp`; v1 script packages are single-file,
  directory, or `.zip`.

## Package Notes

- Supported package forms are single-file `*.mod.as`, directory script mods,
  and `.zip` script packages.
- `.bmodp` is native-only.
- Single-file resources live under the sibling directory named after the entry
  stem, e.g. `Mods/Foo/Resources` for `Mods/Foo.mod.as`. If the script grows
  enough to need helper `.as` libraries, prefer converting it to a directory
  script mod with `Foo/Foo.mod.as` and `Foo/libs/*.as`.
- Directory and zip script mods should keep stable assets under `Resources/`.
- Keep exactly one `*.mod.as` entry per directory or zip package. Helper files
  are fine only when the CKAngelScript build process includes them as part of
  the entry module.
- Do not package `docs/bml-script-mod-api.as` with the mod as runtime code; it
  is an editor/reference stub.
- Declare BML mod dependencies with `[bml.require]` and `[bml.optional]`.
  Mention CKAngelScript/runtime-script requirements in your README because they
  are outside the BML dependency graph.
- If the mod depends on a native plugin's CKAS extension namespace, document the
  native plugin id/version and handle missing APIs with a clear diagnostic.

## Author Checklist

- The package has exactly one `*.mod.as` entry.
- The main class has one `[bml.mod]` with stable `id`, `name`, and `version`.
- Every dependency that affects load order is metadata, not a runtime check.
- Every export has a supported signature and a stable name.
- `OnLoad` logs at least one clear startup line during development.
- Long-lived CK object identity uses CKAS refs or ids, not borrowed raw handles.
- Timers, commands, and DataShare requests are either held in fields or designed
  to finish immediately.
- Render/UI code runs from `OnRender`.
- Heavy scene scans are delayed, cached as refs/ids, or moved to a native
  plugin/CKAS component when needed.
- The script has been tested in Player with the release CKAngelScript runtime.

## English Summary

BML Script Mod v1 uses one `*.mod.as` entry per single-file, directory, or
`.zip` script package, AngelScript metadata declarations, fixed callback
signatures, script-owned Timer/Command objects, typed DataShare requests, and
typed export handles. It supports hot reload for already discovered script mods
within the documented lifecycle boundaries. `BML::ModContext` is the primary
facade. Event objects are snapshots, while `Borrow*` methods remain borrowed CK
escape hatches; CKAS `ObjectRef@`-derived handles are the long-lived CK object
identity model.
CKAngelScript runtime scripts,
`AngelScript Component`, `Scene`, `Behavior`, `BB`, `Param`, `Message`, `Async`,
raw CK/Vx SDK, ImGui, and registered extension APIs remain available according
to their own contracts; BML v1 only defines the BML-owned mod surface. `.bmodp`
script packages, BML-owned entity wrappers, raw CKAS engine/module/function
access, BML callback suspension/resume, and a full sandbox policy are deferred.
