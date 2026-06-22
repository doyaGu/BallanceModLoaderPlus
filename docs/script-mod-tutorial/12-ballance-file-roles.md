# 第 12 章：Ballance 文件各自负责什么

第一篇一直站在 BML 脚本这边：

```text
脚本加载
脚本窗口
命令、配置、Timer
BML 事件和服务
```

从这一章开始，要把脚本看到的东西放回 Ballance 原版运行流程里。

先用一句话定位置：

```text
脚本 mod 在运行中的 Player 里观察和影响 Ballance，Ballance 本身由多个 Virtools 文件和 Building Block 共同驱动。
```

玩家点进第一关时，画面上只看到球、地板、机关和检查点。  
脚本要理解这局游戏，不能只看一个关卡文件。

## 一局游戏由多个文件接起来

Ballance 运行时至少要把这几类内容接起来：

```text
base.cmo
  进入菜单，调度关卡加载

3D Entities/Level/Level_XX.NMO
  提供某一关的地图内容

3D Entities/Levelinit.nmo
  把关卡内容整理成一局游戏要用的状态

3D Entities/Gameplay.nmo
  在关卡开始后持续处理玩法

BuildingBlocks/*.dll
  提供行为图节点的具体能力

BML 脚本 mod
  在运行时接入，读取、显示、少量干预
```

这几个名字先不要背成清单。  
更重要的是看它们在一局游戏里的先后关系：

```text
Player 启动
  base.cmo 进入菜单
    玩家选择关卡
      Levelinit 加载 Level_XX.NMO
        Levelinit 整理对象、组和运行时表
          Gameplay 接手关卡运行
            BML 脚本在事件点和每帧里观察状态
```

脚本碰到的大量问题，都来自这条链上的位置判断。

## `Level_XX.NMO` 提供关卡内容

`Level_XX.NMO` 指原版 13 个关卡文件：

```text
Level_01.NMO
Level_02.NMO
...
Level_13.NMO
```

它们提供的是关卡作者放进去的内容：

```text
地板和轨道
机关对象
检查点标记
复活点标记
起点标记
终点标记
对象组
材质、网格、贴图引用
```

拿第一关检查点举例。关卡内容里能看到：

```text
PC_Checkpoints
  PC_TwoFlames_01
  PC_TwoFlames_02
  PC_TwoFlames_03
```

这说明第一关放了 3 个检查点标记，并把它们收进一份检查点名单。

但这还没有解释：

```text
哪个检查点当前有效
触发后当前复活点怎么变
死亡后球回到哪里
火焰什么时候显示或隐藏
状态写在哪张表里
```

关卡文件提供“内容”。完整玩法还要继续看 Levelinit 和 Gameplay。

## `Levelinit.nmo` 把内容整理成运行时状态

`Levelinit.nmo` 可以先按“关卡初始化流程”理解。

它站在中间：

```text
关卡作者内容
  ↓
运行时状态
```

它会处理这类事情：

```text
根据关卡编号加载 Level_XX.NMO
读取关卡配置表
读取关卡里的对象组
整理检查点、复活点、起点、终点
准备物理化对象
写入 CurrentLevel、Checkpoints、ResetPoints 等表
```

所以脚本看到一个对象存在，只能说明内容已经在运行时里。  
要知道原版流程怎样使用它，还要看初始化后形成的表和状态。

检查点就是典型例子：

```text
Level_01.NMO
  提供 PC_Checkpoints 这份名单

Levelinit.nmo
  读取这份名单
  结合复活点和配置
  整理成运行时表
```

这也是后面会讲 `CurrentLevel`、`Checkpoints`、`ResetPoints` 的原因。

## `Gameplay.nmo` 维持运行中的玩法

`Gameplay.nmo` 接手关卡开始后的运行逻辑。

它持续处理：

```text
球控制
相机控制
当前球类型
检查点推进
复活点更新
分数和生命
掉落死亡
关卡结束
内部消息
运行时表更新
```

这会影响脚本修改的判断。

例如脚本把一个对象移动了，Gameplay 后续仍然可能再设置它的位置。  
脚本改了某个表值，行为图后续仍然可能继续写这张表。

所以后面做修改前，要先问：

```text
我改的是关卡内容对象？
我改的是初始化后的运行时表？
我改的是 Gameplay 正在持续维护的状态？
```

这三个位置的风险不同。

## Building Block 提供行为图节点能力

Virtools 原版逻辑大量放在行为图里。

行为图里的节点需要 Building Block 提供执行能力。可以先这样理解：

```text
行为图负责组织流程。
Building Block 负责具体动作。
```

常见动作包括：

```text
读 DataArray 单元格
写 DataArray 单元格
发送内部消息
等待内部消息
显示或隐藏对象
设置对象位置
把对象交给物理系统
```

后面看到 `Get Cell`、`Set Cell`、`Send Message`、`Physicalize` 这些名字时，不要只当成陌生术语。  
它们就是原版流程真正动起来的动作节点。

## BML 脚本 mod 站在哪

BML 脚本 mod 不替代上面的文件。  
它在 Player 运行后接进来，通过 BML 和 CKAS 接触运行时对象。

可以这样放位置：

```text
Ballance 原版内容
  base.cmo
  Levelinit.nmo
  Gameplay.nmo
  Level_XX.NMO
  BuildingBlocks

BML
  脚本生命周期、事件、命令、配置、Timer、日志、UI

CKAS
  让脚本接触 Virtools 对象、组、DataArray、行为等运行时对象

脚本 mod
  使用 BML 和 CKAS，在合适时机读取或影响这些内容
```

写脚本时，先判断自己碰的是哪一层：

| 想做的事 | 主要接触 |
| --- | --- |
| 显示窗口、注册命令、读配置 | BML |
| 找一个检查点对象 | CKAS / 运行时对象 |
| 找检查点名单 | CKAS / `CKGroup` |
| 读当前关卡、当前球、当前复活点 | CKAS / `CKDataArray` |
| 理解检查点怎样推进 | Levelinit、Gameplay、行为图 |
| 改复活、物理、过关流程 | Gameplay、DataArray、物理相关 Building Block |

越靠近 Gameplay 的持续流程，越需要小心。

## 本章结果

先记住这条线：

```text
Level_XX.NMO 提供关卡内容。
Levelinit.nmo 把内容整理成运行时状态。
Gameplay.nmo 维持关卡开始后的玩法。
Building Block 给行为图节点提供动作。
BML 脚本 mod 在运行时观察和干预。
```

下一章讲对象、实体和名字。  
这一步会解释为什么 `PC_Checkpoints`、`PC_TwoFlames_01`、`CurrentLevel` 不能混在一起看。
