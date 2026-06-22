# 参考 D：路径、资源和日志诊断

前面已经用过 `ctx.GetModRootUtf8()`，但只是把它打印出来。

这一章补上三个问题：

```text
脚本自己的资源文件放在哪里
脚本里该怎样写资源路径
出错以后先看哪几行日志
```

路径和日志看起来很基础，后面做贴图、文本、配置默认值、对象加载时都会碰到。先把这一层弄清楚，后面排错会省很多时间。

## mod 根目录是什么

先看一个脚本文件：

```text
ModLoader/Mods/PathLogMod.mod.as
```

BML 会给这个脚本准备一个 mod 根目录。可以先按下面的结构理解：

```text
ModLoader/Mods/
  PathLogMod.mod.as
  PathLogMod/
    data/
      message.txt
```

脚本文件是入口。  
`PathLogMod/` 这一层用来放脚本自己的资源文件。

脚本里不要写机器上的完整路径。写完整路径会带来两个问题：

```text
换一台电脑就失效
发布给别人以后路径一定对不上
```

所以脚本里只写相对路径：

```text
data/message.txt
```

BML 负责把它拼到当前 mod 根目录下面。

## 准备资源文件

先建资源文件：

```text
ModLoader/Mods/PathLogMod/data/message.txt
```

内容写一行：

```text
hello from resource
```

这个文件不是 AngelScript 代码，只是普通文本。脚本稍后会读取它。

## 读取资源文件的小脚本

新建脚本：

```text
ModLoader/Mods/PathLogMod.mod.as
```

写入：

```angelscript
[bml.mod id="pathlog.script" name="Path Log Demo" version="1.0.0" author="Tutorial" bml="0.3.12" description="Read a resource file and log each step"]
class PathLogMod {
    void OnLoad(const BML::ModContext &in ctx) {
        LogInfo(ctx, "PathLog mod id=" + ctx.GetModId() + " name=" + ctx.GetModName());

        // GetModRootUtf8() 用来确认当前 mod 的根目录。
        // 日常代码里更推荐继续使用相对路径，不要把这个结果写死。
        LogInfo(ctx, "PathLog root=" + ctx.GetModRootUtf8());

        string messagePath = "data/message.txt";
        string resolvedPath = ctx.ResolveModPathUtf8(messagePath);
        bool exists = ctx.ModFileExistsUtf8(messagePath);

        // ResolveModPathUtf8() 只负责拼出完整路径，不代表文件一定存在。
        LogInfo(ctx, "PathLog resource path=" + resolvedPath);
        LogInfo(ctx, "PathLog resource exists=" + BoolText(exists));

        if (!exists) {
            LogWarn(ctx, "PathLog resource missing: " + messagePath);
            return;
        }

        string text = ctx.ReadModTextFileUtf8(messagePath, "");
        LogInfo(ctx, "PathLog resource text=" + text);

        // 可选资源不存在时，用 Warn 提醒即可，不要当成致命错误。
        string optionalPath = "data/missing.txt";
        if (!ctx.ModFileExistsUtf8(optionalPath)) {
            LogWarn(ctx, "PathLog optional missing resource: " + optionalPath);
        }
    }

    private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(message);
        }
    }

    private void LogWarn(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Warn(message);
        }
    }

    private string BoolText(bool value) {
        return value ? "true" : "false";
    }
}
```

这段代码只读 `data/message.txt`。它不会改关卡，也不会写游戏对象。

## 三个路径函数

先记这三个：

```angelscript
string GetModRootUtf8() const;
string ResolveModPathUtf8(const string &in relativePath) const;
bool ModFileExistsUtf8(const string &in relativePath) const;
```

分别对应三件事：

| 函数 | 用途 |
| --- | --- |
| `GetModRootUtf8()` | 取得当前 mod 根目录 |
| `ResolveModPathUtf8("data/message.txt")` | 把相对路径转换成完整路径 |
| `ModFileExistsUtf8("data/message.txt")` | 判断资源文件是否存在 |

读取文本文件再加一个：

```angelscript
string ReadModTextFileUtf8(const string &in relativePath, const string &in defaultValue = "") const;
```

它读取当前 mod 根目录下面的文本文件。文件不存在时返回默认值。

## 为什么先判断存在

