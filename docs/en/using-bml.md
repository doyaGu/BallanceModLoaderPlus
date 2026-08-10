# Use BML+

This guide is for players installing the BML+ runtime and mods. It does not
require a compiler or the BML+ SDK.

## Requirements

- BallancePlayer on Windows. The original Ballance player is not supported.
- The x86 Microsoft Visual C++ 2015–2022 Redistributable.
- A BML+ release compatible with the mods you intend to use.

Close Player before replacing `BMLPlus.dll`, `AngelScript.dll`, or a native
mod. Windows cannot safely replace a DLL while the game is using it.

## Install BML+

1. Download `BMLPlus-<version>.zip` from
   [BML+ Releases](https://github.com/doyaGu/BallanceModLoaderPlus/releases).
2. Extract the complete archive into the Ballance installation directory. Do
   not create an extra `BMLPlus-<version>` directory around its contents.
3. Check the resulting layout:

   ```text
   <Ballance>/
     Bin/Player.exe
     BuildingBlocks/BMLPlus.dll
     BuildingBlocks/AngelScript.dll   # present in script-capable releases
     ModLoader/
   ```

4. Start `Bin/Player.exe`.
5. Confirm that the BML+ version appears at the top of the game window. Open
   `ModLoader/ModLoader.log` and check that initialization completed without a
   loader error.

Press `/` to open the BML+ command bar. Run `bml` to print the loader version
and loaded mods.

## Install mods

Put supported packages in `ModLoader/Mods`:

- A native mod is normally a `.bmodp` DLL or a supported zip package containing
  the native mod and its files.
- A script mod is a single `*.mod.as` file, a directory containing exactly one
  `*.mod.as` entry, or a supported zip package containing exactly one entry.
- Script mods require the `AngelScript.dll` shipped with the matching BML+
  release.

Read the mod's own README for dependencies and required files. Do not keep a
development directory and a zip package with the same mod id in `Mods` at the
same time. Restart Player after adding a new mod or changing its dependencies.

Use `ModLoader/ModLoader.log` and the `bml` command to confirm that the mod was
found and loaded. Script authors can also use `script status` and
`script diag <id>`.

## Files created by BML+

| Path | Purpose |
| --- | --- |
| `ModLoader/Mods` | Installed native and script mods |
| `ModLoader/Configs` | BML+ and per-mod configuration |
| `ModLoader/ModLoader.log` | Loader, dependency, and mod diagnostics |
| `ModLoader/Fonts` | Optional fonts used by the BML+ interface |
| `ModLoader/Themes` | Command-bar color themes |

The `/` command bar provides command history and completion. Run `help` to see
the commands available in the current installation.

## Update BML+

Manual update is the normal recovery-safe path:

1. Close Player.
2. Download the new `BMLPlus-<version>.zip`.
3. Extract it into the Ballance installation directory and allow matching
   runtime files to be replaced.
4. Start Player and repeat the version and log checks from the installation
   section.

Do not extract `BMLPlus-Update-<version>.zip` manually. That archive is an
Updater payload and intentionally omits files that are not updated in place.

The optional updater changes BML+ runtime files only. It does not install,
remove, or update mods and configuration:

```powershell
Bin\Updater.exe check
Bin\Updater.exe update
Bin\Updater.exe doctor
```

If updater verification fails or the updater itself is old, install the latest
full manual package once, then try the updater again.

## Uninstall BML+

1. Close Player.
2. Remove `BuildingBlocks/BMLPlus.dll`.
3. Remove `BuildingBlocks/AngelScript.dll` only if it was installed with BML+
   and no other CKAngelScript integration needs it.

Keep `ModLoader` if you want to preserve mods, configuration, maps, and logs.
Delete that directory only when you intend to remove those files as well.

## Troubleshooting

### No BML+ version appears

- Confirm that `BMLPlus.dll` is directly under `BuildingBlocks`.
- Confirm that you launched `Bin/Player.exe` from BallancePlayer.
- Install the x86 Visual C++ 2015–2022 Redistributable.
- Remove mixed files from older loader installations, then reinstall from one
  complete BML+ package.

### A native mod does not load

- Confirm that the file uses the `.bmodp` extension or the package format
  documented by its author.
- Search `ModLoader/ModLoader.log` for the mod id, a missing dependency, an
  incompatible BML+ version, or a DLL load error.
- Test with only that mod and its required dependencies installed.

### A script mod does not load

- Confirm that the matching `BuildingBlocks/AngelScript.dll` exists.
- Confirm that the package has exactly one `*.mod.as` entry.
- Run `script status`, then `script diag <id>` and `script logs error`.
- Remove duplicate directory, single-file, or zip copies of the same mod id.

### The game becomes slow or unstable after installing a mod

- Test with that mod disabled before changing BML+ settings.
- Re-enable mods one at a time to find a conflict.
- Attach `ModLoader/ModLoader.log`, the BML+ version, the mod versions, and the
  steps needed to reproduce the problem when filing an issue.

Report reproducible BML+ problems in the
[issue tracker](https://github.com/doyaGu/BallanceModLoaderPlus/issues).
