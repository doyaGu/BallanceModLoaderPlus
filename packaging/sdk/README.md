# BML+ SDK

[简体中文](README_zh-CN.md)

This archive is for Mod development. It is not the BML+ runtime package and
must not be extracted into the Ballance directory.

## SDL SDK on Windows, Linux and macOS

Choose the SDK matching the game's runtime, platform, architecture, host linkage,
compiler, C++ standard library and runtime. The SDL CLI currently supports dynamic hosts.

On Linux and macOS, use Python 3.10+ through the executable POSIX launcher:

```sh
"<BML-SDK>/scripts/bml" new native yourname.my-mod
cd MyMod
./bml build
./bml run --ballance-root "/path/to/game"
./bml pack
```

Use `new script` for the same script workflow. Windows keeps `bml.cmd`.
SDL native builds use the configuration in `share/BML/BML.abi.json` and need
matching CK2, VxMath and SDL3 development packages; they do not use the retail
Virtools SDK. Pass additional installed dependency prefixes with `--package-root`.
Select another CMake generator with `--generator`; Ninja or Unix Makefiles is
selected on POSIX, and Visual Studio on Windows. Set `CXX` in a fresh build if
the matching compiler is not the default. A non-Visual-Studio Windows generator
requires an initialized compiler environment.

The game path is its root: `Bin/Player` (Windows: `Bin/Player.exe`) or a flat
`Player` executable alongside `base.cmo`. The runtime must include
`ModLoader/BML.abi.json`. Native deployment checks it against the SDK before
replacing files. Script runs require BML in `Managers` and CKAngelScript in
`BuildingBlocks`, using the platform's shared-library names.

`build` stages `.bmodp` and `.bmodp.abi.json` together. SDL `pack` produces a ZIP
containing both files, with atomic replacement and `--force` protection. Install
that ZIP in `ModLoader/Mods`, or copy both staged files there. A bare SDL
`.bmodp` without its descriptor is rejected. The SDK directory can be relocated; do not copy SDK engine
libraries into an existing game installation.

SDL Mods reuse Player's SDL library. macOS SDL builds require CMake 3.19+.
On macOS, `bml_add_mod` normalizes SDL load
commands to `@rpath` and renews the Mod's ad hoc signature after linking, even
when the development SDL uses an absolute install name. The built `.bmodp` is
ready to copy into `ModLoader/Mods`; deployment needs no binary edits and must
not include another SDL. Apply your own distribution signature after building.

## CMake

```cmake
find_package(BML CONFIG REQUIRED)
bml_add_mod(MyMod src/MyMod.cpp)
bml_install_mod(MyMod)
```

Use `basic`, `interface-provider`, `interface-consumer`, or `imc-provider` with
`bml new native --profile`. Script projects use `bml new script`.
The full guides are installed under `share/BML/docs`.
