# 06 回调模型

前面几章写的代码已经涉及了多个回调：`OnLoad` 做初始化、`OnProcess` 每帧画窗口、命令回调响应输入。但我们一直在用它们，还没有停下来解释：为什么脚本被分成零散的函数，由 BML 在特定时机调用？

本章回答这个问题，给出完整的回调清单、调用时机、以及常见错误。

---

## 脚本由 BML 调用

很多小程序是从第一行一直执行到最后一行，跑完就结束。BML 脚本不是这种模型。它更像一组挂在游戏里的响应函数：游戏加载脚本后，脚本对象一直存在；游戏运行到某个时机，BML 调用对应函数。

例如加载脚本时调用 `OnLoad`，每帧调用 `OnProcess`，退出时调用 `OnUnload`。命令和 Timer 也是同一个思路：你先把函数交给 BML，等玩家输入命令或时间到了，BML 再调用它。

这些由宿主程序在特定时机调用的函数叫回调（callback）。你的脚本声明好函数，BML 在合适的时机来调。

---

## 两类回调

### 固定回调

BML 靠函数名匹配。函数名、返回值、参数类型全部对上，BML 才会认。拼错名字或者参数类型不对，回调不会被调用，日志里通常会出现签名不匹配的提示。

本章覆盖五个固定回调：`OnLoad`、`OnProcess`、`OnUnload`、`OnGameEvent`、`OnRender`。

### 注册回调

函数名可以自由取，注册时把函数交给 BML。前面的命令回调就是注册回调，`RegisterCommand` 注册，玩家输入命令时 BML 才调用。

下一章还会见到 Timer 回调（`SetTimeout` / `SetInterval` 注册，时间到了 BML 调用）。

---

## 时序总览

先给出一帧循环的全貌，后面逐个展开：

```text
游戏启动
  └─ BML 加载脚本
       └─ OnLoad <- 执行一次

游戏运行（每帧循环）
  ┌─ OnProcess <- 逻辑更新
  │     检查按键、更新数据、提交 ImGui
  ├─ OnRender <- Virtools 渲染阶段
  │     处理 BML::UI 或底层渲染相关操作
  └─ 回到下一帧

游戏事件发生（关卡开始、重生等）
  └─ OnGameEvent <- 事件触发时

游戏退出
  └─ OnUnload <- 执行一次
```

---

## OnLoad

```angelscript
void OnLoad(const BML::ModContext &in ctx)
```

**触发时机**：脚本被 BML 加载后，执行且仅执行一次。

**适合做的事**：

