# 第 10 章：怎么查脚本 API

前面几章一直在用 `ctx`：

```angelscript
ctx.BorrowLogger();
ctx.RegisterCommand(def, execute, complete);
ctx.SetTimeout(1000.0f, callback, "hello-command");
```

后面还会开始接触 Ballance 关卡对象：

```angelscript
ctx.Borrow3dEntityByName("Ball");
ctx.BorrowDataArrayByName("Level_Init");
CK::GetName(object);
CK::GetRowCount(array);
```

这两类代码要查的地方不同。

## 先分清 API 来源

教程里主要会碰到三层 API：

| 层 | 查什么 | 主要资料 |
| --- | --- | --- |
| BML 脚本 mod API | mod 加载、日志、ImGui、命令、配置、Timer、BML 提供的游戏入口 | `docs/bml-script-mod-api.as` |
| CKAS 高层脚本 API | 场景对象、行为图、BB、参数、消息、异步任务 | `CKAngelScript/docs/api-inventory.md` 和相关专题文档 |
| CK/Virtools 原始绑定 | `CKContext`、`CKObject`、`CK3dEntity`、`CKDataArray`、`VxVector` 等 SDK 类型 | `CKAngelScript/docs/sdk-bindings.md`，必要时看 `CKAngelScript/src/Script*.cpp` |

第一篇写的命令、配置、Timer，主要查 BML。  
后面开始找对象、读 DataArray、看行为图，就要把 CKAS 和 CK/Virtools 绑定一起看。

## BML API 查什么

BMLPlus 仓库里有当前脚本 mod 的绑定文件：

```text
docs/bml-script-mod-api.as
```

它告诉脚本：

```text
BML::ModContext 有哪些函数
BML::Logger 怎么写日志
BML::CommandDefinition 有哪些字段
BML::TimerCallback 的签名是什么
BML 提供了哪些 CK 辅助函数
```

例如打开这个文件，可以看到：

```angelscript
class ModContext {
  Logger@ BorrowLogger() const;
  Config@ BorrowConfig() const;
  CKContext@ BorrowCKContext() const;
  CKDataArray@ BorrowDataArrayByName(const string &in name) const;
  CK3dEntity@ Borrow3dEntityByName(const string &in name) const;

  CommandRef@ RegisterCommand(const CommandDefinition &in definition,
                              CommandCallback@+ execute,
                              CommandCompletionCallback@+ complete = null) const;
  TimerRef@ SetTimeout(float delayMs,
                       TimerCallback@+ callback,
                       const string &in name = "") const;
}
```

这说明 `ctx` 不只是日志和命令入口。它也是脚本进入 Virtools 运行时的入口。

再往上看，还能看到 BML 提供的 `CK` 辅助命名空间：

```angelscript
namespace CK {
  string GetName(CKObject@ object);
  VxVector GetPosition(CK3dEntity@ entity);
  int GetRowCount(CKDataArray@ array);
  string GetString(CKDataArray@ array, int row, int column, const string &in defaultValue = "");
}
```

所以 BML 脚本里后面会出现这种写法：

```angelscript
CKDataArray@ array = ctx.BorrowDataArrayByName("Level_Init");
if (array !is null) {
    int rows = CK::GetRowCount(array);
}
```

这段代码的来源不是猜出来的：

```text
ctx.BorrowDataArrayByName     来自 BML::ModContext
CK::GetRowCount               来自 namespace CK
CKDataArray@                  来自 CK/Virtools 对象类型
```

## CKAS API 查什么

CKAS 是把 AngelScript 接进 Virtools/CK2 运行时的那一层。它不只提供原始 CK 类型，还提供更适合写脚本的高层 API。

先看总图：

```text
CKAngelScript/docs/api-inventory.md
```

里面把 CKAS API 分成几组：

| API 家族 | 用途 |
| --- | --- |
| `Scene` | 查找、保存、创建、销毁场景对象 |
| `Behavior` | 查找和遍历行为图 |
| `BB` | 查 Building Block 原型，创建和驱动 BB |
| `Param` | 参数类型、参数值、枚举、flags、参数转换 |
| `Message` | 脚本之间发送消息 |
| `Async` | 帧驱动的异步任务 |
| 原始 CK/Vx 绑定 | 直接操作 `CKObject`、`CK3dEntity`、`CKBehavior`、`VxVector` 等 |

