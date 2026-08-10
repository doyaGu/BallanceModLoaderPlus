# BML+ Script Mod Template

This is the smallest supported starting point for a BML+ script mod. If you
are unsure which development route to choose, start here.

Read the SDK's `share/BML/docs/en/modding.md`, then
`share/BML/docs/en/script-mod/index.md`. The same pages are published at
[Create mods](https://doyagu.github.io/BallanceModLoaderPlus/modding/) and
[Script mods](https://doyagu.github.io/BallanceModLoaderPlus/script-mod/).

## Run the template

1. Copy this directory to `<Ballance>/ModLoader/Mods/HelloScript`.
2. Confirm that the matching `BuildingBlocks/AngelScript.dll` is installed.
3. Start `Bin/Player.exe` without editing the template.
4. Look for the greeting in game and `Hello Script loaded` in
   `ModLoader/ModLoader.log`.
5. Replace the example id, name, and author, then restart Player once because
   the Mod identity changed.

BML+ discovers a new Mod only during Player startup. After the Mod has loaded,
saving a source file in a directory package triggers automatic hot reload.
Changing the Mod id or dependencies still requires a restart.

For editor completion, open `ModLoader/Mods` as the workspace and place the
SDK's `docs/api/as.predefined` in that workspace root. Do not package the API
stub with the Mod.

If the Mod does not load, use the BML+ command bar:

```text
script status
script diag example.hello.script
script logs error
```

## Package the Mod

Use the packer shipped in the SDK:

```powershell
Set-Location "<Ballance>/ModLoader/Mods/HelloScript"
& "<BML-SDK>/scripts/Pack-BMLScriptMod.ps1" -Force
```

The package is written to `dist/HelloScript.zip`. Pass `-Source` or `-Output`
only when packaging from another directory or writing the zip elsewhere. The
packer omits editor settings, version-control metadata, `as.predefined`, Python
cache files, and the `dist` directory.

Test the zip without the development directory installed; two packages with
the same Mod id conflict. `.bmodp` is reserved for native DLL mods.

This template proves discovery, compilation, lifecycle entry, logging, and
in-game output. Add commands, configuration, timers, UI, and engine access only
after this file works.
