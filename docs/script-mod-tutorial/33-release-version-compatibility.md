# 第 33 章：发布、版本和兼容性

前面已经能写出一个完整小 mod。发布时要处理的是另一类问题：

```text
别人怎么安装
脚本需要哪个 BMLPlus
缺少依赖时会发生什么
更新后旧配置还能不能用
卸载后游戏运行状态能不能恢复
```

发布不能停在“把 `.mod.as` 发出去”。脚本能在自己机器上跑，只说明代码路径走通了。要给别人用，还要把身份、依赖、文件结构、配置和清理规则固定下来。

## 先固定 mod 身份

入口类前面的元数据就是发布信息：

```angelscript
[bml.mod id="marker.mod" name="Marker Mod" version="1.0.0" author="YourName" bml="0.3.12" description="Creates a private marker object"]
class MarkerMod {
}
```

这行写在 AngelScript 源码里，由 CKAS 的 metadata reflection 保留下来，再交给 BML 读取。AngelScript 本体只负责允许这种 metadata 存在，`bml.mod` 的含义由 BMLPlus 定义。

几个字段要分清：

| 字段 | 发布时怎么定 |
| --- | --- |
| `id` | 稳定的机器名字。配置、依赖、日志都靠它识别这个 mod |
| `name` | 给人看的名字，可以比 `id` 更容易读 |
| `version` | 当前 mod 版本，更新时递增 |
| `author` | 作者名 |
| `bml` | 需要的最低 BMLPlus 版本 |
| `description` | 一句话说明功能范围 |

`id` 不要随便改。改了以后，BML 会把新版本当成另一个 mod，旧配置、旧依赖声明、其他 mod 的查找代码都对不上。

`name` 可以改，但不要频繁改。玩家在日志和菜单里看到的是这个名字。

`class MarkerMod` 是入口类名。BML 真正识别 mod 主要靠 `[bml.mod]` 元数据，类名仍然应该保持清楚，因为编译错误、导出方法、源码组织都会围绕这个类展开。

## 版本号怎么递增

教程里的版本号用三段：

```text
主版本.次版本.修订版本
```

可以按这张表处理：

| 改动 | 版本变化 |
| --- | --- |
| 修日志、修按钮文字、修不会影响配置和命令的小 bug | `1.0.0` -> `1.0.1` |
| 加新命令、新配置项、新窗口信息，旧用法还有效 | `1.0.0` -> `1.1.0` |
| 改 mod id、删命令、改配置含义、改导出函数签名 | `1.0.0` -> `2.0.0` |

版本号会参与依赖判断。别的 mod 用 `[bml.require]` 声明依赖时，会写需要的最低版本：

```angelscript
[bml.require id="marker.mod" version="1.0.0"]
```

如果你把功能删掉了，却还只改修订版本，依赖你的脚本会以为旧能力仍然存在。

## BMLPlus 版本要求

`bml="0.3.12"` 表示这个脚本需要 BMLPlus 0.3.12 或更新版本提供的脚本能力。

```angelscript
[bml.mod id="marker.mod" name="Marker Mod" version="1.0.0" author="YourName" bml="0.3.12"]
class MarkerMod {
}
```

发布说明里也要写清楚：

```text
需要：BMLPlus 0.3.12 或更新版本，且包含脚本支持。
安装：把发布包放入 ModLoader/Mods。
```

不要只写“需要 BML”。玩家需要知道具体版本。脚本 mod 依赖 CKAS 运行时，脚本版 BMLPlus 发布包会带上匹配的 CKAngelScript / AngelScript 运行库。

## 必选依赖和可选依赖

依赖写在入口类前面：

```angelscript
[bml.require id="callbus.provider" version="1.0.0"]
[bml.optional id="debug.helper" version="0.1.0"]
[bml.mod id="marker.mod" name="Marker Mod" version="1.0.0" author="YourName" bml="0.3.12"]
class MarkerMod {
}
```

`bml.require` 用在缺少后无法正常工作的依赖上。  
`bml.optional` 用在有它更好、没有也能运行的功能上。

可选依赖不能只声明就结束。代码里还要检查：

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    BML::ModRef@ helper = ctx.FindMod("debug.helper");

    // 可选依赖缺少时，关闭对应功能即可。
    if (helper is null || !helper.IsValid || helper.IsFailed) {
        LogInfo(ctx, "debug helper unavailable");
        return;
    }

    LogInfo(ctx, "debug helper ready: " + helper.Version);
}
```

这段代码只做一件事：根据依赖是否存在决定功能是否启用。不要在可选依赖缺少时让主功能直接失败。

## 发布包怎么组织

脚本 mod 可以用单文件，也可以用目录或 zip。单文件适合很小的 mod：

```text
ModLoader/Mods/MarkerMod.mod.as
```

文件变多以后，用目录更清楚：

```text
ModLoader/Mods/MarkerMod/
  MarkerMod.mod.as
  libs/
    MarkerUi.as
    MarkerConfig.as
  Resources/
    marker_text.txt
  README.md
```

目录包或 zip 包里只能有一个 `*.mod.as` 入口。库文件放 `.as` 可以，但不要再写 `[bml.mod ...]`。

入口文件顶部引用库：

```angelscript
#include "libs/MarkerConfig.as"
#include "libs/MarkerUi.as"

