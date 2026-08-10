# 02 第一个 Mod

上一节装好了 BML 和编辑器。现在写一个最小的脚本 mod，让 BML 找到它、编译它、执行它。

本节要看到的结果：游戏内出现一条问候语，`ModLoader/ModLoader.log` 里也出现
脚本写出的一行日志。这说明 BML 已经找到文件、编译脚本、创建 Mod 对象，并调用了
`OnLoad`。

## 创建脚本文件

在 `ModLoader/Mods/` 下新建一个文件，命名为 `HelloMod.mod.as`：

```angelscript
[bml.mod id="hello.script" name="Hello Mod" version="1.0.0" author="Tutorial" bml="0.3.13" description="Minimal tutorial script mod"]
class HelloMod {
    void OnLoad(const BML::ModContext &in ctx) {
        ctx.LogInfo("HelloMod loaded from ModLoader/Mods/HelloMod.mod.as");
        BML::UI::AddMessage("Hello from your first script mod!");
    }
}
```

保存后启动游戏。如果 Player 已经在运行，新增的 Mod 入口仍需要重启一次；BML 只在
启动时发现新 Mod。

## 日志结果

打开 `ModLoader/ModLoader.log`，在文件末尾附近找这几行：

```text
[ModLoader/INFO]: Loading Mod hello.script[Hello Mod] v1.0.0 by Tutorial
[hello.script/INFO]: HelloMod loaded from ModLoader/Mods/HelloMod.mod.as
[ModLoader/INFO]: BML script mod summary: loaded=1 failed=0
```

各行的含义：

| 日志 | 含义 |
| --- | --- |
| `Loading Mod hello.script[Hello Mod] v1.0.0 by Tutorial` | BML 读到了脚本文件，解析出了元数据 |
| `HelloMod loaded from ModLoader/Mods/HelloMod.mod.as` | 脚本的 `OnLoad` 回调成功执行了 |
| `loaded=1 failed=0` | 本次加载 1 个脚本 mod，0 个失败 |

如果能看到游戏内问候语和第二行日志，第一个脚本 Mod 就已经跑通了。

## 逐行解释

### 元数据行

```angelscript
[bml.mod id="hello.script" name="Hello Mod" version="1.0.0" author="Tutorial" bml="0.3.13" description="Minimal tutorial script mod"]
```

这是附加到入口类上的 AngelScript 元数据。`bml.mod` 声明 Mod 身份和版本要求，
必须紧邻它所描述的类。参数使用 `key="value"` 形式。

各字段的意义：

| 字段 | 说明 |
| --- | --- |
| `id` | mod 的唯一标识符。BML 用它区分不同 mod。日志前缀也用这个名字 |
| `name` | 显示名称。出现在 BML 的 mod 列表和日志中 |
| `version` | 版本号。格式是 `主.次.补丁` |
| `author` | 作者名 |
| `bml` | 要求的最低 BML 版本。如果用户的 BML 版本低于这个值，加载会被跳过 |
| `description` | 一句话描述 mod 的功能 |

如果你把 `id` 改成 `id="my.first.mod"`，那日志前缀会变成 `[my.first.mod/INFO]`。

### class 声明

```angelscript
class HelloMod {
    ...
}
```

BML 要求每个脚本 Mod 恰好有一个带 `[bml.mod ...]` 的入口类。文件中可以定义
命令、定时器等辅助类，但不能让第二个类也声明 `bml.mod`。BML 会实例化入口类，
并在相应时机调用它的生命周期和事件回调。

类名可以随意取，不需要和文件名相同。但为了好找，建议保持一致。

### OnLoad 回调

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    ...
}
```

`OnLoad` 是 BML 定义的生命周期回调。每次成功装载脚本运行实例时调用一次；
热重载成功后，新实例也会收到一次 `OnLoad`。

参数 `ctx` 的类型是 `BML::ModContext`，它是脚本与 BML 交互的入口。通过 `ctx` 可以获取日志、输入、游戏对象等各种服务。

关于签名里的几个符号：

- `const`  --  表示这个参数是只读的。脚本不能修改 ctx 对象本身。
- `&in`  --  表示参数以引用方式传入（避免拷贝），并且是输入参数。这是 AngelScript 的参数传递语法，`&in` 表示"引用传入"，`&out` 表示"引用传出"。
- 合起来 `const BML::ModContext &in ctx` 的意思是：接收一个 `BML::ModContext` 的只读引用，叫做 `ctx`。

现在不需要完全理解这些修饰符的底层机制，先按这个格式写。第 5 章讲命令回调时会再次遇到函数签名，第 6 章会完整整理回调规则。

### 记录日志和显示消息

```angelscript
ctx.LogInfo("HelloMod loaded from ModLoader/Mods/HelloMod.mod.as");
BML::UI::AddMessage("Hello from your first script mod!");
```

`ctx.LogInfo(...)` 把消息写入 `ModLoader/ModLoader.log`，并自动使用当前 Mod 的
日志前缀。对应的警告和错误方法是 `LogWarn`、`LogError`。普通日志不需要获取
Logger 句柄。

`BML::UI::AddMessage(...)` 在游戏画面中显示短消息，让第一次成功不必只靠翻日志
确认。下一章会继续解释日志、游戏内消息和输入。

## BML 的加载流程

把上面的内容串起来，BML 对一个脚本 mod 的处理流程是：

```text
1. 扫描 ModLoader/Mods/ 目录
2. 找到 .mod.as 文件
3. 用 CKAS 编译脚本
4. 读取入口类的 [bml.mod ...] 元数据
5. 检查 bml 版本和依赖要求
6. 创建入口类对象
7. 调用 OnLoad(ctx)
```

如果第 3-5 步任何一步失败，日志里会有对应的错误信息，而 `OnLoad` 不会被调用。

## 试着改一下

理解了每个部分的作用后，可以尝试以下修改：

1. **改日志和问候语**：修改两个字符串并保存。已经加载的单文件 Mod 会自动热
   重载，不需要重启 Player。
2. **改 id**：把 `id="hello.script"` 改成 `id="my.test"`。身份变化不能热重载，
   需要重启后观察日志前缀是否变成 `[my.test/INFO]`。
3. **故意写错 OnLoad 签名**：比如写成 `void onLoad(...)` 或
   `void OnLoad(BML::ModContext ctx)`，观察 `script diag <id>` 和日志中的错误，随后
   恢复正确签名。

## 加载失败时的排查

| 现象 | 检查 |
| --- | --- |
| 日志里完全没有 `Loading Mod hello.script` | 文件是否在 `ModLoader/Mods/` 下；文件名后缀是否是 `.mod.as`；是否多了 `.txt` |
| 有 `Loading Mod` 但没有问候语或 `HelloMod loaded` | 先运行 `script diag hello.script`，再检查第一条 compile、metadata 或 callback 错误 |
| `loaded=0` 或 `failed` 不为 0 | 向上翻日志找第一条错误行 |
| 改了脚本但日志还是旧内容 | 目录和单文件 mod 默认会自动热重载；先执行 `script status` 检查 watcher，再用 `script reload hello.script` 手动重载 |
| 编辑器里有红色下划线但游戏能跑 | Language Server 和 BML 的检查独立运行，以游戏日志为准 |

## 完成状态

`ModLoader/Mods/HelloMod.mod.as` 能被 BML 加载并执行 `OnLoad`，游戏画面和日志中都
能看到脚本输出的消息；普通源码修改可以通过热重载验证。

-> 下一节：[03 输出与输入](tutorial-03-output-input.md)
