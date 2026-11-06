# Ballance Mod Loader Plus (BML+)

English | [简体中文](README_zh-CN.md)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-blue.svg)]()

BML+ is a mod loader for [BallancePlayer](https://github.com/doyaGu/BallancePlayer).
It loads native and AngelScript mods and supplies the lifecycle, configuration,
commands, UI, engine access, and inter-mod communication used by those mods.

BML+ supports BallancePlayer only. It does not support the original Ballance
player.

## Start here

| I want to... | Read |
| --- | --- |
| Install BML+, install mods, or solve a loading problem | [Use BML+](https://doyagu.github.io/BallanceModLoaderPlus/using-bml/) |
| Create a native or script mod | [Create mods](https://doyagu.github.io/BallanceModLoaderPlus/modding/) |
| Build or change the BML+ loader | [Contribute to BML+](https://doyagu.github.io/BallanceModLoaderPlus/contributing/) |
| Build the native mods maintained in this repository | [In-tree mods](mods/README.md) |

Mod authors should use a released BML+ SDK. Building this repository is needed
only when changing BML+ or its in-tree mods.

## Install and verify

Use the matching Ballanced SDL runtime package and BML+ runtime/SDK for your
platform and architecture. Native Mods use a C ABI; optional engine/ImGui
extensions also match the host dependency/compiler/runtime ABI. Install the Loader
in `Managers`; keep Player's CK2, VxMath and SDL libraries. See the
[installation guide](docs/en/using-bml.md). The original Windows engine backend
is maintained outside this `sdl` branch. Static-host integration remains pending.


## Project links

- [Documentation](https://doyagu.github.io/BallanceModLoaderPlus/)
- [Simplified Chinese documentation](https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/)
- [Releases](https://github.com/doyaGu/BallanceModLoaderPlus/releases)
- [Maintainer release runbook](RELEASING.md)
- [Issues](https://github.com/doyaGu/BallanceModLoaderPlus/issues)
- [BallancePlayer](https://github.com/doyaGu/BallancePlayer)

BML+ is licensed under the [MIT License](LICENSE). It is based on Gamepiaynmo's
original BallanceModLoader and work from the Ballance modding community.
