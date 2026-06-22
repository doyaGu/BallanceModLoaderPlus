# 第 13 章：对象、实体和名字

上一章把文件分工放清楚了：

```text
关卡文件提供内容
Levelinit 整理状态
Gameplay 维持玩法
脚本在运行时观察
```

这一章看脚本最先会碰到的东西：名字。

Ballance 里的很多名字都像“对象名”，但它们可能指向不同层：

```text
一个具体对象
一份对象名单
一张运行时表
一段行为流程
```

这几层混在一起，后面查对象、读表、看行为图都会乱。

## `CKObject` 是共同基础

Virtools 世界里很多东西都属于对象系统：

```text
3D 实体
组
DataArray
行为图
材质
贴图
```

共同基础可以先记成：

```text
CKObject
```

它太宽了。  
脚本写起来时，还要继续区分更具体的类型：

| 类型 | 先这样理解 |
| --- | --- |
| `CK3dEntity` | 有 3D 空间状态的实体 |
| `CKGroup` | 对象名单 |
| `CKDataArray` | 表 |
| `CKBehavior` | 行为图、子图或节点 |

看到 `CKObject@`，先按“某个 Virtools 对象句柄”理解。  
后面要根据 class id 或查询入口确认它到底是哪一类。

## `CK3dEntity` 是有空间状态的实体

关卡里很多东西有位置、旋转、缩放：

```text
地板
机关
检查点火焰
复活点标记
球相关对象
```

这些内容接近脚本 API 里的：

```text
CK3dEntity
```

入门阶段先抓住一件事：

```text
需要读位置、改位置、显示隐藏、看父子层级时，通常会接触 3D 实体。
```

后面会用：

```text
Borrow3dEntityByName(...)
```

这个入口按名字查 3D 实体。

## 名字只是入口

脚本最容易从名字开始查：

```text
Borrow3dEntityByName("PC_TwoFlames_Flame_Big")
BorrowGroupByName("PC_Checkpoints")
BorrowDataArrayByName("CurrentLevel")
```

共同点是：

```text
ByName
```

这表示按名字查。  
但名字本身不能说明完整含义。

拿检查点相关名字举例：

| 名字 | 更接近哪一层 |
| --- | --- |
| `PC_TwoFlames_01` | 关卡内容里的检查点标记 |
| `PC_Checkpoints` | 检查点对象名单 |
| `Checkpoints` | 检查点运行时表 |
| `CurrentLevel` | 当前这一局状态表 |
| `activate next Checkpoint` | 行为流程名字 |

它们都和检查点有关，回答的问题完全不同。

## id 是本次运行编号

脚本日志里会看到：

```text
PC_Checkpoints: id=44123 class=23
PC_TwoFlames_Flame_Big: id=46376 class=33
```

这里的 `id` 是运行时编号。

它适合做两件事：

```text
日志里区分对象
确认两次查到的是否同一个运行时对象
```

不要把 id 写死进脚本逻辑。  
关卡加载顺序、mod 环境、运行状态变化后，编号都可能变。

更稳的习惯是：

```text
保存名字
在合适时机重新查
查到以后立刻读取 name / id / class id / 位置等信息
```

## class id 是类型编号

class id 用来确认对象类型。

常见结果：

```text
class=23  CKGroup
class=33  CK3dEntity
```

看到 `PC_Checkpoints: class=23`，说明它是组。  
看到 `PC_TwoFlames_Flame_Big: class=33`，说明它是 3D 实体。

这能帮助排查查找入口选错的问题：

```text
组名要用 BorrowGroupByName
3D 实体名要用 Borrow3dEntityByName
DataArray 名要用 BorrowDataArrayByName
```

入口选错时，经常得到 `null`。

## 三个检查点名字不要混

检查点最适合练习分层。

先看三层：

| 名字 | 类型 | 回答的问题 |
| --- | --- | --- |
| `PC_TwoFlames_01` | 关卡内容里的检查点标记 | 关卡作者放了哪个标记 |
| `PC_Checkpoints` | 组 | 哪些对象属于检查点名单 |
| `Checkpoints` | DataArray | 初始化后检查点数据怎么组织 |

再加一层运行时 3D 实体：

| 名字 | 类型 | 回答的问题 |
| --- | --- | --- |
| `PC_TwoFlames_Flame_Big` | 3D 实体 | 当前运行时能查到的检查点火焰实体 |

所以脚本找不到 `PC_TwoFlames_01`，不能马上判断检查点系统不存在。  
它可能是作者内容里的标记名，运行时可查的实体已经被 Levelinit 整理成别的对象。

这类差异很常见。  
写脚本要以运行时实际能查到的对象为准，再回头理解它和作者内容的关系。

## 什么时候查对象

时机和名字同样重要。

菜单阶段没有某一关的关卡内容。  
`OnLoad` 只说明脚本加载了，不说明第一关对象已经存在。

更合适的查找时机：

```text
关卡加载后
关卡开始后
手动执行重扫命令时
```

关卡对象按这条规则处理：

| 内容 | 稳定性 |
| --- | --- |
| 脚本自己的数字、字符串、配置 | 由脚本控制 |
| 对象名 | 可以长期保存 |
| 运行时 id | 只适合日志和临时确认 |
| raw `CKObject@` / `CK3dEntity@` | 只在当前操作附近使用 |
| 关卡对象 | 切关或重开后重新查 |

这能避开很多“上一关对象句柄还在用”的问题。

## 本章结果

先形成这个读法：

```text
CKObject 是 Virtools 对象的共同基础。
CK3dEntity 是有 3D 空间状态的实体。
名字只能作为查找入口。
id 是本次运行编号。
class id 帮助确认类型。
组、实体、DataArray 要用不同入口查。
```

下一章讲组。  
组会把多个对象收成一份名单，是理解 Ballance 关卡内容的第一个关键结构。
