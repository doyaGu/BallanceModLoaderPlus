# SDL 原生 Mod

本分支构建十二个原生 Mod。配置时启用 `-DBML_BUILD_MODS=ON`，使用与 Player
匹配的 SDL CK2/VxMath SDK。IVP v1 属于原版运行时，已从本分支移除；IVP v2 尚未实现。

```sh
cmake -S . -B build-mods -DBML_RUNTIME=sdl -DBML_BUILD_MODS=ON
cmake --build build-mods --config Release
cmake --install build-mods --config Release --prefix build-mods-install
```

将 `.bmodp` 与 `.abi.json` 一起安装，或将资源 ZIP 放入 `ModLoader/Mods`。
`BallStickyPackage` 和 `BMLModulsPackage` 负责重新生成资源包；安装清单位于
`share/BML/mods.txt`。
