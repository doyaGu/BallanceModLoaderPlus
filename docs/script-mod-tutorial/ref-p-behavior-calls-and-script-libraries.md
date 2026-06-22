# 参考 P：行为调用和脚本库组织

脚本写到后面，会遇到两类“调用”：

| 名字 | 用在哪里 | 解决什么问题 |
| --- | --- | --- |
| `BML::CallFrame` | BML 脚本 mod 之间 | 一个 mod 调另一个 mod 的导出函数 |
| `BB::Require` / `BBConfig` / `BBInstance` | CKAS runtime script 或 AngelScript Component | 在 Virtools 行为图环境里创建、启动、步进 Building Block |

这两类 API 都有“Call”的味道，但所在层级不同。`CallFrame` 不会启动 Virtools Building Block；`BBInstance` 属于 CKAS 的行为图运行层。

这一章分三步：

```text
先用 CallFrame 调另一个 BML mod 的分数文本服务
再看 CKAS 里运行 Building Block 的模型
最后把可复用代码拆成 .as 脚本库
```

## CallFrame 是一张调用单

参考 H已经用过 `CallString`、`CallInt` 这类窄接口。它们适合一个参数、一个返回值的简单导出。

`CallFrame` 更通用。可以把它想成一张调用单：

```text
调用前：
  第 0 个参数 = string
  第 1 个参数 = int

调用时：
  ExportRef.Call(frame)

调用后：
  frame 里出现一个 result
```

参数靠位置传，不靠参数名传。第 0 个、第 1 个、第 2 个要和导出函数的声明顺序一致。

## 例子：一个 mod 提供分数文本

先新建：

```text
ModLoader/Mods/ScoreTextProvider.mod.as
```

写入：

```angelscript
[bml.mod id="scoretext.provider" name="Score Text Provider" version="1.0.0" author="Tutorial" bml="0.3.12" description="Provides score text exports"]
class ScoreTextProvider {
    void OnLoad(const BML::ModContext &in ctx) {
        LogInfo(ctx, "ScoreTextProvider loaded");
    }

    // 这个导出有两个参数。调用方要用 CallFrame 填入第 0、1 个参数。
    [bml.export name="MakeScoreLine"]
    string MakeScoreLine(const string &in label, int score) {
        return label + ": " + score + " pts";
    }

    // 另一个多参数导出，模拟把基础分和奖励分合成总分。
    [bml.export name="AddBonus"]
    int AddBonus(int baseScore, int bonus) {
        return baseScore + bonus;
    }

    private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(message);
        }
    }
}
```

`[bml.export ...]` 贴在方法上。BML 会把这个方法登记成可被其他 mod 查找的导出。

这里有两个导出：

```text
MakeScoreLine(string, int) -> string
AddBonus(int, int) -> int
```

## 用 CallFrame 调用它

再新建：

```text
ModLoader/Mods/ScoreTextUser.mod.as
```

写入：

```angelscript
[bml.require id="scoretext.provider" version="1.0.0"]
[bml.mod id="scoretext.user" name="Score Text User" version="1.0.0" author="Tutorial" bml="0.3.12" description="Uses score text exports"]
class ScoreTextUser {
    void OnLoad(const BML::ModContext &in ctx) {
        BML::ModRef@ provider = ctx.FindMod("scoretext.provider");
        if (provider is null || !provider.IsValid) {
            LogInfo(ctx, "provider missing");
            return;
        }

        CallMakeScoreLine(ctx, provider);
        CallAddBonus(ctx, provider);
    }

    private void CallMakeScoreLine(const BML::ModContext &in ctx, BML::ModRef@ provider) {
        BML::ExportRef@ makeLine;

        // 签名写全，可以避免同名导出造成歧义。
        int lookup = provider.TryFindExport("MakeScoreLine", makeLine, "string(string,int)");
        if (lookup != BML::ERROR_OK || makeLine is null || !makeLine.IsValid) {
            LogInfo(ctx, "MakeScoreLine lookup status=" + lookup);
            return;
        }

        BML::CallFrame@ frame = BML::CallFrame();

        // 参数位置要和 MakeScoreLine(const string &in label, int score) 对齐。
        frame.SetString(0, "Level score");
        frame.SetInt(1, 1200);

        int status = makeLine.Call(frame);
        string result;
        if (status == BML::ERROR_OK && frame.GetResultString(result) == BML::ERROR_OK) {
            LogInfo(ctx, "MakeScoreLine result=" + result);
            ctx.SendIngameMessage(result);
        } else {
            LogInfo(ctx, "MakeScoreLine call status=" + status + " resultType=" + frame.ResultType);
        }
    }

    private void CallAddBonus(const BML::ModContext &in ctx, BML::ModRef@ provider) {
        BML::ExportRef@ add;
        int lookup = provider.TryFindExport("AddBonus", add, "int(int,int)");
        if (lookup != BML::ERROR_OK || add is null || !add.IsValid) {
            LogInfo(ctx, "AddBonus lookup status=" + lookup);
            return;
        }

        BML::CallFrame@ frame = BML::CallFrame();
        frame.SetInt(0, 1200);
        frame.SetInt(1, 100);

        int status = add.Call(frame);
        int result = 0;
        if (status == BML::ERROR_OK && frame.GetResultInt(result) == BML::ERROR_OK) {
            LogInfo(ctx, "AddBonus result=" + result);
            ctx.SendIngameMessage("Score with bonus: " + result);
        } else {
            LogInfo(ctx, "AddBonus call status=" + status + " resultType=" + frame.ResultType);
        }
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
Loading Mod scoretext.provider[Score Text Provider] v1.0.0 by Tutorial
ScoreTextProvider loaded
Loading Mod scoretext.user[Score Text User] v1.0.0 by Tutorial
MakeScoreLine result=Level score: 1200 pts
AddBonus result=1300
BML script mod summary: loaded=2 failed=0
```

