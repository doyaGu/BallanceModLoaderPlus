# 原生 Interface 使用方

[English](README.md)

先构建、安装[提供方开发包](../native-interface-provider/README_zh-CN.md)。
本工程不依赖提供方源码，需要同时传入 BML SDK 与提供方的安装前缀：

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="/path/to/BML-SDK;/path/to/provider-package" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix "/path/to/game/ModLoader"
```

`BMLExampleValue::Interface` 只包含头文件和 BML SDK 依赖，不包含或链接
`BMLExampleValueProvider.bmodp`。须将提供方的模块和 ABI 描述也安装进同一个游戏。
`Definition::Dependencies` 声明运行时依赖，`Interfaces::Reference` 在 `OnLoad`
查询、在 `OnUnload` 重置。本例不需要引擎/ImGui 扩展。
