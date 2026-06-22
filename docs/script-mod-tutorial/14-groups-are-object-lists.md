# 第 14 章：组是对象名单

上一章把几个名字分开了：

```text
PC_TwoFlames_01          检查点标记
PC_Checkpoints           检查点名单
Checkpoints              检查点运行时表
PC_TwoFlames_Flame_Big   运行时 3D 实体
```

这一章只讲名单这一层。

Virtools 的组叫：

```text
CKGroup
```

先把它当成一份对象名单。

## 为什么需要组

关卡里经常有一批同类对象。

第一关加分机关有 11 个。  
如果原版逻辑每次都逐个猜名字，会很难维护：

```text
P_Extra_Point_01
P_Extra_Point_02
P_Extra_Point_03
...
```

更稳定的方式是给它们一份名单：

```text
P_Extra_Point
```

这样行为图可以先找到这份名单，再处理名单里的对象。

组回答的问题是：

```text
哪些对象属于这一类？
```

它不回答：

```text
这些对象现在是否可见
这些对象是否已经进入物理系统
玩家是否已经吃掉其中一个
检查点当前推进到哪一个
```

这些要继续看 3D 实体状态、DataArray 和行为图。

## 组本身也是对象

组也是 Virtools 对象，所以它也有：

```text
名字
id
class id
```

脚本查到组时，日志会像这样：

```text
PC_Checkpoints: id=... class=23
```

`class=23` 表示它是 `CKGroup`。

组名和具体对象名要分开：

| 组名 | 组里的对象例子 |
| --- | --- |
| `P_Extra_Point` | `P_Extra_Point_01` |
| `PC_Checkpoints` | `PC_TwoFlames_01` |
| `PR_Resetpoints` | `PR_Resetpoint_01` |
| `PE_Levelende` | `PE_Balloon_01` |
| `PS_Levelstart` | `PS_FourFlames_01` |

组名用查组入口。  
具体对象名用查 3D 实体或对象入口。

## 第一关先抓六个组

第一关有很多组，教程先用这六个：

```text
P_Extra_Point
PC_Checkpoints
PR_Resetpoints
PE_Levelende
PS_Levelstart
Sector_01
```

先按用途理解：

| 组名 | 用途 |
| --- | --- |
| `P_Extra_Point` | 加分机关名单 |
| `PC_Checkpoints` | 检查点名单 |
| `PR_Resetpoints` | 复活点名单 |
| `PE_Levelende` | 关卡结束标记名单 |
| `PS_Levelstart` | 关卡起点标记名单 |
| `Sector_01` | 第一小节对象集合 |

这些组名带着玩法含义。  
但组本身仍然只是入口，完整流程还要看后面的表和行为图。

## 加分机关组

第一关的 `P_Extra_Point` 包含 11 个对象：

```text
P_Extra_Point
  P_Extra_Point_01
  P_Extra_Point_06
  P_Extra_Point_07
  P_Extra_Point_08
  P_Extra_Point_02
  P_Extra_Point_03
  P_Extra_Point_04
  P_Extra_Point_05
  P_Extra_Point_09
  P_Extra_Point_10
  P_Extra_Point_11
```

这里有两层：

```text
P_Extra_Point      加分机关名单
P_Extra_Point_01   名单里的一个加分机关对象
```

对象在名单里，只说明它被归入加分机关这一类。  
它是否已经被吃掉、分数是否已经增加、对象是否隐藏，要看 Gameplay 的后续流程。

## 检查点组和复活点组

第一关检查点组：

```text
PC_Checkpoints
  PC_TwoFlames_01
  PC_TwoFlames_02
  PC_TwoFlames_03
```

第一关复活点组：

```text
PR_Resetpoints
  PR_Resetpoint_01
  PR_Resetpoint_02
  PR_Resetpoint_03
  PR_Resetpoint_04
```

这两组要分开看。

检查点更像进度标记。  
复活点更像失败后回到哪里。

第一关有 3 个检查点对象，4 个复活点对象。  
起点附近也需要复活位置，所以数量不同很正常。

后面读 `ResetPoints` 和 `CurrentLevel.CurrentResetpoint` 时，会继续用这个关系。

## 起点、终点和小节组

终点组：

```text
PE_Levelende
  PE_Balloon_01
```

起点组：

```text
PS_Levelstart
  PS_FourFlames_01
```

它们提供的是关卡内容里的标记。  
完整出生流程和过关流程还会经过配置表、初始化流程、Gameplay 状态切换和内部消息。

`Sector_01` 是第一小节对象集合。  
它里面包含多种对象：

```text
纸球相关对象
加分机关
加命机关
变球机关
木球相关对象
若干场景机关
```

这类组按关卡小节组织对象，不按单一机关类型组织。  
后面看到 `Activate Sector` 这类运行时状态时，再把它接回来。

## 组和 DataArray 的边界

组像名单。  
DataArray 像表。

拿检查点举例：

| 名字 | 类型 | 回答的问题 |
| --- | --- | --- |
| `PC_Checkpoints` | 组 | 哪些对象属于检查点名单 |
| `Checkpoints` | DataArray | 初始化后的检查点数据怎么组织 |
| `CurrentLevel` | DataArray | 当前这一局记着什么状态 |

组告诉原版逻辑有哪些对象可以处理。  
DataArray 记录初始化后和运行中的状态。

脚本查到 `PC_Checkpoints`，说明检查点名单存在。  
要知道当前复活点、当前进度，还要继续读表。

## 脚本怎样使用组

后面写 CKAS 脚本时，组通常按这个顺序处理：

```text
按名字找组
  找不到：显示 not found，停止处理这一组
  找到：记录组名、id、class id
    再读取组里的对象
      记录对象名、id、class id
```

不要假设组一定存在。

常见原因：

```text
玩家还在菜单
当前关卡换成了别的关
mod 地图没有这份组
原版流程还没加载到目标内容
```

稳定写法：

```text
每次需要时重新查
找不到就显示清楚
不要长期保存 raw `CKGroup@`
```

第 19 章会把这套流程写成脚本。

## 本章结果

先记住：

```text
CKGroup 是对象名单。
组名通常带着玩法含义。
组回答“哪些对象属于这一类”。
组不记录完整运行状态。
对象存在、在组里、玩法状态改变，是三件事。
```

下一章讲 DataArray。  
DataArray 会把“有哪些对象”推进到“原版逻辑现在记着什么”。
