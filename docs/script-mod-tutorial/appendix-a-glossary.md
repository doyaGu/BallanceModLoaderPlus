# 附录 A：术语表

本附录用于查名字。

遇到不熟的词，可以先看这里，再回到正文。

## 脚本 mod 相关

### BMLPlus

Ballance Mod Loader Plus。

教程里写的脚本 mod 由 BMLPlus 加载。
它负责：

```text
扫描 mod
读取 [bml.mod]
创建脚本类
调用 OnLoad / OnProcess / OnGameEvent 等回调
提供日志、命令、配置、Timer、UI 等服务
```

### BML 脚本 mod

带 `[bml.mod]` 标记的 AngelScript 类。

最小形式：

```angelscript
[bml.mod id="hello.script" name="Hello Mod" version="1.0.0" author="Tutorial" bml="0.3.12"]
class HelloMod {
    void OnLoad(const BML::ModContext &in ctx) {
    }
}
```

正文前半部分一直在写这种脚本。

### `[bml.mod]`

BML 脚本 mod 的声明。

里面的 `id`、`name`、`version`、`author`、`bml` 用来告诉 BMLPlus 这个脚本是谁、需要什么版本。

`id` 要稳定。
日志、配置和互操作都会用到它。

### `BML::ModContext`

BML 回调传进来的上下文对象。

常见用途：

```text
BorrowLogger()
BorrowConfig()
BorrowInputManager()
BorrowCKContext()
BorrowDataArrayByName()
```

它是 BML 脚本 mod 的入口上下文。

### Command

BML 命令。

前面教程里的例子：

```text
hello toggle
hello scan-level
hello scan-state
```

命令适合手动触发扫描、切换窗口、刷新状态。

### Config

BML 配置。

适合保存脚本自己的设置，例如窗口开关、日志开关、调试功能开关。

### Timer

BML 定时器。

适合延迟执行或隔一段时间执行一次。
不要把所有逻辑都塞进 `OnProcess` 每帧跑。

### GameEvent

BML 把游戏事件传给脚本的机制。

正文里用它判断：

```text
关卡开始
相机控制启用
球控制启用
吃到加分道具
```

脚本收到事件后，可以重新扫描对象或 DataArray。

## AngelScript 和 CKAS

### AngelScript

脚本语言。

它提供：

```text
class
函数
字符串
数组
句柄
引用参数
```

Ballance 相关能力来自 BML 和 CKAS 注册的类型与函数。

### CKAS / CKAngelScript

把 AngelScript 接到 Virtools / Ballance 的宿主和 API 层。

常见能力：

```text
Scene::*
ObjectRef@
Behavior::*
BB::*
Param::*
raw CK / Vx 类型绑定
```

正文里主要通过 BML 脚本使用其中一部分能力。

### `ScriptContext`

CKAS runtime script 使用的上下文。

它和 `BML::ModContext` 分属不同入口。
看到 CKAS 示例代码时，要先确认它跑在哪种上下文里。

### `CKBehaviorContext`

Virtools 行为执行时使用的上下文。

AngelScript Component 或 Building Block 执行时会接触这个概念。
它不等同于 BML 脚本回调里的 `BML::ModContext`。

## Virtools 文件和运行时

### Virtools

Ballance 使用的底层创作和运行系统。

它用这些东西组织游戏：

```text
对象
场景
组
DataArray
行为图
Building Block
消息
物理
```

### NMO

Virtools 对象/资源文件。

Ballance 里常见：

```text
Level_01.NMO
Level_13.NMO
Levelinit.nmo
Gameplay.nmo
```

具体关卡 NMO 主要提供关卡作者内容。
`Levelinit.nmo` 和 `Gameplay.nmo` 负责更多运行时逻辑。

### CMO

Virtools composition 文件。

正文里主要提到：

```text
base.cmo
```

它是 Ballance 进入关卡流程时的重要调度入口。

### `base.cmo`

Ballance 的核心加载和调度入口之一。

可以先这样理解：

```text
base.cmo 负责组织加载
Levelinit 准备一局游戏
Gameplay 维持一局游戏
```

### `Levelinit.nmo`

关卡初始化逻辑。

它会把关卡 NMO 里的作者标记转换成运行时表和对象状态。

正文里和它关系很近的表：

```text
AllLevel
PH_Groups
PH
Checkpoints
ResetPoints
Physicalize_Balls
```

### `Gameplay.nmo`

关卡运行中的玩法逻辑。

例如：

```text
球控制
相机控制
检查点
复活
小节激活
加分
过关
```

正文里把它理解为运行时状态机。

## Virtools 基础对象

### `CKObject`

Virtools 基础对象。

通常可以关注：

```text
id
名字
class id
```

脚本查对象时，经常先看这三项。

### `CK_ID`

Virtools 对象 id。

可以用于记录对象身份。
对象删除、重建后，需要重新确认 id 指向的对象仍然符合预期。

### `CK3dEntity` / `CK3dObject`

Virtools 3D 对象相关类型。

关卡里的模型、道具、球、轨道对象很多都是这类对象或它的派生类型。

它们有位置、旋转、缩放等空间状态。

### `CKGroup`

对象组。

Ballance 用组表达关卡作者标记。

正文里常见：

```text
P_Extra_Point
PC_Checkpoints
PR_Resetpoints
PE_Levelende
PS_Levelstart
Sector_01
```

组本身只是分类入口。
运行时逻辑通常还要经过 Levelinit 和 Gameplay。

### `CKDataArray`

Virtools 表格数据对象。

可以按普通表格理解：

```text
行
列
单元格
列名
值类型
```

正文里常见：

```text
CurrentLevel
AllLevel
PH_Groups
IngameParameter
Energy
Physicalize_Balls
```

### `CKBehavior`

