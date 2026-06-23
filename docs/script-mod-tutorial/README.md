# Ballance 脚本 Mod 编写指南

## 教程

从第 01 节开始。每节结束时都能看到明确结果。

| # | 标题 | 你会得到 |
| --- | --- | --- |
| 01 | [环境搭建](tutorial-01-setup.md) | VS Code 能补全 AngelScript |
| 02 | [第一个 Mod](tutorial-02-hello-mod.md) | 日志里出现 "Hello" |
| 03 | [输出与输入](tutorial-03-output-input.md) | 游戏内提示 + 按键响应 |
| 04 | [ImGui 窗口](tutorial-04-imgui-window.md) | 浮动调试窗口 |
| 05 | [命令系统](tutorial-05-commands.md) | `/hello` 可执行 |
| 06 | [回调模型](tutorial-06-callbacks.md) | 理解 BML 何时调用脚本 |
| 07 | [配置与 Timer](tutorial-07-config-timer.md) | 配置文件 + 定时器 |
| 08 | [查找对象](tutorial-08-find-objects.md) | 日志打印对象名列表 |
| 09 | [坐标显示器](tutorial-09-coordinate-mod.md) | **第一个实用 mod** |
| 10 | [Ballance 内部结构](tutorial-10-ballance-internals.md) | 能说出 Level01 里有什么 |
| 11 | [读 DataArray](tutorial-11-dataarray-read.md) | ImGui 显示 CurrentLevel 表 |
| 12 | [状态面板](tutorial-12-status-panel-mod.md) | **第二个实用 mod** |
| 13 | [Levelinit 占位符](tutorial-13-levelinit-placeholders.md) | 能解释 PC_Checkpoints 如何进入运行时 |
| 14 | [Gameplay 状态机](tutorial-14-gameplay-state-machine.md) | ImGui 显示 IngameParameter 表 |
| 15 | [行为图阅读法](tutorial-15-behavior-graph-reading.md) | 能读懂 activate next Checkpoint 流程 |
| 16 | [物理观察](tutorial-16-physics-observe.md) | 日志显示物理对象状态 |
| 17 | [生成物理对象](tutorial-17-physics-create.md) | 游戏里凭空出现一个球 |
| 18 | [控制物理对象](tutorial-18-physics-control.md) | 按键推动自己创建的球 |
| 19 | [修改前检查清单](tutorial-19-safety-checklist.md) | 能判断操作的风险等级 |
| 20 | [完整 Mod](tutorial-20-complete-mod.md) | **完整 mod 骨架** |
| 21 | [发布](tutorial-21-publish.md) | zip 打包可分发 |
| 22 | [排错](tutorial-22-troubleshooting.md) | 按症状快速定位问题 |

---

## 参考

教程中遇到不认识的 API 或概念时来这里查。

- [生命周期与事件](ref-lifecycle.md) | 回调签名、GameEvent 枚举、执行顺序
- [ModContext 方法](ref-modcontext.md) | 所有 ctx 可调用的方法
- [命令、配置、定时器](ref-commands-config-timer.md) | 注册、回调签名、ConfigProperty
- [输入与 ImGui](ref-input-imgui.md) | 键盘 API、窗口模板、每帧模型
- [对象与组](ref-objects-groups.md) | 查找、层级、句柄、组遍历
- [DataArray](ref-dataarray.md) | 读写 API、已知表、回滚模板
- [物理](ref-physics.md) | 物理化、力操作、OnPhysicalize 事件
- [跨 Mod 通信](ref-inter-mod.md) | 依赖、导出、DataShare
- [HUD、消息与资源](ref-hud-resources.md) | 消息、菜单、文件路径、加载资源
- [Ballance 名称速查](ref-ballance-names.md) | 组名、对象名、DataArray 名、资源路径
