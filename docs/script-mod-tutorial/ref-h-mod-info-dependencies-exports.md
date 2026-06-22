# 参考 H：mod 信息、依赖和导出

前面几章一直在写单个脚本。真实 mod 经常会遇到另一个问题：

```text
当前有哪些 mod 已经加载
某个 mod 是否存在
某个 mod 是否加载失败
自己的依赖声明是否满足
另一个 mod 是否提供了可调用的导出函数
```

这一章讲 BML 层的 mod 关系。它仍然站在 BML 脚本 mod 这一层，不进入 Virtools 对象修改。

## 三个概念

先分清三个名字：

| 名字 | 含义 |
| --- | --- |
| `ModRef` | 指向一个 BML mod 的查询句柄 |
| 依赖 | 写在元数据里的加载关系 |
| 导出 | 一个 mod 公开给其他 mod 调用的函数 |

可以把关系想成：

```text
ModContext
  -> GetModCount / GetMod / FindMod
      -> ModRef
          -> 元信息、状态、依赖、导出
              -> ExportRef
                  -> 调用公开函数
```

`ModRef` 不是 Virtools 对象。它只是 BML 层的 mod 信息入口。

## 准备两个脚本

这一章用两个文件：

```text
ModLoader/Mods/ModInfoProvider.mod.as
ModLoader/Mods/ModInfoInspector.mod.as
```

`Provider` 暴露两个导出函数。  
`Inspector` 查询 mod 列表、读取依赖信息，并调用 `Provider` 的导出。

## 提供导出的脚本

新建：

```text
ModLoader/Mods/ModInfoProvider.mod.as
```

写入：

```angelscript
[bml.mod id="modinfo.provider" name="Mod Info Provider" version="1.0.0" author="Tutorial" bml="0.3.12" description="Provides exports for the mod info chapter"]
class ModInfoProvider {
    void OnLoad(const BML::ModContext &in ctx) {
        LogInfo(ctx, "Provider loaded");
    }

    [bml.export name="Greeting"]
    string Greeting(const string &in name) {
        return "Hello, " + name;
    }

    [bml.export name="Answer"]
    int Answer() {
        return 42;
    }

    private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(message);
        }
    }
}
```

`[bml.export ...]` 要贴在 `bml.mod` 类的方法上。  
这里公开了两个函数：

```text
Greeting(string) -> string
Answer() -> int
```

参数名不是导出契约的一部分。BML 会根据编译后的方法声明得到导出签名。

## 查询和调用的脚本

新建：

```text
ModLoader/Mods/ModInfoInspector.mod.as
```

写入：

