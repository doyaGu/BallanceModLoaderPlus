# SDL 原生 Mod

本分支构建十二个原生 Mod。配置时启用 `-DBML_BUILD_MODS=ON`，使用与 Player
匹配的 SDL CK2/VxMath SDK。IVP v1 属于原版运行时，已从本分支移除；IVP v2 不再计划实现。

```sh
cmake -S . -B build-mods -DBML_RUNTIME=sdl -DBML_BUILD_MODS=ON
cmake --build build-mods --config Release
cmake --install build-mods --config Release --prefix build-mods-install
```

将 `.bmodp` 与 `.abi.json` 一起安装，或将资源 ZIP 放入 `ModLoader/Mods`。
`BallStickyPackage` 和 `BMLModulsPackage` 负责重新生成资源包；安装清单位于
`share/BML/mods.txt`。

## 清单

| Mod ID | 功能 |
| --- | --- |
| [BMLModuls](BMLModuls/) | 扩展游戏模块与资源 |
| [BallSticky](BallSticky/) | 可爬墙的球类型与资源 |
| [CameraUtilities](CameraUtilities/) | 相机控制 |
| [DebugUtilities](DebugUtilities/) | 游戏调试工具 |
| [DeformedWB](DeformedWB/) | 木球外观变形 |
| [DualBallControl](DualBallControl/) | 双球地图支持 |
| [DynamicFov](DynamicFov/) | 按球速调整视野 |
| [MapScripts](MapScripts/) | 地图回调脚本 |
| [NewSpawn](NewSpawn/) | 保存出生/传送位置 |
| [Segment](Segment/) | 分段表现记录 |
| [SpiritTrail](SpiritTrail/) | 半透明最佳记录回放 |
| [TravelMode](TravelMode/) | 旅行模式 |

十二个 Mod 已通过 macOS ARM64 SDK 构建和 Player 启动装载检查，尚不等于逐 Mod
玩法验收。关卡切换、重置、热键与各自功能须单独验证，见[验证状态](../docs/zh-CN/validation.md)。