查对象、行为图和参数时，先看这些高层 API。它们通常比直接拿原始 `CKObject@` 更适合长期保存状态。

例如 CKAS 的场景 API 会有这种脚本声明：

```angelscript
Entity3DRef@ FindEntity3D(CKContext@ ctx,
                          const string &in name = "",
                          int occurrence = 0,
                          bool currentSceneOnly = false)
```

这和 BML 里的：

```angelscript
CK3dEntity@ Borrow3dEntityByName(const string &in name) const;
```

目标都和“找 3D 对象”有关，但模型不一样：

| 写法 | 返回值 | 更适合 |
| --- | --- | --- |
| `ctx.Borrow3dEntityByName(...)` | `CK3dEntity@` | 立刻读取或操作对象 |
| `Scene::FindEntity3D(...)` | `Entity3DRef@` | 保存对象身份，后面再重新取对象 |

教程前面先用 BML 的 `Borrow...ByName`，因为它直接、短。后面讲对象生命周期和行为图时，会逐步引入 CKAS 的 `Scene`、`Behavior`、`BB`、`Param`。

## CK/Virtools 原始绑定查什么

CKAS 还绑定了大量 Virtools SDK 类型。查这些时看：

```text
CKAngelScript/docs/sdk-bindings.md
```

它会告诉你哪些区域已经绑定：

```text
VxVector、VxMatrix、VxColor
CK_ID、CKGUID、XObjectArray
CKContext、CKObject、CK3dEntity、CKBehavior
CKDataArray、CKParameter、CKScene
CK 枚举、flags、结构体
```

原始绑定的名字通常接近 Virtools SDK。它们适合最后一步操作，例如移动对象、读取对象名、读取 DataArray 单元格。

如果文档里没有完整声明，就看 CKAS 源码里的注册字符串：

```text
CKAngelScript/src/ScriptCKObjects.cpp
CKAngelScript/src/ScriptCKContext.cpp
CKAngelScript/src/ScriptScene.cpp
CKAngelScript/src/ScriptBridgeGraph.cpp
CKAngelScript/src/ScriptParameterRegistry.cpp
```

源码里这种字符串就是脚本能看到的声明：

```cpp
RegisterGlobalFunction("Entity3DRef@ FindEntity3D(CKContext@ ctx, const string &in name = \"\", int occurrence = 0, bool currentSceneOnly = false)", ...);
```

写脚本时看字符串里的 AngelScript 声明，不要按 C++ 函数名猜。

## Virtools SDK 文档怎么看

Virtools SDK 文档适合解释概念：

```text
CKObject 是什么
CK3dEntity 和 CK3dObject 有什么关系
CKDataArray 的行列模型
CKBehavior 行为图对象的基本结构
CKContext 为什么到处都需要
```

但写脚本时，函数签名以 CKAS/BML 绑定为准。

例如 Virtools SDK 里可能有一个 C++ 方法。脚本里能不能调用、名字是不是一样、参数是不是一样，要看 CKAS 注册出来的 AngelScript 声明。

顺序可以这样记：

```text
想理解 Virtools 概念       看 Virtools SDK 文档
想知道脚本能不能调用       看 CKAS/BML 绑定
想确认脚本参数怎么写       看 .as 声明或 Register* 字符串
```

## 一个 BML 例子：命令

看这一行：

```angelscript
CommandRef@ RegisterCommand(const CommandDefinition &in definition,
                            CommandCallback@+ execute,
                            CommandCompletionCallback@+ complete = null) const;
```

从左到右拆：

```text
CommandRef@                         返回命令引用
RegisterCommand                     函数名
const CommandDefinition &in          第一个参数是命令定义
CommandCallback@+                    第二个参数是执行回调
CommandCompletionCallback@+ = null    第三个参数是补全回调，可以省略
```

所以第 6 章写成：

```angelscript
BML::CommandDefinition def;
def.Name = "hello";
def.Alias = "h";

BML::CommandCallback@ execute = BML::CommandCallback(this.OnHelloCommand);
BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompleteHelloCommand);

@helloCommand = ctx.RegisterCommand(def, execute, complete);
```

