# BML+ for Ballanced SDL

This documentation describes the current `sdl` branch and its versioned Native
C ABI, module-local C++ SDK and AngelScript authoring surface. The old IMod/IBML
system has been removed from this branch, including private implementations.

| Task | Start here |
| --- | --- |
| Install the Loader or a Mod | [Use BML+](using-bml.md) |
| Check an installation or apply a signed update | [Updater and doctor](updater-sdl.md) |
| Check target support and remaining acceptance | [Validation status](validation.md) |
| Create a Mod | [Choose a development route](modding.md) |
| Use C or C++ | [Native Mod API](native-mod-api.md) |
| Port an old Native Mod | [Migration](native-mod-migration.md) |
| Use AngelScript | [Script Mods](script-mod/index.md) |
| Execute or edit Behavior graphs | [Behavior authoring](behavior-authoring.md) |
| Publish a typed Mod service | [IMC](imc-author-guide.md) |
| Change or build BML+ | [Contributing](contributing.md) |

The runtime belongs in the game; the SDK belongs in a development workspace.
Use the ABI descriptors to identify matching packages. Dynamic SDL hosts are
implemented; static Player integration and unexecuted targets remain pending.

The [Chinese tutorial](https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/script-mod-tutorial/) provides a longer
step-by-step script course. Editor declarations are listed in the
[script reference](script-mod/api.md); they are not runtime includes.
