# Native Interface Provider

[简体中文](README_zh-CN.md)

This is a standalone provider project. It builds a native Mod and publishes its
shared header as the CMake package `BMLExampleValue`.

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/sdk -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix /path/to/game/ModLoader
```

The installed package contains:

- `Mods/BMLExampleValueProvider.bmodp`;
- `include/BMLExample/ValueInterface.h`;
- `lib/cmake/BMLExampleValue/BMLExampleValueConfig.cmake` and its target files.

Ship the interface package to developers. Install the `.bmodp` in Ballance for
players. Consumers compile against `BMLExampleValue::Interface`; they never link
this provider binary.
