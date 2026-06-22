# 参考 K：CKAS 三种脚本入口

前面一直使用 BML 脚本 mod。进入 CKAS 扩展能力以前，要先把脚本入口分清楚。

同样是 `.as` 文件，可能由不同宿主加载。宿主不同，回调函数就不同，拿到的 `ctx` 也不同。

```text
BML 脚本 mod
  BMLPlus 加载
  CKAS 编译和执行 AngelScript
  回调里拿到 BML::ModContext

CKAS runtime script
  CKAS runtime manager 加载
  回调里拿到 ScriptContext

AngelScript Component
  Virtools 行为图里的 AngelScript Component BB 创建
  回调里拿到 CKBehaviorContext
```

这一章只讲入口形状。后面继续以 BML 脚本 mod 为主，再逐步使用 CKAS 的 Scene、Behavior、BB、Param、Message、Async 等能力。

## 为什么要分入口

很多脚本错误看起来像语法问题，根源常常是入口用错。

例如看到一段代码里有：

```angelscript
void Update(const ScriptContext &in ctx)
```

这段代码属于 CKAS runtime script。把它直接复制到 BML 脚本 mod 的类里，BML 不会把它当成固定回调调用。

再看一段：

```angelscript
void Update(const CKBehaviorContext &in ctx)
```

这段代码属于 AngelScript Component。它依赖行为图里的组件实例。

BML 脚本 mod 使用的是 BML 固定回调，例如：

```angelscript
void OnLoad(const BML::ModContext &in ctx)
void OnProcess(const BML::ModContext &in ctx)
void OnModifyConfig(const BML::ModContext &in ctx, const BML::ConfigEvent &in event)
```

判断一段脚本能不能放进当前文件，先看三件事：

1. 它写在哪种入口里。
2. 它的回调函数叫什么。
3. 它的 `ctx` 是哪种类型。

## 三种入口对照

| 入口 | 谁加载 | 常见放置位置 | 入口声明 | 上下文类型 | 适合做什么 |
| --- | --- | --- | --- | --- | --- |
| BML 脚本 mod | BMLPlus | `ModLoader/Mods/*.mod.as` | `[bml.mod ...]` 标记一个类 | `BML::ModContext` | mod 身份、日志、配置、命令、Timer、BML UI、DataShare、发布给玩家使用的脚本 mod |
| CKAS runtime script | CKAS runtime manager | CKAS 的 `Scripts` 根目录 | `[script ... entry="..."]` | `ScriptContext` | 独立脚本服务、脚本消息、异步任务、没有 BML mod 身份的场景逻辑 |
| AngelScript Component | Virtools 行为图 | 行为图里的 `AngelScript Component` BB | 一个脚本类，类名填进 BB 参数 | `CKBehaviorContext` | 挂在某个行为图实例上的逻辑、组件参数、跟随行为图执行的脚本 |

这三种入口都使用 AngelScript 语法，也都由 CKAS 提供脚本运行能力。区别在于“谁创建脚本对象、谁调用回调、回调给什么上下文”。

## BML 脚本 mod 的形状

前面章节已经反复使用这种形状：

```angelscript
[bml.mod id="entry.demo" name="Entry Demo" version="1.0.0" author="Tutorial" bml="0.3.12" description="Show BML script mod entry"]
class EntryDemo {
    void OnLoad(const BML::ModContext &in ctx) {
        BML::Logger@ log = ctx.BorrowLogger();
        log.Info("BML script mod loaded");
    }

    void OnProcess(const BML::ModContext &in ctx) {
        // 每帧调用。适合更新状态、处理输入、驱动简单的调试逻辑。
    }
}
```

这里有两个关键点。

第一，`[bml.mod ...]` 是 BML 的元数据。它写在 AngelScript 源码里，但含义由 BMLPlus 读取，不属于 AngelScript 语言本身的关键字。

第二，BML 会创建 `EntryDemo` 的实例，然后按固定时机调用 `OnLoad`、`OnProcess` 等函数。函数签名不对，BML 就不会把它当成对应回调。

BML 脚本 mod 最适合做教程主线，因为它天然有 mod 身份：

- 日志能按 mod 分类。
- 命令、配置、Timer、DataShare 都由 BML 管。
- 玩家安装时只需要把脚本放到 mod 目录。
- 后面需要进入 Virtools 世界时，可以从 `BML::ModContext` 借到 CKAS / Virtools 入口。

例如：

```angelscript
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
    if (event != BML::GAME_EVENT_POST_START_MENU) {
        return;
    }

    CKContext@ ck = ctx.BorrowCKContext();
    if (ck is null) {
        return;
    }

    // 只有 CKAS 函数声明接受 CKContext@ 时，才把 ck 传过去。
}
```

`BorrowCKContext()` 不能把 BML 上下文变成 runtime script 上下文。它只借出 Virtools 的 `CKContext@`，供接受 `CKContext@` 的 CKAS / CK 绑定函数使用。

