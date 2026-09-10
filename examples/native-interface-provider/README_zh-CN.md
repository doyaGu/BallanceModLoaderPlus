# 原生 Interface 提供方

[English](README.md)

这是一个独立的提供方工程。它会构建原生 Mod，并把共享头发布为 CMake package
`BMLExampleValue`。

```powershell
$generator = "Visual Studio 17 2022"
cmake -S . -B build -G $generator -A Win32 `
  -DBML_DIR="<BML-SDK>/lib/cmake/BML" `
  -DVIRTOOLS_SDK_PATH="<Virtools-SDK-2.1>" `
  -DCMAKE_INSTALL_PREFIX="<Provider-Package>"
cmake --build build --config RelWithDebInfo
cmake --install build --config RelWithDebInfo
```

安装结果包括：

- `Mods/BMLExampleValueProvider.bmodp`；
- `include/BMLExample/ValueInterface.h`；
- `lib/cmake/BMLExampleValue/BMLExampleValueConfig.cmake` 及其 target 文件。

面向开发者发布 interface package，面向玩家把 `.bmodp` 安装进 Ballance。使用方只编译
链接 `BMLExampleValue::Interface`，不会链接提供方二进制。
