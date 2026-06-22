# 参考 O：资源、文本和 UI

前面几章主要在改运行中的对象和表。这一章换一个入口：脚本怎样接触资源文件，怎样向游戏里显示短消息，怎样理解 2D Text 和 BML UI。

先把三个名字分开：

| 名字 | 先按什么理解 |
| --- | --- |
| 资源文件 | 硬盘上的 `.nmo`、`.cmo`、贴图、字体、文本 |
| Virtools 对象 | 文件加载后出现在 CKContext 里的对象 |
| UI | 给玩家看的消息、菜单按钮、调试窗口 |

资源文件还没有进入游戏。  
`LoadObject` 会把资源文件里的对象加载进 Virtools。  
`BML::UI::SendMessage` 只是在游戏里显示一行短消息。  
`BML::Text::Create2DText` 是创建 Virtools 的 `2D Text` 行为，需要现成的 owner script 和 2D entity。

这四件事不要混在一起。

## 本章实际使用哪些 API

先看本章会碰到的 API：

| API | 用途 |
| --- | --- |
| `BML::Path::IsFile(path)` | 加载前确认文件存在 |
| `BML::ObjectLoadOptions` | 描述要加载什么 |
| `BML::CK::LoadObject(options)` | 执行 Object Load |
| `BML::ObjectLoadResult` | 查看加载后的对象数量、主对象 |
| `BML::UI::SendMessage(text)` | 在游戏里显示短消息 |
| `BML::UI::ClearMessages()` | 清掉 BML 消息 |
| `BML::Text2DDefinition` | 描述 2D Text 的字体和文字 |
| `BML::Text::Create2DText(...)` | 创建 Virtools `2D Text` 行为 |

本章的完整脚本只演示资源加载和消息。`Create2DText` 单独讲，因为它依赖已有行为图对象，直接塞进入门脚本容易变成“能编译但不知道为什么显示不出来”。

## 完整脚本

把下面脚本放到：

```text
ModLoader/Mods/ResMod.mod.as
```

```angelscript
[bml.mod id="res.script" name="Res Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Load a resource and show messages"]
class ResMod {
    void OnLoad(const BML::ModContext &in ctx) {
        BML::CommandDefinition def;
        def.Name = "res";
        def.Description = "Resource and message demo";
        def.Usage = "res [message|clear|load|missing]";

        BML::CommandCallback@ execute = BML::CommandCallback(this.OnResCommand);
        BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompleteResCommand);
        ctx.RegisterCommand(def, execute, complete);

        BML::UI::SendMessage("Res Mod loaded.");
        ctx.BorrowLogger().Info("Res Mod loaded; use res after the game starts");
    }

    void OnUnload(const BML::ModContext &in ctx) {
        BML::UI::ClearMessages();
    }

    private void OnResCommand(const BML::ModContext &in ctx,
                              const BML::CommandEvent &in event) {
        string action = "message";
        if (event.ArgCount > 0) {
            action = event.GetArg(0);
        }

        if (action == "message") {
            BML::UI::SendMessage("Message from Res Mod.");
            ctx.BorrowLogger().Info("Res message sent");
        } else if (action == "clear") {
            BML::UI::ClearMessages();
            ctx.BorrowLogger().Info("Res messages cleared");
        } else if (action == "load") {
            LoadKnownObject(ctx);
        } else if (action == "missing") {
            TryMissingObject(ctx);
        } else {
            ctx.BorrowLogger().Warn("Unknown res action: " + action);
        }
    }

    private void CompleteResCommand(const BML::ModContext &in ctx,
                                    const BML::CommandEvent &in event,
                                    BML::CommandCompletion &inout completions) {
        completions.Add("message");
        completions.Add("clear");
        completions.Add("load");
        completions.Add("missing");
    }

    private void LoadKnownObject(const BML::ModContext &in ctx) {
        string relativePath = "3D Entities\\PH\\P_Modul_01.nmo";
        string resourcePath = BML::Path::Combine(ctx.GetDirectoryUtf8(BML::DIR_GAME),
                                                 relativePath);

        // LoadObject 之前先查路径。不要把 LoadObject 当作路径检查。
        if (!BML::Path::IsFile(resourcePath)) {
            ctx.BorrowLogger().Warn("Res file not found: " + relativePath);
            return;
        }

        BML::ObjectLoadOptions options;
        options.File = resourcePath;
        options.AddToScene = false;
        options.Rename = true;
        options.Dynamic = true;

        BML::ObjectLoadResult@ result = BML::CK::LoadObject(options);
        if (result is null || result.Count <= 0) {
            ctx.BorrowLogger().Warn("Res load returned no objects: " + relativePath);
            return;
        }

        CKObject@ main = result.BorrowMainObject();
        string mainName = main is null ? "none" : BML::CK::GetName(main);

        ctx.BorrowLogger().Info("Res load ok: success=" +
                                BoolText(result.Success) +
                                " count=" + result.Count +
                                " main=" + mainName);
    }

    private void TryMissingObject(const BML::ModContext &in ctx) {
        string relativePath = "3D Entities\\PH\\Missing_Tutorial_Object.nmo";
        string resourcePath = BML::Path::Combine(ctx.GetDirectoryUtf8(BML::DIR_GAME),
                                                 relativePath);

        if (!BML::Path::IsFile(resourcePath)) {
            ctx.BorrowLogger().Warn("Res missing path blocked before LoadObject: " + relativePath);
            return;
        }

        ctx.BorrowLogger().Warn("Unexpected existing file: " + relativePath);
    }

    private string BoolText(bool value) {
        return value ? "true" : "false";
    }
}
```

