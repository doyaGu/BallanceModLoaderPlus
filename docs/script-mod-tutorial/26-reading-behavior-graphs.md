# 第 26 章：行为图阅读法

前面已经讲过：

```text
对象
组
DataArray
Levelinit
Gameplay
```

这一章开始看行为图。

先把目标说清楚：本章只教阅读，不教修改。  
这一章只解决一件事：

```text
看到一张行为图时，怎样知道它大概在做什么。
```

行为图不能按 `.as` 文件那样从第一行读到最后一行。  
它更像一张电路图或流程图：节点做事，线把节点接起来。

## 先知道你看的是什么

教程里会遇到三种“看行为图”的方式：

| 方式 | 用途 |
| --- | --- |
| 反混淆文件里的 Schematic View | 看 Ballance 原版流程的可读图结构 |
| 教程里的 ImGui 示例图 | 把一小段流程画出来练习读法 |
| BML 脚本日志 | 提供时间和运行时状态锚点 |

这三种东西不要混。

反混淆文件用来阅读原版流程。  
ImGui 示例图只是教学用的简化图。  
脚本日志只能告诉你“现在游戏运行到哪一步、哪些表能读到”，它不能替代行为图本身。

本章先用 `activate next Checkpoint` 练习读法。  
它在 `Gameplay` 的检查点流程附近，规模小，名字也直观。

## 先准备可读文件

发布版 Ballance 文件可以正常运行，但直接拿来在 Virtools Dev 里看行为图会遇到问题。

Ballance 发布时删掉了行为图的 Interface Data，也就是 Schematic View 需要的图结构信息。  
直接打开游戏安装目录里的 `Gameplay.nmo`、`Levelinit.nmo`，很多脚本会显示：

```text
--Script Hidden--
```

所以读行为图时，要使用反混淆后的文件：

```text
BearKidsTeam/BallanceModding
https://github.com/BearKidsTeam/BallanceModding
```

这个仓库提供的是便于查阅和修改的反混淆版本。  
图结构由恢复工具生成，布局可能和原作者编辑时不同，但已经可以用来阅读流程。

注意两点：

```text
游戏安装目录里的文件：用于运行和读取运行时状态。
反混淆文件：用于在 Virtools Dev 里看 Schematic View。
```

不要把反混淆文件直接替换进游戏实例验证。  
教程这里的任务是看图，不涉及替换资源。

## 怎么打开可读图

用 Virtools Dev 打开反混淆文件。先从这两个文件开始：

```text
3D Entities/Levelinit.nmo
3D Entities/Gameplay.nmo
```

本章重点看：

```text
3D Entities/Gameplay.nmo
```

大致步骤：

```text
1. 从反混淆文件中打开 Gameplay.nmo。
2. 在行为对象或脚本列表里找 Gameplay_Events。
3. 打开 Gameplay_Events 的行为图。
4. 在图里找 activate next Checkpoint。
5. 进入这个子图，只看这一小段。
```

第一次不要从 `Gameplay_Ingame` 这种大图开始。  
大图适合知道整体结构，不适合拿来入门。

也不要保存打开的文件。  
这里只是观察。

## 行为图里先认四种东西

打开图后，先认四种东西：

| 图上看到的东西 | 先这样理解 |
| --- | --- |
| 图名或子图名 | 这段流程大概负责什么 |
| Building Block 节点 | 具体动作 |
| 执行线 | 谁先运行，谁后运行 |
| 参数线 | 对象、表、数字、矩阵从哪里来 |

常见 Building Block：

| 节点 | 作用 |
| --- | --- |
| `Activate Script` | 进入或触发一段子流程 |
| `Wait Message` | 等内部消息 |
| `Send Message` | 发内部消息 |
| `Get Cell` | 从 DataArray 读单元格 |
| `Set Cell` | 写 DataArray 单元格 |
| `Get Row` | 从 DataArray 读一整行 |
| `Set World Matrix` | 设置 3D 对象的位置和方向 |
| `Show` / `Hide` | 显示或隐藏对象 |

读图时先看节点名。  
节点名已经能提供一半信息。

## 第一步：先找入口

一段流程从哪里开始，通常看输入和等待节点。

常见入口：

```text
Wait Message
Activate Script
图左侧输入点
```

例如在 `Gameplay_Events` 里看到 `Wait Message`，可以先记：

```text
这段流程由内部消息触发。
它在等某个内部消息。
```

看到 `activate next Checkpoint` 被接到某条线上，可以先记：

```text
某个检查点相关事件发生后，会进入激活下一检查点的子流程。
```

第一步只回答：

```text
谁触发这段流程？
```

不要马上看参数。

## 第二步：只追执行线

第二步只看执行线。  
暂时不管参数线。

拿 `activate next Checkpoint` 举例，能看到这些动作：

```text
Activate Script
Set Cell
Get Row
Set World Matrix
Show
Send Message
```

先把它写成动作链：

```text
进入子流程
写表
读表中一行
设置对象矩阵
显示对象
发送消息
```

这一步已经能看出它不只是在“显示火焰”。  
它同时做了状态更新、读表、移动对象、显示对象、发消息。

第二步只回答：

```text
这段流程先做什么，后做什么？
```

## 第三步：再追 DataArray

看到这些节点，就开始看表：

```text
Get Cell
Set Cell
Get Row
Set Row
Insert Row
```

追 DataArray 时只问四个问题：

```text
哪张表？
哪一行？
哪一列？
读还是写？
```

在检查点流程里，重点表是：

