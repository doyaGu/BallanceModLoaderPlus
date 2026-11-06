# 原生 Interface 使用方

[English](README.md)

该工程与提供方源码树完全独立。构建时只需要安装后的 BML SDK 和提供方 interface
package：

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/sdk -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix /path/to/game/ModLoader
```

`BMLExampleValue::Interface` 只包含头文件和 BML SDK 依赖，不包含、也不链接
`BMLExampleValueProvider.bmodp`。由 `Definition::Dependencies` 声明提供方 Mod 依赖，
`Interfaces::Reference` 在 `OnLoad` 中完成类型化查询，在 `OnUnload` 中重置。
