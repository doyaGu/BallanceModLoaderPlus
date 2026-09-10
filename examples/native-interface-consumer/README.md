# Native Interface Consumer

[简体中文](README_zh-CN.md)

This project is intentionally independent from the provider source tree. Build
it against the installed BML SDK and the provider's installed interface package:

```powershell
$generator = "Visual Studio 17 2022"
cmake -S . -B build -G $generator -A Win32 `
  -DBML_DIR="<BML-SDK>/lib/cmake/BML" `
  -DBMLExampleValue_DIR="<Provider-Package>/lib/cmake/BMLExampleValue" `
  -DVIRTOOLS_SDK_PATH="<Virtools-SDK-2.1>" `
  -DCMAKE_INSTALL_PREFIX="<Ballance>/ModLoader"
cmake --build build --config RelWithDebInfo --target install
```

`BMLExampleValue::Interface` contains headers and a dependency on the BML SDK.
It does not contain or link `BMLExampleValueProvider.bmodp`. Runtime availability
comes from `RequiredInterface`, which declares the provider Mod dependency and
performs the typed lookup in `OnLoad`.
