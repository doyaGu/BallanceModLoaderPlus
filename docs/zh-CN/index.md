# Ballanced SDL 的 BML+

本文档对应 BML+ **0.4.0** 的 `sdl` 分支：原生 Mod 使用版本化 C ABI 和模块内 C++ SDK，脚本使用
AngelScript。旧 IMod/IBML 体系已从本分支彻底移除，也不再作为私有实现保留。

| 任务 | 入口 |
| --- | --- |
| 安装 Loader 或 Mod | [使用 BML+](using-bml.md) |
| 检查安装、执行签名更新 | [Updater 和 doctor](updater-sdl.md) |
| 查看目标支持和待验收内容 | [支持与验证](validation.md) |
| 新建 Mod | [选择开发路线](modding.md) |
| 使用 C 或 C++ | [原生 Mod API](native-mod-api.md) |
| 迁移旧原生 Mod | [迁移指南](native-mod-migration.md) |
| 学习 AngelScript | [脚本分步教程](script-mod-tutorial/README.md) |
| 执行和修改 Behavior 图 | [Behavior 编写](behavior-authoring.md) |
| 发布类型化 Mod 服务 | [IMC](imc-author-guide.md) |
| 构建、修改 BML+ | [参与开发](contributing.md) |

运行时安装到游戏，SDK 放在开发工作区。使用 ABI 描述选择匹配的包。动态 SDL 宿主已
实现；静态 Player 集成和没有实机运行的目标仍待验收。

[脚本 API 参考](api.md) 中的声明文件用于补全和查询，不是运行时 include 文件。
