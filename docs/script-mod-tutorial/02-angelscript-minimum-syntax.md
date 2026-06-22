# 第 2 章：AngelScript 最小语法

这一章只为第 3 章服务。

第 3 章会写一个能加载的脚本。现在先把那段脚本拆开看，知道每一行大概在做什么。

本章不要求学完整 AngelScript。只要能读懂最小脚本里的符号：

```text
[bml.mod ...]
class
void OnLoad(...)
ctx
对象句柄 @
null 检查
字符串和日志
```

## 先看完整形状

最小脚本长这样：

```angelscript
[bml.mod id="hello.script" name="Hello Mod" version="1.0.0" author="Tutorial" bml="0.3.12"]
class HelloMod {
    void OnLoad(const BML::ModContext &in ctx) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info("HelloMod loaded");
        }
    }
}
```

现在不用背语法。先看结构：

```text
[bml.mod ...]       这份脚本的 mod 信息
class HelloMod {    脚本主体开始
    OnLoad {        脚本加载后执行这里
        取得日志对象
        如果日志对象存在
            写一行日志
    }
}                   脚本主体结束
```

代码里的大括号 `{` 和 `}` 很重要。它们表示一段代码的开始和结束。

## 分号、缩进和点号

很多语句末尾有分号：

```angelscript
BML::Logger@ logger = ctx.BorrowLogger();
logger.Info("HelloMod loaded");
```

分号 `;` 表示这一句结束。

少写分号，脚本通常会编译失败。日志里会提示某一行附近有语法错误。

缩进不是必须的，但它能让代码层级更清楚：

```angelscript
if (logger !is null) {
    logger.Info("HelloMod loaded");
}
```

这里 `logger.Info(...)` 缩进了一层，表示它在 `if` 的大括号里面。

点号 `.` 表示“使用这个对象上的功能”。

```angelscript
ctx.BorrowLogger()
logger.Info("HelloMod loaded")
```

可以这样读：

```text
ctx.BorrowLogger()       从 ctx 上调用 BorrowLogger
logger.Info(...)         从 logger 上调用 Info
```

## `.as` 文件

AngelScript 源码文件后缀是 `.as`。

BML 脚本 mod 的入口文件后缀是：

```text
.mod.as
```

所以教程前几章使用：

```text
ModLoader/Mods/HelloMod.mod.as
```

文件名后缀不对，BMLPlus 就不会把它当作脚本 mod 入口。

## 注释

以 `//` 开头的内容是注释：

```angelscript
// 这一行是注释，不会执行。
```

注释给人看，不会被当成脚本命令执行。

中文注释本身不会改变脚本逻辑。文件编码正常时，中文注释可以运行。遇到乱码、日志行号异常、编辑器保存不稳定时，把文件保存为 UTF-8；还不放心，就先删掉注释验证。

## `[bml.mod ...]`

这一行不是普通函数，也不是 AngelScript 语言自带的关键字：

```angelscript
[bml.mod id="hello.script" name="Hello Mod" version="1.0.0" author="Tutorial" bml="0.3.12"]
```

AngelScript 允许宿主程序在脚本里使用这种元数据标记。`bml.mod` 这个标记的含义由 BMLPlus/CKAS 定义。

在 BML 脚本 mod 里，它的作用是告诉 BMLPlus：下面这个类是一个脚本 mod。

先看几个字段：

| 字段 | 含义 |
| --- | --- |
| `id` | mod 的唯一名字 |
| `name` | 显示名 |
| `version` | 版本号 |
| `author` | 作者 |
| `bml` | 需要的 BMLPlus 版本 |

这行后面紧跟 `class HelloMod`，表示 `HelloMod` 这个类就是 BML 脚本 mod 的入口。

## `class HelloMod`

这一段表示脚本主体：

```angelscript
class HelloMod {
}
```

`class` 后面是名字。这里叫 `HelloMod`。

BMLPlus 会创建一个 `HelloMod` 对象，然后调用里面的回调函数。

可以先把它看成：

```text
HelloMod 这份脚本
  里面放 OnLoad、OnProcess、命令回调、Timer 回调
```

## 函数

