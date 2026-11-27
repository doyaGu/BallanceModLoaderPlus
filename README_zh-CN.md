# Ballance Mod Loader Plus（BML+）

[English](README.md) | 简体中文

BML+ **0.4.0** 为 **Ballanced 的 SDL Player** 加载原生和 AngelScript Mod，提供生命周期事件、
配置、命令、输入、UI、Behavior 编写、资源和跨 Mod 通信。

当前是 `sdl` 分支，使用 SDL CK2 和 VxMath，支持**动态 Loader**。原版 Windows Player
及其旧 C++ Mod ABI 由独立分支维护；本分支尚未实现静态 Player 集成。

## 按任务开始

| 任务 | 文档 |
| --- | --- |
| 安装 Loader 和 Mod | [使用 BML+](docs/zh-CN/using-bml.md) |
| 编写原生或脚本 Mod | [开发 Mod](docs/zh-CN/modding.md) |
| 迁移已有原生 Mod | [迁移指南](docs/zh-CN/native-mod-migration.md) |
| 构建或修改 BML+ | [参与开发](docs/zh-CN/contributing.md) |
| 更新运行时 | [Updater 和 doctor](docs/zh-CN/updater-sdl.md) |
| 查看实际支持与验收范围 | [支持与验证](docs/zh-CN/validation.md) |
| 准备发布 | [发布手册（英文）](RELEASING.md) |

使用同一 SDL 目标的运行时与 SDK。不要把通用“最新版”归档当作本分支的配套包：
先核对游戏中的 `ModLoader/BML.abi.json` 和 SDK 的 `share/BML/BML.abi.json`。
没有匹配的 SDL 包时，从本分支与对应 Ballanced `sdl` 源码构建。

## 运行时与 SDK

**运行时**解压到游戏根目录：Loader 位于 `Managers`，CKAngelScript 位于
`BuildingBlocks`，Mod 位于 `ModLoader/Mods`。保留 Player 自带的 CK2、VxMath 和 SDL。
SDK 是独立开发包，不要覆盖到游戏目录。

原生 Mod 导出版本化 C 入口 `BML_GetModInterface`。C++ 作者使用模块内 wrapper 和
`BML_DEFINE_MOD`；C++ 对象与 STL 所有权留在 Mod 内部。旧 SDL `BMLEntry`/`IMod`
产物需要迁移和重编译。发布时将 `.bmodp` 与 `.bmodp.abi.json` 一起分发，或一起装入
ZIP；Linux/macOS 的模块也使用 `.bmodp` 扩展名。

普通 Mod 链接 `BML::BML`。直接使用 CK2/VxMath 或 ImGui 时显式添加
`BML::Engine`、`BML::ImGui`，并匹配对应依赖的编译器及运行库 ABI。
[仓库内 Mod](mods/README_zh-CN.md) 展示当前 SDK 的用法。

## 构建 Loader

准备 CMake 3.25+、C++20 编译器和 Python 3.10+。使用目标 Ballanced Player 锁定的
[NeMo2 `sdl` 分支](https://github.com/doyaGu/NeMo2/tree/sdl)提交构建的 CK2/VxMath 安装包，以及配套的 SDL3 开发包。初始化子模块后使用新的构建目录：

```sh
git submodule update --init --recursive
cmake -S . -B build/sdl -DBML_RUNTIME=sdl \
  -DCMAKE_PREFIX_PATH="/path/to/engine-sdk;/path/to/SDL-prefix" \
  -DCMAKE_BUILD_TYPE=Release -DBML_BUILD_MODS=ON -DBML_BUILD_TESTS=ON
cmake --build build/sdl --config Release --parallel
ctest --test-dir build/sdl -C Release --output-on-failure
```

默认启用脚本。CMake 会获取固定的 CKAngelScript SDL 提交，并使用 AngelScript 2.39.0
构建。设置 `CKANGELSCRIPT_ROOT` 可使用本地源码，设置 `-DBML_ENABLE_ANGELSCRIPT=OFF`
可构建无脚本版本。匹配 SDK 的获取方式、组件安装和 Player 测试流程见开发指南。

## 项目链接

- [文档站](https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/)
- [Ballanced](https://github.com/doyaGu/Ballanced)
- [发布包](https://github.com/doyaGu/BallanceModLoaderPlus/releases)
- [问题反馈](https://github.com/doyaGu/BallanceModLoaderPlus/issues)

BML+ 使用 [MIT 许可证](LICENSE)，基于 Gamepiaynmo 的 BallanceModLoader 及社区贡献。
