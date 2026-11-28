# UI、计时与资源

## 游戏内消息

```angelscript
BML::UI::AddMessage("text");
BML::UI::ClearMessages();
```

这些管理操作返回错误码。`ClearMessages` 会清除当前消息板上的全部消息。

## HUD 控制

```angelscript
BML::UI::HUDState state;
if (BML::UI::ReadHUDState(state) == BML::ERROR_OK)
    BML::UI::SetHUDMode(state.Mode);
BML::UI::ShowTitle(true);
BML::UI::ShowFPS(true);
```

如果 Mod 会修改 HUD 模式，应在加载时检查 ReadHUDState(state) 并保存 state.Mode，并在卸载时恢复。

## Speedrun 计时

```angelscript
BML::Speedrun::SetTimerVisible(true);
BML::Speedrun::ResetTimer();
BML::Speedrun::StartTimer();
BML::Speedrun::PauseTimer();
BML::Speedrun::TimerState state;
if (BML::Speedrun::ReadTimerState(state) != BML::ERROR_OK) return;
float elapsed = state.ElapsedTime;
```

Speedrun 计时是 Loader 的共享功能。只有确实需要控制计时流程的 Mod 才应调用修改函数。

## 菜单

```angelscript
BML::UI::OpenModsMenu();
BML::UI::CloseModsMenu();
BML::UI::OpenMapMenu();
BML::UI::CloseMapMenu();
```

不要在 `OnLoad` 中自动打开菜单。

## 文件路径

```angelscript
string root; string path; bool exists = false; array<uint8> bytes;
int code = ctx.GetDirectory(BML::DIR_MOD_ROOT, root);
code = ctx.ResourceExists("data/message.txt", exists);
if (code == BML::ERROR_OK && exists) {
    code = ctx.GetResourcePath("data/message.txt", path);
    code = ctx.ReadResource("data/message.txt", bytes);
}
```

规则：
- 路径用正斜杠
- 不要硬编码绝对路径
- 读取和解析均检查状态；GetResourcePath 只解析已存在文件，构造新路径使用 Path::Combine

## 文件布局

```
ModLoader/Mods/
  MyMod/
    MyMod.mod.as
    libs/
      MyUtils.as
    data/
      message.txt
```

## 脚本库 (#include)

```angelscript
#include "libs/MyUtils.as"
```

库文件不能有 [bml.mod] 属性。放在当前 mod 目录自己的 `libs/` 下，不要放到全局
`ModLoader/Mods/libs/` 里给多个 mod 共用。热重载按单个 mod 的入口和资源根目录
计算源码范围；全局 include 文件不是稳定的热重载边界。

## 加载资源对象

```angelscript
BML::ObjectLoadOptions options;
string game;
if (ctx.GetDirectory(BML::DIR_GAME, game) != BML::ERROR_OK) return;
options.File = BML::Path::Combine(game, "3D Entities/PH/P_Ball_Wood.nmo");
options.Rename = true;
options.MasterName = "UniqueObjectName";
options.AddToScene = true;
options.Dynamic = true;

BML::ObjectLoadResult@ result = BML::CK::LoadObject(options);
if (result !is null && result.Success && result.Count > 0) {
    CKObject@ obj = result.BorrowObject(0);
}
```

返回值必须检查 `result !is null && result.Success`。
