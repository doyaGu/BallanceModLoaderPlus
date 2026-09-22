# 原版 IVP API

独立基础 Mod `IVP.bmodp` 提供对 Ballance 原版 `physics_RT.dll` 内嵌 IVP 引擎的
原生访问。它通过 BML 的 provider-interface 注册表发布 `bml.ivp`，不导出任何 IVP API
符号，使用方也不链接它。使用方在构造函数中声明 `AddDependency(IVP_MOD_ID)`，再通过
BML 取用接口。它不是独立物理引擎，也没有链接或整体搬入其他版本的 IVP。

目标二进制固定为：

- Ballance 安装目录下的 `BuildingBlocks/physics_RT.dll`
- x86，PE 时间戳 `0x3DAC380C`，映像大小 `0x81000`
- SHA-256 `E72E4AFCFA5C33A7D3D27776137F8C997B3C52D89D8A8A4745F1CA21E45893EC`

## 发布清单与 Retail Contract

公开 IVP 文件不再由 umbrella header 和安装脚本分别维护。
`cmake/IVPPublicSurface.cmake` 是经过审核的安装清单，并在配置时检查磁盘上的公开
header 与 `<BML/IVP/IVP.h>` 是否一致。隔离安装消费测试只使用安装目录编译完整 umbrella，
因此不会再把源码树中存在、SDK 中缺失的文件误判为可用。此前漏装的 `Attacher.h`、
`Car.h`、`ConstraintCar.h`、`Forcefield.h`、`ObjectAttach.h`、`RaycastCar.h` 和
`Reaction.h` 已全部纳入。

`tools/ivp/retail-contract.json` 是人工审核的 IVP Retail Contract，集中保存目标 DLL
身份、指令锚点、运行时对象偏移、685 个 code RVA、5 个 data RVA、681 项直接依赖闭包、
14 个间接调用点和 1071 条公开候选逐项证据。运行：

```powershell
python tools/ivp/sync_retail_contract.py --check
```

可检查 `Calls.h` 使用的地址 include、runtime image include 和三个 TSV 视图没有漂移；
`--write` 只用于在审核 catalog 修改后重建这些派生文件。修正 IDB 的 1600 项符号 manifest
和 879 项 guarded function corrections 仍是独立产物：它们包含尚未升级为 retail-confirmed 的名称、
类型和 vtable 信息，不能因为 catalog 的存在而被当作 ground truth。

`tools/ivp/Audit-IvpIdbDirectTypes.py` 可通过 `idat.exe` 对 Retail Contract 的直接依赖做
只读检查。canonical IDB 已通过事务提交全部 681 个目标的非空函数原型，包括两个 protected
Phantom transition、Core worst-case mass、匿名 OV Element、OV Node/Tree Manager、Spring、
Mindist Manager、`IVP_Hash`、`IVP_U_Memory`、Matrix Cache initializer 与三个 3D Solver 入口。关闭重开后的
直接类型回读为 681/681；
该工具只输出缺失项，
避免为了审计生成大段反汇编文本。

`tools/ivp/Audit-IvpIdbPublicAddressTypes.py` 从公开头文件反向枚举调用目标，当前确认
静态反向闭包现为 681 个 Address ID 对应 680 个唯一公开 RVA，未解析 ID 为 0，
原型回读门禁已通过 681/681。该门禁与
Retail Contract 的正向清单同时运行，因此以后新增包装却忘记入账会直接失败，而不会再次
得到只覆盖人工子集的“完成”数字。

更宽的 `tools/ivp/Audit-IvpIdbPublicTypes.py` 消费公开接口 methods TSV，只检查能以邻近
装饰名精确解析到零售函数体的 409 项。当前 409 项全部有原型，缺失原型和未解析名字均为
0；这仍与“名称正确”分开统计，防止把 IDA 能显示函数名误当成参数和返回 ABI 已完善。
最后一批覆盖 utility/cache transform、cache/hash、对象与 controller 生命周期、material、
anomaly 和 object-space ray solver；参数、返回值与 calling convention 由零售装饰名、`ret`
及调用点共同约束，不把导入 UDT 当作唯一依据。

`tools/ivp/Audit-IvpIdbNamedFunctionTypes.py` 将缺失原型检查扩展到主库全部 1044 个带
`IVP_`/`ivp_` 名称的函数。十三批补齐 Object/Cluster 生命周期、Friction System、
Simulation Unit、core memory/math、Buoyancy、Mindist/Minimizer/Event 与 hash/pair
拓扑、Impact System、OV/delegator/surface collision 链、Compact Ledge/3D Solver
几何核、Linear Constraint Solver 状态机，以及剩余 utility、Buoyancy、Surface Builder、
递归 compact geometry、mass center 和 polygon/tetra 入口，共补齐 253 项原型，使空原型
从 253 降到 0；当前 1044/1044 项均具有可解析原型。反向公开地址审计另发现并修正了此前
不在旧子集中的 `IVP_Mindist_Manager::insert_exact_mindist` 与 `p_strdup` 原型。这个数字不表示所有导入语义类型均已
最终确认。

主 IDB 只允许通过事务包装更新。执行前须通过 `IVP_REFERENCE_ROOT` 提供对照
IVP 源码目录的绝对路径：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/ivp/Invoke-IvpIdbTransaction.ps1
```

普通查询同样不直接打开主库，而是在自动清理的一次性副本上运行：

```powershell
& tools/ivp/Invoke-IvpIdbReadOnly.ps1 `
  -Script tools/ivp/Inspect-IvpTypes.py `
  -ScriptArgument IVP_Core
