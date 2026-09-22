# In-tree native Mods

English | [简体中文](README_zh-CN.md)

This directory contains the native Mods formerly maintained in BallanceMods.
EditorMode, TASSupport, and the experimental Mod directories are intentionally
not imported. The original BallanceMods workspace is unchanged.

The normal BML+ build does not compile these Mods. To include them in a Win32
build, configure with `-DBML_BUILD_MODS=ON`:

```powershell
cmake -S . -B build-mods -A Win32 -DBML_BUILD_MODS=ON
cmake --build build-mods --config RelWithDebInfo
cmake --install build-mods --config RelWithDebInfo --prefix build-mods-install
```

IVP's own tests remain separately controlled by `IVP_BUILD_TESTS`; see the
[IVP build and test instructions](IVP/README.md#build-and-test).

Each Mod keeps its own source and CMake file. `cmake/BallanceMod.cmake` handles
the shared build and packaging rules. The resources under `3D Entities` are
packaged with their Mod rather than installed as loose files.
Build `BallStickyPackage` or `BMLModulsPackage` to regenerate an archive after
changing a resource without rebuilding the DLL. Each build configuration has
its own archive under its Mod's build directory.

The install tree contains all 14 packages under `Mods/` and their inventory at
`share/BML/mods.txt`. Copy an individual `.bmodp` or `.zip` into the game's
`ModLoader/Mods/` to enable it. Release builds also produce the optional
`BMLPlus-Mods-<version>.zip`, whose contents can be extracted at the game root.
The main BML+ installer enables only CameraUtilities, DebugUtilities, and
TravelMode by default; the updater never changes installed Mods.
