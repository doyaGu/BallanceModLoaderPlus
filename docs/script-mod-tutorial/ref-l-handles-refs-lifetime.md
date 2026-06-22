# 参考 L：句柄、引用和对象生命周期

`Borrow3dEntityByName` 和 `BorrowGroupByName` 适合在当前函数里拿对象、打印信息、马上结束。

写到对象引用时，要考虑“脚本能不能把对象记住”。这个问题比看起来重要。Ballance 会加载关卡、卸载关卡、重置关卡，Virtools 对象也会跟着创建和销毁。脚本如果把一个已经失效的对象当成还活着，就会得到错误结果，严重时会让脚本回调异常。

先记住三种东西：

```text
名字：string，例如 "PC_TwoFlames_MF"
运行时 id：int / CK_ID，例如 45418
对象句柄：CK3dEntity@、ObjectRef@、Entity3DRef@
```

它们都能指向“某个对象”，但适用场景不同。

## 先区分两类句柄

AngelScript 里的 `@` 表示对象句柄。句柄可以理解成“指向一个对象的引用”。例如：

```angelscript
CK3dEntity@ entity = ctx.Borrow3dEntityByName("PC_TwoFlames_MF");
```

这里的 `entity` 是一个 `CK3dEntity@` 句柄。

但 CKAS 里还有一类更安全的引用包装：

```angelscript
Entity3DRef@ ref = Scene::FindEntity3D(ck, "PC_TwoFlames_MF", 0, true);
```

这里的 `ref` 也是 AngelScript 句柄，因为它有 `@`。它指向的是 CKAS 包装出来的 `Entity3DRef`，不是原始 `CK3dEntity`。

两者的差别如下：

| 类型 | 从哪里来 | 适合保存吗 | 用之前检查什么 |
| --- | --- | --- | --- |
| `CK3dEntity@` | BML `Borrow3dEntityByName`、原始 CK API 返回值 | 只在当前函数附近使用 | `entity !is null` |
| `Entity3DRef@` | CKAS `Scene::FindEntity3D` | 可以作为脚本侧对象身份保存 | `ref !is null && ref.valid` |
| `ObjectRef@` | CKAS `Scene::Find`、`Scene::ById`、`Scene::Ref` | 可以作为通用对象身份保存 | `ref !is null && ref.valid` |

`CK3dEntity@` 更接近 Virtools 原始对象。它适合马上调用一个 CK 方法。

`ObjectRef@` / `Entity3DRef@` 会在访问时重新检查对象是否还有效。它们更适合跨多次回调保存。

## 名字、id、引用分别解决什么问题

假设脚本想记住第一关里检查点火焰相关的一个运行时实体。

用名字：

```angelscript
string rememberedName = "PC_TwoFlames_MF";
```

名字容易写进配置、命令和日志。缺点是名字可能重复，也可能在另一关不存在。

用 id：

```angelscript
int rememberedId = BML::CK::GetId(entity);
```

id 是运行时编号。它适合在同一轮加载期间做诊断，但不适合写进配置或发布 mod。下一次进入关卡，id 可能变化。

用 CKAS 引用：

```angelscript
Entity3DRef@ rememberedRef = Scene::FindEntity3D(ck, "PC_TwoFlames_MF", 0, true);
```

`Entity3DRef@` 适合脚本在内存里保存对象身份。关卡重置或对象删除以后，`rememberedRef.valid` 会变成 `false`，脚本可以重新查找。

## 一个观察脚本

示例只做观察，不改对象。脚本提供一个命令：

```text
ckref
```

命令执行时，它会：

1. 用名字借到原始 `CK3dEntity@`。
2. 打印对象的名字、id、class id。
3. 用 `Scene::FindEntity3D` 取得 `Entity3DRef@`。
4. 保存这个 `Entity3DRef@` 到成员变量。
5. 下一帧检查保存的引用是否仍然有效。

脚本入口路径：

```text
ModLoader/Mods/RefMod.mod.as
```

