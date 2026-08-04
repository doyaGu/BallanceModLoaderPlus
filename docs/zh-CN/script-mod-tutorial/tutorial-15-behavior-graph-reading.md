# 15 行为图阅读法

上一章看到 Gameplay 持续维护运行时表格：IngameParameter、Energy、CurrentLevel 这些表在关卡运行期间会不断被更新。分数增加了，Sector 编号变了，ActiveBall 换了。

谁在更新它们？

你的脚本没有写入操作，但值确实在变。答案是行为图（Behavior Graph）。行为图是 Ballance 原版逻辑的载体，用可视化方式编写，节点连线组成流程。理解行为图的阅读方法，能帮你弄清楚游戏状态为什么会按照特定顺序变化。

本章目标：掌握一套从行为图中提取流程信息的阅读方法，不需要修改任何行为图。

## 行为图是什么

Virtools 引擎的可视化编程系统。每一段逻辑由若干节点和连线组成，这些节点叫做 Building Block（简称 BB）。

一段行为图的执行过程：

```text
节点 A 做一个操作
完成后沿着线跑到节点 B
节点 B 需要数据，数据通过另一种线从别处传过来
节点 B 完成后继续沿线到节点 C
```

和文本代码的区别：文本代码从上往下读，行为图从左往右（或按线的方向）追。但原理一样，都是顺序执行加数据传递。

Ballance 几乎所有核心逻辑都在行为图里：

| 文件 | 包含的行为图 | 负责 |
| --- | --- | --- |
| `base.cmo` | 菜单流程、关卡调度 | 启动、加载、退出 |
| `Levelinit.nmo` | 分组、建表、初始化 | 开局整理 |
| `Gameplay.nmo` | 球控制、碰撞、计分、检查点 | 运行时规则 |

## 行为图的三个层次

一个 CKBehavior（行为对象）可以是三种东西：

| 层次 | 说明 |
| --- | --- |
| Building Block | 最小执行单元，做一个具体操作 |
| 子图（Sub-Graph） | 包含多个 Building Block 和连线，组成一段流程 |
| 脚本行为（Script Behavior） | 附加在某个对象上的顶级行为图 |

要看这些图，需要使用保留了 Interface Data 的反混淆文件。原版文件里的界面数据被删除，直接打开通常只能看到对象和行为存在，不能正常看到 Schematic View。

实际操作顺序：

