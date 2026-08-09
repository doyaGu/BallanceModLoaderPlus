# BML+ Script Mod Template

Minimal BML+ script mod template using the current script API:

- one arbitrary `*.mod.as` entry file
- AngelScript metadata declarations
- fixed callback signatures
- script-owned Timer and Command objects
- typed BML service facades

## Develop In The Mods Directory

Copy this template out of the SDK and use the copy as the working directory:

```text
<Ballance>/ModLoader/Mods/HelloScript/
```

Make sure CKAngelScript is installed as `BuildingBlocks/AngelScript.dll`, then
start Player. Directory script mods use automatic hot reload by default. Keep
Player open and edit the installed copy; BML reloads it after saved `.as`
source files change.

Use these commands in the BML command bar when needed:

```text
script status
script reload example.hello.script
script diag example.hello.script
script logs error
script panel
```

`script watch on` re-enables automatic reload if it was disabled. Adding a new
script mod, changing its id, or changing its dependency graph still requires a
Player restart.

For editor completion, place the SDK's `docs/api/as.predefined` at the
`ModLoader/Mods` workspace root. Do not put it inside this mod directory.

## Package As Zip

Use the packer shipped in the extracted BML+ SDK:

```powershell
& "<BML-SDK>/scripts/Pack-BMLScriptMod.ps1" `
  -Source "<Ballance>/ModLoader/Mods/HelloScript" `
  -Output "dist/HelloScript.zip" `
  -Force
```

Install the resulting zip at:

```text
<Ballance>/ModLoader/Mods/HelloScript.zip
```

Test the zip in a clean `ModLoader/Mods` directory so the development directory
and zip do not declare the same mod id at the same time. Zip packages use manual
reload and are intended for distribution and final package validation.

`.bmodp` is reserved for native DLL mods. Do not use `.bmodp` for script mods.
