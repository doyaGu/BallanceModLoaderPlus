# 参考 B：游戏事件和关卡时机

参考 A 对应脚本对象的生命周期：

```text
OnLoad
OnProcess
OnUnload
```

本参考说明 Ballance 游戏流程里的时机：

```text
开始菜单
关卡加载
关卡开始
暂停和恢复
死亡
退出关卡
```

BML 把这些时机转成 `OnGameEvent` 回调。

## 生命周期和游戏事件的区别

先分清两件事：

```text
脚本加载了没有
游戏走到哪一步了
```

`OnLoad` 只能说明脚本已经加载。这个时候通常还不能认为某个关卡对象已经准备好。

`OnGameEvent` 才用来判断游戏流程。例如：

```text
GAME_EVENT_PRE_START_MENU    准备进入开始菜单
GAME_EVENT_POST_START_MENU   开始菜单已经进入
GAME_EVENT_PRE_LOAD_LEVEL    准备加载关卡
GAME_EVENT_POST_LOAD_LEVEL   关卡加载后
GAME_EVENT_START_LEVEL       关卡开始运行
GAME_EVENT_PRE_EXIT_LEVEL    准备退出关卡
GAME_EVENT_POST_EXIT_LEVEL   退出关卡后
```

查关卡对象、读运行时 DataArray、扫描行为图，通常等到 `GAME_EVENT_START_LEVEL`。

## 用一个小脚本观察

新建脚本：

```text
ModLoader/Mods/EventMod.mod.as
```

写入：