```

底层 runner 会拒绝文件名为 `physics_RT-analysis.i64` 的数据库，因此遗漏包装器不会再把
探索脚本或自动分析写进主库。

该命令先核对指定 Ballance 安装目录中 DLL 的 SHA-256，并在 IDA 仍打开时拒绝运行；随后
先从当前头文件/证据现生成一次性 methods TSV，再把主库复制为同目录临时库，只在临时库
应用修正并暂存符号 manifest，关闭并重新打开后依次执行全命名、直接依赖、
公开、constructor、complete destructor、deleting destructor、ownership comment、
complete-object lifetime comment、direct complete-destructor comment、`long double`、
binary64 UDT 布局、位域存储和虚表 address point 十四项审计，最后才以同卷覆盖
移动提交。应用、保存、重开或任一审计失败都只删除临时 IDB 和 manifest，不触碰两个正式
产物，也不保留长期备份。公开精确类型审计显式读取该次暂存 TSV；不会再因复用旧
`ivp-methods-current.tsv` 而让新命名函数逃过同一事务。

该审计还逐方法收集实际引用的 `ABI::Address` 标识并与自身装饰名 RVA 对照，而不是把任意
`Invoke`（例如字符串分配器调用）误算成调用自身零售函数体。409 项中 397 项直接进入自己的
DLL 地址；其余 12 项是逐项登记的分层析构或 thunk 路由，因为进入 C++ 析构
函数体时基类/成员生命周期已由编译器安排，再调用 DLL complete body 会重复处理。合计 409/409，
当前没有无法解释的
`missing-retail-route`。

17 个确定性物理场景现在共享 `tests/IvpTestAdapter.cpp` 中唯一的 `BML_IvpInterface` 和
版本协商；每个场景仍在本地登记自己的 `Address -> callable`，并保留为 `__thiscall` 模拟
ECX 的 x86 `__fastcall` shim、状态和物理不变量。共享 adapter 没有进入生产代码，也没有
把测试控制流模型变成原版实现。另有 `IvpPublicHeadersCompile` 逐个独立包含所有公开 header；
必须等待完整协作类型才能定义的方法记录在 `tools/ivp/header-completion-sites.tsv`，因此不会
为了缩短大 header 而贸然移动 inline body、形成 include cycle 或改变消费者链接语义。

`IvpRetailContactLifecycleTest` 与上述 shim 场景分开：它从配置的 Ballance 安装目录加载
精确的 `BuildingBlocks/physics_RT.dll`，直接执行原版 contact-created、post-collision 和
contact-deleted 全局派发函数，并让 Mod-owned listener 接收事件。该案例把存留槽位固定为
post、object-deleted、friction-created、friction-deleted、析构；同时直接执行原版
Environment object-created/deleted/frozen/revived 四个匿名保留体，验证倒序回调和 revived
回调内自注销。对应 event/contact payload 已用原版字段读写链固定大小与偏移。

## 使用方式

原生 Mod 包含完整的重建头文件：

```cpp
#include <BML/IVP/IVP.h>

IVP_Environment *environment = BML::IVP::Environment();
IVP_Real_Object *object = BML::IVP::RealObject(entity);
IVP_Core *core = object ? object->get_core() : nullptr;
```

`BML::IVP::Environment()`、`RealObject()`、`Core()` 和 `Material()` 返回借用指针。
不得释放这些对象，也不得跨越对象删除、取消物理化、关卡重置或环境重建保存它们。
所有 IVP 操作都应在游戏线程执行。

求解器临时状态可以使用原版事务池；一次 transaction 结束后，其中返回的全部指针立即
失效。`get_memc` 是经核对的 link-stripped 薄封装，实际分配仍进入原 DLL：

```cpp
IVP_U_Memory memory;
memory.init_mem_transaction_usage();
memory.start_memory_transaction();
auto *scratch = static_cast<float *>(
    memory.get_memc(64 * sizeof(float)));
// 在当前求解步骤内使用 scratch。
memory.end_memory_transaction();
```

固定大小的二进制 key 使用 `IVP_Hash(bucketCount, keySize, notFound)`；它不会把 NUL 当作
终止符。构造、析构、`add` 和 `find` 进入原 DLL，`remove` 与 source-inline CRC index 按
相同链布局重建。不要把它和字符串专用的 `IVP_U_String_Hash` 混用。

连续碰撞求交内部使用 `IVP_U_Matrix_Cache` 保存同一 PSI 区间内最多 21 个对象变换。
构造与时间码失效进入原版 `p_init`；按索引惰性计算和当前时刻读取是从零售调用点恢复的
内联体。它借用 `IVP_Cache_Object`、`IVP_Real_Object` 与 `IVP_Core`，不能延长这些对象的
生命周期；取得 locking cache 的调用者仍必须用 `remove_reference()` 配对。

`IVP_3D_Solver` 公开原版的最大偏差搜索、碰撞感知搜索和根细化三个保留体。Mod 派生类只需
实现 vtable slot 0 的 `get_value(matrixA, matrixB)`；原版 DLL 会在栅格搜索和根细化期间回调
该函数。`find_first_t_for_value_max_dev2` 在邻近版本中只是对保留 max-dev 入口的一行转发，
因此作为第 2 类薄重建保留。被 `#if 0` 裁掉且依赖 Ballance 缓存中不存在字段的
`find_first_t_for_value_no_zero_dev`，以及只有声明没有实现证据的 `print`，均明确禁用。

