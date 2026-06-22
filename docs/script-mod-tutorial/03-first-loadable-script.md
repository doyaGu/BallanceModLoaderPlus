# 第 3 章：第一个可加载脚本

本章创建一个单文件脚本 mod。

文件位置：

```text
ModLoader/Mods/HelloMod.mod.as
```

本章只使用单文件形式。目录形式和 zip 形式后面再讲。

本章结束时，只看一个结果：

```text
ModLoader.log 里出现 HelloMod loaded...
```

## 创建文件

在 Ballance 游戏目录下找到 `ModLoader/Mods`。

如果 `Mods` 目录不存在，先创建：

```text
ModLoader/Mods
```

然后创建文件：

```text
ModLoader/Mods/HelloMod.mod.as
```

文件名要以 `.mod.as` 结尾。BML 用这个后缀识别脚本 mod 入口。

## 写入代码

把下面代码写入 `HelloMod.mod.as`：

```angelscript
// bml.mod 是 BMLPlus/CKAS 识别的元数据标记。
[bml.mod id="hello.script" name="Hello Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Minimal tutorial script mod"]
class HelloMod {
    // OnLoad 在脚本 mod 加载完成后调用一次。
    void OnLoad(const BML::ModContext &in ctx) {
        // 从 BML 上下文取得本 mod 的日志对象。
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            // 写入 ModLoader.log。
            logger.Info("HelloMod loaded from ModLoader/Mods/HelloMod.mod.as");
        }
    }
}
```

第 2 章已经拆过这段脚本的结构。本章只确认它作为文件放进游戏后能不能运行。

## 运行检查

启动 Ballance。

检查 BML 日志或 Mod 菜单诊断信息。

看到下面这行，说明本章脚本已经加载并执行了 `OnLoad`：

```text
HelloMod loaded from ModLoader/Mods/HelloMod.mod.as
```

这一步确认了三件事：

- BML 找到了 `HelloMod.mod.as`；
- CKAS 编译通过；
- BML 调用了 `OnLoad`。

本教程按下面这个相对位置做过实际测试：

```text
<Ballance 游戏目录>\ModLoader\Mods\HelloMod.mod.as
```

测试环境中的 `ModLoader.log` 出现了这些行：

```text
[ModLoader/INFO]: Loading Mod hello.script[Hello Mod] v1.0.0 by Tutorial
[hello.script/INFO]: HelloMod loaded from ModLoader/Mods/HelloMod.mod.as
[ModLoader/INFO]: BML script mod summary: loaded=1 failed=0
```

## 修改后重启 Player

本教程不假设脚本会自动重新加载。

修改 `.as` 文件后，关闭 Player，重新启动，再看日志。

## 常见检查项

### 文件名

确认文件名是：

```text
HelloMod.mod.as
```

下面这些文件名不会被本章脚本加载：

```text
HelloMod.as
HelloMod.mod.txt
HelloMod.mod.as.txt
```

如果 Windows 资源管理器隐藏扩展名，文件名需要打开属性确认。文件无法加载时，先确认真实文件名。

### 放置位置

本章单文件 mod 放在：

```text
ModLoader/Mods/HelloMod.mod.as
```

### 元数据

确认类前面有 `[bml.mod ...]`。

一个脚本 mod 入口需要一个带 `[bml.mod]` 的主类。

本章使用的元数据格式是：

```angelscript
[bml.mod id="hello.script" name="Hello Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Minimal tutorial script mod"]
```

### 回调签名

本章使用的签名是：

```angelscript
void OnLoad(const BML::ModContext &in ctx)
```

函数名、返回类型、参数类型都按这个写。

### 日志位置

脚本没有输出时，先看：

- BML 日志；
- Mod 菜单诊断信息。

## 本章结果

本章完成后，`ModLoader/Mods` 下有一个脚本 mod：

```text
HelloMod.mod.as
```

启动 Ballance 后，日志中出现：

```text
HelloMod loaded from ModLoader/Mods/HelloMod.mod.as
```

下一章先讲 BML 什么时候调用这些函数。第 5 章再在这个脚本上添加调试窗口。
