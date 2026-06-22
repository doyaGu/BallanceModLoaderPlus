# 第 17 章：用检查点串起四个概念

第 13 到 16 章分别讲了：

```text
对象
组
DataArray
行为图
```

这一章只看一个现象：检查点。

玩家看到的是一团火焰。  
脚本要把它拆成几层：

```text
关卡内容里的标记
对象名单
初始化后的运行时表
Gameplay 中的触发和复活流程
```

这就是后面读 Ballance 原版内容的基本方法。

## 从玩家看到的现象开始

检查点在游戏里看起来很简单：

```text
球经过火焰
检查点激活
死亡后从后面的位置复活
```

脚本要回答的问题更多：

```text
火焰对象叫什么？
哪些火焰属于检查点？
复活点对象在哪里？
当前复活点写在哪？
触发检查点后谁更新状态？
死亡后谁读取复活点？
```

这些问题分属不同层。

## 第一层：关卡内容里的检查点标记

第一关内容里有：

```text
PC_TwoFlames_01
PC_TwoFlames_02
PC_TwoFlames_03
```

先把它们理解成关卡作者放进去的检查点标记。

这层回答：

```text
第一关放了哪些检查点标记？
```

它不回答：

```text
现在激活到哪个检查点
死亡后回到哪个位置
Gameplay 后续会不会隐藏或移动它
```

所以脚本只盯着一个火焰对象，看不到完整检查点系统。

## 第二层：检查点组

第一关有一份检查点名单：

```text
PC_Checkpoints
  PC_TwoFlames_01
  PC_TwoFlames_02
  PC_TwoFlames_03
```

组回答：

```text
哪些对象属于检查点？
```

这比单独查一个对象更接近玩法结构。  
Levelinit 可以通过这份名单批量处理检查点。

但组仍然只是名单。  
它没有记录当前进度，也不说明哪个复活点正在使用。

## 第三层：复活点组

检查点和复活点要分开。

第一关复活点名单：

```text
PR_Resetpoints
  PR_Resetpoint_01
  PR_Resetpoint_02
  PR_Resetpoint_03
  PR_Resetpoint_04
```

检查点更像进度标记。  
复活点更像失败后回到哪里。

第一关 3 个检查点，4 个复活点。  
起点附近也需要一个复活位置，所以数量不同。

后面看到 `CurrentLevel.CurrentResetpoint` 时，要把它和 `PR_Resetpoints`、`ResetPoints` 联系起来，不要直接和火焰对象画等号。

## 第四层：运行时表

DataArray 开始记录初始化后和运行中的状态。

检查点相关先记四张表：

```text
Checkpoints
ResetPoints
CurrentLevel
AllLevel
```

它们的大致位置：

| 表 | 作用 |
| --- | --- |
| `AllLevel` | 每关起始配置 |
| `Checkpoints` | 检查点运行时数据 |
| `ResetPoints` | 复活点运行时数据 |
| `CurrentLevel` | 当前这一局状态 |

放在一起看：

```text
PC_Checkpoints
  原始检查点名单

Checkpoints
  初始化后的检查点数据

PR_Resetpoints
  原始复活点名单

ResetPoints
  初始化后的复活点数据

CurrentLevel
  当前这一局正在使用哪个复活点、哪个球、多少分
```

脚本读到 `CurrentLevel`，才开始接近“现在这局游戏的状态”。

## 第五层：行为图流程

对象、组和表还不足以解释“什么时候变”。

行为图负责流程：

```text
什么时候读取检查点组
什么时候整理 Checkpoints
什么时候显示下一个检查点
什么时候更新当前复活点
死亡后什么时候读取复活点
什么时候发内部消息
```

这里会碰到第 16 章讲过的节点：

```text
Get Cell
Set Cell
Send Message
Wait Message
Show
Hide
Set World Matrix
```

看到检查点相关行为图时，先按这条线追：

```text
事件从哪里来？
读了哪张表？
写了哪张表？
操作了哪个对象？
发了什么消息？
```

这比一开始尝试看完所有节点更有效。

## 一条简化流程

把几层合起来，检查点可以先这样理解：

```text
Level_01.NMO
  放入 PC_TwoFlames_01 / 02 / 03
  放入 PR_Resetpoint_01 / 02 / 03 / 04
  用 PC_Checkpoints 收起检查点
  用 PR_Resetpoints 收起复活点

Levelinit.nmo
  读取检查点组和复活点组
  整理成 Checkpoints / ResetPoints
  设置 CurrentLevel 初始状态

Gameplay.nmo
  等待触发
  推进检查点状态
  更新当前复活点
  控制显示和隐藏
  处理死亡后的复位

BML 脚本 mod
  在合适时机观察对象、组、表和事件
```

这条线是简化模型。  
它的作用是给后面读脚本、读表、读行为图提供方向。

## 修改检查点前先定位层级

以后想改检查点，先判断目标在哪一层：

| 想法 | 更接近哪一层 | 风险 |
| --- | --- | --- |
| 显示检查点对象名字 | 对象 / 组 | 低 |
| 读取当前复活点 | `CurrentLevel` | 低到中 |
| 临时改当前复活点 | `CurrentLevel` / Gameplay 状态 | 中 |
| 改检查点推进规则 | Gameplay 行为流程 | 高 |
| 改关卡里的检查点数量 | 关卡内容、组、初始化表 | 高 |

脚本教程前半部分先做低风险观察。  
修改会放到最后，因为修改必须知道自己碰的是哪一层。

## 本章结果

检查点先按五层记：

| 层 | 例子 | 含义 |
| --- | --- | --- |
| 对象标记 | `PC_TwoFlames_01` | 关卡内容里的检查点标记 |
| 组 | `PC_Checkpoints` | 检查点名单 |
| 组 | `PR_Resetpoints` | 复活点名单 |
| 表 | `Checkpoints` / `ResetPoints` | 初始化后的运行时数据 |
| 表 | `CurrentLevel` | 当前这一局状态 |
| 行为图 | 检查点和复活相关流程 | 何时触发、何时写表、何时复位 |

下一章回到第一关内容本身。  
第 18 章会把第一关里要观察的名字整理成一份入口清单。
