# 原生 API 该走哪条路

原生 Mod 有三种方式访问 Loader，部分能力可以由其中不止一种提供。本页给出每项
能力有哪些写法、该优先选哪个，以及为什么会同时存在多种写法。IMC 不在这三种之内：
它不承载任何 Loader 能力，是一个 Mod 发布给别的 Mod 的东西，见
[跨 Mod 通信](imc.md)。

如果只想要一条规则：凡是要拿到引擎对象、或只有它们提供的能力，走旧式 `IBML` 与
`IMod`；读取游戏状态、接收 Loader 事件、控制 Loader 自己的界面，走 interface
struct；要把接口发布给别的 Mod，走 IMC。

## 为什么会有多种写法

`IBML`、`IMod`、`IMessageReceiver`、`IConfig`、`ICommand` 都是带虚函数的 C++ 类，
而 Mod 是跨 DLL 边界调用它们的。虚函数调用跳转到的槽位号在 Mod 编译时就已固定，
因此新增、删除或重排任何一个虚函数都会移动它之后的全部槽位，已经构建好的
`.bmodp` 会调到错误的函数上。Loader 用 `src/ModContext.cpp` 里的静态断言钉住这些
槽位号，并在每次构建时把导出符号集与
`tests/abi/legacy-native-exports-x86-msvc.txt` 比对。所以这些接口在当前发布线上是
冻结的：不能再往里加东西。

interface struct 没有这个问题。Loader 交出的是一个函数指针结构体，Mod 通过唯一的
导出 `BML_GetInterface` 按 id 和主版本号取用它，而这个结构体只会在末尾追加成员并
提升次版本号。旧 Mod 继续调用自己编译时就有的那些成员；新 Mod 用 `BML_IFACE_HAS`
询问正在运行的 Loader 有没有它之后才加进来的成员。这就是冻结之后新增的能力都以
interface struct 形式出现的原因，而且每个都配了一层 inline C++ 命名空间，用起来和
调用普通函数一样。

`BML.h` 里的 `BML_*` 函数是第三种写法。它们是纯 C，不涉及任何 vtable，所以可以
自由增加。但能落在这里的只有两类。一类是与游戏和 Loader 状态无关的纯工具：字符串、
路径、文件、编码与内存分配，Loader 与 Mod 目录查询属于这一类。另一类是补齐一个另
一半冻结在 C++ 里的操作，`BML_UnregisterCommand` 就是这样站到 `IBML::RegisterCommand`
旁边的。把一对操作的正反两半拆到两种机制上，比任选一种都更难读，所以反向操作跟着
正向走。

新能力该落在这三种里的哪一种，由一个问题决定：谁来提供它。Loader 提供、Mod 读取或
驱动的，是 interface struct。某个 Mod 提供给别的 Mod 的，是 IMC 接口，因为 Loader
不参与这段对话，而且双方各自按自己的节奏发布。与游戏和 Loader 状态都无关的纯工具，
或者某个已冻结在 C++ 里的操作的反向操作，走 C 导出。

冻结不等于弃用。旧式接口仍在支持，Loader 的大部分能力仍然只有它们提供，而且它们
是唯一能拿到引擎对象的途径。

这些能力在脚本侧有五个够得着：`BML::Runtime`、`BML::Gameplay`、`BML::UI`、
`BML::Events`、`BML::Speedrun`。只有 `BML::Scene` 只在原生侧。脚本侧的
`BML::Speedrun` 目前并不是原接口的投射：它的写法是 `SetTimerVisible`、
`StartTimer`、`PauseTimer`、`ResetTimer`、`GetElapsedTime`，返回的是值或者什么都不
返回，而不是状态码。脚本侧的投射是手写的，与 interface struct 之间没有任何校验，
所以无论是缺的那项能力还是写法上的差异，都不会自动收敛。

## 逐项能力对照

