# BML+ script API reference

The downloadable reference files describe the AngelScript API exposed by BML+.
They are intended for editor completion and API lookup; they are not runtime
script files and must not be included in a mod package.

<ul>
  <li><a href="../../api/as.predefined"><code>as.predefined</code></a> — complete definitions for AngelScript Language Server, including CKAngelScript and BML+.</li>
  <li><a href="../../api/bml-script-mod-api.as"><code>bml-script-mod-api.as</code></a> — BML+ script mod types, functions, callbacks, and properties.</li>
  <li><a href="../../api/bml-imgui-api.as"><code>bml-imgui-api.as</code></a> — Dear ImGui bindings available to scripts.</li>
</ul>

An accessor named `get_Name()` defines an AngelScript property and is normally
used as `object.Name`. A declaration containing `@+` transfers a retained
reference to BML+ when the corresponding registration succeeds.

Use `as.predefined` for editor tooling. Use the two smaller files when looking
up only the BML+-owned API surface.
