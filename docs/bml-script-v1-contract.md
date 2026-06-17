# BML Script Mod v1 Contract

This document freezes the BML+ script mod v1 author-facing contract. It is a
release contract, not a design sketch.

The contract is intentionally scoped to BML-owned mod semantics. CKAngelScript
runtime scripts, `AngelScript Component`, high-level Virtools namespaces, raw
CK/Vx bindings, and plugin-owned engine extensions follow the CKAngelScript or
owning-plugin contract.

## Stable in v1

- Entry discovery: one arbitrary `*.mod.as` entry per single-file script, script mod directory, or `.zip` script package.
- Package forms: `Mods/Foo.mod.as`, `Mods/Foo/<entry>.mod.as`, and `Mods/Foo.zip`.
- `.bmodp` is native-only and is not a script mod package format.
- Declaration model: AngelScript metadata reflection, not source parsing and not a separate manifest DSL.
- Metadata tags: `bml.mod`, `bml.require`, `bml.optional`, `bml.export`.
- Main class: one `[bml.mod]` class in any AngelScript namespace; BML uses CKAS metadata reflection for the namespace and class identity.
- Fixed callbacks: only the signatures documented in `bml-script-mod-author-guide.md` and `bml-script-mod-api.as`.
- Event model: snapshot event objects, typed `GameEvent` enum values,
  `Borrow*` accessors for borrowed CK handles or BML service wrappers, and
  `OnModifyConfig` config-change events.
- Export model: typed export registry with generation-checked `ExportRef` / native `BML_ModExport`; `BML_FindModExportEx` and `ModRef.TryFindExport` expose stable Interop lookup status codes.
- Interop `CallFrame`: bool/int/float/string/void plus scalar arrays,
  `array<uint8>` buffers, and `CKObject@` identity values. Calls clear result
  before dispatch and leave no result after failure or `void` success.
- Script-owned Timer: `BML::Timer` object callbacks, automatic unload cleanup.
- Script-owned Command: `BML::Command` object callbacks, automatic unload cleanup.
- Typed DataShare: bool/int/float/string get/set, `DataShareSizeOf`, and script-owned `DataShareRequest` object requests.
- Core capabilities: `BML::UI` messages, `BML::Menu`, and `BML::HUD` are
  backed by Interop exports from the built-in `BML` mod. Compatibility
  `ModContext` helpers route through the same capability contract where
  available.
- UI: stable `BML::UI` menu facade for ordinary render-time Bui-style controls.
- Advanced ImGui: generated `ImGui` namespace documented in `docs/bml-imgui-api.as`; frame-scope only, with context/platform lifecycle, allocators, raw callbacks, and raw `void*` intentionally omitted.
- Diagnostics: script failures expose phase/message through logs, Mod menu, script `ModRef`, and native interop.
- CKAngelScript integration: official script-capable BML+ release packages include the matching CKAngelScript runtime.
- CKAngelScript API coexistence: BML script mods may use CKAngelScript's
  registered script APIs, including runtime/component-facing `Scene`,
  `Behavior`, `BB`, `Param`, `Message`, `Async`, raw CK/Vx SDK bindings, and
  APIs from other registered engine extensions. This contract defines only the
  BML-owned surface.

## Experimental or Deferred

- Hot reload. No transactional or fail-stop reload semantics are promised in v1.
- Script package signing, package permissions, and multi-mod script archives.
- `.bmodp` script packages. `.bmodp` remains reserved for native DLL mods.
- Complete `IBML` facade coverage. v1 covers the practical script-facing subset and documents omissions.
- Stable BML-owned object/game-entity wrappers. v1 uses `CK_ID`, borrowed object
  handles, and event views in the BML facade; CKAngelScript's own `Scene`
  refs and CK/Vx bindings follow the CKAngelScript contract.
- Full permission/sandbox policy. v1 keeps path escape checks and does not expose process/network APIs.
- Formal performance thresholds. The perf probe records baseline numbers, but pass/fail thresholds are not frozen.
- Raw CKAngelScript engine/context/module/function access from ordinary script mods.
- Treating `NativePointer`, `DynCall`, or writable native memory as a supported
  replacement for plugin-owned script APIs.
