# 第 16 章：行为图和 Building Block

第 15 章讲了 DataArray：

```text
表保存状态
```

这一章讲谁在读写这些状态。

Virtools 里大量原版逻辑放在行为图里。  
行为图可以先理解成可执行的流程图。

一句话：

```text
行为图回答“原版逻辑怎么跑”。
```

## 行为图是什么

Virtools 的行为对象叫：

```text
CKBehavior
```

一个 `CKBehavior` 可能代表：

```text
一张行为图
一个子图
一个 Building Block 节点
```

入门阶段先用这套模型：

```text
图      容器
节点    做事
线      连接执行顺序和数据流向
参数    决定节点操作什么
```

Ballance 的 `Levelinit.nmo` 和 `Gameplay.nmo` 里有大量行为图。

`Levelinit.nmo` 偏初始化：

```text
加载关卡
读取组
整理表
准备物理化对象
```

`Gameplay.nmo` 偏运行中：

```text
球控制
相机控制
检查点推进
复活
分数
生命
关卡结束
```

## Building Block 是动作节点

Building Block 可以先理解成行为图里的动作块。

它有：

```text
输入    什么时候开始
输出    做完以后去哪里
参数    读写什么对象或值
```

常见名字：

```text
Get Cell
Set Cell
Send Message
Wait Message
Physicalize
Set World Matrix
Show
Hide
```

按用途看：

| Building Block | 作用 |
| --- | --- |
| `Get Cell` | 从 DataArray 读值 |
| `Set Cell` | 往 DataArray 写值 |
| `Send Message` | 发内部消息 |
| `Wait Message` | 等内部消息 |
| `Physicalize` | 把对象交给物理系统 |
| `Set World Matrix` | 设置对象世界矩阵 |
| `Show` / `Hide` | 显示或隐藏对象 |

这些节点组合起来，形成原版流程。

## 两种线要分开看

行为图里最容易混的是线。

第一类是执行线：

```text
这个节点执行完
  触发下一个节点
```

它回答：

```text
谁先运行，谁后运行？
```

第二类是参数线：

```text
这个值
  传给那个节点使用
```

它回答：

```text
数据从哪里来，给谁用？
```

拿 `Set Cell` 举例。

只看执行线，可以知道它什么时候写表。  
只看参数线，可以知道它写哪张表、哪一行、哪一列、写入什么值。

两类线合起来，才知道这个写表动作的完整含义。

## 用 DataArray 读写来理解行为图

第 15 章说过：

```text
DataArray 保存状态
```

行为图里看到：

```text
Get Cell
Set Cell
```

通常就在读写 DataArray。

追一个 `Set Cell` 时，按五个问题看：

```text
哪张表？
哪一行？
哪一列？
写入什么？
什么时候写？
```

例如 `CurrentLevel.Points` 变化时，画面上的分数只是结果之一。  
原版流程可能还在某处写了 `CurrentLevel` 表里的分数字段。

再比如关卡加载时，`AllLevel.Levelfile` 会作为配置来源，告诉流程加载哪个关卡文件。

表名、列名和行为图节点要一起看。

## 用消息理解流程之间的连接

Ballance 原版流程不只靠表。

行为图之间还会用内部消息连接：

```text
Send Message
Wait Message
```

可以先这样读：

```text
Send Message
  某个流程发出通知

Wait Message
  另一个流程等到通知后继续
```

检查点、死亡、复活、过关这类流程，经常会分成多条线。  
一个地方触发后，会通过消息让别的流程接着做事。

看消息时先问：

```text
谁发？
谁等？
收到后改了哪个对象或哪张表？
```

这三个问题比一开始记消息编号更有用。

## Levelinit 里重点看什么

`Levelinit.nmo` 的重点是把关卡内容准备成运行时状态。

可以先抓两类图：

```text
总流程
  加载关卡、设置 CurrentLevel、初始化表

处理关卡标记的流程
  检查点、复活点、起点、终点、物理化对象
```

第 14 章的组在这里开始变得有用：

```text
PC_Checkpoints
PR_Resetpoints
P_Extra_Point
```

第 15 章的表也在这里接上：

```text
CurrentLevel
Checkpoints
ResetPoints
PH_Groups
```

Levelinit 是“关卡内容”和“运行时状态”之间的桥。

## Gameplay 里重点看什么

`Gameplay.nmo` 的重点是关卡开始后的持续逻辑。

常见关注点：

```text
当前球
相机
检查点触发
复活
分数
生命
掉落死亡
关卡结束
```

看 Gameplay 行为图时，不要从整张大图开始硬读。

先选一个问题：

```text
检查点触发后谁更新 CurrentLevel？
死亡后谁读取当前复活点？
当前球什么时候切换？
分数在哪里增加？
终点触发后谁发结束消息？
```

然后沿着：

```text
对象名
组名
DataArray 名
Building Block 名
消息名
```

一路追。

## 为什么脚本修改可能被覆盖

行为图解释了一个常见现象：

```text
脚本改了对象或表，过一会又变回去了。
```

原因通常在原版流程里。

脚本设置对象位置后，Gameplay 可能继续执行：

```text
Set World Matrix
```

脚本改了表值后，行为图可能继续执行：

```text
Set Cell
```

所以修改前要判断：

```text
这个值是一次性初始化结果，还是 Gameplay 每帧或每个事件都会维护？
我改的是源头，还是只改了后果？
下一次复活、重开、切关会不会重建它？
```

这就是教程先安排观察，再安排修改的原因。

## 脚本 mod 怎样使用行为图知识

脚本入门阶段不需要改行为图。

行为图知识先用来判断：

```text
为什么这个对象会变化
为什么这个表值会变
一个组为什么会被 Levelinit 使用
一个消息可能连接哪些流程
修改会不会被后续流程覆盖
```

代码仍然从简单观察开始：

```text
找对象
找组
读 DataArray
记录事件
显示结果
```

行为图提供解释路径。  
它让这些观察结果变成可以追下去的线索。

## 本章结果

先记住：

```text
CKBehavior 可以表示图、子图或节点。
Building Block 是行为图里的动作节点。
执行线决定先后。
参数线决定数据来源和去向。
Get Cell / Set Cell 对应表读写。
Send Message / Wait Message 对应流程通信。
行为图解释原版逻辑怎样运行。
```

下一章用检查点把对象、组、DataArray、行为图串起来。
