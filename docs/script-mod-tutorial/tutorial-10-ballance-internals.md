# 10 Ballance 内部结构

上一章做出了坐标显示器，进入关卡后找到球体对象，每帧读取坐标。但有一个问题你可能已经注意到：为什么必须等到 `GAME_EVENT_START_LEVEL` 才能找到球？为什么关卡开始前查找会返回 null？

答案在 Ballance 的文件架构和启动流程里。这一章会解释：当你点击"Play Level 1"，游戏内部到底发生了什么。理解了这个流程，你就知道什么时候能找到什么对象，以及为什么有些操作必须在特定时机执行。

## 四个关键文件

Ballance 的运行时由四类文件协作组成。想象成一条流水线，每个文件负责一个阶段：

| 文件 | 位置 | 职责 |
| --- | --- | --- |
| `base.cmo` | 游戏根目录 | 总指挥：菜单系统、关卡调度、全局状态管理 |
| `Levelinit.nmo` | `3D Entities/` | 初始化工人：关卡加载后整理对象、创建组和运行时表 |
| `Gameplay.nmo` | `3D Entities/` | 规则裁判：球的控制、碰撞、计分、检查点逻辑 |
| `Level_XX.NMO` | `3D Entities/Level/` | 关卡蓝图：XX 关的地形、机关、加分点等具体内容 |

`base.cmo` 从游戏启动就常驻内存。其余三个是关卡需要时加载的。

## 一次关卡启动的完整流程

当你在菜单里选择"Level 1"并点击开始，游戏内部按以下顺序执行：

```text
1. 玩家点击开始
2. base.cmo 收到指令，开始加载关卡
3. base.cmo 加载 Level_01.NMO -> 地形、机关、加分点等对象出现在场景中
4. base.cmo 启动 Levelinit.nmo
5. Levelinit 遍历场景，按规则整理对象（例如把检查点放入 PC_Checkpoints 组）
6. Levelinit 创建运行时表格（CurrentLevel、ResetPoints、Checkpoints...）
7. Levelinit 设置初始状态（起始球、起始复活点、初始分数=0）
8. Levelinit 完成，通知 base.cmo
9. base.cmo 启动 Gameplay.nmo
10. Gameplay 接管球的物理控制、碰撞检测、计分逻辑
11. BML 发出 GAME_EVENT_START_LEVEL <- 你的脚本在这里开始工作
```

注意第 11 步，你的脚本收到事件时，前面所有步骤已经完成。这意味着：

- 所有对象已经创建并命名
- 所有组已经填充完毕
- 所有运行时表格已经有了初始数据
- 球已经就位并准备好被控制

这就是为什么查找关卡对象通常放在 `GAME_EVENT_START_LEVEL` 之后。时机对了，还要名字和类型也对；名字拼错或用错查找函数仍然会返回 `null`。

## 为什么"太早"会失败

如果你在 `OnLoad` 里调用 `ctx.Borrow3dEntityByName("Ball")`，结果是 null。因为 `OnLoad` 在游戏启动时执行，那时候 Level_01.NMO 还没加载，球根本不存在。

常见的"时机太早"错误：

| 代码位置 | 能找到的对象 | 找不到的对象 |
| --- | --- | --- |
| `OnLoad` | 无（关卡对象都不存在） | 球、地板、检查点、DataArray |
| `OnProcess`（菜单中） | 无 | 同上 |
| `GAME_EVENT_START_LEVEL` | 已创建的关卡对象、组、DataArray | 名字拼错或类型不匹配的对象 |
| `OnProcess`（关卡中） | 已创建的关卡对象、组、DataArray | 名字拼错或类型不匹配的对象 |

规则：**关卡对象只在关卡运行时存在。** 菜单里、加载画面里，它们都不在。

## 对象命名约定

Level_01.NMO 里的对象遵循 Ballance 的命名约定。上一章已经从 `CurrentLevel.ActiveBall` 取到了当前玩家球；这里再看关卡对象常见的命名规则。

### 前缀决定用途