`旧式 C++` 一列若未特别说明，均为 `IBML` 的成员。`新路线` 一列里写成
`命名空间::函数` 的，是 interface struct 外面那层 inline C++，声明在 `include/BML/`
下的同名头文件里；其余的是 `BML.h` 的 `BML_*` C 导出。

| 能力 | 旧式 C++ | 新路线 | 选哪个 |
| --- | --- | --- | --- |
| CK 上下文、渲染上下文与各引擎管理器 | `GetCKContext`、`GetRenderContext`、`GetInputManager`、`GetTimeManager` 等 | 无 | 只有旧式 C++。interface struct 有意不交出引擎指针：指针无法带上一个对方能校验的生命期。 |
| 是否在关卡内、是否暂停、是否运行、是否开作弊 | `IsIngame`、`IsPaused`、`IsPlaying`、`IsCheatEnabled` | `Runtime::ReadState` | 两者皆可。门面一次读回五个标志，且不要求调用方是 `IMod`。 |
| 帧时间与帧计数 | `GetTimeManager()` 与 CK 时钟 | `Runtime::ReadClock` | 两者皆可。 |
| 竞速用时与 highscore 数值 | `GetSRScore`、`GetHSScore` | `Runtime::ReadScore`、`Speedrun::ReadTimerState` | 两者皆可。`Score::SR` 是以毫秒计的竞速用时，不是分数。 |
| 启动、暂停、重置或显示竞速计时器 | 无 | `Speedrun::StartTimer`、`PauseTimer`、`ResetTimer`、`SetTimerVisible` | 只有 interface struct。 |
| 按名字查找对象 | `Get3dObjectByName`、`GetGroupByName`、`GetMaterialByName` 等一整族 | `Scene::FindObject`，可带 class id | 拿到之后还要用 CK SDK 操作它，就走旧式 C++，因为它直接给出指针。`Scene::FindObject` 给出的是 `BML_ObjectRef`，适合只需要标识或转手传递的场合。 |
| 读取对象的类、名字或变换 | 经指针使用 CK SDK | `Scene::ReadObject`、`Scene::ReadEntityTransform` | 两者皆可。 |
| 关卡状态、能量、检查点、重置点、关卡目录 | `GetArrayByName` 加 `CKDataArray` 按列读取 | `Gameplay::ReadLevel`、`ReadEnergy`、`ReadCheckpoints`、`ReadResetpoints`、`ReadCatalog` | 走 interface struct。它已经知道游戏那些数组的列顺序，而这正是最容易写错的部分。集合类读取会整份拷贝，属于初始化或换关时做的事，不适合每帧调用。 |
| 游戏内消息板 | `SendIngameMessage` | `UI::AddMessage`、`UI::ClearMessages` | 两者皆可。清空消息板只有门面能做。 |
| HUD 各部分、Mod 菜单、地图菜单 | 无 | `UI::SetHUDMode`、`ShowTitle`、`ShowFPS`、`OpenModsMenu`、`CloseModsMenu`、`OpenMapMenu`、`CloseMapMenu` | 只有 interface struct。 |
| Loader 事件 | `IMod` 上的 `IMessageReceiver` 虚函数 | `Events::Stream` | 两者皆可，事件内容相同。虚函数在 Loader 的派发过程内执行，且要求继承 `IMod`。流是一个自己排空的队列，适合不是 `IMod` 的代码、对所有事件一律处理的代码，以及宁愿先缓冲而不是当场响应的代码。 |
| 作弊模式 | 写用 `EnableCheat`，读用 `IsCheatEnabled` | `Runtime::ReadState` 可读 | 读两者皆可，写走旧式 C++。 |
| 控制台命令 | `RegisterCommand` 加 `ICommand` 子类 | 无 | 注册走旧式 C++。注销是 C 导出 `BML_UnregisterCommand`，因为 `IBML` 已经无法再加函数。 |
| 配置 | `IMod::GetConfig` 加 `IConfig`、`IProperty` | 无 | 只有旧式 C++。 |
| 定时器 | `AddTimer`、`AddTimerLoop` | 无 | 只有旧式 C++。 |
| 退出游戏、初始条件、显隐、物理类型注册、跳过一次渲染 | `ExitGame`、`SetIC`、`RestoreIC`、`Show`、`RegisterBallType` 等注册族、`SkipRenderForNextTick` | 无 | 只有旧式 C++。 |
| 已加载了哪些 Mod，以及依赖 | `GetModCount`、`GetMod`、`FindMod`、`RegisterDependency`、`CheckDependencies` | 无 | 只有旧式 C++。 |
| 把自己的接口发布给别的 Mod | 无 | IMC，最好从 `.imc` 文件生成 | 只有 IMC。自己定义 C++ 类，等于把自己的 vtable 布局和标准库塞进每个使用方的构建里；`BML_GetInterface` 也不是替代品，它交出的是 Loader 自己的接口，Mod 无法往里添加。IMC 到达的是原生使用方：脚本 Mod 目前既不能调用别的 Mod 的路由，也不能发布自己的。 |
| 绘制自己的界面 | `Bui` 画 ImGui 控件，`BGui` 用游戏内 2D 实体 | 无 | 这两者都不是 `BML::UI`，后者控制的是 Loader 自己的界面，不画你的东西。 |
| 字符串、路径、文件、内存分配 | 无 | `BML.h` 的 `BML_*` 函数 | 走 C 导出。它们返回的东西要用对应的 `BML_Free*` 释放，不能用 CRT 的 `free`。 |
| Loader 的各个目录，以及自己 Mod 的安装目录 | 无 | `BML_GetLoaderPathW`、`BML_GetLoaderPathUtf8`、`BML_GetModRootW`、`BML_GetModRootUtf8`，同样是 `BML.h` 的 C 导出 | 走 C 导出。`IBML` 从来没有提供过这些。Loader 目录是借用指针，Mod 根目录是新分配的，只有后者需要释放。 |

