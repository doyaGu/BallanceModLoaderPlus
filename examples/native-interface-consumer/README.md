# Native Interface Consumer

[简体中文](README_zh-CN.md)

This project is intentionally independent from the provider source tree. Build
it against the installed BML SDK and the provider's installed interface package:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/sdk -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix /path/to/game/ModLoader
```

`BMLExampleValue::Interface` contains headers and a dependency on the BML SDK.
It does not contain or link `BMLExampleValueProvider.bmodp`. Runtime availability
comes from `Definition::Dependencies`; `Interfaces::Reference` performs the typed
lookup in `OnLoad` and resets in `OnUnload`.