Virtools 行为对象。

它可能是一个 Building Block，也可能是一个包含子行为的行为图。

看行为图时，先分清：

```text
这是一个节点
还是一个里面还有子图的容器
```

## 行为图

### Behavior Graph

行为图。

由节点、执行线、参数线、子图组成。

Ballance 的 `Levelinit.nmo` 和 `Gameplay.nmo` 中有大量行为图。

第 16 章给出的读法：

```text
图名
顶层块
DataArray 读写
消息
参数细节
```

### Building Block / BB

Virtools 的可视化逻辑块。

常见例子：

```text
Get Cell
Set Cell
Get Row
Send Message
Wait Message
Set World Matrix
Show
Physicalize
```

可以先把 BB 理解成带插口的函数块。

### Input / Output

行为图上的执行入口和执行出口。

Input 被激活后，BB 开始执行。
Output 被激活后，流程继续走向下一个节点。

### In Parameter / Out Parameter / Local Parameter

行为图里的数据参数。

```text
In Parameter：输入数据
Out Parameter：输出数据
Local Parameter：本行为或子图内部保存的数据
```

### 执行线

控制谁先运行、谁后运行的线。

读行为图时，它回答：

```text
流程怎么走？
```

### 参数线

传递数据的线。

读行为图时，它回答：

```text
这个值从哪里来？
这个值给谁用？
```

### Message

Virtools / Ballance 内部通信方式之一。

常见 BB：

```text
Send Message
Wait Message
```

脚本里的 BML GameEvent 和 Virtools Message 属于不同层。
读行为图时不要混在一起。

## CKAS 引用和句柄

### `ObjectRef@`

CKAS 高层对象引用。

适合保存对象身份。
使用时会重新验证对象是否仍然有效。

比长期保存 raw `CKObject@` 更适合脚本层。

### raw CK handle

直接借到的原始 Virtools 对象句柄，例如：

```text
CKObject@
CK3dEntity@
CKDataArray@
```

适合短期操作。
不要把它当作长期稳定引用。

### Borrow

教程里很多 API 名字带 `Borrow`。

可以先理解为：

```text
临时借用一个运行时对象
用完就放下
下次需要再借
```

这类句柄不要跨关卡、跨长时间保存。

## Ballance 关卡名词

### Checkpoint

检查点。

关卡 NMO 里通常由 `PC_Checkpoints` 组提供作者标记。
运行时会进入 `Checkpoints` 表，并由 Gameplay 接管显示和切换。

### Resetpoint

复活点。

关卡 NMO 里通常由 `PR_Resetpoints` 组提供作者标记。
运行时会写入 `ResetPoints` 和 `CurrentLevel.CurrentResetpoint`。

### Sector / 小节

原版对象名和列名里常见 `Sector`。

正文里按“小节”理解。

相关名字：

```text
Sector_01
Activate Sector
Deactivate Sector
Gameplay_SectorManager
```

### Placeholder / PH

Levelinit 使用的占位符体系。

正文里常见：

```text
PH_Groups
PH
```

它用于把关卡作者标记整理成运行时可用的数据和对象关系。

### Extra Point

加分道具。

关卡 NMO 里常见组名：

```text
P_Extra_Point
```

脚本里收到 `GAME_EVENT_EXTRA_POINT` 时，可以重新观察分数相关状态。

### Level End

关卡结束标记。

关卡 NMO 里常见组名：

```text
PE_Levelende
```

它是作者放在关卡里的入口，完整过关流程还要经过运行时逻辑。

## 物理

### Physicalize

物理化。

`Physicalize` 会根据摩擦、弹性、质量、碰撞组、碰撞形状等参数，为 3D 对象创建物理对象。

### `Physicalize_Balls`

球形物理参数表。

正文里读到：

```text
P_Ball_Wood
P_Ball_Stone
```

它说明木球和石球的差异包含摩擦、弹性、质量和半径。

### Impulse

瞬时冲量。

可以理解成“给物体来一下”。

适合描述撞击、弹开、一次性推力。

### Force

持续力。

它会持续作用，直到关闭。
使用持续力时，要知道什么时候清掉。

### WakeUp

唤醒物理对象。

物理对象可能进入休眠状态。
WakeUp 让它重新参与仿真。

## 运行状态表

### `CurrentLevel`

当前关卡状态表。

正文里读过：

```text
Level ID
ActiveBall
CurrentResetpoint
Activation Phase?
Points
```

写它属于高风险操作。

### `AllLevel`

关卡配置表。

正文里用它观察：

```text
Levelfile
StartBall
StartResetpoint
Sky
Music
```

### `PH_Groups`

Levelinit 用来整理作者标记的表。

常见列：

```text
Group Names
Amount
Activation
Reset
```

写它属于高风险操作。

### `IngameParameter`

Gameplay 运行中的状态表之一。

正文里观察过：

```text
Activate Sector
Deactivate Sector
Tutorial activ?
RollSound activate?
```

写它属于高风险操作。

### `Energy`

分数、生命和奖励相关状态表。

正文里观察过：

```text
Points
Lifes
StartPoints
StartLifes
Timefactor
LifeBonus
```

## 风险词

### 低风险

通常只影响脚本自己，或者查看原版状态。

例子：

```text
日志
UI
命令
配置
Timer
查找对象
打印 DataArray
```

### 中风险

开始碰到游戏对象，但范围可控。

例子：

```text
创建自己的 debug marker
临时显示或隐藏非关键对象
关卡加载后重新查对象
```

### 高风险

会改变原版运行时状态或流程。

例子：

```text
写 CurrentLevel
写 IngameParameter
改 PH_Groups
移动当前活动球
修改 Gameplay 行为图
操作持续 Force
```

高风险操作要先写检查清单和回滚方式。