这里存在两个必须显式遵守的原版 ABI/边界条件。第一，VC6 的 `calc_nullstelle` 通过首个隐藏
栈参数返回 8 字节 `IVP_Time`，当前 MSVC 会把同一平凡类型放进寄存器；包装层使用专用 sret
thunk，不能改回通用 `InvokeThis<IVP_Time>`。第二，原版 collision 路径可能在 `tMax` 后再取
一个 `0.005f` 栅格样本，又没有 max-dev 路径拥有的 index-20 终止检查；调用者必须保证该额外
样本仍位于 21 槽缓存中，正常小于 0.1 秒的 PSI 满足这一条件。

## 原生 ABI 与跨 DLL 所有权

这套 C++ 接口固定为 MSVC x86 ABI，不是可移植的 C++ ABI。`BML_IvpInterface` 的每个
函数指针都显式使用 `BML_CDECL`；编译门禁分别在 `/Gz` 和 `/Gr` 默认调用约定下构建，
避免 Mod 翻译单元的编译选项静默改变接口。Windows 下包含原生 IVP header 时也会
拒绝非 32 位指针目标。

原版 `physics_RT.dll` 从旧 `MSVCRT.dll` 导入 `operator new/delete` 与 `malloc/free`，
BML+ 和现代 Mod 则使用 VCRUNTIME/UCRT。因此尺寸和 vtable 对齐仍不够：分配和释放必须
落在同一套 CRT 堆上。凡是会被原版引擎删除，或完整原版构造函数最终留下原版 vptr 的公开
类型，都提供不改变对象布局的 class-specific 标量 `new/delete`，并通过已校验 DLL 的原版
分配入口完成配对。数组 `new[]` 被有意禁止，因为尚无经过证明的原版 array-cookie 契约。
只能由工厂创建的 `IVP_Environment` 与 `IVP_Controller_Phantom` 只提供原版释放入口。
规则同样覆盖会被引擎回调删除的 Mod 派生 controller、material、collision filter、
forcefield 与 raycast car。

不得用 `malloc`、外部 placement storage、自定义 arena、`BML_MALLOC` 或其他模块的全局
`new` 创建可能进入 IVP 所有权回调的对象。Mod 的虚函数被 `physics_RT.dll` 调用时不得抛出
C++ 异常，也不得让异常跨过 DLL 边界展开。Mod 自有 policy/listener 必须先从 environment、
core、active set 或 listener list 完整注销，再结束生命周期。

推荐入口 `<BML/IVP/IVP.h>` 会在声明期间临时切换到原版类型使用的 MSVC 8 字节 pack，
结束后恢复 Mod 原先的设置。`IvpPackedConsumerCompile` 会从恶意的 `#pragma pack(1)` 作用域
包含该总头，锁定这两条规则；不要把声明复制到使用不同 pack 的本地头文件中。

`IvpCrossDllOwnershipTest` 用带计数的替代分配器锁定当前全部已审核类型；
`IvpRetailCrossDllOwnershipTest` 则真实加载指定游戏 DLL，确认原版 vptr、deleting destructor、
environment callback 与 active-set 自删路径。这一类错误无法由只在栈上构造对象的 ABI
测试发现。

目前的兼容层覆盖：

- `IVP_Environment`、`IVP_Object`、`IVP_Real_Object`、`IVP_Core`；
- 由原版 `IVP_Mindist_Settings` setter 和全局对象支撑的 collision-tolerance 读写；
- 可由 Mod 直接继承的原版六槽 `IVP_Material`、Simple 材质及默认材质管理器，
  控制器与 controller manager、完整 constraint 公共 vtable，
  以及 Ballance `IVP_Constraint_Local` 的创建、端点/core 查询和各轴约束修改接口；
- group/meta/exclusive-pair collision filter、active value manager、terminal，以及可组合的
  oscillator/mixer/filter/switch 表达式节点、performance counter；
- 定长二进制键 `IVP_Hash`，以及 friction/constraint solver 使用的 32-byte 对齐
  `IVP_U_Memory` 事务池；
- 对象和球模板、phantom 模板；
- buoyancy 模板、liquid surface descriptor、phantom core set、buoyancy attacher，以及公开的
  `IVP_Attacher_To_Cores<ATTACH_T>` 动态 core 集合挂接模板；另有供 Mod 派生的
  `IVP_Forcefield`，把 independent controller 持续同步到活动 core 集合；
- phantom listener、intruding object/core/mindist 查询、`0x40` Ballance Phantom
  完整构造/析构生命周期、完整 mindist 状态读取与 manager 的 exact/invalid/hull/phantom
  管理入口，以及 contact-point 受保护访问器；
- collision、collision delegator/root 的完整可继承回调接口，以及由原版五槽虚表驱动、
  能创建普通/Recursive mindist 的默认 `IVP_Collision_Delegator_Root_Mindist`；
- real/friction synapse 遍历、anomaly limits/manager 与标准重力 controller；
- 原版摩擦能量修正使用的 `0xA0` `IVP_Mutual_Energizer`，包括初始化、平移/旋转
  相对能量计算和按比例消能的四个保留入口；
- friction system 内嵌的 `0x08` static/energy 辅助控制器真实 ABI、四个保留执行入口，
  指向 `+0x08/+0x10` 内嵌实例的借用访问器，以及恢复源码可见性的三个 topology vector、
  状态位和 energy 字段；
- 原版栈上 `0x840` `IVP_Friction_Solver`，包括 11 个保留的矩阵、坐标、easing 和冲量
  求解入口、经 `calc_solver_PSI` 两个展开体确认的 distance-matrix column builder、两个
  release no-op 诊断接口、`IVP_Contact_Point::get_lt`，以及经字段布局核对的短 inline
  冲量 helper；