## 先看 `message` 和 `clear`

这两个命令只用 BML 的消息区域：

```angelscript
BML::UI::SendMessage("Message from Res Mod.");
BML::UI::ClearMessages();
```

它们不需要关卡对象，也不需要行为图。脚本加载时、命令回调里都能用。

这类消息适合做：

```text
脚本已加载
命令已执行
资源路径错误
状态已经恢复
```

不要把它当成长文本面板。消息是一行短提示，信息量要小。

## 再看资源路径

本章加载的是：

```text
3D Entities\PH\P_Modul_01.nmo
```

脚本里写成：

```angelscript
string relativePath = "3D Entities\\PH\\P_Modul_01.nmo";
string resourcePath = BML::Path::Combine(ctx.GetDirectoryUtf8(BML::DIR_GAME),
                                         relativePath);
```

AngelScript 字符串里，反斜杠要写成 `\\`。

加载前先查文件：

```angelscript
if (!BML::Path::IsFile(resourcePath)) {
    ctx.BorrowLogger().Warn("Res file not found: " + relativePath);
    return;
}
```

这一步很重要。一次实际测试里，先成功加载 `P_Modul_01.nmo`，再把缺失路径直接传给 `LoadObject`，日志里仍然出现了：

```text
Resource missing load success=true count=21 main=none
```

所以本章不把 `ObjectLoadResult.Success` 当作路径存在的证明。路径存在由 `BML::Path::IsFile` 负责，`LoadObject` 只负责执行加载。

## `ObjectLoadOptions` 怎么看

示例里只设置四个字段：

```angelscript
BML::ObjectLoadOptions options;
options.File = resourcePath;
options.AddToScene = false;
options.Rename = true;
options.Dynamic = true;
```

字段含义先按下面理解：

| 字段 | 含义 |
| --- | --- |
| `File` | 要加载的资源文件 |
| `AddToScene` | 加载后是否加入当前场景 |
| `Rename` | 遇到重名对象时是否自动改名 |
| `Dynamic` | 加载出的对象是否按动态对象处理 |
| `ReuseMeshes` | 是否复用已有 mesh |
| `ReuseMaterials` | 是否复用已有 material |
| `MasterName` | 指定主对象名 |
| `FilterClass` | 过滤对象类型 |

入门阶段不要急着改 `FilterClass`。默认值已经适合常见 3D 对象加载。

本章把 `AddToScene` 设成 `false`，原因很简单：先验证加载，不让对象直接出现在当前场景里。等能稳定判断加载结果以后，再考虑是否加入场景、是否移动位置、是否物理化。

## `ObjectLoadResult` 怎么看

加载后返回：

```angelscript
BML::ObjectLoadResult@ result = BML::CK::LoadObject(options);
```

检查时不要只看 `Success`：

```angelscript
if (result is null || result.Count <= 0) {
    ctx.BorrowLogger().Warn("Res load returned no objects: " + relativePath);
    return;
}
```