```angelscript
[bml.mod id="ref.script" name="Ref Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Observe CK object handles and CKAS refs"]
class RefMod {
    private string targetName = "PC_TwoFlames_MF";
    private Entity3DRef@ rememberedRef;
    private int rememberedId = 0;
    private bool checkNextFrame = false;

    void OnLoad(const BML::ModContext &in ctx) {
        BML::CommandDefinition def;
        def.Name = "ckref";
        def.Description = "Observe one CK object handle and one CKAS ref";
        def.Usage = "ckref";

        ctx.RegisterCommand(def, BML::CommandCallback(this.OnCommand));
        ctx.BorrowLogger().Info("RefMod loaded; use ckref after a level starts");
    }

    void OnProcess(const BML::ModContext &in ctx) {
        if (!checkNextFrame) {
            return;
        }

        checkNextFrame = false;
        BML::Logger@ log = ctx.BorrowLogger();

        if (rememberedRef is null) {
            log.Warn("rememberedRef is null");
            return;
        }

        // Entity3DRef 会重新检查目标对象。关卡切换后这里可能变成 false。
        if (!rememberedRef.valid) {
            log.Warn("rememberedRef is no longer valid: " + rememberedRef.Error());
            return;
        }

        log.Info("rememberedRef still valid: name=" + rememberedRef.Name() +
                 " id=" + rememberedRef.Id());
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL ||
            event == BML::GAME_EVENT_PRE_LOAD_LEVEL) {
            // 关卡退出或准备加载新关卡时，丢掉旧关卡对象身份。
            @rememberedRef = null;
            rememberedId = 0;
            checkNextFrame = false;
        }
    }

    private void OnCommand(const BML::ModContext &in ctx,
                           const BML::CommandEvent &in event) {
        BML::Logger@ log = ctx.BorrowLogger();

        CK3dEntity@ entity = ctx.Borrow3dEntityByName(targetName);
        if (entity is null) {
            log.Warn("Borrow3dEntityByName not found: " + targetName);
            return;
        }

        rememberedId = BML::CK::GetId(entity);
        log.Info("borrowed raw entity: name=" + BML::CK::GetName(entity) +
                 " id=" + rememberedId +
                 " class=" + BML::CK::GetClassId(entity));

        CKContext@ ck = ctx.BorrowCKContext();
        if (ck is null) {
            log.Warn("BorrowCKContext returned null");
            return;
        }

        @rememberedRef = Scene::FindEntity3D(ck, targetName, 0, true);
        if (rememberedRef is null || !rememberedRef.valid) {
            string reason = rememberedRef is null ? "null ref" : rememberedRef.Error();
            log.Warn("Scene::FindEntity3D failed: " + reason);
            return;
        }

        log.Info("saved Entity3DRef: name=" + rememberedRef.Name() +
                 " id=" + rememberedRef.Id() +
                 " class=" + rememberedRef.ClassId());

        checkNextFrame = true;
    }
}
```

## 代码怎么读

先看成员变量：

```angelscript
private string targetName = "PC_TwoFlames_MF";
private Entity3DRef@ rememberedRef;
private int rememberedId = 0;
private bool checkNextFrame = false;
```

`targetName` 是最容易理解的身份。它保存对象名字。

`rememberedId` 只用来打印对照，不把它当成长期配置。

`rememberedRef` 是这里的重点。它保存 CKAS 引用包装，不保存原始 `CK3dEntity@`。

`checkNextFrame` 用来让脚本在下一帧再检查一次引用。这样可以看到“引用被保存到成员变量以后还能继续使用”。

## 原始句柄只在附近使用

命令回调里先写：

```angelscript
CK3dEntity@ entity = ctx.Borrow3dEntityByName(targetName);
if (entity is null) {
    log.Warn("Borrow3dEntityByName not found: " + targetName);
    return;
}
```

这一步只确认对象当前能找到。找到以后马上打印：

```angelscript
rememberedId = BML::CK::GetId(entity);
log.Info("borrowed raw entity: name=" + BML::CK::GetName(entity) +
         " id=" + rememberedId +
         " class=" + BML::CK::GetClassId(entity));
```

