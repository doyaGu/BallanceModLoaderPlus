# Native Interface Provider

[简体中文](README_zh-CN.md)

This is a standalone provider project. It builds a native Mod and publishes its
shared header as the CMake package `BMLExampleValue`.

```powershell
$generator = "Visual Studio 17 2022"
cmake -S . -B build -G $generator -A Win32 `
  -DBML_DIR="<BML-SDK>/lib/cmake/BML" `
  -DVIRTOOLS_SDK_PATH="<Virtools-SDK-2.1>" `
  -DCMAKE_INSTALL_PREFIX="<Provider-Package>"
cmake --build build --config RelWithDebInfo
cmake --install build --config RelWithDebInfo
```

The installed package contains:

- `Mods/BMLExampleValueProvider.bmodp`;
- `include/BMLExample/ValueInterface.h`;
- `lib/cmake/BMLExampleValue/BMLExampleValueConfig.cmake` and its target files.

Ship the interface package to developers. Install the `.bmodp` in Ballance for
players. Consumers compile against `BMLExampleValue::Interface`; they never link
this provider binary.
