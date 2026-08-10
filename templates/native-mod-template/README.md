# BML+ Native Mod Template

Minimal CMake-based template to build a BML+ mod with a sample command.

Read the SDK's `share/BML/docs/en/modding.md` before choosing the native route,
then use `share/BML/docs/en/native-mod-api.md` for the native API and ownership
rules. The same pages are published at
[Create mods](https://doyagu.github.io/BallanceModLoaderPlus/modding/) and
[Native mod API](https://doyagu.github.io/BallanceModLoaderPlus/native-mod-api/).

## Prerequisites

- Windows + Visual Studio 2019+ (C++20)
- CMake 3.14+
- Virtools SDK 2.1
- BML installed (so that `BMLConfig.cmake` is available).

If you built BML+ from source, configure that build with
`-DCMAKE_INSTALL_PREFIX="<BML-SDK>"`, then install its SDK:

```powershell
cmake --build <BML-build-dir> --config Release --target install
```

Then pass `<BML-SDK>` through `CMAKE_PREFIX_PATH` when configuring this
template.

## Configure

Ballance and the Virtools SDK are 32-bit. The Visual Studio generator must use
the Win32 target platform.

```powershell
cmake -S . -B build -A Win32 `
  -DCMAKE_PREFIX_PATH="<path-to-BML-SDK>" `
  -DVIRTOOLS_SDK_PATH="<path-to-Virtools-SDK-2.1>" `
  -DCMAKE_INSTALL_PREFIX="<Ballance>/ModLoader"
```

`CMAKE_INSTALL_PREFIX` names the existing `ModLoader` directory, not the
Ballance root.

## Build And Install For Testing

Close Player before replacing a loaded native mod, then run:

```powershell
cmake --build build --config Debug --target install
```

This builds `HelloMod.bmodp` and installs it to
`<Ballance>/ModLoader/Mods/HelloMod.bmodp`. Start `Bin/Player.exe` and check
`ModLoader/ModLoader.log` for the `HelloMod loaded` line.

Use `--config Release` for the artifact you publish.

## Notes

- Entry point: `BMLEntry(IBML*) -> IMod*`
- Cleanup: `BMLExit(IMod*)` destroys the object returned by `BMLEntry` in the
  same DLL and is required for new mods.
- Registers a sample command: `hello [name]`
- `bml_add_mod` requires a 32-bit MSVC-compatible target and verifies both
  loader entry points while linking the BML SDK.
- `bml_add_mod` enables C++20 and applies the `.bmodp` suffix.
- `bml_install_mod` installs the target under the configured `Mods` directory.

The template demonstrates loading and one command. It is not an API catalog;
use the installed headers and SDK documentation for additional services.
