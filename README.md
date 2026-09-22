# Ballance Mod Loader Plus (BML+)

English | [简体中文](README_zh-CN.md)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platform](https://img.shields.io/badge/platform-Windows-blue.svg)]()

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

1. Download `BMLPlus-<version>.zip` from
   [Releases](https://github.com/doyaGu/BallanceModLoaderPlus/releases).
2. Extract the complete archive into the Ballance installation directory.
3. Confirm that `BuildingBlocks/BMLPlus.dll` exists. A script-capable release
   also contains `BuildingBlocks/AngelScript.dll`.
4. Start `Bin/Player.exe`.
5. Confirm that the BML+ version appears at the top of the game window, then
   check `ModLoader/ModLoader.log` for load errors.

`BMLPlus-Update-<version>.zip` is an updater payload, not a manual installation
package. Use the full `BMLPlus-<version>.zip` archive for manual installation
and recovery.

`BMLPlus-Mods-<version>.zip` is optional. Extract it into the Ballance root to
install all in-tree Mods, or take individual packages from its
`ModLoader/Mods/` directory. The main installer includes only the three
default utility Mods.

Press `/` in game to open the command bar. The `bml` command prints the loader
version and loaded mods.

## Project links

- [Documentation](https://doyagu.github.io/BallanceModLoaderPlus/)
- [Simplified Chinese documentation](https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/)
- [Releases](https://github.com/doyaGu/BallanceModLoaderPlus/releases)
- [Maintainer release runbook](RELEASING.md)
- [Issues](https://github.com/doyaGu/BallanceModLoaderPlus/issues)
- [BallancePlayer](https://github.com/doyaGu/BallancePlayer)

BML+ is licensed under the [MIT License](LICENSE). It is based on Gamepiaynmo's
original BallanceModLoader and work from the Ballance modding community.