1. 从 [BearKidsTeam/BallanceModding](https://github.com/BearKidsTeam/BallanceModding) 找到整理后的 Ballance 文件。
2. 用 Virtools Dev 打开其中的 `Gameplay.nmo` 或 `Levelinit.nmo`。
3. 在对象列表里找到行为对象，例如 `Gameplay_Ingame`。
4. 打开 Schematic View，从顶层子图开始往里看。

打开反混淆后的 `Gameplay.nmo`，最外层看到的是脚本行为，例如 `Gameplay_Ingame`。进入它，看到若干子图，例如 `Gameplay_Events`、`Gameplay_SectorManager`。再进入子图，看到具体的 Building Block 节点。

这种嵌套关系像文件夹：外面的图给出概览，里面的图给出细节。

## 两种连线

行为图里的线有两种，必须区分清楚：

| 线的类型 | 方向 | 含义 |
| --- | --- | --- |
| 执行线（Behavior Link） | 节点输出 -> 节点输入 | 控制流：谁先跑，谁后跑 |
| 参数线（Parameter Link） | 参数输出 -> 参数输入 | 数据流：值从哪里来 |

执行线决定顺序。参数线决定数据。

一个典型的场景：

```text
[Get Row] --执行线--> [Set World Matrix] --执行线--> [Show]
     |                       ^
     |--参数线(matrix)-------|
```

Get Row 先执行，取出一行数据。执行完后轮到 Set World Matrix，它需要矩阵参数，这个矩阵通过参数线从 Get Row 的输出拿到。最后 Show 执行，显示对象。

阅读时的原则：先追执行线搞清顺序，再追参数线搞清数据来源。

## 常见 Building Block

在 Ballance 的行为图里频繁出现这些节点：

| 节点名 | 作用 |
| --- | --- |
| `Activate Script` | 启动一段子流程 |
| `Deactivate Script` | 停止一段子流程 |
| `Wait Message` | 等待内部消息到达后才继续 |
| `Send Message` | 向其他流程发送内部消息 |
| `Get Cell` | 从 DataArray 读一个单元格 |
| `Set Cell` | 往 DataArray 写一个单元格 |
| `Get Row` | 从 DataArray 读一整行 |
| `Set World Matrix` | 设置 3D 对象的位置和旋转 |
| `Show` | 显示对象 |
| `Hide` | 隐藏对象 |
| `Op` | 算术运算（加减乘除） |

先看节点名，通常能判断这段流程的大致用途。看不懂时再追参数线，不要一开始就陷进每个参数的来源。

## 四步阅读法

面对一张行为图，按这个顺序读：

```text
第一步：看图名
第二步：看顶层子图和节点
第三步：找 DataArray 读写
第四步：找消息收发
```

不要从左上角开始逐个节点细读。不要第一时间追参数。先把大方向搞清楚。

### 第一步：看图名

图名通常能告诉你这段流程的职责。

| 图名 | 先这样理解 |
| --- | --- |
| `Gameplay_Ingame` | 关卡运行中的总流程 |
| `Gameplay_Events` | 事件中转和处理 |
| `Gameplay_SectorManager` | 小节切换管理 |
| `activate next Checkpoint` | 激活下一个检查点 |
| `set Resetpoint` | 设置复活点 |

名字越具体，流程越小。名字越笼统，流程越大。入门时先从具体的小图开始。

### 第二步：看顶层子图和节点

进入一张图后，先扫一遍顶层的子图名和 BB 节点名，列出来：

```text
Gameplay_Events 顶层包含：
  Wait Message
  activate next Checkpoint
  activate Sektor
  set Resetpoint
  Send Message
  Key Event
```

这就够了。从名字可以看出这张图负责事件中转：等消息进来，然后分发给检查点、小节、复活点等子流程，最后再发消息出去。

### 第三步：找 DataArray 读写

看到 `Get Cell`、`Set Cell`、`Get Row`、`Set Row` 这些节点，说明流程在读写表格。接着问四个问题：

```text
哪张表？
哪一行？
哪一列？
读还是写？
```

这些信息在参数线上。对接到前面几章你已经实测过的表（CurrentLevel、Checkpoints、IngameParameter），就能把行为图的操作和你观察到的状态变化对应起来。

### 第四步：找消息收发

`Send Message` 和 `Wait Message` 是流程之间的连接点。

- `Wait Message` 出现在流程开头：这段流程由消息触发
- `Send Message` 出现在流程末尾：做完后通知其他流程继续

Ballance 的原版逻辑大量使用消息串联。一个复杂的过程（过检查点、变球、过关）往往由多段子流程通过消息接力完成。

## 实例：activate next Checkpoint

用四步阅读法读这张图。

### 第一步：图名

```text
activate next Checkpoint
```

职责明确：激活下一个检查点。

### 第二步：执行线上的节点

按执行线顺序，这张图包含：

```text
Activate Script -> Set Cell -> Get Row -> Set World Matrix -> Show -> Send Message
```

翻译成自然语言：

```text
进入子流程
更新表中的某个状态
从表里取出一行数据
把某个对象设置到指定位置
显示该对象
发消息通知其他流程
```

### 第三步：DataArray 操作

`Set Cell` 的参数追踪：

```text
目标表：CurrentLevel 或检查点状态相关字段
操作：写入下一个检查点的索引
```

`Get Row` 的参数追踪：

```text
目标表：Checkpoints
行号：由 Set Cell 更新后的索引决定
读出内容：检查点对象引用、世界矩阵
```

和你在第 11 章实测的 Checkpoints 表对照：

```text
Checkpoints 表结构
  行 0: object=PC_TwoFlames_01   matrix=...
  行 1: object=PC_TwoFlames_02   matrix=...
  行 2: object=PC_TwoFlames_03   matrix=...
```

所以 Get Row 取出的就是下一个检查点的对象和位置。

### 第四步：消息

末尾的 `Send Message` 通知其他流程：检查点已激活。后续可能触发复活点更新、提示显示、声音播放等。

### 完整流程图（文本表示）

```text
activate next Checkpoint
=========================

[Activate Script]
       |
       v  执行线
[Set Cell]  ---- 参数线 ----> CurrentLevel (更新检查点索引)
       |
       v  执行线
[Get Row]   ---- 参数线 <--- Checkpoints 表 (取出对象+矩阵)
       |
       |--- 参数线(object) -----.
       |--- 参数线(matrix) --.  |
       v  执行线              |  |
[Set World Matrix] <---------'  |
       |                        |
       v  执行线                |
[Show] <------------------------'
       |
       v  执行线
[Send Message]  ----> 通知后续流程
```

### 一句话总结

```text
收到检查点事件后，更新索引，从 Checkpoints 表读出下一检查点的对象和位置，
把对象放到该位置并显示出来，最后发消息通知后续流程。
```

## 实例：Gameplay_SectorManager 简化视图

`Gameplay_SectorManager` 比 `activate next Checkpoint` 大得多，但阅读方法一样。

### 第一步：图名

```text
Gameplay_SectorManager
```

负责小节（Sector）相关的激活和关闭。第 10 章提到 Ballance 把每关分成若干 Sector，只渲染当前 Sector 附近的对象。

### 第二步：顶层子图

```text
Wait Message
activate Sektor
deactivate Sektor
Send Message
```

大方向：等消息 -> 激活某个小节 -> 关闭某个小节 -> 发消息。

### 第三步：DataArray 操作

子图内部的关键操作：

```text
读 IngameParameter."Activate Sector" -> 得到要激活的小节编号
读 IngameParameter."Deactivate Sector" -> 得到要关闭的小节编号
```

对照第 11 章你观察到的 IngameParameter 表内容：

```text
IngameParameter.Activate Sector: 1
IngameParameter.Deactivate Sector: 0
```

当玩家经过检查点，行为图会先更新这两个值，再触发 SectorManager 执行切换。

### 第四步：消息

`Wait Message`：等待某个消息到达后才开始执行切换。这个消息通常由检查点流程发出。

`Send Message`：切换完成后通知其他流程（可能触发对象显示、声音等）。

### 简化流程

```text
Gameplay_SectorManager 简化流程
===================================

收到小节切换消息
  |
  v
读 IngameParameter 表
  得到 Activate Sector 编号 (例如 2)
  得到 Deactivate Sector 编号 (例如 1)
  |
  v
activate Sektor
  显示 Sector_02 组中的所有对象
  |
  v
deactivate Sektor
  隐藏 Sector_01 组中的所有对象
  |
  v
发送完成消息
```

这就是为什么玩的时候，经过检查点后远处的路面突然出现而身后的路面消失。行为图按照 IngameParameter 表里的编号控制 Sector 组的显示和隐藏。

## 脚本能做什么，不能做什么

明确边界：

| 能力 | 说明 |
| --- | --- |
| 读 DataArray | 可以。观察行为图写入的结果 |
| 监听游戏事件 | 可以。知道行为图执行到了哪个阶段 |
| 查看对象状态 | 可以。看到行为图设置的位置、可见性 |
| 修改行为图 | 不可以。无法增删节点或改连线 |
| 拦截行为图执行 | 不可以。无法暂停或跳过某个节点 |
| 替换行为图逻辑 | 不可以。但可以在事件后覆盖效果 |

这个限制意味着什么？你的脚本和行为图是并行的两套系统。行为图按原版流程运行，你的脚本通过事件和 DataArray 观察它的输出。如果你想改变游戏行为，方式是在行为图执行完之后覆盖它的结果（比如修改 DataArray 的值，移动对象位置），而不是修改行为图本身。

## 用脚本观察行为图的执行效果

下面这段代码不读取行为图，而是观察行为图执行后的状态变化。当玩家经过检查点时，行为图会更新 Checkpoints 表和 IngameParameter 表，脚本能看到变化：

```angelscript
[bml.mod id="bgwatch.script" name="BG Watcher" version="1.0.0" author="Tutorial" bml="0.3.12" description="Observe behavior graph effects"]
class BGWatcher {
    private int lastSector = 0;
    private int lastCheckpointIndex = -1;

    void OnLoad(const BML::ModContext &in ctx) {
        LogInfo(ctx, "BGWatcher loaded");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_START_LEVEL) {
            lastSector = 0;
            lastCheckpointIndex = -1;
            SnapshotState(ctx, "level_start");
        }
    }

    void OnProcess(const BML::ModContext &in ctx) {
        CKDataArray@ ingame = ctx.BorrowDataArrayByName("IngameParameter");
        if (ingame is null) return;

        int colSector = BML::CK::FindColumn(ingame, "Activate Sector");
        if (colSector < 0) return;

        int currentSector = BML::CK::GetInt(ingame, 0, colSector, 0);
        if (currentSector != lastSector) {
            lastSector = currentSector;
            LogInfo(ctx, "Sector changed -> " + currentSector
                         + " (SectorManager activated a new sector)");
        }
    }

    private void SnapshotState(const BML::ModContext &in ctx,
                               const string &in reason) {
        CKDataArray@ ingame = ctx.BorrowDataArrayByName("IngameParameter");
        if (ingame is null) {
            LogInfo(ctx, "IngameParameter not available at " + reason);
            return;
        }

        int colActivate = BML::CK::FindColumn(ingame, "Activate Sector");
        int colDeactivate = BML::CK::FindColumn(ingame, "Deactivate Sector");

        if (colActivate >= 0 && colDeactivate >= 0) {
            int activate = BML::CK::GetInt(ingame, 0, colActivate, 0);
            int deactivate = BML::CK::GetInt(ingame, 0, colDeactivate, 0);
            LogInfo(ctx, "Snapshot[" + reason + "] Activate Sector="
                         + activate + " Deactivate Sector=" + deactivate);
        }
    }

    private void LogInfo(const BML::ModContext &in ctx, const string &in msg) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(msg);
        }
    }
}
```

运行后进入第一关，经过第一个检查点时日志会出现：

```text
Sector changed -> 2 (SectorManager activated a new sector)
```

你没有修改任何值。是行为图执行了 `activate next Checkpoint`，更新了 IngameParameter 表里的 `Activate Sector`，然后 SectorManager 执行了小节切换。你的脚本只是观察到了这个变化。

## 概念总结

把行为图和前面章节学过的概念连接起来：

```text
DataArray（第 11 章）
  行为图读写 DataArray 来存储和传递状态
  你的脚本读 DataArray 来观察状态

游戏事件（第 6 章）
  BML 在行为图的关键节点处发出事件
  你的脚本收到事件后执行响应

对象和组（第 8、10 章）
  行为图通过 Show/Hide/SetWorldMatrix 操作对象
  你的脚本通过 Borrow3dEntityByName 获取同一个对象并读取它的状态

消息（Virtools 内部）
  行为图子流程之间用 Send/Wait Message 串联
  脚本不直接收发 Virtools 消息，但能通过 BML 事件间接感知
```

行为图负责推进原版逻辑。DataArray 保存运行时状态。脚本先读这些状态，确认原版逻辑在做什么；需要改行为时，再选择一个影响范围明确的状态或对象去改。

## 常见问题

**Q：我必须用 Virtools Dev 打开行为图才能读吗？**

可以，但要打开反混淆后的文件。原版文件缺少 Interface Data，直接打开看不到可读的 Schematic View。反混淆文件可以在 [BearKidsTeam/BallanceModding](https://github.com/BearKidsTeam/BallanceModding) 找到。

**Q：BML 游戏事件和 Virtools 消息是同一个东西吗？**

不是。BML 游戏事件（`BML::GameEvent`）是 BML 框架提供给脚本 mod 的回调接口。Virtools 消息（`Send Message` / `Wait Message`）是行为图内部的通信机制。它们属于不同层。BML 会在行为图执行到某些关键点时发出对应的游戏事件，所以你可以间接感知行为图的进度。

**Q：如果行为图改了 DataArray，我在 OnProcess 里能立刻读到新值吗？**

在 `OnProcess` 里观察，通常能读到本帧行为图处理后的结果，所以适合做状态面板和日志观察。事件回调的时机要看具体事件来源，不能把所有事件都当成同一个时间点。

**Q：我的脚本能在行为图之前执行吗？**

脚本层没有 `OnPreProcess` 固定回调。常规脚本主要在 `OnProcess` 里观察本帧状态，或通过 `GameEvent` 响应关卡阶段变化。脚本不能阻止某个行为图节点执行。

**Q：行为图里的 Send Message 会触发 BML 事件吗？**

不是每个 Virtools 消息都对应一个 BML 事件。BML 只在特定的关键消息处生成事件（开始关卡、死亡、加分、过关等）。大部分行为图内部的消息传递对脚本是不可见的，你只能通过 DataArray 的变化来间接推断。

## 完成状态

本章完成后你能做到：

- [x] 理解行为图是 Virtools 的可视化编程系统
- [x] 区分执行线和参数线的作用
- [x] 使用四步阅读法（图名 -> 顶层子图 -> DataArray -> 消息）
- [x] 读懂 `activate next Checkpoint` 的完整流程
- [x] 理解 `Gameplay_SectorManager` 的大致职责
- [x] 知道脚本能观察行为图效果但不能修改行为图
- [x] 通过监听 DataArray 变化间接观察行为图执行

## 下一步

[16 物理观察](tutorial-16-physics-observe.md)，看行为图如何通过 Physicalize 操作把对象接入物理系统，以及脚本怎么观察这个过程。