- compact ledge/triangle/edge/surface、surface manager；
- point-soup 与 ledge-soup 的 compact surface 构建入口；
- cache object 及对象/世界坐标变换；
- anchor、spring template、spring actuator，以及 `0x98` 次基类偏移经过验证的
  `IVP_Actuator_Spring_Active`；另外提供经 Ballance 布局核对的 Force、Torque、
  rotational-motor 模板、普通/Active actuator 和 environment factory；独立的 Stiff
  Spring controller/Active controller 及 Check Dist 的 anchor、hull 更新、输出和 factory
  也已补齐；Four Point/Stabilizer 提供四锚点受控 core 登记与两组距离耦合；车辆用
  Suspension 则提供压缩/回弹双阻尼、reduced virtual mass 适配和车身侧力限幅；
- 固定/可变时间推进、time-event 插入/移除/改期/计数/重置、标准 event manager、
  environment/controller/core/object 的完整时间重基准、PSI、全局/对象局部 object
  listener、constraint listener，以及完整 motion controller；Golem 公开模板和运行时
  policy controller 分别按 `0x38/0x130` 布局提供，Fixed-Keyframed 模板和运行时
  constraint 分别为 `0x24/0xE0`，支持 reference frame 中的关键帧位置和姿态跟随；
- 对象 radar 邻域查询、按目标对象选择性解绑接触、删除前的邻域恢复、兼顾复合对象偏移并
  同步维护碰撞缓存/mindist/broadphase/hull 的 beam 传送，以及共享 core 的立即停止模拟；
- 可由 Mod 实现并在 application environment 中安装的四槽 universe manager 流式加载策略；
- ray template、ray listener、单对象射线求最近命中；
- 车辆系统的 Ballance 版 28 槽抽象接口、`0x304` 配置、`0x308` 调试数据、skid 信息，
  以及 raycast wheel/temporary/axis 和单 core 内嵌 vector；`0x958`
  `IVP_Controller_Raycast_Car` 已按 Ballance 双基类/字段边界公开，Mod 只需派生实现
  `do_raycasts`，其余轮射线准备、接触、稳定杆、悬挂、转向、轮胎力、增压器和 controller
  生命周期由选择性重建实现；
- 早期 real-wheel 车辆约束接口：`0xB0` 的 wheel/body 节点、`0xA0` 的
  `IVP_Constraint_Solver_Car` 和 `0x30` 的有效质量矩阵 builder；四轮真实刚体案例会构造并
  求解 8x8 x/z 双向约束，验证每个轮 core 的接触点速度收敛以及总线性动量守恒；
- 原版向量、矩阵、四元数和 IVP 容器的常用接口；
- sparse great matrix 33/33、incremental LU 27/27、历史 `IVP_Complex_Simple` 7/7 的
  公开声明面，以及原版 linear constraint solver；有零售函数体或可核对邻近实现的部分
  可以直接调用，邻近树本身也没有实现的历史方法保留精确签名并标成 `= delete`，编译期
  明确拒绝优于伪造行为；
  Great Matrix 的 Crout LU 分解、求解和求逆则在核对原版布局后逐项重建；Fixed-Keyframed
  所需的 `0x148` core-reaction 求解器仅保留两个零售入口，其余短成员按已验证布局重建；
- 修正后 IDA 数据库中 1600 个函数名称/RVA 的只读查询，以及经版本锁保护的可执行
  RVA 解析；独立全库审计确认 47/47 个已命名 IVP deleting destructor 均具有具体
  `this`、32 位无符号 flags、指针返回值和匹配原版机器码的 MSVC x86 ABI，并确认
  69/69 个 complete destructor 均为具体 owner、无显式栈参数的 void thiscall；构造侧
  审计另确认 93/93 个 complete constructor 均按 MSVC x86 实体 ABI 在 EAX 返回具体
  `this` 指针，不能误写成源码层面的 void 声明。IDA legacy type parser 无法表达两项已核对
  的模板指针：RVA `0x10310` 的
  `IVP_Attacher_To_Cores<IVP_Controller_Buoyancy> *` 与 RVA `0xFE50` 的
  `IVP_U_Vector<IVP_Core> *`；IDB 使用 ABI 等价的 `void *`，具体语义类型保留在函数注释中。

`IVP_Real_Object::beam_object_to_new_position` 已可调用。对象姿态到 core 姿态的短转换，
以及被链接裁掉的 next-PSI transform 控制流，按邻近源码选择性重建；时间缓存失效、四元数、
exact/invalid mindist、broadphase、hull reset 和旋转轴计算仍进入 Ballance 保留入口。确定性
测试分别覆盖休眠复合对象传送和活动对象重复更新快路径。这属于“邻近源码 + 相邻零售函数体”
证据，不把已裁掉的方法伪装成原版保留函数体。

`IVP_Real_Object::change_unmovable_flag` 也已可调用。迁移会先断开共享 core 上每个对象的
接触，释放 movable/static 两种不同的 friction 表示，并冻结仍在活动仿真的 core；切到
静态时，再通过零售分配器和已核对的 `0x24` `IVP_Simulation_Unit` 构造函数替换仿真所有权，
清掉旧 controller 后只重新安装环境 gravity controller。反向切回可动时，则通过零售 hash
析构和 operator delete 销毁静态 friction hash。`SimulationUnit.h` 同时公开了匹配原版的
`0x24` unit、`0x1B0` manager 布局及保留入口。复合物体固定化和静态障碍恢复可动两个
确定性案例核对了事务顺序；顶层对象方法本身仍明确记为被裁掉后的选择性重建。