[bml.mod id="marker.mod" name="Marker Mod" version="1.0.0" author="YourName" bml="0.3.12"]
class MarkerMod {
}
```

资源路径用 mod 自己的目录解析：

```angelscript
string text = ctx.ReadModTextFileUtf8("Resources/marker_text.txt", "");
```

这样发布到别人机器上仍然能找到资源。不要把自己电脑上的完整路径写进脚本。

`.bmodp` 是原生 DLL mod 的包格式，不用于脚本 mod。

## 配置默认值放在哪里

配置默认值应该由代码设置：

```angelscript
private void LoadConfig(const BML::ModContext &in ctx) {
    BML::Config@ config = ctx.BorrowConfig();
    if (config is null) {
        return;
    }

    BML::ConfigProperty@ show = config.GetProperty("MarkerMod", "ShowWindow");
    if (show !is null) {
        // 默认值写在代码里，第一次运行时会进入配置。
        show.SetDefaultBoolean(true);
        show.SetComment("Show the Marker Mod window.");
    }
}
```

发布包不要靠覆盖玩家配置文件来提供默认值。用户改过配置以后，更新 mod 不应该把他的设置改回去。

如果要给高级用户一个参考配置，可以在 `README.md` 里写说明，或者放一个示例文件。它只能作为说明，不能替代代码里的默认值。

## 更新旧配置

改配置名时，先兼容旧 key。下面示例把旧的 `ShowMessages` 迁移到新的 `MessagesEnabled`：

```angelscript
private void LoadConfig(const BML::ModContext &in ctx) {
    BML::Config@ config = ctx.BorrowConfig();
    if (config is null) {
        return;
    }

    bool hadVersion = config.HasKey("MarkerMod", "ConfigVersion");
    bool hadNewKey = config.HasKey("MarkerMod", "MessagesEnabled");

    BML::ConfigProperty@ version = config.GetProperty("MarkerMod", "ConfigVersion");
    BML::ConfigProperty@ messages = config.GetProperty("MarkerMod", "MessagesEnabled");

    if (version is null || messages is null) {
        return;
    }

    version.SetDefaultInteger(2);
    messages.SetDefaultBoolean(true);

    int configVersion = hadVersion ? version.GetInteger(1) : 1;

    if (configVersion < 2 && !hadNewKey && config.HasKey("MarkerMod", "ShowMessages")) {
        BML::ConfigProperty@ oldMessages = config.GetProperty("MarkerMod", "ShowMessages");
        if (oldMessages !is null) {
            // 旧配置还在时，把旧值写入新 key。
            messages.SetBoolean(oldMessages.GetBoolean(true));
        }
    }

    version.SetInteger(2);
}
```

这个迁移策略很朴素：

```text
先看旧 key 是否存在
再看新 key 是否已经被用户设置
只在需要时复制旧值
最后写入新的配置版本
```

当前配置 API 没有提供删除旧 key 的脚本接口。旧 key 留在配置文件里没关系，只要新代码不再读取它。

## 卸载时要清理什么

脚本卸载时至少考虑三类状态：

| 状态 | 处理方式 |
| --- | --- |
| 自己注册的命令、Timer、请求 | BML 会随脚本卸载清理，代码里也可以显式取消 |
| 自己创建或物理化的对象 | 在 `OnUnload`、关卡切换前清理 |
| 原版表或对象上的临时修改 | 恢复旧值，再释放脚本保存的句柄 |

第 31 章里的清理思路可以保留：

```angelscript
void OnUnload(const BML::ModContext &in ctx) {
    CleanupMarker(ctx, "unload");

    if (commandRef !is null && commandRef.IsValid) {
        commandRef.Unregister();
    }
}
```

卸载 mod 后，游戏里不应该还留着脚本创建的物理对象、隐藏状态、持续力、临时 HUD 状态或被改过的 DataArray 值。

配置文件可以保留。它记录用户偏好，不会改变当前游戏运行状态。完整卸载说明里可以写：

```text
删除 mod 文件或目录即可停止加载。
如需同时删除用户设置，再删除 ModLoader/Configs/marker.mod.cfg。
```

## 发布说明写什么

发布包里至少放一份 `README.md`。内容不用长，但要回答这些问题：

```text
这个 mod 做什么
需要哪个 BMLPlus 版本
安装到哪里
有哪些命令
有哪些配置项
如何卸载
在哪个游戏版本和 BMLPlus 版本上验证过
```

例如：

```text
# Marker Mod

需要：BMLPlus 0.3.12 或更新版本。
安装：把 MarkerMod 文件夹或 MarkerMod.zip 放入 ModLoader/Mods。

命令：
- marker status
- marker create
- marker cleanup

配置：
- ShowWindow
- AutoCreate
- MessagesEnabled

卸载：
删除 MarkerMod 文件夹或 MarkerMod.zip。
如需清掉设置，删除 ModLoader/Configs/marker.mod.cfg。

已验证：
- Ballance Player
- BMLPlus 0.3.12
```

这里的“已验证”要写真实跑过的版本。没验证过的版本不要写“支持”。

## 发布前检查清单

发布前按这张表过一遍：

| 检查项 | 通过标准 |
| --- | --- |
| 入口 | 包里只有一个 `*.mod.as` 入口 |
| 元数据 | `id`、`name`、`version`、`bml` 都准确 |
| 依赖 | 必选依赖用 `bml.require`，可选依赖有运行时判断 |
| 路径 | 脚本里没有本机完整路径 |
| 资源 | 随包资源能通过 `ctx.ResolveModPathUtf8` 或 `ctx.ReadModTextFileUtf8` 找到 |
| 配置 | 默认值在代码里，更新时不覆盖用户配置 |
| 清理 | 退出关卡、重置、卸载都会恢复临时修改 |
| 文档 | README 写清安装、命令、配置、卸载、已验证版本 |

到这里，第四篇的主线就收住了：先做低风险观察，再做自己的可控对象，再组合成小 mod，最后把它整理成可以交给别人安装和更新的发布包。