函数是一段有名字的代码。

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
}
```

拆开看：

| 部分 | 先这样看 |
| --- | --- |
| `void` | 这段代码不返回结果 |
| `OnLoad` | 函数名 |
| `( ... )` | BMLPlus 传进来的东西 |
| `{ ... }` | 函数里面真正执行的代码 |

`OnLoad` 是 BMLPlus 认识的名字。脚本加载完成后，BMLPlus 会调用它。

名字不能随便改。写成 `OnLoaded`，BMLPlus 就不会把它当成 `OnLoad`。

## `ctx`

这段里有一个参数：

```angelscript
const BML::ModContext &in ctx
```

第一次看到会很长。先只认最后的名字：

```text
ctx
```

`ctx` 是 BMLPlus 传进来的上下文。前几章拿日志、输入、配置、命令、Timer，都会从 `ctx` 开始。

例如：

```angelscript
BML::Logger@ logger = ctx.BorrowLogger();
```

意思是：从 `ctx` 里借一个日志对象。

`const BML::ModContext &in` 先按“BMLPlus 传进来的上下文类型”理解。以后查 API 时再细分 `const` 和 `&in`。

## 对象句柄 `@`

这一行里有 `@`：

```angelscript
BML::Logger@ logger = ctx.BorrowLogger();
```

先拆开看：

| 部分 | 含义 |
| --- | --- |
| `BML::Logger@` | 一个日志对象句柄 |
| `logger` | 变量名 |
| `ctx.BorrowLogger()` | 从 BMLPlus 借日志对象 |

`@` 表示对象句柄。可以先按“指向一个对象”理解。

这个句柄可能拿不到对象，所以后面要检查。

## `null`

`null` 表示没有对象。

```angelscript
if (logger !is null) {
    logger.Info("HelloMod loaded");
}
```

这段意思是：

```text
如果 logger 不是空的
  就写日志
```

`!is null` 表示不是空引用。

对应地：

```angelscript
if (logger is null) {
    return;
}
```

意思是：

```text
如果 logger 是空的
  直接离开当前函数
```

以后看到 BML 或 CKAS 返回对象句柄，先检查是不是 `null`，再使用。

## 字符串

双引号里的内容是字符串：

```angelscript
"HelloMod loaded"
```

字符串就是一段文本。

写日志时常用：

```angelscript
logger.Info("HelloMod loaded");
```

也可以拼接：

```angelscript
string message = "Loaded: " + "HelloMod";
```

第 3 章先用固定文字。后面显示 FPS、命令参数、对象名字时，会继续用字符串拼接。

## 变量

变量用来保存值。

```angelscript
bool showWindow = true;
int count = 0;
float fps = 0.0f;
string message = "Hello";
```

先记这几个类型：

| 类型 | 保存什么 |
| --- | --- |
| `bool` | true / false |
| `int` | 整数 |
| `float` | 小数 |
| `string` | 文本 |

变量名前面的词说明它保存什么类型的值。

## 成员变量

变量写在函数外、类里面，叫成员变量：

```angelscript
class HelloMod {
    private bool showWindow = true;

    void OnProcess(const BML::ModContext &in ctx) {
    }
}
```

`showWindow` 会跟着 `HelloMod` 对象一起存在。

`OnProcess` 每帧都会被调用，但 `showWindow` 不会每帧重新变回默认值。它适合保存窗口是否显示、上一次命令、Timer 跑了几次。

`private` 表示这个变量只给 `HelloMod` 自己用。

## `if` 和 `return`

`if` 表示判断：

```angelscript
if (showWindow) {
    DrawWindow();
}
```

意思是：

```text
如果 showWindow 为 true
  执行 DrawWindow()
```

`return` 表示离开当前函数：

```angelscript
if (!showWindow) {
    return;
}
```

意思是：

```text
如果 showWindow 为 false
  直接离开
```

`!` 表示取反。`!showWindow` 可以读成“不是 showWindow”。

## `BML::`

`BML::Logger`、`BML::ModContext` 里的 `BML::` 表示这些名字来自 BML API。

```angelscript
BML::Logger
BML::ModContext
BML::CommandDefinition
```

后面还会看到：

```angelscript
BML::CK::GetName
```

可以先读成：

```text
BML 这一组 API 里的 CK 工具函数 GetName
```

## 本章先记住的读法

看到这段代码：

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    BML::Logger@ logger = ctx.BorrowLogger();
    if (logger !is null) {
        logger.Info("HelloMod loaded");
    }
}
```

按这个顺序读：

```text
OnLoad 被 BMLPlus 调用
  从 ctx 借 Logger
  如果 Logger 存在
    写入 HelloMod loaded
```

到这里就够了。

第 3 章会把这段脚本放进 `ModLoader/Mods`，启动游戏，看它有没有真的写出日志。
