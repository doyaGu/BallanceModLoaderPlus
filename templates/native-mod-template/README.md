# BML+ Native Mod Template

Minimal CMake-based BML+ mod with a sample command.

Read the SDK's `share/BML/docs/en/modding.md` before choosing the native route,
then use `share/BML/docs/en/native-mod-api.md` for the native API and ownership
rules. The same pages are published at
[Create mods](https://doyagu.github.io/BallanceModLoaderPlus/modding/) and
[Native mod API](https://doyagu.github.io/BallanceModLoaderPlus/native-mod-api/).

## Prerequisites

- Windows + Visual Studio 2019+ (C++20)
- CMake 3.15+ (`bml_add_mod` needs policy CMP0091 to pin the MSVC runtime)
- Virtools SDK 2.1
- An extracted BML+ SDK (so that `BMLConfig.cmake` is available).

If you built BML+ from source, configure that build with
`-DCMAKE_INSTALL_PREFIX="<BML-SDK>"`, then install its SDK:

```powershell
cmake --build <BML-build-dir> --config Release --target install
```

Then pass `<BML-SDK>` through `CMAKE_PREFIX_PATH` when configuring this
template.

## Configure

Ballance and the Virtools SDK are 32-bit. First run `cmake --help` and copy the
exact Visual Studio generator name available on your machine. The example below
uses Visual Studio 2022; use `Visual Studio 16 2019` for Visual Studio 2019.
The generator must use the Win32 target platform.

```powershell
$generator = "Visual Studio 17 2022"
cmake -S . -B build -G $generator -A Win32 `
  -DCMAKE_PREFIX_PATH="<path-to-BML-SDK>" `
  -DVIRTOOLS_SDK_PATH="<path-to-Virtools-SDK-2.1>" `
  -DCMAKE_INSTALL_PREFIX="<Ballance>/ModLoader"
```

`CMAKE_INSTALL_PREFIX` names the existing `ModLoader` directory, not the
Ballance root. Keep the source and build paths short, for example
`C:\Mods\HelloMod`; MSBuild file tracking can fail before compilation in deeply
nested paths.

## Build And Install For Testing

Close Player before replacing a loaded native mod, then run:

```powershell
cmake --build build --config RelWithDebInfo --target install
```

This builds `HelloMod.bmodp` and installs it to
`<Ballance>/ModLoader/Mods/HelloMod.bmodp`. Start that installation's
`Bin/Player.exe`, wait until the main menu is visible, and check both the
in-game greeting and `ModLoader/ModLoader.log` for the `HelloMod loaded` line.
Type `hello` in the BML+ command bar to verify the sample command. The BML+
version banner or loader initialization line alone does not prove that this
Mod loaded.

The runtime in `BMLPlus-<version>.zip` uses the Release MSVC runtime. Do not
load a Debug `.bmodp` into it: the native API passes C++ objects across the DLL
boundary, so the Debug and Release runtimes are not ABI-compatible.
`RelWithDebInfo` keeps debug information while using the compatible Release
runtime. Build with `--config Release` and test that exact artifact before
publishing it.

`bml_add_mod` pins this Mod's MSVC runtime to the one the configured SDK was
built against, so `--config Debug` still produces a loadable `.bmodp`. The one
case where a Debug Mod is intended is the Debug SDK:
`BMLPlus-SDK-<version>-Debug.zip` ships a Debug `bin/BMLPlus.dll` and its
`.pdb`, and a Mod built against it requires that Debug loader to be copied over
`BuildingBlocks/BMLPlus.dll`. Do not mix the two SDKs in one installation.

## Notes

- Entry point: `BMLEntry(IBML*) -> IMod*`
- Cleanup: `BMLExit(IMod*)` destroys the object returned by `BMLEntry` in the
  same DLL and is required for new mods.
- Registers a sample command: `hello [name]`
- `bml_add_mod` requires a 32-bit MSVC-compatible target and verifies both
  loader entry points while linking the BML SDK.
- `bml_add_mod` enables C++20 and applies the `.bmodp` suffix.
- `bml_install_mod` installs the target under the configured `Mods` directory.

This mod demonstrates loading and one command. It is not an API catalog;
use the installed headers and SDK documentation for additional services.
