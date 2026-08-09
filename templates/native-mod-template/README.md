# BML+ Native Mod Template

Minimal CMake-based template to build a BML+ mod with a sample command.

## Prerequisites

- Windows + Visual Studio 2019+ (C++20)
- CMake 3.14+
- Virtools SDK 2.1
- BML installed (so that `BMLConfig.cmake` is available).

If you built BML from source, run `cmake --install .` on the BML project first, then set `CMAKE_PREFIX_PATH` to BML's install prefix when configuring this template.

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
- `bml_add_mod` links the BML SDK, enables C++20, and applies the `.bmodp` suffix.
- `bml_install_mod` installs the target under the configured `Mods` directory.
