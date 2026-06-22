# 附录 C：常用 Ballance 对象和组名

本附录用于查 Ballance 里常见的名字。它不替代正文里的概念解释，只帮你快速判断一个名字大概属于哪一层。

先看四类：

```text
组名          一份对象名单
对象名        关卡内容里的具体对象
运行时名字    Player 运行过程中可查到的对象
资源名        硬盘上的资源文件或可加载对象
```

同一个玩法概念可能跨好几层。检查点就是典型例子：

```text
PC_TwoFlames_01          关卡作者内容里的检查点对象
PC_Checkpoints           检查点对象名单
Checkpoints              Levelinit 整理出的运行时表
PC_TwoFlames_MF          Player 运行时可查到的检查点相关 3D 实体
```

写脚本时要按当前要做的事选入口。查组用 `BorrowGroupByName`，查 3D 实体用 `Borrow3dEntityByName`，查表用 `BorrowDataArrayByName`。

## 常用组名

| 组名 | 先这样理解 | 常见用途 |
| --- | --- | --- |
| `P_Extra_Point` | 加分机关名单 | 观察加分机关对象 |
| `P_Extra_Life` | 加命机关名单 | 观察加命对象 |
| `PC_Checkpoints` | 检查点名单 | 进入检查点系统的作者内容入口 |
| `PR_Resetpoints` | 复活点名单 | 进入复活点系统的作者内容入口 |
| `PE_Levelende` | 关卡结束标记名单 | 查关卡终点标记 |
| `PS_Levelstart` | 关卡起点标记名单 | 查关卡起点标记 |
| `Sector_XX` | 第 XX 小节对象集合 | 观察某一小节包含的对象 |

组名适合回答：

```text
这一类对象有哪些？
这批对象被作者放进了哪份名单？
```

组名不能直接回答：

```text
对象当前是否已经激活
分数是否已经增加
检查点当前推进到哪里
复活点当前选中了哪个
```

这些运行状态要继续看 DataArray 和行为图。

## 第一关常见组

第一关正文重点用过这六个组：

```text
P_Extra_Point
PC_Checkpoints
PR_Resetpoints
PE_Levelende
PS_Levelstart
Sector_01
```

它们适合用来练习“组是一份对象名单”：

| 组名 | 第一关里对应的内容 |
| --- | --- |
| `P_Extra_Point` | 第一关加分机关对象 |
| `PC_Checkpoints` | 第一关 3 个检查点标记 |
| `PR_Resetpoints` | 第一关 4 个复活点标记 |
| `PE_Levelende` | 第一关终点标记 |
| `PS_Levelstart` | 第一关起点标记 |
| `Sector_01` | 第一小节对象集合 |

这些名字属于关卡内容层。完整玩法还要经过 `Levelinit.nmo` 和 `Gameplay.nmo`。

## 常用对象名

下面这些名字在正文里反复出现，用来建立“具体对象”的概念：

| 对象名 | 先这样理解 |
| --- | --- |
| `P_Extra_Point_01` | 一个加分机关对象 |
| `P_Extra_Life_04` | 一个加命对象 |
| `PC_TwoFlames_01` | 一个检查点标记 |
| `PR_Resetpoint_01` | 一个复活点标记 |
| `PE_Balloon_01` | 一个关卡结束标记 |
| `PS_FourFlames_01` | 一个关卡起点标记 |
| `P_Trafo_Stone_02` | 一个变石球机关对象 |
| `P_Trafo_Wood_05` | 一个变木球机关对象 |
| `P_Ball_Wood_03` | 一个木球相关对象 |
| `P_Ball_Paper_01` | 一个纸球相关对象 |

对象名通常来自关卡作者内容。运行时能不能直接按这个名字查到 3D 实体，要看加载和初始化后对象是否还以这个名字存在。

## 检查点相关名字

检查点容易混淆，单独列出来：

| 名字 | 类型 | 先这样理解 |
| --- | --- | --- |
| `PC_Checkpoints` | 组 | 检查点名单 |
| `PC_TwoFlames_01` | 关卡对象 | 作者放置的检查点标记 |
| `Checkpoints` | DataArray | 初始化后的检查点运行时表 |
| `PC_TwoFlames_MF` | 3D 实体 | Player 运行时可查到的检查点相关实体 |
| `PC_TwoFlames_Flame_Big` | 3D 实体 | Player 运行时可查到的检查点火焰相关实体 |
| `PC_TwoFlames_Flame_SmallA` | 3D 实体 | Player 运行时可查到的检查点火焰相关实体 |
| `PC_TwoFlames_Flame_SmallB` | 3D 实体 | Player 运行时可查到的检查点火焰相关实体 |

