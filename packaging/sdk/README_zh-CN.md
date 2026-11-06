# BML+ SDL SDK

[English](README.md)

本包用于 Mod 开发。游戏中安装配套的**运行时包**，SDK 留在独立源码工作区。
本分支支持动态 SDL 宿主，尚不提供静态宿主 SDK，也不接受原 Windows Mod 二进制。

## 创建项目

需要 Python 3.10+。以下命令跨平台通用；系统命令为 `python3` 时替换 `python`：

```sh
python "/path/to/BML-SDK/scripts/bml.py" new native yourname.my-mod
cd MyMod
python bml.py build
python bml.py run --ballance-root "/path/to/game"
python bml.py pack
```

脚本项目使用 `new script`。POSIX 还可运行 `scripts/bml` 和项目 `./bml`，
Windows 使用 `scripts/bml.cmd` 和项目 `bml.cmd`。原生 `--profile` 支持 `basic`、
`interface-provider`、`interface-consumer`、`imc-provider`。
`python bml.py help --verbose` 列出完整命令。

游戏根目录包含 `base.cmo`，Player 位于 `Bin/Player` 或根目录（Windows 为
`Player.exe`），并须有 `ModLoader/BML.abi.json`。不要把 `Bin` 当作根目录。
`build` 准备产物；`run` 还会部署并启动 Player。脚本 `run` 会同步保存后的源码，
让已加载 Mod 自动热重载。

## ABI 与依赖

SDK 须匹配运行时、平台、架构、宿主链接方式和协议/SDK ABI。
SDK 描述为 `share/BML/BML.abi.json`，部署前与游戏运行时描述比对。

- 核心原生 API 使用 C11 协议 `BML_GetModInterface` 和模块内 C++20 wrapper，
  只需 `BML::BML`。C 边界不传递 STL、虚表、异常或跨模块分配器所有权。
- 直接访问 CK2/VxMath 才启用 `BML::Engine`，直接使用宿主 ImGui 才启用
  `BML::ImGui`。这些扩展须匹配引擎/ImGui ABI、编译器、标准库、运行库和构建配置。
  用 `--package-root` 提供相应开发包，不得使用原 Virtools SDK。
- CK2、VxMath、SDL 和 ImGui 共享状态归 Player 所有。Mod 不得携带第二份 SDL，
  也不得再次静态链接引擎。

CLI 默认使用描述中的配置；只有匹配的 SDK 才能用 `--configuration` 指定其他配置。
`--generator` 选择 CMake 生成器，Windows 需要已配置的编译器环境。
更换工具链、架构或 SDK 后使用全新构建目录。

macOS 的 SDK 在链接时将 SDL 依赖规范为 `@rpath` 并更新临时签名，部署直接复制
未经修改的产物。发行签名应在构建后执行。SDK CMake helper 需要 CMake 3.19+，
BML+ 自身构建需要 CMake 3.25+。

## 手动 CMake 构建

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

`.bmodp` 与 `.bmodp.abi.json` 必须一起携带。原生 `pack` 目前只将模块和 ABI 描述放入 ZIP，
额外资源需要项目自己的 CMake 资源打包步骤。脚本 `pack` 包含源码与资源，排除
编辑器声明和本地设置；覆盖须显式 `--force`。把 ZIP 放入 `ModLoader/Mods`。
缺失或不兼容的 ABI 描述会在执行原生入口前被拒绝。

SDK 可移动目录。完整指南位于 `share/BML/docs`，补全声明位于 `docs/api`。
示例包括 `examples/native-mod`、`examples/native-c-mod`、接口提供方/使用方，以及
启用脚本时的 `examples/script-mod`。
