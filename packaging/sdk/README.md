# BML+ SDL SDK

[简体中文](README_zh-CN.md)

This package is for Mod development. Install the matching **runtime** in the game;
keep the SDK in a separate source workspace. This branch supports dynamic SDL
hosts. Static-host SDKs and original Windows binary Mods are not supported here.

## Start a project

Python 3.10+ is required. The following commands work in all supported shells
(use `python3` if that is your Python executable):

```sh
python "/path/to/BML-SDK/scripts/bml.py" new native yourname.my-mod
cd MyMod
python bml.py build
python bml.py run --ballance-root "/path/to/game"
python bml.py pack
```

Use `new script` for AngelScript. POSIX launchers are `scripts/bml` and project
`./bml`; Windows launchers are `scripts/bml.cmd` and project `bml.cmd`.
`--profile` selects `basic`, `interface-provider`, `interface-consumer`, or
`imc-provider` for native projects. `python bml.py help --verbose` lists commands.

The game root contains `base.cmo`, with `Bin/Player` or a flat `Player` executable
(`Player.exe` on Windows). It must include `ModLoader/BML.abi.json`. Do not pass
its `Bin` subdirectory as the game root. `build` prepares artifacts; `run` also
deploys and starts Player. Script `run` synchronizes saved sources for hot reload.

## ABI and dependencies

Choose a package matching runtime, platform, architecture, host linkage and
protocol/SDK ABI. `share/BML/BML.abi.json` describes the SDK; deployment compares
it with the installed runtime before replacing files.

- Core native Mods use the C11 protocol (`BML_GetModInterface`) and module-local
  C++20 wrappers. They need only `BML::BML`; the core C seam does not expose C++
  STL, virtual tables, exceptions or allocator ownership.
- Direct CK2/VxMath access opts into `BML::Engine`. Direct host ImGui access opts
  into `BML::ImGui`. These extensions require the matching engine/ImGui ABI,
  compiler, C++ standard library, runtime and build configuration. Supply matching
  development packages with `--package-root`; never use the original Virtools SDK.
- Player owns CK2, VxMath, SDL and ImGui state. Do not ship another SDL or
  statically link another engine into a Mod.

The CLI takes its default configuration from the descriptor. Override it only
with a compatible SDK, using `--configuration`. `--generator` selects CMake's
generator; Windows generators need a working compiler environment. Use a fresh
build directory after changing compiler, architecture or SDK.

On macOS the SDK normalizes Mod SDL load commands to `@rpath` and renews the
ad hoc signature during linking. Deployment copies unchanged output. Apply any
distribution signature after the build. The CMake helper needs CMake 3.19+;
building BML+ itself requires CMake 3.25+.

## Manual CMake build

```cmake
cmake_minimum_required(VERSION 3.19)
project(MyMod LANGUAGES CXX)
find_package(BML CONFIG REQUIRED)
bml_add_mod(MyMod src/MyMod.cpp)
bml_install_mod(MyMod)
```

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="/path/to/BML-SDK" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix "/path/to/game/ModLoader"
```

Keep `.bmodp` and `.bmodp.abi.json` together. `pack` produces a ZIP with both
and rejects accidental overwrite without `--force`. Native `pack` currently
contains only that pair; projects with extra resources need their own CMake
resource-package step. Script `pack` includes source/resources and excludes
editor stubs and local settings. Copy that ZIP into `ModLoader/Mods`.
Missing or incompatible sidecars are rejected before loading native code.

The SDK is relocatable. Full guides live in `share/BML/docs`; editor declarations
live in `docs/api`. See `examples/native-mod`, `examples/native-c-mod`, the
interface provider/consumer pair, and (when enabled) `examples/script-mod`.
