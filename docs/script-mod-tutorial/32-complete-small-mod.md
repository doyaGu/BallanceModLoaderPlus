# 第 32 章：一个完整小 mod

这一章把前面的能力收成一个完整小 mod。

功能范围先控制住：

```text
有配置
有命令
有 ImGui 窗口
能查看游戏状态
能创建自己的可控对象
能隐藏和清理可控对象
关卡切换、退出、卸载时会清理
```

这个 mod 叫 `Marker Mod`。它不会写原版 DataArray，不会改 Gameplay，不会移动当前球。修改范围只落在自己加载出来的可控对象上。

## 功能设计

先写需求，不急着写代码：

| 功能 | 说明 |
| --- | --- |
| 配置 | 保存窗口开关、自动创建开关、消息开关 |
| 命令 | `marker status/create/show/hide/cleanup/window/auto/messages` |
| UI | 一个 ImGui 面板，显示状态和按钮 |
| 查看状态 | 显示 `IsInGame`、`IsInLevel`、`IsPlaying` |
| 小修改 | 加载一个可控对象，显示、物理化、隐藏 |
| 回滚 | `cleanup`、关卡切换、卸载时取消物理化并隐藏 |

这就是一个小 mod 的基本骨架：能打开、能操作、能关闭、能清理。

## 先看代码骨架

完整代码比较长，先看它的形状：

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    LoadConfig(ctx);
    RegisterCommand(ctx);
    if (autoCreate) {
        CreateMarker(ctx, "load");
    }
}

void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
    if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL ||
        event == BML::GAME_EVENT_PRE_LOAD_LEVEL ||
        event == BML::GAME_EVENT_EXIT_GAME) {
        CleanupMarker(ctx, "game-event");
    }
}

void OnProcess(const BML::ModContext &in ctx) {
    if (showWindow) {
        DrawWindow(ctx);
    }
}

