# Native interface provider

[简体中文](README_zh-CN.md)

This standalone project builds a C-protocol native Mod and an independent,
header-only CMake package named `BMLExampleValue`.

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="/path/to/BML-SDK" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix "/path/to/provider-package"
```

The prefix contains `Mods/BMLExampleValueProvider.bmodp` and its
`.bmodp.abi.json`, `include/BMLExample/ValueInterface.h`, and
`lib/cmake/BMLExampleValue/BMLExampleValueConfig.cmake` with its targets.
Give developers the interface headers/CMake package. For players, copy the Mod
and sidecar from `Mods` into the matching game’s `ModLoader/Mods`.

Consumers find the independent package and link `BMLExampleValue::Interface`,
which never links the provider binary. `Definition::Dependencies` retains the
provider for runtime lookup; consumers unload first. No engine/ImGui extension
is needed. See the [consumer](../native-interface-consumer/README.md).
