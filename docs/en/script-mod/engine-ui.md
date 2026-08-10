# Engine access and UI

BML+ script mods run in CKAngelScript. Use the owner of each operation instead
of building overlapping wrappers:

- BML+ owns mod identity, load order, callbacks, config, commands, resources,
  loader UI, built-in services, and DataShare.
- CKAngelScript owns scene and behavior APIs, runtime scripts, components,
  messages, async work, and registered CK/Vx bindings.
- A native plugin owns unsafe hooks, native memory, performance-critical loops,
  and plugin-specific CKAngelScript extensions.

## Choose the CKAngelScript surface

| Goal | Surface |
| --- | --- |
| Find or change scene objects, data arrays, materials, meshes, textures, cameras, or scene membership | CKAS `Scene` and CK/Vx APIs, coordinated by a BML+ script mod when mod services are also needed |
| Add logic to one behavior instance | `AngelScript Component` |
| Search or edit behavior graphs and Building Blocks | CKAS `Behavior`, `BB`, and `Param` |
| Communicate among CKAS runtime scripts or components | CKAS `Message`; use `Async` only from an execution context that permits suspension |
| Add BML+ config, commands, resources, lifecycle, or loader UI | BML+ script mod APIs |
| Patch engine internals or expose a native service | Native plugin with a guarded CKAS extension if scripts need it |

BML+ callbacks receive `BML::ModContext`, not `ScriptContext` or
`CKBehaviorContext`. Use `ctx.BorrowCKContext()` only with APIs that accept a
`CKContext@`. Do not fake one context as another.

Raw `NativePointer`, `NativeBuffer`, DynCall/DynLoad/DynCallback, and writable
native memory are escape hatches for proven native integration work. They are
not a normal replacement for a missing plugin API.

## CK objects and durable identity

Start with CKAngelScript `Scene`, `Behavior`, `BB`, and `Param`. BML+
`ctx.Borrow*` calls provide convenient access to named CK objects and managers,
but those handles are non-owning.

Prefer CKAS references for identity that crosses callbacks:

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

Resolve raw CK handles close to the operation. Revalidate after deletion, level
changes, and CK reset.

`BML::CK` contains small stateless helpers, not a second complete Virtools API:

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

## Content registration

`RegisterBallType`, `RegisterFloorType`, and `RegisterModule` expose common
Ballance content registration. Call them from `OnLoad`; late registration
returns `false`.

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

Definitions are value objects. Keep asset paths mod-relative.

## Hook Block

Hook Block inserts a callback at a known point in an existing Virtools behavior
graph. CKAngelScript should find the owner script and Building Blocks; BML+
owns the inserted native block and retained callback.

```angelscript
BML::HookBlockRef@ hook;

int OnBallHook(const BML::ModContext &in ctx,
               const BML::HookBlockEvent &in event) {
  ctx.BorrowLogger().Info("Hooked " + event.BlockName);
  return CKBR_OK;
}

void OnLoad(const BML::ModContext &in ctx) {
  CKBehavior@ owner = ctx.BorrowScriptByName("Gameplay_Ingame");
  CKBehavior@ source = FindSubBehaviorByName(owner, "Some_Behavior");
  if (owner is null || source is null)
    return;

  @hook = ctx.InsertHookBlockAfter(owner, source, OnBallHook, "example hook");
}
```

`CreateHookBlock` creates an unattached block. `InsertHookBlockAfter`,
`InsertHookBlockBefore`, and `InsertHookBlockBetween` cover common one-input,
one-output patches. If several links match, find a more specific source or
target before insertion.

By default the block activates all outputs after the callback. Set
`AutoActivateOutputs` to `false` when the callback selects a branch through
`event.ActivateOutput(index)`.

BML+ removes installed hooks at unload. Store `HookBlockRef@` only when the mod
needs to disable or uninstall a hook earlier. Do not store borrowed behavior
handles from a Hook Block event.

## Physics and text helpers

`BML::Physics` wraps BML+'s runtime physics actions. Calls return `false` when
no level is loaded or the target is null. It does not expose private PhysicsRT
manager types.

```angelscript
void MakeBallPhysical(const BML::ModContext &in ctx, CK3dEntity@ target) {
  BML::PhysicalizeDefinition def;
  def.Fixed = false;
  def.Friction = 0.4f;
  def.Elasticity = 0.2f;
  def.Mass = 1.0f;
  def.CollisionGroup = "Ball";
  def.EnableCollision = true;

  if (!BML::Physics::PhysicalizeBall(
          target, def, VxVector(0.0f, 0.0f, 0.0f), 2.0f))
    ctx.BorrowLogger().Warn("physicalize failed");
}
```

`BML::Text` creates a `2D Text` behavior under an owner script and returns a
borrowed behavior handle. Keep `Text2DDefinition` value-only and pass required
materials explicitly.

## BML UI

`BML::UI` contains two kinds of operation:

- messages, menu state, and HUD commands that can run from normal callbacks;
- drawing controls that require the active ImGui frame and must run from
  `OnProcess`.

```angelscript
bool enabled = true;
int count = 3;
string search = "";

void OnProcess(const BML::ModContext &in ctx) {
  BML::UI::Title("Example Script");
  BML::UI::WrappedText("BML controls", 360.0f);
  if (BML::UI::MainButton("Click"))
    ctx.BorrowLogger().Info("clicked");
  BML::UI::YesNoButton("Enabled", enabled);
  BML::UI::InputIntButton("Count", count);
  BML::UI::SearchBar(search);
}
```

Calls made outside an active frame return defaults or draw nothing. This guard
prevents host corruption; it does not make the call correct.

## Advanced ImGui

Use `ImGui` for custom windows, tables, trees, images, and frame-local draw-list
work. Use it only from `OnProcess`. Handles such as `ImDrawList@`, `ImGuiIO@`,
and `ImGuiStyle@` are valid only for the current frame.

```angelscript
string filter = "";

void OnProcess(const BML::ModContext &in ctx) {
  if (ImGui::Begin("Advanced ImGui")) {
    ImGui::TextUnformatted("Custom script UI");
    ImGui::InputText("Filter", filter, 128);
  }
  ImGui::End();
}
```

Use BML+ timers for delayed work from a BML+ mod. BML+ callbacks are
no-suspend, so they cannot await CKAngelScript async work.

Next: [Communication](communication.md).