## CKAS runtime script 的形状

CKAS runtime script 有自己的脚本清单。清单使用 `[script ...]` 元数据，指向真正的入口文件。

清单可以写成这样：

```angelscript
[script id="entry.runtime" name="Entry Runtime" version="1.0.0" entry="runtime.as"]
```

`runtime.as` 里写 runtime script 回调：

```angelscript
void OnLoad(const ScriptContext &in ctx) {
    print("Runtime script loaded: " + ctx.Id());
}

void Update(const ScriptContext &in ctx) {
    if (ctx.FrameIndex() == 1) {
        print("Runtime script first update");
    }
}
```

这里没有 `[bml.mod ...]`，也没有 BML 的 `OnProcess`。CKAS runtime manager 会根据 runtime script 的生命周期调用 `OnLoad`、`Update`、`OnMessage` 等函数。

runtime script 常用这些能力：

- `Runtime`：查询和管理 runtime script 状态。
- `Message`：脚本之间发送消息。
- `Async`：做协作式异步任务。
- `Scene`：查找场景对象、组、DataArray 等。

如果目标是写一个给 Ballance 玩家安装的 BML 脚本 mod，通常不从 runtime script 起步。runtime script 更适合 CKAS 自己的脚本服务。

## AngelScript Component 的形状

AngelScript Component 跑在 Virtools 行为图里。行为图里放一个 `AngelScript Component` BB，BB 的参数指定脚本类名，CKAS 创建这个类的实例。

脚本类可以写成这样：

```angelscript
class EntryComponent {
    [param type="float" default="1.0"]
    float Speed;

    void Start(const CKBehaviorContext &in ctx) {
        print("Component started");
    }

    void Update(const CKBehaviorContext &in ctx) {
        // 这里的 ctx 来自行为图执行环境。
        // 组件代码天然贴着某个 BB 实例运行。
    }
}
```

`[param ...]` 是组件参数元数据。CKAS 会把它暴露给组件配置，让行为图实例可以给 `Speed` 这样的字段传值。

Component 适合处理“某个行为图节点自己要做什么”。例如一个模块、机关、触发区域需要在行为图里带一段脚本逻辑，Component 比 BML 脚本 mod 更贴近那个位置。

本教程前半段不使用 Component 当主入口。它需要先理解 Virtools 行为图、BB 实例、参数连接和执行时机。

## 元数据也要分清

`.as` 文件里方括号标记很多，含义由宿主解释。

| 元数据 | 归属 | 用途 |
| --- | --- | --- |
| `[bml.mod ...]` | BML | 声明一个 BML 脚本 mod 的主类 |
| `[bml.require ...]` | BML | 声明 BML mod 依赖 |
| `[bml.optional ...]` | BML | 声明可选 BML mod 依赖 |
| `[bml.export ...]` | BML | 声明 BML 导出服务 |
| `[script ...]` | CKAS runtime | 声明 runtime script 清单 |
| `[script.depends ...]` | CKAS runtime | 声明 runtime script 依赖 |
| `[param ...]` | AngelScript Component | 声明组件参数 |

这些标记都写在 AngelScript 源码里，但不属于 AngelScript 的基础语法。AngelScript 只负责承载这些元数据；BML 或 CKAS 再读取这些元数据。

## 看代码时先问哪种 ctx

后面遇到 CKAS API 时，先看函数声明里的上下文类型。

```angelscript
SomeApi(const BML::ModContext &in ctx)
```

这是 BML 侧 API。

```angelscript
SomeApi(const ScriptContext &in ctx)
```

这是 runtime script 侧 API。

```angelscript
SomeApi(const CKBehaviorContext &in ctx)
```

这是 Component / 行为图侧 API。

```angelscript
SomeApi(CKContext@ context)
```

这是更底层的 Virtools 入口。BML 脚本 mod 里可以考虑用 `ctx.BorrowCKContext()`，拿到以后立刻判空，再传给这个 API。

不要为了让代码编译，把上下文类型硬改成当前手里有的类型。上下文代表宿主提供的运行环境。类型改了，含义也变了。

## 本教程后面的路线

第 29 章之后，教程仍然以 BML 脚本 mod 为主线：

```text
BML 脚本 mod 负责 mod 身份、命令、配置、日志、UI
CKAS API 负责接触 Virtools 对象、行为图、参数和脚本协作能力
必要时再单独讲 runtime script 或 Component
```

这样安排的好处很直接：先写出玩家能安装、能调试、能关闭的 BML 脚本 mod，再慢慢把 CKAS 能力接进来。遇到 CKAS 示例代码时，先确认它属于哪种入口，再决定能不能移到 BML 脚本 mod 里。

本章的三段代码展示的是入口形状，不作为当前 BML 示例脚本直接运行。下一章开始进入 CKAS 的对象引用和生命周期问题。