函数属于 `ModContext`，所以用 `ctx.RegisterCommand(...)`。

## 一个 CKAS 思路：找对象

后面如果要找游戏里的 3D 对象，先问两个问题。

第一，当前脚本环境手里有什么上下文？

```text
BML 脚本 mod 回调里有 BML::ModContext，也就是 ctx
CKAS 组件脚本里可能有 CKBehaviorContext 或 ScriptContext
```

第二，要立刻操作，还是要保存对象身份？

立刻操作可以从 BML 入口拿原始对象：

```angelscript
CK3dEntity@ entity = ctx.Borrow3dEntityByName("Ball");
if (entity !is null) {
    VxVector position = CK::GetPosition(entity);
}
```

需要保存身份时，优先看 CKAS 的 `Scene` API：

```angelscript
Entity3DRef@ ref = Scene::FindEntity3D(ckContext, "Ball");
```

`CK3dEntity@` 是原始对象句柄。对象消失或场景切换后，长期保存它风险更高。  
`Entity3DRef@` 是 CKAS 的对象引用层，更适合跨多次调用保存对象身份。

这里先建立判断方法。后面讲关卡对象时再写完整示例。

## `@` 先这样理解

AngelScript 里的 `@` 表示对象句柄。入门阶段可以先把它理解成“指向对象的引用”。

例如：

```angelscript
BML::Logger@ logger = ctx.BorrowLogger();
BML::CommandRef@ helloCommand;
BML::TimerRef@ statusTimer;
CKDataArray@ array = ctx.BorrowDataArrayByName("Level_Init");
CK3dEntity@ entity = ctx.Borrow3dEntityByName("Ball");
```

这些变量都可能为空。使用前先检查：

```angelscript
if (entity !is null) {
    string name = CK::GetName(entity);
}
```

判断句柄是否为空，用：

```angelscript
entity is null
entity !is null
```

不要写成：

```angelscript
entity == null
```

## `&in`、`&inout` 先这样理解

API 声明里经常出现：

```text
const X &in
X &inout
```

先按下面理解：

| 写法 | 含义 |
| --- | --- |
| `const X &in` | 把对象传进函数，脚本只使用它 |
| `X &inout` | 把对象传进函数，脚本可以往里面写内容 |

命令执行回调里有：

```angelscript
const BML::CommandEvent &in event
```

脚本读取命令参数：

```angelscript
int count = event.ArgCount;
string first = event.GetArg(0);
```

命令补全回调里有：

```angelscript
BML::CommandCompletion &inout completions
```

脚本可以往补全列表里加内容：

```angelscript
completions.Add("status");
completions.Add("toggle");
completions.Add("messages");
```

## 回调签名照抄

API 文件里有 `funcdef`：

```angelscript
funcdef void CommandCallback(const BML::ModContext &in ctx,
                             const BML::CommandEvent &in event);
```

`funcdef` 是回调函数的形状。脚本里的函数要照这个形状写：

```angelscript
private void OnHelloCommand(const BML::ModContext &in ctx,
                            const BML::CommandEvent &in event) {
}
```

函数名可以自己取。返回值和参数要对。

Timer 也是同一个规则：

```angelscript
funcdef void TimerCallback(const BML::ModContext &in ctx,
                           const BML::TimerEvent &in event);

funcdef bool TimerLoopCallback(const BML::ModContext &in ctx,
                               const BML::TimerEvent &in event);
```

脚本里分别写：

```angelscript
private void OnCommandTimer(const BML::ModContext &in ctx,
                            const BML::TimerEvent &in event) {
}

private bool OnStatusTimer(const BML::ModContext &in ctx,
                           const BML::TimerEvent &in event) {
    return true;
}
```

一个返回 `void`，一个返回 `bool`。这里不能互换。

## 属性从 `get_` 来

API 文件里会看到：

```angelscript
class CommandRef {
  string get_Name() const;
  bool get_IsValid() const;
}

class CommandEvent {
  int get_ArgCount() const;
}
```

脚本里通常写成属性：

