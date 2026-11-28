# 原生 Interface 提供方

[English](README.md)

该独立工程构建 C 协议原生 Mod，并发布名为 `BMLExampleValue` 的独立纯头文件
CMake 包。开发包安装前缀和玩家部署目录分开：

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="/path/to/BML-SDK" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix "/path/to/provider-package"
```

产物包括 `Mods/BMLExampleValueProvider.bmodp` 与 `.bmodp.abi.json`，
`include/BMLExample/ValueInterface.h`，以及
`lib/cmake/BMLExampleValue/BMLExampleValueConfig.cmake` 和 targets。
开发者拿头文件/CMake 包；玩家只需把 `Mods` 中模块及其描述复制进配套游戏的
`ModLoader/Mods`。

使用方链接 `BMLExampleValue::Interface`，不链接提供方二进制。
`Definition::Dependencies` 建立运行时保留关系，消费者先卸载。
本例只需核心 C ABI，不需要 Engine/ImGui 扩展。见[使用方](../native-interface-consumer/README_zh-CN.md)。