```angelscript
[bml.require id="modinfo.provider" version="1.0.0"]
[bml.optional id="modinfo.debug" version="0.1.0"]
[bml.mod id="modinfo.inspector" name="Mod Info Inspector" version="1.0.0" author="Tutorial" bml="0.3.12" description="Inspects mod metadata, dependencies and exports"]
class ModInfoInspector {
    void OnLoad(const BML::ModContext &in ctx) {
        LogInfo(ctx, "Inspector loaded modCount=" + ctx.GetModCount());

        InspectSelf(ctx);
        InspectProvider(ctx);
    }

    private void InspectSelf(const BML::ModContext &in ctx) {
        BML::ModRef@ self = ctx.FindMod(ctx.GetModId());
        if (self is null || !self.IsValid) {
            LogInfo(ctx, "Inspector self ref unavailable");
            return;
        }

        LogInfo(ctx, "Inspector self id=" + self.Id +
                     " name=" + self.Name +
                     " version=" + self.Version +
                     " deps=" + self.GetDependencyCount() +
                     " check=" + self.CheckDependencies());

        for (int i = 0; i < self.GetDependencyCount(); i++) {
            LogInfo(ctx, "Inspector dep " + i +
                         " id=" + self.GetDependencyId(i) +
                         " version=" + self.GetDependencyVersion(i) +
                         " optional=" + BoolText(self.IsDependencyOptional(i)));
        }
    }

    private void InspectProvider(const BML::ModContext &in ctx) {
        BML::ModRef@ provider = ctx.FindMod("modinfo.provider");
        if (provider is null || !provider.IsValid) {
            LogInfo(ctx, "Provider not found");
            return;
        }

        LogInfo(ctx, "Provider id=" + provider.Id +
                     " name=" + provider.Name +
                     " version=" + provider.Version +
                     " script=" + BoolText(provider.IsScript) +
                     " failed=" + BoolText(provider.IsFailed) +
                     " exports=" + provider.GetExportCount());

        for (int i = 0; i < provider.GetExportCount(); i++) {
            LogInfo(ctx, "Provider export " + i +
                         " name=" + provider.GetExportName(i) +
                         " signature=" + provider.GetExportSignature(i));
        }

        CallGreeting(ctx, provider);
        CallAnswer(ctx, provider);
    }

    private void CallGreeting(const BML::ModContext &in ctx, BML::ModRef@ provider) {
        BML::ExportRef@ greeting;
        int lookup = provider.TryFindExport("Greeting", greeting);

        if (lookup != BML::ERROR_OK || greeting is null || !greeting.IsValid) {
            LogInfo(ctx, "Greeting lookup status=" + lookup);
            return;
        }

        string result;
        int callStatus = greeting.CallString("Ballance", result);
        LogInfo(ctx, "Greeting call status=" + callStatus + " result=" + result);
    }

    private void CallAnswer(const BML::ModContext &in ctx, BML::ModRef@ provider) {
        BML::ExportRef@ answer;
        int lookup = provider.TryFindExport("Answer", answer);

        if (lookup != BML::ERROR_OK || answer is null || !answer.IsValid) {
            LogInfo(ctx, "Answer lookup status=" + lookup);
            return;
        }

        int result = 0;
        int callStatus = answer.CallInt(result);
        LogInfo(ctx, "Answer call status=" + callStatus + " result=" + result);
    }

    private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(message);
        }
    }

    private string BoolText(bool value) {
        return value ? "true" : "false";
    }
}
```

这份脚本开头有两行依赖元数据：

```angelscript
[bml.require id="modinfo.provider" version="1.0.0"]
[bml.optional id="modinfo.debug" version="0.1.0"]
```

`bml.require` 表示必需依赖。缺少它，当前 mod 不应该正常加载。  
`bml.optional` 表示可选关系。缺少它，不阻止当前 mod 加载，但可以通过 `ModRef` 查询出来。

依赖是元数据，不是在 `OnLoad` 里临时创建的运行时状态。

## 查询 mod 列表

入口在 `ctx` 上：

```angelscript
int count = ctx.GetModCount();
BML::ModRef@ mod = ctx.GetMod(index);
BML::ModRef@ provider = ctx.FindMod("modinfo.provider");
```

`GetModCount()` 告诉你 BML 当前知道多少个 mod。  
`GetMod(index)` 按序号取一个 `ModRef`。  
`FindMod(id)` 按 mod id 查找。

拿到 `ModRef@` 后先判空，再看 `IsValid`：

```angelscript
if (provider is null || !provider.IsValid) {
    return;
}
```

`ModRef` 常用信息：

| 属性或函数 | 含义 |
| --- | --- |
| `Id` | mod id |
| `Name` | 显示名称 |
| `Version` | mod 自己的版本 |
| `Author` | 作者 |
| `Description` | 描述 |
| `BMLVersion` | 需要的 BML 版本 |
| `IsScript` | 是否脚本 mod |
| `IsFailed` | 是否加载失败 |
| `Diagnostic` | 失败诊断信息 |
| `GetState()` | BML 层状态编号 |
| `GetKind()` | mod 类型编号 |

`Diagnostic` 主要在 `IsFailed` 为 `true` 时有用。

## 查询依赖

依赖从 `ModRef` 上读：

```angelscript
int count = self.GetDependencyCount();
string id = self.GetDependencyId(i);
string version = self.GetDependencyVersion(i);
bool optional = self.IsDependencyOptional(i);
```

本章脚本声明了两个依赖：

```text
modinfo.provider  必需
modinfo.debug     可选
```

所以日志里会看到两条依赖记录。  
`CheckDependencies()` 用来判断依赖是否满足。当前实现里：

```text
1  满足
0  不满足
```

可选依赖缺失不会让 `CheckDependencies()` 失败。

## 查询导出

导出也从 `ModRef` 上读：

