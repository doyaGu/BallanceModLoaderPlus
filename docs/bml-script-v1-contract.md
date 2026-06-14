# BML Script Mod v1 Contract

This document freezes the BML+ script mod v1 author-facing contract. It is a
release contract, not a design sketch.

## Stable in v1

- Entry discovery: one arbitrary `*.mod.as` entry per single-file script, script mod directory, or `.zip` script package.
- Package forms: `Mods/Foo.mod.as`, `Mods/Foo/<entry>.mod.as`, and `Mods/Foo.zip`.
- `.bmodp` is native-only and is not a script mod package format.
- Declaration model: AngelScript metadata reflection, not source parsing and not a separate manifest DSL.
- Metadata tags: `bml.mod`, `bml.require`, `bml.optional`, `bml.export`.
- Main class: one `[bml.mod]` class in any AngelScript namespace; BML uses CKAS metadata reflection for the namespace and class identity.
- Fixed callbacks: only the signatures documented in `bml-script-mod-author-guide.md` and `bml-script-mod-api.as`.
- Event model: callback-scope borrowed event views and integer `GameEvent` constants.
- Export model: typed export registry with generation-checked `ExportRef` / native `BML_ModExport`; `BML_FindModExportEx` and `ModRef.TryFindExport` expose stable Interop lookup status codes.
- `CallFrame`: bool/int/float/string/void only, with arg/result type introspection and targeted arg/result clearing for reusable frames. Calls clear result before dispatch and leave no result after failure or `void` success.
- Script-owned Timer: `BML::Timer` object callbacks, automatic unload cleanup.
- Script-owned Command: `BML::Command` object callbacks, automatic unload cleanup.
- Typed DataShare: bool/int/float/string get/set, `DataShareSizeOf`, and script-owned `DataShareRequest` object requests.
- UI: stable `BML::UI` menu facade for ordinary render-time Bui-style controls.
- Advanced ImGui: generated `ImGui` namespace documented in `docs/bml-imgui-api.as`; frame-scope only, with context/platform lifecycle, allocators, raw callbacks, and raw `void*` intentionally omitted.
- Diagnostics: script failures expose phase/message through logs, Mod menu, script `ModRef`, and native interop.
- CKAngelScript integration: optional runtime dependency; BML+ must still load without `AngelScript.dll`.
- CKAngelScript API coexistence: BML script mods may use CKAngelScript's
  registered script APIs, including `Scene`, `Behavior`, `BB`, `Param`, raw
  CK/Vx SDK bindings, and APIs from other registered engine extensions. This
  contract defines only the BML-owned surface.

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
- Async resume or script suspension.

## Compatibility Rules

- A v1 script should not depend on undocumented declarations, old callback overloads, or generated implementation helper names.
- BML may add new facade functions in later minor releases without breaking v1.
- BML may reject metadata or callback shapes that were never documented here.
- V1 export signatures support only `void`, `bool`, `int`, `float`, and `string`/`const string &in`; signatureless lookup is valid only when the export name is unique.
- `docs/bml-script-mod-api.as` is a stub for authoring and validation. The guide and this contract define the supported surface.

## Optional Dependency Policy

Release packages may include an `optional/CKAngelScript` directory with a known-good CKAngelScript/AngelScript build. Installing it enables script mods. Removing it must not prevent native BML+ features from loading. BML script v1 requires a CKAngelScript build that reports `CKAS_FEATURE_OBJECT_TYPE_NAMESPACE`, because script mod main classes may live in AngelScript namespaces and BML creates them through `CKAngelScriptObjectOptions::ClassNamespace`.

## Shutdown Anomaly Policy

Some Player runs may report a non-zero exit code after `Goodbye!` has been written. v1 validation reports this separately as a shutdown anomaly. It is not treated as a script facade regression when all smoke markers and logs completed before shutdown.

## English Quick Contract

Stable v1 is: single-file/directory/zip `*.mod.as` entry, AngelScript metadata, fixed callbacks, callback-scope event views, typed export registry, script-owned Timer/Command/DataShareRequest, optional CKAngelScript. CKAngelScript's own Scene/Behavior/BB/Param and CK/Vx bindings are available under the CKAngelScript contract. Deferred: hot reload, `.bmodp` script packages, BML-owned full object wrappers, full sandbox policy, raw CKAS engine/module/function access, async resume.