Simulation Unit 的 split/fuse 现也可调用。零售 `split_sim_unit`（RVA `0x11470`）在第三
分量仍残留时用 do-while 尾循环再分配 `0x24` MOVING unit，而不是邻近源码的递归；
`fusion_simulation_unities` 由 `IVP_Mindist::try_to_generate_managed_friction` 两次调用。
`union_find_get_father` 按 Core `tmp +0x228` 的 8 指令 walk 内联。确定性 host 案例覆盖
二分量拆分、三分量尾循环、fuse 注销 donor unit，以及 controller 去重与
`get_controller_priority` 排序。生产接口直接进入这些零售保留函数体；host resolver 只按
已解码控制流建立行为模型来验证状态不变量，不再为了测试而在公开头文件复制函数体。
这些是 Ballance 可达的 intern ABI，不进入 1575 公开候选分母，也不抬高 527 个公开 host
测试计数。

当前追踪的 681 个原版 IVP 直接依赖已逐项闭合：680 个使用类型化原函数包装，另 1 个被
证明只属于适配层 non-deleting 析构步骤，不作为普通 IVP API 公开。14 个额外的 IVP vtable
间接调用点也已核对。完整逐项证据见
[`physics_RT` 使用 API 覆盖清单](ivp-coverage.md)。

例如，对一个已知对象的表面做射线检测：

```cpp
IVP_Ray_Solver_Template rayTemplate{};
rayTemplate.ray_start_point.set(0.0, 10.0, 0.0);
rayTemplate.ray_normized_direction.set(0.0f, -1.0f, 0.0f);
rayTemplate.ray_length = 20.0f;

IVP_Ray_Solver_Min ray(&rayTemplate);
object->get_surface_manager()->insert_all_ledges_hitting_ray(&ray, object);
const IVP_Ray_Hit *hit = ray.get_ray_hit();
```

## 兼容性证据

头文件中的信息按以下优先级校验：

1. 原版 DLL 的指令、调用约定、构造函数清零大小、字段访问偏移和 vtable；
2. 原版 DLL 中保留的 MSVC 装饰名和函数边界；
3. IDA 中从源码导入并经过修订的类型；
4. 邻近 IVP 源码，仅用于恢复与原版二进制一致的内联逻辑。

IDA 类型和参考源码都不是单独的 ground truth。例如，IDA 导入的
`IVP_Environment` 曾把统计管理器识别为 `0x58` 字节，但原 DLL 构造函数明确清零
`0x60` 字节；参考源码的环境又多出原 DLL 不存在的 `constraint_listeners` vector，
而原 DLL 实际保留 `environment_manager` backlink。
类似地，参考源码的 spring template 多出一个原 DLL 不读取的字段。兼容层采用由
二进制访问偏移证实的 `IVP_Environment` `0x178` 和 spring template `0x38` 布局。

`IVP_FLOAT` 固定为 32 位 `float`，`IVP_DOUBLE` 固定为 64 位 `double`。IDA 在此
Windows x86 数据库中显示的部分 `long double` 参数实际仍占 8 字节，不能据此把
`IVP_FLOAT` 改成 `double`。

公开接口审计不再用一个百分比代表“准确”。当前候选集有 1575 个方法，其中 1549 个与
邻近公开声明精确匹配，2 个采用零售证据确认的 Ballance 签名变体，24 个后期接口必须
保持缺席；其中 17 个属于 Ballance 缺少 owner 布局、虚表或实现支撑的 MOPP
manager/builder 边界。所有差异均已逐项分类，未知签名和普通 declaration-only 都为 0。
零售函数体、适配层调用点、owner 布局、host 行为测试和真实 Player 验证分别计数，不能把
1549/1575 称为行为准确率。2026-09-07 的严格下限是
403 个 IDB 精确 complete-body 归属、13 个独立二进制确认的函数体变体、5 个单独分类的
零售确认内联体、398 个直接
调用零售函数体的方法、6 个通过 14 个登记 vtable 调用点进入零售体的方法、87 个零售确认布局 owner、46 个仅 IDB
导入布局 owner、284 个明确以邻近源码为推定基础的方法、527 个 host 测试方法和
15 个 Player 场景方法。
各维度允许重叠，不能相加；详见
[`physics_RT` 使用 API 覆盖清单](ivp-coverage.md)。

可调用性现在也独立审计。脚本关闭 MSVC delayed template parsing，并合并类外 inline 与
显式 defaulted 定义，不再把真实头文件实现误报成普通声明。当前分类为：1434 个头文件
实现、90 个纯虚合约、25 个 Ballance 明确不可用接口，普通 declaration-only 为 0。
`IVP_Object_Attach` 的 attach、detach、reposition 三条路径均已选择性重建并通过生命周期案例。
明确不可用的历史入口使用 `= delete`，因此调用者
会在编译期得到错误，而不是到链接阶段才发现不存在的符号。

字段与自由函数另有独立审计。字段审计枚举 582 个公开字段：567 个与邻近公开声明的名称、
规范化类型和相对顺序精确一致，剩余 15 个全部记入
`tools/ivp/public-field-evidence.tsv`，其中 12 个是 Ballance 布局中确认不存在的后续字段，
3 个是 ABI 等价存储表示；未解释、位宽和顺序差异均为 0。Core friction/flags/old-sync、
Real Object 嵌套 flags、Compact Surface 的 8/24 位历史名称，以及 IDB 布局支持的
`IVP_Template_Extra::info` 已恢复。自由函数审计为 34/34：8 个直达 DLL、21 个选择性重建、
1 个组合相邻保留原语、4 个因 Ballance 缺少所需结构支撑而保持省略。

这三个大型 AST 审计已进入 CTest，并固定使用 64 位 Python；测试和 Clang 子进程均有超时，
防止 32 位 Python 内存耗尽后留下无法收尾的分析进程。

