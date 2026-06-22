# 第 30 章：修改前风险分级

从这里开始，教程进入“受控修改”。前面已经看过对象、组、DataArray、行为图、物理事件和脚本 API。现在要先停一下：同样是几行脚本，风险差别很大。

风险不按代码长短判断。要看它碰到哪一层、什么时候执行、失败后能不能恢复。

## 三个问题

动手前先写三句话：

```text
我要改什么？
它属于哪一层？
失败后怎么恢复？
```

如果这三句话写不出来，先做观察工具，不做修改。

## 四个判断轴

可以用四个轴判断风险：

| 判断轴 | 问题 |
| --- | --- |
| 目标 | 改脚本自己的状态，还是改原版运行时状态 |
| 时机 | 在菜单、进关、运行中、复活、重开、过关哪个阶段执行 |
| 可恢复性 | 能不能关闭、清理、写回原值 |
| 覆盖范围 | 只影响一个可控对象，还是影响当前关卡甚至所有关卡 |

风险高低常常来自组合。

例如“显示一个 ImGui 窗口”是低风险；“每帧移动当前球”是高风险。两者都可以写成很短的代码，但后者碰到了 Gameplay 和物理系统正在管理的对象。

## 低风险

低风险通常只影响脚本自己，或者只读取原版状态：

```text
打日志
显示 ImGui 调试窗口
显示 FPS
注册命令
读取配置
Timer 定时输出
查找对象并打印名字
打印组数量
读取 DataArray 并显示结果
```

低风险也要写检查：

```text
对象找不到时安全返回
关卡未加载时不访问关卡对象
表不存在时写清楚日志
窗口或命令能关闭
```

低风险适合用来练手，因为失败后大多只影响脚本自己。

## 中风险

中风险开始碰到游戏对象，但范围仍可控制：

```text
创建自己的调试对象
临时显示或隐藏非关键对象
读取当前球位置
关卡加载后重新查对象
只在明确的练习关卡里操作自己创建的对象
```

中风险重点看生命周期。

对象可能在这些时机失效：

```text
切换关卡
重开当前关
死亡复活
返回菜单
动态对象被原版流程删除
```

中风险至少要准备：

```text
对象为空时怎么处理
关卡切换后是否重新查找
是否避免每帧全局扫描
是否避免长期保存 raw CKObject@
是否能关掉功能
```

第 31 章会用“自己的可控对象”继续讲这一级。

## 高风险

高风险会改变原版运行时逻辑：

```text
写 CurrentLevel
写 IngameParameter
写 PH_Groups
移动当前活动球
对原版对象重复 Physicalize
修改 Gameplay 行为图
修改检查点或复活点流程
改变小节激活流程
持续施加 Force
```

这些操作的共同点：

```text
原版流程也会读写同一批状态
失败后可能影响当前关卡进度
问题可能只在死亡、复活、重开、过关时出现
一次进关通过不能说明稳定
```

高风险功能可以做，但先做低风险观察版本。

例如想改当前球加速，先写：

```text
显示当前球名字
显示当前是否在关卡中
记录 OnPhysicalize
显示准备施加的力
```

这些都稳定后，再进入真正的力或速度修改。

## 把风险判断写进代码

下面这段脚本不修改游戏。它只做一件事：在代码里明确哪些动作允许，哪些动作要等到关卡运行中才允许。

新建：

```text
ModLoader/Mods/RiskGate.mod.as
```

写入：

```angelscript
[bml.mod id="risk.gate" name="Risk Gate" version="1.0.0" author="Tutorial" bml="0.3.12" description="Shows a safe risk gate"]
class RiskGate {
    private bool checkedOnce = false;

    void OnLoad(const BML::ModContext &in ctx) {
        LogInfo(ctx, "Risk Gate loaded");
        PrintState(ctx, "OnLoad");
    }

    void OnProcess(const BML::ModContext &in ctx) {
        if (checkedOnce) {
            return;
        }
        checkedOnce = true;

        PrintState(ctx, "OnProcess");

        // 只写日志、UI 这类动作，不依赖关卡对象。
        LogInfo(ctx, "ui/log action=allowed");

        // 触碰关卡对象前，先通过守门函数判断时机。
        LogInfo(ctx, "level object action=" + GateText(CanTouchLevelObject(ctx)));
    }

    private bool CanTouchLevelObject(const BML::ModContext &in ctx) {
        return ctx.GetIsInLevel() && ctx.GetIsPlaying();
    }

    private void PrintState(const BML::ModContext &in ctx, const string &in source) {
        LogInfo(ctx, source +
                     " inGame=" + BoolText(ctx.GetIsInGame()) +
                     " inLevel=" + BoolText(ctx.GetIsInLevel()) +
                     " playing=" + BoolText(ctx.GetIsPlaying()) +
                     " paused=" + BoolText(ctx.GetIsPaused()));
    }

    private string GateText(bool allowed) {
        return allowed ? "allowed" : "blocked";
    }

    private string BoolText(bool value) {
        return value ? "true" : "false";
    }

    private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(message);
        }
    }
}
```

