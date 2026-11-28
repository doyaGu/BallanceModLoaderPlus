# Ballance Mod Loader Plus (BML+)

English | [简体中文](README_zh-CN.md)

BML+ **0.4.0** loads Native and AngelScript Mods into **Ballanced's SDL Player** and the Win32 **BallancePlayer 0.3.9**.
It supplies Mod lifecycle events, configuration, commands, input, UI, Behavior authoring,
resources and inter-Mod communication.

Both builds use the BML+ 0.4 Mod API and dynamic Loader. BallancePlayer Win32
uses the Virtools 2.1 engine and an x86 build.

## Choose your task

| Task | Guide |
| --- | --- |
| Install the Loader and Mods | [Use BML+](docs/en/using-bml.md) |
| Create a Native or Script Mod | [Mod development](docs/en/modding.md) |
| Port an existing Native Mod | [Migration](docs/en/native-mod-migration.md) |
| Build or change BML+ | [Contributing](docs/en/contributing.md) |
| Update an installed runtime | [Updater and doctor](docs/en/updater-sdl.md) |
| Check what has actually been validated | [Support and validation](docs/en/validation.md) |
| Prepare a release | [Release runbook](RELEASING.md) |

Use a runtime and SDK built for the same target. A generic “latest” BML
archive may belong to another runtime; check `ModLoader/BML.abi.json` in the game
and `share/BML/BML.abi.json` in the SDK. If a matching package is unavailable,
build this branch against the matching Ballanced `sdl` sources.

## Runtime and SDK

Extract the **runtime** into the game root. The Loader goes in `Managers` on SDL and `BuildingBlocks` on Virtools,
CKAngelScript in `BuildingBlocks`, and Mods in `ModLoader/Mods`. Keep Player's
CK2, VxMath and SDL libraries. An SDK is a separate development package; do not
extract it over a game installation.

Native Mods export the versioned `BML_GetModInterface` C entry. C++ authors use
module-local wrappers and `BML_DEFINE_MOD`; C++ objects and STL ownership stay
inside the Mod. Old SDL `BMLEntry`/`IMod` binaries require migration and rebuilding.
A Native Mod package keeps its `.bmodp` and `.bmodp.abi.json` together, directly
or in a ZIP. The module is an OS shared library even on Linux and macOS; the
`.bmodp` extension is the same on all platforms.

Core Mods use `BML::BML`. Direct CK2/VxMath and ImGui access opt into
`BML::Engine` and `BML::ImGui` and must match those dependencies' compiler and
runtime ABI. [In-tree Mods](mods/README.md) demonstrate the current SDK.

## Build the Loader

Install CMake 3.25+, a C++20 compiler and Python 3.10+. Use the CK2/VxMath packages from the [NeMo2 `sdl` branch](https://github.com/doyaGu/NeMo2/tree/sdl)
at the commit pinned by your Ballanced Player, and the matching SDL3 development package.
Initialize the submodules and use a fresh build directory:

```sh
git submodule update --init --recursive
cmake -S . -B build/sdl -DBML_RUNTIME=sdl \
  -DCMAKE_PREFIX_PATH="/path/to/engine-sdk;/path/to/SDL-prefix" \
  -DCMAKE_BUILD_TYPE=Release -DBML_BUILD_MODS=ON -DBML_BUILD_TESTS=ON
cmake --build build/sdl --config Release --parallel
ctest --test-dir build/sdl -C Release --output-on-failure
```

Scripts are enabled by default for both runtimes. Set `CKANGELSCRIPT_ROOT` to use the
reviewed dual-runtime CKAngelScript checkout, or `-DBML_ENABLE_ANGELSCRIPT=OFF` for a script-free build. See the
contribution guide for SDK setup, installation and Player tests.

### Build for BallancePlayer Win32

Use Visual Studio 2022 and the Virtools 2.1 SDK at
`175bcb12d829c687359b90615be53524cdba0132`. With scripting enabled,
`CKANGELSCRIPT_ROOT` selects the same dual-runtime CKAngelScript sources as SDL.
To use a matching installed CKAngelScript package, set BML_BUILD_SCRIPT_RUNTIME=OFF.

```powershell
cmake -S . -B build/virtools -G "Visual Studio 17 2022" -A Win32 -DBML_RUNTIME=virtools -DVIRTOOLS_SDK_PATH=C:/sdk/Virtools-SDK-2.1 -DCKANGELSCRIPT_ROOT=C:/src/CKAngelScript -DBML_BUILD_MODS=ON -DBML_BUILD_TESTS=ON
cmake --build build/virtools --config Release --parallel
cmake --install build/virtools --config Release --prefix build/virtools-runtime --component BMLRuntime
cmake --install build/virtools --config Release --prefix build/virtools-sdk
```

Use `-DBML_ENABLE_ANGELSCRIPT=OFF` in a separate build for a script-free runtime.
Player acceptance uses the dynamic BallancePlayer at `52bcdc9` and a copy of
the game data. [Player acceptance](tests/player/README.md)

## Project links

- [Documentation site](https://doyagu.github.io/BallanceModLoaderPlus/)
- [中文文档](https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/)
- [Ballanced](https://github.com/doyaGu/Ballanced)
- [Releases](https://github.com/doyaGu/BallanceModLoaderPlus/releases)
- [Issues](https://github.com/doyaGu/BallanceModLoaderPlus/issues)

BML+ is licensed under the [MIT License](LICENSE) and builds on Gamepiaynmo's
BallanceModLoader and work from the Ballance modding community.
