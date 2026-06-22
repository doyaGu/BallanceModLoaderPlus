# 第 18 章：第一关内容个案

第二篇前面几章已经建立了这条线：

```text
文件分工
对象和名字
组
DataArray
行为图
检查点分层模型
```

这一章把它落到第一关。

第一关内容文件：

```text
3D Entities/Level/Level_01.NMO
```

它提供的是作者内容：

```text
关卡里放了哪些对象
这些对象叫什么
哪些对象被放进同一份组
```

完整玩法还要经过 Levelinit、Gameplay、DataArray 和行为图。

## 本章只抓观察入口

第一关有很多对象。  
先抓六个组：

```text
P_Extra_Point
PC_Checkpoints
PR_Resetpoints
PE_Levelende
PS_Levelstart
Sector_01
```

它们覆盖了几类典型内容：

| 组名 | 用途 |
| --- | --- |
| `P_Extra_Point` | 加分机关名单 |
| `PC_Checkpoints` | 检查点名单 |
| `PR_Resetpoints` | 复活点名单 |
| `PE_Levelende` | 关卡结束标记名单 |
| `PS_Levelstart` | 关卡起点标记名单 |
| `Sector_01` | 第一小节对象集合 |

第 19 章会写脚本验证这些组能否在运行时找到。

## 加分机关入口

加分机关组：

```text
P_Extra_Point
```

第一关里包含 11 个对象：

```text
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

这里要记住两件事：

```text
P_Extra_Point 是组。
P_Extra_Point_01 是组里的一个对象。
```

脚本观察时先找组，再看组里对象。  
分数增加、对象隐藏、声音播放这些属于 Gameplay 流程，后面再追。

## 检查点和复活点入口

检查点组：

```text
PC_Checkpoints
  PC_TwoFlames_01
  PC_TwoFlames_02
  PC_TwoFlames_03
```

复活点组：

```text
PR_Resetpoints
  PR_Resetpoint_01
  PR_Resetpoint_02
  PR_Resetpoint_03
  PR_Resetpoint_04
```

这两组对应第 17 章的分层模型：

```text
PC_Checkpoints  ->  Checkpoints
PR_Resetpoints  ->  ResetPoints
CurrentLevel    ->  当前这一局使用的状态
Gameplay        ->  触发、推进、死亡、复位
```

后面的脚本会先做两件事：

```text
确认组能找到
确认运行时表能读到
```

至于检查点触发后的流程，要等行为图章节再追。

## 起点和终点入口

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

这两个组提供关卡内容里的起点和终点标记。

出生流程还会用到：

```text
AllLevel
CurrentLevel
起始球
起始复活点
Levelinit 初始化
Gameplay 接手运行
```

过关流程还会用到：

```text
触发检测
分数结算
结束消息
关卡调度
Gameplay 状态切换
```

脚本查到这些对象，只是确认标记存在。  
要解释出生和过关，还要继续看表和行为图。

## 小节对象集合

`Sector_01` 是第一小节对象集合。

它按关卡小节收一批对象，里面会有多种内容：

```text
纸球相关对象
加分机关
加命机关
变球机关
木球相关对象
若干场景机关
```

它不按单一机关类型组织。  
这个组更接近“第一小节里有哪些东西”。

后面读到 `IngameParameter` 里的 `Activate Sector`、`Deactivate Sector` 时，再回到小节组。  
现在先知道 `Sector_01` 是一份对象集合。

## 第一关内容和运行流程的边界

第一关内容能说明：

```text
有哪些检查点标记
有哪些复活点标记
哪些对象属于加分机关
起点和终点标记叫什么
第一小节包含哪些对象
```

它还不能说明：

```text
检查点何时激活
死亡后回到哪里
分数何时增加
球何时切换
终点触发后如何结算
对象何时进入物理系统
```

这些要继续看：

```text
CurrentLevel
Checkpoints
ResetPoints
AllLevel
PH_Groups
Gameplay 行为图
Levelinit 行为图
```

第二篇后半段会把这些名字放到正确层级。  
后面会用 BML 脚本 mod 调用 CKAS/CK 绑定，验证运行时能看到什么。

## 后续要验证的名字

先验证组：

```text
P_Extra_Point
PC_Checkpoints
PR_Resetpoints
PE_Levelende
PS_Levelstart
Sector_01
```

再验证运行时可查的检查点相关 3D 实体：

```text
PC_TwoFlames_Flame_Big
PC_TwoFlames_MF
PC_TwoFlames_Flame_SmallA
PC_TwoFlames_Flame_SmallB
```

这些名字和 `PC_TwoFlames_01` 所在层级不同。  
前者是 Player 运行时更适合脚本观察的实体名，后者来自第一关内容里的检查点标记。

最后验证表：

```text
CurrentLevel
AllLevel
Physicalize_Balls
```

后面再扩展到：

```text
Checkpoints
ResetPoints
PH_Groups
IngameParameter
```

## 本章结果

第二篇的世界模型到这里先收住。

现在应该能把第一关内容放到这张图里：

```text
Level_01
  对象和组
    ↓
Levelinit
  整理成运行时表
    ↓
Gameplay
  持续处理玩法状态
    ↓
BML + CKAS 脚本
  在运行时观察对象、组、表和事件
```

下一章开始用 CKAS 脚本查找对象和组。