```angelscript
int exportCount = provider.GetExportCount();
string name = provider.GetExportName(i);
string signature = provider.GetExportSignature(i);
```

本章的 provider 有两个导出：

```text
Greeting
Answer
```

查询某个导出：

```angelscript
BML::ExportRef@ greeting;
int lookup = provider.TryFindExport("Greeting", greeting);
```

`TryFindExport` 比 `FindExport` 更适合教程示例，因为它会返回查找状态。  
查找成功时，`lookup == BML::ERROR_OK`，并且 `greeting` 是有效的 `ExportRef@`。

如果同名导出有多个签名，需要传入签名：

```angelscript
provider.TryFindExport("Greeting", greeting, "string Greeting(const string &in)");
```

签名字符串可以先用 `GetExportSignature(i)` 打印出来，再复制到查询代码里。

本章的导出名没有重载，所以省略签名。

## 调用导出

`Greeting` 是：

```text
string Greeting(string)
```

所以用：

```angelscript
string result;
int callStatus = greeting.CallString("Ballance", result);
```

`Answer` 是：

```text
int Answer()
```

所以用无参数的 `CallInt`：

```angelscript
int result = 0;
int callStatus = answer.CallInt(result);
```

`callStatus == BML::ERROR_OK` 表示调用成功。  
返回非零值时，先把状态码写进日志，不要直接使用结果变量。

`ExportRef` 还支持更通用的 `CallFrame`。它适合多参数、数组、对象 id 这类情况。入门阶段先用 `CallString`、`CallInt`、`CallBool`、`CallFloat` 这种窄接口，出错点少很多。

## 运行后看日志

保存两个脚本，重启 Player。

可以看到：

```text
[ModLoader/INFO]: Loading Mod modinfo.provider[Mod Info Provider] v1.0.0 by Tutorial
[modinfo.provider/INFO]: Provider loaded
[ModLoader/INFO]: Loading Mod modinfo.inspector[Mod Info Inspector] v1.0.0 by Tutorial
[modinfo.inspector/INFO]: Inspector loaded modCount=4
[modinfo.inspector/INFO]: Inspector self id=modinfo.inspector name=Mod Info Inspector version=1.0.0 deps=2 check=1
[modinfo.inspector/INFO]: Inspector dep 0 id=modinfo.provider version=1.0.0 optional=false
[modinfo.inspector/INFO]: Inspector dep 1 id=modinfo.debug version=0.1.0 optional=true
[modinfo.inspector/INFO]: Provider id=modinfo.provider name=Mod Info Provider version=1.0.0 script=true failed=false exports=2
[modinfo.inspector/INFO]: Provider export 0 name=Answer signature=int Answer()
[modinfo.inspector/INFO]: Provider export 1 name=Greeting signature=string Greeting(const string &in)
[modinfo.inspector/INFO]: Greeting call status=0 result=Hello, Ballance
[modinfo.inspector/INFO]: Answer call status=0 result=42
[ModLoader/INFO]: BML script mod summary: loaded=2 failed=0
```

`modCount=4` 是这次测试环境里的结果。启用其他 mod 时数字会变。

## 依赖、导出、DataShare 怎么选

先按需求选：

| 需求 | 优先考虑 |
| --- | --- |
| 必须等另一个 mod 先加载 | `bml.require` |
| 有另一个 mod 时启用额外功能，缺少也能运行 | `bml.optional` + `FindMod` |
| 调用另一个 mod 的函数并拿返回值 | 导出 |
| 共享一个状态，其他脚本稍后按名字读取 | DataShare |
| 和 CKAS runtime script 或组件通信 | CKAS `Message` |

导出更像函数调用：现在请求，现在返回。  
DataShare 更像一块命名状态：写入后，别人之后可以读取。

参考 I会专门讲 DataShare。

## 本章结果

现在可以把 BML 层的 mod 关系看成：

```text
metadata
  -> bml.require / bml.optional / bml.export

runtime query
  -> ctx.GetModCount()
  -> ctx.FindMod(id)
  -> ModRef
  -> dependency list / export list
  -> ExportRef
```

本章只做了一个最小导出调用。跨 mod 设计不要从“能调用”直接跳到“到处互相调用”。先判断关系是加载顺序、函数调用，还是共享状态。

下一章讲 DataShare 和跨脚本共享状态。