```text
CurrentLevel
Checkpoints
ResetPoints
```

前面章节已经实测过，第一关的 `Checkpoints` 表像这样：

```text
row 0  object=PC_TwoFlames_MF     matrix=...
row 1  object=PC_TwoFlames_MF001  matrix=...
row 2  object=PC_TwoFlames_MF     matrix=...
```

所以看到 `Get Row` 接到 `Checkpoints` 时，就可以理解成：

```text
从检查点表里取出某一行。
这一行里有检查点对象和矩阵。
```

看到 `Set Cell` 接到 `CurrentLevel` 或检查点状态相关字段时，就先记：

```text
这段流程会更新运行时状态。
```

第三步只回答：

```text
这段流程读写了哪些表？
```

## 第四步：追对象和矩阵

检查点火焰为什么会出现在正确位置？  
要看对象和矩阵。

在 `activate next Checkpoint` 里，关键动作是：

```text
Get Row
Set World Matrix
Show
```

可以按这条线理解：

```text
Get Row
  从 Checkpoints 表中取出一行

Set World Matrix
  把这一行里的矩阵设置给检查点对象

Show
  显示这个对象
```

这样就能把第 24 章读到的表和行为图接起来：

```text
Checkpoints 表
  Object 列：要操作哪个检查点对象
  Matrix 列：把它放到哪里
```

第四步只回答：

```text
这段流程最后改变了哪个对象？
对象用到了哪个位置或矩阵？
```

## 第五步：最后看消息

行为图经常用消息串起来。

常见节点：

```text
Send Message
Wait Message
```

先按入口和出口理解：

```text
Wait Message
  等别的流程通知我

Send Message
  我做完以后通知别的流程
```

在 `activate next Checkpoint` 末尾看到 `Send Message`，可以先记：

```text
激活下一检查点后，还会通知后续流程。
```

现在不需要马上知道每个消息编号的完整含义。  
先知道它是流程之间的连接点。

第五步只回答：

```text
这段流程和别的流程怎样接起来？
```

## 把图翻译成一句话

读完上面五步，再写一句自然语言总结。

`activate next Checkpoint` 可以先写成：

```text
收到检查点相关事件后，这段流程会更新检查点状态，
从 Checkpoints 表里取出下一检查点对象和矩阵，
把对象放到对应位置并显示出来，
然后发送消息通知后续流程。
```

这句话不要求覆盖每个参数。  
它的作用是把图从“节点堆”变成“流程”。

以后读加分、复活、变球、小节激活，也按同样方法：

```text
入口
执行线
DataArray
对象和矩阵
消息
一句话总结
```

## 教程里的 ImGui 示例图怎么看

教程目录里有一个简化图示例：

```text
docs/script-mod-tutorial/examples/BehaviorGraphViewerExample.mod.as
```

它使用：

```text
docs/script-mod-tutorial/libs/BehaviorGraphImGui.as
```

这个示例手工画了一张 `activate next Checkpoint` 的教学图。  
它不会解析原版文件，也不会修改行为图。

它的用途是练习读图：

```text
左边看入口
中间看 DataArray
右边看对象操作和消息
标着 event / exec / done 的线按执行顺序读
较细的黄色参数线按数据来源读
```

操作方式：

```text
右键或中键拖动：平移
鼠标滚轮：缩放
左键点击节点：选中
左键拖动节点：移动节点
```

这个示例能帮助你建立“图上怎么读”的感觉。  
真正分析原版流程时，仍然要回到 Virtools Dev 里的反混淆图。

## 脚本日志怎么配合读图

BML 脚本日志不显示行为图。  
它只提供时间和状态锚点。

例如看到：

```text
GAME_EVENT_START_LEVEL
CurrentLevel.Level ID = 1
Checkpoints rows = 3
IngameParameter.Activate Sector = 1
```

可以得到：

```text
关卡已经开始。
Levelinit 已经准备好运行时表。
Gameplay 可以读取这些表继续运行。
```

然后再回到行为图里看：

```text
谁读 CurrentLevel？
谁读 Checkpoints？
谁写 IngameParameter？
谁在状态变化后发送消息？
```

脚本日志负责告诉你“现在状态是什么”。  
行为图负责解释“这些状态为什么会被这样读写”。

## 常见误区

第一次读行为图，最容易犯这几个错：

| 错法 | 问题 |
| --- | --- |
| 从大图左上角开始逐个节点读 | 很快会迷路 |
| 一开始就追所有参数 | 会卡在行号、列号、矩阵和对象引用上 |
| 把 BML 游戏事件当成 Virtools 消息 | 它们是不同层 |
| 只看 `Show` / `Hide` | 会漏掉前面的表读写 |
| 只看 DataArray | 会漏掉消息和对象操作 |

更稳的做法：

```text
先选一小段
先追执行线
再追表
再追对象
最后看消息
```

## 本章结果

现在看行为图时，按这个顺序：

```text
1. 找入口
2. 只追执行线
3. 追 DataArray 读写
4. 追对象和矩阵
5. 看消息怎样接到别的流程
6. 写一句话总结
```

`activate next Checkpoint` 先读成：

```text
收到检查点相关事件后，
更新运行时状态，
从 Checkpoints 表里取出下一检查点对象和矩阵，
设置对象位置并显示，
再发送消息接到后续流程。
```

下一章进入对象接入物理系统的事件。  
物理化同样要按这套方法看：谁触发、读了什么参数、操作哪个对象、后续由谁接手。
