# Script mod lifecycle and reload

This page defines mod metadata, callbacks, hot reload, diagnostics, and the
lifetime of script-owned resources.

## Metadata and dependencies

`bml.mod` marks the one main class BML+ creates:

```angelscript
[bml.mod id="example.core" name="Example Core" version="1.2.0"
         author="You" description="Example" bml="0.3.13"]
class ExampleCore {
}
```

`id`, `name`, and `version` are required. `author`, `description`, and `bml`
are optional. The `bml` value is the minimum BML+ version, not the script's
version. Unknown `bml.*` metadata is an error.

Use a stable owner-prefixed id such as `author.example.mod`. Changing it makes
a different mod and breaks dependencies and service ownership associated with
the previous id.

Declare load-order dependencies with metadata:

```angelscript
[bml.require id="other.mod" version="1.0.0"]
[bml.optional id="debug.helper" version="0.1.0"]
[bml.mod id="example.dep" name="Example With Deps" version="1.0.0"]
class ExampleWithDeps {
}
```

`bml.require` blocks loading when the dependency is missing or too old.
`bml.optional` records a dependency that can be queried through `ModRef` but
does not block loading. These declarations belong to the BML+ dependency graph;
CKAngelScript runtime scripts use their own `[script.depends]` metadata.

## Callbacks

BML+ recognizes these exact signatures:

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

| Callback | Use |
| --- | --- |
| `OnLoad` | Create config, commands, timers, DataShare requests, hooks, and registered content. A failure fails the script mod load. |
| `OnUnload` | Undo script-authored world changes and clear local state. BML+ also removes script-owned registrations. |
| `OnProcess` | Per-tick logic, input handling, and BML/ImGui drawing. |
| `OnRender` | Observe the Virtools render callback. Do not draw BML/ImGui UI here. |
| `OnGameEvent` | Handle level, reset, pause, finish, ball, and navigation events. |
| `OnCheatEnabled` | Observe cheat-state changes. |
| `OnLoadObject` / `OnLoadScript` | Observe object and behavior-script loading. |
| `OnCommandEvent` | Observe command execution and completion phases. |
| `OnModifyConfig` | React to a changed category and key. Same-key re-entry is suppressed. |
| `OnPhysicalize` / `OnUnphysicalize` | Observe physics operations. |

Event objects copy scalar, string, and list data. Their `Borrow*` methods resolve
non-owning CK handles that may become null or stale after the callback. Store a
CK id or CKAngelScript reference for delayed work, then resolve the raw object
near the operation.

## Hot reload

Hot reload replaces a mod that BML+ discovered at startup. It does not add a
new mod or rebuild the dependency graph.

```text
script reload all
script reload <id>
script reload <id> --dry-run
script reload <id> --dry-run --check-state
script watch on
script watch off
script diag <id>
script logs error
```

Directory and single-file mods are watched automatically. Zip packages use
manual reload. A compile or metadata failure keeps the previous runtime active
when possible.

`--dry-run` checks compilation, metadata, compatibility, and required state
hook declarations without calling lifecycle or state hooks. Adding
`--check-state` calls the old `SaveState` and the candidate's migration/restore
hooks, then discards the candidate without calling its `OnLoad`. State hooks
used by this mode must therefore be pure.

Reload has these limits:

- A new entry, changed mod id, or changed dependency graph requires restart.
- Reload does not cascade to dependent mods.
- A candidate that no longer satisfies a loaded dependent is rejected.
- Old timers, commands, DataShare requests, hooks, and callback handles become
  invalid when replacement succeeds.
- BML+ can restore resources it owns, but cannot undo arbitrary CKAS or raw
  CK/Vx changes made to the game world.

Use `ctx.IsReloading` and `ctx.ReloadPhase` only when cleanup or recovery must
distinguish reload from ordinary shutdown. Normal startup and shutdown use
`BML::RELOAD_NONE`.

## Preserve primitive state

Script fields are not copied automatically. Use state hooks for primitive and
string data:

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

BML+ verifies that the old runtime can restore its saved state and that the
candidate can migrate or restore before calling `SaveState`. Candidate order is
`MigrateState`, `RestoreState`, then `OnLoad`. Rollback restores a clone of the
original bag before calling the old runtime's rollback `OnLoad`.

`StateBag` stores only `bool`, `int`, `float`, and `string`. Do not store script
objects, callbacks, ModRef, CK handles, timers, commands, or DataShare requests.
A reload bag is enabled only during the state hook. Recreate owned resources in
`OnLoad` after state restoration.

State hooks may copy values and log. They must not register resources, execute
commands, write config or DataShare values, mutate input, call host mutations,
or change CK/game objects.

## Diagnostics

Diagnostics identify the failing phase:

```text
phase=compile message=...
phase=callback message=...
```

Inspect them from the BML+ Mod menu, `ModRef.Diagnostic`,
`script diag <id>`, `script logs error`, and `ModLoader/ModLoader.log`.

Debug in this order:

1. Fix `ckas-host`, `compile`, and `metadata` errors first.
2. Check the exact callback named by a callback diagnostic.
3. Log ids, names, and status codes rather than raw handle addresses.
4. When a borrowed handle is null, log its durable id/name or the CKAS
   reference error.
5. Reproduce with a minimal package before restoring unrelated features.

## Lifetime rules

- BML+ fixed callbacks and registered callbacks use no-suspend execution.
- `CommandCompletion`, borrowed managers, and borrowed CK handles are
  callback-scoped.
- CKAngelScript `ObjectRef@`-derived handles are preferred for long-lived CK
  identity; the raw pointer resolved from them is still short-lived.
- `Logger`, `Config`, and `ConfigProperty` wrappers revalidate their owning mod,
  but should still be checked before use after failure or unload.
- Timer, Command, DataShare request, callback, and Hook Block resources are
  removed on unload and successful replacement.
- A hot reload does not undo script-authored game-world side effects.

## Release checklist

- The package contains exactly one `*.mod.as` entry.
- The main class has one `[bml.mod]` with a stable id, name, and version.
- Dependencies that affect load order are declared as metadata.
- Long-lived CK identity uses refs or ids, not borrowed raw handles.
- Drawing runs from `OnProcess`; hot callbacks do bounded work.
- The exact zip or single-file artifact was tested with the target BML+ and
  CKAngelScript release.

Next: [BML services](services.md).
