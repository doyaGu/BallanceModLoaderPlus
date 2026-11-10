# ModContext 与服务边界

回调传入的 `const BML::ModContext &in ctx` 是显式的 Mod 上下文。所有可失败操作返回
`int` 状态，成功为 `BML::ERROR_OK`。Ref 输出失败为 null；其他输出仅在成功后使用。
完整签名来自[实际注册表](https://doyagu.github.io/BallanceModLoaderPlus/api/bml-script-mod-api.as)，旧源码见[迁移指南](migration.md)。

## 常用入口

```angelscript
BML::Config@ Config() const;
BML::Input@ Input() const;
BML::Timers@ Timers() const;
BML::Commands@ Commands() const;
bool IsValid() const;
int Owner(BML::ModRef@ &out mod) const;
int FindMod(const string &in id, BML::ModRef@ &out mod) const;
int ReadMods(? &out mods) const; // array<BML::ModInfo> 对象
int LogInfo(const string &in message) const;
int LogWarn(const string &in message) const;
int LogError(const string &in message) const;
int GetDirectory(BML::DirectoryType directory, string &out path) const;
int GetResourcePath(const string &in path, string &out absolutePath) const;
int ReadResource(const string &in path, array<uint8> &out bytes) const;
int ResourceExists(const string &in path, bool &out exists) const;
```

```angelscript
BML::ModRef@ owner;
BML::ModInfo info;
if (ctx.Owner(owner) == BML::ERROR_OK && owner.Read(info) == BML::ERROR_OK)
    ctx.LogInfo(info.Definition.Id + ": " + info.Definition.Name);
```

服务视图属于一次物理运行期。OnLoad 结束不会让它失效；卸载或替换后永久失效，回滚
也不能复活旧视图。配置属性和控制 Ref 同样遵守这个边界。诊断用的 IsReloading、
ReloadPhase、ReloadAttemptId、ModGeneration、RuntimeGeneration 属性不是有效性凭据。
丢弃脚本控制 Ref 不取消注册；宿主退休会清理，或在当前运行期显式结束。

## 专用模块

| 任务 | 当前入口 |
| --- | --- |
| 配置、命令、定时器 | [服务参考](ref-commands-config-timer.md) |
| 键鼠与手柄 | [输入参考](ref-input-imgui.md) |
| Ingame/Paused/Playing、球与注册 | BML::Gameplay::ReadState / BorrowActiveBall / Register* |
| 游戏时钟 | BML::Time::ReadClock(Clock &out) |
| HUD、消息、菜单、Speedrun | [UI 与资源](ref-hud-resources.md) |
| CK 对象查找 | BML::Scene::FindObject(name, ObjectRef &out)，再 Engine::BorrowObject |
| CKContext 和管理器借用 | BML::Engine::BorrowContext(ctx, out) 及 Borrow*Manager(ctx, out) |
| Behavior、IMC、DataShare | 保留各自专用协议和失败处理，不强制改为同一种 Ref |

Gameplay 六种类型注册仅允许 OnLoad，继续经过 Capture/Validate 事务。注册失败必须
检查错误码。数据查询返回拥有数据的快照。

## Borrow

Borrow* 返回引擎借用，必须检查状态和 null，
跨关卡不要保存裸 CK 句柄。需要跨回调身份时保存 ObjectRef，每次重新借用。
