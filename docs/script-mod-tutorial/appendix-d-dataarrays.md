# 附录 D：常用 DataArray

本附录用于查 Ballance 教程里反复出现的 DataArray 名字。

DataArray 先按表格理解：

```text
表名
  行
  列
  单元格
```

脚本读取时通常按这条线走：

```text
BorrowDataArrayByName(表名)
  -> GetRowCount / GetColumnCount
  -> FindColumn(列名)
  -> GetString / GetInt / GetFloat / GetBool
```

表名和列名要按运行时实际名字写。大小写不同，查找结果可能为空。

## 总览

| 表名 | 先这样理解 | 风险 |
| --- | --- | --- |
| `CurrentLevel` | 当前这一局的状态 | 高 |
| `IngameParameter` | Gameplay 运行中的开关和小节编号 | 高 |
| `Energy` | 分数、生命和奖励参数 | 高 |
| `AllLevel` | 13 关起始配置 | 中到高 |
| `PH_Groups` | 作者标记分类和运行时处理配置 | 高 |
| `PH` | Levelinit 的占位符/物理化整理结果之一 | 高 |
| `Checkpoints` | 检查点运行时表 | 高 |
| `ResetPoints` | 复活点运行时表 | 高 |
| `Physicalize_Floors` | 地板物理参数来源之一 | 高 |
| `Physicalize_Convex` | 凸包物理参数来源之一 | 高 |
| `Physicalize_Balls` | 木球、石球球形物理参数 | 中到高 |

这里的风险指“写入风险”。读取这些表用于学习和诊断，风险低得多。写入前要知道原版流程什么时候读它、什么时候改它、退出时如何恢复。

## `CurrentLevel`

`CurrentLevel` 记录当前这一局的关键状态。

正文里读过的常见列：

| 列名 | 先这样理解 |
| --- | --- |
| `Level ID` | 当前关卡编号 |
| `ActiveBall` | 当前活动球 |
| `Ball_Pos_Frame` | 球出生或复位相关位置 |
| `CurrentResetpoint` | 当前复活点矩阵 |
| `Activation Phase?` | 运行阶段中的开关状态 |
| `Points` | 当前分数 |

常见结果：

```text
CurrentLevel rows=1 columns=6
Level ID: 1
ActiveBall: Ball_Wood
Points: 0
```

它适合用来观察：

```text
当前第几关
当前球类型
当前复活点
当前分数
```

写 `CurrentLevel` 属于高风险操作。Gameplay 会持续使用这张表。

## `AllLevel`

`AllLevel` 是原版 13 关的起始配置表。

正文里读过的常见列：

| 列名 | 先这样理解 |
| --- | --- |
| `Levelfile` | 关卡内容文件 |
| `StartBall` | 起始球 |
| `StartResetpoint` | 起始复活点 |
| `Sky` | 天空配置 |
| `Light` | 光照配置 |
| `Skytranslation` | 天空位移相关配置 |
| `LevelBonus` | 关卡奖励分 |
| `Music` | 音乐编号 |

行号和关卡号要分清：

```text
row 0   第 1 关
row 12  第 13 关
```

常见结果：

```text
AllLevel rows=13 columns=8
Level 1: Level_01.nmo start=Ball_Wood bonus=100 music=1
Level 13: Level_13.nmo start=Ball_Stone bonus=1300 music=5
```

它适合用来观察：

```text
每关加载哪个关卡内容文件
每关起始球是什么
奖励分和音乐编号怎么配
```

修改 `AllLevel` 会影响开局配置，发布 mod 前要非常谨慎。

## `Physicalize_Balls`

`Physicalize_Balls` 保存木球、石球的球形物理参数。

正文里读过的常见列：

| 列名 | 先这样理解 |
| --- | --- |
| `Group Name` | 球类型组名 |
| `Friction` | 摩擦 |
| `Elasticity` | 弹性 |
| `Mass` | 质量 |
| `Radius` | 球形碰撞半径 |

常见结果：

```text
Physicalize_Balls rows=2 columns=12
P_Ball_Wood: friction=0.6 elasticity=0.2 mass=2 radius=2
P_Ball_Stone: friction=0.7 elasticity=0.1 mass=10 radius=2
```

这张表说明木球和石球的物理参数不同。运行中的物理对象还要经过物理化流程创建。

## `PH_Groups`

`PH_Groups` 把作者内容里的分类名接到 Levelinit 的运行时处理流程里。

正文里读过的常见列：

| 列名 | 先这样理解 |
| --- | --- |
| `Group Names` | 作者内容里的分类名 |
| `Amount` | 当前关卡里这一类对象数量 |
| `Activation` | 激活分类编号 |
| `Reset` | 复位分类编号 |

常见结果：

```text
PH_Groups rows=23 columns=4
PH_Groups P_Extra_Point: amount=4 activation=1 reset=1
PH_Groups P_Trafo_Stone: amount=1 activation=1 reset=0
PH_Groups P_Trafo_Wood: amount=1 activation=1 reset=0
PH_Groups P_Ball_Stone: amount=5 activation=3 reset=2
PH_Groups P_Ball_Wood: amount=2 activation=3 reset=2
```

`Amount` 会随关卡变化。`Activation` 和 `Reset` 先按分类编号理解，具体流程要看 Levelinit / Gameplay 行为图。

## `PH`

`PH` 属于 Levelinit 的占位符和物理化整理体系。

正文没有把 `PH` 的列结构完整展开。现在先按用途记：

