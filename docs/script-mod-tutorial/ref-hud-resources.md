# HUD、消息与资源

## 游戏内消息

```angelscript
ctx.SendIngameMessage("text");
ctx.ClearIngameMessages();
// 或
BML::UI::SendMessage("text");
BML::UI::ClearMessages();
```

注意：ClearMessages 清除所有 mod 的消息。

## HUD 控制

```angelscript
ctx.ShowFPS(bool);
ctx.ShowSRTimer(bool);
ctx.ResetSRTimer();
ctx.StartSRTimer();
ctx.PauseSRTimer();
float time = ctx.GetSRTime();
int mode = ctx.GetHUD();
ctx.SetHUD(mode);
```

规则：加载时保存原始 HUD 状态，卸载时恢复。

## 菜单

```angelscript
ctx.OpenModsMenu();
ctx.CloseModsMenu();
ctx.OpenMapMenu();
ctx.CloseMapMenu();
```

不要在 OnLoad 里自动打开菜单。

## 文件路径

```angelscript
string root = ctx.GetModRootUtf8();
string path = ctx.ResolveModPathUtf8("data/message.txt");
bool exists = ctx.ModFileExistsUtf8("data/message.txt");
string content = ctx.ReadModTextFileUtf8("data/message.txt", "");
```

规则：
- 路径用正斜杠
- 不要硬编码绝对路径
- 读取前先检查 ModFileExistsUtf8

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
options.File = BML::Path::Combine(ctx.GetDirectoryUtf8(BML::DIR_GAME), "3D Entities\\PH\\P_Ball_Wood.nmo");
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
