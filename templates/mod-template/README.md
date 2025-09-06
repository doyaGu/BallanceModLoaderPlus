# BML+ Mod Template

Minimal CMake-based template to build a BML+ mod with a sample command.

## Prerequisites

- Windows + Visual Studio 2019+ (C++20)
- CMake 3.14+
- BML installed (so that `BMLConfig.cmake` is available).

If you built BML from source, run `cmake --install .` on the BML project first, then set `CMAKE_PREFIX_PATH` to BML's install prefix when configuring this template.

## Build

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<path-to-BML-install>"
cmake --build build --config Release
```

The resulting DLL will be named `HelloMod.dll`. Drop it into `ModLoader/Mods/HelloMod.bmodp` (or directly as a DLL depending on your workflow).

## Notes

- Entry point: `BMLEntry(IBML*) -> IMod*`
- Cleanup (optional): `BMLExit(IMod*)`
- Registers a sample command: `hello [name]`

