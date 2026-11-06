# 支持与验证

本页区分“存在构建目标”和“完成游戏验收”。以下是 **2026-11-06 的本地验证记录**，
不代表任意发布包或后续提交都已通过相同验收。

## 运行时范围

- 本分支只接受 `BML_RUNTIME=sdl`，使用 Ballanced SDL CK2/VxMath。
- 已实现动态宿主；配置 `BML_HOST_LINKAGE=static` 会失败。更新器识别静态包元数据，
  不等于已经实现静态 Loader 集成。
- 原生 Mod 使用 `bml.mod` major 1 和 schema 2 ABI 伴随文件；旧 SDL C++ 入口产物需
  迁移重编译。原版 Windows 后端由独立分支维护。
- 默认启用脚本，也验证无脚本构建。
- 本分支不含 IVP v1；不实施 IVP v2。

| 目标 | SDK 目标标识 | 本记录的原生 Player 验收 |
| --- | --- | --- |
| Windows x86、x64、ARM64 | `windows` / `x86`、`x64`、`arm64` | 未运行 |
| Linux x64、ARM64 | `linux` / `x64`、`arm64` | 未运行；X11/Wayland 待验收 |
| macOS x64 | `macos` / `x64` | 未运行 |
| macOS ARM64 | `macos` / `arm64` | 动态版，脚本开启和关闭 |
| 任意静态宿主 | 尚未集成 | 待完成 |

## 已完成的本地检查

macOS ARM64 Release 动态构建的完整 CTest：脚本开启 **107/107**，关闭 **100/100**。
这是本次快照；可选测试开关和后续新增测试会改变数量。

检查包括安装后和移动 SDK 的独立消费者、C/C++ 原生示例、Native/Interface/IMC/Script
开发工作流、全部仓库内原生 Mod 的构建、32/64 位 ABI 基准（在宿主字长执行）、所选引擎
指纹、Behavior/IMC/配置/命令/输入契约及更新器样例。64 位宿主上的基准检查不表示运行过
32 位程序。

独立 SDK 构建的 Player probe 在脚本开启、关闭时均通过多次独立启动。它检查引擎先于
Mod 的 PostProcess 顺序、输入按下/松开/屏蔽、一次待处理 Render 跳过、命令注销寿命、
配置通知、定时器、脚本值和 CK 对象删除后的借用验证，并检查卸载和干净退出。
原始与部署后的 Mod SHA-256 必须一致；测试部署不修改 Mod 二进制。

可选 `--benchmark-api` 记录常用 API 耗时。包括交替对照的五次本地样本中，目录读取
中位数约 13.5→6.8 µs，键名约 63.6→32.5 ns，屏蔽键盘约 40.8→34.9 ns；原始键盘
控制组仍约 40 ns。这是热路径微基准，不是帧率或其他平台的性能保证。SDK 头文件内的
优化需要重编译 Mod 才会生效。

## 文档示例

2026-11-06 文档更新中，在临时 macOS ARM64 Player 中编译并加载 21 个完整脚本
示例，包含中英文 IMC 指南、中文课程、三个 SDK 示例和完整教程资源。暂存副本使用
唯一 id，IMC include 从指南的 IDL 生成。纯 C、C++、接口提供方/使用方、原生完整
入口和 IMC C++ 片段均通过安装后 SDK 编译。另有 18 个服务、场景、Hook 和状态迁移
片段在独立脚本模块中通过编译，未执行其玩法逻辑。两种语言站点通过严格 MkDocs 链接和
锚点检查。这些属于编译与启动验证，不代表已执行示例所有玩法分支。

## 尚未完成

Windows、Linux、macOS x64 实机运行；全部静态宿主集成；X11/Wayland 及完整的输入法、
焦点、手柄、DPI、全屏覆盖；完整关卡、死亡重生、重置、地图和 UI 流程；逐 Mod 玩法验收；
生产更新通道验收；本地样例以外的发行签名和依赖部署检查。构建或 doctor 通过不能代替
这些运行验证。

构建命令见[参与开发](contributing.md)，可重复的实机和性能检查见仓库
[Player probe](https://github.com/doyaGu/BallanceModLoaderPlus/tree/sdl/tests/player/sdl)。