## 能不能混用

混用是预期用法，同一个函数里同时用三种也可以。interface struct 由 Loader 自己实现，
读的是 `IBML` 读的同一份状态，因此不存在第二份副本，也没有需要同步的东西。
`Runtime::ReadState` 与 `IsIngame` 不会互相矛盾。

会显现出来的差异有三处：

- **失败怎么报告。** 旧式 C++ 函数大多直接返回值或什么都不返回，`RegisterCommand`
  失败时只写日志。interface struct 的函数一律返回状态：`BML_OK`，或者 `Defines.h`
  里的某个负数错误码。它们外层的 inline C++ 包装带 `[[nodiscard]]`，要么处理这个
  状态，要么显式转成 `(void)`。
- **拿回来的是什么。** 旧式 C++ 查找给的是引擎指针，只在对象存活期间有效，且只能
  在游戏线程上使用。门面给的是值：一个普通结构体，或者一个 Loader 在解析前能先
  校验的 `BML_ObjectRef`。
- **线程。** 旧式 C++ 接口只能在游戏线程上用。interface struct 的调用是在调用线程上
  直接进入 Loader，不入队、也没有什么要等，这就是 `Runtime::ReadState` 能在
  `OnProcess` 里用的原因。`BML::Gameplay`、`BML::Scene`、`BML::UI` 触碰的是游戏的
  数组、游戏对象和 Loader 自己绘制的界面，`BML::Events` 的队列不带锁，因此这四个从
  别的线程调用一律返回 `BML_ERROR_WRONG_THREAD`。`BML::Runtime` 和 `BML::Speedrun`
  不拒绝其他线程，但同样是给游戏线程用的。在 Loader 加载完 Mod 之前，它们全都返回
  `BML_ERROR_FAIL`。

## 延伸阅读

- [原生 Mod API 概览](native-mod-api.md)
- [跨 Mod 通信](imc.md)
- [编写类型化 IMC 接口](imc-author-guide.md)