正常结果：

```text
Loading Mod risk.gate[Risk Gate] v1.0.0 by Tutorial
Risk Gate loaded
OnLoad inGame=false inLevel=false playing=false paused=false
BML script mod summary: loaded=1 failed=0
OnProcess inGame=false inLevel=false playing=false paused=false
ui/log action=allowed
level object action=blocked
```

这个脚本没有做任何危险操作，但它已经有了修改脚本该有的结构：

```text
先看当前游戏状态
低风险动作直接允许
关卡对象动作先过守门函数
不满足条件就不执行
```

以后要写中风险代码时，不要把对象操作直接塞进 `OnProcess`。先写一个 `Can...` 函数，把时机条件说清楚。

## 常见想法分级

| 想法 | 分级 | 原因 |
| --- | --- | --- |
| 显示 FPS 和当前关卡状态 | 低 | UI 和只读状态 |
| 打印 `PC_Checkpoints` 数量 | 低 | 只读组信息 |
| 读取 `CurrentLevel` 并显示 | 低 | 只读 DataArray |
| 给屏幕加一个调试窗口开关 | 低 | 脚本自己的状态 |
| 创建自己的 marker | 中 | 新建对象，要处理生命周期 |
| 暂时隐藏一个非关键道具 | 中 | 改对象显示状态 |
| 修改 `CurrentLevel.Points` | 高 | 写原版运行时状态 |
| 移动当前活动球 | 高 | Gameplay 和物理都在控制 |
| 给当前球持续 `SetForce` | 高 | 需要清理物理效果 |
| 改检查点行为图 | 高 | 影响复活、显示和消息流程 |
| 改小节激活流程 | 高 | 影响关卡推进和对象切换 |

分级时不要只看“代码能跑”。能跑只是第一步。

## 修改前检查清单

### 对象

```text
对象存在
名字符合预期
class id 符合预期
它属于当前场景
关卡切换后会重新获取
```

需要保存对象身份时，优先保存名字、id 或 CKAS 引用层。raw `CKObject@` 只在靠近使用点时借用。

### DataArray

读表前确认：

```text
表存在
行数符合预期
列数符合预期
列名存在
值类型符合预期
```

写表前再确认：

```text
什么时候写
原版行为图会不会覆盖
写错后怎么恢复
关卡重启后是否需要重新写
是否只影响当前关卡
```

只要写 `CurrentLevel`、`IngameParameter`、`PH_Groups`，就按高风险处理。

### 行为图

看行为图时确认：

```text
图名
所在父图
输入和输出
DataArray 读写
消息发送和等待
执行线
参数线
```

改行为图前再确认：

```text
是否只改最小局部
输入和输出激活是否理解
参数从哪里来
原来的线语义是否保留
关卡重启和复活后是否仍然正常
```

行为图修改容易出现“刚进关正常，复活后异常”的问题，因为很多流程只在特定事件发生时执行。

### 物理

动物理前确认：

```text
目标已经物理化
目标仍在仿真中
用的是 Impulse 还是 Force
Force 什么时候清掉
关卡重置时是否清理
变球后是否仍然正确
```

`SetPosition` 不能替代物理移动。当前球和很多机关都由原版流程控制，直接改位置通常会和原版逻辑抢状态。

### 验证路径

一次进关验证不够。

至少按修改类型选几条路径：

```text
进入关卡
重开当前关
死亡复活
吃分或触发机关
过关
返回菜单再进入
暂停和恢复
```

如果功能影响所有关卡，还要换关卡验证。

## 回滚方式

每个中高风险修改都要有回滚方式。

常见做法：

```text
配置开关关闭功能
命令手动关闭功能
关卡切换时清理状态
脚本卸载时清理状态
保留原值并能写回
保留原文件备份
```

只要功能会改变游戏状态，就要能关掉。只能靠重启游戏恢复的功能，不适合作为第一个正式 mod。

## 小结

修改前先分级：

```text
低风险：日志、UI、命令、读取状态。
中风险：自己的对象、可控显示隐藏、关卡内临时状态。
高风险：原版 DataArray、Gameplay、当前球、物理和检查点流程。
```

第 31 章会从中风险里选一个可控目标：创建自己的可控对象。先练会“创建、使用、清理”，再碰原版核心对象。