重建优先级固定为：DLL 有函数体就直接调用原版；DLL 无函数体、但 Ballance 的对象布局和
虚表支持该接口时，才根据邻近版本选择性复现；所需字段、尺寸或虚槽在原版中不存在时
直接省略或 `= delete`。source-reconstructed 方法不会获得伪造的零售 RVA。

`IVP_Core` 的内部/公开混合头单独审计，不混入上述 1575 分母：当前 83/83 个公开可见
成员签名与邻近 Ballance-era 声明一致，82/83 可调用。停止物理运动、冻结参考重置、
进入模拟、冲击旋转同步、冗余量重算、movement-state 分类、exact-mindist 刷新和完整析构
均直接使用零售函数体；速度 anomaly/clipping 与多对象 revive 等短方法按可见内联逻辑
恢复。质量/材质变化路径通过零售 contact refresh、material、virtual-mass、friction fusion
及 transaction-memory 入口重建。受保护的单对象构造函数也归入选择性重建：先调用
RVA `0xD2F0` 的原版 `CoreInitialize`，再通过 RVA `0x11EF0` 把得到的 simulation unit
登记到环境 manager，完整保持邻近源码的两步语义且不伪造构造函数 RVA。废弃的
merged-core 拆分入口以 `= delete` 明确拒绝，
不再把失败推迟到链接阶段。

方法审计覆盖 252 个 owner；其中 87 个具有零售指令/分配/vtable 支撑的整体布局证据，
46 个只有经复核的 IDB 导入布局，13 个明确没有跨边界实例布局；其余不会因为存在
`sizeof` 断言就自动升级证据等级。
87 个零售确认 owner 现已全部具备 x86 尺寸门禁；80 个含自有字段的 owner 还有关键
`offsetof`/继承尾部门禁。其余 7 个恰为 vptr-only 接口或不增加存储的模板/派生包装：
`IVP_Controller`、`IVP_U_Vector`、`IVP_U_FVector`、`IVP_U_BigVector`、`IVP_U_Set`、
`IVP_U_Float_Hesse`、`IVP_U_Hesse`。
全部 46 个 IDB-only owner 均有 x86 `sizeof` 门禁；其中 actuator、motion/golem、fixed
keyframed、forcefield、compact grid、Q12、interpolator、raycast car 和 statistics entity
等 41 个有自有字段的 owner 又逐字段回读主库的一次性副本，并补入关键
`offsetof`/继承尾部约束。其余 5 个是纯接口或无尾字段的基类包装，因此没有可表达的自有
字段偏移。六个此前混在 IDB-only 中的无状态 helper 已改列 layout-not-applicable，因为
它们的公开入口全部是 static，从不跨 DLL 传递 `this`。
`IVP_Compact_Surface` 则向相反方向提升：RVA `0x39600/0x39B30` 的写集覆盖完整 `0x30`
公开头，exact-DLL tetra/pointsoup 案例又验证了生成值，因此现在属于零售确认布局。
这些约束用于发现头文件漂移，不会把其余 owner 的证据等级自动提升为零售确认。
新加入的三个 constraint-car 类型和 `IVP_Object_Attach` 均在导入 IDB 中逐字段核对；
`IVP_Complex_Simple` 原先不在 IDB，现已按邻近公开头补入并明确标成 source-only，而不是
零售确认。直接基类顺序、virtual 属性和新虚槽顺序的审计均为 0 个未解释不匹配；3 个
仅由导入 Car vtable 支持的 virtual 版本差异单独标成 IDA 证据，不冒充零售确认。特别是原版
`IVP_Real_Object` 的前三槽为 deleting destructor、quat 更新、matrix 更新；浮力
attacher 为 6 槽；Anomaly 两个类型的析构分别位于末槽，而不是槽 0。

## 真实 Player 测试

下列命令按在 `mods/IVP` 中独立构建 IVP 的方式编写。仓库内构建见
[IVP README](../../README.md)。运行 Player 脚本前请将 `BML_BALLANCE_ROOT`
设为 Ballance 安装目录。

静态 RVA、类型尺寸或“返回值非空”不能证明 API 可用。当前保留四个相互独立、都从
原版菜单进入第一关并操作真实 `Ball_Wood` 的场景。每个场景是独立的测试 Mod，由
`PlayerFlowDriver` 驱动原版流程，并在玩法控制就绪后启动探针，因此任一场景失败都
不会掩盖其他场景。

首要场景是一个普通玩法 Mod 会实际需要的球速限制器：正常 Ball Navigation 输入先让
木球进入滚动状态；Mod 通过公开 IVP core 线速度和角速度施加等比例限速，同时确认球
仍受玩家控制并持续移动；解除限速后，再确认原版 Ball Navigation 能重新加速木球。

```powershell
cmake --build ..\build-ivp --config RelWithDebInfo --target IVP `
  IvpBallSpeedGovernorTest IvpBallStateRoundTripTest `
  IvpBallSurfaceRaycastTest IvpBallConstraintTest
tests\player\Invoke-IvpBallSpeedGovernorTest.ps1 `
  -BallanceRoot $env:BML_BALLANCE_ROOT
```

高级场景保留完整的运动状态往返恢复：在木球滚动时捕获 CK/IVP transform、线速度和
角速度，允许原版物理继续产生可测偏离，再恢复状态并确认后续仿真继续。它使用原版
`IVP_Core::transform_PSI_matrizes_core` 同步 PSI 状态；只调用 CK 的
`SetPosition`/`SetQuaternion` 会在下一次物理同步时被旧 core transform 覆盖。

```powershell
tests\player\Invoke-IvpBallStateRoundTripTest.ps1 `
  -BallanceRoot $env:BML_BALLANCE_ROOT
```

