# BML+ script API reference

The downloadable reference files describe the AngelScript API exposed by BML+.
They are intended for editor completion and API lookup; they are not runtime
script files and must not be included in a mod package.

<ul>
  <li><a href="https://doyagu.github.io/BallanceModLoaderPlus/api/as.predefined"><code>as.predefined</code></a> — complete definitions for AngelScript Language Server, including CKAngelScript and BML+.</li>
  <li><a href="https://doyagu.github.io/BallanceModLoaderPlus/api/bml-script-mod-api.as"><code>bml-script-mod-api.as</code></a> — BML+ script mod types, functions, callbacks, and properties.</li>
  <li><a href="https://doyagu.github.io/BallanceModLoaderPlus/api/bml-imgui-api.as"><code>bml-imgui-api.as</code></a> — Dear ImGui bindings available to scripts.</li>
</ul>

The SDK installs the same files locally under `docs/api`. Use
`<BML-SDK>/docs/api/as.predefined` for editor configuration without a network
dependency.

An accessor named `get_Name()` defines an AngelScript property and is normally
used as `object.Name`. `@+` is an AngelScript auto-handle annotation at the native boundary. It does
not replace each service’s documented ownership/release contract. Prefer
property syntax for recorded values, such as `info.Definition.Id`. Service and Ref
validity uses `IsValid()`. Old getter aliases and flat Context services are removed;
see the [migration guide](migration.md).

Use `as.predefined` for editor tooling. Use the two smaller files when looking
up only the BML+-owned API surface.

## Mods menu pages

Script Mods use the same page registry as Native Mods. Register a visible
details entry and, optionally, hidden child pages from `OnLoad`:

```angelscript
void DrawEntry(BML::MenuPageFrame &inout frame) {
  if (BML::UI::MainButton("Details")) frame.Push("child");
}

void DrawChild(BML::MenuPageFrame &inout frame) {
  ImGui::TextUnformatted("More information");
  if (BML::UI::MainButton("Return to overview")) frame.Back();
}

[bml.mod id="example.menu.script" name="Menu Example" version="1.0.0"]
class MenuExampleMod {
  BML::MenuPageRef@ entryPage;
  BML::MenuPageRef@ childPage;

  void OnLoad(const BML::ModContext &in ctx) {
    BML::MenuPageDefinition entry;
    entry.Id = "overview";
    entry.Label = "Overview";
    int code = ctx.RegisterMenuPage(entry, DrawEntry, entryPage);
    if (code != BML::ERROR_OK) return;

    BML::MenuPageDefinition child;
    child.Id = "child";
    child.Label = "Details";
    child.ShowInDetails = false;
    code = ctx.RegisterMenuPage(child, DrawChild, childPage);
  }

  void OnUnload(const BML::ModContext &in ctx) {
    if (childPage !is null) childPage.Unregister();
    if (entryPage !is null) entryPage.Unregister();
  }
}
```

`Push` preserves the current page for `Back`; `Replace` swaps it without
adding history; `Close` exits the Mods menu. Page IDs are at most 255 bytes,
and targets resolve only within the registering Mod. Pages are also
unregistered automatically before a Script Mod is unloaded or hot-reloaded.
A `MenuPageRef` becomes invalid after removal
or replacement, and its callbacks are not called again after unregistration.
Draw callbacks run inside the Mods menu's central, scrollable ImGui content
region; do not start or end a frame. The loader supplies the native Back
control separately, so a page need not draw its own.

## ImGui properties

`ImGuiIO` is read-only through script bindings. `ImGuiStyle` exposes the existing
writable properties; the reference includes their setters as well as getters.
Both are borrowed from the current ImGui context. Prefer balanced Push/Pop style
calls for a local change instead of leaving a shared style mutation behind.

## Reference baseline

The BML section is reflected from the macOS ARM64 SDL dynamic Player running
AngelScript 2.38.0. Native `@+` annotations, omitted by engine reflection, are
restored from the BML registrations. The existing CKAngelScript/ImGui editor
baseline is retained. These files are editor references, not a universal engine
ABI; CKAS platform aliases and optional FFI still depend on the matching host.

To update or verify the BML reference, build the independent Player probe with
matching CKAngelScript headers, run `RunPostProcessTest.py --dump-api <export>`,
then run `python3 tools/generate_script_api.py --export <export>` (or add
`--check`). The generator preserves the non-BML portions of `as.predefined`.
Registration and teardown tests must still pass; a text export alone does not
prove object lifetime or config-group cleanup.
