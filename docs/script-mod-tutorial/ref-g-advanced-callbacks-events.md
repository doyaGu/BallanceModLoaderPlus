# 参考 G：BML 进阶回调和事件对象

相关基础中已经讲过几个固定回调：

```text
OnLoad
OnUnload
OnProcess
OnGameEvent
```

BML 还提供一组更靠近底层流程的回调。它们能看到命令执行、配置修改、对象加载、行为脚本加载、物理化等事件。

先建立一个判断方法：

```text
先看回调发生在什么时候
再看事件对象里有什么
最后判断能不能修改
```

不要一看到事件对象里有 CK 句柄就马上改游戏对象。很多进阶回调更适合先做观察。

## 进阶回调列表

当前 BML 脚本 mod 支持这些固定签名：

```angelscript
void OnRender(const BML::ModContext &in ctx, const BML::RenderEvent &in event);
void OnCheatEnabled(const BML::ModContext &in ctx, const BML::CheatEvent &in event);
void OnLoadObject(const BML::ModContext &in ctx, const BML::LoadObjectEvent &in event);
void OnLoadScript(const BML::ModContext &in ctx, const BML::LoadScriptEvent &in event);
void OnCommandEvent(const BML::ModContext &in ctx, const BML::CommandEvent &in event);
void OnModifyConfig(const BML::ModContext &in ctx, const BML::ConfigEvent &in event);
void OnPhysicalize(const BML::ModContext &in ctx, const BML::PhysicalizeEvent &in event);
void OnUnphysicalize(const BML::ModContext &in ctx, const BML::ObjectEvent &in event);
```

签名要完全一致。函数名、参数类型、`const`、`&in` 写错，BML 就不会按这个回调调用。

## 事件对象是什么

`OnGameEvent` 的第二个参数只是一个枚举：

```angelscript
BML::GameEvent event
```

进阶回调的第二个参数通常是一个事件对象：

```angelscript
const BML::CommandEvent &in event
const BML::LoadObjectEvent &in event
const BML::PhysicalizeEvent &in event
```

事件对象可以先按“这一次事件的快照”理解。
例如命令事件里有命令名、参数、阶段；配置事件里有 mod id、分类、键名、配置项类型。

事件对象里的字符串、数字、布尔值适合记录。
`Borrow*` 返回的 CK 句柄仍然按临时借用处理。

## 用小脚本观察事件

新建脚本：

```text
ModLoader/Mods/CallbackMod.mod.as
```

写入：

