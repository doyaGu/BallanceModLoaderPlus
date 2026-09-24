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
used as `object.Name`. A declaration containing `@+` transfers a retained
reference to BML+ when the corresponding registration succeeds.

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

// Add your [bml.mod] annotation to this class as shown in the lifecycle guide.
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