- 注册命令
- 读取配置
- 启动 Timer
- 写一条启动日志

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    LoadConfig(ctx);
    RegisterCommands(ctx);
    LogInfo(ctx, "MyMod loaded");
}
```

**不要做的事**：

- 画 ImGui 窗口（此时渲染循环还没开始）
- 访问关卡对象（玩家可能还在主菜单，关卡没有加载）
- 把 `ctx` 存到成员变量里长期持有（BML 不保证跨回调有效性）

---

## OnProcess

```angelscript
void OnProcess(const BML::ModContext &in ctx)
```

**触发时机**：游戏运行时，每一帧调用一次。如果游戏跑 60 fps，这个函数每秒被调 60 次。

**适合做的事**：

- 检查输入
- 更新数据
- 提交 ImGui 窗口

```angelscript
void OnProcess(const BML::ModContext &in ctx) {
    HandleWindowToggle(ctx);
    UpdateFps();
    DrawWindow();
}
```

**不要做的事**：

- 每帧写日志（日志文件会迅速膨胀到几百 MB）
- 在里面注册命令或读配置（那是一次性动作，放 `OnLoad`）
- 做耗时运算（搜索文件、解析大数据）。每帧有 16ms 预算（60fps），超了就会卡顿

### 为什么 OnProcess 和 ImGui 的关系如此紧密

第 04 章提到：ImGui 是即时模式 UI，每帧提交才能持续显示。而 `OnProcess` 恰好就是每帧调用的函数。所以你在 `OnProcess` 里调用 `ImGui::Begin`/`End`，窗口就每帧都在；这一帧不调用，窗口就消失。

### 成员变量在帧之间活着

`OnProcess` 结束后，脚本类的成员变量不会销毁。下一帧进来时，`showWindow`、`fps` 这些值和上一帧结束时一样。这就是成员变量能充当「状态」的原因，在第 04 章里 `showWindow` 记住窗口开关，`fps` 记住帧率。

---

## OnUnload

```angelscript
void OnUnload(const BML::ModContext &in ctx)
```

**触发时机**：脚本被 BML 卸载时，执行且仅执行一次。通常发生在游戏退出时。

**适合做的事**：

- 取消还在运行的 Timer
- 写一条卸载日志

```angelscript
void OnUnload(const BML::ModContext &in ctx) {
    if (statusTimer !is null && statusTimer.IsValid) {
        statusTimer.Cancel();
    }
    LogInfo(ctx, "MyMod unloaded");
}
```

**不要做的事**：

- 画 ImGui 窗口（渲染循环已经结束或即将结束）
- 注册命令（太晚了，命令不会被使用）
- 访问关卡对象（可能已经被销毁）

---

## OnGameEvent

```angelscript
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event)
```

**触发时机**：游戏状态发生变化时，关卡开始、关卡结束、玩家重生等。

注意参数 `event` 是按值传递的，没有 `const &in`。这和其他回调不同。

**适合做的事**：

- 关卡开始时初始化关卡数据
- 重生时重置计时器或统计
- 关卡结束时汇总信息

```angelscript
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
    // 根据 event 判断发生了什么，再执行对应逻辑
    LogInfo(ctx, "Game event received");
}
```

**不要做的事**：

- 不判断事件类型就执行逻辑（你会在关卡结束时也执行关卡开始的代码）
- 假设此时一定有关卡对象（某些事件类型在关卡卸载后触发）

---

## OnRender

```angelscript
void OnRender(const BML::ModContext &in ctx, const BML::RenderEvent &in event)
```

**触发时机**：每帧的 Virtools 渲染阶段，在 `OnProcess` 之后。

**和 OnProcess 的区别**：`OnProcess` 是脚本每帧逻辑阶段，前面教程里的 ImGui 窗口都放在这里提交。`OnRender` 对接的是 Virtools 的渲染窗口期，适合处理 BML::UI 或底层渲染相关操作；一般脚本不需要用它。

**不要做的事**：

- 把前面章节的 ImGui 窗口代码搬到这里
- 在这里做逻辑计算（逻辑放 `OnProcess`，职责分离）

---

## ctx：服务入口

每个回调都收到 `const BML::ModContext &in ctx`。它是当前脚本的服务入口，通过它借日志、借输入管理器、注册命令。

几个要点：

1. **不要存起来**。BML 每次调用回调时传入 `ctx`，保证这一次调用期间有效。不要把它保存到成员变量留着下次用。
2. **每个回调都有**。无论 `OnLoad` 还是 `OnProcess`，拿到的 `ctx` 指向同一个 mod 的服务。
3. **往下传就行**。需要用 `ctx` 的 helper 函数，把它当参数传进去即可（前面章节的 `LogInfo(ctx, ...)` 就是这个模式）。

### 已经用过的服务

| 写法 | 用途 |
| --- | --- |
| `ctx.BorrowLogger()` | 日志 |
| `ctx.BorrowInputManager()` | 输入 |
| `ctx.SendIngameMessage(...)` | 游戏内消息 |
| `ctx.RegisterCommand(...)` | 注册命令 |

### 下一章将用到的服务

| 写法 | 用途 |
| --- | --- |
| `ctx.BorrowConfig()` | 配置 |
| `ctx.SetTimeout(...)` | 一次性 Timer |
| `ctx.SetInterval(...)` | 循环 Timer |

---

## 回调签名必须精确匹配

BML 通过函数名和完整签名来匹配固定回调。如果你的函数签名写错了，BML 会跳过这个函数；排查时先看 `ModLoader/ModLoader.log` 里的签名提示。

常见的签名错误：

| 错误写法 | 问题 |
| --- | --- |
| `void onLoad(...)` | 小写 `o`，BML 不认识 |
| `void OnLoad(BML::ModContext &in ctx)` | 少了 `const` |
| `int OnProcess(...)` | 返回值不是 `void` |
| `void OnGameEvent(const BML::ModContext &in ctx, const BML::GameEvent &in event)` | `event` 不是 `const &in`，是按值传 |

如果一个回调看起来永远不被调用，首先检查签名是否和本章列出的完全一致。

---

## 故障诊断

### OnLoad 没有执行

- 脚本加载失败。检查日志里有没有语法错误
- 函数签名不对。和上面的签名表逐字对照
- 脚本文件没有放在 `ModLoader/Mods/` 目录下

### OnProcess 没有执行

- `OnLoad` 里有异常导致脚本被卸载。先确认 `OnLoad` 的日志出现
- 脚本在加载阶段就失败了。看 BML 日志里有没有报错

### OnGameEvent 没有收到事件

- 签名错误（最常见：给 `event` 加了 `const &in`）
- 当前没有进入关卡（主菜单里不会有关卡事件）

### ImGui 窗口不显示

- `OnProcess` 没有被调用（先解决上面的问题）
- `Begin`/`End` 不配对（即使窗口折叠也必须调 `End`）
- `showWindow` 初始值设成了 `false`

---

## 本章如何连接前后

回顾前面的章节：

- 第 02 章的 `OnLoad` 里写启动日志，现在知道这是「脚本加载后执行一次」
- 第 04 章在 `OnProcess` 里画窗口，现在知道这是「每帧调用」，所以窗口每帧存在
- 第 05 章在 `OnLoad` 里注册命令，现在知道注册只做一次，执行靠另一个注册回调

下一章会引入 Config 和 Timer。Config 在 `OnLoad` 读取一次，Timer 回调是又一种注册回调，BML 在时间到了之后才调用你给它的函数。

---

## 完成状态

理解了回调模型后，前面写过的窗口、输入、命令都能串起来：`OnLoad` 做一次初始化，`OnProcess` 每帧更新，`OnUnload` 清理退出，`OnGameEvent` 响应关卡变化，`OnRender` 对接 Virtools 渲染阶段。固定回调靠名字匹配，注册回调靠 `RegisterCommand` / `SetTimeout` / `SetInterval` 交出函数引用。

下一步：[07 配置与 Timer](tutorial-07-config-timer.md)
