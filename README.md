# Ballance Mod Loader Plus (BML+)

English | [简体中文](README_zh-CN.md)

BML+ **0.4.0** loads Native and AngelScript Mods into **Ballanced's SDL Player**.
It supplies Mod lifecycle events, configuration, commands, input, UI, Behavior authoring,
resources and inter-Mod communication.

This checkout is the `sdl` branch. It builds a **dynamic** Loader for the SDL CK2
and VxMath runtime. The original Windows Player and its old C++ Mod ABI are
maintained separately. Static Player integration is not implemented here.

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

Use a runtime and SDK built for the same SDL target. A generic “latest” BML
archive may belong to another runtime; check `ModLoader/BML.abi.json` in the game
and `share/BML/BML.abi.json` in the SDK. If a matching package is unavailable,
build this branch against the matching Ballanced `sdl` sources.

## Runtime and SDK

Extract the **runtime** into the game root. The Loader goes in `Managers`,
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

Install CMake 3.25+, a C++20 compiler and Python 3.10+. Use NeMo2's exported
CK2/VxMath packages and the SDL3 development package matching your Player.
Initialize the submodules and use a fresh build directory:

```sh
git submodule update --init --recursive
cmake -S . -B build/sdl -DBML_RUNTIME=sdl \
  -DCMAKE_PREFIX_PATH="/path/to/engine-sdk;/path/to/SDL-prefix" \
  -DCKANGELSCRIPT_ROOT=/path/to/CKAngelScript \
  -DCMAKE_BUILD_TYPE=Release -DBML_BUILD_MODS=ON -DBML_BUILD_TESTS=ON
cmake --build build/sdl --config Release --parallel
ctest --test-dir build/sdl -C Release --output-on-failure
```

Scripts are enabled by default. `-DBML_ENABLE_ANGELSCRIPT=OFF` builds the
script-free variant. See the contribution guide for obtaining the matching SDK,
installing components and running the Player tests.

## Project links

- [Documentation site](https://doyagu.github.io/BallanceModLoaderPlus/)
- [中文文档](https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/)
- [Ballanced](https://github.com/doyaGu/Ballanced)
- [Releases](https://github.com/doyaGu/BallanceModLoaderPlus/releases)
- [Issues](https://github.com/doyaGu/BallanceModLoaderPlus/issues)

BML+ is licensed under the [MIT License](LICENSE) and builds on Gamepiaynmo's
BallanceModLoader and work from the Ballance modding community.