- Suspending or asynchronously resuming BML fixed callbacks. CKAngelScript
  `Async` remains a CKAS-owned runtime/component facility, not a BML callback
  contract.

## Compatibility Rules

- A v1 script should not depend on undocumented declarations, old callback overloads, or generated implementation helper names.
- BML may add new facade functions in later minor releases without breaking v1.
- BML may reject metadata or callback shapes that were never documented here.
- Export signatures support scalars plus `const array<bool/int/float/string> &in`,
  `array<bool/int/float/string>@`, `const array<uint8> &in`,
  `array<uint8>@`, and `CKObject@`. Signatureless lookup is valid only when the
  export name is unique; explicit lookup compares compiled descriptors.
- `docs/bml-script-mod-api.as` is a stub for authoring and validation. The guide and this contract define the supported surface.

## CKAngelScript Coexistence

- A BML script mod is a CKAS-hosted AngelScript class with BML mod identity. It
  is not a CKAS runtime script and not an `AngelScript Component`.
- `[bml.mod]`, `[bml.require]`, `[bml.optional]`, and `[bml.export]` are BML
  metadata. They do not replace CKAS `[script]` manifests or
  `[script.depends]` for runtime scripts.
- BML dependency ordering is a mod-loader graph. CKAS runtime script
  dependencies remain CKAS runtime-manager state.
- BML fixed callbacks receive `BML::ModContext`. CKAS runtime scripts receive
  `ScriptContext`; components receive `CKBehaviorContext`. Shared CKAS helpers
  may also expose `CKContext@` overloads. Use the natural context for the
  surface being authored.
- BML callbacks are synchronous and no-suspend. Event object scalar/string/list
  data is copied, but CK handles returned by event `Borrow*` methods remain
  borrowed CK handles. BML service wrappers such as `Logger`, `Config`, and
  `ConfigProperty` are ref-counted handles that revalidate their owning mod.
  Long-running script work should be expressed through BML Timer/Command/DataShare
  when it is a BML mod concern, or through CKAS `Async`/`Message` when it is
  runtime/component script coordination.
- Source builds may set `BML_ENABLE_ANGELSCRIPT=OFF`. Official
  script-capable release packages include the matching CKAngelScript runtime and
  SDK headers.

## CKAngelScript Release Policy

Official script-capable release packages include a known-good
CKAngelScript/AngelScript runtime and install `AngelScript.dll` next to
`BMLPlus.dll` under `BuildingBlocks`. SDK packages include the matching
`CKAngelScript.h`, `angelscript.h`, and AngelScript add-on headers. Source
builds may still disable script support with `BML_ENABLE_ANGELSCRIPT=OFF`.
BML script support requires a CKAngelScript build that reports
`CKAS_FEATURE_OBJECT_TYPE_NAMESPACE`,
`CKAS_FEATURE_OBJECT_METHOD_CONTEXT_ACCESS`, and
`CKAS_FEATURE_SCRIPT_ARRAY_ACCESS`. These are not Interop feature flags:
object-method context access lets hosts configure/read prepared AngelScript
contexts, and script-array access exposes CKAS-owned `array<T>` objects through
generic lifecycle/type/size/element-address operations. CKAS never depends on
BML or Interop.

## Shutdown Anomaly Policy

Some Player runs may report a non-zero exit code after `Goodbye!` has been written. v1 validation reports this separately as a shutdown anomaly. It is not treated as a script facade regression when all smoke markers and logs completed before shutdown.

## English Quick Contract

The stable BML script mod contract is: single-file/directory/zip `*.mod.as`
entry, AngelScript metadata, fixed callbacks, snapshot event objects with
borrowed CK handle accessors or BML service wrappers, typed export registry with
scalar arrays/buffers/object identity, script-owned Timer/Command/DataShareRequest,
and official release packages that include the matching CKAngelScript runtime.
CKAngelScript runtime scripts, `AngelScript Component`, Scene/Behavior/BB/Param,
Message/Async, and CK/Vx bindings are available under the CKAngelScript
contract. Deferred: hot reload, `.bmodp` script packages, BML-owned full object
wrappers, full sandbox policy, raw CKAS engine/module/function access, and BML
fixed-callback suspension/resume.