| 前缀 | 含义 | 第一关里的例子 |
| --- | --- | --- |
| `P_` | 物理实体（会参与碰撞） | `P_Ball_Paper_01`（纸球）、`P_Modul_18_01`（推箱） |
| `PR_` | 复活点（Resetpoint） | `PR_Resetpoint_01`、`PR_Resetpoint_02` |
| `PC_` | 检查点（Checkpoint） | `PC_TwoFlames_01`（双火焰柱） |
| `PS_` | 起始点（Start） | `PS_Levelstart_01` |
| `PE_` | 终点（End） | `PE_Levelende_01` |

### 不带前缀的对象

地板、装饰物等不带前缀：`Floor_01`、`Sector_01`。它们是静态几何体，不参与游戏逻辑，通常不需要脚本操作。

### 命名规则的 mod 写作意义

- **前缀让你精确查找**：想找球？搜 `P_Ball_`。想找检查点？搜 `PC_`
- **编号从 01 开始**：第一关第一个复活点是 `PR_Resetpoint_01`，不是 `_00`
- **球的名字包含材质**：`P_Ball_Paper_01`（纸）、`P_Ball_Wood_01`（木）、`P_Ball_Stone_01`（石）

## 组：Levelinit 整理的结果

前面流程里提到 Levelinit 会"把对象分组"。组（Group）是 Ballance 里的对象列表，Levelinit 根据命名规则把同类对象归入同一个组。

第一关运行时常用的关键组：

| 组名 | 内容 | 第一关的成员数 |
| --- | --- | --- |
| `All_Trafo` | 所有变球器 | 2 个 |
| `P_Extra_Point` | 加分机关 | 11 个 |
| `PC_Checkpoints` | 检查点 | 3 个 |
| `PR_Resetpoints` | 复活点 | 4 个 |
| `PE_Levelende` | 关卡终点 | 1 个 |
| `PS_Levelstart` | 关卡起点 | 1 个 |
| `Sector_01` ~ `Sector_04` | 各小节的对象集合 | 每节不同 |

组的意义：你不需要记住每个对象的名字，通过遍历组就能找到同类对象。第 8 章的 `PC_Checkpoints` 示例已经演示了最基本的组遍历。

注意 `Sector` 组，Ballance 把每关分成若干小节（Sector），每个小节包含一段连续的地形和机关。第一关有 4 个 Sector。Sector 的划分影响游戏的渲染优化：只有当前 Sector 附近的对象会被渲染，远处的暂时隐藏。

## DataArray：Levelinit 创建的运行时表

除了分组，Levelinit 还创建运行时表格（DataArray）来记录游戏状态。把 DataArray 想象成 Excel 表格，有行有列，每个单元格存一个值。

关卡运行时的核心表格：

| 表名 | 作用 | 行数 | 打个比方 |
| --- | --- | --- | --- |
| `CurrentLevel` | 当前关卡即时状态 | 1 | 仪表盘，实时显示当前状况 |
| `AllLevel` | 13 关的起始配置 | 13 | 目录，每关的基本信息 |
| `Checkpoints` | 检查点状态 | 每关不同 | 签到表，记录哪些点已激活 |
| `ResetPoints` | 复活点位置 | 每关不同 | 坐标簿，记录每个复活点的位置 |
| `IngameParameter` | 游戏参数 | 1 | 设置面板，球速度、摄像机距离等 |

下一章会详细讲怎么用脚本读取这些表。这里先记住：**DataArray 和组一样，只在关卡运行时存在。**

## 行为图：不可修改的原版逻辑

你可能好奇：Levelinit 怎么知道要把对象分到哪个组？Gameplay 怎么知道吃到加分点要加分？

答案是行为图（Behavior Graph），Ballance 原版的逻辑系统。它是一种可视化编程，节点连线表示逻辑流程。Levelinit 和 Gameplay 的内部逻辑都是行为图写的。

脚本 mod 不能修改行为图。这是一个硬限制。但你不需要修改它们，行为图处理的是游戏的核心流程（物理、碰撞、计分），而脚本 mod 做的是在这个流程之上增加新功能（显示信息、修改状态、增加 HUD）。

如果你在日志里看到某个状态突然变化（比如分数增加、球被弹飞），但你的脚本没有做任何操作，那通常是行为图在工作。