`ReadModTextFileUtf8(...)` 已经有默认值参数：

```angelscript
string text = ctx.ReadModTextFileUtf8("data/message.txt", "");
```

但教程里仍然先写：

```angelscript
bool exists = ctx.ModFileExistsUtf8(messagePath);
if (!exists) {
    LogWarn(ctx, "PathLog resource missing: " + messagePath);
    return;
}
```

原因很简单：日志更清楚。

如果直接读取，最后只得到一个空字符串，很难判断是文件不存在、文件内容为空，还是路径写错。  
先判断存在，日志会直接告诉你问题在路径或文件。

## 日志前缀怎么看

运行后看日志，会看到这种格式：

```text
[pathlog.script/INFO]: PathLog mod id=pathlog.script name=Path Log Demo
[pathlog.script/INFO]: PathLog root=<mod 根目录>
[pathlog.script/INFO]: PathLog resource path=<mod 根目录>\data\message.txt
[pathlog.script/INFO]: PathLog resource exists=true
[pathlog.script/INFO]: PathLog resource text=hello from resource
[pathlog.script/WARN]: PathLog optional missing resource: data/missing.txt
[ModLoader/INFO]: BML script mod summary: loaded=1 failed=0
```

方括号前半段是来源：

```text
pathlog.script
```

这就是 `[bml.mod ...]` 里写的 `id`。  
一个日志文件里可能混着 BML、游戏、多个 mod 的输出，先按这个 id 找自己的日志。

斜杠后面是级别：

| 级别 | 适合写什么 |
| --- | --- |
| `INFO` | 正常步骤，例如加载完成、资源存在、命令注册成功 |
| `WARN` | 可以继续运行的问题，例如可选资源不存在、配置项缺失后使用默认值 |
| `ERROR` | 当前功能无法继续，例如必需资源无效、关键服务拿不到 |

入门阶段不要把正常流程写成 `ERROR`。日志里错误太多，真正的问题会被淹没。

## 编译错误怎么定位

脚本编译失败时，BML 会把文件、行号和错误写进日志。形状大致是：

```text
<mods>/PathLogMod.mod.as(12,5): ERROR: Expected ...
```

括号里的两个数字按下面读：

```text
(行号, 列号)
```

排查顺序：

```text
先看第一条 ERROR
打开对应 .mod.as 文件
跳到行号
检查这一行和上一行
```

很多 AngelScript 编译错误出现在“下一行”。例如上一行少了分号，错误可能报到下一行开头。看行号时不要只盯一行，顺手看它前面一两行。

常见原因：

| 日志里的现象 | 先检查 |
| --- | --- |
| `Expected ',' or ';'` | 上一行是不是少了分号 |
| `No matching signatures` | 函数名、参数数量、参数类型是否和 API 声明一致 |
| `Type ... is not available` | 类型名是否拼错，命名空间是否漏了 |
| `No conversion` | 把数字、字符串、句柄混用了 |
| `Null pointer access` | `Borrow...()` 返回后有没有判空 |

## 路径问题怎么查

资源加载失败时，按这个顺序查：

```text
相对路径是否写对
资源文件是否放在当前 mod 根目录下面
文件名大小写是否一致
脚本修改后是否重启 Player
日志里显示的 mod id 是否是当前脚本
```

路径字符串建议统一使用 `/`：

```angelscript
string messagePath = "data/message.txt";
```

不要写成：

```angelscript
string messagePath = "某台电脑上的完整路径";
```

`ResolveModPathUtf8(...)` 打印出来的完整路径只用于调试。它能帮你确认 BML 最终找的是哪里，但不要把这条完整路径再复制回脚本。

## 本章结果

现在可以把资源读取理解成这条链：

```text
脚本里的相对路径
  -> ResolveModPathUtf8 拼到 mod 根目录
  -> ModFileExistsUtf8 确认文件存在
  -> ReadModTextFileUtf8 读取文本
  -> Logger 写出每一步结果
```

以后碰到“资源没加载”“文本是空的”“命令注册没成功”，先补日志，不要急着改逻辑。日志要写在关键分叉点上：开始做、拿到了什么、失败原因是什么。

下一章讲输入、UI 和每帧状态。那一章会把 `OnProcess`、按键触发和 ImGui 窗口状态放在同一张图里讲清楚。