`Count` 表示这次结果里记录了多少个对象 id。  
`BorrowMainObject()` 尝试借主对象：

```angelscript
CKObject@ main = result.BorrowMainObject();
string mainName = main is null ? "none" : BML::CK::GetName(main);
```

一次实际运行中，加载 `P_Modul_01.nmo` 的日志是：

```text
Res load ok: success=true count=21 main=none
```

`main=none` 不代表加载失败。这个文件加载出了对象列表，但没有给出主对象。判断这一类结果时，看 `Count` 更有用。

## 缺失路径怎么处理

本章的 `missing` 命令不会把缺失路径交给 `LoadObject`：

```angelscript
if (!BML::Path::IsFile(resourcePath)) {
    ctx.BorrowLogger().Warn("Res missing path blocked before LoadObject: " + relativePath);
    return;
}
```

这是资源加载最基本的防线：

```text
路径不存在，不加载
路径存在，再 LoadObject
LoadObject 返回后，再看 Count 和主对象
```

如果脚本要加载自己随包发布的资源，应该把资源放在 mod 自己的目录里，再用路径 API 拼出文件位置。不要把自己电脑上的完整路径写进脚本。

## `Create2DText` 的心智模型

`BML::Text::Create2DText` 不等于 `SendMessage`。

`SendMessage` 是 BML 消息：

```text
给玩家显示一行短提示
调用简单
不需要自己准备 2D entity
```

`Create2DText` 是 Virtools 行为图能力：

```text
给某个 owner script 添加一个 2D Text 行为
需要 CKBehavior@ ownerScript
需要 CK2dEntity@ target
返回新建的 CKBehavior@
```

调用形状是：

```angelscript
CKBehavior@ owner = ctx.BorrowScriptByName("Level_Init");
CK2dEntity@ target = ctx.Borrow2dEntityByName("Some_2D_Entity");

BML::Text2DDefinition text;
text.Font = BML::FONT_GAME_NORMAL;
text.Text = "Hello";

CKBehavior@ behavior = BML::Text::Create2DText(owner, target, text);
```

这段代码只说明参数关系。真正放进关卡前，还要解决两件事：

```text
目标 2D entity 从哪里来
新建的 2D Text 行为什么时候销毁
```

所以入门阶段先用 `BML::UI::SendMessage`。等后面讲行为调用和脚本库组织时，再把 `Create2DText` 放进行为图上下文里处理。

## BML UI 辅助函数怎么理解

`BML::UI` 里有两类函数。

第一类是消息函数，本章已经用了：

```angelscript
BML::UI::SendMessage("text");
BML::UI::ClearMessages();
```

第二类是菜单辅助函数：

```text
Title
WrappedText
MainButton
OptionButton
YesNoButton
InputTextButton
InputIntButton
SearchBar
NavBack
```

这些函数服务于 BML 菜单绘制层。它们不是 ImGui 调试窗口 API，也不是 Virtools 2D Text。

当前版本里，不要把这些菜单绘制函数作为本章第一段可复制代码。实际测试中，把 `BML::UI::Title`、`BML::UI::MainButton` 这类函数直接放进脚本 `OnRender`，会触发嵌套调用错误。要做调试窗口，前面已经讲过 ImGui；要显示短消息，用本章的 `SendMessage`。

## 运行后看什么

执行：

```text
res message
```

游戏里应当出现一行短消息，日志里有：

```text
Res message sent
```

执行：

```text
res load
```

正常结果示例：

```text
Res load ok: success=true count=21 main=none
```

执行：

```text
res missing
```

应当在加载前被拦住：

```text
Res missing path blocked before LoadObject: 3D Entities\PH\Missing_Tutorial_Object.nmo
```

如果 `res load` 也提示文件不存在，先检查相对路径是否写对，再检查脚本是不是在正确的游戏目录里运行。

## 这一章的边界

本章只讲三件事：

```text
资源加载前先检查路径
LoadObject 之后看对象数量
短消息用 BML::UI::SendMessage
```

还没有进入这些内容：

```text
把加载出的对象加入场景
移动加载出的对象
给加载出的对象做物理化
创建和管理完整 2D 文本 UI
封装自己的脚本库
```

下一章会进入行为调用和脚本库组织。那时再处理“创建出来的行为归谁管、什么时候销毁、怎样封装成可复用函数”。