```angelscript
[bml.mod id="event.script" name="Event Demo" version="1.0.0" author="Tutorial" bml="0.3.12" description="Observe BML game events"]
class EventMod {
    private int eventCount = 0;
    private int levelStartCount = 0;
    private bool levelReady = false;
    private string lastEvent = "none";

    void OnLoad(const BML::ModContext &in ctx) {
        LogInfo(ctx, "EventMod OnLoad");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        eventCount++;
        lastEvent = BML::GetGameEventName(event);

        LogInfo(ctx, "EventMod event " + eventCount + ": " + lastEvent);

        if (event == BML::GAME_EVENT_START_LEVEL) {
            levelReady = true;
            levelStartCount++;
            LogInfo(ctx, "EventMod level ready count=" + levelStartCount);
        }

        if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL ||
            event == BML::GAME_EVENT_POST_EXIT_LEVEL ||
            event == BML::GAME_EVENT_EXIT_GAME) {
            ClearLevelState(ctx, lastEvent);
        }
    }

    void OnUnload(const BML::ModContext &in ctx) {
        LogInfo(ctx, "EventMod OnUnload lastEvent=" + lastEvent +
                     " levelReady=" + BoolText(levelReady));
    }

    private void ClearLevelState(const BML::ModContext &in ctx, const string &in reason) {
        if (!levelReady) {
            return;
        }

        levelReady = false;
        LogInfo(ctx, "EventMod clear level state: " + reason);
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

这份脚本只做三件事：

```text
记录收到的事件顺序
在 GAME_EVENT_START_LEVEL 标记关卡已经开始
退出关卡或退出游戏时清掉关卡状态
```

## `OnGameEvent` 的签名

固定签名是：

```angelscript
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event)
```

第二个参数是 `BML::GameEvent`。

日志里要显示事件名，用：

```angelscript
BML::GetGameEventName(event)
```

判断某个事件时，直接比较枚举值：

```angelscript
if (event == BML::GAME_EVENT_START_LEVEL) {
}
```

不要只靠字符串比较：

```angelscript
if (BML::GetGameEventName(event) == "GAME_EVENT_START_LEVEL") {
}
```

字符串适合写日志。流程判断用枚举值更直接。

## 菜单事件

启动 Player 到开始菜单时，通常会看到：

```text
[ModLoader/INFO]: Loading Mod event.script[Event Demo] v1.0.0 by Tutorial
[event.script/INFO]: EventMod OnLoad
[ModLoader/INFO]: BML script mod summary: loaded=1 failed=0
[event.script/INFO]: EventMod event 1: GAME_EVENT_PRE_START_MENU
[event.script/INFO]: EventMod event 2: GAME_EVENT_POST_START_MENU
[event.script/INFO]: EventMod OnUnload lastEvent=GAME_EVENT_POST_START_MENU levelReady=false
```

这两个事件说明游戏已经进入开始菜单流程。

它们不代表关卡对象已经存在。这个时候不要急着查某个关卡里的球、机关或 DataArray。

如果同时安装了其他脚本，`loaded=1` 里的数字会变大。重点看 `event.script` 这几行。

## 关卡加载事件

进入关卡时，会按大致顺序收到：

```text
GAME_EVENT_PRE_LOAD_LEVEL
GAME_EVENT_POST_LOAD_LEVEL
GAME_EVENT_START_LEVEL
```

先按这个模型理解：

```text
PRE_LOAD_LEVEL     准备加载关卡
POST_LOAD_LEVEL    关卡文件加载后
START_LEVEL        关卡开始运行
```

脚本要读关卡运行时对象，先从 `GAME_EVENT_START_LEVEL` 开始。


```angelscript
if (event == BML::GAME_EVENT_START_LEVEL) {
    // 查对象、读 DataArray、扫描行为图
}
```

## 为什么不在 `OnLoad` 里查关卡对象

`OnLoad` 只说明脚本加载了。

此时可能还在开始菜单，也可能还没加载任何关卡。直接写：

```angelscript
CK3dEntity@ ball = ctx.Borrow3dEntityByName("Ball");
```

很容易拿到 `null`。

等到 `GAME_EVENT_START_LEVEL`，关卡内容和运行时表更接近可用状态。读取对象、组、DataArray 时，扫描入口通常接到这里。

## 为什么退出关卡要清状态

脚本里可能会保存一些关卡状态：

```text
当前关卡是否已经开始
上次扫描到的对象名
上次读取到的 DataArray 信息
某些对象引用或运行时编号
```

退出关卡后，这些状态可能已经过期。

所以脚本在退出关卡或退出游戏时清掉状态：

```angelscript
if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL ||
    event == BML::GAME_EVENT_POST_EXIT_LEVEL ||
    event == BML::GAME_EVENT_EXIT_GAME) {
    ClearLevelState(ctx, lastEvent);
}
```

这里的 `levelReady` 只是一个布尔值。如果脚本还保存了对象引用、扫描结果、表格行列信息，也按同样思路清掉。

## 常见事件速查

常用事件先按这一组查：

| 事件 | 先怎么理解 |
| --- | --- |
| `GAME_EVENT_PRE_START_MENU` | 准备进入开始菜单 |
| `GAME_EVENT_POST_START_MENU` | 开始菜单已经进入 |
| `GAME_EVENT_PRE_LOAD_LEVEL` | 准备加载关卡 |
| `GAME_EVENT_POST_LOAD_LEVEL` | 关卡加载后 |
| `GAME_EVENT_START_LEVEL` | 关卡开始运行 |
| `GAME_EVENT_PAUSE_LEVEL` | 暂停 |
| `GAME_EVENT_UNPAUSE_LEVEL` | 恢复 |
| `GAME_EVENT_DEAD` | 球死亡 |
| `GAME_EVENT_PRE_EXIT_LEVEL` | 准备退出关卡 |
| `GAME_EVENT_POST_EXIT_LEVEL` | 退出关卡后 |
| `GAME_EVENT_EXIT_GAME` | 退出游戏 |

完整列表在：

```text
docs/bml-script-mod-api.as
```

搜索：

```text
enum GameEvent
```

## 速查结论

脚本可区分两条线：

```text
脚本生命周期
  OnLoad
  OnProcess
  OnUnload

游戏流程
  OnGameEvent
  GAME_EVENT_START_LEVEL
  GAME_EVENT_PRE_EXIT_LEVEL
```

查对象、读表、看行为图时，会反复用这个判断：

```angelscript
if (event == BML::GAME_EVENT_START_LEVEL) {
    // 从这里开始碰关卡运行时内容
}
```