第 28 章使用运行时名字做查找练习。第 33 章把 `PC_Checkpoints` 和 `Checkpoints` 表串起来。

## 复活点相关名字

| 名字 | 类型 | 先这样理解 |
| --- | --- | --- |
| `PR_Resetpoints` | 组 | 复活点名单 |
| `PR_Resetpoint_01` | 关卡对象 | 一个复活点标记 |
| `ResetPoints` | DataArray | 初始化后的复活点运行时表 |
| `CurrentLevel.CurrentResetpoint` | DataArray 单元格 | 当前复活点状态 |

注意大小写：正文统一写 `ResetPoints` 表。对象组名是 `PR_Resetpoints`。

## 起点和终点相关名字

| 名字 | 类型 | 先这样理解 |
| --- | --- | --- |
| `PS_Levelstart` | 组 | 关卡起点标记名单 |
| `PS_FourFlames_01` | 关卡对象 | 起点标记 |
| `PE_Levelende` | 组 | 关卡结束标记名单 |
| `PE_Balloon_01` | 关卡对象 | 终点标记 |

起点和终点只是作者内容入口。开始一局游戏、结束一局游戏还会涉及配置表、运行时表和 Gameplay 行为图。

## 加分、加命和变球

| 名字 | 类型 | 先这样理解 |
| --- | --- | --- |
| `P_Extra_Point` | 组 | 加分机关名单 |
| `P_Extra_Point_01` | 关卡对象 | 一个加分机关对象 |
| `P_Extra_Life` | 组 | 加命机关名单 |
| `P_Extra_Life_04` | 关卡对象 | 一个加命对象 |
| `P_Trafo_Stone_02` | 关卡对象 | 一个变石球机关对象 |
| `P_Trafo_Wood_05` | 关卡对象 | 一个变木球机关对象 |

这些名字只说明关卡内容里有对应对象。分数增加、生命增加、球类型变化都属于运行时流程。

## 球相关名字

常见球类型：

```text
P_Ball_Wood
P_Ball_Stone
P_Ball_Paper
```

正文里会在两个地方看到它们：

| 场景 | 含义 |
| --- | --- |
| `Physicalize_Balls` 表 | 球形物理参数来源 |
| `PH_Groups` 表 | 需要整理和物理化的作者标记类别 |

常见具体对象名：

```text
P_Ball_Wood_03
P_Ball_Paper_01
P_Ball_Paper_02
P_Ball_Paper_03
P_Ball_Paper_04
```

不要把这些对象名直接当成“当前活动球”。当前活动球要看 `CurrentLevel.ActiveBall`、Gameplay 状态和实际运行对象。

## 可加载测试资源

后期章节用过一个稳定的测试资源：

```text
3D Entities\PH\P_Modul_01.nmo
```

脚本字符串里写反斜杠时要转义：

```angelscript
string relativePath = "3D Entities\\PH\\P_Modul_01.nmo";
```

这个资源适合练习：

```text
LoadObject
查找加载结果里的 CK3dEntity
移动到安全位置
显示 / 隐藏
物理化 / 取消物理化
```

发布自己的 mod 时，资源应当放进 mod 自己的目录，再用相对路径读取。

## 小节名字

关卡里常见：

```text
Sector_01
Sector_02
Sector_03
...
```

正文按“小节对象集合”理解。它们通常表示一批关卡对象，而非某一种机关。

运行时还会看到和小节管理有关的名字：

```text
Activate Sector
Deactivate Sector
Gameplay_SectorManager
```

`Sector_XX` 更接近作者内容里的分组。`Activate Sector`、`Deactivate Sector` 更接近运行时状态和行为图流程。

## 查找入口速查

| 要查什么 | 用什么 |
| --- | --- |
| 组 | `ctx.BorrowGroupByName(name)` |
| 3D 实体 | `ctx.Borrow3dEntityByName(name)` |
| DataArray | `ctx.BorrowDataArrayByName(name)` |
| 对象 id / 名字 / class id | `BML::CK::GetId`、`BML::CK::GetName`、`BML::CK::GetClassId` |

找不到时先按这个顺序排查：

```text
是否已经进入关卡
名字是否属于当前关卡
查找入口是否对应正确类型
这个名字是否属于作者内容层，运行时可能已经换成别的对象名
```

附录 D 会继续列常用 DataArray。对象和组告诉你“有哪些东西”，DataArray 更接近“原版逻辑现在记着什么”。
