# Behavior Runtime 剩余能力

最后更新：2026-09-11。公开接口主体已实现，没有“声明了但只是空壳”的大块功能。
空闲 Node Replace/Remove、AngelScript `BML::Behavior` 投影、GameplayTweaks /
GameEventHooks / NewBallType 的 Plan 迁移，以及拆开的 Player probe 都已在树里。
下面是仍未完成的能力。

## 核心建模

- [x] Parameter Operation：`Edit::AddOperation` 按 operation GUID 和精确 type tuple 创建
  graph-owned `CKParameterOperation`，复用 Bind 连接输入，保留 Virtools 的惰性求值。
- [x] 空闲 Node Replace / Remove：`Edit::Replace` 用相同公共接口的 Block 换掉空闲
  child；`Edit::Remove` 停放节点和入射 Link，关闭 Patch 时按原 identity 恢复。
  活动节点替换、进行中的延迟 Link、以及公共接口不一致的替换都不在 1.0 内。
- [ ] 完整的动态 interface 编辑：动态 In、Out、Pin、Pout 已有基础；Local 可加入 graph root
  或同一 Edit 新增的 Block，但不能修改 borrowed Node 的实现私有状态；任意 Target/interface
  重构尚无完整、稳定的作者模型。

## 类型与执行边界

- [ ] 第三方参数类型：只支持已知安全格式和 object-derived 参数；含 provider-owned 内存或
  复制/销毁回调的类型不能安全形成 owned Frame；没有自定义类型注册，也没有 opaque-byte fallback。
- [ ] `CKBR_BREAK`：Detached Runtime 报告 `UnsupportedBreak` 并终止；未实现 Virtools
  debugger/message-pump 意义上的恢复，也不应简单当成 retry。
- [ ] Detached compatibility 的权威判断：Prototype discovery 已实现，但 CK2 元数据无法证明
  一个 Prototype 适合 detached execution；未分类 BB 显示 `Unverified`，没有 provider 显式声明机制。

## 图观察能力的固有限制

- [ ] 精确的参数变化通知：Watch 每 game frame 采样；CK2.1 没有公开、可靠的
  parameter-data notification seam，因此不是事件驱动观察。
- [ ] Parameter Operation 结果的非侵入观察：Graph 可枚举 Operation，但 `Graph::Read` 遇到
  operation source 仍报告 `Indeterminate`；观察 API 不会擅自触发 Virtools 的惰性求值。
- [ ] 部分运行时状态无法可靠恢复：某些 delayed-list membership 只能报告 `Unknown`，
  不根据残余字段猜测 Virtools 调度状态。

## 面向作者的覆盖

- [x] AngelScript Behavior 投影：每个 Script Mod 有 owner-scoped 的 `BML::Behavior`，
  覆盖 Block、Run、Frame、Inspect、Watch、Edit、Patch、Plan 和 Hook。脚本 Patch 一次一个
  Graph，脚本 Plan 一次一条 Script 规则；typed `Behavior/Blocks/*.hpp` 没有 AS 投影。
- [ ] 更高层组合能力：C++ facade 已比 C seam 易用，但没有面向常见模式的组合库
  （条件路径、参数变换、可复用 Patch 模板）。Transform 模型稳定前不堆语法糖。

## 工程闭合

- [ ] 内部调用迁移：ExecuteBB 已是 Runtime adapter；Physics Force、NewBallType、
  GameplayTweaks、GameEventHooks 已走 Plan/Edit。剩余直接操作 CK graph 的旧调用需逐个判断
  是查询、底层 graph primitive，还是应迁移到 Patch/Plan。
  - 保留为 QUERY：`ScriptHelper` 的 Find* 系列、`BMLMod` 的字体查找、
    `ObjectLoadHook`/`PhysicsHook` 的参数读取、`ModContext::OnLoadGame` 的脚本枚举。
  - 保留为 PRIMITIVE：`ScriptHelper` 公开 API 本身（`CreateBB`/`CreateLink`/`InsertBB`/
    `DeleteLink`/`DeleteBB`），`CKGetPrototypeFromGuid(...)->SetFunction` 原型钩子，
    `BML.cpp` 的 `CreateCKBehaviorPrototypeRunTime` 重定向，`Bui::ActivateScript`。
    注意 `ScriptHelper::DeleteLink` 持有一个进程级静态 `Nop` block，跨关卡不销毁。
  - 仍待迁移：
    1. `CustomMaps::PatchLevelLoader`：一个新 block、一个 local、两条 Link、一次重定向。
    2. `ScriptHookBlockService::InsertAfter/InsertBefore/InsertBetween` 与 `RestoreHookBlockGraph`：
       三份“把 HookBlock 拼进 Link”的手写 splice 和回滚。
    3. `UI/Gui/Label`、`Input`：每帧 `ActivateInput`+`Execute`，析构直接 `DestroyObject`
       Runtime 拥有的 Text2D block；应改为 Pulse 与 Close。
    4. `CustomMaps::OnProcess`/`LoadMap`：从 UI 激活原版 block，需要 Runtime 的外部激活入口。
    5. `BMLMod::OnEditScript_Menu_OptionsMenu`：新 block、`AddInput`/`AddOutput`、`ShareSourceWith`、
       Link 重定向和一次 `Secure Key` 参数写入的混合体。
  - 重复实现：`BMLAS_CK_LoadObject` 与 `NewBallTypeMod::OnLoadBalls` 各自复制了
    `ExecuteBBAdapter::LoadObjects`；`ScriptHookBlockService` 内部重写了 ScriptHelper 的 Link 查找。
- [x] 真实 Player 覆盖：Hook 错误阻断控制流、动态 Port 删除后的 `EDITED`、Target 变化影响
  fingerprint、Splice anchor 外部漂移、`CKPGUID_MESSAGE` 真实发送、Physics Force retiring
  session 隔离、teardown 重入、空闲 Node replacement、AngelScript authoring 均有独立断言。
- [x] Player 测试结构：Behavior 按主题拆成 ScriptHook、RuntimeSemantics、Patch、Facade、
  Transport 五个 Mod。

## 建议顺序

1. 迁移 CustomMaps、HookBlock splice、BGui Text2D 驱动/拆除、Options 菜单。
2. 设计完整的动态 interface / Target 重构模型。
3. 补齐需要明确求值语义的 Parameter Operation 结果读取。
4. 最后考虑第三方参数扩展和组合库。
