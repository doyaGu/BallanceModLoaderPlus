# 02 第一个 Mod

上一节装好了 BML 和编辑器。现在写一个最小的脚本 mod，让 BML 找到它、编译它、执行它。

本节要看到的结果：`ModLoader/ModLoader.log` 里出现脚本写出的一行日志。看到这行日志，就说明 BML 已经找到文件、编译脚本、创建 mod 对象，并调用了 `OnLoad`。

## 创建脚本文件

在 `ModLoader/Mods/` 下新建一个文件，命名为 `HelloMod.mod.as`：

```angelscript
[bml.mod id="hello.script" name="Hello Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Minimal tutorial script mod"]
class HelloMod {
    void OnLoad(const BML::ModContext &in ctx) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info("HelloMod loaded from ModLoader/Mods/HelloMod.mod.as");
        }
    }
}
```

保存后，关闭 Player.exe（如果还开着），再重新启动游戏。

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

如果能看到第二行，第一个脚本 mod 就已经跑通了。

## 逐行解释

### 元数据行

```angelscript
[bml.mod id="hello.script" name="Hello Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Minimal tutorial script mod"]
```

这行不是 AngelScript 语法的一部分。它是 BML 定义的特殊注解，必须放在文件开头（前面不能有代码）。BML 在编译脚本之前先扫描这行，从中提取 mod 的信息。

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

BML 要求每个脚本 mod 有一个入口类。文件里应该只有一个顶层 class（元数据行之后紧跟的那个）。BML 会实例化这个类的一个对象，然后在不同时机调用它上面的方法（回调）。

类名可以随意取，不需要和文件名相同。但为了好找，建议保持一致。

### OnLoad 回调

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    ...
}
```

`OnLoad` 是 BML 定义的生命周期回调。当 BML 加载这个 mod 并准备好运行环境后，会调用一次 `OnLoad`。之后不会再调用第二次（除非游戏重启）。

参数 `ctx` 的类型是 `BML::ModContext`，它是脚本与 BML 交互的入口。通过 `ctx` 可以获取日志、输入、游戏对象等各种服务。

关于签名里的几个符号：

- `const`  --  表示这个参数是只读的。脚本不能修改 ctx 对象本身。
- `&in`  --  表示参数以引用方式传入（避免拷贝），并且是输入参数。这是 AngelScript 的参数传递语法，`&in` 表示"引用传入"，`&out` 表示"引用传出"。
- 合起来 `const BML::ModContext &in ctx` 的意思是：接收一个 `BML::ModContext` 的只读引用，叫做 `ctx`。

现在不需要完全理解这些修饰符的底层机制，先按这个格式写。第 5 章讲命令回调时会再次遇到函数签名，第 6 章会完整整理回调规则。

### Logger 和 @ 符号

```angelscript
BML::Logger@ logger = ctx.BorrowLogger();
if (logger !is null) {
    logger.Info("HelloMod loaded from ModLoader/Mods/HelloMod.mod.as");
}
```

`BML::Logger@`  --  这里的 `@` 表示这是一个"句柄"（handle）。句柄类似于其他语言里的指针或引用，它指向一个对象，但这个对象可能为空。

为什么需要句柄？因为 `ctx.BorrowLogger()` 不一定总能返回有效的 Logger。虽然实际上在 OnLoad 时几乎不会失败，但 API 的设计是"可能为空"，所以返回类型是句柄。

`logger !is null`  --  检查句柄是否为空。`!is null` 是 AngelScript 的写法，含义是"不是空的"。如果不检查就直接用，空句柄会导致脚本运行出错。

`logger.Info(...)`  --  把一条消息写入 `ModLoader/ModLoader.log`，前缀是 `[hello.script/INFO]`。Logger 还有 `Warn` 和 `Error` 级别。

## BML 的加载流程

把上面的内容串起来，BML 对一个脚本 mod 的处理流程是：

```text
1. 扫描 ModLoader/Mods/ 目录
2. 找到 .mod.as 文件
3. 读取文件开头的 [bml.mod ...] 元数据
4. 检查 bml 版本要求是否满足
5. 用 CKAS 编译脚本
6. 找到入口类，创建对象
7. 调用 OnLoad(ctx)
```

如果第 3-5 步任何一步失败，日志里会有对应的错误信息，而 `OnLoad` 不会被调用。

## 试着改一下

理解了每个部分的作用后，可以尝试以下修改，每次改完重启游戏看日志：

1. **改 id**：把 `id="hello.script"` 改成 `id="my.test"`，重启后看日志前缀是否变成 `[my.test/INFO]`
2. **改日志内容**：把 `logger.Info(...)` 里的字符串换成别的，确认日志跟着变
3. **观察 null 检查的作用**：把 `if (logger !is null)` 那层暂时去掉，直接调用 `logger.Info(...)`。在正常情况下这也能工作，因为 OnLoad 时 logger 通常不为空。看完效果后把检查加回去
4. **故意写错 OnLoad 签名**：比如写成 `void onLoad(...)` 或 `void OnLoad(BML::ModContext ctx)`（去掉 const &in），看日志里的错误信息长什么样

## 加载失败时的排查

| 现象 | 检查 |
| --- | --- |
| 日志里完全没有 `Loading Mod hello.script` | 文件是否在 `ModLoader/Mods/` 下；文件名后缀是否是 `.mod.as`；是否多了 `.txt` |
| 有 `Loading Mod` 但没有 `HelloMod loaded` | 编译错误。在 `Loading Mod` 之后的日志行找 error 信息 |
| `loaded=0` 或 `failed` 不为 0 | 向上翻日志找第一条错误行 |
| 改了脚本但日志还是旧内容 | 确认保存了文件；确认关闭了 Player.exe 再重启（脚本只在启动时编译一次） |
| 编辑器里有红色下划线但游戏能跑 | Language Server 和 BML 的检查独立运行，以游戏日志为准 |

## 完成状态

`ModLoader/Mods/HelloMod.mod.as` 能被 BML 加载并执行 `OnLoad`，日志中能看到脚本输出的消息。

-> 下一节：[03 输出与输入](tutorial-03-output-input.md)
