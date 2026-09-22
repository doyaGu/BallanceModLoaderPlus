# 仓库内原生 Mod

[English](README.md) | 简体中文

这里收纳原 BallanceMods 工作区中的原生 Mod。EditorMode、TASSupport 和
Experimental 下的 Mod 没有迁入；原工作区未改动。

默认构建 BML+ 时不会构建这些 Mod。使用 Win32 配置并开启选项：

```powershell
cmake -S . -B build-mods -A Win32 -DBML_BUILD_MODS=ON
cmake --build build-mods --config RelWithDebInfo
cmake --install build-mods --config RelWithDebInfo --prefix build-mods-install
```

IVP 的测试另由 `IVP_BUILD_TESTS` 控制，具体见 [IVP 构建说明](IVP/README.md#build-and-test)。
每个 Mod 保留自己的源码和 CMake 文件；
`cmake/BallanceMod.cmake` 统一处理构建和打包。`3D Entities` 资源会放进对应
Mod 的 ZIP，而不是作为散文件安装。

只修改资源后，可单独构建 `BallStickyPackage` 或 `BMLModulsPackage` 重新打包，
无需重编译 DLL。不同构建配置的 ZIP 分别保存在对应 Mod 的构建目录中。

安装树的 `Mods/` 下包含全部 14 个包，`share/BML/mods.txt` 是包清单。把需要的
`.bmodp` 或 `.zip` 复制到游戏的 `ModLoader/Mods/` 即可启用。发布时另有可选的
`BMLPlus-Mods-<version>.zip`，可在游戏根目录解压。BML+ 主安装包默认只启用
CameraUtilities、DebugUtilities 和 TravelMode；Updater 不会改动已安装的 Mod。