object-space compact-ledge helper 不再是纯本地重建。保留的 ledge-tree 遍历 RVA
`0x21EE0` 会直接调用原先未命名的精确 Ballance 函数体 RVA `0x21AB0`；它同时包含双三角
快速路径和一般凸 ledge 路径。但尽管邻近源码公开声明返回 `IVP_BOOL`，Ballance 机器码并未
在所有出口规范化 EAX。包装层因此按 `void` 调用原体，临时安装两槽 forwarding listener，
观察并原样转发命中回调，以 RAII 恢复调用者 listener，再返回稳定的源码级布尔值。强制使用
原版 DLL 的 host 案例覆盖三角形命中、未命中、重复调用后的 listener 恢复和 tetrahedron
一般分支；本地算法只作为没有 resolver 时的测试 fallback。

第三个场景从真实木球的 IVP 几何中心发射表面射线，同时走公开的 surface-manager
vtable 与 Ballance 原版 ray/compact-surface 路径，验证命中对象、球半径、命中距离、
表面法线和后续运动；此外从球心向下命中真实关卡的静态 polygon，取回 compact ledge，
再通过 ABI 包装显式调用恢复的精确 object-space ledge 函数体，并比较对象、ledge、triangle
与距离。它不是
只检查非空指针的 probe。

```powershell
tests\player\Invoke-IvpBallSurfaceRaycastTest.ps1 `
  -BallanceRoot $env:BML_BALLANCE_ROOT
```

第四个场景用 `IVP_Template_Constraint::set_ballsocket_ws` 把真实木球约束到世界，持续
施加正常 Ball Navigation 输入，验证球仍停留在锚点附近；随后通过
`IVP_Constraint_Local::free_translation_axis` 释放 X/Y/Z 三轴，验证同一木球重新移动。
同时核对 `get_objectR/get_objectA` 端点和 `get_associated_controlled_cores` 返回的真实
core，因而覆盖创建、对象布局、vtable 调用、求解行为和删除生命周期。

```powershell
tests\player\Invoke-IvpBallConstraintTest.ps1 `
  -BallanceRoot $env:BML_BALLANCE_ROOT
```

2026-09-01 对指定原版安装的最新实测结果：限速器把最大线速度限制为 `1.25`，限速期间
木球移动 `4.161395`，解除后速度回升到 `2.719653`；状态往返在偏离 `1.009137` 后恢复
到位置误差 `0.046073`、旋转误差 `0.000743`，线速度和角速度误差均为 `0`，随后继续
移动 `0.104141`。
射线场景同样已在指定安装通过：公开和原版路径都命中 `Ball_Wood`，半径为 `2.0`，
两次球面命中距离均为 `0.75`；静态关卡表面与直接 compact-ledge 重算距离均为
`2.020610`，最新复测期间球心继续移动 `0.504934`。约束场景中，持续输入 1.2 秒时木球
只移动 `0.004907`，释放三个平移轴后移动 `0.501212`；端点和 core 集合均与原版对象
映射一致。四个 runner 都显示
真实 Player 窗口、保存日志和截图，并在结束后恢复原安装。

这些场景证明活动玩家球映射、core 速度读写、原版唤醒路径、PSI transform，以及
surface/ray 几何查询路径，以及 Local ballsocket constraint 的创建、求解、修改和
删除路径；它们不替代尚未编写的 spring、buoyancy 和
surface builder 行为测试。

此外，构建内的领域测试覆盖共享 friction system 的选择性 contact 解绑、radar 的
exact/hull 筛选与回调方向、共享 core 的完整冻结状态迁移、事件队列/controller/core/
object hull/environment 的整条时间重基准，以及 Force/Torque/rotational motor 的双端冲量、
轴向冲量、功率换算、限速、Stiff Spring 的质量适配恢复/阻尼冲量、断裂监听、Active
更新，以及 Check Dist 的真实对象移动、阈值跨越和 hull min-list 生命周期。这些测试验证重建内联算法的状态
不变量；另有 Stabilizer 双 anchor-pair 距离差产生四个配对冲量、以及 pinned/sleeping
抑制的案例，以及 Suspension 对压缩/回弹方向、车身单侧限幅和质量适配的两个案例。
另有动态 core 集合案例覆盖已有 core 挂接、运行中添加/移除和集合关闭时的完整解绑；
两个 Forcefield 案例验证同一 controller 子对象对已有/新增 core 的登记、移除/析构注销、
listener 清理，以及 owner-set 销毁触发的自删除；
两个 Fixed-Keyframed 案例分别验证 reference frame 中的位置跟随与相对姿态修正，并检查
两端获得等量反向的线性/角向冲量和完整 controller 生命周期。
Core 策略案例另外验证 anomaly manager 对线速度/角速度的修正、逐轴 spin clipping，以及
当前速度和待提交速度的限幅规则。
Raycast Car 案例以四轮、双轴车辆配置验证 `0x958` 对象的轮距/轴距、悬挂参数、前轮转向、
后轮驱动扭矩唤醒、锁轮、增压器重入限制、调试射线数据，以及次基类 `this+4` 的登记和
注销；另一个空中 PSI 案例真正执行 controller 调度，检查四条轮射线、无命中时满伸/零
压力，以及额外重力和 booster 对车身速度与计时器的更新。它验证 host 内重建逻辑，
不宣称 Ballance 曾保留同类函数体。
real-wheel 约束案例则使用一个车身和四个独立轮刚体生成 8x8 有效质量矩阵，给每个轮子
不同的 x/z 初速度并执行一次 PSI；断言四个轮接触点分别收敛到车身表面速度，同时系统
x/z 总线性动量保持不变，并检查五个 core 的 controller 登记和析构解绑。它覆盖的是
实际求解路径，不是只构造对象的 probe。
它们不会被表述成已经通过真实 Player 的场景。

