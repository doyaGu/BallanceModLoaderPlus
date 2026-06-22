# 参考 A：BML 生命周期总览

基础模板已经写过 `OnLoad` 和 `OnProcess`。

本参考篇把它们放回 BML 的运行框架里看：

```text
Player 启动
  BML 扫描脚本文件
    创建脚本对象
      调用 OnLoad
        每帧调用 OnProcess
      退出或卸载时调用 OnUnload
```

这章先只看三个固定回调：

```text
OnLoad       启动初始化
OnProcess    每帧更新
OnUnload     卸载清理
```

游戏事件由参考 B 单独说明。

## 用一个小脚本观察

新建一个小脚本：

```text
ModLoader/Mods/LifeMod.mod.as
```

写入：

```angelscript
[bml.mod id="life.script" name="Lifecycle Demo" version="1.0.0" author="Tutorial" bml="0.3.12" description="Observe BML script lifecycle"]
class LifeMod {
    // 成员变量跟着 LifeMod 对象一起存在。
    private bool firstProcess = true;
    private int processFrames = 0;

    void OnLoad(const BML::ModContext &in ctx) {
        LogInfo(ctx, "LifeMod OnLoad");
        ctx.SendIngameMessage("LifeMod loaded.");
    }

    void OnProcess(const BML::ModContext &in ctx) {
        processFrames++;

        if (firstProcess) {
            firstProcess = false;
            LogInfo(ctx, "LifeMod first OnProcess");
        }
    }

    void OnUnload(const BML::ModContext &in ctx) {
        LogInfo(ctx, "LifeMod OnUnload frames=" + processFrames);
    }

    private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(message);
        }
    }
}
```

这份脚本不画窗口，也不注册命令。它只用日志观察 BML 调用顺序。

## 运行后看日志

保存脚本，重启 Player。

启动后先看到：

```text
[BML/INFO]: Loading Mod life.script[Lifecycle Demo] v1.0.0 by Tutorial
[life.script/INFO]: LifeMod OnLoad
[BML/INFO]: LifeMod loaded.
[ModLoader/INFO]: BML script mod summary: loaded=4 failed=0
```

紧接着，第一帧 `OnProcess` 执行：

```text
[life.script/INFO]: LifeMod first OnProcess
```

退出 Player 时，脚本卸载：

```text
[life.script/INFO]: LifeMod OnUnload frames=1151
```

`frames` 后面的数字不固定。Player 多运行一会儿，这个数字就更大。

## `OnLoad` 适合放什么

`OnLoad` 在脚本加载成功后调用一次。

适合放：

```text
读取配置
注册命令
启动 Timer
写启动日志
准备脚本自己的成员变量
```

基础模板里就是这样：

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    LoadConfig(ctx);
    RegisterCommands(ctx);
    StartTimers(ctx);
}
```

`OnLoad` 不适合做每帧检查。比如按键、窗口绘制、FPS 更新，应该放在 `OnProcess`。

## `OnProcess` 适合放什么

`OnProcess` 在游戏运行过程中每帧调用。

适合放：

```text
读取这一帧的输入
更新调试窗口显示
绘制 ImGui 窗口
处理短小的每帧状态
```

基础模板里是：

```angelscript
void OnProcess(const BML::ModContext &in ctx) {
    HandleWindowToggle(ctx);
    DrawWindow();
}
```

`OnProcess` 执行频率很高。不要在里面每帧写日志：

```angelscript
void OnProcess(const BML::ModContext &in ctx) {
    LogInfo(ctx, "frame");
}
```

这样会快速刷满日志。

需要确认 `OnProcess` 是否开始执行时，用一次性标记：

```angelscript
private bool firstProcess = true;

void OnProcess(const BML::ModContext &in ctx) {
    if (firstProcess) {
        firstProcess = false;
        LogInfo(ctx, "first OnProcess");
    }
}
```

## `OnUnload` 适合放什么

`OnUnload` 在脚本卸载时调用。

适合放：

```text
写卸载日志
取消仍然不需要继续的 Timer
注销外部资源
把调试状态收尾
```

脚本注册的命令和 Timer 属于 BML 管理的脚本资源。脚本卸载时，BML 会处理这些资源。
写 `OnUnload` 的意义主要是把自己额外维护的状态收干净，也方便从日志确认脚本完整退出。

一个常见写法：

```angelscript
void OnUnload(const BML::ModContext &in ctx) {
    if (startupTimer !is null && startupTimer.IsValid) {
        startupTimer.Cancel();
    }

    LogInfo(ctx, "unloaded");
}
```

如果脚本没有额外清理工作，可以不写 `OnUnload`。

## 成员变量什么时候重置

成员变量属于脚本对象。

例如：

```angelscript
private int processFrames = 0;
```

它不会每帧重新变成 `0`。`OnProcess` 每执行一次：

```angelscript
processFrames++;
```

它就会继续累加。

它会在这些时候重新开始：

```text
Player 重启
脚本重新加载
脚本对象重新创建
```

这也是修改 `.as` 后要重启 Player 的原因之一。旧的脚本对象已经创建出来了，旧代码还在运行。

## 三个位置不要混

可以按这个规则放代码：

| 位置 | 问题 |
| --- | --- |
| `OnLoad` | 这件事是不是启动时做一次就够 |
| `OnProcess` | 这件事是不是每帧都要检查或绘制 |
| `OnUnload` | 这件事是不是脚本结束时需要收尾 |

例子：

| 任务 | 放哪里 |
| --- | --- |
| 注册 `base` 命令 | `OnLoad` |
| 读取 `ShowMessage` 配置 | `OnLoad` |
| 检查 `F9` 是否按下 | `OnProcess` |
| 绘制 ImGui 窗口 | `OnProcess` |
| 取消还没触发的 Timer | `OnUnload` |


## 速查结论

可将基础模板的代码放进更清晰的位置：

```text
OnLoad
  读取配置
  注册命令
  启动 Timer

OnProcess
  读取输入
  绘制 ImGui

OnUnload
  清理脚本自己的状态
```
