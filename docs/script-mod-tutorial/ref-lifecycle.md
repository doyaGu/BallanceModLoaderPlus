# 生命周期与事件

## 回调签名

```angelscript
void OnLoad(const BML::ModContext &in ctx)
void OnUnload(const BML::ModContext &in ctx)
void OnProcess(const BML::ModContext &in ctx)
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event)
void OnRender(const BML::ModContext &in ctx, const BML::RenderEvent &in event)
void OnCommandEvent(const BML::ModContext &in ctx, const BML::CommandEvent &in event)
void OnModifyConfig(const BML::ModContext &in ctx, const BML::ConfigEvent &in event)
void OnLoadObject(const BML::ModContext &in ctx, const BML::LoadObjectEvent &in event)
void OnLoadScript(const BML::ModContext &in ctx, const BML::LoadScriptEvent &in event)
void OnPhysicalize(const BML::ModContext &in ctx, const BML::PhysicalizeEvent &in event)
void OnUnphysicalize(const BML::ModContext &in ctx, const BML::ObjectEvent &in event)
void OnCheatEnabled(const BML::ModContext &in ctx, const BML::CheatEvent &in event)
```

签名必须完全匹配，否则 BML 不会调用。

热重载状态迁移使用三个可选方法：

```angelscript
void SaveState(BML::StateBag@ state)
void MigrateState(const string &in fromVersion, BML::StateBag@ state)
void RestoreState(BML::StateBag@ state)
```

这些方法不是普通回调。只有热重载需要迁移脚本字段时才实现它们。

## 回调时机

| Callback | 触发时机 | 典型用途 |
|----------|----------|----------|
| `OnLoad` | Mod 加载完成 | 初始化配置、注册命令、启动计时器 |
| `OnUnload` | Mod 即将卸载 | 清理资源 |
| `OnProcess` | 每帧调用 | ImGui 绘制、输入处理、状态刷新 |
| `OnGameEvent` | 游戏事件发生 | 关卡开始时查找对象、退出时清理 |
| `OnRender` | 渲染阶段 | 观察渲染状态 |
| `OnCommandEvent` | 命令执行 | 观察命令执行流程 |
| `OnModifyConfig` | 配置变更 | 同步成员变量 |
| `OnLoadObject` | 对象加载 | 观察对象进入场景 |
| `OnLoadScript` | 行为脚本加载 | 观察脚本加载 |
| `OnPhysicalize` | 对象物理化 | 记录物理参数 |
| `OnUnphysicalize` | 对象取消物理化 | 清理物理相关状态 |
| `OnCheatEnabled` | 作弊开关切换 | 切换调试功能 |

## GameEvent 枚举

| 枚举值 | 说明 |
|--------|------|
| `GAME_EVENT_PRE_START_MENU` | 进入主菜单前 |
| `GAME_EVENT_POST_START_MENU` | 进入主菜单后 |
| `GAME_EVENT_EXIT_GAME` | 游戏退出 |
| `GAME_EVENT_PRE_LOAD_LEVEL` | 关卡加载前 |
| `GAME_EVENT_POST_LOAD_LEVEL` | 关卡加载后 |
| `GAME_EVENT_START_LEVEL` | 关卡开始 |
| `GAME_EVENT_PRE_RESET_LEVEL` | 重置关卡前 |
| `GAME_EVENT_POST_RESET_LEVEL` | 重置关卡后 |
| `GAME_EVENT_PAUSE_LEVEL` | 关卡暂停 |
| `GAME_EVENT_UNPAUSE_LEVEL` | 关卡恢复 |
| `GAME_EVENT_PRE_EXIT_LEVEL` | 退出关卡前 |
| `GAME_EVENT_POST_EXIT_LEVEL` | 退出关卡后 |
| `GAME_EVENT_PRE_NEXT_LEVEL` | 进入下一关前 |
| `GAME_EVENT_POST_NEXT_LEVEL` | 进入下一关后 |
| `GAME_EVENT_DEAD` | 玩家死亡 |
| `GAME_EVENT_PRE_END_LEVEL` | 关卡结束前 |
| `GAME_EVENT_POST_END_LEVEL` | 关卡结束后 |
| `GAME_EVENT_COUNTER_ACTIVE` | 倒计时/计数器激活 |
| `GAME_EVENT_COUNTER_INACTIVE` | 倒计时/计数器关闭 |
| `GAME_EVENT_BALL_NAV_ACTIVE` | 球导航激活 |
| `GAME_EVENT_BALL_NAV_INACTIVE` | 球导航关闭 |
| `GAME_EVENT_CAM_NAV_ACTIVE` | 摄像机导航激活 |
| `GAME_EVENT_CAM_NAV_INACTIVE` | 摄像机导航关闭 |
| `GAME_EVENT_BALL_OFF` | 球离开有效区域 |
| `GAME_EVENT_PRE_CHECKPOINT_REACHED` | 到达存档点前 |
| `GAME_EVENT_POST_CHECKPOINT_REACHED` | 到达存档点后 |
| `GAME_EVENT_LEVEL_FINISH` | 关卡完成 |
| `GAME_EVENT_GAME_OVER` | 游戏结束 |
| `GAME_EVENT_EXTRA_POINT` | 获得额外分数 |
| `GAME_EVENT_PRE_SUB_LIFE` | 扣除生命前 |
| `GAME_EVENT_POST_SUB_LIFE` | 扣除生命后 |
| `GAME_EVENT_PRE_LIFE_UP` | 增加生命前 |
| `GAME_EVENT_POST_LIFE_UP` | 增加生命后 |

