# Ballance Mod Loader Plus（BML+）

[English](README.md) | 简体中文

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-blue.svg)]()

BML+ 是 [BallancePlayer](https://github.com/doyaGu/BallancePlayer) 使用的 Mod
Loader。它可以装载原生 Mod 和 AngelScript Mod，并为这些 Mod 提供生命周期、
配置、命令、UI、引擎访问和跨 Mod 通信。

BML+ 只支持 BallancePlayer，不支持原版 Ballance Player。

## 从这里开始

| 你要做什么 | 阅读 |
| --- | --- |
| 安装 BML+、安装 Mod 或排查加载问题 | [使用 BML+](https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/using-bml/) |
| 编写原生 Mod 或脚本 Mod | [开发 Mod](https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/modding/) |
| 构建或修改 BML+ Loader | [参与 BML+ 开发](https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/contributing/) |
| 构建仓库内维护的原生 Mod | [仓库内 Mod](mods/README_zh-CN.md) |

Mod 作者应使用已发布的 BML+ SDK。只有修改 BML+ 或仓库内的 Mod 时才需要构建本仓库。

## 安装与验证

使用匹配平台、架构的 Ballanced SDL 运行包与 BML+。原生 Mod 使用 C ABI，
显式 Engine/ImGui 扩展另需匹配宿主依赖、编译器和运行库 ABI。
Loader 安装在 `Managers`，CK2、VxMath 和 SDL 复用 Player 随包的库。
详见[安装说明](docs/zh-CN/using-bml.md)。本 `sdl` 分支不包含原版 Windows 引擎后端，
静态宿主集成仍待完成。


## 项目链接

- [简体中文文档](https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/)
- [English documentation](https://doyagu.github.io/BallanceModLoaderPlus/)
- [发布包](https://github.com/doyaGu/BallanceModLoaderPlus/releases)
- [维护者发布操作手册](RELEASING.md)
- [问题反馈](https://github.com/doyaGu/BallanceModLoaderPlus/issues)
- [BallancePlayer](https://github.com/doyaGu/BallancePlayer)

BML+ 使用 [MIT License](LICENSE)。项目基于 Gamepiaynmo 的原版
BallanceModLoader，并包含 Ballance Mod 社区贡献的工作。