## 综合：一关里有什么

以第一关为参考，运行时场景里的内容可以这样分层理解：

```text
Level_01.NMO 提供（关卡原始内容）
├── 约 42 块地板
├── 11 个加分机关
├── 3 个检查点（双火焰柱）
├── 4 个复活点
├── 2 个变球器
├── 1 个起点、1 个终点
└── 各种装饰物

Levelinit 整理后（有序的工作区）
├── PC_Checkpoints 组：3 个检查点
├── PR_Resetpoints 组：4 个复活点
├── CurrentLevel 表：Level=1, Ball=Ball_Wood, Points=0
├── ResetPoints 表：4 行，每行一个复活点的位置矩阵
└── Checkpoints 表：3 行，每行一个检查点的状态

Gameplay 持续维护
├── 球的物理运动
├── 碰撞检测 -> 触发变球/计分/死亡
├── 检查点激活 -> 更新 Checkpoints 表和 CurrentLevel 表
└── 通关判定 -> 碰到 PE_Levelende 时结算
```

你的脚本 mod 在最外层观察和操作这些内容。

## 脚本 mod 的操作边界

了解了内部结构，明确一下你能做什么、不能做什么：

| 能做 | 不能做 |
| --- | --- |
| 按名字查找任何对象 | 修改行为图节点和连线 |
| 读取 DataArray 的值 | 改变文件加载顺序 |
| 修改对象的位置和属性 | 创建新的行为图 |
| 遍历组成员 | 替换原版 Building Block |
| 监听游戏事件并响应 | 阻止原版逻辑执行 |
| 创建 ImGui 界面 | 直接修改 .cmo/.nmo 文件 |

大多数实用 mod 都在"读取"范围内工作。你已经做过的坐标显示器就是典型例子：查找对象、读取位置、显示到 ImGui。接下来的几章会从读取 DataArray 开始，逐步进入修改操作。

## 验证你的理解

进入第一关后，在你的坐标 mod（第 9 章）的 `GAME_EVENT_START_LEVEL` 回调里加几行日志：

```angelscript
if (event == BML::GAME_EVENT_START_LEVEL) {
    // 验证对象存在
    CK3dEntity@ ball = ctx.Borrow3dEntityByName("Ball");
    LogInfo(ctx, "Ball: " + (ball is null ? "null" : "found"));

    // 验证 DataArray 存在
    CKDataArray@ table = ctx.BorrowDataArrayByName("CurrentLevel");
    LogInfo(ctx, "CurrentLevel: " + (table is null ? "null" : "found"));
}
```

如果两行都输出 "found"，说明你的理解是正确的：在 `GAME_EVENT_START_LEVEL` 时，Levelinit 已经完成了所有准备工作。

## 常见问题

**"我在 OnLoad 里找对象，总是 null"**，OnLoad 在游戏启动时执行一次，那时还在菜单，没有任何关卡对象。等 `GAME_EVENT_START_LEVEL` 再找。

**"我找到了对象但读出来的值很奇怪"**，确认你找的对象名拼写完全正确。Ballance 区分大小写：`P_Ball_Paper_01` 和 `p_ball_paper_01` 是不同的名字。

**"退出关卡后我的 mod 崩溃了"**，你可能在 `OnProcess` 里使用了已经失效的句柄。退出关卡时对象被销毁，之前缓存的句柄变成悬空引用。一定要在 `GAME_EVENT_PRE_EXIT_LEVEL` 里把句柄设为 null，并在使用前检查 `is null`。

**"AllLevel 表在菜单里也能读到吗？"**，不能。DataArray 和其他关卡对象一样，只在关卡运行时存在。即使是"看起来像全局信息"的 AllLevel，也是由 Levelinit 创建的。

## 下一步预告

你已经知道 CurrentLevel 表里有关卡号、活动球、分数等信息。下一章会动手用脚本读取它，把这些数据取出来，用 ImGui 显示在屏幕上。

---

**完成状态**：能说出一关里有哪些对象、组和表，以及它们的创建时机

---

下一步：[11 读 DataArray](tutorial-11-dataarray-read.md)