```angelscript
helloCommand.Name
helloCommand.IsValid
event.ArgCount
```

看到 `get_Name()`，脚本里就可以找 `.Name`。  
看到 `get_IsValid()`，脚本里就可以找 `.IsValid`。

这个规则也会出现在 CKAS 的对象引用、参数引用、行为图引用里。

## 默认参数怎么看

有些函数声明里带默认值：

```angelscript
CommandRef@ RegisterCommand(const CommandDefinition &in definition,
                            CommandCallback@+ execute,
                            CommandCompletionCallback@+ complete = null) const;
```

`complete = null` 表示第三个参数可以省略：

```angelscript
@helloCommand = ctx.RegisterCommand(def, execute);
```

省略后就没有补全回调。第 6 章为了展示补全，写了三个参数：

```angelscript
@helloCommand = ctx.RegisterCommand(def, execute, complete);
```

Timer 的名字也有默认值：

```angelscript
TimerRef@ SetTimeout(float delayMs,
                     TimerCallback@+ callback,
                     const string &in name = "") const;
```

教程里传名字，是为了日志里更容易看：

```angelscript
ctx.SetTimeout(1000.0f, callback, "hello-command");
```

CKAS 的查找函数也常有默认参数：

```angelscript
FindEntity3D(CKContext@ ctx,
             const string &in name = "",
             int occurrence = 0,
             bool currentSceneOnly = false)
```

只传 `ctx` 和名字时，后面的参数会使用默认值。

## 找不到函数时怎么查

按这个顺序查：

```text
1. 先判断要查 BML、CKAS 高层 API，还是 CK/Virtools 原始绑定
2. 搜关键词
3. 看函数写在哪个 class 或 namespace 里
4. 看手里有没有这个对象或上下文
5. 拆返回值和参数
6. 照 funcdef 写回调
```

例如想执行命令，搜：

```text
ExecuteCommand
```

看到：

```angelscript
class ModContext {
  void ExecuteCommand(const string &in command) const;
}
```

它属于 `ModContext`，所以代码写成：

```angelscript
ctx.ExecuteCommand("hello status");
```

例如想读 DataArray，先搜：

```text
BorrowDataArrayByName
GetRowCount
GetString
```

看到：

```angelscript
CKDataArray@ BorrowDataArrayByName(const string &in name) const;
int GetRowCount(CKDataArray@ array);
string GetString(CKDataArray@ array, int row, int column, const string &in defaultValue = "");
```

代码就按这个关系写：

```angelscript
CKDataArray@ array = ctx.BorrowDataArrayByName("Level_Init");
if (array !is null) {
    int rows = CK::GetRowCount(array);
}
```

## API 和教程不一致时

以当前绑定为准。

BML 脚本 mod 代码先看：

```text
docs/bml-script-mod-api.as
```

CKAS 高层 API 先看：

```text
CKAngelScript/docs/api-inventory.md
CKAngelScript/docs/sdk-bindings.md
```

需要精确声明时，看对应源码里的注册字符串：

```text
CKAngelScript/src/ScriptScene.cpp
CKAngelScript/src/ScriptBridgeGraph.cpp
CKAngelScript/src/ScriptParameterRegistry.cpp
CKAngelScript/src/ScriptCKObjects.cpp
CKAngelScript/src/ScriptCKContext.cpp
```

Ballance 自己导出的参数类型和 Building Block 名称，可以看：

```text
CKAngelScript/docs/catalog-ballance.md
```

不要自己改函数名来试。比如绑定文件写的是：

```angelscript
Logger@ BorrowLogger() const;
```

就写：

```angelscript
ctx.BorrowLogger();
```

不要猜：

```angelscript
ctx.GetLogger();
ctx.Logger();
ctx.log();
```

## 本章结果

现在已经知道 API 该按来源查：

```text
BML 脚本 mod API       查 docs/bml-script-mod-api.as
CKAS 高层 API          查 CKAngelScript/docs/api-inventory.md
CK/Virtools 原始绑定   查 CKAngelScript/docs/sdk-bindings.md 和注册源码
```

下一章给一份可拆的基础模板，把第一篇用过的 BML 能力放在一个脚本外壳里。
