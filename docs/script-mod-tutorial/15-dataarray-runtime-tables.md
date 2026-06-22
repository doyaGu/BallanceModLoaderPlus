# 第 15 章：DataArray 是运行时表

第 14 章讲了组：

```text
组回答：哪些对象属于这一类
```

这一章讲 DataArray：

```text
DataArray 回答：原版逻辑现在记着什么
```

Virtools 的 DataArray 类型叫：

```text
CKDataArray
```

先把它当成表。

## 表格模型

DataArray 可以按三层看：

```text
列       字段
行       一组字段值
单元格   某一行某一列的值
```

例如 `CurrentLevel` 可以先想成这样：

```text
表名：CurrentLevel

列：
  Level ID
  ActiveBall
  Ball_Pos_Frame
  CurrentResetpoint
  Activation Phase?
  Points

第 0 行：
  当前关卡
  当前球
  球位置相关对象
  当前复活点
  运行阶段开关
  分数
```

后面写脚本读表时，也按这个顺序走：

```text
找表
找列
读行列交叉的单元格
```

## DataArray 接住初始化结果

对象和组来自关卡内容。  
DataArray 常常出现在初始化和运行流程里。

继续用检查点看：

```text
Level_01.NMO
  PC_Checkpoints      检查点名单
  PR_Resetpoints      复活点名单

Levelinit.nmo
  读取这些名单
  整理出运行时状态

DataArray
  Checkpoints
  ResetPoints
  CurrentLevel
```

这就是 DataArray 的位置。  
它把“关卡里放了什么”变成“这一局现在怎样运行”。

## `CurrentLevel` 是当前这一局的黑板

`CurrentLevel` 只有一行，适合入门。

常见列：

```text
Level ID
ActiveBall
Ball_Pos_Frame
CurrentResetpoint
Activation Phase?
Points
```

这些列对应的问题很直接：

| 列名 | 含义 |
| --- | --- |
| `Level ID` | 当前关卡编号 |
| `ActiveBall` | 当前活动球 |
| `Ball_Pos_Frame` | 球出生或复位相关位置 |
| `CurrentResetpoint` | 当前复活点 |
| `Activation Phase?` | 运行阶段中的开关状态 |
| `Points` | 当前分数 |

进入第一关后，能读到这类结果：

```text
CurrentLevel: rows=1 columns=6
Level ID: 1
ActiveBall: Ball_Wood
Points: 0
```

这说明脚本已经读到了当前一局的状态，而不只是关卡里有什么对象。

入门阶段先读。  
写 `CurrentLevel` 会影响 Gameplay，放到后面的修改章节。

## `AllLevel` 是 13 关起始配置

`AllLevel` 更像关卡配置表。

它有 13 行，对应原版 13 关。  
常见列：

```text
Levelfile
StartBall
StartResetpoint
Sky
Light
Skytranslation
LevelBonus
Music
```

它回答：

```text
第几关加载哪个关卡文件
起始球是什么
起始复活点是什么
天空和光照配置是什么
关卡奖励是多少
音乐编号是多少
```

`AllLevel` 和 `CurrentLevel` 的区别：

| 表名 | 更像什么 |
| --- | --- |
| `AllLevel` | 每一关开始前的配置 |
| `CurrentLevel` | 当前这一局正在使用的状态 |

配置表常常在初始化时被读取，然后写入当前运行状态。

## `Checkpoints` 和 `ResetPoints`

检查点和复活点在组里是对象名单。  
经过初始化后，会进入运行时表。

先按名字理解：

| 表名 | 作用 |
| --- | --- |
| `Checkpoints` | 检查点运行时数据 |
| `ResetPoints` | 复活点运行时数据 |

它们和第 14 章的组连起来：

```text
PC_Checkpoints  ->  Checkpoints
PR_Resetpoints  ->  ResetPoints
```

箭头表示“运行流程会读取前面的组，并整理出后面的表”。  
初始化过程还可能处理顺序、对象引用、矩阵、状态位等信息。

所以脚本读表时，要把它当成原版流程整理后的结果。

## `Physicalize_Balls` 和 `PH_Groups`

物理相关表会在后面继续用。

先记两个：

```text
Physicalize_Balls
PH_Groups
```

`Physicalize_Balls` 保存球的物理参数，例如：

```text
P_Ball_Wood: friction=0.6 elasticity=0.2 mass=2 radius=2
P_Ball_Stone: friction=0.7 elasticity=0.1 mass=10 radius=2
```

`PH_Groups` 把对象组接到物理化处理配置里，常见列：

```text
Group Names
Amount
Activation
Reset
```

这能解释为什么同一个组名会在不同位置出现：

```text
P_Extra_Point
  在关卡内容里：加分机关名单
  在 PH_Groups 里：需要进入运行时处理的一类对象
```

组提供对象来源。  
表告诉原版流程怎样处理这一类对象。

## 行为图会读写 DataArray

DataArray 自己只保存数据。  
真正让状态变化的是行为图。

后面会看到这两个 Building Block：

```text
Get Cell
Set Cell
```

先按名字理解：

| Building Block | 作用 |
| --- | --- |
| `Get Cell` | 从表格单元格读值 |
| `Set Cell` | 往表格单元格写值 |

看到 `Set Cell` 时，应该问：

```text
写哪张表？
写哪一行？
写哪一列？
写入什么值？
什么时候写？
```

这比记住表名更关键。

## 脚本读表的稳定顺序

后面写 CKAS 脚本读 DataArray 时，按这条线走：

```text
按名字找表
  找不到：显示 not found
  找到：读取行数和列数
    按列名找列号
      按行号和列号读单元格
```

稳定写法：

```text
进入关卡后重新找表
按列名找列号，不靠猜列号
读不到列时显示清楚
先读，不急着写
不要长期保存 raw `CKDataArray@`
```

写表风险更高。  
有些值会被 Gameplay 持续更新，有些值是对象引用或矩阵文本，有些表会在关卡切换后重新创建。

## 本章结果

先记住：

```text
CKDataArray 是表。
表有行、列、单元格。
CurrentLevel 是当前这一局的状态黑板。
AllLevel 是 13 关起始配置。
Checkpoints / ResetPoints 是检查点和复活点的运行时数据。
Physicalize_Balls / PH_Groups 和物理化流程有关。
行为图通过 Get Cell / Set Cell 读写表。
```

下一章讲行为图和 Building Block。  
它会解释这些表里的值怎样被原版流程读取和改写。
