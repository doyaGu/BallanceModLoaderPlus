# 选择原生 API

原生作者使用版本化 C 表之上的 C++ wrapper；类、STL 和虚调用留在 Mod 模块内。
C11 作者直接使用同一份 .h 表。原生和脚本共享同一份 Loader 服务状态。

| 需要 | 使用 |
| --- | --- |
| 身份、生命周期、原有游戏事件名 | Mod.h/.hpp、Instance、不可变 Definition |
| 日志、目录、Mod 查询、资源 | Mod::Context |
| 配置值和菜单编辑元数据 | Config::View / Property |
| 一次、循环、有限重复计时器 | Timer::Registration，显式毫秒/tick |
| 键鼠/手柄快照、计数屏蔽 | Input::View / Block，RAW 读取原始状态 |
| 命令与补全 | Command::Registration |
| 对象身份、场景状态 | Scene.h、已有 ObjectRef |
| 游戏、作弊、分数、能量、关卡数据 | Gameplay.h |
| CK 时钟/tick、计速器 | Time.h/.hpp、Speedrun.h |
| 装载阶段球/地面/机关类型注册 | Gameplay 定义和显式 Context |
| 下一次 Render 跳帧、宽屏 | UI::SkipNextRender / SetWidescreenAdjustment |
| 初始条件、可见性 | Scene::SetIC / RestoreIC / Show |
| 消息、HUD、Loader 菜单 | UI.h |
| Mod 详情的自定义页面 | ModMenu::Page |
| Ballance 风格即时 UI | Bui.hpp、本地 Window/Page/Menu |
| 引擎 2D UI | Gui.hpp、本地 BGui 控制器 |
| 原型、Block、执行、图编辑 | Behavior.hpp、Behavior/Blocks |
| 直接 CK 对象/管理器 | Engine.hpp + BML::Engine，然后复用现有 CK2 API |
| 直接使用宿主 ImGui | ImGui.hpp + BML::ImGui |
| 高级控件 flags/callback | Bui/ImGui.hpp，依赖 Engine + ImGui |
| BGui 的 Vx/CK 便利操作 | Gui/Engine.hpp，依赖 Engine |
| 命名字节共享 | DataShare |
| 原生/脚本类型化服务 | 生成的 IMC RPC/Topic/future |
| 原生 provider 进程内 C 表 | Interface.hpp，并声明 provider 依赖 |
| 既有文件/ZIP/编码/内存工具 | BML.h，按各自约定 BML_Free |

BMLAll.h 汇总核心。引擎相关 Blocks、ScriptHelper、Guids、TypeConvert 和直接 CK/ImGui
访问需要显式扩展目标。IMod、IBML 及旧服务虚表保留为本分支宿主内部适配，不再增加公开兼容代理。

普通跨 Mod 服务用 IMC。确需进程内函数表的原生基础设施可以使用 provider C 表，消费者必须声明
依赖。宿主先完成消费者卸载，再清理 provider；缓存一个未声明 provider 的指针不构成保活。

新调用通常返回 BML_OK 或负错误码，Result 只拥有显式请求的值，事件视图仍为借用。
主通知失败会停用 owner；普通服务错误、命令退出状态由调用方处理。
旧 BML.h 工具保留其 0/1 约定，默认服务和引擎操作仍要求游戏线程。

参见[原生 API](native-mod-api.md)、[迁移（英文）](../en/native-mod-migration.md)、
[Behavior](behavior-authoring.md)、[IMC](imc.md)。
