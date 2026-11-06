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
property syntax (`input.IsValid`, `ctx.IsInLevel`) when an accessor is present,
even if a same-named method alias also appears in the declarations.

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
    @entryPage = ctx.RegisterMenuPage(entry, DrawEntry);

    BML::MenuPageDefinition child;
    child.Id = "child";
    child.Label = "Details";
    child.ShowInDetails = false;
    @childPage = ctx.RegisterMenuPage(child, DrawChild);
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

The current combined declaration file was regenerated on 2026-11-06 from a
macOS ARM64 SDL dynamic host running AngelScript 2.38.0, with DynCall disabled,
then combined with the BML+ and ImGui declarations. It includes current
BehaviorFrame/Scene signatures and 64-bit pointer-sized aliases. CKAS platform
aliases and optional FFI bindings depend on the host; this is an editor snapshot,
not a universal ABI definition. Regenerate against the target host when those
features matter. The runtime’s registered declarations decide whether a script
compiles.