```angelscript
[bml.mod id="callback.script" name="Callback Demo" version="1.0.0" author="Tutorial" bml="0.3.12" description="Inspect advanced BML callbacks"]
class CallbackMod {
    private BML::ConfigProperty@ enabledProp;
    private BML::CommandRef@ commandRef;

    private bool enabled = true;
    private bool firstRender = true;

    private int commandEvents = 0;
    private int configEvents = 0;
    private int objectLoadEvents = 0;
    private int scriptLoadEvents = 0;
    private int physicalizeEvents = 0;
    private int unphysicalizeEvents = 0;

    void OnLoad(const BML::ModContext &in ctx) {
        SetupConfig(ctx);
        RegisterCommand(ctx);

        // 用 Timer 稳定触发一次命令执行和一次配置修改。
        BML::TimerCallback@ commandTimer = BML::TimerCallback(this.OnCommandTimer);
        ctx.SetTimeout(500.0f, commandTimer, "callback-command");

        BML::TimerCallback@ configTimer = BML::TimerCallback(this.OnConfigTimer);
        ctx.SetTimeout(1000.0f, configTimer, "callback-config");

        LogInfo(ctx, "CallbackMod loaded");
    }

    void OnUnload(const BML::ModContext &in ctx) {
        LogInfo(ctx, "CallbackMod unload commandEvents=" + commandEvents +
                     " configEvents=" + configEvents +
                     " objectLoads=" + objectLoadEvents +
                     " scriptLoads=" + scriptLoadEvents +
                     " physicalize=" + physicalizeEvents +
                     " unphysicalize=" + unphysicalizeEvents);
    }

    void OnRender(const BML::ModContext &in ctx, const BML::RenderEvent &in event) {
        if (!firstRender) {
            return;
        }

        firstRender = false;
        LogInfo(ctx, "Callback render flags=" + event.Flags);
    }

    void OnCommandEvent(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        if (event.CommandName != "cb") {
            return;
        }

        commandEvents++;
        LogInfo(ctx, "Callback command event phase=" + CommandPhaseText(event.Phase) +
                     " args=" + event.ArgsText +
                     " count=" + event.ArgCount);
    }

    void OnModifyConfig(const BML::ModContext &in ctx, const BML::ConfigEvent &in event) {
        if (event.ModId != ctx.GetModId()) {
            return;
        }

        configEvents++;
        BML::ConfigProperty@ prop = event.BorrowProperty();
        bool hasBorrowedProp = prop !is null && prop.IsValid;

        LogInfo(ctx, "Callback config event " + event.Category + "." + event.Key +
                     " type=" + ConfigTypeText(event.Type) +
                     " hasProperty=" + BoolText(event.HasProperty) +
                     " borrowed=" + BoolText(hasBorrowedProp));
    }

    void OnLoadObject(const BML::ModContext &in ctx, const BML::LoadObjectEvent &in event) {
        objectLoadEvents++;
        if (objectLoadEvents > 3) {
            return;
        }

        LogInfo(ctx, "Callback load object file=" + event.Filename +
                     " isMap=" + BoolText(event.IsMap) +
                     " objects=" + event.ObjectCount);
    }

    void OnLoadScript(const BML::ModContext &in ctx, const BML::LoadScriptEvent &in event) {
        scriptLoadEvents++;
        if (scriptLoadEvents > 3) {
            return;
        }

        LogInfo(ctx, "Callback load script file=" + event.Filename +
                     " scriptId=" + event.ScriptId);
    }

    void OnCheatEnabled(const BML::ModContext &in ctx, const BML::CheatEvent &in event) {
        LogInfo(ctx, "Callback cheat enabled=" + BoolText(event.Enabled));
    }

    void OnPhysicalize(const BML::ModContext &in ctx, const BML::PhysicalizeEvent &in event) {
        physicalizeEvents++;
        if (physicalizeEvents <= 3) {
            LogInfo(ctx, "Callback physicalize target=" + event.TargetName +
                         " mass=" + event.Mass);
        }
    }

    void OnUnphysicalize(const BML::ModContext &in ctx, const BML::ObjectEvent &in event) {
        unphysicalizeEvents++;
        if (unphysicalizeEvents <= 3) {
            LogInfo(ctx, "Callback unphysicalize target=" + event.TargetName);
        }
    }

    private void SetupConfig(const BML::ModContext &in ctx) {
        BML::Config@ config = ctx.BorrowConfig();
        if (config is null) {
            LogInfo(ctx, "Callback config unavailable");
            return;
        }

        @enabledProp = config.GetProperty("Callback", "Enabled");
        if (enabledProp is null) {
            LogInfo(ctx, "Callback Enabled property unavailable");
            return;
        }

        enabledProp.SetDefaultBoolean(true);
        enabledProp.SetComment("Enable CallbackMod test state.");
        enabled = enabledProp.GetBoolean(true);
    }

    private void RegisterCommand(const BML::ModContext &in ctx) {
        BML::CommandDefinition def;
        def.Name = "cb";
        def.Description = "CallbackMod status";
        def.Usage = "cb status";
        def.Category = "Tutorial";
        def.Enabled = true;

        BML::CommandCallback@ execute = BML::CommandCallback(this.OnCbCommand);
        BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompleteCbCommand);

        @commandRef = ctx.RegisterCommand(def, execute, complete);
        bool valid = commandRef !is null && commandRef.IsValid;
        LogInfo(ctx, "Callback command registered=" + BoolText(valid));
    }

    void OnCbCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        LogInfo(ctx, "Callback command execute args=" + event.ArgsText +
                     " enabled=" + BoolText(enabled));
    }

    void CompleteCbCommand(const BML::ModContext &in ctx,
                           const BML::CommandEvent &in event,
                           BML::CommandCompletion &inout completions) {
        completions.Add("status");
        completions.Add("toggle");
    }

    void OnCommandTimer(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
        ctx.ExecuteCommand("cb status");
    }

    void OnConfigTimer(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
        if (enabledProp is null || !enabledProp.IsValid) {
            return;
        }

        enabled = !enabled;
        enabledProp.SetBoolean(enabled);
        LogInfo(ctx, "Callback config timer set enabled=" + BoolText(enabled));
    }

    private string CommandPhaseText(BML::CommandEventPhase phase) {
        if (phase == BML::COMMAND_EVENT_PRE) {
            return "pre";
        }
        if (phase == BML::COMMAND_EVENT_POST) {
            return "post";
        }
        if (phase == BML::COMMAND_EVENT_EXECUTE) {
            return "execute";
        }
        if (phase == BML::COMMAND_EVENT_COMPLETE) {
            return "complete";
        }
        return "unknown";
    }

    private string ConfigTypeText(BML::ConfigPropertyType type) {
        if (type == BML::CONFIG_PROPERTY_BOOLEAN) {
            return "boolean";
        }
        if (type == BML::CONFIG_PROPERTY_STRING) {
            return "string";
        }
        if (type == BML::CONFIG_PROPERTY_INTEGER) {
            return "integer";
        }
        if (type == BML::CONFIG_PROPERTY_FLOAT) {
            return "float";
        }
        if (type == BML::CONFIG_PROPERTY_KEY) {
            return "key";
        }
        return "none";
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

这份脚本只负责把事件顺序打出来。

## 命令事件和命令执行回调

脚本里有两个和命令有关的入口：

```angelscript
void OnCommandEvent(const BML::ModContext &in ctx, const BML::CommandEvent &in event)
void OnCbCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event)
```

`OnCbCommand` 是 `cb` 命令自己的执行函数。
`OnCommandEvent` 是 BML 通知脚本“某个命令进入执行流程”的事件。

执行：

```angelscript
ctx.ExecuteCommand("cb status");
```

会看到 `pre`、`execute`、`post` 这几类信息：

```text
pre      命令执行前
execute  命令自己的执行回调
post     命令执行后
```

补全函数对应 `complete` 阶段。它通常由命令栏补全触发，本参考的 Timer 执行命令不会触发补全。

## 配置事件

脚本启动时创建配置项：

```angelscript
@enabledProp = config.GetProperty("Callback", "Enabled");
enabledProp.SetDefaultBoolean(true);
enabled = enabledProp.GetBoolean(true);
```

Timer 到时间后修改它：

```angelscript
enabled = !enabled;
enabledProp.SetBoolean(enabled);
```

这会触发：

```angelscript
void OnModifyConfig(const BML::ModContext &in ctx, const BML::ConfigEvent &in event)
```

配置事件里常用字段：

| 字段 | 含义 |
| --- | --- |
| `event.ModId` | 哪个 mod 的配置变化 |
| `event.Category` | 配置分类 |
| `event.Key` | 配置项 |
| `event.Type` | 配置值类型 |
| `event.HasProperty` | 事件里是否有配置项 |
| `event.BorrowProperty()` | 借到配置项包装 |

在配置事件里可以读新值，但不要在同一个事件里反复写同一个配置项。否则逻辑会变得很难跟踪。

## 对象加载和脚本加载事件

`OnLoadObject` 观察 Object Load：

```angelscript
void OnLoadObject(const BML::ModContext &in ctx, const BML::LoadObjectEvent &in event)
```

常用字段：

| 字段 | 含义 |
| --- | --- |
| `event.Filename` | 来源文件名 |
| `event.IsMap` | 是否按地图加载 |
| `event.ObjectCount` | 本次加载出的对象数量 |
| `event.GetObjectId(index)` | 某个对象 id |
| `event.BorrowObject(index)` | 临时借用某个对象 |

`OnLoadScript` 观察 Virtools 行为脚本加载：

```angelscript
void OnLoadScript(const BML::ModContext &in ctx, const BML::LoadScriptEvent &in event)
```

常用字段：

| 字段 | 含义 |
| --- | --- |
| `event.Filename` | 来源文件名 |
| `event.ScriptId` | 行为脚本对象 id |
| `event.BorrowScript()` | 临时借用 `CKBehavior@` |

这里的 `BorrowObject`、`BorrowScript` 仍然是临时借用。需要长期记住对象时，先保存 id、名字，或使用参考 L 的引用包装。

## 物理化事件

`OnPhysicalize` 在对象创建物理表示时触发：

```angelscript
void OnPhysicalize(const BML::ModContext &in ctx, const BML::PhysicalizeEvent &in event)
```

这个事件里能看到目标名、质量、摩擦、弹性、碰撞组、球形/凸包/凹包数量等信息。它已经很接近游戏物理流程。

本参考只建议记录：

```angelscript
event.TargetName
event.Mass
event.Friction
event.Elasticity
```

不要在第一版脚本里直接对当前活动球施力或改物理参数。物理修改同样要先考虑边界和回滚。

`OnUnphysicalize` 对应物理表示移除：

```angelscript
void OnUnphysicalize(const BML::ModContext &in ctx, const BML::ObjectEvent &in event)
```

常用的是目标 id 和目标名。

## 运行后看日志

保存脚本，重启 Player。

菜单阶段可以看到类似结果：

```text
[ModLoader/INFO]: Loading Mod callback.script[Callback Demo] v1.0.0 by Tutorial
[callback.script/INFO]: Callback command registered=true
[callback.script/INFO]: CallbackMod loaded
[ModLoader/INFO]: BML script mod summary: loaded=1 failed=0
[callback.script/INFO]: Callback load object file=base.cmo isMap=false objects=0
[callback.script/INFO]: Callback load script file=base.cmo scriptId=87
[callback.script/INFO]: Callback load script file=base.cmo scriptId=156
[callback.script/INFO]: Callback load script file=base.cmo scriptId=370
[callback.script/INFO]: Callback render flags=65791
[callback.script/INFO]: Callback load object file=3D Entities\Language.nmo isMap=false objects=3
[callback.script/INFO]: Callback load object file=3D Entities\Sound.nmo isMap=false objects=47
[callback.script/INFO]: Callback physicalize target=Ball_Paper_piece01 mass=0.2
[callback.script/INFO]: Callback physicalize target=Ball_Paper_piece02 mass=0.2
[callback.script/INFO]: Callback physicalize target=Ball_Paper_piece03 mass=0.2
[callback.script/INFO]: Callback command event phase=pre args=status count=1
[callback.script/INFO]: Callback command execute args=status enabled=true
[callback.script/INFO]: Callback command event phase=post args=status count=1
[callback.script/INFO]: Callback unphysicalize target=Ball_Paper_piece01
[callback.script/INFO]: Callback unphysicalize target=Ball_Paper_piece02
[callback.script/INFO]: Callback unphysicalize target=Ball_Paper_piece03
[callback.script/INFO]: Callback config event Callback.Enabled type=boolean hasProperty=true borrowed=true
[callback.script/INFO]: Callback config timer set enabled=false
```

退出 Player 时会看到计数汇总：

```text
[callback.script/INFO]: CallbackMod unload commandEvents=2 configEvents=1 objectLoads=10 scriptLoads=63 physicalize=51 unphysicalize=51
```

不同启动阶段会影响 `objectLoads`、`scriptLoads`、`physicalize` 的数量。
这次测试只停在菜单，也触发了纸球碎片相关的物理化日志。进入关卡后，物理化相关日志通常会更多。

## 哪些回调先观察

可以先按风险分：

| 回调 | 入门阶段建议 |
| --- | --- |
| `OnCommandEvent` | 记录命令名、参数、阶段 |
| `OnModifyConfig` | 记录变化的分类和键名，必要时同步脚本成员变量 |
| `OnLoadObject` | 记录文件名、对象数量、对象 id |
| `OnLoadScript` | 记录脚本 id 和来源文件 |
| `OnRender` | 记录少量渲染状态，不刷日志 |
| `OnCheatEnabled` | 同步脚本自己的开关状态 |
| `OnPhysicalize` | 先记录目标和参数 |
| `OnUnphysicalize` | 先记录目标 |

容易出问题的做法：

```text
在高频回调里每次都写日志
在对象加载事件里立即移动或删除对象
把 BorrowObject / BorrowScript 返回的句柄长期保存
在配置事件里反复写同一个配置项
在物理化事件里直接改当前活动球
```

## 速查结论

可将进阶回调看成一组观察窗口：

```text
命令事件看命令流程
配置事件看配置变化
加载事件看对象和行为脚本进入运行时
物理事件看物理表示创建和移除
事件对象保存快照
Borrow* 句柄仍然临时使用
```
