# 原生 Interface 提供方

[English](README.md)

这是一个独立的提供方工程。它会构建原生 Mod，并把共享头发布为 CMake package
`BMLExampleValue`。

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/sdk -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix /path/to/game/ModLoader
```

安装结果包括：

- `Mods/BMLExampleValueProvider.bmodp`；
- `include/BMLExample/ValueInterface.h`；
- `lib/cmake/BMLExampleValue/BMLExampleValueConfig.cmake` 及其 target 文件。

面向开发者发布 interface package，面向玩家把 `.bmodp` 安装进 Ballance。使用方只编译
链接 `BMLExampleValue::Interface`，不会链接提供方二进制。
