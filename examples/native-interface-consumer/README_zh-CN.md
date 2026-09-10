# 原生 Interface 使用方

[English](README.md)

该工程与提供方源码树完全独立。构建时只需要安装后的 BML SDK 和提供方 interface
package：

```powershell
$generator = "Visual Studio 17 2022"
cmake -S . -B build -G $generator -A Win32 `
  -DBML_DIR="<BML-SDK>/lib/cmake/BML" `
  -DBMLExampleValue_DIR="<Provider-Package>/lib/cmake/BMLExampleValue" `
  -DVIRTOOLS_SDK_PATH="<Virtools-SDK-2.1>" `
  -DCMAKE_INSTALL_PREFIX="<Ballance>/ModLoader"
cmake --build build --config RelWithDebInfo --target install
```

`BMLExampleValue::Interface` 只包含头文件和 BML SDK 依赖，不包含、也不链接
`BMLExampleValueProvider.bmodp`。运行期由 `RequiredInterface` 声明提供方 Mod 依赖，
并在 `OnLoad` 中完成类型化查询。