```text
PH_Groups 说明有哪些分类要处理
PH 保存进一步整理后的运行时数据
```

看到 `PH` 时，不要把它和 `PH_Groups` 混成一张表。它们名字接近，但用途不同。

第一次学习时，优先读 `PH_Groups`。等能看懂 Levelinit 行为图以后，再展开 `PH`。

## `Checkpoints`

`Checkpoints` 是检查点运行时表。

它来自 Levelinit 对 `PC_Checkpoints` 组的整理。

正文里读过的常见列：

| 列名 | 先这样理解 |
| --- | --- |
| `Matrix` | 检查点位置和方向 |
| `Object` | 检查点相关运行时对象 |

常见结果：

```text
Checkpoints rows=3 columns=2
row=0 object=PC_TwoFlames_MF matrix=[...]
row=1 object=PC_TwoFlames_MF001 matrix=[...]
row=2 object=PC_TwoFlames_MF matrix=[...]
```

和作者内容的关系：

```text
PC_Checkpoints 组
  -> Levelinit
  -> Checkpoints 表
  -> Gameplay 使用
```

写这张表会影响检查点流程。

## `ResetPoints`

`ResetPoints` 是复活点运行时表。

它来自 Levelinit 对 `PR_Resetpoints` 组的整理。

正文里读过的常见列：

| 列名 | 先这样理解 |
| --- | --- |
| `Resetpoint` | 复活点位置和方向 |

常见结果：

```text
ResetPoints rows=4 columns=1
row=0 resetpoint=[...]
row=1 resetpoint=[...]
row=2 resetpoint=[...]
row=3 resetpoint=[...]
```

注意表名大小写：

```text
ResetPoints
```

不要写成 `Resetpoints`。

`CurrentLevel.CurrentResetpoint` 会保存当前使用的复活点矩阵。

## `IngameParameter`

`IngameParameter` 保存 Gameplay 运行中的开关和小节编号。

正文里读过的常见列：

| 列名 | 先这样理解 |
| --- | --- |
| `Activetrafo` | 当前变球器相关状态 |
| `Activate Sector` | 当前要激活的小节编号 |
| `Deactivate Sector` | 当前要关闭的小节编号 |
| `Tutorial activ?` | 教程提示相关开关 |
| `RollSound activate?` | 滚动声音相关开关 |

常见结果：

```text
IngameParameter rows=1 columns=5
Activetrafo:
Activate Sector: 1
Deactivate Sector: 0
Tutorial activ?: TRUE
RollSound activate?: FALSE
```

列名里的 `Sector` 是原版名字。正文里按小节理解。

写这张表会影响 Gameplay 正在运行的状态，风险很高。

## `Energy`

`Energy` 保存分数、生命和奖励相关参数。

正文里读过的常见列：

| 列名 | 先这样理解 |
| --- | --- |
| `Points` | 当前分数相关值 |
| `Lifes` | 当前生命显示相关值 |
| `StartPoints` | 开局基础分 |
| `StartLifes` | 开局生命数 |
| `Timefactor` | 计时或分数计算相关参数 |
| `LifeBonus` | 生命奖励分 |

常见结果：

```text
Energy rows=1 columns=6
Points: 0
Lifes: 0
StartPoints: 1000
StartLifes: 3
Timefactor: 00m 00s 500ms
LifeBonus: 200
```

`Points` 在 `CurrentLevel` 里也出现过。分数相关流程可能同时读写多张表。

## `Physicalize_Floors`

`Physicalize_Floors` 是地板物理参数来源之一。

正文没有单独展开它的列结构。先按这个位置理解：

```text
Physicalize_Balls    球形物理参数
Physicalize_Floors   地板相关物理参数
Physicalize_Convex   凸包相关物理参数
```

遇到地板摩擦、弹性、碰撞组相关问题时，可能会看到这张表。

## `Physicalize_Convex`

`Physicalize_Convex` 是凸包物理参数来源之一。

有些对象进入物理系统时使用凸包碰撞，而非球形碰撞。遇到这类对象时，会从 `Physicalize_Balls` 转向 `Physicalize_Convex` 或其他物理配置。

第一次学习物理参数时，先从 `Physicalize_Balls` 入门，再看 `OnPhysicalize` 事件，最后再展开 `Physicalize_Convex`。

## 读取 DataArray 的安全顺序

每张表都按这个顺序读：

```angelscript
CKDataArray@ table = ctx.BorrowDataArrayByName("CurrentLevel");
if (table is null) {
    return;
}

int rows = BML::CK::GetRowCount(table);
if (rows <= 0) {
    return;
}

int column = BML::CK::FindColumn(table, "Level ID");
if (column < 0) {
    return;
}

int value = BML::CK::GetInt(table, 0, column, 0);
```

顺序不要反过来。先找表，再看行数，再找列，再读单元格。

## 写入前检查

写 DataArray 之前，至少确认：

```text
这张表由哪个流程维护
要写哪一行哪一列
旧值如何保存
什么时候恢复旧值
关卡切换时如何清理
写错后能不能靠重进关卡恢复
```

教程里的主线是：

```text
先读 CurrentLevel / AllLevel / Physicalize_Balls
再理解 Levelinit / Gameplay 如何读写表
最后才在受控修改章节做写入和回滚
```

DataArray 是理解原版流程的重要入口。把表当成运行时状态看，比把它当成“可以随手改的配置文件”更稳。
