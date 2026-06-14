# BML Script Mod v1 Author Guide

This document is the BML+ Script Mod v1 author guide.
`docs/bml-script-mod-api.as` is the API stub for editor completion and static
checks; it is not the primary tutorial.

## Quickstart

Start from `templates/script-mod-template` for a minimal script mod. The
template is intentionally small; `tests/smoke/AngelScript` contains regression
assets, not starter examples.

v1 supports three package forms under `ModLoader/Mods`:

- A single `Foo.mod.as` file.
- A directory containing exactly one `*.mod.as` entry.
- A `.zip` package containing exactly one script mod entry.

The entry file must be valid AngelScript. BML does not parse source text; it
compiles the entry through CKAngelScript and reads declaration metadata from
CKAS metadata reflection.

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

Recommended directory shape:

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

`.bmodp` is reserved for native DLL mods. Do not use `.bmodp` for script mods.

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

## API Layers

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

## Architectural Patterns

- Pure BML script mod: good for commands, config-driven toggles, HUD/menu
  changes, DataShare, exports, resource loading, and light level scripting.
- BML orchestrator plus CKAS components: BML owns mod configuration and public
  interop; components live on behavior instances and execute per-object logic.
- Native core plus script policy: native code owns hooks, renderer/input patches,
  or high-frequency loops; AngelScript exposes safe knobs, scripts, and
  callbacks through a CKAS extension.
- Script service bus: CKAS `Message`/`Async` connects runtime scripts and
  components; BML exports/DataShare expose the stable mod-facing service.

## Metadata

`bml.mod` declares the main script mod class. It is required and must appear
exactly once.

```angelscript
[bml.mod(id: "example.core", name: "Example Core", version: "1.2.0",
         author: "You", description: "Example", bml: "0.3.12")]
class ExampleCore {
}
```

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

Exports are methods marked with `[bml.export]`. The public signature supports
only `void`, `bool`, `int`, `float`, and `string` values. Parameter names are
not part of the contract.

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

## Fixed Callbacks

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

## ModContext

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

`BML::Physics` wraps runtime `ExecuteBB` physics actions. These helpers return
`false` when BML is not in a loaded level or the target is null. It does not
expose PhysicsRT private manager types such as `CKIpionManager`.

`BML::Text` creates a `2D Text` behavior under an owner script and returns a
borrowed `CKBehavior@`. Pass materials as explicit borrowed arguments when
needed; keep `Text2DDefinition` value-only.

## UI

`BML::UI` is a small BML-style menu facade over the native Bui helpers. Use it
from `OnRender`; drawing and input calls outside a render callback are safe
no-ops or return default values. The facade intentionally does not expose
`Bui::Window`, `Bui::Page`, raw draw lists, raw text buffers, resource
initialization, or script/menu transition internals.

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
int status = exportRef.Call(frame);
string result;
if (status == 0)
  frame.GetResultString(result);
```

`CallFrame` exposes `ArgCount`, `GetArgType(index)`, `ClearArg(index)`,
`ResultType`, and `ClearResult()` for reusable dynamic frames. Type values are
the `BML::CallValueType` enum: empty, bool, int, float, and string.
Each `ExportRef.Call*` clears the frame result before dispatch. A failed call or
`void` export leaves `ResultType == BML::CALL_VALUE_EMPTY`; do not reuse an old
result after a non-zero status.

`ExportRef` is generation-checked. After owner unload or export unregister,
old handles do not call stale runtime state and return
`BML::ERROR_INTEROP_HANDLE_STALE`. `TryFindExport` returns
`BML::ERROR_INTEROP_TARGET_NOT_FOUND`, `ERROR_INTEROP_TARGET_FAILED`,
`ERROR_INTEROP_EXPORT_NOT_FOUND`, `ERROR_INTEROP_EXPORT_AMBIGUOUS`,
`ERROR_INTEROP_BAD_SIGNATURE`, or `ERROR_INTEROP_SIGNATURE_MISMATCH` for lookup
failures. V1 export signatures support only `void`, `bool`, `int`, `float`, and
`string`/`const string &in`.

## Timer

Timers are owned by the script mod. BML cancels them during mod unload. Timer
registration stores a script object and caches its `Tick` method at registration.
This mirrors the native `Timer` model: provide either `DelayTicks` or `DelayMs`,
then optional `Name`, `Loop`, `RepeatCount`, `Priority`, and `StartPaused`
getters.

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

The API stub shows `Timer@+`, `Command@+`, and `DataShareRequest@+`.
That is an AngelScript/native ownership contract: BML retains the object when
registration succeeds and releases it on unregister, completion, cancellation,
or mod unload. Script authors still pass normal script objects, including
temporaries such as `ctx.AddTimer(HeartbeatTimer())`.

## Command And Completion

Commands are script objects. Implement the `BML::Command` interface and
register an instance. BML owns the command object until unregister or mod
unload. The interface only requires `Name` and `Execute`; optional metadata
getters and `Complete` are detected at registration.

Do not expose raw callbacks or userData. Self-unregister is delayed until the
callback returns.

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

## DataShare

Synchronous typed read:

```angelscript
bool enabled = BML::DataShareGetBool("feature.enabled", false, "BML");
int size = BML::DataShareSizeOf("feature.enabled", "BML");
```

Typed request objects become inert after completion, cancellation, or mod
unload. Implement `BML::DataShareRequest`, then pass the object to
`RequestDataShare`. If the value is already available, the callback may run
before `RequestDataShare` returns and the returned ref may already be invalid.

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

- `ckas-host`: `AngelScript.dll` is missing, CKAS v3 features are incomplete,
  or the installed CKAS build is too old for namespaced script mod classes.
- `compile`: AngelScript compilation failed.
- `metadata`: `bml.mod`, dependency, or export metadata is invalid.
- `callback`: a fixed callback failed at runtime.
- `export-lookup` / `export-call`: export missing, signature mismatch, or call failed.

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
- Timer, Command, and DataShareRequest resources are owned by the script mod
  and are cleaned up on mod unload.
- Export handles are generation-checked and become invalid when the owner or
  export disappears.
- Hot reload has no v1 contract.

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
  stem, e.g. `Mods/Foo/Resources` for `Mods/Foo.mod.as`.
- Directory and zip script mods should keep stable assets under `Resources/`.

## English Summary

BML Script Mod v1 uses one `*.mod.as` entry per single-file, directory, or
`.zip` script package, AngelScript metadata declarations, fixed callback
signatures, script-owned Timer/Command objects, typed DataShare requests, and
typed export handles. `BML::ModContext` is the primary facade. Event objects are
snapshots, while `Borrow*` methods remain borrowed CK escape hatches; CKAS
`ObjectRef@`-derived handles are the long-lived CK object identity model.
CKAngelScript runtime scripts,
`AngelScript Component`, `Scene`, `Behavior`, `BB`, `Param`, `Message`, `Async`,
raw CK/Vx SDK, ImGui, and registered extension APIs remain available according
to their own contracts; BML v1 only defines the BML-owned mod surface. Hot
reload, `.bmodp` script packages, BML-owned entity wrappers, raw CKAS
engine/module/function access, BML callback suspension/resume, and a full
sandbox policy are deferred.