void OnUnload(const BML::ModContext &in ctx) {
    CleanupMarker(ctx, "unload");
    if (commandRef !is null && commandRef.IsValid) {
        commandRef.Unregister();
    }
}
```

这就是完整小 mod 的主干：

| 位置 | 负责什么 |
| --- | --- |
| `OnLoad` | 读取配置、注册命令、按配置创建对象 |
| `OnGameEvent` | 关卡切换前清理对象 |
| `OnProcess` | 每帧画窗口 |
| `OnUnload` | 卸载前收尾 |

后面的完整代码只是把这几块填满。

## 完整代码

新建：

```text
ModLoader/Mods/MarkerMod.mod.as
```

写入：

```angelscript
[bml.mod id="marker.mod" name="Marker Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="A complete small script mod"]
class MarkerMod {
    private bool showWindow = true;
    private bool autoCreate = false;
    private bool messagesEnabled = true;

    private BML::ConfigProperty@ showWindowProp;
    private BML::ConfigProperty@ autoCreateProp;
    private BML::ConfigProperty@ messagesProp;
    private BML::CommandRef@ commandRef;

    private CK3dEntity@ marker;
    private bool markerVisible = false;
    private bool markerPhysical = false;
    private int markerId = 0;
    private string markerName = "none";
    private string lastAction = "loaded";

    void OnLoad(const BML::ModContext &in ctx) {
        LoadConfig(ctx);
        RegisterCommand(ctx);

        if (autoCreate) {
            CreateMarker(ctx, "load");
        }

        LogInfo(ctx, "Marker Mod loaded");
    }

    void OnUnload(const BML::ModContext &in ctx) {
        CleanupMarker(ctx, "unload");

        if (commandRef !is null && commandRef.IsValid) {
            commandRef.Unregister();
        }

        LogInfo(ctx, "Marker Mod unloaded");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL ||
            event == BML::GAME_EVENT_PRE_LOAD_LEVEL ||
            event == BML::GAME_EVENT_EXIT_GAME) {
            CleanupMarker(ctx, "game-event");
        }

        if (event == BML::GAME_EVENT_START_LEVEL && autoCreate && marker is null) {
            CreateMarker(ctx, "level-start");
        }
    }

    void OnProcess(const BML::ModContext &in ctx) {
        if (showWindow) {
            DrawWindow(ctx);
        }
    }

    private void LoadConfig(const BML::ModContext &in ctx) {
        BML::Config@ config = ctx.BorrowConfig();
        if (config is null) {
            LogWarn(ctx, "config unavailable");
            return;
        }

        config.SetCategoryComment("MarkerMod", "Marker Mod settings");

        @showWindowProp = config.GetProperty("MarkerMod", "ShowWindow");
        if (showWindowProp !is null) {
            showWindowProp.SetDefaultBoolean(true);
            showWindowProp.SetComment("Show the Marker Mod ImGui window.");
            showWindow = showWindowProp.GetBoolean(true);
        }

        @autoCreateProp = config.GetProperty("MarkerMod", "AutoCreate");
        if (autoCreateProp !is null) {
            autoCreateProp.SetDefaultBoolean(false);
            autoCreateProp.SetComment("Create the marker automatically when the mod loads or a level starts.");
            autoCreate = autoCreateProp.GetBoolean(false);
        }

        @messagesProp = config.GetProperty("MarkerMod", "MessagesEnabled");
        if (messagesProp !is null) {
            messagesProp.SetDefaultBoolean(true);
            messagesProp.SetComment("Show short in-game messages for Marker Mod actions.");
            messagesEnabled = messagesProp.GetBoolean(true);
        }
    }

    private void RegisterCommand(const BML::ModContext &in ctx) {
        BML::CommandDefinition def;
        def.Name = "marker";
        def.Alias = "mk";
        def.Description = "Control Marker Mod";
        def.Usage = "marker [status|create|show|hide|cleanup|window|auto|messages]";
        def.Category = "Tutorial";
        def.Enabled = true;

        BML::CommandCallback@ execute = BML::CommandCallback(this.OnMarkerCommand);
        BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompleteMarkerCommand);
        @commandRef = ctx.RegisterCommand(def, execute, complete);

        bool ok = commandRef !is null && commandRef.IsValid;
        LogInfo(ctx, "Marker command registered=" + BoolText(ok));
    }

    private void OnMarkerCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        string action = "status";
        if (event.ArgCount > 0) {
            action = event.GetArg(0);
        }

        if (action == "create") {
            CreateMarker(ctx, "command");
        } else if (action == "show") {
            ShowMarker(ctx, "command");
        } else if (action == "hide") {
            HideMarker(ctx, "command");
        } else if (action == "cleanup") {
            CleanupMarker(ctx, "command");
        } else if (action == "window") {
            SetShowWindow(ctx, !showWindow);
        } else if (action == "auto") {
            SetAutoCreate(ctx, !autoCreate);
        } else if (action == "messages") {
            SetMessagesEnabled(ctx, !messagesEnabled);
        } else {
            ReportStatus(ctx, "command");
        }
    }

    private void CompleteMarkerCommand(const BML::ModContext &in ctx,
                                       const BML::CommandEvent &in event,
                                       BML::CommandCompletion &inout completions) {
        completions.Add("status");
        completions.Add("create");
        completions.Add("show");
        completions.Add("hide");
        completions.Add("cleanup");
        completions.Add("window");
        completions.Add("auto");
        completions.Add("messages");
    }

    private void DrawWindow(const BML::ModContext &in ctx) {
        ImGui::SetNextWindowSize(ImVec2(390.0f, 250.0f), ImGuiCond_Once);

        if (ImGui::Begin("Marker Mod", showWindow)) {
            ImGui::TextUnformatted("Game: inGame=" + BoolText(ctx.GetIsInGame()) +
                                   " inLevel=" + BoolText(ctx.GetIsInLevel()) +
                                   " playing=" + BoolText(ctx.GetIsPlaying()));
            ImGui::TextUnformatted("Marker: " + markerName + " id=" + markerId);
            ImGui::TextUnformatted("Visible=" + BoolText(markerVisible) +
                                   " physical=" + BoolText(markerPhysical));
            ImGui::TextUnformatted("Last: " + lastAction);

            if (ImGui::Checkbox("Auto create", autoCreate)) {
                SaveBoolean(autoCreateProp, autoCreate);
            }
            if (ImGui::Checkbox("Messages", messagesEnabled)) {
                SaveBoolean(messagesProp, messagesEnabled);
            }

            if (ImGui::Button("Create", ImVec2(82.0f, 0.0f))) {
                CreateMarker(ctx, "ui");
            }
            ImGui::SameLine();
            if (ImGui::Button("Show", ImVec2(82.0f, 0.0f))) {
                ShowMarker(ctx, "ui");
            }
            ImGui::SameLine();
            if (ImGui::Button("Hide", ImVec2(82.0f, 0.0f))) {
                HideMarker(ctx, "ui");
            }
            ImGui::SameLine();
            if (ImGui::Button("Cleanup", ImVec2(82.0f, 0.0f))) {
                CleanupMarker(ctx, "ui");
            }
        }

        ImGui::End();
    }

    private void CreateMarker(const BML::ModContext &in ctx, const string &in reason) {
        if (marker !is null) {
            ShowMarker(ctx, reason);
            return;
        }

        string relativePath = "3D Entities\\PH\\P_Modul_01.nmo";
        string resourcePath = BML::Path::Combine(ctx.GetDirectoryUtf8(BML::DIR_GAME), relativePath);
        if (!BML::Path::IsFile(resourcePath)) {
            SetLast(ctx, "missing resource: " + relativePath);
            return;
        }

        BML::ObjectLoadOptions options;
        options.File = resourcePath;
        options.Rename = true;
        options.MasterName = "Tutorial_Marker";
        options.AddToScene = true;
        options.ReuseMeshes = true;
        options.ReuseMaterials = true;
        options.Dynamic = true;

        BML::ObjectLoadResult@ result = BML::CK::LoadObject(options);
        if (result is null || !result.Success) {
            SetLast(ctx, "load failed");
            return;
        }

        @marker = FindFirstEntity(result);
        if (marker is null) {
            SetLast(ctx, "no entity in loaded result");
            return;
        }

        markerId = BML::CK::GetId(marker);
        markerName = BML::CK::GetName(marker);
        BML::CK::SetPosition(marker, VxVector(0.0f, 8.0f, 0.0f));
        BML::CK::Show(marker, CKSHOW, true);
        markerVisible = true;

        BML::PhysicalizeDefinition physics;
        physics.Fixed = true;
        physics.Friction = 0.5f;
        physics.Elasticity = 0.1f;
        physics.Mass = 1.0f;
        physics.CollisionGroup = "Floor";
        physics.EnableCollision = true;

        markerPhysical = BML::Physics::PhysicalizeBall(marker,
                                                       physics,
                                                       VxVector(0.0f, 0.0f, 0.0f),
                                                       2.0f);

        SetLast(ctx, "created by " + reason + " physical=" + BoolText(markerPhysical));
    }

    private CK3dEntity@ FindFirstEntity(BML::ObjectLoadResult@ result) {
        for (int i = 0; i < result.Count; i++) {
            CKObject@ object = result.BorrowObject(i);
            CK3dEntity@ entity = cast<CK3dEntity>(object);
            if (entity !is null) {
                return entity;
            }
        }
        return null;
    }

    private void ShowMarker(const BML::ModContext &in ctx, const string &in reason) {
        if (marker is null) {
            SetLast(ctx, "show skipped: no marker");
            return;
        }

        BML::CK::Show(marker, CKSHOW, true);
        markerVisible = true;
        SetLast(ctx, "shown by " + reason);
    }

    private void HideMarker(const BML::ModContext &in ctx, const string &in reason) {
        if (marker is null) {
            SetLast(ctx, "hide skipped: no marker");
            return;
        }

        BML::CK::Show(marker, CKHIDE, true);
        markerVisible = false;
        SetLast(ctx, "hidden by " + reason);
    }

    private void CleanupMarker(const BML::ModContext &in ctx, const string &in reason) {
        if (marker is null) {
            return;
        }

        if (markerPhysical) {
            BML::Physics::Unphysicalize(marker);
            markerPhysical = false;
        }

        BML::CK::Show(marker, CKHIDE, true);
        @marker = null;
        markerVisible = false;
        markerId = 0;
        markerName = "none";
        SetLast(ctx, "cleaned by " + reason);
    }

    private void ReportStatus(const BML::ModContext &in ctx, const string &in reason) {
        SetLast(ctx, "status by " + reason + " marker=" + markerName +
                     " visible=" + BoolText(markerVisible) +
                     " physical=" + BoolText(markerPhysical));
    }

    private void SetShowWindow(const BML::ModContext &in ctx, bool value) {
        showWindow = value;
        SaveBoolean(showWindowProp, showWindow);
        SetLast(ctx, "window=" + BoolText(showWindow));
    }

    private void SetAutoCreate(const BML::ModContext &in ctx, bool value) {
        autoCreate = value;
        SaveBoolean(autoCreateProp, autoCreate);
        SetLast(ctx, "autoCreate=" + BoolText(autoCreate));
    }

    private void SetMessagesEnabled(const BML::ModContext &in ctx, bool value) {
        messagesEnabled = value;
        SaveBoolean(messagesProp, messagesEnabled);
        SetLast(ctx, "messages=" + BoolText(messagesEnabled));
    }

    private void SaveBoolean(BML::ConfigProperty@ property, bool value) {
        if (property !is null && property.IsValid) {
            property.SetBoolean(value);
        }
    }

    private void SetLast(const BML::ModContext &in ctx, const string &in text) {
        lastAction = text;
        LogInfo(ctx, "Marker Mod " + text);

        if (messagesEnabled) {
            ctx.SendIngameMessage("Marker Mod: " + text);
        }
    }

    private string BoolText(bool value) {
        return value ? "true" : "false";
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
}
```

第一次启动时，正常日志包括：

```text
Loading Mod marker.mod[Marker Mod] v1.0.0 by Tutorial
Marker command registered=true
Marker Mod loaded
BML script mod summary: loaded=1 failed=0
```

如果配置里打开 `AutoCreate`，还会看到：

```text
Marker Mod created by load physical=true
```

执行命令：

```text
marker status
marker cleanup
```

正常日志：

```text
Execute Command: marker status
Marker Mod status by command marker=P_Modul_01_Pusher_BMLLoad_1 visible=true physical=true
Execute Command: marker cleanup
Marker Mod cleaned by command
```

## 代码骨架

先看生命周期：

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    LoadConfig(ctx);
    RegisterCommand(ctx);

    if (autoCreate) {
        CreateMarker(ctx, "load");
    }
}

void OnUnload(const BML::ModContext &in ctx) {
    CleanupMarker(ctx, "unload");

    if (commandRef !is null && commandRef.IsValid) {
        commandRef.Unregister();
    }
}
```