游戏里也会出现两条短消息：

```text
Level score: 1200 pts
Score with bonus: 1300
```

这样这个例子不只是在日志里证明调用成功。它更接近实际用途：一个 mod 提供格式化和计算服务，另一个 mod 拿结果显示给玩家。

这段代码里有四个检查点：

| 检查点 | 为什么要查 |
| --- | --- |
| `provider is null` | 目标 mod 没有找到 |
| `!provider.IsValid` | 拿到的句柄不可用 |
| `lookup != BML::ERROR_OK` | 导出函数没有找到，或者签名不匹配 |
| `frame.GetResultString/GetResultInt` | 返回值类型要和导出声明一致 |

`makeLine.Call(frame)` 返回成功，只说明导出函数执行完成。读取结果时还要用对应的 `GetResult...`。函数返回 `string` 就用 `GetResultString`，返回 `int` 就用 `GetResultInt`。

## 什么时候用 CallFrame

优先顺序可以这样定：

| 情况 | 选法 |
| --- | --- |
| 无参数、无返回 | `CallVoid()` |
| 一个 `string/int/bool/float` 参数或返回 | `CallString`、`CallInt`、`CallBool`、`CallFloat` |
| 多个参数 | `CallFrame` |
| 数组、对象 id、动态参数列表 | `CallFrame` |
| 每帧频繁调用 | 先缓存 `ExportRef@`，不要每帧重新 `FindMod` / `TryFindExport` |

`CallFrame` 可以重复使用，但入门代码里每次新建一个更清楚。等调用路径稳定后，再考虑把 `CallFrame@` 存成成员并在每次调用前 `Clear()`。

## Building Block 调用模型

CKAS 的 Building Block bridge 面向 Virtools 行为图。常见对象关系如下：

| 对象 | 含义 |
| --- | --- |
| `BBDecl@` | 某个 Building Block 原型，例如 `Interface/Text/2D Text` |
| `BBSlot@` | 原型上的一个输入、输出、参数、设置项 |
| `BBConfig@` | 准备创建实例前的配置 |
| `BBInstance@` | 已经创建出来的运行时行为 |

常见流程：

```text
BB::Require(ctx, 名字)
  -> 得到 BBDecl
  -> 找 pin / input / output
  -> Configure()
  -> SpawnStarted(ctx) 或 EnsureStarted(ctx)
  -> Step / Set / Stop / Destroy
```

下面是 CKAS AngelScript Component 里的写法：

```angelscript
class TextOverlay {
    [bbconfig prototype="Interface/Text/2D Text" lifetime="component"]
    [bbsetting "Text Properties"="Screen Proportionnal,WordWrap"]
    [bbpin "Text"="Ready"]
    BBConfig@ Text;

    [bbslot from="Text" pin="Text"]
    BBSlot@ TextPin;

    BBInstance@ Instance;

    void Start(const CKBehaviorContext &in ctx) {
        // EnsureStarted 会创建并启动这个 BB；已经存在时复用当前实例。
        @Instance = Text.EnsureStarted(ctx);
    }

    void Update(const CKBehaviorContext &in ctx) {
        if (Instance !is null) {
            // StepSet 写入参数并执行一步。
            Instance.StepSet(ctx, TextPin, "Frame running");
        }
    }
}
```

