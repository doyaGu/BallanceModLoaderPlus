# BML+ Script Mod Template

This is the smallest supported starting point for a BML+ script mod. If you
are unsure which development route to choose, start here.

Read the SDK's `share/BML/docs/en/modding.md`, then
`share/BML/docs/en/script-mod/index.md`. The same pages are published at
[Create mods](https://doyagu.github.io/BallanceModLoaderPlus/modding/) and
[Script mods](https://doyagu.github.io/BallanceModLoaderPlus/script-mod/).

## Run the mod

From this project directory, run:

```bat
.\bml run
```

The first run asks for the Ballance folder, verifies the matching
`BuildingBlocks/AngelScript.dll`, deploys a managed copy, and starts Player.
Look for the greeting in game and `Hello Script loaded` in
`ModLoader/ModLoader.log`.

BML+ discovers a new Mod only during Player startup. After the Mod has loaded,
saving a source file is synchronized to the managed copy and triggers automatic
hot reload. Changing the Mod id or dependencies still requires a restart.

For editor completion, open this project directory as the workspace and copy the
SDK's `docs/api/as.predefined` into the project root. The Developer Workflow
keeps that editor-only file out of both the managed copy and the release zip.

If the Mod does not load, use the BML+ command bar:

```text
script status
script diag example.hello.script
script logs error
```

## Package the Mod

Run the same project-local workflow:

```bat
.\bml pack
```

The package is written to `dist/HelloScript.zip`. Pass `--project` or `--output`
only for automation; pass `--force` to replace an existing package. The workflow
omits its own files, local settings, editor settings, version-control metadata,
`as.predefined`, Python cache files, and the `dist` directory.

Test the zip without the development directory installed; two packages with
the same Mod id conflict. `.bmodp` is reserved for native DLL mods.

This mod proves discovery, compilation, lifecycle entry, logging, and
in-game output. After it works, use the SDK's `examples/script-mod` directory
for focused command/config, input/UI, and game-state examples. Copy only the
example you want to run.
