# Ballance Mod Loader Plus（BML+）

[English](README.md) | 简体中文

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platform](https://img.shields.io/badge/platform-Windows-blue.svg)]()

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

Mod 作者应使用已发布的 BML+ SDK。只有修改 BML+ 本身时才需要构建本仓库。

## 安装并确认运行正常

1. 从 [Releases](https://github.com/doyaGu/BallanceModLoaderPlus/releases)
   下载 `BMLPlus-<version>.zip`。
2. 将压缩包内的全部文件解压到 Ballance 安装目录。
3. 确认 `BuildingBlocks/BMLPlus.dll` 存在。支持脚本 Mod 的发布包还包含
   `BuildingBlocks/AngelScript.dll`。
4. 启动 `Bin/Player.exe`。
5. 确认游戏窗口顶部显示 BML+ 版本，再检查 `ModLoader/ModLoader.log` 中是否有
   加载错误。

`BMLPlus-Update-<version>.zip` 是 Updater 使用的更新载荷，不能作为手动安装包。
手动安装、覆盖更新和修复安装都应使用完整的 `BMLPlus-<version>.zip`。

在游戏中按 `/` 打开命令栏。输入 `bml` 可以查看 Loader 版本和已加载的 Mod。

## 项目链接

- [简体中文文档](https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/)
- [English documentation](https://doyagu.github.io/BallanceModLoaderPlus/)
- [发布包](https://github.com/doyaGu/BallanceModLoaderPlus/releases)
- [问题反馈](https://github.com/doyaGu/BallanceModLoaderPlus/issues)
- [BallancePlayer](https://github.com/doyaGu/BallancePlayer)

BML+ 使用 [MIT License](LICENSE)。项目基于 Gamepiaynmo 的原版
BallanceModLoader，并包含 Ballance Mod 社区贡献的工作。