这个骨架回答三个问题：

```text
加载时准备什么
运行时怎么操作
卸载时怎么收尾
```

`OnGameEvent` 负责关卡切换清理：

```angelscript
if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL ||
    event == BML::GAME_EVENT_PRE_LOAD_LEVEL ||
    event == BML::GAME_EVENT_EXIT_GAME) {
    CleanupMarker(ctx, "game-event");
}
```

只要对象跟当前关卡环境有关，关卡切换前就要收掉。

## 配置

本章有三个配置项：

| 配置 | 默认值 | 用途 |
| --- | --- | --- |
| `ShowWindow` | `true` | 是否显示 ImGui 窗口 |
| `AutoCreate` | `false` | 加载或进关时是否自动创建 marker |
| `MessagesEnabled` | `true` | 是否显示 BML 游戏内消息 |

配置的写法仍然是：

```angelscript
@autoCreateProp = config.GetProperty("MarkerMod", "AutoCreate");
autoCreateProp.SetDefaultBoolean(false);
autoCreate = autoCreateProp.GetBoolean(false);
```

状态变化后写回：

```angelscript
private void SaveBoolean(BML::ConfigProperty@ property, bool value) {
    if (property !is null && property.IsValid) {
        property.SetBoolean(value);
    }
}
```

这样命令和 UI 改动都能保存。