这段代码需要 `CKBehaviorContext`。它适合放在 AngelScript Component 中，因为组件本来就挂在 Virtools 行为图里。

BML 脚本 mod 的固定回调拿到的是 `BML::ModContext`。在 BML mod 里要优先使用 `BML::CK`、`BML::UI`、`BML::Command`、`BML::Timer`、`ModRef/ExportRef` 这些 BML 层 API；需要长期参与行为图执行时，再考虑 CKAS runtime script 或 AngelScript Component。

## 哪些调用适合封装

适合封装的代码有几个特征：

| 代码 | 封装价值 |
| --- | --- |
| 重复查同一个导出 | 封装 lookup 和错误日志 |
| 多处都要构造同样的 `CallFrame` | 封装参数顺序，避免第 0、1、2 个参数写错 |
| ImGui 绘制工具 | 封装布局、缩放、拖动和绘制细节 |
| 行为图查看器 | 封装节点、连线、视图状态 |

不急着封装的代码：

```text
只出现一次的查询
还在试名字、试参数的探索代码
生命周期还没想清楚的对象缓存
```

封装的目标是减少重复和降低出错概率。只为了让代码“看起来高级”而拆文件，会让调试更麻烦。

## .as 脚本库怎么放

可以把入口脚本和库文件放成这样：

```text
ModLoader/Mods/MyTool.mod.as
ModLoader/Mods/libs/MyToolCalls.as
ModLoader/Mods/libs/BehaviorGraphImGui.as
```

入口脚本顶部引用：

```angelscript
#include "libs/MyToolCalls.as"
#include "libs/BehaviorGraphImGui.as"
```

库文件不要写 `[bml.mod ...]`。一个脚本 mod 只需要一个入口类，库文件只放命名空间、类和函数。

例如：

```angelscript
namespace MyToolCalls {
    string FormatScore(const string &in name, int score) {
        return name + "=" + score;
    }
}
```

入口脚本里调用：

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    BML::Logger@ logger = ctx.BorrowLogger();
    if (logger !is null) {
        logger.Info(MyToolCalls::FormatScore("Include", 7));
    }
}
```

正常结果：

```text
Loading Mod include.test[Include Test] v1.0.0 by Tutorial
Include=7
BML script mod summary: loaded=1 failed=0
```

## 行为图 ImGui 查看器为什么适合做成库

行为图查看器包含很多和业务无关的细节：

```text
节点结构
连线结构
缩放和平移
节点拖动
端口位置
文字裁剪
网格背景
右侧详情面板
```

这些东西放在入口脚本里，会淹没真正想表达的内容。拆成库以后，入口脚本只负责构造图：

```angelscript
#include "libs/BehaviorGraphImGui.as"

[bml.mod id="graph.viewer" name="Graph Viewer" version="1.0.0" author="Tutorial" bml="0.3.12" description="Shows a small graph"]
class GraphViewer {
    private BGImGui::Graph graph;
    private BGImGui::View view;

    void OnLoad(const BML::ModContext &in ctx) {
        int start = graph.AddNodeIndex("start", "Activate Script", 20.0f, 100.0f);
        int show = graph.AddNodeIndex("show", "Show", 300.0f, 100.0f);
        graph.AddEdge(start, show, "exec");
    }

    void OnProcess(const BML::ModContext &in ctx) {
        if (ImGui::Begin("Graph Viewer")) {
            view.Draw(graph, "graph_canvas", ImVec2(0.0f, 420.0f));
        }
        ImGui::End();
    }
}
```

入口脚本现在只回答三个问题：

```text
这张图有哪些节点
节点之间怎么连
什么时候画出来
```

至于节点怎么画、缩放怎么处理、拖动怎么更新坐标，都交给 `BehaviorGraphImGui.as`。

## 小结

这一章要记住三件事：

```text
CallFrame 是 BML mod 导出调用的参数和返回值容器。
CKAS 的 BBConfig / BBInstance 是 Virtools 行为图运行时对象。
.as 脚本库适合放稳定、重复、和入口逻辑无关的代码。
```

写脚本时先让入口文件保持直观。等某段逻辑出现第二次、第三次，再把它移到库里。