按值传递已有独立闭包审计：邻近公开面共有 20 个记录按值方法，真正跨入
`physics_RT.dll` 的 5 个全部使用 8 字节 `IVP_Time`；4 个记录返回值全部为本地内联，
这项结论只针对 `IVP_EXPORT_PUBLIC` 候选集。邻近 private collision header 的低层
`IVP_3D_Solver::calc_nullstelle` 另有一个已确认的 VC6 隐藏结构返回指针，并由专用 sret
thunk 和精确 DLL 根细化案例锁定。190 个枚举按值方法涉及的 38 个公开枚举均固定为
`std::int32_t`。`IvpByValueAbiCompile` 锁定声明和布局，`IvpRetailByValueAbiTest` 则对指定
原 DLL 实际运行 Core/Real Object 的 AT 插值、静止 Core 的 slow/calm 判定、性能窗口
重置和 Controller 的 `RET 8` 栈清理。

主 IDB 中两个 `calc_at_matrix` 入口现均正确标为返回 `void` 并接收 `IVP_Time`。所有
IVP/IVV UDT 下由旧导入器误写的 `long double` 节点也已递归改为 binary64 `double`；该
过程只重建标量、数组、指针和函数类型节点，并在每个 owner 上恢复原 `sda`、拒绝任何
总大小变化。另一个事务门禁检查 57 个直接含 binary64 的 UDT、7 个精确尺寸以及完整
`IVP_Environment 0x178` 偏移表；`long double` 残留和布局问题数均为 0。

位域另有独立的编译器门禁。它不是用 `sizeof` 猜测：Clang 以 i686-MSVC ABI 输出 10 个
owner、22 个声明位域的 byte/bit 位置，并与逐项证据 manifest 比较。Simulation Unit 的
混合 enum 位域仍共用开头的 32 位分配单元，Impact 临时记录保持 `8/2/22`，VHash 保持
`24/8`，Force 的两个开关保持 `+0x74` 的 bit 0/1。IDB 侧再独立回读 11 个 owner、23 个
存储字段；`IVP_Compact_Surface` 现在用匿名 union 同时公开历史
`max_factor_surface_deviation/byte_size` 8/24 位字段和原始 `factor_and_size` 字，
其 `+0x1C` 存储与原版/MOPP 一致。两项审计的问题数均为 0。

虚表也不再按导入的 `*_vtbl` UDT 猜测。构造函数写入的 vptr 共确认 75 个 address point、
335 个槽，其中包括被链接器剥掉装饰名的 construction vtable 和次基类表；18 张高风险表
逐槽固定原版目标地址，覆盖 Mindist 基表、Active Spring、active terminal、Recursive Mindist、
OO Watcher、OV Element、`IVP_ov_tree_hash` 与 Collision Delegator。`0x10063A24` 属于
Tree Manager 单独分配的 hash helper，非多态 Manager 自身没有虚表。Collision Delegator
一组尤其说明导入类型为什么不是最终真值：
`0x10063A34` 是两槽 Ballance 基表，紧随的 `0x10063A3C` 是另一张五槽 Root Mindist
表，并非一张连续七槽表。邻近源码新增的两个计数虚函数因此继续作为非虚兼容方法提供。
Mindist 基表也存在同类偏差：`0x100637B4` 只有 8 槽，而非导入 UDT 的 9 槽；Ballance
省略了邻近源码中的虚 `is_recursive`，所以 `exact_mindist_went_invalid` 与 `do_impact`
保持槽 6/7。兼容查询改为非虚，并通过版本锁定的模块解析识别原版 Recursive 主表。

Recursive Mindist 本体也已按原版拆开验证。两个工厂点都明确分配 `0x98`，不是导入类型/
邻近源码的 `0xA0`；次基类 Collision Delegator 位于 `+0x88`，状态位于 `+0x8C`，collision
FVector 位于 `+0x90`，后期 `spawned_mindist_count +0x98` 根本不在对象内。当前借用型接口
公开这个精确布局和七个保留实体，但删除普通 C++ 构造，因为零售 complete constructor
已经负责构造两个基类。主/次表固定为 8/2 槽，collision-deletion 包装显式传递 `+0x88`
调整后的 ECX，避免跨 DLL 调用时把完整对象指针错交给次基类实体。独立 i686-MSVC
编译器审计还会固定完整大小、两个基类偏移和两个尾字段偏移，防止次 vptr 契约悄悄漂移。

## 校验原版文件

```powershell
tools\ivp\Test-RetailPhysicsImage.ps1 `
  -BallanceRoot $env:BML_BALLANCE_ROOT
```

该脚本验证 SHA-256、PE 架构/时间戳/映像大小，以及来自五个独立引擎区域的指令锚点。
运行时也会在解析任何 RVA 前校验 PE 身份和这些指令锚点；不匹配时返回
`BML_ERROR_VERSION_MISMATCH`。

`ResolveSymbol` 的目录来自 IDA 名称/RVA 导出，它方便研究未封装入口，但名称和导入
类型本身不等于已验证的 C++ ABI。优先使用 `BML/IVP/` 下已有的类型和方法；自行解析
函数时必须再次核对调用约定、参数宽度、对象布局和生命周期。

所有已经确认的版本偏差集中记录在
[Ballance 原版 IVP 与邻近源码差异账本](ivp-retail-differences.md)，后续逆向发现也应
继续追加到该文档。