## 命令

命令只做分发：

```text
marker status     显示状态
marker create     创建 marker
marker show       显示 marker
marker hide       隐藏 marker
marker cleanup    清理 marker
marker window     切换窗口
marker auto       切换自动创建
marker messages   切换消息
```

`OnMarkerCommand` 不直接写大段逻辑。它只根据第一个参数调用对应函数：

```angelscript
if (action == "create") {
    CreateMarker(ctx, "command");
} else if (action == "cleanup") {
    CleanupMarker(ctx, "command");
} else {
    ReportStatus(ctx, "command");
}
```

这样命令、UI、自动创建都能复用同一套 `CreateMarker` / `CleanupMarker`。

## UI

窗口只做两类事：

```text
显示状态
提供按钮
```

状态区：

```angelscript
ImGui::TextUnformatted("Game: inGame=" + BoolText(ctx.GetIsInGame()) +
                       " inLevel=" + BoolText(ctx.GetIsInLevel()) +
                       " playing=" + BoolText(ctx.GetIsPlaying()));
ImGui::TextUnformatted("Marker: " + markerName + " id=" + markerId);
```

按钮区：

```angelscript
if (ImGui::Button("Create", ImVec2(82.0f, 0.0f))) {
    CreateMarker(ctx, "ui");
}
```

UI 也不直接复制对象逻辑。按钮只是调用同一个函数。

## 可回滚的小修改

本章的修改是创建一个 marker：

```angelscript
@marker = FindFirstEntity(result);
BML::CK::SetPosition(marker, VxVector(0.0f, 8.0f, 0.0f));
BML::CK::Show(marker, CKSHOW, true);
markerPhysical = BML::Physics::PhysicalizeBall(marker, physics, VxVector(0.0f, 0.0f, 0.0f), 2.0f);
```

它的回滚函数是：

```angelscript
if (markerPhysical) {
    BML::Physics::Unphysicalize(marker);
    markerPhysical = false;
}

BML::CK::Show(marker, CKHIDE, true);
@marker = null;
```

这里没有永久写入原版表，也没有改原版行为图。退出、切关、卸载时都走同一个清理函数。

## 小结

完整小 mod 不靠堆功能变完整。它要有边界：

```text
配置保存长期偏好
命令提供手动入口
UI 显示当前状态
修改只碰自己的可控对象
清理路径和创建路径同样重要
```

第 33 章讲发布、版本和兼容性。到那一步，脚本文件、配置默认值、脚本库和资源文件都要按安装包组织好。