工具函数：`string BML::GetGameEventName(BML::GameEvent event)`，返回枚举的可读名称。

## 执行顺序

```
启动 -> BML 扫描脚本 -> 创建脚本对象 -> OnLoad
  -> [每帧: OnProcess]
  -> OnUnload
```

关卡加载序列：

```
PRE_LOAD_LEVEL -> POST_LOAD_LEVEL -> START_LEVEL
```

## 热重载

热重载只替换已经被 BML 发现的脚本 mod runtime，不会在运行中新增 mod，也不会重排依赖图。

常用命令：

```text
script reload                  # 重载全部脚本 mod
script reload <id>             # 重载一个脚本 mod
script reload <id> --dry-run
script reload <id> --dry-run --check-state
script reload <id> --force-exports
script diag <id>
script logs error
script panel
```

关键规则：

- 编译失败或 metadata 失败时，如果旧 runtime 存在，旧 runtime 会继续运行。
- 启动时首次编译失败的脚本会保留 failed placeholder。修好文件后可以用 `script reload` 或 `script reload <id>` 恢复。
- 运行中放入新的 `*.mod.as` 不会新增 mod。新增 mod 需要重启 Player。
- 改 mod id、改 dependency 声明、删除或改签名旧 export，默认会拒绝热重载。只有明确知道调用方能接受时才用 `--force-exports`。
- 旧 Timer、Command、DataShare、callback、export method handle 在替换后失效。新资源必须在新 `OnLoad` 里重新注册。
- BML 只能恢复自己持有的脚本资源，不能自动撤销脚本改过的游戏世界状态。

状态迁移示例：

```angelscript
int counter = 0;
string mode = "idle";

void SaveState(BML::StateBag@ state) {
    state.SetInt("counter", counter);
    state.SetString("mode", mode);
}

void MigrateState(const string &in fromVersion, BML::StateBag@ state) {
    if (fromVersion == "1.0.0" && state.Has("oldMode")) {
        state.SetString("mode", state.GetString("oldMode", "idle"));
        state.Remove("oldMode");
    }
}

void RestoreState(BML::StateBag@ state) {
    counter = state.GetInt("counter", counter);
    mode = state.GetString("mode", mode);
}
```

`StateBag` 只保存 `bool`、`int`、`float`、`string`。不要把 CK 句柄、脚本对象、Timer、Command、DataShare、ModRef、ExportRef 放进去。状态迁移方法应只复制/转换纯数据，不注册资源、不执行命令、不修改游戏世界。资源在 `RestoreState` 后的 `OnLoad` 里按当前状态重新创建。

## 规则

- 不要在 `GAME_EVENT_START_LEVEL` 之前查询关卡对象
- 在 `PRE_EXIT_LEVEL` 或 `PRE_LOAD_LEVEL` 时清除关卡状态
- 禁止在 `OnProcess` 中每帧打日志
- 成员变量在帧之间持久存在
- 回调签名必须精确匹配，否则 BML 不会调用
- 如果需要跨热重载保留成员变量，实现 `SaveState` / `RestoreState`，版本升级时再实现 `MigrateState`
