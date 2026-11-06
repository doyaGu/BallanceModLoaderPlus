# Native interface consumer

[简体中文](README_zh-CN.md)

Build and install the [provider package](../native-interface-provider/README.md)
first. This consumer is independent of the provider source and needs **both**
installed package prefixes:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="/path/to/BML-SDK;/path/to/provider-package" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix "/path/to/game/ModLoader"
```

`BMLExampleValue::Interface` contains headers and the BML SDK dependency; it
neither contains nor links `BMLExampleValueProvider.bmodp`. Install the provider’s
module/sidecar in the same game. `Definition::Dependencies` declares the runtime
dependency; `Interfaces::Reference` queries in `OnLoad` and resets in `OnUnload`.
The core C ABI is sufficient; no engine/ImGui extension is needed.
