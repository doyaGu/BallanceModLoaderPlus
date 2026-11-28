# Pure C native Mod / 纯 C 原生 Mod

`Mod.c` uses the C11 tables in `BML/Mod.h` and exports `BML_GetModInterface`.
It demonstrates the same `bml.mod` protocol as the C++ wrapper without C++
virtual tables, STL or exception handling at the module boundary.

`Mod.c` 通过 `BML/Mod.h` 的 C11 表和 `BML_GetModInterface` 接入，
与 C++ wrapper 使用同一协议。查看返回的错误码，按表的 `StructSize` 和版本访问字段。

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="/path/to/BML-SDK" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix "/path/to/game/ModLoader"
```

Use a matching SDL dynamic-host SDK. Keep the `.bmodp` and `.bmodp.abi.json`
together. / 使用配套的 SDL 动态宿主 SDK，模块与 ABI 伴随文件一起安装。
