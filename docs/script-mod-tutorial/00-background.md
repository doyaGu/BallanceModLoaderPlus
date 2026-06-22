# 第 0 章：几个名字到底指什么

写脚本 mod 前，先把几个名字的位置摆清楚。

后面会反复看到 BML、AngelScript、CKAS、Virtools、NMO、CMO。它们不是同一种东西。先分清层级，后面读代码和报错时不会乱。

这一章只解决一个问题：

```text
现在写的 .as 脚本，放在 Ballance 的哪一层？
```

看完这一章，只需要能分清：

```text
.mod.as 是脚本
BMLPlus 负责加载脚本
AngelScript 是脚本语言
CKAS 让脚本碰到 Virtools 对象
NMO / CMO 是原版内容文件
```

## 先看最短主线

脚本 mod 的运行顺序可以先记成这样：

```text
启动 Ballance Player
  BMLPlus 被加载
    BMLPlus 扫描 ModLoader/Mods
      找到 HelloMod.mod.as
        AngelScript 编译脚本
          BMLPlus 创建 HelloMod 对象
            BMLPlus 调用 OnLoad
```

这里最重要的是：

```text
HelloMod.mod.as 是运行时加载的脚本文件。
```

它不是关卡文件，也不是原版行为图文件。

前几章只改这个脚本文件。

## 要改的文件在哪里

教程前几章写的文件放在：

```text
ModLoader/Mods/HelloMod.mod.as
```

这个文件由 BMLPlus 加载。

第 2、3 章会看到两处关键写法：

```text
[bml.mod ...]   告诉 BMLPlus：这个类是脚本 mod 入口
OnLoad          BMLPlus 加载脚本后会调用这个函数
```

这里先知道它们的用途，不需要在本章记代码。

## 暂时不改哪些文件

Ballance 原版内容主要在这些文件里：

| 文件 | 先理解成 |
| --- | --- |
| `base.cmo` | 游戏启动、菜单、关卡加载调度 |
| `3D Entities/Levelinit.nmo` | 关卡开始前的初始化流程 |
| `3D Entities/Gameplay.nmo` | 关卡运行中的玩法逻辑 |
| `3D Entities/Level/Level_01.NMO` | 第一关的地板、轨道、机关、道具、检查点等内容 |

前几章不改这些文件。

脚本 mod 会在游戏运行时工作。它可以写日志、显示窗口、注册命令、保存配置。后面进入 CKAS 后，脚本还可以查看关卡对象、组和 DataArray。

原版文件和脚本文件的区别先记成这样：

```text
NMO / CMO       Ballance 原版内容和逻辑
.mod.as         BMLPlus 运行时加载的脚本
```

## BMLPlus 和 BML

BMLPlus 是 mod loader。它接入 Ballance，负责加载 mod。

教程里经常写 `BML::`，这是脚本里使用的 BML API，例如：

```angelscript
BML::Logger
BML::ModContext
BML::CommandDefinition
```

可以先这样区分：

```text
BMLPlus.dll     加载 mod 的程序部分
BML::...        脚本里调用的 API 名字
```

前几章主要使用 BML API：

- 日志；
- ImGui 调试窗口；
- 命令；
- 配置；
- Timer；
- 游戏事件。

## AngelScript

AngelScript 是脚本语言。`.as` 文件就是 AngelScript 源码。

这部分负责语法：

```angelscript
class HelloMod {
    private bool showWindow = true;

    void OnProcess(const BML::ModContext &in ctx) {
    }
}
```

这些属于 AngelScript 语法：

- `class`
- `private`
- `bool`
- `string`
- `if`
- `return`
- 函数和变量

`BML::ModContext`、`BML::Logger`、`ImGui::TextUnformatted` 不是 AngelScript 自带的东西。它们是 BMLPlus 和相关模块注册给脚本使用的 API。

## CKAS

CKAS 是 CKAngelScript。

它把 AngelScript 接到 Virtools/Ballance 的运行时对象上。

后面开始查关卡内容时，会遇到这些名字：

```text
CKContext
CKObject
CK3dEntity
CKGroup
CKDataArray
CKBehavior
```

这些属于 Virtools 体系。

前几章写日志、窗口、命令、配置、Timer 时，还不需要深入 CKAS。等教程开始查找对象、读取组、读取 DataArray，再进入这一层。

## Virtools

Ballance 使用 Virtools 制作。关卡内容、对象、组、DataArray、行为图都来自这个体系。

先按下面理解：

| 名字 | 先理解成 |
| --- | --- |
| `CKObject` | Virtools 里的基础对象 |
| `CK3dEntity` | 有 3D 位置和模型的对象 |
| `CKGroup` | 一份对象名单 |
| `CKDataArray` | 游戏运行时使用的表格数据 |
| `CKBehavior` | 行为图上的一段逻辑 |
| Building Block | 行为图里的功能块 |

比如检查点在游戏里看起来是一组火焰。放到 Virtools 里看，它会牵涉对象、组、DataArray 和行为图。

这部分后面单独讲。现在先记住：脚本能通过 CKAS 接触这些东西。

## 三种脚本入口

同样是 AngelScript 代码，入口可能不同。

| 场景 | 常见上下文 | 出现位置 |
| --- | --- | --- |
| BML 脚本 mod | `BML::ModContext` | 本教程主线 |
| CKAS runtime script | `ScriptContext` | 进阶参考 |
| AngelScript Component | `CKBehaviorContext` | 进阶参考 |

前几章写的是 BML 脚本 mod，所以函数参数通常是：

```angelscript
const BML::ModContext &in ctx
```

如果在别的示例里看到 `ScriptContext` 或 `CKBehaviorContext`，不要直接复制到 BML 脚本 mod 里。入口不同，能使用的上下文也不同。

## 现在只需要记住什么

这一章不用背 API。

先记住这张图：

```text
Ballance Player
  BMLPlus
    BML 脚本 mod：HelloMod.mod.as
      AngelScript 语法
      BML API：日志、窗口、命令、配置、Timer
      CKAS API：后面用来接触 Virtools 对象
        Virtools：对象、组、DataArray、行为图
          NMO / CMO：原版内容文件
```

前几章的目标很小：

- 把脚本放到 `ModLoader/Mods`；
- 让 BMLPlus 加载它；
- 在 `OnLoad` 写日志；
- 在 `OnProcess` 画 ImGui 窗口；
- 再加命令、配置和 Timer。

等这些跑通，再进入对象、组、DataArray 和行为图。