这里没有把 `entity` 存进成员变量。`Borrow3dEntityByName` 返回的是借来的 Virtools 对象句柄。用完就结束。

## 保存 CKAS 引用

接着取得 `CKContext@`：

```angelscript
CKContext@ ck = ctx.BorrowCKContext();
if (ck is null) {
    log.Warn("BorrowCKContext returned null");
    return;
}
```

BML 脚本 mod 没有 `ScriptContext`，所以这里用 `BorrowCKContext()` 进入 CKAS `Scene` API 的 `CKContext@` 重载。

然后查找对象：

```angelscript
@rememberedRef = Scene::FindEntity3D(ck, targetName, 0, true);
```

参数含义：

| 参数 | 含义 |
| --- | --- |
| `ck` | Virtools 上下文 |
| `targetName` | 要找的对象名 |
| `0` | 找第一个匹配项 |
| `true` | 只在当前场景里找 |

保存之前要检查：

```angelscript
if (rememberedRef is null || !rememberedRef.valid) {
    string reason = rememberedRef is null ? "null ref" : rememberedRef.Error();
    log.Warn("Scene::FindEntity3D failed: " + reason);
    return;
}
```

`rememberedRef is null` 检查 AngelScript 句柄有没有对象。

`rememberedRef.valid` 检查 CKAS 引用指向的 Virtools 对象当前是否有效。

这两个检查不一样。句柄不为空，只说明 `Entity3DRef` 包装对象存在；`valid` 为真，才说明它现在还能解析到目标对象。

## 关卡切换时清理

脚本里有这段：

```angelscript
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
    if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL ||
        event == BML::GAME_EVENT_PRE_LOAD_LEVEL) {
        @rememberedRef = null;
        rememberedId = 0;
        checkNextFrame = false;
    }
}
```

这不是因为 `Entity3DRef@` 完全不能保存。它可以保存，也会自检。

清理的原因更简单：脚本已经知道要离开当前关卡，就不要继续拿旧关卡对象做后续逻辑。进入新关卡后按名字重新找，代码更清楚。

## 运行后看什么

进入第一关，等关卡开始后执行：

```text
ckref
```

日志里应当看到两组信息：

```text
borrowed raw entity: name=PC_TwoFlames_MF id=... class=...
saved Entity3DRef: name=PC_TwoFlames_MF id=... class=...
rememberedRef still valid: name=PC_TwoFlames_MF id=...
```

一次实际运行里输出是：

```text
borrowed raw entity: name=PC_TwoFlames_MF id=46378 class=33
saved Entity3DRef: name=PC_TwoFlames_MF id=46378 class=33
rememberedRef still valid: name=PC_TwoFlames_MF id=46378
```

这次运行里的 id 是 `46378`。它可以帮助确认三行日志说的是同一个对象，但不要把这个数字写进配置或发布脚本。

第一行来自原始 `CK3dEntity@`。

第二、三行来自 `Entity3DRef@`。

如果还在菜单里执行，可能看到：

```text
Borrow3dEntityByName not found: PC_TwoFlames_MF
```

这说明对象还没有随关卡加载出来。等进入关卡后再执行命令。

## 什么时候用哪一种

按这个顺序判断：

| 需求 | 用什么 |
| --- | --- |
| 只是这次函数里打印名字、id、位置 | `CK3dEntity@`，用完就丢 |
| 下次回调还要找同一个对象 | 保存名字，下一次重新查 |
| 同一轮关卡里要多次操作同一个对象 | 保存 `Entity3DRef@`，每次用前检查 `valid` |
| 要写进配置文件 | 保存名字，不保存 id 或句柄 |
| 要跨关卡继续有效 | 保存逻辑名字或配置项，进入关卡后重新查 |
| 要调用 CK SDK 方法 | 先用 `Entity3DRef@` 确认身份，再按当前 API 借到 `CK3dEntity@`，操作完不要保存原始句柄 |

显示、移动、DataArray 写入同样使用这条规则：

```text
长期保存身份：名字或 CKAS Ref
临时执行操作：原始 CK 句柄
关卡切换：清缓存，重新查
```
