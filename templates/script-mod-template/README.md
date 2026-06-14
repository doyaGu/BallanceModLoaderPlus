# BML+ Script Mod Template

Minimal BML+ script mod template using the v1 contract:

- one arbitrary `*.mod.as` entry file
- AngelScript metadata declarations
- fixed callback signatures
- script-owned Timer and Command objects
- typed export registry

## Install For Testing

Copy this directory to:

```text
<Ballance>/ModLoader/Mods/HelloScript/
```

Make sure CKAngelScript is installed as `BuildingBlocks/AngelScript.dll`, then start Player.

## Package As Zip

From the BML+ repository root:

```powershell
scripts/Pack-BMLScriptMod.ps1 `
  -Source templates/script-mod-template `
  -Output dist/HelloScript.zip `
  -Force
```

Install the resulting zip at:

```text
<Ballance>/ModLoader/Mods/HelloScript.zip
```

`.bmodp` is reserved for native DLL mods. Do not use `.bmodp` for script mods.
