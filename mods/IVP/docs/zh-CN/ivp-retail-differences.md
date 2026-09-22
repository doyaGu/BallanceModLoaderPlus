# Ballance 原版 IVP 与邻近源码差异账本

本文记录 Ballance 原版 `physics_RT.dll` 与以下邻近 IVP 源码之间已经确认的 ABI
差异：

- 原版二进制：Ballance 安装目录下的 `BuildingBlocks/physics_RT.dll`
- 对照源码：通过 `IVP_REFERENCE_ROOT` 指定的 IVP 源码绝对路径
- 原版 SHA-256：`E72E4AFCFA5C33A7D3D27776137F8C997B3C52D89D8A8A4745F1CA21E45893EC`

这不是邻近源码的勘误表。两者可能来自不同修订；本文只说明哪些定义不能原样用于
Ballance。每一项必须先由原版 DLL 的构造函数、字段访问、调用约定或原始 vtable
复核，才能标记为“已确认”。IDA 中导入的源码类型是重要线索，但不是单独的最终证据。

## IDA 数据库修正状态

当前实际修正的是分析副本 `build-dev\physics_RT-analysis.i64`。此前为避免和 IDA GUI
争用只在工作副本验证；经用户明确授权后，现已把同一套可重放修正原位写回这个分析
副本。写回前保留了 `physics_RT-analysis.pre-contact-topology.i64` 和
`physics_RT-analysis.pre-immediate-freeze.i64`、
`physics_RT-analysis.pre-universe-interface.i64`、
`physics_RT-analysis.pre-actuator-scalar-types.i64` 和
`physics_RT-analysis.pre-stiff-check-dist-comments.i64`、
`physics_RT-analysis.pre-four-point-stabilizer-comments.i64`、
`physics_RT-analysis.pre-suspension-comments.i64`、
`physics_RT-analysis.pre-template-comments.i64` 等阶段备份；游戏目录中的 DLL
从未修改。
可重复修正脚本是 `tools\ivp\Apply-BallanceIvpIdbCorrections.py`；它先检查零售映像基址、
listener、controller、material 与 spring vtable 指针和关键指令字节，任一不符就拒绝
写入，避免把 Ballance 结论误套到邻近 IVP 构建。

本轮已在数据库中修正 RVA `0x42B0` 的 post-collision 误名与原型，并为 RVA
`0xA770/0xA850/0xA8A0/0xA900` 的对象 listener 派发、RVA
`0x13B80/0x13BC0/0x13C00` 的环境 listener 派发写入名称、类型和证据注释。脚本还在
VA `0x10063214` 的 vtable 上记录 Ballance 的五槽顺序。另行修正了 RVA
`0x15FC0/0x15FD0/0x160C0` 的 mindist-settings 静态初始化链、RVA `0x15FE0` setter
的错误返回值/`this` 类型，并把 VA `0x10075DB0` 标成大小 `0x138` 的
`ivp_mindist_settings`。

材质部分另行确认了 VA `0x1006344C/0x10063464` 的两张六槽 vtable，并修正 RVA
`0xBE00/0xBE10/0xBF00/0xBF50/0xBF70/0xBF80` 的析构、构造与 `get_name` 名称/类型。
adapter/controller 部分修正了 RVA `0x46A0/0x46B0/0x4760/0x4780/0x47A0/0x4C20/`
`0x4C30/0x4C50`：前五项属于 `PhysicsControllerForce`，不是导入类型声称的
`IVP_Actuator_Force`；VA `0x10063240` 是 adapter 表，`0x1006325C` 才是七槽
`IVP_Controller` 基表。两点 actuator 模板构造 RVA `0x13E70` 也已恢复为
`IVP_Template_Two_Point`：它清零 `+0x00/+0x04/+0x08`，确认完整 `0x0C` 布局。
Spring 部分修正了 RVA
`0x14200/0x14220/0x14350/0x14370/0x143F0/0x14440/0x145F0/0x14610`，其中
`0x145F0/0x14610` 实为 `IVP_Actuator_Spring_Active` 析构链，并非 IDB 旧名中的
`Torque_Active`。最后把 RVA `0x13270` 修正为非虚的 `IVP_Environment` 析构函数，并
修正 RVA `0x13F60/0x13FA0/0x13FB0/0x13FC0/0x13FE0/0x14020/0x14130/0x14150`
的 actuator 基类、Two Point 构造/查询/优先级/析构名称与类型。Great Matrix 部分还为
RVA `0x33D80`–`0x34270` 的 11 个已保留入口补上了此前缺失的 thiscall 原型，并为 RVA
`0x35780`–`0x36030` 的 15 个增量 LU 入口完成同样修正。Spring 的 RVA
`0x14390/0x143B0/0x143D0/0x146D0` 也已补上准确的 `IVP_DOUBLE` 参数和 simulation
callback 原型。
Local constraint 部分又修正了 RVA `0x9C40/0x129C0/0x28230/0x28240/0x28250/`
`0x28270/0x288A0/0x28960/0x2A210`–`0x2A920/0x37600` 的名称与类型，并给
VA `0x10063930` 的 23 槽表逐槽写入语义。
随后又确认 VA `0x10063B20` 才是 23 槽 `IVP_Constraint` 抽象基表：RVA
`0x375B0/0x375E0/0x37650` 是基类构造、scalar-deleting destructor 和完整析构，RVA
`0x376D0`–`0x37780` 是 12 个被折叠复用的缺省修改实现。只有 simulation 槽是纯虚，
后 16 个修改槽都是会进入原版诊断路径的具体函数，不能声明成纯虚。core-reaction 的
RVA `0x33A30/0x33AD0` 也已按 `IVP_Solver_Core_Reaction` owner 命名。写回这些结论前保留
`physics_RT-analysis.before-fixed-keyframed-and-constraint-base.i64`。
Surface manager 部分修正 RVA `0x2DA0/0xBD00/0x22160/0xBCF0` 的 Polygon/base
析构链与 `get_type`，并记录 VA `0x100631E0/0x10063890` 两张 11 槽表。最后给两个
编译器共享 helper 写入不冒充 C++ member 的中性分析名。

本轮 ABI 校正又核验并命名 VA `0x10063390/0x100633A0` 的
`IVP_Real_Object`/`IVP_Object` 表、VA `0x10063524` 的六槽 buoyancy-attacher 表、
VA `0x10063434` 的六槽 material-manager 表，以及 VA
`0x10063A50/0x10063A58` 的 Anomaly 两张表。RVA
`0xBDE0/0x2F610/0x2F630/0x2F680/0x2F6A0` 的错误 STL/Triangle owner 已改成
对应 manager/anomaly 析构链。写回前新增备份
`physics_RT-analysis.before-abi-vtable-corrections.i64`。随后把本轮零售布局注释写回前，
又保留了 `physics_RT-analysis.before-retail-layout-comments.i64`；写入 Golem 运行时布局注释
前另保留 `physics_RT-analysis.before-golem-comment.i64`。本轮对象生命周期修正写回前另保留
`physics_RT-analysis.before-object-lifetime.i64`。
Phantom 生命周期和尾字段修正写回前另保留
`physics_RT-analysis.before-phantom-lifetime.i64`。
写入 Forcefield 的证据分级布局注释前另保留
`physics_RT-analysis.before-forcefield-interface.i64`。
写入车辆基础类型与版本差异注释前另保留
`physics_RT-analysis.before-car-foundation.i64`。写入完整 raycast controller 与其导入 vtable
的证据分级注释前，又保留 `physics_RT-analysis.before-raycast-car-interface.i64`。
写入 real-wheel constraint-car、Object Attach 与 historical Complex 类型前，先在独立副本
连续执行两次修正并回读全部成员；更新主库前另保留
`physics_RT-analysis.before-constraint-car-closure.i64`。游戏目录中的 DLL 仍未修改。
补齐 Core 内部接口前又先在 `physics_RT-analysis.core-closure-validation.i64` 上重复写入并
回读，主库写回前保留 `physics_RT-analysis.before-core-interface-closure.i64`。
恢复 Core 的材质/质量重编译路径前，在
`physics_RT-analysis.contact-recompile-validation.i64` 上完成类型回读和幂等复写；主库写回前
另保留 `physics_RT-analysis.before-core-contact-recompile.i64`。
恢复对象的受控删除路径前，又在 `object-delete-validation.i64` 上完成两次修正和精确类型
回读；主库写回前保留 `physics_RT-analysis.before-object-delete.i64`。RVA `0x99F0` 的
错误 `int` 返回类型已修正为 `void`，RVA `0xDAB0` 原先缺失的 Core thiscall 原型也已补齐。
恢复 beam 路径时又先在 `physics_RT-analysis.beam-validation.i64` 上完成 200 项修正的
幂等复写和十个相关函数的类型回读；主库写回前保留
`physics_RT-analysis.before-beam.i64`。新增校正覆盖 RVA
`0x99A0/0x138F0/0x187A0/0x1A820/0x1E300/0x1E730/0x1E750/0x1E870/0x1EA50/0x1EB10`；
其中五个原先匿名函数获得可核对的名称，其余已有名称的错误或缺失原型得到修正。

Active value 部分又修正 RVA `0x15060/0x150D0` 被错误导入为 `std::ios_base` 的 hash
析构链、RVA `0x15280/0x152A0` 的 manager 析构链，以及此前匿名的 RVA `0x156D0`
PSI 刷新函数；RVA `0x150F0/0x15140/0x15160/0x15260/0x15360/0x15410`–`0x158E0`
均写入从装饰名、`ret`、字段访问和调用点共同确认的 thiscall 原型。脚本还逐槽校验并
命名 VA `0x100636D0` 的两槽名称 hash 表与 VA `0x100636D8` 的 15 槽 manager 表。

对象局部 listener 部分确认：`IVP_Real_Object::add/remove_listener_object` 与
`IVP_Environment::add/remove_listener_object_private` 在 Ballance 中均被内联，但对象
callback hash 的查找和删除仍保留在 RVA `0xA630/0xAC70`。对象 hash 位于
`IVP_Cluster_Manager + 0x08`，callback table 为 `0x0C` 字节（对象指针加一个
`IVP_U_Vector`），对象指针按四个 x86 字节做 CRC hash；首个 listener 注册时设置
`IVP_Real_Object::flags` 的 `0x1000` 位，最后一个注销时清除。VA
`0x100633C8/0x100633C0` 分别是 object/collision callback hash 的两槽 vtable，两者共享
RVA `0x1EE50` 的对象指针比较函数，但各自保留 deleting destructor。当前兼容层只重建
被内联的短注册逻辑，查找、删除、底层 `IVP_VHash` 和分配器继续调用原 DLL。

接触拓扑和立即冻结路径又补充修正了 RVA `0x9A40/0xAFE0/0xB0D0/0xB100/0xB4D0/`
`0xD680/0x120B0/0x120F0`，并命名、校验 VA `0x100633D0` 的 friction-system 主表；
Universe manager 路径进一步修正 RVA `0xA9F0/0x17140`。独立回读现确认脚本中的
207 个函数、1 个全局对象和 25 张相关 vtable。此前新增的八个函数是 Cluster 根构造与
完整析构、Hull Manager Base 完整构造与析构、此前匿名的 Min List 构造与析构，以及
Phantom 的 protected 完整构造和此前匿名的完整析构；
VA `0x100633A4` 也已命名为一槽 `IVP_Cluster` 表。脚本还把
`IVP_Actuator_Rot_Mot::rot_inertia +0x90` 与
`IVP_Actuator_Torque::rot_inertia +0x88` 两个导入成员从语义错误的 `long double` 修正为
`double`，结构大小分别保持 `0xA0/0x98`。由修正库重新导出的只读函数符号目录现有
1530 项。对函数体已被链接裁掉、但源码兼容布局已经核对的 Stiff Spring、Check Dist、
Four Point、Stabilizer、Suspension、Golem template/runtime、Fixed-Keyframed template/runtime
和 Forcefield、车辆基础类型、完整 raycast controller 及其导入 vtable，脚本只写入分级
类型注释，不伪造函数名或 RVA；`Inspect-IvpTypes.py` 会回读这些注释和全部成员偏移。

本轮又给 `IVP_Object_Attach`、三个 constraint-car UDT 及其七槽虚表类型加入分级注释，
并把虚表第 0 槽从错误的 priority 名称/返回类型修成
`core_is_going_to_be_deleted_event(IVP_Core*)`，第 1 槽修成 64 位 `double` 返回，第 5 槽
恢复真正的 priority 名称。实际具体 vtable 已被裁掉，所以这些修正仍明确写成
IDB/source 证据。`IVP_Complex_Simple` 原先完全不在 IDB，只按邻近公开声明补入 0x34
source-only UDT；没有为它伪造函数或 RVA。由构造、字段访问或原始 vtable 复核过的类型、
以及仅靠导入/源码恢复的类型继续使用不同证据等级。

Core 闭包又把原先匿名的 RVA `0xCE20/0xCEC0/0xCF20/0xCFA0/0xD1A0/0xD930`
分别恢复为 `stop_physical_movement`、`reset_freeze_check_values`、
`init_core_for_simulation`、`synchronize_with_rot_z`、`calc_calc` 和
`update_exact_mindist_events_of_core`。这些归属不是依据函数排列猜测：对象 movement-state、
hull/cache 调用、calm-reference 偏移、sim-unit transaction allocation、inertia 字段写入及
逐对象 exact-mindist 调用均与各自邻近实现逐项相符。RVA `0xB540/0xD2F0/0xD580`
原先已有名称但类型错误或缺失，也已修正为 environment 延迟 revive、protected Core init
和完整 Core 析构。随后又把匿名 RVA `0x1F860` 由调用序列和完整函数体确认为
`IVP_Contact_Point::recalc_friction_s_vals`，并修正 contact destructor、friction fusion、
virtual-mass、transaction-memory epilogue 与 material refresh 的原型。修正后的只读目录由
1530 项增加为 1537 项，是七个匿名函数获得名称。beam 相关修正随后又命名五个匿名
零售 helper，使当前目录达到 1542 项；这些都不是 DLL 新增代码。

Simulation Unit 本轮再对 RVA `0x116F0/0x11890/0x11920/0x119D0/0x11E70/0x12010/`
`0x121B0` 写入机器码锚点和精确 thiscall 原型。其中 controller/core-list 析构、unit 完整
析构、`add_sim_unit_core`、标准重力 controller 的仿真槽和单 unit PSI 五个原先匿名函数
获得装饰名，目录由 1542 项增加到 1547 项；unit 构造和 manager 构造原有名称但缺少
可靠原型。主库写回前先在 `physics_RT-analysis.simunit-validation.i64` 上完成幂等复写和
类型回读，并保留 `physics_RT-analysis.before-simunit.i64`。

本轮再给 split/fuse/sort/calc_redundants、`Core::union_find_get_father` 以及此前误标成
`int(void *, int)` 的 `sim_unit_remove_core` 写入精确 thiscall 原型。这些函数原先已有
装饰名，目录仍为 1547 项；独立回读现确认脚本中的 228 个函数（此前 207）、1 个全局对象
和 25 张相关 vtable。零售 `split_sim_unit`（RVA `0x11470`）在第三分量仍残留时跳回
`0x1148E` 再分配一个 `0x24` MOVING unit，而不是邻近源码的递归；
`fusion_simulation_unities` 由 `IVP_Mindist::try_to_generate_managed_friction` 两次调用；
father walk 的保留实体位于 RVA `0x1C7D0`，沿 `Core+0x228` 的 `tmp` 链迭代到根；
公开包装现直接调用该实体。主库写回前先在
`physics_RT-analysis.split-fuse-validation.i64` 上完成幂等复写，并保留
`physics_RT-analysis.before-split-fuse.i64`。

摩擦节点接口本轮又恢复了三个原先匿名的零售函数：`IVP_Contact_Point` 构造、
`IVP_Friction_Manager::generate_contact_point` 和
`IVP_Friction_Core_Pair::remove_energy_gained_by_real_friction`。工厂 RVA `0x20030`
在调用 allocator 前明确 `push 0x78`，随后调用 RVA `0x1A8B0`；构造函数把两条 synapse
放在 `+0x08/+0x1C`，`IVP_Time` 放在 `+0x68`，friction-system backlink 放在 `+0x70`。
因此 Ballance 的 `IVP_Contact_Point` 是 `0x78`，不是导入类型和另一配置显示的 `0x88`；
那个版本尾部的 `get_contact_point_ws` 数据在 Ballance 对象中没有存储空间，当前接口明确
不提供该查询。RVA `0xB2C0` 的 `push 0x38` 与 RVA `0x1D340` 构造函数另确认
`IVP_Friction_Core_Pair` 为 `0x38`，其 vector、span、next-ease、time、energy 和 core
指针依次位于 `+0x00/+0x08/+0x18/+0x20/+0x28/+0x2C`；每 core 的 friction info 是
`0x0C`（vector `+0x00`，system `+0x08`）。随后又按零售装饰名和函数边界补齐 15 个
friction-system PSI/complex/energy 操作，并把 system 和 pair 的 retained constructor 接入
公开包装。pair 拓扑又确认 RVA `0x1C6F0/0x1C720` 只操作 system `+0x34` vector，完全没有
邻近源码中的 friction-pair listener 派发；RVA `0x1C680/0x1C690` 则是 pair `+0x00`
contact vector 的数量查询和删除。随后从 system RVA `0x1BC20` 的内联 pair 循环确认：
滑移上限逐项读取 contact `+0x54/+0x44/+0x30`，pretension 使用 `+0x38/+0x3C`，
二维/一维约束按 `+0x34 == 1` 分派至 RVA `0x1B080/0x1B8B0`，正的二维能量写回
pair `+0x28`。`get_sum_slide_way` 与 `pair_calc_friction_forces` 因而按该机器码重建；
`get_average_friction_vector` 与 `set_friction_vectors` 没有独立零售函数体，采用邻近源码公式，
但只依赖已经由零售访问确认的 contact `+0x40`、long-term span `+0x80/+0x90`、
span scalar `+0x38/+0x3C` 和 Real Object core 路径。脚本现在有 255 项 guarded corrections，
命名函数目录为 1555 项；主库写回前已保留
`physics_RT-analysis.before-friction-nodes.i64`，并在
`physics_RT-analysis.friction-nodes-validation.i64` 上完成幂等复写和独立类型回读。
后续 17 项 friction operation/constructor 原型先在
`physics_RT-analysis.friction-operations-validation.i64` 上重复应用，再写回主库；写回前副本为
`physics_RT-analysis.before-friction-operations.i64`，17 项原型均已从主库独立回读。
四项 pair 拓扑修正另在 `physics_RT-analysis.friction-pair-topology-validation.i64` 上完成
两次应用；写回前副本为 `physics_RT-analysis.before-friction-pair-topology.i64`，主库中的
名称和类型已独立回读。pair 计算接口另在
`physics_RT-analysis.friction-pair-inline-compute-validation.i64` 上通过二进制防护，写回前副本为
`physics_RT-analysis.before-friction-pair-inline-compute.i64`。

pair 的 `+0x08` 区域现已从 `std::byte[0x10]` 恢复为原公开类型名
`IVP_U_Float_Point span_vector_sum`。这不是只凭导入 UDT：邻近声明给出成员语义，IDB 的
`0x38` UDT 给出 `+0x08/0x10` 布局，而零售构造 RVA `0x1D340` 的完整指令序列只写
`+0x00..+0x04`、`+0x18`、`+0x20/+0x24` 和 `+0x28`，明确不写该调试向量。为保持这一
行为，`IVP_U_Float_Point()` 同步改回邻近公开头的 trivial、未初始化默认构造；带参数构造
及显式 `set_to_zero()` 的行为不变。这样既恢复强类型接口，也不会在进入零售构造体前偷偷
清零 DLL 本来保留为未定义值的区域。

`IVP_Impact_Solver_Long_Term` 本轮从 forward declaration 提升为真实 `0xE0` 类型。基类
`IVP_Contact_Situation` 为 `0x58`；零售 RVA `0x24140` 直接访问 `contact_core`
`+0x78/+0x7C`、`contact_point_cs +0xA0/+0xB0`、impact conservation `+0x68`，并以
`retn 0x0C` 返回。原 IDB 将该函数错误标为返回 `int`、参数均为整数，现已改为
`void __thiscall(IVP_Impact_Solver_Long_Term *, IVP_Core **, float,
IVP_Contact_Point *)`。`get_closing_speed` 是裁剪掉的 inline，按邻近公式重建，但只读取
零售约束函数已经确认的 core、normal 和 cross-normal 字段。主库写回前保留
`physics_RT-analysis.before-impact-long-term.i64`，并在
`physics_RT-analysis.impact-long-term-validation.i64` 上先通过保护脚本；该阶段脚本为 256 项
guarded function corrections，Retail Contract 为 503 个 code RVA。

`IVP_Mutual_Energizer` 随后恢复为真实 `0xA0` 值对象，而不是只保留 pair 内部的匿名调用。
`IVP_Friction_Core_Pair::destroy_mutual_energy` 在 RVA `0x1D2A0` 明确保留 `0xA0` 字节栈空间，
并依次调用初始化 `0x1CDB0`、成员能量计算 `0x1D060` 和按比例消能 `0x1D0E0`；初始化体
对 `+0x90/+0x94` 的写入确认两个公开 core 指针。五参数静态能量函数在 `0x1CD60` 也有
独立原版函数体，使用五个 caller-cleaned 64 位参数并从 x87 返回 `double`。因此四个公开方法
均直接包装原版，而非移植另一版本实现；host 案例用两刚体质心速度验证结果只包含可由互相
冲量消除的相对动能，并验证交换 core 与反转相对方向不改变能量。

这里邻近源码的 `IVP_DOUBLE` 布局本身与原版一致，偏差来自旧 IDB 将九个 8 字节字段显示为
`long double`，并把私有冲量 helper 与 `destroy_percent_energy` 错标为 `stdcall/int`。
机器码、装饰名、`retn 0x20/0x08` 和 x87 访问共同证明这些字段/参数都是 MSVC `double`；
IDB 已逐字段修正，并为原先匿名的 `0x1CD60/0x1CDB0` 补上类方法名。验证副本为
`physics_RT-analysis.mutual-energizer-validation-4.i64`，主库写回前副本为
`physics_RT-analysis.before-mutual-energizer.i64`。该阶段脚本为 261 项 guarded function
corrections，命名函数目录 1557 项，Retail Contract 为 507 个 code RVA；公开候选证据表仍为
311 条，因为该原版内部辅助类不属于邻近 `IVP_EXPORT_PUBLIC` 审计分母。

friction system 的两个内嵌辅助控制器也已从匿名存储恢复为真实类型。
`IVP_Friction_Sys_Static` 和 `IVP_Friction_Sys_Energy` 都是大小 `0x08` 的
`IVP_Controller_Independent` 派生类：vptr 位于 `+0x00`，指回 owner system 的指针位于
`+0x04`。零售 system 构造函数在 `system+0x08` 安装 static vtable `0x10063408`，在
`system+0x10` 安装 energy vtable `0x100633EC`，并把两个 backlink 分别写到
`+0x0C/+0x14`。因此兼容层公开真实类以及 `get_static_friction_controller()` /
`get_energy_friction_controller()` 借用访问器，但没有把它们声明成会自动构造析构的普通
C++ 数据成员：零售 system 完整析构已经负责这些子对象，再由包装层自动析构会重复执行
controller 析构。

四个有独立函数体的执行入口分别是 energy `do_simulation_controller` RVA `0x1D610`、
static 单接触分支 `0x1D660`、static 总调度 `0x1D6C0` 和 static core-deleted 处理
`0x1D9A0`。优先级函数 `0xB080/0xB0B0` 分别返回 2000 和 0；两类的 scalar deleting
destructor 被链接器折叠到同一个 RVA `0xB090`，所以 IDB 使用中性名称，不能伪装成其中
某个类独占的方法。邻近源码里 energy 的 `get_mimumum_simulation_frequency` 拼写错误是
一个额外的非虚成员；零售 vtable 槽 1 实际仍继承拼写正确的基类虚函数。旧 IDB 把 static
槽 0 错标为最小频率、重复命名槽 1，并把两个返回类型显示成 `long double`，现已按
vtable 调用点和函数体改为 core-deleted 槽及 `double`。

修正先在 `physics_RT-analysis.friction-helper-controllers-validation-3.i64` 上完成两次幂等
应用；主库写回前副本为 `physics_RT-analysis.before-friction-helper-controllers.i64`。
当前脚本为 268 项 guarded function corrections，命名函数目录 1559 项，Retail Contract
为 511 个 code RVA；公开候选证据仍为 311 条，因为这些是 Ballance 可达的 intern ABI，
没有混入邻近公开候选的分母；修正公开标记和匿名字段识别后当前分母为 1575。

`IVP_Friction_Solver` 随后恢复为实际的 `0x840` 临时对象。零售
`IVP_Friction_System::do_friction_system` 在栈上明确预留 `0x840` 字节并把同一地址依次传给
构造、contact 坐标准备、PSI 矩阵计算和线性求解；构造函数确认 sparse matrix 位于 `+0x00`、
`correct_x_factor` 位于 `+0x20`、environment/event 位于 `+0x28/+0x2C`，固定 512 项的
contact-info vector 位于 `+0x30`，诊断计数位于 `+0x838`。兼容层使用 union 抑制宿主 C++
对 matrix/vector 的预构造，避免在进入零售完整构造函数前重复初始化；析构只执行零售
EH cleanup `0x36E90` 已证明的 vector 清理逻辑。该地址属于 `do_friction_system` 的共享函数块，
不是独立函数边界，因而没有被伪造成 Retail Contract 入口或在 IDB 中强行拆函数。

11 个有独立零售函数体的方法现直接包装原版：构造、`calc_solver_PSI`、
`do_resulting_pushes`、pair/two-mindist easing、`factor_result_vec`、
`get_closing_speed_core_i`、`normize_constraint_equ`、`setup_coords_mindists`、
`solve_linear_equation_and_push` 和 `test_gauss_solution_suggestion`。短小的
`get_inv_virtual_mass`、同步/异步 contact impulse、`test_push_core_i` 与
`calc_desired_gap_speed` 才按邻近源码重建，并用双刚体接触案例验证等量反向冲量保持总线性
动量，同时按两个 contact arm 和逆转动惯量更新角速度。随后又从 `calc_solver_PSI` 的两段
展开体恢复 `calc_distance_matrix_column`，并将 Ballance release 中编译为空操作的
`ease_test_two_mindists`、`debug_distance_after_push` 明确恢复为 no-op。其余 penalty、
complex、inactive 和 debug-print 路径只保留精确声明：当前零售镜像没有独立函数体，不能把
邻近实现直接搬进来。

IDA 9.4 legacy parser 曾将 `IVP_Great_Matrix_Many_Zero` 和 solver 中用于保持 8 字节成员
对齐的 `IVP_DOUBLE` 显示为 `long double`，但成员宽度实际是 8 字节。把单个成员机械替换
为 parser 的 `double` 会错误地把 UDT 从 `0x20/0x840` 缩成 `0x1C/0x83C`。当前已将
`IVP_Great_Matrix_Many_Zero` 整体重建为 binary64 `double/double *` 字段，并显式加入
`+0x1C` tail padding。其余导入类型不再通过字符串重解析或替换外围 UDT，而是递归变换
成员 `tinfo_t` 中的标量节点；数组维度、指针、函数调用约定和模板引用保持不变。由于
IDA 的 `set_udm_type` 会把部分显式 8 字节结构对齐从 `sda=4` 静默降为 `sda=3`，转换器
必须先快照 owner 的大小与 `sda`，修改后恢复并断言总大小完全不变。验证副本为
`physics_RT-analysis.friction-solver-validation.i64`，完成两次幂等应用。当前脚本为 279 项
guarded function corrections，命名函数目录仍为 1559 项（11 个入口原先已有名字），
该阶段 Retail Contract 为 522 个 code RVA；contact complete constructor 接入后曾为 523 个。

column-builder 的二进制防护和注释先在
`physics_RT-analysis.friction-column-validation.i64` 上完成两次幂等应用，再写回主库；写回前
副本为 `physics_RT-analysis.before-friction-column.i64`。这轮只补充内联区间注释，不制造
函数边界，因此 guarded correction、命名函数和 Retail Contract 数量均保持不变。

数据库文件是分析产物；上述脚本、`Inspect-IvpFunction.py`、`Inspect-IvpVtable.py` 与
`Inspect-IvpTypes.py` 是可审查、可重放的修正来源。新增结论应继续写入脚本并附带二进制
指纹，而不是只在某次 GUI 会话里手工改名。

新增 `Audit-IvpIdbLongDouble.py` 会遍历全部 `IVP_/IVV_` UDT。首次审计得到 177 个残留
成员；本轮按完整 UDT 重建 `IVP_Time`、`IVP_Event_Sim`、`IVP_U_Point`、`IVP_U_Quat`、
`IVP_Great_Matrix_Many_Zero` 和 `IVP_Incr_L_U_Matrix` 后降为 157。它们的 20 个字段现均为
`double/double *`，大小仍分别为 `0x08/0x18/0x20/0x20/0x20/0x30`。后续又按零售字段访问
完整重建 `IVP_Time_Manager`、`IVP_Range_Manager`、`IVP_Radar_Hit`、`IVP_Radar`、
`IVP_U_Min_Hash_Elem`、`IVP_Material_Simple`、polygon solver、两个 mindist workspace 和
Better Statistics double-array，共修正 32 个误标成员，残留降为 125。随后递归处理剩余
125 个标量、数组、指针及虚表函数节点，审计现为 0，IDB 事务硬校验 0。早期尝试的
“字符串重解析成员后再回滚”会丢失 UDT alignment 元数据，仍禁止使用；仅递归替换
`tinfo_t` 也不够，必须恢复显式 `sda`。新增布局审计确认 57 个直接含 binary64 的 UDT
有效对齐均为 8，且精确核对 Environment `0x178`、Debug Manager `0x68`、Friction Solver
`0x840`、Impact Solver `0x128`、short-range callback `0x68`、Extra Info `0x40` 和
Geompack `0x90`。规范 IDB 中 `IVP_Environment` 也已按零售字段访问整体重建：统计管理器
位于 `+0x38`、时间值从 `+0x120` 开始。后续复核构造/析构发现，先前把
`+0x10C` 误作 `constraint_listeners`、并删掉 `environment_manager` 的结论正好相反：
原版在 `+0x10C/+0x110/+0x114` 保存授权字符串、代码和计数，在 `+0x118` 保存 manager backlink。

## 差异总览

| 项目 | 邻近源码 | Ballance 原版 | 当前兼容层处理 |
| --- | --- | --- | --- |
| `IVP_Environment` | `core_revive_list` 后还有 `constraint_listeners`，再接授权字段和 `environment_manager` | 只构造前三个 listener/revive vector；`+0x10C` 是授权字符串，`+0x118` 是 `environment_manager`，总大小 `0x178` | 删除不受结构支持的 constraint-listener API，恢复 manager backlink，并按原版偏移重建 |
| `IVP_Template_Real_Object` | `physical_unmoveable` 后依次为 `enable_piling_optimization`、`pinned` | 保留前者 `+0x10`、省略后者，总大小 `0x70` | 公开 `enable_piling_optimization`，不虚构模板 `pinned`；保持 `material +0x14` 等原版偏移 |
| `IVP_Object_Attach` 三个顶层方法 | 公开头有声明，`ivp_object_attach.cxx` 有 attach/detach/reposition 实现 | 导入空类型存在，但三个顶层函数均被链接裁掉；其 Core/Object/碰撞依赖多数保留 | 明确标成 source-reconstructed，不伪造 RVA；只移植短控制流并调用零售入口，三条复合对象生命周期案例分别验证 |
| `IVP_Object` / `IVP_Real_Object` 继承与虚表 | 四级真实继承链；Real Object 新增 quat、matrix 两槽 | 原版表为析构、quat、matrix；中间前缀大小 `0x40/0x88` | 恢复真实类继承，不用扁平字段或假 `*_base`；按原版槽序声明 |
| `IVP_Hull_Manager_Base` 成员生命周期 | C++ 完整构造/析构负责内嵌 Min List | RVA `0x1A740/0x1A7A0` 本身已构造/析构 `+0x20` 的 Min List；RVA `0x300F0/0x30170` 是原先匿名的实际 Min List 入口 | 用 union 只提供成员存储，完整零售函数只运行一次；避免包装层再自动构造/析构一次 |
| `IVP_Controller_Phantom` 尺寸与生命周期 | `0x48`，尾部 `client_data +0x40` | `convert_to_phantom` 只分配 `0x40`；RVA `0x10D00/0x108A0` 完整构造/析构内嵌容器，尾部字段不存在 | 删除 `client_data`，以 union 保留内嵌容器存储，只调用一次零售完整构造/析构 |
| `IVP_Template_Spring` / `IVP_Actuator_Spring` | 含 `spring_force_only_on_stretch` | 模板和运行时对象均不含该成员，simulation 也无对应分支 | 使用模板 `0x38`、actuator `0x98` 的原版布局；兼容查询恒为 `IVP_FALSE` |
| `IVP_Controller_Floating` | 公开模板 `0x50`、抽象 controller `0x40`，按 ray distance 在一点施加限幅冲量 | 导入 UDT 保留完整相同字段，但类专属函数体和 vtable 均被裁掉 | 按 Ballance 布局选择性重建整类，复用原版 Cache/Reaction/Controller Manager ABI；不伪造 RVA |
| `IVP_Controller_World_Friction` | 公开模板用四个 double Point，运行时用四个 Float Point，并按轴限制线性/角速度修正 | Ballance 导入布局为 `0x80/0x4C`，保留尾部 `clip_manhattan +0x48`，但类专属实体全部被裁掉 | 保留完整 Ballance 布局；按邻近算法重建 moving-platform 速度跟随并调用原版 Core/Controller ABI |
| `IVP_Range_Manager` | 删除标记、环境指针和十个 `IVP_DOUBLE` | 构造器 RVA `0x2D8E0` 证明 `+0x04` 是八字节类对齐产生的 padding，删除标记在 `+0x08`、环境在 `+0x0C`，总大小 `0x60` | 依靠 MSVC 对齐保持真实偏移，不虚构字段；四个公开方法全部直达原版 DLL，双物体 look-ahead 案例复测算法 |
| `IVP_Listener_Collision` vtable | 有 pre/post、object-deleted、两个 friction 与两个 friction-pair 回调 | 只有 post、object-deleted、friction-created、friction-deleted 和析构，共 5 槽 | 删除原版没有的槽，保留其余回调的相对顺序 |
| `IVP_Collision_Delegator` vtable | 在析构后追加两个 spawned-mindist 计数虚函数，Root 的 object-removal 因而后移 | RVA `0x13650` 对 Root 直接调用槽 2；该槽只能是 `object_is_removed_from_collision_detection` | 两个后期计数方法保留为非虚兼容默认值，维持 Ballance 两槽基表和 Root 槽序 |
| `IVP_Collision_Delegator_Root_Mindist` | 邻近内部头声明默认 root factory，后期基类还带额外 bookkeeping 虚槽 | 完整构造 RVA `0x2F5A0` 只安装 `0x10063A3C` 五槽表，对象大小 `0x04`；环境删除 RVA `0x2F3E0` 通过 DLL deleting destructor 自删 | 公开原版具体工厂与四个行为实体，不导入后期槽/字段；使用零售 `new/delete`，真实 DLL 生命周期验证跨 CRT 释放 |
| `IVP_Mindist` vtable | `mindist_rescue_push` 后有虚 `is_recursive`，再接 invalidation 与 impact，共 9 槽 | 构造器 RVA `0x16290` 安装 `0x100637B4`；该表只有 8 槽，槽 5/6/7 为 rescue、invalidation、impact，`0x240A0` 是无参数 impact 实体 | `is_recursive` 改为非虚兼容查询，通过版本锁定的 Recursive 主表 `0x10063AD0` 识别原版实例；两个后续虚函数直达 DLL 并保持槽 6/7 |
| `IVP_Mindist_Recursive` 布局/次基类 ABI | `0xA0`，尾部有 `spawned_mindist_count +0x98`；继承 Mindist 与 Collision Delegator | 工厂 RVA `0x2F430` 分配 `0x98`；构造器写主表 `0x63AD0` 和 `+0x88` 次表 `0x63AC8`，只初始化 status `+0x8C` 与 FVector `+0x90`；callback RVA `0x309C0` 的 ECX 是已调整的 `+0x88` 指针 | 删除不存在的尾字段，公开 `0x98` 借用型精确布局与七个 DLL 实体；普通构造删除，次基类 callback 包装显式调整 this，主/次表固定为 8/2 槽 |
| friction pair 计算 helper | 部分方法是独立源码定义，部分声明为 inline | `get_sum_slide_way`、pretension 和 pair force 被整体内联进 `calc_friction_forces`；二维约束仍保留但原先匿名 | 从 `0x1BC20` 的逐字段机器码重建 inline helper；二维/一维约束调用原版 `0x1B080/0x1B8B0`，平均向量仅在已确认布局上采用邻近公式 |
| `IVP_Impact_Solver_Long_Term` | `IVP_Contact_Situation` 后接状态、impact/friction union、core 与三组向量 | 大小和主要偏移一致，但 IDB 的 `do_impact_long_term` 函数原型丢失返回值与参数类型 | 恢复 `0xE0` 真实类型；保留函数调用 `0x24140`，inline closing-speed 只按零售确认字段重建 |
| `IVP_Material` / manager adhesion | 邻近公开围栏把 adhesion 标为 future/internal，因此公开审计只见四个材质查询 | 原版 Material 和 manager 表都实际保留 adhesion 槽 | 按原版六槽表公开 adhesion，不为迁就邻近公开围栏删槽 |
| `IVP_Attacher_To_Cores_Buoyancy` | 从公开模板实例继承，模板负责三个 active-set 回调 | 原版派生对象 `0x70`，表为三个回调、析构、参数查询、表面查询 | 保持真实模板继承；对私有不完整 controller 的三个回调转发原版 RVA，不塞入猜测类体 |
| `IVP_Controller` vtable | 在析构前含虚 `get_controller_name` | 无该虚槽，共 7 槽 | 保留同名非虚默认查询，避免析构及派生类槽位整体错位 |
| IDB 中的 `IVP_Actuator_Force::*` | 导入名把 adapter 方法归给 IVP actuator | vptr 写入和行为源码证明它们属于 0x40 字节的 `PhysicsControllerForce` | 在 IDB 中改回 adapter owner，不把它们作为 IVP Force API |
| IDB 中的 `IVP_Actuator_Torque_Active` 析构 | 导入名指向 Torque Active | 构造、双 vptr 与四个 active-float 解绑均证明是 Spring Active | 修正函数名/类型和 primary/secondary vtable |
| actuator 的 `rot_inertia` | IDB 渲染为 8 字节 `long double` | 邻近 typedef 与零售 `N` 装饰类型均为 64 位 `double` | 保持偏移/大小，只把 Rot Mot `+0x90` 与 Torque `+0x88` 的成员类型修正为 `double` |

两点模板构造器 RVA `0x13E70` 只写三个空指针：`client_data +0x00`、
`anchors[0] +0x04`、`anchors[1] +0x08`。Spring 构造器 RVA `0x14200` 先调用它，随后用
`rep stosd` 清零完整 `0x38` 字节，再把单精度位型 `0x60AD78EC` 写入
`break_max_len +0x24`，即 `1.0e20f`。测试先逐字段核对这些零售默认值，再配置两个不同
anchor、相对长度、弹性/阻尼和断裂阈值；这既验证原版入口，也避免把邻近版本多出的
`spring_force_only_on_stretch` 塞进 Ballance 对象尾部。
| `IVP_Controller_Stiff_Spring` | 独立于普通 Spring 的 controller | DLL 未保留函数体；导入布局与公开源码一致，且只以已验证 Two Point/controller 前缀进入原版 | 逐项重建短算法，不把 `IVP_Actuator_Spring` 当成别名，也不虚构 RVA |
| Check Dist 回调参数名 | 名为 `distance_shorter_than_range` | 同一源码实现实际传递新 `is_outside` 状态，且输出 terminal 也叫 `mod_is_outside` | 保留实际状态约定并在 API/IDB 注释中明确旧参数名已过时 |
| `IVP_Actuator_Four_Point` 构造 | 只安装 `physical_unmoveable` core，且不 announce controller | 与 Two Point、析构 remove 和 Stabilizer 的可执行前提自相矛盾；DLL 未保留函数体 | 保持 `0xD0` 布局，去重安装可移动 core，并向首个 core 的 environment announce |
| `IVP_Template_Suspension` / actuator | 派生模板在 Spring 构造后清空完整对象并把 break length 设为 `10e8f`；simulation 对 wheel/body 使用非对称冲量 | 导入布局为 `0x40/0xA0`，全部 Suspension 函数体均被裁掉；保留的 Spring 构造证实其 `0x38/0x98` 基类偏移 | 显式恢复派生默认值与短算法；wheel 侧不限幅，body 侧按 `max_body_force` 限幅，不臆造 RVA |
| `IVP_Controller_Golem` | 公开抽象 policy controller，运行时算法使用线性/角目标与每轴限幅 | 导入 UDT 为 `0x130` 且字段偏移与邻近声明一致，但所有 Golem 专属函数体均被裁掉 | 保持唯一新增纯虚槽 `resolve_for_problem` 和 `0x130` 布局；选择性重建控制算法，并把布局明确标为 IDB/source 而非零售代码确认 |
| `IVP_Constraint_Fixed_Keyframed` | `0xE0` runtime controller，以 reference-object 坐标系中的位置/姿态目标驱动两端 core；源码算法没有使用已保存的 `max_translation_force/max_torque` 做限幅 | 导入 UDT 与声明逐字段一致，但 class-specific 函数体和 vtable 全被裁掉；只保留可复用的 core-reaction 零售求解入口 | 保持 `0xE0` ABI，选择性恢复邻近算法且不臆造类 RVA；同样不额外发明源码中不存在的限幅，并明确标为 IDB/source 行为假设 |
| `IVP_Forcefield` | protected 继承 active-set listener 与 independent controller，以 `0x10` 对象跟踪动态 core 集合 | 导入 UDT 与邻近声明的基类顺序及 `+0x08/+0x0C` 尾字段一致，但类专属函数体和 vtable 均被裁掉 | 保持真实多继承和 `0x10` ABI，选择性重建登记/注销/拥有权生命周期；只把两个引擎基类视为零售确认，Forcefield 自身保持 IDB/source 证据等级 |
| Car debug block | `0x348`，尾部另有四个 actuator float vectors | Ballance 导入 UDT 为 `0x308`；主 controller 在 `+0x650` 嵌入该块并于 `0x958` 结束，没有尾部空间 | 删除四个后续版本字段，固定 `IVP_CarSystemDebugData_t` 为 `0x308` |
| `IVP_Car_System` vtable | 31 槽：含 `set_powerslide`、`get_booster_time_to_go`、`event_object_deleted`，且 `do_steering(float,bool)` | 导入 Ballance vtable 类型只有 28 槽，省略上述三槽，`do_steering(float)`；DLL 未保留实际表或类专属函数体 | 按 28 槽 Ballance 类型声明，并明确保持 IDB/source 证据等级；不为邻近版本追加槽 |
| `IVP_Controller_Raycast_Car` | 基于后期 Car System 的具体类；大量方法在 `.cxx`，四个更新钩子是头文件 no-op | 导入对象为 `0x958`，次基类在 `+0x04`，主 vtable 类型为 `0x74` 且只在 Ballance Car System 后追加 `do_raycasts`；具体表和函数体均被裁掉 | 保持 Ballance 双基类和 29 槽主表，后期三个方法仅作非虚兼容声明；选择性重建仿真/生命周期，不臆造类 RVA，并把算法标成 source + host 证据 |
| Raycast Car `update_wheel_positions` | Car System 声明为 pure virtual，但邻近 raycast 派生头/`.cxx` 没有 override，和“子类只需实现 `do_raycasts`”的注释矛盾 | 导入 Ballance raycast vtable 保留该继承槽，但无实际表地址或函数体可判定实现 | 提供不改变槽序的 no-op override，使 Mod 派生类只需实现 raycast batch；行为仍列为 IDB/source 假设 |
| real-wheel constraint car | 邻近早期 solver 用一个车身和最多 12 个真实轮刚体建立 x/z 有效质量约束 | Ballance IDB 的 object/solver/builder 为 `0xB0/0xA0/0x30` 且字段一致，但类专属函数体和具体 vtable 均被裁掉 | 保持导入布局和七槽 controller ABI，逐段恢复构造、矩阵建立、旋转与平移求解；证据标成 IDB/source + host，不冒充零售类体 |
| constraint-car matrix 行宽 | 邻近 builder 设置 `columns` 后直接通过 `set_value` 写矩阵，未设置 `aligned_row_len` | Ballance 保留的 great-matrix 方法都以 `aligned_row_len` 索引，且该构建明确要求它等于 `columns` | builder 在任何写入前显式调用 `calc_aligned_row_len`；这是适配 Ballance 已证实矩阵 ABI 所需的兼容修正 |
| `IVP_Constraint_Solver_Car_vtbl` 导入标签 | 第 0 槽应为 core 删除回调，第 5 槽才是 priority | 导入 UDT 把第 0 槽误标且误型为 priority，并给真正第 5 槽加 `_2`；无具体零售表可查 | 依据已确认的七槽 `IVP_Controller` 基表与邻近 override 集合修正名称和前两槽原型，同时保留 IDB/source 证据等级 |
| historical matrix/complex 声明 | 头文件含 allocating great-matrix ctor、complex/LP、index-LU 和 `IVP_Complex_Simple`；多数只剩声明，allocating ctor 的源码还会无条件断言失败 | Ballance 同样没有这些算法的对应函数体或调用点；Complex 类型也未导入 IDB；但增量 LU 的 L/U 列交换原语仍保留 | 对仍无可证明语义的历史入口保留精确签名并标为 `= delete`，Complex 只建 source-only 0x34 UDT；能完全落到已保留原语或完整邻近函数体的便利/诊断入口单独恢复为第 2 类 |
| Core collision merge | `create_collision_merged_core_with` 在函数开头无条件 `return`，其后的 merged-core 代码不可达 | 无独立函数体或调用点 | 保留同签名 no-op，不激活废弃代码；`set_matrizes_and_speed` 以 `= delete` 明确拒绝 |
| `apply_velocity_limit` 的待提交角速度规则 | 以 `speed_change` 长度判断并据此缩放 `rot_speed_change`，而非用后者自身长度 | 无保留函数体或调用点可证明应“修正”此规则 | 严格保留邻近实现并在代码/测试中注明，不凭直觉改写成另一版本行为 |
| Core 质量/材质重编译 | 邻近实现遍历所有 contact，必要时合并 friction system，并用 transaction memory 重算材质和 virtual mass | contact refresh 保留为匿名 RVA `0x1F860`；内存事务计数为 `IVP_U_Memory+0x10`，friction-system 的 `IVP_BOOL:8` 因 MSVC 分配单元对齐位于 `+0x44`，不能按紧凑字节字段猜成 `+0x42` | 调用零售 contact/material/virtual-mass/fusion/destructor/transaction 函数，只重建短遍历；两个 Real Object recompile 入口也恢复可调用，并用三条状态场景验证 |
| `IVP_Core::calc_virt_mass_worst_case` | 邻近源码在 pinned 状态返回 `1.0 + rotational term`，其余状态返回 `inv_mass + rotational term` | 匿名 RVA `0xC200` 的三个 contact-refresh 调用点、`Core+0x34..+0x40` 读取、`ret 4` 与 x87 `double` 返回共同确认函数归属；零售体没有 pinned 分支，始终加入 `inv_mass` | 公开方法直接调用零售体，不移植邻近分支；非等轴惯量的 Ballance contact-arm 案例验证最大旋转项、逆质量项和跨 DLL 的 64 位浮点返回 |
| `delete_and_check_vicinity` | 删除前先保证 movable sim unit 活跃；静态对象还会发现邻近 mindist、增长 friction system 并唤醒相邻 core | 完整方法被裁掉，但 `get_all_near_mindists` RVA `0x99F0`、Core ensure/grow/contact-refresh 与虚删除 helper RVA `0x9990` 都保留；`0xDAB0` 指令已包含被源码内联的 sim-unit 唤醒/清理分支 | 只重建短控制流，所有状态操作和最终 scalar-deleting destructor 继续进入零售函数；movable/static 两种领域场景验证先修复邻域后删除 |
| `beam_object_to_new_position` / next-PSI transform | 邻近对象方法先把 object pose 转成 core pose，再由 `IVP_Calc_Next_PSI_Solver::set_transformation` 更新时间、core、碰撞和 hull 状态 | 两个顶层方法均被裁掉；同一编译单元的 `calc_next_PSI_matrix` RVA `0x1E300` 与邻近实现逐段一致，且 time、quaternion、rotation-axis、mindist、broadphase、hull reset 等依赖均保留为可调用零售入口 | 只选择性重建顶层控制流，不为被裁掉的方法伪造 RVA；休眠复合对象和活动 repeated-call 两条领域场景验证位姿转换、碰撞重建与优化边界 |
| `change_unmovable_flag` / Simulation Unit 所有权 | movable/static core 使用不同 friction 表示；切静态时断开所有接触、冻结活动 core、替换为独立 unit、清空 controller 并重新加入 gravity | 顶层对象方法被裁掉；unit `0x24`、manager `0x1B0` 的字段访问，unit/entry 析构、构造、增删 core、manager 链表、Core controller 和零售 allocator 均有保留指令；`ensure_cores_movement` 与 `clear_movement_check_values` 被折叠到同一 RVA `0x120B0` | 只重建顶层事务和源码内联小方法，其余状态操作进入零售入口；移动复合物体固定化与静态障碍恢复可动案例验证接触、friction、仿真 unit 和 controller 的顺序及所有权 |
| Simulation Unit split/fuse / controller 排序 | 邻近 `split_sim_unit` 在第三分量残留时递归；`union_find_get_father` 标成 inline | 零售 RVA `0x11470` 用 do-while 尾循环再分配 MOVING unit；father walk 实际保留为 RVA `0x1C7D0`，沿 `Core+0x228` tmp 链迭代；`fusion` 由 mindist managed-friction 两次调用；`sim_unit_remove_core` 原先被误标成 `int(void*,int)` | split/fuse/sort/calc_redundants、father walk 及 leaf 状态操作直接进入零售体；host resolver 按零售控制流建模并覆盖 2/3 分量 split、fuse 注销 donor、controller 去重与 priority 排序；不进入 1575 公开分母 |
| Contact Point / friction pair 尺寸 | 邻近配置及导入类型把 Contact Point 记为 `0x88`，尾部含 world-space contact point | 零售工厂只分配 `0x78`；构造访问至 `+0x70` system backlink，RVA `0x1F850` 在 `+0x68` 修改 8 字节时间；pair 独立分配 `0x38` | 使用 `0x78/0x38/0x0C` 的零售布局；公开已确认的 synapse、friction、material、拓扑和 manager 操作，不提供原版无存储的 `get_contact_point_ws` |
| friction pair listener 派发 | `add_fr_pair/del_fr_pair` 在 vector 操作前后调用 environment 的 pair-created/pair-deleted 事件 | Ballance 的 collision listener vtable 没有两个 pair 槽；RVA `0x1C6F0/0x1C720` 只操作 `fr_pairs_of_objs +0x34` | 包装直接调用两个零售体，不移植不存在的 pair 事件，也不扩张 listener vtable |
| Core 内部状态入口 | 多个 `.cxx` 方法在头中公开可见 | 七个函数体保留但 IDB 原先匿名；另有 freeze、movement-state、init、析构等具名实现 | 83/83 精确处置、82 项可调用；匿名函数按机器码恢复名称/类型，仅废弃的 merged-core 拆分入口以 `= delete` 拒绝 |
| `IVP_Core(IVP_Real_Object *)` | 受保护的单对象构造先 `init`，再把新建 simulation unit 登记到环境 manager | 顶层小构造体被裁掉，但 `init` RVA `0xD2F0`、manager add RVA `0x11EF0` 和 `IVP_Environment::sim_units_manager +0x08` 均保留 | 归入选择性重建：严格复现两步顺序并继续调用两个零售入口；不伪造构造函数 RVA，也不再误标为不可用 |
| Core / Simulation Unit 内嵌 Vector 生命周期 | 普通 C++ 成员会在包装构造函数体前自动构造，并在包装析构函数体后自动析构 | Core 完整构造 RVA `0xD400` 管理 `controllers_of_core +0x1C8`，完整析构 RVA `0xD580` 释放并清空它；Simulation Unit 完整构造/析构 RVA `0x11920/0x11890` 同样管理 `+0x0C/+0x1C` 两个 Vector | 改用匿名 union 仅提供原字段名和存储，禁止宿主编译器包上第二层生命周期；poison-storage 测试证明进入 DLL/重建初始化器前 Vector 字节未被宿主预写，并且完整析构只派发一次 |
| `IVP_Template_Real_Object` / `IVP_Template_Spring` 分层构造 | 派生完整构造器会先调用模板基类构造器 | RVA `0x15EF0` 调用只清空 name 指针的 `0x15E90` 后整体清零 `0x70`；RVA `0x14200` 调用只清三个指针的 `0x13E70` 后整体清零 `0x38` | 明确记录为原版旧式、无资源的重复写入；没有误套 Constraint Local 的拆分方案，也不为它们臆造内部 initializer |
| Environment debug-vector 生命周期 | 邻近实现以 LIFO 单链表保存起点、方向、颜色和复制后的标签 | `add_draw_vector` 被裁掉，但 `delete_draw_vector_debug` 的完整实体保留在 RVA `0x13940`：沿 `draw_vectors +0x168` 遍历，释放 `node+0x4C` 标签，调用 RVA `0x13920` 的节点完整析构与零售 `operator delete`，最后清空表头 | `add_draw_vector` 仍按邻近控制流在 Ballance 布局上重建；删除直接调用原版 RVA，真实 Environment 上的双节点场景验证 LIFO、复制标签和跨 DLL 释放，不再把二者都误记为裁剪函数 |
| `IVP_Anchor::move_anchor` | 公开声明用于在同一对象上移动 anchor；`init_anchor` 建立 object/core 两套坐标并加入对象链 | 顶层方法被裁掉，但 `0x30` Anchor 布局、cache 世界到对象变换、object 到 core 矩阵计算和矩阵乘法入口均保留 | 恢复为第 2 类：只更新 `object_pos/core_pos` 并返回原 anchor，不改变对象、actuator 或 intrusive-list 所有权 |
| `IVP_Incr_L_U_Matrix::debug_print_l_u` | 邻近 `.cxx` 有完整的逐元素 L/U 输出实现 | Ballance 确认的 `IVP_Incr_L_U_Matrix` 布局包含相同矩阵字段；方法只读数据，不需要不存在的字段、虚槽或所有权 | 按完整邻近函数体恢复为第 2 类；不为被裁掉的顶层方法伪造 RVA |
| `IVP_Incr_L_U_Matrix::exchange_columns_l_u` | 邻近公开头只保留声明；分解器分别提供 L/U 两个列交换原语 | Ballance DLL 保留 `exchange_columns_L`（RVA `0x35E80`）和 `exchange_columns_U`（RVA `0x35EE0`），且二者使用相同 `n_sub/aligned_row_len` 布局 | 恢复为第 2 类组合入口，按 L 后 U 调用两个原版函数；三接触变量重排案例验证两套因子与 padding |
| `IVP_Incr_L_U_Matrix::debug_print_a` | 完整邻近函数体通过 `inverse(L) * U` 还原并输出原矩阵，但依赖会断言且泄漏的 allocating great-matrix constructor | Ballance 保留安全的默认 great-matrix constructor，L/U 字段、步长及 `double` 精度均已确认；顶层诊断体被裁掉 | 保持原数学算法，改用调用模块内自动释放的临时数组承载三个默认构造矩阵；非平凡两接触分解验证输出的确是原矩阵 |
| `IVP_Great_Matrix_Many_Zero::get_number_null_lines` | 所有已找到的 IVP/Source 镜像都只保留声明，没有可复制函数体 | Ballance 的 `0x20` matrix header、`MATRIX_EPS`、逻辑列数、行步长和 row-major double 存储均由保留求解器确认 | 恢复为无副作用的第 2 类秩辅助函数：逐项检查完整逻辑行；零对角但仍有约束的行不误计，padding 不参与判定 |
| `IVP_Incr_L_U_Matrix::delete_row_and_col_l_u` | 旧公开头只保留 `void` 声明；邻近实现使用返回状态的 `decrement_l_u` 执行同一变量删除事务 | Ballance 保留完整 `decrement_l_u` RVA `0x35D00` 及其内部 L/U 原语 | 恢复为丢弃状态的旧 ABI 适配入口，不复制求解器；原版 DLL 对耦合 3x3 系统删除中间变量后保留正确的 2x2 子矩阵 |
| `IVP_Constraint` 修改槽 | 公开声明是 16 个 virtual 修改钩子 | VA `0x10063B20` 证明只有 simulation 槽纯虚；16 个修改槽均指向 RVA `0x376D0`–`0x37780` 的具体缺省诊断函数，其中四对槽共享函数体 | 基类方法保持 concrete 并调用原 DLL，派生 Mod 只需实现 simulation；不再把修改槽错误声明为 pure virtual |
| `IVP_Constraint` / `IVP_Constraint_Local` | 基类尾部含 `client_data`，使 Local 后续字段从 `0x1C` 开始 | 基类大小 `0x18`，无该字段；Local 大小 `0x190`，`force_factor` 从 `0x18` 开始 | 删除邻近修订字段，按零售构造函数、析构和 23 槽 vtable 恢复 Local |
| `IVP_Template_Surbuild_LedgeSoup` | 首字段是 `force_convex_hull` 指针，随后才是构建选项 | 只有四个 32 位构建选项，总大小 `0x10` | 删除该指针，保持四个原版字段的 `0x00/04/08/0C` 偏移 |
| `IVP_SurfaceBuilder_Ledge_Soup` 私有状态 | 邻近公开头给出 25 个字段及六个 vector 子对象 | 原版构造/析构和 builder 方法逐一命中同一组 `+0x00..+0x94` 偏移，整体为 `0xA0`；旧 IDB 仅把 `smallest_radius` 误显示为 `long double` | 恢复完整强类型字段；四个公开方法仍调用原版入口，并用构造/析构指令、offset 断言和 exact-DLL Pointsoup 构建共同锁定 ABI |
| 旧式 Polygon transport 模板 | `Point/Line/Surface/Polygon` 位于 compact builder 内部头，部分方法只在源码中出现 | Ballance 保留 Polygon 构造/析构和 Surface 构造/close/index 五个实体；指令确认 `0x18/0x30` 布局 | 恢复真实类型和共享拓扑字段，五个保留方法直接调用 DLL；未确认的分配型便利方法暂不伪造 |
| `IVP_U_Vector_Enumerator` / `IVP_U_BigVector_Enumerator` | 构造时令 `index = 0`，取元素后递减，却只检查 `index >= len` | 该实现第二次调用会访问 `element_at(-1)`，与枚举器用途及源码中的注释相冲突 | 保持相同接口，按 `len - 1` 到 `0` 倒序遍历并检查负下标 |

上述 Constraint 差异现也已写入规范 IDB，而不只停留在公开头文件。旧导入 UDT 的
`IVP_Constraint 0x1C` / `IVP_Constraint_Local 0x198` 已在零售指令防护下替换为
`0x18/0x190`：基类 vector 固定在 `+0x08`，Local 两个 anchor 固定在
`+0x70/+0xF8`，mapping 固定在 `+0x180/+0x183`。事务回读同时确认所有公开函数原型
仍可解析。

`IVP_Linear_Constraint_Solver` 也不再以 `std::byte[0x130]` 暂存。保留求解入口、字段访问
和邻近声明共同确定四个 binary64 epsilon/step 值、九个 double buffer、索引与状态区、
`lu_sub_solver +0x80`，以及 `+0xB0/+0xD0/+0xF0/+0x110` 的四个 matrix 对象。
公开头文件现保留这些真实私有成员并逐段断言偏移；规范 IDB 同步改用 `double`，不再把
`IVP_DOUBLE` 显示成误导性的 `long double`。
| `IVP_U_BigVector_Base` | 三个字段为 capacity、count、pointer；固定容量派生类把内联指针数组紧跟在基类之后 | RVA `0x21A50` 直接访问 `+0x00/+0x04/+0x08`，以 `this+0x0C` 判断是否为内联存储，并按 `2*n+1` 扩容 | 固定 `0x0C` 基类布局；exact-DLL broadphase 风格列表从 1 扩到 3 时验证元素身份保持和零售 allocator 配对 |
| `IVP_U_Vector_Base` / `IVP_U_Vector<T>` | 16 位 capacity、16 位 count 和一个指针；模板层不增加字段 | RVA `0xB600` 读取 `+0x00/+0x02/+0x04`，以 `this+0x08` 判断内联存储，并按 `2*n+1` 扩容 | 固定 `0x08` 基类及模板实例布局；exact-DLL Core/object 邻域列表从 1 扩到 3，验证顺序、身份和 allocator 配对 |
| `IVP_U_FVector<T>` | 与 Vector 共用 `0x08` 基类布局，元素自身保存两个集合反向索引 | 扩容同样直接进入 RVA `0xB600`；模板层无附加字段 | exact-DLL friction/mindist 双集合案例验证第二索引选择、中间项无序删除、尾项补位、另一集合保持和交换后的双向索引修复 |
| `IVP_U_Set<T>` / `IVP_U_Set_Active<T>` | Set 不增加 VHash 字段；Active Set 再附加一个 listener Vector | Phantom 构造 RVA `0x10D00` 以 `0x18` 分配 Active Set，在 `+0x00` 构造 VHash、`+0x10` 构造 `0x08` listener vector；Set 边界为 `0x10` | exact-DLL Core membership 案例验证 identity add/find/install/remove；forcefield 风格 Active Set 验证重复安装抑制、add/remove 通知顺序和析构通知 |
| `IVP_Synapse` | 注释声称大小为 32 字节 | 原版嵌入对象中的步长为 `0x1C`；RVA `0x16610` 写 object `+0x10` 和 mindist offset `+0x18`，RVA `0x16470` 读 edge `+0x14` | 使用 `0x1C` 布局；exact-DLL 案例验证有符号回偏移、object 绑定以及 links/edge/status 不被初始化入口破坏 |
| `IVP_Synapse_Friction` | 注释声称大小为 32 字节 | 原版对象链步长为 `0x14`，contact offset/status/edge 位于 `0x0C/0x0E/0x10` | 使用 `0x14` 布局，不保留虚构填充 |
| `IVP_Anomaly_Limits` | 后续修订含 collision-check 和 friction-mass 字段，总体约 `0x20` | 原版构造函数只初始化到 `0x10`，对象大小 `0x14` | 只公开原版三项限制，不加入后续字段 |
| `IVP_Anomaly_Manager::inter_penetration` | 四参数，末尾有 `IVP_DOUBLE` | 原版装饰名与 `ret 0x0C` 均证明只有三个指针参数 | 公开三参数 Ballance 签名，并把审计差异保留为显式版本差异 |
| Anomaly 析构槽 | 两个类型都在其他虚方法之后声明析构 | Limits 为槽 1；Manager 为槽 6 | 析构保持末槽；不能因实现方便移到槽 0 |

## `IVP_Environment`

邻近源码在 `core_revive_list` 后声明 `constraint_listeners`，并在 `pw_count` 后声明
`IVP_Environment_Manager *environment_manager`。Ballance 原版保留后者、删除前者：RVA
`0x12A60` 只初始化 `+0xF4/+0xFC/+0x104` 三个 vector，把授权字符串、授权码、计数和
manager 依次写入 `+0x10C/+0x110/+0x114/+0x118`。RVA `0x13270` 又释放 `+0x10C` 的
字符串并通过 `+0x118` 从 manager 列表解绑，完整对象范围仍为 `0x178` 字节。

Manager 本身也不再只依赖导入 UDT。零售私有构造 RVA `0x136E0` 清零优化标志
`+0x00`，并把 environment vector 的 `len/memsize/elems` 写到 `+0x04/+0x06/+0x08`；
静态初始化 thunk 以 VA `0x10075DA0` 调用它，getter RVA `0x138B0` 原样返回同一地址。
强制 Ballance DLL 案例确认加载后的 singleton 地址、优化标志和空 vector 状态，并进一步
通过 factory 创建真实 Environment，核对授权字段/backlink，再经零售析构与 operator delete
恢复空 manager 列表。因此
`IVP_Environment_Manager` 的完整 `0x0C` 布局升级为零售确认。它的析构 thunk RVA
`0x13890` 把同一 singleton 交给 RVA `0xAAD0`；该函数按 owner `+0x04` 销毁 vector，
但与 callback-table 析构器发生 linker folding，所以证据账本登记为函数体变体，IDB
仍只保留一个主名称。

公开访问边界也按原头文件恢复：`IVP_Environment_Manager` 的默认构造为 private，Mod
只能取得 DLL singleton；`IVP_Cache_Object` 的 time-code/reference-count/object backlink
以及 `IVP_Cache_Object_Manager` 的 count/reuse-index/buffer 均为 private，而不是可由 Mod
任意改写的状态。Cache 的矩阵、位置和查询操作仍为 public。原内联
`remove_reference()` 在 debug 断言引用数为一后无条件减一；当前实现恢复这一状态转换，
不再把错误引用数静默吞掉。

两个 Real Object cache accessor 现在也不再只照抄邻近头文件。匿名零售 RVA `0x1A190`
是编译器为公开内联 `IVP_Real_Object::get_cache_object()` 发出的共享实体：缺 cache 时通过
`object+0x18 -> Environment+0xA4` 调用 manager RVA `0x18930`，随后先增加
`IVP_Cache_Object+0x04` 的引用计数，再按 `object+0x80` 的 movement state 和
`Environment+0x138` 的 time code 决定是否调用 update RVA `0x18A40`。此前包装层把 update
放在加引用之前，并加入原版没有的空 environment/cache 分支，现已按这条 retail 指令顺序
修正。no-lock 版本没有独立装饰名实体，但具名 Ray Solver Object 构造 RVA `0x22030`
完整内联了相同的创建/时间码刷新路径，且不会写引用计数。两者仍作为本地公开 inline
实现，不把编译器私有实体伪装成稳定 DLL 导出；证据审计为此新增独立的
`retail-inline-body` 类别。

容器模板也不能只按现代直觉整理。匿名 RVA `0x24C20` 是多个 compact 16-bit
`IVP_U_Vector<T>` 实例共享的分配构造体；Phantom 的 Active Set listener vector 与 Impact
Solver 的 Core/stack vector 都直接调用它。该实体先把传入的 `size` 截入 `memsize +0x00`，
清零 `n_elems +0x02`，再按 `size != 0` 从零售堆分配 `size * 4` 字节并写入 `elems +0x04`。
当前 API 此前只在分配成功分支写 `memsize`，且以 `size > 0` 判断，现已改成与这条零售指令
顺序一致。邻近同版本模板还表明 `clear()` 遇到 `elems == this + 1` 的固定内联 vector 时只清
元素计数，不能把内联指针和容量抹掉；`IVP_Vector_of_Cores_2` 的领域案例验证清空两个 Core
身份后仍保留容量 2，并可无分配再次加入。动态 vector 则由 RVA `0x24C20` 构造、当前公开
操作读取，最后经零售堆释放。它仍是编译器共享的内联实体，不作为稳定导出地址公开。

Active Float dependency 同样留下了可核对的编译器共享体。Spring Active 完整构造在
RVA `0x14557/0x1457B/0x1459F/0x145C6` 四次调用 `0x15BA0`；该体先在 owner `+0x0C`
的 compact listener vector 追加 listener，再增加 `reference_count +0x08`。对应析构四次
调用 `0x15BE0`，它先按 vector 的注册项删除规则移除 listener，再减少引用，并在归零时
调用 vtable slot 0 的 deleting destructor。当前 `remove_dependency()` 此前额外把“不存在”
静默处理成 no-op，这与零售体及邻近源码的已登记 listener 前置条件都不一致，现已删除该
分支。领域案例分别经公开方法和两个匿名 retail 体建立/拆除 actuator 参数依赖，验证引用数、
下一次值变更通知和解除后不再通知；匿名实体只登记为 `retail-inline-body`，不作为稳定导出。

另外恢复了此前整个漏掉的 `IVP_Event_Manager_D`。它与
`IVP_Event_Manager_Standard` 一样只覆盖 base slot 0，完整对象仍是 vptr 加 `mode` 的
`0x08`，不要求 Ballance 增加字段或虚槽；Ballance 没有保留它的专属函数体，因此按邻近
`ivp_time.cxx` 选择性复现“一次 advance 只取出一个 Min List 事件”的策略。override 按原
头文件保持 private，调用者通过 `IVP_Event_Manager` 接口使用。确定性时间场景用两个分别
位于 base time 后 2 秒和 4 秒的事件，确认两次推进各消费一个事件、回调时钟分别为
12/14 秒，而每次返回前环境时钟推进到请求的 15/16 秒。这属于第 2 类重建，不宣称存在
可跳转的零售 RVA。

`IVP_SurfaceManager` 的析构也恢复为原公开声明的 pure virtual destructor，并在类外提供
必需的空定义；这不改变 Ballance 的析构 slot，却避免当前头文件把抽象接口错误描述成
带普通虚析构实现的类型。`IVP_Event_Manager_Standard::simulate_time_events` 同样恢复原来的
private override 访问边界，仍可通过公开 base interface 调用零售实现。

创建链本身也已重新核对。RVA `0x137E0` 的 manager factory 固定分配 `0x178` 字节，
然后依次传入 manager、`IVP_Application_Environment *`、customer name 和 authorization
code；RVA `0x12A60` 因而是带四个参数的 private `IVP_Environment` 构造器，不是旧 IDB
所写的默认构造器。静态 getter RVA `0x138B0` 无隐藏 `this`，直接返回 VA
`0x10075DA0` 的 manager singleton；该地址之后 `0x10` 字节处正是已独立确认的全局
Mindist Settings。两个公开 manager 方法已恢复精确装饰名，因此可调用体审计不再把它们
留在 unclassified 名称状态。

已确认的原版关键偏移如下：

| 成员 | 偏移 |
| --- | ---: |
| `static_object` | `0x30` |
| `statistic_manager` | `0x38` |
| `freeze_manager` | `0x98` |
| `delta_PSI_time` | `0xC0` |
| `gravity` | `0xD0` |
| `collision_listeners` | `0xF4` |
| `current_time` | `0x120` |
| `state` | `0x144` |
| `client_data` | `0x16C` |
| `environment_magic_number` | `0x170` |

还有一项相关但不是邻近源码差异的问题：IDA 曾将 `IVP_Statistic_Manager` 导入为
`0x58` 字节，并把两个 64 位成员按四字节边界排布。RVA `0x15E50` 明确以
`rep stosd` 清零 `0x18` 个 dword；环境构造又在 `+0x38` 构造它，并在 `+0x98`
构造下一项 `IVP_Freeze_Manager`。因此 Ballance ABI 中该类型实际为 `0x60`：
`l_environment +0x00` 后有四字节对齐填充，`last_statistic_output +0x08`，
`sum_energy_destr +0x38`，`global_fmd_counter +0x58`，末尾再填充到 `0x60`。
头文件现用真实 MSVC 对齐和逐字段 `offsetof` 约束表达该布局；规范 IDB 也已在零售
指令防护下改为相同的 `0x60` UDT，不再保留错误的 `0x58` 导入结果。

两个内嵌 manager 也已从“环境总体偏移正确”拆成各自可复核的完整布局。RVA `0x15E50`
用 `rep stosd` 清零 `0x18` 个 dword，直接给出 `IVP_Statistic_Manager` 的 `0x60` 边界；
精确 DLL 案例验证完整清零，并验证 `clear_statistic()` 只重置一次报告窗口的 impact/
collision 计数，保留累计能量、mindist 和全局计数。RVA `0x12A40` 调用 `0x12A50`，后者
只在 `IVP_Freeze_Manager+0x00` 写入 float `0.3`；环境构造又在 `+0x98` 构造它，下一成员
从 `+0x9C` 开始。案例同时验证构造默认值和运行中重新初始化，不把四字节字段仅按导入
类型猜成整数。

`IVP_Controller_Manager` 的公开构造器没有独立保留函数体，但不是未知实现。Environment
构造 RVA `0x12A60` 在 `0x12DB6` 只分配 `4` 字节，内联写入当前 environment 指针，随后
把结果存到 `environment+0xA0`；保留的 `announce_controller_to_environment` RVA
`0x11B30` 又从 `manager+0x00` 读取同一 backlink 传给 simulation-unit revive。因此
当前 `0x04` 单指针布局和本地构造器属于有零售调用点支撑的第 2 类重建，并未给内联函数
伪造 DLL RVA。

`IVP_Inline_Math` 是用 class 表达的纯静态工具集合，不存在可构造实例或 `this` 指针。
原版保留的 `isqrt_float`/`isqrt_double` 也都是静态入口。审计将它显式记为
`layout-not-applicable`，而不是为了消除未分类项虚构一个 owner 大小，也不把它误算为
零售确认的对象布局。

`IVP_Time` 是单个 `IVP_DOUBLE`，完整大小 `0x08`，不是 float 时间码。原版
`IVP_Environment::set_current_time` RVA `0x138F0` 从 by-value 参数读取两个连续 dword，
写到 `current_time+0x00/+0x04` 并以 `ret 8` 返回；Friction System 的 RVA `0x1CBC0`
同样把两个 dword 原样转发给所有 contact。精确 DLL 案例使用非平凡小数时间验证两个
半字都能跨边界保存，并确认 setter 同时递增 environment time code。IDB 中
`IVP_Time_Manager::env_set_current_time` 和 `IVP_Friction_System::reset_time` 原先遗留的
`int` 返回及错误 `this` 类型已按装饰名、`ret` 和指令流修正。

公开面自动审计共找到 20 个记录按值方法，其中真正调用原版 DLL 的 5 个全部传入
`IVP_Time`；该 `IVP_EXPORT_PUBLIC` 候选集中的 4 个记录返回值全部是本地内联方法。
邻近 private collision header 的 `IVP_3D_Solver` 不在这个分母中；其保留的
`calc_nullstelle` 是另行确认并由专用 thunk 处理的 VC6 隐藏返回指针边界。原版
`IVP_Core::calc_at_matrix`、`IVP_Real_Object::calc_at_matrix` 都从 `[ebp+8]` 读取 qword，
并以 `ret 0x0C` 清理时间值和输出指针；前者 IDB 的错误 `int` 返回、后者把 `IVP_Time`
抹成裸 `double` 的类型现已修正。`IvpRetailByValueAbiTest` 对指定原 DLL 实际运行 Core 与
Real Object 的 AT 插值、静止 Core 的 slow/calm 判定、性能窗口时间重置和 Controller
默认槽的 `ret 8` 栈平衡。另有 190 个枚举按值方法；38 个公开枚举均显式固定为
`std::int32_t`，避免消费者启用不同枚举推断选项时改变 ABI。

## `IVP_Mindist_Settings` 与全局碰撞容差

Ballance 保留了 RVA `0x15FE0` 的
`IVP_Mindist_Settings::set_collision_tolerance(IVP_DOUBLE)`，装饰名中的 `X` 表示返回
`void`，函数以 `ret 8` 清理唯一的 8 字节参数。IDA 旧类型却把它标成返回 `int`、
`this` 为 `float *`；该类型已修正。RVA `0x15FD0` 的静态初始化器把 ECX 明确加载为
VA `0x10075DB0`，再调用 RVA `0x160C0` 的构造函数，因此全局 settings 对象地址不是
依据邻近源码猜测出来的。导入的 `IVP_Mindist_Settings` 大小 `0x138` 与 setter 最高字段
访问及邻近布局一致，脚本会在写库前校验该尺寸。

原版 setter 把 `real_coll_dist` 和 `min_coll_dists` 写到 `+0x00/+0x04`，后者就是公开
getter 的结果。邻近 `IVP_Environment` 的两参数包装在该二进制中被裁剪/内联；当前 API
用版本锁定的 setter 操作这个已确认全局对象，并从 `+0x04` 读取 getter。第二个
`gravity_length` 参数在邻近实现和 Ballance setter 中都未被使用，因此兼容包装保留公开
签名但明确忽略它。

## `IVP_Material` 虚接口

原版抽象材质表位于 VA `0x1006344C`，槽 0–4 全部指向 `_purecall`，槽 5 指向 RVA
`0xBE10` 的 scalar deleting destructor。`IVP_Material_Simple` 表位于 VA
`0x10063464`，依次指向 friction、second friction、elasticity、adhesion、name 和
deleting destructor；RVA `0xBF00` 的构造函数会把该表地址写到对象 `+0x00`。RVA
`0xBF70` 的完整析构先恢复 Simple vptr，再调用 RVA `0xBE00` 的基类析构，后者恢复抽象
基表。这个序列同时确认了 `IVP_Material` 大小 `0x0C`、Simple 大小 `0x30`，以及
`material_type`/`second_friction_x_enabled` 的 `+0x04/+0x08` 偏移。

因此兼容头现在直接声明同序的五个纯虚函数和虚析构。这样 Mod 可以实现自己的材质；
对原版创建的材质执行 `delete`/`destroy()` 时仍会经对象的原版第 5 槽进入
零售析构。精确 DLL 材质案例还验证了非默认 friction/elasticity、禁用的第二摩擦值、
零 adhesion 以及原版静态名称；其中 adhesion 是 Ballance 保留而邻近公开围栏排除的槽，
因此执行测试但不混入 1575 个邻近公开候选分母。

`IVP_Liquid_Surface_Descriptor_Simple` 的零售构造 RVA `0x10820` 安装 VA
`0x10063558` 的 vtable，把四 float 的 Hesse 平面复制到 `+0x04`，并把三分量绝对流速
复制到 `+0x14`，确认派生对象大小为 `0x24`。RVA `0x107D0` 原样输出这两组数据且不读取
传入的 environment/Core；精确 DLL 案例使用非零平面常数和非平凡流速验证所有有效分量。
此前的显式 `IVP_Material_VTable*` 已移除。

邻近源码用 `INTERN_START/INTERN_END` 把 `get_adhesion` 标成 future/internal，因此
“邻近公开接口”审计会排除它；这不代表 Ballance 没有该槽。VA `0x10063434` 的
`IVP_Material_Manager` 表依次为 material lookup、friction、elasticity、adhesion、
deleting destructor、environment callback。当前 Material 与 manager 都按实际六槽表
保留 adhesion，并把 IDB 中 RVA `0xBDE0` 的错误 `std::locale::facet` 名修正为 manager
deleting destructor。

## `PhysicsControllerForce` 不是 `IVP_Actuator_Force`

IDA 旧数据库把 RVA `0x46A0/0x46B0/0x4760/0x4C50` 归到了
`IVP_Actuator_Force`。但 VA `0x10063240` 的七槽表名和 vptr xref 均指向
`PhysicsControllerForce`：RVA `0x4930` 的 `PhysicsForceCall::Call` 分配 `0x40` 字节，
直接写入这张表，并填充 manager、core、位置和力向量。对应行为源码
`Behaviors\PhysicsForce.cpp` 也定义了同一 adapter。真正的 `IVP_Actuator_Force` 在这个
零售 DLL 中没有可调用实现，不能拿这四个函数冒充其 API。

共享的 RVA `0x4C50` 被 VA `0x10063248/0x100633F4/0x10063410/0x10063510/`
`0x100635A0` 五张表引用，并始终返回 VA `0x10075D90` 的空 vector，因此它已改名为
`IVP_Controller_Independent::get_associated_controlled_cores`。相邻 VA `0x1006325C` 的
七槽表是原版 `IVP_Controller` 基表。

## Actuator 基类与 Two Point vtable

原版 `IVP_Actuator` 表位于 VA `0x100635C0`，`IVP_Actuator_Two_Point` 表位于
`0x100635E0`，两者均为 8 槽：core 删除回调、minimum frequency、受控 core 查询、
reset time、纯虚 simulation、priority、deleting destructor、anchor 删除回调。RVA
`0x13FA0` 直接返回 `this+4`，RVA `0x13FB0` 返回 `1500`；这分别确认受控 core vector
偏移和 `IVP_CP_ACTUATOR`。slot 0 与 slot 7 都指向 RVA `0x28950`，这是编译器把两个
“删除自身”的回调实现合并后的结果；数据库在两个槽位保留各自语义，但不把共享代码
地址强命名成其中任意一个。

RVA `0x13F60` 的基类构造初始化 `+4/+6/+8` 的 vector 并安装 `0x100635C0`；RVA
`0x13FE0` 的完整析构释放同一 vector，随后恢复 `IVP_Controller` 基表。Two Point 构造
RVA `0x14020` 从 `+0x0C` 构造两个步长 `0x30` 的 `IVP_Anchor`，在 `+0x6C` 保存
`client_data`，并安装 `0x100635E0`；RVA `0x14150` 反向完成注销、anchor 析构和基类
析构。这些证据共同固定了 `IVP_Actuator` 的 `0x0C` 基布局和 Two Point 的 `0x70`
布局。真正的 Force、Torque 与 rotational motor 在该 DLL 中被链接裁掉，当前实现因此只在
上述已验证基类/anchor/controller ABI 上重建对应公开短算法，不把 adapter RVA 冒充成它们。

Anchor 自身也据此升级为零售确认布局：对象链指针位于 `+0x00/+0x04`，对象位于 `+0x08`，
`object_pos/core_pos` 位于 `+0x0C/+0x1C`，所属 actuator 位于 `+0x2C`，总大小 `0x30`。
RVA `0x13EA0` 的 `init_anchor` 访问这些字段，RVA `0x141F0` 的对象删除回调从 `+0x2C`
取出 actuator 并调用其 vtable slot 7。精确 DLL 生命周期案例确认传给该槽的是当前 anchor，
而不是即将删除的对象参数。

## Force、Torque 与 rotational motor

原版 DLL 没有保留这三族真正的构造、setter、simulation body、派生 vtable 或 environment
factory；`PhysicsControllerForce` 是 Building Block adapter，不能替代它们。因此这些入口
没有虚构 RVA，而是以邻近公开源码为算法参考，并逐项受 Ballance 导入布局、MSVC 多继承
规则和已经验证的 `IVP_Actuator_Two_Point` 生命周期约束。

关键 x86 布局为：

| 类型 | Ballance 大小 | 关键字段 |
| --- | ---: | --- |
| `IVP_Template_Force` | `0x1C` | `force +0x0C`、active pointer `+0x10`、两个完整 32 位 `IVP_BOOL` 位于 `+0x14/+0x18` |
| `IVP_Actuator_Force` / Active | `0x78` / `0x80` | `force +0x70`、两个运行态 bitfield 共用 `+0x74`；Active listener vptr/pointer 为 `+0x78/+0x7C` |
| `IVP_Template_Torque` | `0x20` | torque、最大角速度及三个 active/output 指针保持导入偏移 |
| `IVP_Actuator_Torque` / Active | `0x98` / `0xA8` | 轴 `+0x78`、`double rot_inertia +0x88`、输出 pointer/value `+0x90/+0x94` |
| `IVP_Template_Rot_Mot` | `0x28` | 最大角速度、power、最大 torque 和四个 active/output 指针 |
| `IVP_Actuator_Rot_Mot` / Active | `0xA0` / `0xB0` | 轴 `+0x7C`、`double rot_inertia +0x90`、输出 pointer/value `+0x98/+0x9C` |

创建和删除都使用 physics_RT 的 `operator new/delete`，anchor 初始化、controlled-core 登记、
controller announce/remove 与基类析构继续进入原 DLL。Active 派生类按次基类 vptr 布局登记
和解除依赖；速度输出 terminal 使用引用计数。六个独立行为案例验证 Force 的双物体等大
反向 world impulse 及 pinned/sleeping 抑制、Torque 的轴向冲量/限速/速度输出，以及 motor
的 `power / (angular_speed * rotational_inertia)` 换算、低速下限、方向处理、最大 torque
裁剪与三路 Active 更新。它们是构建内的确定性物理语义测试，尚不宣称为 Player 场景。

## Stiff Spring 与普通 Spring 是两个类型

邻近公开接口同时定义 `IVP_Actuator_Spring` 和
`IVP_Controller_Stiff_Spring`，两者不能互相 typedef。Ballance DLL 保留了前者的构造、
setter、simulation body 和 vtable，却把后者全部链接裁掉。IDA 导入类型仍给出与邻近
声明一致的 Stiff Spring 布局；其共享给原版 controller manager 的部分只到已经验证的
`IVP_Actuator_Two_Point` `0x70` 前缀，后续字段由当前重建代码自行使用：

| 类型 | x86 大小 | 关键字段/基类 |
| --- | ---: | --- |
| `IVP_Template_Stiff_Spring` | `0x20` | Two Point `0x0C`，随后 rest length、constant、damp、break policy/length |
| `IVP_Template_Stiff_Spring_Active` | `0x2C` | 三个 active-float 指针位于 `+0x20/+0x24/+0x28` |
| `IVP_Controller_Stiff_Spring` | `0x90` | Two Point `0x70`；environment、四个 float/policy 与 listener vector |
| `IVP_Controller_Stiff_Spring_Active` | `0xA0` | listener 次基类 `+0x90`，三个 active 指针 `+0x94..+0x9C` |

simulation 按两个 anchor 的当前世界位置建立一维约束方向，显式计算平移逆质量与
`r × n` 产生的转动响应，再应用恢复和相对速度阻尼冲量。这与邻近
`IVP_Solver_Core_Reaction` 的一维公式等价，但不把该受保护内部 solver 伪装成新的公开
零售入口。三个独立测试覆盖不同质量下的恢复力、相对速度阻尼、超长断裂回调/自删除，
以及三路 Active 参数的更新和解绑。

## Check Dist 的状态语义与 hull 生命周期

`IVP_Template_Check_Dist`、`IVP_Anchor_Check_Dist` 和
`IVP_Actuator_Check_Dist` 的导入布局分别为 `0x58/0x20/0x58`，与邻近公开声明逐字段
一致。DLL 同样没有保留这组构造和方法体；不过 anchor 作为
`IVP_Listener_Hull` 进入原版对象的 hull min-list，因此当前实现只复用已验证的
`IVP_U_Min_List` 零售入口和 `IVP_Hull_Manager` 布局，状态判断本身保持短内联实现。

每次求值把两端 object-space anchor 转回当前世界坐标，比较距离与 range，更新
`is_outside` 和可选 `IVP_U_Active_Terminal_Int`，然后按 `abs(length-range)/2` 重插两个
hull listener。构造会先注册两端再做首次求值；析构先发删除通知，再从两个 hull 中移除。

旧头文件把 listener 的布尔参数命名成 `distance_shorter_than_range`，但相邻实现进入范围
时传 `IVP_FALSE`、离开范围时传 `IVP_TRUE`，与字段 `is_outside` 和输出名
`mod_is_outside` 一致。因此当前接口不擅自翻转行为，而把该参数解释为新 outside 状态。
行为测试让两个 Ballance 布局对象实际跨越阈值两次，同时核对输出、去重 listener、
hull 重插和删除数量。

## Four Point / Stabilizer 的邻近死代码缺陷

导入 UDT 与公开声明共同给出 `IVP_Template_Four_Point` `0x14`、
`IVP_Actuator_Four_Point` `0xD0`、`IVP_Template_Stabilizer` `0x1C` 和
`IVP_Actuator_Stabilizer` `0xD8`；后者的 environment/constant 位于 `+0xD0/+0xD4`。
DLL 没有保留这组构造、simulation body 或派生 vtable，所以不能给它们编造零售 RVA。

邻近 `IVP_Actuator_Four_Point` 构造有两处互相佐证的死代码问题：它只在
`physical_unmoveable` 为真时安装 controlled core，而同文件 Two Point 构造明确安装
可移动 core；它还完全没有 `announce_controller_to_environment`，但析构无条件执行
remove。若原样复制，Stabilizer 不会进入 PSI，列表里即使有元素也只会是不能施力的
static core。当前兼容实现沿用已验证 Two Point 的规则：四个 anchor 全部初始化，对可移动
core 去重登记，再向首个 core 的 environment announce 一次。析构只对应 remove 一次。

Stabilizer simulation 本身保持邻近短算法：分别测量两对 friction-core anchor 的距离，
用 `(distance1-distance0) * constant * delta_time` 生成大小相反的两组冲量，并在每对内施加
等大反向 world impulse；sleeping 或 pinned core 不受力。一个领域测试使用长度 2/4 的
两组 anchor 验证四个冲量、controlled-core 去重、注册/注销和状态抑制。模板中保留的
`active_float_stabi_constant` 没有对应公开 Active 派生类，邻近 factory 也不消费它；当前
不通过改变 `0xD8` 继承布局擅自增加订阅行为。

## Spring Active 多继承布局

原版 `IVP_Actuator_Spring` 主表位于 VA `0x10063604`，共 8 槽；Active 派生类的主表
位于 `0x10063628`，而 `IVP_U_Active_Float_Listener` 次基类的一槽表位于
`0x10063624`。RVA `0x144D0` 的构造函数把次基类 vptr 写到完整对象 `+0x98`，再把四个
active-float 指针写到 `+0x9C..+0xA8`，因此完整大小为 `0xAC`。RVA `0x14440` 接收的
ECX 是 `+0x98` 的 listener 子对象，并在调用 Spring setter 前减去 `0x98`；RVA
`0x14610` 依次解除四个依赖后调用 Spring 析构。这些指令确认了当前公开
`IVP_Actuator_Spring_Active` 的多继承布局。当前 API 不再只按源码重写这层：
`IVP_Actuator_Spring` 完整构造、断裂监听派发、Active Float 回调和
`IVP_Actuator_Spring_Active` 完整构造分别直达 RVA
`0x14250/0x14220/0x14440/0x144D0`。Spring 的 listener vector 以 inactive union
提供存储，两个父类的 storage-only 构造路径防止在进入完整 DLL 构造器前重复建立
Actuator vector、anchors 和 listener vector；析构仍按已确认的分层链逐层执行一次，
避免完整析构器与 C++ 自动基类析构重入。Host 场景验证四个 Active Float 参数变化、
断裂 listener 逆序派发、销毁解绑以及跨 DLL 构造入口前未预写的字段区域。

## `IVP_Environment` 析构调用约定

RVA `0x13270` 没有虚表入口。RVA `0x6B70` 和 `0x79B0` 的两个 CKIpionManager 调用点
都直接调用该函数，随后把同一指针交给 physics_RT 的 `operator delete`。因此 IDB 中
原来的虚析构装饰名已改成非虚 `??1IVP_Environment@@QAE@XZ`。公开类提供真实析构和
类专用 `operator delete`，`destroy()` 只是 `delete this` 的安全便利入口，确保释放仍在
原版运行时中完成。

## `IVP_Template_Object` 与 `IVP_Template_Real_Object`

基础 `IVP_Template_Object` 只有一个 `+0x00` 名称指针，大小为 `0x04`。构造 RVA
`0x15E90` 清空该指针；`set_name` RVA `0x15EC0` 先通过 physics_RT 的 free 释放旧副本，
再调用保留的字符串复制分配函数；析构 RVA `0x15EA0` 释放最终副本并清零。精确 DLL
案例用可修改调用方缓冲区证明名称是深拷贝，并连续替换为两个 Ballance 风格对象名，
最后由原版析构跨同一 allocator 边界释放。

`IVP_Template_Phantom` 构造 RVA `0x10880` 清零三个 `IVP_BOOL`，并把
`exit_policy_extra_radius/time +0x0C/+0x10` 都设为 `0.5f`，确认 `0x14` 布局。测试在
这些默认值上启用对象成员跟踪并调整退出半径，覆盖实际 trigger-volume 配置，而非只检查
构造函数可达。

邻近源码把 `enable_piling_optimization` 放在 `physical_unmoveable` 和 `pinned` 之间。
原版 RVA `0x15EF0` 的模板构造函数确认对象总长 `0x70`；更关键的是 RVA `0x9787`
在调用 `IVP_Core` 六参数构造函数前，从模板 `+0x10` 取出最后一个
`enable_piling_optimization` 参数。由此可知 Ballance 保留的是该字段，省略的是它后面的
模板 `pinned`；旧 IDB 只凭导入名称作出了相反判断，现已纠正。原版布局为：

| 成员 | 偏移 |
| --- | ---: |
| `physical_unmoveable` | `0x0C` |
| `enable_piling_optimization` | `0x10` |
| `material` | `0x14` |
| `mass` | `0x18` |
| `rot_inertia` | `0x24` |
| `speed_damp_factor` | `0x38` |
| `extra_radius` | `0x60` |
| `client_data` | `0x68` |

直接采用邻近源码的两个连续字段会让 `material` 之后整体错开 4 字节，最早会把材质指针
和质量解释错误。模板没有 `pinned` 不等于运行时对象不能调用 `set_pinned`；只是对象构造
模板不携带该策略。

同一构造体还确认 `speed_damp_factor` 和 `rot_speed_damp_factor` 的 ABI 存储仍是
`IVP_DOUBLE`，但默认常量来自源码中的 `0.01f`：零售先采用 float 精度，再扩展为
double，实际值为约 `0.0099999997764825821`。不能因为这个初始化序列把字段错误改为
float，也不能在精确回归中拿 double 字面量 `0.01` 做逐位相等比较。

`set_nocoll_group_ident` RVA `0x15F70` 写入基类名称指针之后的 `+0x04..+0x0B`
八字节数组；非空标识最长为八字节，传入 null 时只需把首字节清零即可恢复空 C 字符串。
精确 DLL 物理创建模板案例验证默认质量、惯性、阻尼、碰撞/材质状态，再设置和清除一个
七字符 no-collision group。

## `IVP_Object` / `IVP_Real_Object` 继承与槽序

原版 VA `0x100633A0` 的 `IVP_Object` 基表只有 deleting destructor 一槽。VA
`0x10063390` 的 `IVP_Real_Object` 表恰有三槽：RVA `0x9810` 的 deleting destructor、
RVA `0x9520` 的 protected `set_new_quat_object_f_core`、RVA `0x93D0` 的 public
`set_new_m_object_f_core`。后两个槽的顺序不能由访问级别推断，也不能按包装函数在头文件
中出现的方便顺序排列。

当前声明恢复真实继承链
`IVP_Object -> IVP_Real_Object_Fast_Static -> IVP_Real_Object_Fast -> IVP_Real_Object`，
中间前缀分别固定为 `0x40/0x88`，完整对象为 `0xB8`。此前把 Object vptr 表示成普通
`void *`、把中间层扁平化的做法虽然可能保持部分字段偏移，却不能产生原版虚表和基类
转换 ABI，现已删除。

## `IVP_Object` / `IVP_Cluster` 构造链与 Hull 成员生命周期

原版保留了 RVA `0x9B00/0x9B60/0x9BA0` 的两个 `IVP_Object` 构造路径和完整析构，
以及 RVA `0x9C80/0x9CB0` 的根 Cluster 构造和完整析构；带 father/template 的公开
Cluster 构造则被链接裁掉。当前公开构造按邻近源码只组合已确认的 Object 构造、
`objects = nullptr` 和 `IVP_CLUSTER` 类型写入，不虚构 Cluster RVA。完整析构按原版
`0x9CB0` 的循环语义反复通过当前 child 的 deleting-destructor 槽删除头结点；随后
Object 析构负责从父链表解绑。因此 Mod 构造的嵌套 Cluster 与原版对象共享相同的一槽
虚表形状和链表约定。

这里不能从 C++ `IVP_Cluster::~IVP_Cluster` 函数体直接跳到 RVA `0x9CB0`：该 RVA 是
complete destructor，已经在删除所有 child 后调用一次 `IVP_Object::~IVP_Object`；函数体
返回后编译器还会自动析构 Object 基类，造成父链解绑和名称释放各执行两次。当前实现只在
派生析构体复现原版 child 循环，再让编译器进入已经绑定原版 RVA 的 Object 析构，完整效果
相同且基类恰好析构一次。这是完整对象析构 ABI 的结构例外，不是“DLL 有体却漏接包装”。

更关键的是，RVA `0x1A740` 的 `IVP_Hull_Manager_Base` 构造函数会直接调用
RVA `0x300F0`，在 `this+0x20` 构造 `IVP_U_Min_List`；RVA `0x1A7A0` 同样调用
RVA `0x30170` 释放该成员。旧包装把 `sorted_synapses` 声明为普通自动生命周期成员，
进入包装函数体前后 C++ 还会各执行一次 Min List 构造/析构，导致双重分配与双重释放。
当前改用非自动激活的 union 成员提供完全相同的 `0x20` 存储和字段访问，同时只让零售
完整函数管理一次生命周期。`IvpObjectHierarchyTest` 用嵌套 owning Cluster、两个叶对象
和临时 Fast 对象验证插入、递归虚析构、父链解绑，并明确检查 Min List 没有第二次进入。

## `IVP_Controller_Phantom` 完整析构与内嵌容器

邻近公开头文件声明了非虚的公开析构函数，但原 IDB 没有给它归属名称。两个独立零售
调用点——`IVP_Real_Object` 完整析构 RVA `0x9830` 与
`convert_to_phantom` RVA `0xA2D0`——都会先调用匿名 RVA `0x108A0`，再调用
`operator delete`，证明这里是 `IVP_Controller_Phantom` 的完整析构体而不是任意清理
helper。RVA `0x10D00` 则是已有装饰名的 protected 完整构造函数。

`convert_to_phantom` 在 RVA `0xA301` 明确以 `0x40` 调用 `operator new`，反证了导入
IDB 和邻近源码中的 `0x48` 大小。Ballance 布局是：监听器向量 `+0x08`、active
mindist set `+0x10`、可选 object/core set 与两个计数 hash `+0x28..+0x34`，最后变换
时间 `+0x38` 占满至 `+0x3F`。邻近版本位于 `+0x40` 的 `client_data` 尚不存在，已经从
当前公开类型和 IDB 中删除。析构按逆序派发
`phantom_is_going_to_be_deleted_event`，清空所属 real object 的
`controller_phantom +0x1C`，释放四个可选跟踪对象，再拆掉两个内嵌容器。

因此当前包装和 Hull Manager 一样把两个内嵌容器放入非自动激活的 union：字段类型和
偏移不变，但构造、析构只由零售完整函数执行一次。此前依赖隐式 C++ 析构不仅不会进入
零售清理逻辑，还可能用 Mod 编译单元的成员析构破坏 DLL 所有权。领域测试
`IvpPhantomLifecycleTest` 覆盖两名监听器的逆序通知、对象状态恢复、object/core 跟踪
资源释放和内嵌容器单次析构。

该类原先还漏了 reference header 中供碰撞引擎使用的两个 protected 方法。匿名 RVA
`0x109E0` 是 `mindist_entered_volume(IVP_Mindist *)`：Controller 构造和 Mindist Manager
共四个调用点传入同一 `this + mindist`，函数把 mindist 加入 `+0x10` active set，按可选
object/Core counter 更新聚合集合，最后通知 Phantom listener。RVA `0x10B70` 是严格对称的
离开路径，两个 Mindist Manager 调用点以及 remove/counter/listener 指令链确认同一 ABI。
当前头文件已恢复 `IVP_Mindist_Manager` friend 和两个 protected 薄包装，直接调用这两个
零售体，没有扩大 public 访问边界。trigger-volume 案例在关闭可选 object/Core 聚合时让一个
mindist 完整进入、离开，检查 active set membership 与 listener 的两次领域事件。

## `IVP_Forcefield` 的证据边界

Ballance DLL 没有保留 `IVP_Forcefield` 专属构造、析构或 vtable，不能为它编造 RVA，
也不能把邻近 `.cxx` 直接当作零售实现。可用的交叉证据是 IDB 导入的 `0x10` UDT 与邻近
公开声明一致：`IVP_Listener_Set_Active<IVP_Core>` 位于 `+0x00`，
`IVP_Controller_Independent` 位于 `+0x04`，active core-set 指针和 owner flag 分别位于
`+0x08/+0x0C`。两个基类的引擎交互 ABI 已由零售 controller manager 和 active-set
路径独立确认，但这不会自动把整个派生类提升为零售确认。

当前接口据此选择性恢复集合监听和生命周期：构造时为既有 core 登记 controller，之后
跟随集合增删，析构时逐 core 注销并移除 listener；拥有集合时由集合删除回调销毁自身。
`IvpCoreAttachmentLifecycleTest` 用变化集合与 owning 集合两个场景检查 controller 子对象
身份、登记/注销、listener 清理、self-delete 和基类析构。该结果证明重建实现内部一致且
可供 Mod 派生，证据等级仍明确保持为 IDB/source + host test，而不是零售类专属行为。

## Car 系列的早期 Ballance 修订

车辆类型揭示了一个不能靠整体搬运邻近源码解决的版本断层。邻近
`IVP_CarSystemDebugData_t` 在两组四轮 torque 数组后又追加四个
`IVP_U_Float_Point` actuator 向量，因此大小为 `0x348`；Ballance 导入 UDT 明确为
`0x308`。更重要的是，`IVP_Controller_Raycast_Car` 把 debug block 嵌在 `+0x650`，完整
对象恰好在 `0x958` 结束。这一父对象边界为“不能加入四个向量”提供了独立的结构约束，
尽管尚无保留指令直接访问这些尾字段。

Car System 的虚接口也较早。导入的 `IVP_Car_System_vtbl` 是 `0x70` 字节、28 个 x86
槽；邻近源码中的 `set_powerslide`、`get_booster_time_to_go` 和末尾
`event_object_deleted` 不在表内，`do_steering` 只接收一个 `IVP_FLOAT`。导入的
`IVP_Controller_Raycast_Car_vtbl` 为 `0x74`，即相同 28 槽再追加一个 `do_raycasts`；首槽
被导入器误标成 `GetCarSystemDebugData`，按继承顺序实际只能是 deleting destructor。由于
原 DLL 已把这些具体 vtable 和车辆函数体全部裁掉，这组结论是两个相互一致的导入类型加
对象布局交叉证据，不提升为零售代码确认。

当前 `Car.h` 提供能守住边界的基础层：`0x304` template、`0x20` skid info、
`0x308` debug data、28 槽抽象 Car System、`0x7C` wheel、`0x98` temporary、`0x04`
axis 和 `0x0C` 单 core 内嵌 vector。`RaycastCar.h` 进一步按导入字段偏移恢复 `0x958`
具体 controller，并保持 `IVP_Car_System +0x00`、`IVP_Controller_Dependent +0x04`；
Mod 派生类只需实现受保护的 `do_raycasts`。轮射线准备、接触整理、稳定杆、悬挂冲量、
转向/轮胎力、增压器、调参和 manager 生命周期均从邻近 `.cxx` 逐段选择性重建，未引入
另一版本类体。`IvpCarGeometryTest` 用四轮双轴场景检查对象空间轮位、轮距/轴距、悬挂、
前轮转向、后轮扭矩唤醒、锁轮、增压器重入限制、debug-ray round trip，以及登记/注销时
次基类指针必须为 `this+4`。空中 PSI 场景还实际传递四条轮射线，验证无命中时的满伸/零
压力，以及额外重力、booster 速度和计时器更新。这些结果证明当前重建在 host 边界内部一致，但由于零售类体
已被裁掉，仍不提升为原版函数体确认。

邻近头中的 `get_wheel_position(IVP_U_Point*, IVP_U_Quat*)` 是例外：同一源码树的 raycast
 car、airboat 和 fake jetski 都只声明而没有实现，Ballance 也没有保留函数体或调用点，
公开代码搜索亦未找到可交叉验证实现。当前保留原签名并标成 `= delete`，不为它编造语义；
误用会在编译期被拒绝。其余 30 个 raycast-car 公开方法都有邻近实现基础。

## Real-wheel constraint car 的行宽修正

另一套更早的车辆实现直接把车身和各个轮子都作为 `IVP_Real_Object` 约束。Ballance IDB
保留了 `IVP_Constraint_Car_Object`、`IVP_Constraint_Solver_Car` 和 builder 的完整 UDT，
大小分别为 `0xB0/0xA0/0x30`；车身指针、两个 vector、`co_matrix +0x10`、六个约束开关、
十二个 fallback ballsocket 指针和环境/计数器偏移均与邻近头一致。不过 DLL 中没有保留
这三类的专属函数体或具体 vtable，因此这只能证明布局候选，不能证明算法就是零售代码。

当前实现逐段采用邻近的 effective-mass、旋转对齐和 x/z 双向平移约束，并只调用已经核对
过的 Ballance core、reaction solver、great-matrix 和 controller-manager ABI。邻近 builder
有一处不能照搬：它写入 `columns` 后立即调用 `set_value`，却没有设置
`aligned_row_len`；Ballance 保留的矩阵函数明确以该字段索引，且此构建的正确值就是
`columns`。当前 builder 因此在第一次矩阵写入前显式计算行宽。

`IvpCarGeometryTest` 不是只检查构造成功：它建立一个车身和四个独立轮 core 的 8x8
有效质量逆矩阵，赋予四轮不同的 x/z 速度后执行一次 PSI，逐轮检查接触点速度收敛到车身
表面速度，并检查系统 x/z 总线性动量不变；同时验证五个 core 的 controller 注册、wheel
反向指针和析构解绑。12 个公开方法因此都记为 source basis + host behavior，而不升级成
retail body。

## Spring 模板与 actuator

邻近源码在 `spring_values_are_relative` 后加入了
`spring_force_only_on_stretch`，运行时 `IVP_Actuator_Spring` 中也有对应成员。Ballance
原版 RVA `0x14250` 的 spring 构造函数按没有该字段的模板偏移读取参数，相关 setter
也访问没有该成员的运行时布局。

当前原版尺寸和关键偏移：

| 类型或成员 | 尺寸/偏移 |
| --- | ---: |
| `sizeof(IVP_Template_Spring)` | `0x38` |
| `IVP_Template_Spring::spring_constant` | `0x14` |
| `IVP_Template_Spring::break_max_len` | `0x24` |
| `sizeof(IVP_Actuator_Spring)` | `0x98` |

原版 RVA `0x14200` 的模板构造函数还会把 `break_max_len` 初始化为 `1e20f`；这与
邻近版本中常见的手写零初始化不同，因此当前 API 直接调用原版构造函数。

运行时调参同样直接进入原版：RVA `0x14370` 把 `IVP_DOUBLE` rest length 收窄写到
`spring_len +0x74`；RVA `0x14390/0x143B0/0x143D0` 先乘以
`spring_values_factor +0x78`，再分别写入 `+0x7C/+0x80/+0x84`。四个入口随后都调用
RVA `0x141C0`，由第一 anchor 的 object 找到 environment controller manager 并请求重新
进入模拟。精确 DLL 测试使用空 controlled-core 集合终止后续 Simulation Unit 遍历，验证
`1.5` 的 factor 将 `12.0/0.4/0.2` 变成 `18.0/0.6/0.3`，并确认四次调参产生四次 wake 查询。

因此不能只删除功能实现而保留占位字段；保留该字段同样会破坏后续字段 ABI。

## Suspension 的裁剪函数与车辆侧非对称力

`IVP_Template_Suspension` 和 `IVP_Actuator_Suspension` 在 IDA 导入类型中分别为
`0x40` 与 `0xA0`。模板是在 Ballance 的 `0x38` Spring 模板后追加
`spring_dampening_compression +0x38`、`max_body_force +0x3C`；运行时对象同样在
`0x98` Spring 后追加这两个 float，偏移为 `+0x98/+0x9C`。这些偏移与邻近公开声明
一致，但 DLL 没有保留 Suspension 的构造、setter、simulation、析构或 environment
factory，因而不能绑定到虚构的零售地址。

邻近派生模板构造会在 Spring 基类构造后清空整个 `0x40` 对象，再把
`break_max_len` 设为 `10e8f`；它并不继承普通 Spring 的 `1e20f` 默认值。当前构造显式
恢复这一结果。运行时构造则复用由 RVA `0x14250` 字段访问证实的 Ballance Spring
基布局与 reduced virtual mass 公式，再把压缩阻尼乘以同一质量因子。

simulation 保留车辆专用的非对称规则：相对速度为正时采用压缩阻尼，为负时采用普通
Spring 回弹阻尼；第二个 anchor（wheel）获得未裁剪冲量，第一个 anchor（body）获得
反向且按 `max_body_force` 裁剪的冲量。该 override 不执行普通 Spring 的 break-length、
only-stretch 或 pinned/sleeping 分支，这也是邻近实现的实际行为。两个领域测试分别覆盖
压缩/回弹方向和 body-only 裁剪，以及 `2` 与 `6` 的端点虚质量产生 `1.5` reduced mass
因子。IDB 只增加这两个尺寸已验证 UDT 的注释，不伪造已经被链接裁掉的函数或 vtable。

## `IVP_Listener_Collision` vtable

邻近源码的虚函数顺序是：

1. `event_pre_collision`
2. `event_post_collision`
3. `event_collision_object_deleted`
4. `event_friction_created`
5. `event_friction_deleted`
6. `event_friction_pair_created`
7. `event_friction_pair_deleted`
8. 析构

原版 vtable 只有：

1. `event_post_collision`
2. `event_collision_object_deleted`
3. `event_friction_created`
4. `event_friction_deleted`
5. 析构

这里不能用空实现“兼容”多出的槽位，因为从第一槽开始顺序就已经不同。原版环境全局
listener 的 RVA `0x13B80` 在 `enabled_callbacks & 1` 时调用槽 0；对象私有 listener 的
RVA `0xA850` 在 `enabled_callbacks & 2` 时用待删除对象调用槽 1；RVA `0xA8A0` 与
`0xA900` 分别用 friction event 调用槽 2、槽 3。槽语义不能只靠旧 IDB 名称判断：
`IVP_Mindist::try_to_generate_managed_friction` 的新 contact 分支调用 `0x13BC0/0xA8A0`，
而 `IVP_Contact_Point::~IVP_Contact_Point` 调用 `0x13C00/0xA900`。因此 slot 2 是
created、slot 3 是 deleted，恰好保留邻近版本中这两个回调的相对顺序。此前 IDB 把这四个
派发函数的 created/deleted 名称对调，现已用创建/析构调用图修正。精确原版 DLL 测试还用
两个 Mod-owned listener 执行 `0x13BC0 -> 0x13B80 -> 0x13C00`，验证 created、post、
deleted 均按逆注册顺序落入正确槽，禁用 listener 不会收到事件。原版

同一测试现在还从原版 RVA `0x13C70` 完成三次全局注册，而不是直接伪造“已注册”长度；
随后通过公开的 `remove_listener_collision_global` 短内联移除一个 listener，确认三个原版
派发器都不再调用它，再注册后又恢复为逆注册顺序。原版注册函数是 `add` 而不是
`install`，所以重复注册会产生重复回调；邻近源码的移除实现直接调用
`collision_listeners.remove`，要求调用者只移除仍在列表中的注册项。当前头文件明确保留这两个
所有权前提，没有擅自改成去重或容错语义。

`PhysicsCollDetectionListener` 的表位于 VA `0x10063214`，槽 0 指向 RVA `0x42B0`；
虽然 IDA 导入名把它写成 `event_pre_collision`，对应 Building Block 源码实现、构造时的
`POST_COLLISION | OBJECT_DELETED` 掩码以及函数实际读取的碰撞后结果共同证明它是
`event_post_collision`。这是 IDA 导入类型/名称只作为线索、不能单独定案的具体例子。
这些误名及七个派发函数的名称、原型和注释已经通过上述修正脚本写回分析数据库。

当前 `Listeners.h` 因而只保留上述原版四个回调并按零售槽序排列；对象大小仍为 `0x08`
（vptr 与 `enabled_callbacks`）。四个槽保留默认空实现，使 Mod 只需覆写启用的事件；
邻近版本的 pre-collision 和两个 friction-pair 回调不能加入该虚表。

## `IVP_Controller` vtable

邻近源码在 `get_controller_priority` 和析构之间声明了 `get_controller_name`。原版
controller 表没有这一槽，顺序为：core 删除通知、最小模拟频率、关联 core 列表、
重置时间、执行模拟、优先级、析构，共 7 槽。

若直接使用邻近定义，原版尝试析构 Mod controller 时会调用 `get_controller_name`，而
邻近布局中的真正析构槽也不会被原版按预期调用。因此当前 `Controller.h` 只提供不占
虚表槽的同名兼容查询，不把它声明成 virtual。

这一段现在也有完整的基类尺寸链，而不再依赖导入 UDT。RVA `0x13F60` 的
`IVP_Actuator` 构造函数在 `this+0x04/+0x06/+0x08` 初始化 controlled-core vector；
RVA `0x14020` 的 `IVP_Actuator_Two_Point` 构造函数随即从 `this+0x0C` 开始构造两个
`0x30` 字节 anchor（第二个在 `+0x3C`），并在 `+0x6C` 写 `client_data`。Force、Torque、
Rotational Motor 和 Spring 的保留成员又从 `+0x70` 开始访问各自派生字段。因此可由原版
指令确认：`sizeof(IVP_Controller)==0x04`、`sizeof(IVP_Actuator)==0x0C`、
`sizeof(IVP_Actuator_Two_Point)==0x70`；相应的七槽、七槽和八槽 vtable 仍按各自原版表
处理。这三个 owner 现归入 `layout-retail`，不是仅凭源码或 IDB 类型升级。

此前 `IVP_Actuator` 构造已进入 RVA `0x13F60`，但析构仍由宿主默认生成，只是碰巧使用同一
allocator 清理 `+0x04` vector；这不满足“DLL 有函数体就用 DLL”的规则。现在 vector 以
匿名 union 保持原偏移并抑制宿主自动析构，`~IVP_Actuator` 直接进入原版 complete
destructor RVA `0x13FE0`，无 resolver 时才执行等价本地清理。原版体负责清空 vector 并把
vptr 恢复成 `IVP_Controller` 表；宿主随后进入无状态 Controller 基析构，不会二次释放。
精确 DLL 案例会创建具体的最小 actuator、让原版 vector 扩容并保存两个 core 指针，再经
原版完整析构释放，从而覆盖构造、访问、priority 和所有权边界。Clang 的参考声明探针把
基析构装饰为 `??_D...`，而 DLL 保留的是 MSVC complete destructor `??1...`，所以账本把
该函数体准确列作第六个 `binary-confirmed-body-variant`，而不是伪称装饰名完全相同。

## `IVP_Constraint` 与 `IVP_Constraint_Local`

邻近源码的 `IVP_Constraint` 基类在两个关联 core 指针和 core 向量之后还有
`client_data`，基类大小为 `0x1C`。Ballance 原版的 Local constraint 构造函数 RVA
`0x28270` 却直接在 `this+0x18` 写入 `force_factor`、在 `this+0x1C` 写入阻尼/力参数；
`IVP_Environment::create_constraint`（RVA `0x129C0`）固定分配 `0x190` 字节后调用该构造
函数。因此 Ballance 基类大小是 `0x18`，不存在这个 `client_data` 字段，保留它会使整个
Local 对象错开 4 字节。

原版已确认的 Local 关键布局为：

| 成员 | 偏移/尺寸 |
| --- | ---: |
| `force_factor` | `0x18` |
| 阻尼/力参数 | `0x1C` |
| 六个固定轴标志 | `0x20` |
| 两组 border | `0x38` / `0x50` |
| `stiffness` | `0x68` |
| max-impulse 指针 | `0x6C` |
| object R/A anchor | `0x70` / `0xF8`，每个 `0x88` |
| 三轴映射 | `0x180` / `0x183` |
| 轴计数 | `0x186`–`0x18A` |
| norm 位域 | `0x18C` |
| `sizeof(IVP_Constraint_Local)` | `0x190` |

VA `0x10063930` 的原版 Local vtable 共 23 槽：前 7 槽是 controller/constraint 生命周期与
模拟接口，后 16 槽对应 ballsocket、translation/rotation axis、limited axis、固定轴、
自由轴、border 和 friction 等公开修改操作。当前头文件让这些操作直接进入对应零售
函数，而不是复制另一修订的求解器实现。完整析构 RVA `0x288A0` 还证明三个可选
max-impulse 指针位于 `0x6C/0xF4/0x17C`，释放必须使用原 DLL 分配器。

独立的基表位于 VA `0x10063B20`，同样是 23 槽。构造 RVA `0x375B0` 会安装该表；slot 4
是 `_purecall`，而 slots 7–22 全部是具体缺省函数。四对 target/source 变体分别折叠为
同一 RVA，所以 16 个槽对应 `0x376D0`–`0x37780` 的 12 个代码体。当前基类据此只把
`do_simulation_controller` 留作纯虚；若把后 16 槽也声明成纯虚，Mod 派生类的 vtable
会与原版基类语义不符。

这两张表也纠正了一个明显的 IDB 导入误名：slot 2 的 RVA `0x28240` 只有
`lea eax,[ecx+8]; ret`，且 `IVP_Constraint` 基表与 Local 表都引用它，所以它是
`IVP_Constraint::get_associated_controlled_cores`，不是 `type_info::raw_name`。slot 1/5
的共享函数分别返回 minimum frequency 和 `IVP_CP_CONSTRAINTS`，slot 4 与后 16 槽则按
基类/Local 实现和参数清栈逐一确认。Fixed-Keyframed 没有任何保留 vtable，不能再据此
声称它引用 `0x28240`。共享删除回调 `0x28950` 没有硬归到 Local 或 Actuator，
而是使用中性分析名 `ivp_shared_delete_this_callback`；这样保留编译器合并事实，也能在
IDB 中检索所有已追踪目标。

此处也给出一个 IDA 类型不能孤立使用的具体例子：数据库曾把 RVA `0x288A0` 尾部调用的
基类析构标成 `IVP_Mindist_Base`，但调用链、vptr 和对象偏移都证明它是 constraint 基类
析构。当前定义采用这些联合证据，没有沿用该错误名称，也没有为未知部分新造 `*_Base`
类型。

## SurfaceBuilder ledge soup

邻近源码的 `IVP_Template_Surbuild_LedgeSoup` 在选项之前带有
`force_convex_hull` 指针。原版 RVA `0x384B0` 的 compile 路径只按 `0x10` 字节模板
读取四个 32 位字段；它的缺省模板初始化路径也只写偏移 `0x00`、`0x04`、`0x08`、
`0x0C`。因此原版布局从 `build_root_convex_hull` 开始，不能保留该指针作占位。

`IVP_SurfaceBuilder_Ledge_Soup` 现在不再保留 opaque storage。原版 RVA `0x38290`
构造函数清零完整 `0xA0`，并初始化 `+0x20/+0x28/+0x30/+0x64/+0x6C/+0x94` 六个
vector；RVA `0x38340` 对同一组 vector 释放并清空。compile 及内部 builder 的字段访问与
邻近公开头给出的 compact surface、sphere 计数、`smallest_radius +0x10`、两个 extent、
hash、工作 ledgetree 和 all-spheres 字段逐项吻合。因此公开头恢复完整强类型成员，仍把
construct、destruct、insert、compile 全部送回原 DLL；x86 断言锁定大小、对齐及八个关键
偏移。`IvpRetailPointsoupBuilderTest` 使用指定 Ballance 原 DLL 构造 3×4 face 几何并通过。

规范 IDB 也同步重建为 `0xA0` UDT：`smallest_radius` 明确为 binary64 `double`，并显式
记录 `+0x0C/+0x9C` padding。IDA 的 C parser 不能重新解析其已有的 `<...>` 模板类型名，
所以 IDB 内六个 vector 子对象用等价的 0x08 `IVP_U_Vector_Base` 物理布局表示，注释保留
真实元素类型；C++ 公开头仍使用精确的 `IVP_U_Vector<IVP_Compact_Ledge>` 与
`IVP_U_Vector<IVV_Sphere>`，没有向用户暴露 base façade。

## Polygon transport 模板

此前接口遗漏了 `IVP_Template_Point`、`IVP_Template_Line`、`IVP_Template_Surface` 和
`IVP_Template_Polygon`，但它们并非只存在于邻近源码。原 DLL 的 Polygon 构造 RVA
`0x3BF80` 清零六个 32 位 count/pointer 字段，析构 RVA `0x3BFA0` 删除 line、point，
并按 `0x30` 步长析构 surface 数组；因此 Polygon 的 Ballance 大小明确是 `0x18`。
Surface 构造 RVA `0x3C000` 用 `rep stosd` 清零十二个 dword，`close_surface` RVA
`0x3C020` 操作 `lines +0x28` 与 `revert_line +0x2C`，`get_surface_index` RVA `0x3C050`
从 `templ_poly +0x20` 取得 Polygon，再以 `0x30` 步长计算共享 surface 数组下标。

当前 `Templates.h` 恢复这四个真实类型：Point 是不增加字段的 `IVP_U_Point`，Line 是
两个 16 位点下标，Surface 为 `0x30`，Polygon 为 `0x18`。上述五个保留实体全部直接
路由原 DLL。exact-DLL 测试以四点、六共享边、四面的 tetra transport 验证构造清零、
共享拓扑及四个 surface index；测试结束前解除栈上借用数组，避免违反 Polygon 析构的
heap-owner 约定。带计数的分配构造、`init_surface`、normal 计算和 `scale` 尚无零售实体，
本轮没有为它们猜入口或跨 CRT 所有权。

这次还纠正了 IDB 的反例：`0x15CB0/0x15D20` 旧名都声称是 Surface 析构，实际目标分别
安装 VA `0x10063718/0x10063720` 的 Active Float/Active Int vtable，再进入 Active Value
基析构。它们现已改成正确的 Active Float/Int complete destructor；Surface 的公开析构
则按确切所有权只调用 `close_surface`，不依赖这两个错误导入名。

## Vector 枚举器

邻近 `ivu_vector.hxx` 和 `ivu_bigvector.hxx` 的枚举器实现把 `index` 初始化为 `0`，
`get_next_element()` 返回该位置后执行 `index--`，但负下标检查被注释掉。第二次调用因而
访问 `element_at(-1)`；同一处注释又保留了原本的 `vec->n_elems - 1` 初始化，说明这不是
可依赖的 API 语义。

当前兼容层没有照抄这个明显损坏的内联体，而是按容器中其他倒序遍历及 listener 删除
安全语义，从 `len() - 1` 遍历到 `0`。构造函数和 `get_next_element` 的公开签名保持
一致，枚举器仍为单个 32 位下标、大小 `0x04`。这一项是“参考源码有偏差”的实例，
不应反过来当成 Ballance 二进制必须复现的越界行为。

## Synapse、摩擦接触点与 contact API

邻近源码中 `IVP_Synapse` 和 `IVP_Synapse_Friction` 的“32 字节”注释不能用于 Ballance。
原版 mindist 构造/更新路径把两个 real synapse 嵌在 `+0x18` 与 `+0x34`，因此步长明确为
`0x1C`；原版 friction 链则以 `0x14` 为单元，并在 `+0x0C` 读取 contact-point 相对偏移。
当前 `Mindist.h` 据此恢复 next/prev、object、edge、status、mindist/contact backlink 与
hull 回调，未照搬错误尺寸。

`IVP_Collision` 的公共前缀也已独立拆出证据。Mindist Base 构造 RVA `0x160E0` 在继承的
`IVP_Time_Event` vptr/index 后，把 delegator 写到 `+0x08`，再把两个 FVector 索引
`+0x0C/+0x10` 初始化为 `-1`，因此完整大小为 `0x14`。exact-DLL FVector 案例现在使用
真正的 `IVP_Collision` 派生对象，验证同一碰撞同时进入 active/delegated 两个集合时，
补位、删除和交换只修改匹配的索引槽。IDB 中 Mindist Base 和 Mindist 两个构造器原有的
整数 `this`/参数类型也已按装饰名和调用链修正。

进一步核对原版 RVA `0x160E0`/`0x16290` 后，`IVP_Mindist_Base` 的两个 synapse 位于
`0x18/0x34`，contact plane 位于 `0x68`，总大小为 `0x78`；派生 `IVP_Mindist` 的
recalc timestamp、next、prev 和 last-visited triangle 位于 `0x78/0x7C/0x80/0x84`，
总大小为 `0x88`。`IVP_Mindist_Manager` 的原版完整构造 RVA `0x186E0` 清零 `0x18`
字节并在 `0x04` 写入 environment，exact list、wheel look-ahead vector、invalid list
分别位于 `0x08/0x0C/0x14`。紧邻的匿名 RVA `0x18710` 是完整析构：先沿两个 list 的
`next +0x7C` 遍历，通过每个 Mindist 零售 vtable 的 scalar-deleting destructor 删除，
再由原版 `free` 释放 wheel-lookahead vector。Environment 析构在 `+0x10` 取出 manager
后正是先调用 `0x18710`、再单独调用原版 `operator delete`。当前接口因此恢复了公开
构造/析构，vector 改为 inactive union storage，外层 `new/delete` 固定走零售堆；既不
预构造成员，也不把完整析构误当 deleting destructor。

其中若干函数在 IDA 中没有可靠的源码名，但指令仍足以恢复身份：RVA `0x16E30` 是
`insert_exact_mindist`，`0x17140` 是 `recheck_ov_element`，`0x17790` 是 wheel mindist
批量重算，`0x183C0` 是双 hull-time 的 `insert_hull_mindist`，`0x187A0` 是单个 exact
mindist 重算；RVA `0x197F0/0x19950` 则分别是 `recalc_invalid_mindist/recalc_mindist`。
这些判断来自调用者、参数清栈、字段偏移和邻近算法的共同吻合，不是仅凭导入类型命名。

这个差异还会移动 `IVP_Contact_Point` 的后续字段。按两个 `0x14` friction synapse
重新计算，并用 IDA 反汇编中对 `this+0x40` 的临时 contact-info 读取交叉验证后，原版
`integrated_destroyed_energy` 与 `now_friction_pressure` 分别位于 `0x48` 和 `0x54`。
`IVP_Contact_Point_API` 仍只公开源码原有的四个受保护访问器。完整 contact-point 随后按
零售访问拆出命名字段，不再使用 `std::byte[0x78]`：链指针 `+0x00/+0x04`、两个 synapse
`+0x08/+0x1C`、无方向逆虚质量和 two-value bitfield `+0x30/+0x34`、friction spans
`+0x38`、long-term 指针 `+0x40`、五个 friction/energy scalar `+0x44..+0x54`、gap/keeper/
break-state/negative-pull `+0x58/+0x5C/+0x60/+0x64`、recalc time `+0x68`、system backlink
`+0x70`。这些字段保持原源码的 private 边界；不是把求解器内部状态改成任意 Mod 可写的
公共接口。私有 complete constructor/destructor 分别接入 RVA `0x1A8B0/0x1C230`，确保
friend 路径不再遇到被删除的伪生命周期函数。

在此布局基础上，又接入八个有独立零售函数体的 contact 内部成员：双 friction value
投影 RVA `0x1AA20`、real-friction length 更新 `0x1AD90`、单接触静摩擦
`0x1AE40`、friction easing `0x1BD90`、旋转不确定度 `0x248A0`、impact rescue speed
`0x24930`、collision-distance predictor `0x249B0` 和 timestamp reset `0x1F850`。
它们保持邻近类中的 private 可见性，只供已有 friend 引擎路径使用；接口通过零售函数体，
没有搬入算法。

同一真实类中还补回了 contact 身份判断 RVA `0x1D910`，以及 point-face、point-point、
point-edge、edge-edge 四条接触几何计算路径 RVA `0x1EE70/0x1F050/0x1F160/0x1F3B0`。
已有合约包装的 virtual-mass 计算和 material 读取也改为在 `IVP_Contact_Point` 本体声明，
避免只在地址表里存在却无法由正确的 friend 路径调用。这七项都保留原类的 private 边界；
没有为了表面可用性添加伪 public bridge，也没有复制邻近版本依赖 cache/compact geometry 的
算法。该阶段 IDB 脚本为 457 项 guarded corrections，Retail Contract 为 587 个
code RVA；后续生命周期审计继续扩展到当前 879 项校正脚本、685 个 code RVA 和 1600 个
命名函数，旧阶段数字不再代表当前覆盖。

邻近源码把 `friction_force_local_constraint_2d_wheel` 写成单独的 private 方法，但 Ballance
零售链接结果没有这个函数边界。RVA `0x1B080` 的二维摩擦函数在 `0x1B117` 直接进入轮胎
专用路径：检查 `contact_core[0]` 的 car-wheel、要求 `contact_core[1] == nullptr`，变换车轴，
更新 skid 状态并施加二维冲量；拒绝该路径后才在 `0x1B713` 进入通用双 core 求解。因此当前
接口只包装真实的 `friction_force_local_constraint_2d`，它已包含轮胎语义；不会虚构一个可跳转
的 wheel helper RVA。邻近头中只有声明、在邻近实现和零售镜像都没有独立函数体的
`leave_friction_mode`、distance-keeper 更新和旧 contact-side impact 三角函数，同样不伪装成
可调用入口。

### 完整构造器不能从 C++ 构造函数体二次进入

早期公开审计中有 7 个精确零售函数体没有直接调用自己的 Address ID：
`IVP_Actuator_Two_Point`、`IVP_Constraint_Local_Anchor`、`IVP_U_Active_Value`、
`IVP_U_Active_Float`、`IVP_U_Active_Int`、`IVP_U_Active_Terminal_Double` 和
`IVP_U_Active_Terminal_Int` 的完整构造器。这不是遗漏 DLL 包装。C++ 在进入派生构造函数体前
已经自动构造基类；此时再调用原版完整构造器会重复构造基类、重复分配 active-value 名称，
或重复初始化 actuator 的受控 core vector。当时的接口因此只调用可独立进入的零售基类、
anchor、字符串分配等子入口，再按零售字段访问重建该层一次。后续 storage-only 子对象改造
消除了这个限制：完整构造器现在从未被宿主预构造的存储进入 DLL，已不存在以本地分层实现
代替具名完整构造体的情况。`Audit-IvpPublicInterface.py` 当前报告 397 项直接零售路由，
其余 12 项均为有证据的分层析构或 thunk，合计 409/409；任何新增的
`missing-retail-route` 都会成为明确的审计失败候选。

`IVP_Constraint_Local_Anchor` 还暴露出“完整构造器”不能按字段直觉补写的另一处差异。
RVA `0x28210` 只有一条成员写入：把 `rot +0x84` 清零；它不会清零
`object +0x80`。外层 RVA `0x28270` 分别对 `this+0x70` 和 `this+0xF8` 调用该层，随后才
把两个 object 写到外层 `+0xF0/+0x178`。当前公开构造器因此同样只初始化 `rot`，不再把
`object` 猜成空指针。exact-DLL 几何案例用预置 object sentinel 调用原构造层，确认 sentinel
保持不变而 rot 被清零；这同时确认了 `IVP_U_Matrix 0x80 + object +0x80 + rot +0x84`
组成的 `0x88` 布局。IDB 原先的 `int __thiscall(_DWORD)` 也已修正为返回 self 的真实
`IVP_Constraint_Local_Anchor *__thiscall` 原型。

`IVP_Friction_System` 则不能继续只以前置声明和 `void *` 暴露。原版在 contact 创建路径
明确分配 `0x50` 字节；构造函数把 environment 写入 `+0x04`，两个嵌入 controller handle
位于 `+0x08/+0x10`，contact 链头位于 `+0x20`，三个 `IVP_U_Vector` 位于
`+0x24/+0x2C/+0x34`，三个 16 位计数位于 `+0x3C/+0x3E/+0x40`，union-find 标志从
`+0x44` 开始，能量累计值位于 `+0x48`。主虚表 VA `0x100633D0` 仍是 Ballance 的 7 槽
controller ABI。当前 `Friction.h` 因而公开真实的 `IVP_Friction_System` owner、`0x50`
尺寸、类型化查询以及创建/迁移/删除 contact 所需的保留入口；对象仍由原版 contact manager
拥有，不能用 Mod CRT 任意构造或释放。

RVA `0xB360` 此前在 IDB 中只是 `sub_1000B360`。它接收 ECX 中的 system，把 contact 的
system/next/prev 写到 `+0x70/+0x00/+0x04`，更新 system 的链头 `+0x20`，并递增
`+0x3E` 的 16 位计数；`try_to_generate_managed_friction` 又在新 contact 分支直接调用它。
这些证据与邻近 `add_dist_to_system` 算法一致，因此现已安全命名并加入校正脚本。这里也再次
证明 Ballance contact 的 system backlink 是 `+0x70`，不能采用导入源码类型因编译配置而
给出的 `+0x80`。

`IVP_Real_Object::unlink_contact_points_for_object` 在零售 DLL 中被内联，但它依赖的
私有全量解绑 helper 保留在 RVA `0x9A40`。该 helper 的指令确认 contact-point 中两个
对象指针位于 `+0x10/+0x24`、friction-system 指针位于 `+0x70`，system 的
friction-distance 数量是 `+0x3E` 的 16 位值。选择性解绑因而只遍历本对象的 friction
synapse，删除确实涉及目标对象的 distance；共享 system 仍有其他 distance 时保留，计数
归零时才经 VA `0x100633D0` 第 6 槽对应的 RVA `0xB0D0` scalar-deleting destructor
销毁。测试同时覆盖“移除旧平台接触但保留同一 system 的地面接触”和“最后一个接触被
移除后销毁空 system”，防止把全量清除误实现成选择性 API。

## Radar 回调 ABI

`IVP_Radar_Hit` 的两个对象指针位于 `+0x00/+0x04`，距离位于 `+0x08`，x86 大小为
`0x10`。`IVP_Radar` 虽然只有一个 vptr 和两个 `IVP_DOUBLE` 数据成员，但 Ballance 使用
MSVC x86 的 8 字节成员对齐：vptr 后有 4 字节填充，`max_range` 和
`max_relative_error` 位于 `+0x08/+0x10`，总大小是 `0x18`，不是按直觉紧排得到的
`0x14`。

`IVP_Real_Object::do_radar_checking` 本体同样被内联。当前重建先遍历 exact synapse 并按
range 过滤，再遍历 hull-manager 的 min-list；hull 候选先调用原版
`IVP_Mindist::recalc_mindist`，只报告有效结果。当当前对象位于 mindist 的第二个 synapse
时，回调仍会把当前对象放入 `this_object`，而不会照内部存储顺序颠倒语义。对应测试覆盖
近距离 exact 命中、超距过滤、有效/无效 hull 候选和这个方向约束。

## Universe manager 回调

大型世界流式加载接口没有从邻近版本整块移入。RVA `0xA9F0` 的原版
`IVP_Cluster_Manager::check_for_unused_objects` 先调用 `IVP_Universe_Manager` slot 3
取得四个 32 位阈值，再用 slot 1 通知不再需要的对象；RVA `0x17140` 在 environment
`+0x2C` 取得同一 manager，并用 slot 0 请求给定球形邻域内的对象。结合删除路径的
slot 2，可确认 Ballance 的完整四槽顺序是 ensure、no-longer-needed、object-deleted、
provide-settings，且没有虚析构槽。

当前 `Universe.h` 按这个顺序提供可继承回调和大小 `0x10` 的 settings，默认阈值仍为
`1/1/1000000/10`。对象由安装它的 Mod 持有，不能经基类指针删除；manager 可在创建环境
前放入 `IVP_Application_Environment::universe_manager`。修正脚本同时对上述 slot 调用
指令做版本保护，避免把邻近修订的回调表假定成 Ballance ABI。

## 立即停止模拟

`IVP_Real_Object::disable_simulation` 在 Ballance 中没有独立函数体，不能简化成只把速度
清零。原版相关路径表明 core flags 的 physical-unmoveable 状态占 bit 2–3，wakeup-vector
状态占 bit 4–5；RVA `0xB4D0` 会从 environment 的 revive vector 移除 core 并清除后者。
对仍在移动的共享 core，兼容实现按对象 vector 的逆序为每个附着对象调用 RVA `0x9A40`
的静默全接触解绑，把 next-PSI 四元数和 last-PSI 位置复制到 calm-reference，并用当前
时间减 20 初始化 calm 时间；最后依次调用 RVA `0x120D0` 的 union-find 和 RVA
`0x120F0` 的 movement-state 计算。RVA `0xAFE0`、`0xD680` 和 `0x120B0` 的冻结、core
状态计算与 movement-check 清理函数也已在 IDB 中按实际调用约定命名。

测试使用两个对象共享一个 core，确认两者的接触均被静默解绑、待唤醒项只移除一次、
calm reference 完整播种且 simulation unit 重新计算；另验证静态 core 直接成功且不触发
任何状态变更。这是冻结状态迁移测试，不是只看返回值的 smoke test。

## Anomaly limits 与 penetration 回调

原版 `IVP_Anomaly_Limits` 构造函数 RVA `0x2F5E0` 写入 vptr、删除标志、最大速度、每 PSI
最大碰撞数和最大角速度，最后一个字段位于 `0x10`，总大小为 `0x14`。邻近源码后续加入
的 `max_collision_checks_per_psi`、最小/最大 friction mass 不属于该对象修订，当前接口
因此不提供对应 getter。

邻近源码的 `inter_penetration` 还有第四个 `IVP_DOUBLE` 参数；原版装饰名只编码
`mindist/object0/object1`，RVA `0x2F8F0` 以 `ret 0x0C` 返回。当前接口故意采用三个参数，
这是方法级审计中两个零售确认签名变体之一；另一个是 Car System 的单参数转向槽。

两张原版表还确定了析构位置：VA `0x10063A50` 的 Limits 表是 environment callback、
deleting destructor；VA `0x10063A58` 的 Manager 表是六个业务 callback 后接 deleting
destructor。此前当前头文件把两个析构都提前到槽 0，尺寸虽不变但动态调用完全不兼容；
现已按原版末槽顺序修正。IDB 中这两条完整析构曾被错误归为 `IVP_Triangle`，相应 owner、
原型和 vtable 名也已写回分析副本。

新增精确 DLL 的异常限幅案例后，构造默认值也由运行结果固定为：线速度 `2000.0f`、每
PSI 最多 `70000` 次碰撞、每 PSI 角速度 `pi/2`。RVA `0x2F6D0` 将超限线速度缩放至
配置上限的 `99%`；RVA `0x2F720` 则先用 `IVP_Core +0x0C` 的 environment 读取
`inv_delta_PSI_time`，再缩放至每 PSI 角速度换算值的 `90%`。测试以 `3-4-5` 线速度和
60 rad/s 角速度验证结果分别为 `(1.188, 1.584, 0)` 与 `(0, 0, 45)`，同时确认
RVA `0x2FCC0` 返回 32 位 `IVP_TRUE`。这不是把两个保护阈值混成同一个量，也确认了
manager 回调的 core 参数只在角速度路径中参与计算。

## Material manager 的组合精度与默认材质

RVA `0xBD10/0xBD40/0xBD70` 分别从 `IVP_Contact_Situation +0x50/+0x54` 读取两侧
材质，摩擦和弹性相乘，黏着相加。真实 DLL 案例用两张 `IVP_Material_Simple` 虚表确认
结果为 `0.36f`、`0.072f` 和 `0.5`。前两项虽然按装饰名 `N` 和 x87 ABI 返回 8 字节
`double`，乘法在 Ballance 当前 x87 控制字下产生单精度舍入，再扩展为 double；这不能
被误解成 `IVP_DOUBLE` 是 float。两侧材质 getter 单独返回的 `0.72/0.18/0.5/0.4`
仍保持各自的 64 位存储值。

RVA `0xBE30` 忽略 world position 和 material index，第一次调用时用原版 allocator 创建
摩擦/弹性均为 `0.5` 的 `0x30` 字节进程级默认材质，以后返回同一指针。构造函数本体的
零售装饰名是 protected `IVP_Material_Manager(unsigned int)`，而邻近公开声明是 public
`IVP_Material_Manager(IVP_BOOL)`；二者在 MSVC x86 上都是一个 4 字节参数，包装保持公开
签名但差异记录不抹除。非 owning manager 的 environment 删除回调是 no-op，测试确认
回调后仍能继续组合接触材质；owning 分支会删除 receiver，不能用栈对象试验。

## Fast inverse square root

`IVP_Inline_Math::isqrt_float/isqrt_double` 在 Ballance 中分别保留于 RVA
`0xDAE0/0xDB80`。它们不是 CRT `1/sqrt` 的别名：原版从 IEEE-754 double 指数构造
倒平方根初值，再以固定 Newton 序列修正；float 入口先把 32 位输入装入 x87，double
入口直接读取 8 字节输入。装饰名的 `M/N`、栈宽和 ST0 返回共同确认结果分别为
`IVP_FLOAT`/`IVP_DOUBLE`，旧 IDB 的 `int __cdecl` 原型错误。

精确 DLL 测试用 float `25` 归一化 `(3,4,0)`，并用非完全平方 double `7.25` 检查
`q * isqrt(q)^2` 接近 `1`。在 Ballance 当前 x87 单精度控制字下，double 入口结果精确
等于 `1.0f / sqrtf(7.25f)` 再扩展为 double；它仍是 8 字节参数/返回 ABI，不能因为
运算精度被控制字限制就把 `IVP_DOUBLE` 改成 float。这对应碰撞法线和摩擦方向的实际
用途，也能发现误接普通 sqrt、float/double 地址互换或整数返回 ABI。

## IDA 名称需要与对象布局共同判断

IDA 数据库中 RVA `0x46A0/0x46B0/0x4760/0x4C50` 曾被标成
`IVP_Actuator_Force` 方法，但指令显示对象在 `+0x08` 直接保存 core、在 `+0x10` 保存
world point，且 `get_associated_controlled_cores` 返回固定全局向量；这与邻近
`IVP_Actuator_Force : IVP_Actuator_Two_Point` 的 anchor/base 布局不相容。它们实际属于
`physics_RT` 适配层的 force controller，不能拿来充当公共 IVP actuator 的实现。
因此这里既没有全盘否定 IDA 类型，也没有照抄名称：函数指令和对象布局冲突时保持待判定。

## `_Base` 类型名不是兼容层占位符

兼容层不再为“不知道完整类型”人为制造新的 `*_Base` 替代类型。公开声明中的这种名字
只在它本来就是 IVP 类型、并且有 Ballance 零售 ABI 或邻近源码结构证据时保留。例如
`IVP_Mindist_Base`、`IVP_Hull_Manager_Base` 和 `IVP_U_Vector_Base` 都出现在原版 DLL 的
MSVC 装饰名中；phantom listener 的回调参数也直接编码
`IVP_Mindist_Base *`。把它们重命名成派生类、`void *` 或兼容层自造名称都会改变装饰名、
虚表签名或对象布局，因而不是兼容的“简化”。

邻近 SDK 的图形示例类 `IVP_Example_Base` 不属于 Ballance `physics_RT.dll` 的运行时 IVP
接口，现已从公开成员方法候选审计中排除。这个规则的边界是“不新增占位 Base 类型”，
不是删掉零售 ABI 已经证明存在的原生 IVP 类。

## Environment 配置默认值

`IVP_Application_Environment` 构造 RVA `0x15E70` 先清零完整 `0x30` 字节，再把首个
`n_cache_object` 写成 `0x100`。因此默认缓存数是 256，不是 0；其余 scratchpad 和
可替换 manager 指针均为 null。公开 wrapper 不再用
C++ 字段初始化器预清零：解析到原版时完整 `0x30` 写集只由 DLL 拥有，无解析器时才执行
等价本地回退。精确 DLL 测试从 `0xA5` poison storage 构造并逐字段验证启动配置，防止
调用原版前的宿主预写再次掩盖错误入口。

同类问题也存在于 `IVP_Template_Phantom`：RVA `0x10880` 依次把 `+0x00/+0x04/+0x08`
清零，再把 `+0x0C/+0x10` 写成 `0.5f`，完整覆盖 `0x14`。五个公开字段不再携带 C++
默认初始化器；DLL 可用时由该 complete body 独占写入，无解析器时才执行等价回退。
精确 DLL 案例同样从 `0xA5` storage 开始，避免字段默认值把漏调用伪装成正确结果。

`IVP_Environment::set_delta_PSI_time`（RVA `0x13240`）把 8 字节步长原样保存到
`+0xC0`，并在 `+0xC8` 保存 double 倒数；旧 IDB 的 `__int64(double *this, double)`
原型与装饰名 `QAEXN`、`ret 8` 和字段写入均冲突，现改为 void thiscall。
`set_gravity`（RVA `0x13680`）复制 `IVP_U_Point` 的 double 分量到 `+0xD0`，把长度以
float 缓存在 `+0xF0`，还会调用标准重力控制器同步其 float 向量。测试使用非轴对齐重力
同时检查三份状态，避免把它错误重建成只写 environment 字段的 setter。

全局 collision tolerance 还有一个容易被后期源码误导的版本差异。公开
`IVP_Environment::set_global_collision_tolerance(tolerance, gravity_length)` 仍保留两个
double 参数以维持源码接口，但 Ballance 实际调用的 RVA `0x15FE0` 是
`IVP_Mindist_Settings::set_collision_tolerance(double)`，只接收 tolerance；第二个
`gravity_length` 不参与计算。原版测试用相同 tolerance、两个相差 250 倍的 gravity length
重复配置，确认 `get_global_collision_tolerance()` 始终从零售全局设置的 `+0x04`
`min_coll_dists` 读回同一 float 阈值，并在结束时恢复进程全局状态。

Environment 的全局 object-listener 容器由 RVA `0x13A80` 直接追加，四类事件分发按注册
顺序逆向执行。旧 IDB 把 RVA `0x13650` 标成 `fire_event_object_revived`，但函数实际读取
Environment `+0x158` 的 `collision_delegator_roots`，而不是 `+0x150` 的
`global_object_listeners`，并调用 Root vtable 第 2 槽。它因此只能是
`fire_object_is_removed_from_collision_detection(IVP_Real_Object *)`；名称、参数和返回类型
已一起修正。

后续逐条检查相邻匿名函数后，确认 object event 并非都被链接裁掉：RVA
`0x13AC0/0x13AF0/0x13B20/0x13B50` 分别是 created、deleted、frozen、revived dispatcher。
四个函数都读取 `environment+0x152` 的 listener 数量和 `+0x154` 的指针，按倒序调用
`IVP_Listener_Object` 槽 `1/0/3/2`，并以 `ret 4` 结束。当前公开方法因此不再把这四项归为
纯本地重建：解析到受版本锁保护的 DLL 时直接进入原函数，仅在 resolver-free host 测试中
执行等价回退。精确 DLL 场景验证四种事件的顺序、原 event 指针以及 revived 回调内自注销
后同轮较早 listener 仍被调用；collision-root 场景则继续单独验证 RVA `0x13650`。

这些调用链也把 callback payload 从“源码/IDA 看起来合理”提升为零售布局证据。
`IVP_Event_Object` 由 Real Object 析构 `0x9830` 和 Core freeze/revive `0xAF90/0xAEA0`
写出 `environment +0x00`、`real_object +0x04`；collision caller `0x23CD0` 写出
`IVP_Event_Collision` 的 float 时间 `+0x00`、environment `+0x04`、contact `+0x08`；
friction 创建/删除 `0x22180/0x1C230` 写出 `IVP_Event_Friction` 的三个指针
`+0x00/+0x04/+0x08`。Building Block callback `0x42B0`、Contact Point material fill
`0x24040` 和 material manager 读取链共同固定 `IVP_Contact_Situation` 为 `0x58`：normal、
speed、world point、objects、edges、materials 依次位于 `+0x00/+0x10/+0x20/+0x40/+0x48/+0x50`。
`IVP_Event_Sim` 的两个 binary64 与两个指针也由 controller 读取固定为
`+0x00/+0x08/+0x10/+0x14`。RVA `0x13C40` 则只在栈上构造一个 environment 指针的
`IVP_Event_PSI`；旧 IDB 把私有 `fire_event_PSI()` 误标成 `int`，现按无定义 EAX 的函数体
修为 `void __thiscall`。`IVP_Contact_Point_API` 本身只是无状态 static accessor 容器，不会有
helper 实例跨 DLL，故单独记为 layout-not-applicable。

同一轮复核否定了此前照搬的 constraint-listener 路径。零售 Environment 构造函数只在
`+0xF4/+0xFC/+0x104` 初始化 collision、PSI 与 Core-revival 三个 vector，随后直接把
`p_strdup` 返回值、授权码、计数 `10` 和 manager 分别写到
`+0x10C/+0x110/+0x114/+0x118`；析构又把 `+0x10C` 交给原版 `operator delete`，并通过
`+0x118` 从 manager 的环境列表解绑。DLL 中没有任何对第四个 vector 的访问。
`IVP_Constraint_Local::do_simulation_controller` 的两个 BREAK 出口
`0x2A07A/0x2A09C` 也不是通知循环：它们以参数 `1` 调用 vtable 字节偏移 `+0x18`，即槽 6
的 scalar deleting destructor `0x28250`。因此 `add/remove_listener_constraint_global` 与
`fire_event_constraint_broken` 现明确 `= delete`；保留纯回调类型只用于源码表面完整性，
不宣称有 Ballance 跨 DLL 实例 ABI。

该调用还纠正了另一个版本断层：邻近 Source-era `IVP_Collision_Delegator` 在析构后新增
`change_spawned_mindist_count` 与 `get_spawned_mindist_count` 两个虚槽；若直接搬入，Root 的
object-removal 会从槽 2 移到槽 4，与 RVA `0x13650` 的机器码冲突。当前 API 保留这两个
默认方法供源码调用，但将其设为非虚，维持 Ballance 的两槽基表与 Root ABI。

本轮又直接读取原 DLL 的相邻指针并追构造函数 xref，排除了“`0x10063A34` 起始的一张
七槽表”这一错误解释。`IVP_Collision_Delegator` 构造/析构阶段只安装 `0x10063A34`
的 pure callback 与 deleting destructor 两槽；Root Mindist 构造 RVA `0x2F5A0` 另行安装
`0x10063A3C`，后者五槽依次为 collision deleted、deleting destructor、object removed、
delegate 和 environment deleted。主 IDB 的 `IVP_Collision_Delegator_vtbl` 已由错误的
`0x10` 改为 `0x08`，Root/Root Mindist vtable UDT 均由 `0x1C` 改为 `0x14`；此前匿名的
RVA `0x2F5C0` 也确认为 Root Mindist scalar deleting destructor。

这不再只是 IDB 注释：公开的 `IVP_Collision_Delegator_Root_Mindist` 构造函数直达
RVA `0x2F5A0`，四个行为方法分别直达 `0x2F3B0/0x2F3E0/0x2F3F0/0x2F430`。该类没有
可从邻近版本搬入的额外状态，大小固定为 `0x04`。由于环境删除会经五槽表的 deleting
destructor 回到 `physics_RT.dll`，它也使用零售分配器；精确 DLL 测试实际完成
allocate → retail vptr → environment callback → DLL delete，而不是只验证可编译。

Environment 的其余保留工厂入口也已逐条回到机器码核对，而不是沿用 IDB 导入类型。
`get_root_cluster`（RVA `0x137D0`）从 `environment+0x0C` 取 cluster manager，再尾调用
只读取 manager 首字段的 getter；返回值确定为 `IVP_Cluster *`，不是旧 IDB 的 `int`。
`create_polygon`（RVA `0x139A0`）分配 `0xB8` 字节，把 root cluster 和四个调用者参数
原序转交给 `IVP_Polygon` 构造器；其返回值也由 `int` 修正为 `IVP_Polygon *`。
`create_ball` 同样分配 `0xB8` 字节并转交四个参数。`create_constraint` 在 template 的
`objectR+0x04` 与 `objectA+0xF4` 均为空时返回 null，否则固定分配 `0x190` 字节构造
`IVP_Constraint_Local`；这与 Ballance ball-constraint 场景所验证的 endpoint/core
membership 和实际约束效果一致。

两个全局 listener 追加入口也曾被 IDB 类型误导：RVA `0x13A80/0x13C70` 都返回 `void`，
分别操作 `environment+0x150/+0xF4` 的向量，并不返回刚加入的指针。私有 PSI 主体
RVA `0x13CB0` 的旧类型把参数拆成两个 `int`；装饰名、`ret 8` 和邻近实现共同确认它是
一个 by-value `IVP_Time`。这些更正已写回主 IDB；公开包装仍只暴露原本公开的环境方法。

同一轮还清理了八个“已有精确 DLL 路由、IDB 却只保留简写或 `unknown_libname`”的入口。
Environment 的 `create_spring` 会根据 template `+0x28..+0x34` 四个 active-value 指针选择
`0x98` 字节 passive 或 `0xAC` 字节 active 实现；`simulate_dtime` 把 double 增量加到
`current_time+0x120` 后以完整 `IVP_Time` 进入 event loop。Real Object 的
`async_push_object_ws` 明确完成 world-to-core 位置/冲量变换后调用 core impulse，
`remove_listener_collision` 虽然尾调用的内部 helper 会在 EAX 留下值，公开装饰名和接口
都确认返回 `void`，不能据寄存器残值改成 `short`。

Utility 一侧，RVA `0xB600` 是 `IVP_U_Vector_Base::increment_mem`，按 `2*n+1` 扩容并
区分 `this+0x08` 的内联存储；RVA `0xE030` 的 float-point `fast_normize` 以 EAX 返回
`IVP_RETURN_TYPE`，RVA `0xE2C0` 是双指针、`ret 8` 的 float 叉积，RVA `0xE480` 则从
x87 返回三项平方和的平方根 `IVP_DOUBLE`。这些入口现在都有精确装饰名和类型。相反，
RVA `0xBDA0` 的 Material Manager 实体仍保持 Ballance 的 protected `unsigned int`
构造器身份；公开 `IVP_BOOL` 包装虽然参数宽度兼容，也没有被伪装成另一个原版符号。

RVA `0x2F780` 与 `0x18A40` 则是两个真正缺名的本体，而不是版本别名。前者接收两个
`IVP_Real_Object *`，计算中心方向并向可移动双方施加相反冲量和角速度修正，现恢复为
`IVP_Anomaly_Manager::solve_inter_penetration_simple`；后者把 environment time code 写入
`IVP_Cache_Object+0x00`，再从 `object+0x08` 更新插值后的旋转、位置与对象矩阵，现恢复为
`IVP_Cache_Object::update_cache_object`。两者都由 `ret`、字段访问和邻近实现共同约束。

Constraint 的四个 default diagnostic 是另一种情况。Ballance 链接器分别把
`change_target_fixing_point_Ros`、`change_target_translation_axes_Ros`、
`change_target_rotation_axes_Ros` 和 `change_Ros_to_relaxe_constraint` 折叠到同签名的
非 target/Aos 诊断体 RVA `0x376D0/0x376E0/0x37730/0x37780`。一个 RVA 在 IDB 中只能有
一个主名称，因此不伪造第二个符号；Retail Contract 将这四项登记为
`retail-body-variant`，公开方法继续调用共享的原版机器码。

析构函数审计还需要区分 Clang AST 与 MSVC 镜像的名称表示。Clang 对声明输出
`??_D<Class>@@QAEXXZ` 伪名称，而 Ballance DLL 的 complete destructor 正确使用
`??1<Class>@@QAE@XZ` 或 `??1<Class>@@UAE@XZ`。审计器现在只对同 owner、零参数析构形状
做这一受限映射，不会把 scalar deleting destructor 或其他类的折叠体混入。
其中 12 项公开析构直接进入自己的 DLL 地址；`IVP_Template_Object` 使用 RVA `0x15F60`
的一跳零售 thunk 进入 RVA `0x15EA0` 的 complete body。

另外 12 个派生/抽象层不能从 C++ 析构函数体再次调用 complete body：Spring、Active
Spring、Two Point Actuator、Cluster、Constraint、Constraint Local、Material、Material
Simple、Surface Manager、Active Value、Active Float 和 Active Int。当前实现只复现该层实际拥有的 dependency
注销、child/allocation 清理或 physics CRT name 释放，再让 C++ 恰好一次进入成员与基类；
Retail Contract 对每个类型分别登记 `retail-destructor-reconstruction`，没有使用全局豁免。

## Time manager 与可变步长

Ballance 原版保留了 `IVP_Time_Manager::event_loop`（RVA `0x2F250`）和
`IVP_Event_Manager::simulate_variable_time_step`（RVA `0x2F010`），但没有单独保留
邻近源码中很薄的 `IVP_Time_Manager::simulate_variable_time_step` 包装体。当前兼容层
按原版 `IVP_Time_Manager` 的 `0x20` 布局恢复包装：步长限制为 `1/200` 到 `1/10` 秒，
x86 下临时设置与原版相同的 x87 精度控制，然后进入原 DLL 的 event-manager 主体。
因此 `IVP_Environment::simulate_variable_time_step` 没有被错误地降级成固定 PSI 的
`simulate_dtime`。

本轮还接入原版保留的 manager 构造/析构、event 插入/移除与 standard event-manager
入口（RVA `0x2F0B0/0x2F170/0x2F1D0/0x2F200/0x2EF70`）。`update_event`、
`get_event_count` 和 `reset_time` 在零售 DLL 中没有独立函数体，因而只按已经确认的
`IVP_U_Min_List` 与 `IVP_Time_Manager` 字段偏移重建邻近源码中的短内联逻辑，没有为它们
虚构 RVA。

原版 manager 构造器会先创建并在时间 `0` 插入一个自己拥有的 PSI 事件，所以新对象的
事件数是 `1`，不是 `0`。`insert_event` 的 `ret 0x0C` 与两次 dword 传递确认
`IVP_Time` 仍按 8 字节值传递；MinList 返回的 16 位有效句柄写进
`IVP_Time_Event +0x04`。`remove_event` 只消费该句柄，并不把缓存索引改回 unused。
精确 DLL 场景在 PSI 两侧插入两个栈上 gameplay 事件，验证按时间排序、内联
`update_event` 重排、计数和索引更新，再在 manager 析构前显式移除它们；这同时避免把
非堆对象留给原版析构器删除。`IVP_Time_Event`/`IVP_Time_Manager` 是环境可达的内部 ABI，
邻近公开源码清单没有这两个 owner，因此该案例不虚增 1575 项公开接口覆盖分母。

环境级 `IVP_Environment::reset_time` 还必须把同一个 offset 传播到活动 simulation unit，
不能只重置事件队列。原版 sim-units manager 的构造和保留函数共同确认首个活动 unit 在
manager `+0x18`，unit 的 next、core vector、controller-entry vector 分别在
`+0x08/+0x0C/+0x1C`；controller 的 `reset_time` 是七槽 Ballance vtable 的第 3 槽。
当前内联实现据此重置每个 controller、core 和 real-object hull time，再把 environment
当前时间减去 old-last-PSI、递增 `current_time_code`，并把 last/next PSI 设为 `0` 和一个
`delta_PSI_time`。测试用排队事件、controller、core 与 object hull 的不同绝对时间，
确认重基准后相对时间全部保持一致。

## Floating controller

`ivp_controller_floating.hxx` 明确将 `IVP_Template_Controller_Floating` 和
`IVP_Controller_Floating` 标为公开 API，但 Ballance 链接结果没有留下类专属函数体或
vtable，因而不能为这些方法填写零售 RVA。IDB 导入 UDT 仍提供了可交叉检查的布局：模板
为 `0x50`，两个 force limit 位于 `+0x00/+0x04`，两个 `IVP_U_Point` 位于
`+0x08/+0x28`，两个 float distance 位于 `+0x48/+0x4C`；运行时对象为 `0x40`，
Independent base、object、两个 limit、两个 `IVP_U_Float_Point` 和两个 double distance
依次位于 `+0x00/+0x04/+0x08/+0x0C/+0x10/+0x20/+0x30/+0x38`。这里把 IDB 当布局证据，
没有把它当函数行为的 ground truth。

当前实现按邻近公开实现恢复完整第 2 类接口，并只调用已经确认的 Ballance 基础 ABI：
object-space 作用点通过 Cache 变换，构造/析构通过 Controller Manager 对同一 Core
注册/注销，每个 PSI 用 `IVP_Solver_Core_Reaction` 建立一维有效质量，再分别按 adhesive
和 repulsive force 上限截断冲量。ray cast 返回 `IVP_FAULT` 时保持 Core 不变；成功路径
取得和释放 Cache reference 恰好一次。Host 场景用 0.5 米距离误差和 2 N adhesive 上限，
验证 0.1 秒 PSI 只产生 0.2 的 y 向冲量，并验证失败 ray、priority、注册/注销和 cache
引用平衡。这不是用构造 smoke 代替物理行为。

同一批差分还恢复了 `IVP_Template_Controller_World_Friction` 与
`IVP_Controller_World_Friction`。Ballance 的导入布局分别为 `0x80` 和 `0x4C`：模板是四个
连续的 `IVP_U_Point`；运行时对象在 Independent base 和 object 后保存四个
`IVP_U_Float_Point`，并保留邻近头中未被算法读取的 `clip_manhattan +0x48`。由于原 DLL
没有类专属函数体或 vtable，这一类型同样是第 2 类，不登记伪 RVA。

重建的 PSI 路径先把 desired/current 世界线速度差变换到 core space，按
`friction_value_translation * delta_time` 逐轴裁剪后再变回世界空间；随后对 core-space
角速度差按 rotational rate 裁剪，以转动惯量换算角冲量。测试使用 moving-platform
目标速度、非对称三轴线性/角速度上限和 `0.25 s` PSI，验证每一轴只靠近期允许的最大
速度增量接近目标，同时验证 controller priority 以及 Core 注册/注销。测试还显式补齐
Matrix3 float-vector 的 ABI binding，避免未绑定包装留下旧输出而制造假阳性。

## Motion controller 的模板与运行时布局

`IVP_Template_Controller_Motion` 的 `max_torque` 是偏移 `0x20` 的单个 `float`，模板
总大小 `0x24`；创建运行时 controller 时，原算法把它展开为三个轴相同的
`IVP_U_Float_Point`。`IVP_Controller_Motion` 中的运行时 `max_torque` 位于 `0x58`，
对象总大小 `0x88`。当前实现分别保持这两个布局，并恢复目标 core/对象位置、目标
四元数、三轴推力/力矩限幅、线性/角阻尼以及 core 注册和唤醒逻辑。不能把模板的标量
字段直接改成向量，否则模板后续字段和构造读取都会错位。

这部分现在明确归为第 2 类重建，而不是无出处的本地实现：构造/析构顺序选择性取自
邻近 `ivp_controller_motion.cxx`，注册和注销分别落到 Ballance 保留的
`ControllerManagerAddToCore`/`ControllerManagerRemoveFromCore` 路径，基类析构继续调用
零售 `IVP_Controller` 析构体。x86 host 的完整生命周期案例以同一组 Real Object/Core
身份核对一次注册和一次注销，并验证模板标量力矩展开、逐轴平移限幅、目标位置/四元数
变化时唤醒以及重复目标不重复唤醒。运行时 `0x88` 的完整字段布局仍标为 IDB-only，
因为保留的 manager 入口只证明 Core-facing 身份和调用顺序，不能反推所有字段偏移。

同一条继承链上的两个公开配置类型也已单独核对。IDB 回读确认
`IVP_Template_Controller_Golem` 总大小为 `0x38`，其五个 `IVP_FLOAT` 策略字段位于
`+0x24/+0x28/+0x2C/+0x30/+0x34`；构造默认值来自邻近实现，但 Ballance DLL 没有保留
可跳转的构造函数体，因此当前只重建短构造逻辑，不虚构 RVA。运行时
`IVP_Controller_Golem` 的导入 UDT 和邻近声明都给出 `0x130`：`0x88` 字节 Motion 基类后，
动态目标状态位于 `+0x88..+0x117`，五个策略浮点位于 `+0x118..+0x128`。当前保持唯一新增
纯虚槽 `resolve_for_problem`，并选择性重建移动目标外推、逐轴推力/力矩限幅和异常策略
回调。邻近实现的旋转阻尼实际使用 `damp_factor` 而非 `angular_damp_factor`，当前原样保留
这一版本行为。两个 x86 host 场景验证正常追踪和越界回调，但由于 DLL 没有任何 Golem
专属函数体，这仍只属于 IDB/source 布局与邻近算法证据，不提升为零售行为确认。
`IVP_Template_Constraint_Fixed_Keyframed` 则正好是 `0x24` 字节的 Motion template 基类，
没有附加字段；其 default constructor 同样是源码内联接口。两项结论已写入可重放 IDB
修正脚本和获授权的分析副本。

独立的 `IVP_Template_Constraint` 为 `0x200`。零售构造 RVA `0x12570` 确认
`force_factor/damp_factor/limited_axis_stiffness` 位于 `+0x17C/+0x180/+0x184`，默认值
为 `1/1/0.3`；六个 axis type 从 `+0x188` 开始，平移三轴默认固定、旋转三轴默认自由。
RVA `0x12960/0x12990` 分别把平移索引和加三后的旋转索引设为 LIMITED，并写入
`borderleft_Rfs +0x1A0` 与 `borderright_Rfs +0x1B8`。精确 DLL 案例据此构造一个有界
平移/扭转 joint，验证非对称边界、正交轴状态和 break/clip impulse 配置。

Fixed-Keyframed 运行时类型也已拆开恢复，而不是把另一版实现整类搬入。主 IDB 回读确认
`IVP_Constraint_Fixed_Keyframed` 为 `0xE0`：`IVP_Controller_Dependent` 前缀在 `+0x00`，
最大平移力/力矩在 `+0x04/+0x14`，四个控制因子在 `+0x24..+0x30`，environment 在
`+0x34`，位置/姿态时间和两个对齐四元数位于 `+0x38..+0x8F`，位置、速度、两端对象和
受控 core vector 分别位于 `+0x98/+0xB8/+0xC8/+0xCC/+0xD0`。这些偏移与邻近声明逐项
一致，但 DLL 中不存在该类自己的函数体或 vtable，因此只证明对象 ABI，不证明算法版本。

当前控制算法逐段复用已经确认的 Ballance core/cache/controller-manager ABI：目标位置
在 reference-object 坐标系中按速度外推，目标姿态用两帧四元数插值，再对 attached 和
reference core 施加等量反向修正。它所需的 `IVP_U_Point_4` 和
`IVP_Solver_Core_Reaction` 分别为 `0x10` 和 `0x148`；零售 RVA `0x33A30` 的平移初始化、
RVA `0x33AD0` 的二维冲量，以及保留代码对 `+0x0C/+0xD8/+0x138` 的访问共同确认求解器
布局。只对链接裁掉的 rotation 初始化、3x3 求逆和一/三维线性或角冲量短方法采用邻近
实现。邻近 Fixed 算法虽然保存 `max_translation_force/max_torque`，simulation 中并不
用它们限幅；在没有零售类函数体佐证前，当前也不擅自添加另一套限幅语义。

两个确定性 x86 host 场景分别验证 reference frame 中的位置目标和相对姿态目标：两端
获得相反的线性/角向速度变化，动量或角向和保持平衡，并检查 controller 注册、唤醒、
environment 查询与析构注销。它们证明当前重建内部自洽，但不冒充 Ballance Player 中
执行过已被裁掉的 Fixed-Keyframed 原版算法。

## 基础值类型的默认构造与 Time 差值精度

邻近公开头明确把 `IVP_Time`、`IVP_U_Float_Point`、`IVP_U_Point` 和
`IVP_U_Quat` 的 default constructor 定义为不初始化；`IVP_U_Float_Point3` 也只有
裸数组成员。此前公开兼容头虽然保留了 `= default`，但 `IVP_Time::seconds`、
`IVP_U_Float_Point3::k` 和 `IVP_U_Point::k/hesse_val` 的成员初始化器会在构造前隐式
清零，进而使 `IVP_U_Matrix3` 与 `IVP_U_Matrix` 也变成清零构造。这不是布局差异，却会
改变 Ballance 构造器和内联算法把这些对象当作仅写所需分量的 scratch storage 时的行为。

当前已移除这些隐式初始化，并对 Time、float/double point、matrix 和 quaternion 整条
值类型链增加 trivial-default-construction 门禁。运行案例在 placement-new 前以 `0xA5`
填充对象存储，确认 default construction 不改写任何字节；随后原有 Ballance 朝向组合、
矩阵往返和角度提取案例仍通过。该结论只涉及 C++ 内联构造语义，IDB 没有可对应表达的
成员初始化器，因此不为了“同步”而虚构新的 DLL 函数或 IDB 入口。

这项修正还揭出了 Check Distance host 案例此前的假通过：anchor 初始化确实调用保留的
`IVP_U_Matrix::vimult4(Point, Point)`，但测试适配器遗漏了该 RVA，旧的隐式清零使空调用
看起来像合法的零向量结果。现在适配器按真实 `__thiscall` 签名绑定逆变换，案例使用非零
object-space anchor，使漏掉该调用时初始 inside/outside 状态立即失败；同一场景连续运行
30 次通过，不再依赖栈内容。

邻近公开源码的 `IVP_Time::operator-` 虽然返回 `double`，却会先把秒差强制舍入为
`IVP_FLOAT`；这一点不能搬入 Ballance。全量行为测试证明该舍入会同时改变 compound
object detach 后 PSI event 的 `delta_time`；Check Distance 的同轮失败另经重复运行定位为
测试适配器漏绑逆矩阵变换，与 Time 精度无关。当前因此保留 binary64 差值，并增加非整数大时间戳门禁，明确把这里记录为 Ballance 与邻近
源码的行为差异。这个反例也是“源码导入类型/实现不是 ground truth”的直接证据。

## Material Simple 构造写集

`IVP_Material_Simple(double,double)` 不能由普通成员初始化器包住再跳入 DLL。Ballance
RVA `0xBF00` 的完整写集为：vptr `+0x00`、`second_friction_x_enabled +0x08`、
`friction_value +0x10`、`elasticity +0x20`、`adhesion +0x28`；它不写
`material_type +0x04` 和 `second_friction_x +0x18`。附近源码虽然给 `material_type`
提供 `UNINITIALIZED` 成员默认值，但该写入不存在于零售构造指令中，因此不能作为
Ballance ground truth。

此前兼容头在进入 RVA `0xBF00` 前分别把这两个未写字段设为 `-1` 和 `0.0`，使对象状态
比原版多了两次写入。当前 `IVP_Material` 基类只初始化零售确认的 anisotropic-friction
开关，`IVP_Material_Simple` 四个 double 字段不再带成员初始化器；friction、elasticity
和 adhesion 仍由 DLL 实体初始化，second friction 必须由调用者在启用对应开关前显式
设置。exact-DLL 测试把整对象预填 `0xA5` 后 placement-new，确认 `+0x04` 与 `+0x18`
保持原字节，同时验证三个被写物理参数和 vtable getter。

## Compact ledge 射线求交

Ballance 原版保留了 `IVP_Ray_Solver_Os` 的构造、ledge-tree 遍历和 compact-surface
入口。进一步追踪保留的 tree walker RVA `0x21EE0` 在 VA `0x10021FAE` 的直接调用后，确认
原先未命名的 RVA `0x21AB0` 正是
`IVP_Ray_Solver_Os::check_ray_against_compact_ledge_os` 的精确函数体，而不是已内联且没有
可复用入口。机器码同时包含邻近算法的双三角快速路径和多三角凸 ledge 路径：前者保留
双面命中与法线翻转，后者只接受入射面并用重心坐标判断三角形内部。

这里存在必须保留的 Ballance ABI 差异。邻近源码声明返回 `IVP_BOOL`，但 RVA `0x21AB0`
在若干命中和未命中出口不会规范化 EAX，函数以 `ret 4` 返回；把它直接声明成 bool thiscall
会把临时寄存器误当成返回值。当前包装按 `void` 调用精确原体，并暂时把 `hit_listener`
替换成两槽 forwarding listener：它记录是否收到 `add_hit_object`，同时把对象、ledge、
triangle、binary64 距离和法线原样转发给调用者，最后通过 RAII 恢复原 listener，再返回
稳定的源码级 `IVP_BOOL`。没有 resolver 的 host 环境才使用现有本地算法 fallback；没有
引入邻近版本的整套内部 `IVP_Compact_Ledge_Solver` 类型。

强制加载用户指定原版 DLL 的测试现在用 retained Pointsoup 构造真实 compact geometry：
双三角中心向下射线必须命中并得到距离 2、外部射线必须未命中，重复命中验证代理没有泄漏
或覆盖调用者 listener；四面体另覆盖一般多三角分支并检查法线反向于射线。真实关卡 Player
对照也保留：同一静态 compact ledge 的 surface-manager 和显式 object-space 路径命中距离
均为 `2.020610`，命中对象与 ledge 一致，不是只验证非空返回值。`IVP_Ray_Solver` 的环境
OV tree 遍历、ray group、最短命中和最多 256 项的 min-hash 收集器也已恢复；后者的 x86
大小是 `0x2088`，其中包含 `hit_info` 前的 4 字节对齐。

## Active value 的可构造 terminal 类型

此前头文件只允许接收 manager 返回的 active value，并把原版可构造的
`IVP_U_Active_Terminal_Double/Int` 错声明成抽象类。原版保留的构造函数、当前静态布局
和邻近算法相互吻合，因此现在按原版布局恢复名称复制/释放、引用计数、依赖通知、
立即或 PSI 延迟更新，以及 terminal 的具体 vtable；名称和对象内存均使用
`physics_RT.dll` 的分配器。非 const 的 `get_name`、float/double/int getter 也已补回，
同时保留 const 重载方便 Mod 使用。

这里的多继承调用约定不能从装饰名表面推断。RVA `0x15D30` 的
`IVP_U_Active_Terminal_Double::update_float` 实际由 delayed 次基类表进入，ECX 指向完整对象
`+0x28`；函数以 `ECX-0x08` 读取 `double_value +0x20`、以 `ECX+0x08` 读取
`old_value +0x30`，最后先减 `0x28` 再调用主基类 listener 分发。RVA `0x15D60` 的 int
版本同理接收 `this+0x20`，从 `-0x04/+0x04` 读取 `int_value +0x1C` 和
`old_value +0x24`，再减 `0x20`。公开 wrapper 因此显式把完整对象调整为
`IVP_U_Active_*_Delayed *` 后才进入 DLL；直接传完整 `this` 会把相邻字段和次 vptr 当成值。

这些指令连同构造 RVA `0x15980/0x15A00/0x15A80/0x15C50/0x15CC0` 及 setter
`0x15D80/0x15DF0`，把五个 owner 的布局证据从导入类型提升为 retail-confirmed：
`Active_Value 0x0C`、`Active_Float 0x28`、`Active_Int 0x20`、terminal double `0x38`、
terminal int `0x28`。IDB 原先给后三个构造器的自动类型分别是 `_WORD *(..., int)`、
`int __stdcall(int, double)` 和 `int __thiscall(int, int, int)`，与装饰名、RET 和字段访问冲突；
现已改为正确的 x86 `__thiscall` 构造器原型。两个 update 入口的 IDB `self` 类型也刻意标为
delayed 次基类，以表达真正的入口寄存器语义，而不是只复制源级完整类签名。

manager 的 `get_active_float_by_name` / `get_active_int_by_name` 在 Ballance 中被链接器
裁掉，但保留的 install/create 调用序列证明了搜索哨兵 `+0x24`、两个名称 hash
`+0x08/+0x0C` 和相同的 CRC/字符串比较语义。当前逐项重建这两个查询，并保留邻近实现
对数字名称即时创建 terminal 的行为。十种 oscillator/mixer/filter/logic 表达式节点也
逐类重建在已经验证的 value/float/int 基类之上；原版没有实例化这些节点，故不宣称存在
可跳转的零售函数体或零售派生 vtable。它们的依赖传播、限幅/切换和时间波形由
`IvpActiveValueGraphTest` 通过真实 x86 thiscall 解析边界验证。

`IVP_Cluster` 的四个公开方法现已闭合。根构造与完整析构由 RVA `0x9C80/0x9CB0` 证明；
father/template 构造沿用已核对的 `IVP_Object` 前缀，析构逐个删除子对象并依靠对象析构
解除链首。子对象链遍历和完整生命周期都有确定性 host 测试，不再列为待判定。

## Buoyancy attacher 的真实模板基类

RVA `0x104C0` 的派生构造函数初始化到对象 `+0x6C`，证明完整大小为 `0x70`；其
`IVP_Attacher_To_Cores<IVP_Controller_Buoyancy>` 基类前缀为 `0x1C`。VA
`0x10063524` 的六槽表依次指向 RVA `0x106B0/0x10720/0x10740/0x10790/0x10630/0x10640`，
即 active-set add/remove/delete 三个回调、deleting destructor、参数查询和 surface
查询。

此前 IDB 把槽 3 的 RVA `0x10790` 留成 `int __thiscall(void *, char)`，并把它调用的
RVA `0x10650` 保持匿名。指令实际证明前者是带 `unsigned int flags`、返回原对象指针的
scalar-deleting destructor：先调用后者，再按 flags bit 0 决定是否进入原版
`operator delete`；后者则注销 active-set listener 并析构 `+0x04` 的
`IVP_VHash_Store`，但不释放外层对象。两者现已在主 IDB 中分别命名为派生类 deleting
destructor 和完整析构器，类型不再沿用错误导入。

当前声明保留真实公开模板继承，而不是以 `std::byte[0x1C]` 假装基类。由于
`IVP_Controller_Buoyancy` 的构造/析构是私有且内部算法尚未完整验证，这个特定模板实例
用显式特化把前三个回调直接转发到原 DLL；不会为了让模板编译而公开一个猜测的 controller
类体，也不会把邻近版本整个搬入。

浮力输入/输出现在使用独立恢复的 `IVP_MI_Vector_Base`，大小为 `0x10`；输入和输出
对象大小分别为 `0x40` 与 `0x58`，元素数按原版启用四分量 float vector 的布局固定为
12 和 18。可变长 `IVP_MI_Vector` 的 11 个公开方法已经恢复，但没有顺带搬入未在原版
符号目录中保留的整套 multidimensional interpolator；分配仍走 `physics_RT.dll`，避免
跨 CRT 释放。

## 标准重力 controller 的 pinned 分支差异

邻近源码的 `IVP_Standard_Gravity_Controller::do_simulation_controller` 会先检查
`IVP_Core::pinned`，只对未固定 core 执行 damping、提交异步 push 和重力积分。Ballance
零售 RVA `0x12010` 没有这条检查：它从 `IVP_U_Vector +0x02/+0x04` 读取长度和元素数组，
随后无条件对每个元素调用 RVA `0xC610`/`0xCBD0`，并更新 core
`speed +0xA4/+0xA8/+0xAC`。此前把 `mov ax,[ecx+2]` 误读为 Core 位域访问是不成立的；
这里的 `ecx` 是 vector。

因此兼容包装在 DLL 可用时直接调用零售函数，无 DLL fallback 也复现 Ballance 的无条件
循环，不搬入邻近版本的 pinned 分支。精确 DLL 案例给第二个 Core 设置邻近声明中的
`pinned` bit 8–9，仍观察到 `speed_change` 被提交且重力被积分，锁定了这一版本差异。
`set_standard_gravity` RVA `0x11FE0` 另确认对象 `grav_vec` 位于 `+0x04/+0x08/+0x0C`；
Environment 构造处以 `0x14` 分配对象并安装 VA `0x10063598` 的七槽 vtable，所以
`IVP_Standard_Gravity_Controller` 的完整 x86 大小为 `0x14`。

`IVP_Template_Anchor::set_anchor_position_ws` 的零售 RVA `0x13E80` 先把对象指针写入
`+0x00`，再把输入点的完整 `0x20` 字节复制到按 8 字节对齐的 `+0x08`。因此模板
anchor 大小为 `0x28`，中间 `+0x04` 是对齐填充，不应错误压缩。精确 DLL 案例使用
非平凡世界坐标和单独的 `hesse_val`，确认四个 double lane 全部复制且对象身份不变。

## 已核对但目前没有版本差异的项目

- `IVP_FLOAT`：邻近源码 `ivu_types.hxx` 同样定义为 `float`；原版布局也要求 4 字节。
  `IVP_DOUBLE` 为 8 字节 `double`。IDA 显示的部分 `long double` 形参不是改写这两个
  typedef 的依据。
- `IVP_SurfaceManager`：原版和邻近源码都使用 11 槽顺序，析构位于 `get_type` 之前。
- `IVP_SurfaceManager_Polygon` 与 `IVP_SurfaceManager_Ball`：原版均为 4 字节 vptr 加
  4 字节 compact geometry 指针，总大小 `0x08`。Polygon 现已恢复为可由 Mod 构造的
  具体类：ray/radius/terminal-ledge/mass-center/inertia/radius-dev/single-convex/type 以及
  两个共享 reference no-op 均直达 RVA `0xA490`、`0xBB00`–`0xBCF0` 的原版实体。
  析构层不嵌套调用包含基类析构的 RVA `0xBD00`。精确 DLL 案例以非零 mass center、
  三轴惯量和 3-4-5 中心偏移验证半径 `15.0`、量化 deviation `5.2`。
- `IVP_BetterDebugmanager`：零售构造 RVA `0x60C30` 安装 VA `0x10063D34` 的两槽
  vtable，将 `initialized +0x04` 置一，并清零从 `+0x08` 开始的 2048 个 32 位开关，
  因而完整大小为 `0x2008`。`is_debug_enabled`、`dprint`、`output_function` 和构造函数
  直达零售实体；enable/disable 与空析构按邻近源码在同一布局上重建。特别地，x86 的
  variadic member `dprint` 是把 `this` 也压栈的 `__cdecl`，不是普通 `__thiscall`；包装
  先安全格式化，再以 `"%s"` 进入原 RVA `0x60BC0`，保留其 vtable slot 0 输出分派。
  静态初始化 thunk RVA `0x60C90` 还确认全局实例位于 RVA `0x77AD8`。精确 DLL 案例用
  surface-builder 诊断验证开关、越界拒绝、格式化文本与派生输出重定向。
- Pointsoup：`convert_triangle_to_compace_ledge` RVA `0x3AC00` 直接读取 RVA
  `0x763A8` 的 canonical triangle、经原版 aligned allocator 克隆，再写入三个输入点；
  `IVP_SurMan_PS_Plane::get_qlen_of_all_edges` RVA `0x3A2D0` 则确认 Point 基类 `0x20`
  后的 vector 位于 `+0x20/+0x22/+0x24`，完整大小 `0x28`。缺失的 oriented-area、
  compact-surface wrapper、cached-triangle cleanup 和 `IVP_Vector_of_Points_256` 只组合
  这些已确认布局及 Ledge Soup 原版入口。独立 exact-DLL 案例验证 3×4 face 的 double
  area `24`、edge square sum `50`，以及 triangle/tetrahedron hull 和 compact surface。
  另一个不能忽略的零售行为是 RVA `0x3AE20` 的非三角 hull 路径将 x87 control word
  OR `0x0300` 后从分支直接返回，邻近源码中位于 return 之后的恢复代码同样不可达；
  这会改变同进程后续浮点舍入。因此 Pointsoup 场景必须像 Ballance 的初始化/构建阶段
  一样隔离，不能和依赖初始 x87 precision 的材质数值案例串在同一测试进程中。
- Better Statistics：manager 无 vptr，零售构造 RVA `0x2DB50` 确认 enabled `+0x00`、
  两个 vector `+0x04/+0x0C`、simulation time `+0x18`、delayed `+0x20` 和 interval
  `+0x28`，完整大小 `0x30`。该构造不会构造额外基类，可以安全直接调用；其余 manager
  方法及 `0x48` Data Entity 在 DLL 中均被裁剪，按相邻实现和现有 vector/allocator
  入口重建。Callback Interface 保持原版三个纯虚槽，刻意没有虚析构槽。
  Data Entity 析构只释放 `text`，不释放 array，这是相邻版本的实际所有权规则；调用者
  在销毁 array entity 前仍需负责其 array storage。另一个保留的历史怪异行为是
  DOUBLE_ARRAY 构造最后三次写入使用 `data.int_array` 的 color 成员：它们覆盖 double
  layout 的 `height/bg_color/border_color`，结果分别为 `0/1/2`，而最后的 graph color
  没有初始化。兼容层没有“顺手修好”这处 union 偏移，运行案例明确锁定该行为。
- `IVP_Collision_Filter`：原版和邻近源码都是“pair check、environment deleting、析构”
  的 3 槽接口；group-ident 子类大小 `0x08`，meta filter 大小 `0x10`。
- active value：manager 的原版 vtable 为 15 槽、对象大小 `0x28`；float/int 与 terminal
  类型的原版大小分别为 `0x28`/`0x20` 与 `0x38`/`0x28`，目前与邻近源码一致；
  terminal 已可由 Mod 直接构造，不再只是 manager 返回值的只读声明；manager 名称查询
  的搜索哨兵和哈希路径也与原版 install/create 调用序列一致。
- `IVP_Vector_of_Ledges_16` / `IVP_Vector_of_Ledges_256`：内嵌容量和 x86 尺寸分别为
  16/`0x4C` 与 256/`0x40C`。
- `IVP_U_Float_Hesse`：是独立的源码语义类型，不是简单 typedef；在启用
  `IVP_VECTOR_UNIT_FLOAT` 的原版 ABI 中继承已含第四个 `hesse_val` float 的
  `IVP_U_Float_Point`，所以两者大小仍同为 `0x10`。RVA `0xE7F0` 与 `0xE8C0` 的
  字段访问确认了这个布局。
- `IVP_U_Hesse`：RVA `0xE530/0xE5C0/0xE740` 分别在投影、三点构面和归一化中读取或
  写入 `+0x00/+0x08/+0x10/+0x18` 四个 double，因此是 `0x18` 的 `IVP_U_Point`
  前缀加一个 `IVP_DOUBLE hesse_val`，完整大小 `0x20`。精确 DLL 案例还确认三点构面先
  保留未归一化的面积法向量及同比例常数项，必须显式调用 `normize()` 才成为单位平面。
- `IVP_U_Quat`：RVA `0x190C0/0x191B0/0x194E0` 的矩阵写入、矩阵读取和归一化连续访问
  四个 double，确认 `x/y/z/w` 位于 `+0x00/+0x08/+0x10/+0x18`，大小为 `0x20`。
  精确 DLL 案例把 Ballance 的非平凡 Core/Object 旋转矩阵转为四元数，模拟积分漂移后
  归一化并转回矩阵；误差保持在 float 角度输入造成的 `1e-8` 范围内。
- `IVP_VHash_Store`：RVA `0x1DE80` 的构造依次写 `size +0x00`、mask `+0x04`、元素数
  `+0x08`、三 dword 元素数组 `+0x0C` 和外部存储哨兵 `+0x10`；RVA `0x1DEE0` 的扩容与
  `0x1DEB0` 的析构独立确认数组所有权和 `0x14` 边界。精确 DLL 案例按 Phantom 的
  object/Core mindist counter 用法从四槽加入三个对象，触发扩容到八槽，再验证查询、
  更新、删除及最终释放；不是只构造空容器。
- `IVP_VHash`：它不是 `IVP_VHash_Store` 的同布局别名。RVA `0x1DB30` 的构造写入
  vptr `+0x00`、表长 `+0x04`、打包的 `nelems:24/dont_free:8` 字 `+0x08` 和槽数组
  指针 `+0x0C`，完整大小为 `0x10`；RVA `0x1DB80/0x1DC60/0x1DD10/0x1DE10`
  的析构、加入、删除和查找都独立使用同一组偏移。精确 DLL 案例让三个不同对象共享
  同一 hash，验证扩容 `4 -> 8`、碰撞链查找、删除中间对象后位移元素仍可找到以及元素
  计数；因此覆盖的是 Ballance 实际依赖的开放寻址语义，而不是无碰撞的容器演示。
- `IVP_Cache_Object` / `IVP_Cache_Object_Manager`：manager 构造 RVA `0x189C0` 明确按
  `0xD0 × count` 分配连续缓冲，析构 RVA `0x189F0` 同样以 `0xD0` 遍历；manager 的
  count、复用下标和缓冲指针分别位于 `+0x00/+0x04/+0x08`，完整大小 `0x0C`。六个
  cache 坐标变换体在 `+0x30` 读取 double 矩阵，在 `+0x90/+0x98/+0xA0` 读取平移，
  与 `q_world_f_object +0x10`、`m_world_f_object +0x30`、`core_pos +0xB0` 和 `0xD0`
  边界一致。精确 DLL 案例使用真实 Core→World 组合帧验证点和方向的 double/float
  往返，并让原版 manager 构造、按步长访问和析构同一四项 cache ring。另一组两项
  ring 场景先以 no-lock accessor 取得 ray 查询 cache，再以 locking accessor 正常把第一项
  引用计数从 0 增为 1，确认 RVA `0x18930` 会跳过仍被 contact/坐标调用者持有的项、淘汰
  第二项并同时清除旧对象 `+0x40` backlink；同一案例还以匿名 retail RVA `0x1A190` 为
  oracle 比较 cache identity 和引用转换。静态 RVA
  `0x18910` 只清 cache `+0x08` owner 和对象 backlink，不擅自减少引用计数。调用者释放
  引用后该槽才能复用，manager 析构最终清掉所有仍存活对象的 backlink。这固定了缓存
  引用与所有权语义，而不只是检查连续内存步长。
- `IVP_U_Matrix_Cache`：邻近 `ivp_3d_solver.hxx` 把连续碰撞的 21 个时间采样全部写成
  header-inline，但 Ballance 仍保留 private `p_init` RVA `0x2B300`。该实体从 Cache Object
  `+0x08` 取得 Real Object，继而从对象 `+0xA4` 取得 Core；它把 cache time code 复制到
  `+0x10`，并按 movement state 将 `+0x14..+0x64` 的 21 个指针全部指向 cache 当前矩阵，
  或仅保留槽 0。求交 RVA `0x37910` 又直接证明缺失槽使用
  `+0x68 + index*0x80` 的内嵌矩阵并调用原版 `calc_at_matrix`，最大索引 20 因而把完整尺寸
  固定为 `0xAE8`。当前构造和时间码刷新调用原 DLL；按索引惰性计算、当前时刻读取与
  时间码 guard 仅重建这些已内联的短逻辑。真实 DLL 案例验证静态对象全槽别名、动态对象
  只保留槽 0、Core/时间码捕获，以及 movement state 改变但时间码不变时不得擅自刷新。
  `base_time +0x08` 在 Ballance release 的 `p_init` 中没有写入；兼容层没有用邻近 DEBUG
  分支替它补值。该 owner 来自邻近 private collision header，不计入 1575 个
  `IVP_EXPORT_PUBLIC` 候选分母，但属于 physics runtime 实际使用并已验证的低层接口。
- `IVP_3D_Solver`：Ballance 保留 RVA `0x37790/0x37910/0x37B30` 的根细化、最大偏差搜索和
  collision-aware 搜索。装饰名、`ret 0x34/0x2C/0x30`、`this+0x08/+0x10` 字段访问和
  vtable slot 0 间接调用共同确认参数与关键布局；IDB 原先的三个整数/`__stdcall` 占位原型
  已改成真实 thiscall。邻近 `find_first_t_for_value_max_dev2` 的函数体仅转发到 max-dev，
  当前按第 2 类选择性重建；`find_first_t_for_value_no_zero_dev` 被 `#if 0` 排除且访问
  Ballance `0xAE8` Matrix Cache 没有的 next-PSI 字段，`print` 也没有实现体，二者不伪造。
  精确 DLL 案例覆盖阈值外无碰撞、起始穿透后继续靠近触发重复碰撞、起始穿透后逐渐分离不
  误报，以及两个 Object/Core 在 `0.05 s` 穿越目标距离的根细化。后三步实际形成
  Mod vtable → DLL solver → Mod `get_value` 的跨模块往返。

  这组测试还发现了邻近源码看不出的编译器 ABI 风险：原版 VC6 `calc_nullstelle` 把
  `IVP_Time*` 隐藏结果地址作为第一个栈参数并用 `ret 0x34` 清理，而现代 MSVC 会将当前
  8 字节平凡 `IVP_Time` 作为寄存器返回。通用 `InvokeThis<IVP_Time>` 因而会把后续参数整体
  错位；现在使用显式 sret thunk。另一个零售边界是 collision 路径以单精度 `0.005f`
  形成时间栅格、可在 `tMax` 后再采样一次，却没有 max-dev 路径的 index-20 停止判断；
  `tMax=0.1` 且始终低于阈值的合成输入会越过 21 槽。接口不篡改 DLL 行为，而是在头文件中
  要求调用者让额外样本仍落在缓存范围，测试使用正常的短 PSI 时间窗。
- `IVP_U_Min_Hash`：构造 RVA `0x37000` 确认 `size +0x00`、树 `+0x04`、每桶最小项
  `+0x08`、桶链表 `+0x0C` 和计数 `+0x10`，完整大小 `0x14`。add RVA `0x371C0`
  固定分配 `0x18` 字节元素，并访问 double value `+0x08`、`cmp_index +0x10`、payload
  `+0x14`，直接证明 Ballance 启用了 `SORT_MINDIST_ELEMENTS`，不能采用未启用该宏的
  `0x10` 元素布局。精确 DLL 案例以三个不同时间的 mindist 事件验证最小项选择、第二类
  `change_value` 的 remove/add 组合、原版 `remove_min`、显式删除、counter 和最终析构。
- `IVP_U_Matrix3::calc_eigen_vector`：邻近 `ivu_linear.cxx` 的公开实现意图是对
  `M - lambda I` 做带主元的零空间消元，但该版本声明局部 `m[3][3]` 后从未填充，且数处
  误读 `this->rows` 而不是已经减去特征值的副本；直接搬运会读取未初始化栈内容。
  Ballance DLL 没有保留这个函数体或可确认的调用点，因此当前第 2 类实现只保留接口、
  返回的自由度含义和零空间数学目标，改用三对行叉积选择最稳定的零空间向量并归一化。
  这项修复不声称是零售机器码复原；刚体惯量案例分别验证单一纵向主轴、二自由度横向
  特征空间和逆矩阵乘积。
- Matrix3/Matrix 刚体变换：重建的 `init_rotated3` 和 `inline_mmult4` 已与原版
  `IVP_U_Matrix::mmult4` 在非平凡 object-from-Core、world-from-object 组合上逐项比较，
  九个旋转元素和三个平移元素一致；随后由原版 `vmult4`/`vimult4` 完成接触点的
  Core→World→Core 往返，并由原版 `real_invert` 验证完整单位变换。往返误差约为
  `4e-8`，来源是旋转接口的角度参数在 Ballance ABI 中确为 32 位 `IVP_FLOAT`；矩阵存储
  和位置仍是 64 位 `IVP_DOUBLE`，不能据此把矩阵字段降成 float。
- `IVP_Controller` non-deleting destructor：IDA 的导入名把 RVA `0xB0F0` 标成
  `IVP_Triangle` 析构副本；其写入的 vptr 和 RVA `0x47A0` 异常清理引用证明它实际属于
  `IVP_Controller`。兼容层按指令证据修正名称，不沿用该条导入类型。
- `IVP_Core::transform_PSI_matrizes_core`：邻近源码与原版 RVA `0xD270` 一致，都是把
  `m_world_f_core_last_psi` 右乘传入的相对矩阵，然后刷新 last-PSI position、last-PSI
  quaternion 和 next-PSI quaternion。当前包装调用原版函数，没有移植邻近实现。
- `IVP_Time_Manager`：原版字段偏移为 `event_manager=0x04`、`min_hash=0x08`、
  `psi_event=0x0C`、`last_time=0x10`、`base_time=0x18`，总大小 `0x20`；这些偏移与
  RVA `0x2F010`、`0x2F250` 的访问一致。
- great-matrix / incremental-LU：原版 RVA `0x34100` 的构造函数以及
  `0x33D80`–`0x36030` 的字段访问确认 `IVP_Great_Matrix_Many_Zero` 为 `0x20`、
  `IVP_Incr_L_U_Matrix` 为 `0x30`；linear constraint solver 的零售入口访问到
  `this+0x128`，完整自然布局为 `0x130`。这些尺寸与邻近声明在 Ballance x86 对齐规则下
  一致，当前接口转发 27 个已保留函数体。原版 `align_matrix_values` 只把
  `matrix_values` 地址按 8 字节向下对齐，证明该构建的 vector-FPU 单元宽度为一个
  `double`，所以 `aligned_row_len == columns`；据此又选择性重建
  `calc_aligned_row_len`、清零、元素读写、compact-buffer/子矩阵拷贝、矩阵乘法共八个
  短成员；`mult()` 则直接复用原版 `mult_aligned()`。增量 LU 另重建了邻近源码确有
  函数体、且只访问上述已确认字段的 `add_neg_row_upwards_l_u`、`add_neg_col_L` 和
  `debug_print_l_u`；`exchange_columns_l_u` 则顺序组合原版 L/U 两个列交换入口。
  `lu_crout`、`lu_solve`、`lu_inverse`、`invert` 也按同一 `double` 行布局逐项移植，并由
  三接触耦合有效质量矩阵验证求逆乘积、由相关接触方程验证奇异拒绝。四个纯诊断输出
  成员按邻近源码恢复，它们只读取上述已确认布局。公开声明面现为 Great Matrix 33/33、
  Incremental LU 27/27、`IVP_Complex_Simple` 7/7；这不是可调用率：邻近树中只有声明、没有
  实现且原版也没有函数体的 allocating constructor、三个 complex/LP 求解入口、六个 index-LU/
  遗留入口和全部七个 Complex 算法保留精确签名并标为 `= delete`。调用它们会在编译期明确
  失败，而不会
  从另一修订伪造实现。Complex 的 0x34 字段布局也仅作为 source-only 类型写入 IDB。六个
  `IVP_VecFPU` 公开行操作在原版里都被完全内联；原版循环和
  `aligned_row_len == columns` 证据确认每个向量单元就是一个 `IVP_DOUBLE`，因此兼容层
  提供不改变对象 ABI 的标量实现，`address_aligned` 只保留为源码兼容的性能提示。
  裸加载 DLL 的增量 LU 回归还暴露了一个宿主前置条件：RVA `0x35780`–`0x36030`
  完全使用 x87，并假定进入物理步骤时 x87 寄存器栈为空。Ballance 主循环会维持这一状态，
  但独立测试进程在 DLL 初始化和测试运行库调用后不一定满足；精确 DLL 案例因此在进入
  LU 前调用 `_fpreset()`。这不是兼容层对求解算法的改写，实际 Mod 也不应在任意线程或
  占用 x87 栈的内联汇编中调用这些入口。

## PSI transform 与 CK transform 的边界

这不是邻近 IVP 源码本身的版本差异，而是 Ballance 适配层必须明确记录的集成边界。
真实 `Ball_Wood` 测试证明，只写 CK `SetPosition`/`SetQuaternion` 不会更新 IVP core；
下一次物理同步会用旧的 PSI transform 覆盖 CK 位置。要按一个已捕获的 core 矩阵恢复，
兼容路径为：

```cpp
const IVP_U_Matrix current = *core->get_m_world_f_core_PSI();
IVP_U_Matrix relative;
current.mimult4(&captured, &relative); // relative = current^-1 * captured
core->transform_PSI_matrizes_core(&relative);
```

同一帧仍需同步 CK transform，并按用途更新 `speed`/`speed_change`、
`rot_speed`/`rot_speed_change` 后调用 `ensure_in_simulation()`。这个方法只改 core 的 PSI
矩阵与四元数缓存，不等价于 `IVP_Real_Object::beam_object_to_new_position`：它本身不会
完成 broadphase、mindist 或碰撞缓存的整体重建。因此状态往返案例仍只证明活动木球上的
短距离恢复与后续仿真；需要真正传送对象时应调用现已恢复的 beam API。beam 会处理复合
对象的 `shift_core_f_object`/`q_core_f_object`，并通过零售 helper 失效缓存、重算
exact/invalid mindist、重查 broadphase、增长和必要时 reset hull。其顶层控制流来自邻近源码，
只有相邻 solver 和依赖 helper 的函数体在 Ballance 中保留，所以证据等级仍明确低于
“完整方法零售函数体保留”。

## 内联函数的处理规则

原版 DLL 没有函数符号并不等于该接口在 Ballance 版本不存在；大量 IVP 方法被编译器
内联。对于这类方法，邻近源码可以提供算法线索，但只有在原版调用点同时证明字段偏移、
常量、浮点宽度和边界行为一致后，才把实现选择性移入兼容头文件。无法完成这种交叉验证
的实现保持未公开，而不是从邻近源码整段搬入。

`IVP_Friction_Solver::calc_distance_matrix_column` 属于已经完成这种交叉验证的情况。
原版 `calc_solver_PSI` 的 RVA `0x36760` 在 `0x368BB` 和 `0x369BE` 各展开一次该成员，
分别从 long-term contact 的 `friction_infos[0]` 与 `[1]` 遍历同一 core 上的接触，读取
`index_in_fs`，按第一侧负号、第二侧正号，把接触点速度变化累加到
`matrix[row * aligned_row_len + current_column]`。当前头文件实现保持这一行主序索引、
空 core 分支和符号规则。`ease_test_two_mindists` 与 `debug_distance_after_push` 的邻近实现
主体均受恒假编译条件保护；Ballance release 也没有可观察代码，因此公开为明确的 no-op，
而不是声明一个不存在的 DLL 入口。

相反，`do_penalty_method` 在邻近源码里整体受 `#if 0` 保护，并且 Ballance 的
`do_friction_system` 从 `calc_solver_PSI` 直接进入 linear-constraint solve；不存在 penalty
分支。`do_penalty_step`、`do_inactives_pushes`、`complex_failed` 和 debug-only
`print_dist_velocity` 也没有原版可达调用或独立函数体，目前继续只保留声明，不能据此宣称
Ballance 支持另一构建配置的 penalty solver。

`IVP_Friction_System::core_is_found_in_pairs` 是邻近源码中的短成员：倒序扫描已经由零售
`find_pair_of_cores`、`core_is_terminal_in_fs` 确认布局的 `fr_pairs_of_objs +0x34`，比较每个
`IVP_Friction_Core_Pair::objs[0/1] +0x2C/+0x30`。它在 Ballance 中没有独立函数体，当前按这
些已确认字段选择性恢复。`get_controlled_cores` 只把按值传入的局部指针设为 null，优化后的
可观察行为为空操作；真正的 controller 接口是零售 vtable 中保留的
`get_associated_controlled_cores`，它返回 `moveable_cores_of_friction_system +0x2C`。
`do_pushes_distance_keepers` 则只在启用另一构建宏时进入主流程；Ballance RVA `0x36D70`
没有该分支，因此仅保留声明。七个 friction-system debug 成员同样只恢复声明，不把 debug
源码输出行为塞进 release ABI。

此前兼容层把 `IVP_Friction_System` 的 `+0x04` 至 `+0x4F` 全部封装为私有字节存储，虽然
尺寸正确，却破坏了邻近 IVP 源码对三个公开 vector、两个状态位和
`sum_energy_destroyed` 的直接访问。现在已拆成真实成员：vectors 位于
`+0x24/+0x2C/+0x34`，三个计数位于 `+0x3C/+0x3E/+0x40`，两项 8-bit enum bitfield
共享 `+0x44` 开始的四字节存储单元，最终 `IVP_DOUBLE` 位于 `+0x48`。内嵌 helper 和
vector 使用匿名 union 抑制宿主自动构造/析构，零售 complete constructor/destructor 仍是
唯一生命周期所有者；这既恢复源码可见性，也没有重复释放零售 vector。

## 完整构造器与短内联入口

当前公开层次还原了 `IVP_Core_Fast_Static -> IVP_Core_Fast_PSI -> IVP_Core_Fast ->
IVP_Core`，不再把三个无 vptr 前缀仅作为 `IVP_Core` 内的一段注释。MSVC 实际布局回读
给出 `0x60/0x1A8/0x1C8/0x238`；其中 PSI 基类结束于 `0x1A8`，派生类首个向量也从
`0x1A8` 开始，不能把十进制布局输出 `424` 误读成十六进制偏移或把 PSI 尺寸猜成
`0x1B0`。
零售 Core 指令对 `rot_inertia +0x14`、`inv_rot_inertia +0x34` 以及最终
`controllers_of_core +0x1C8` 的访问与该结果一致；`IVP_Core::init` 还以 `rep stosd`
清零 `0x8E` 个 dword，直接把完整对象尺寸锁定为 `0x238`。完整构造和析构对
`objects +0x50`、`controllers_of_core +0x1C8`、`sim_unit_of_core +0x1D4` 的独立访问
进一步排除了仅靠导入类型自洽的可能。五个公开 inline getter 现在归属真正的
`IVP_Core_Fast_Static`；material recompile 案例确认它们读取零售重算后的质量、逆质量和
三轴惯量，并确认四分量 solver view 与 `inv_rot_inertia` 完全别名。为保持已经发布的
BML 直接字段访问兼容性，字段可见性没有在此次层次恢复中收紧。

`IVP_Compact_Surface` 的两个历史公开 inline 原名是 `get_size` 和
`get_compact_ledge_tree_root`。早期兼容层只提供了自行改名的 `get_byte_size` 和
`get_ledgetree_root`，导致签名审计把原接口视为缺失。现在恢复原名：前者读取 `+0x1C`
中高 24 位的完整 allocation byte size，后者把 `+0x20` 的 root offset 加到 surface
基址；旧名字仅作为向后兼容转发保留。原版 pointsoup/Ledge Soup 生成的 tetra surface
同时验证了大于 `0x30` 的完整尺寸和非空根节点。

同一类型的 `byte_swap` 与 `byte_swap_all` 也已恢复。DLL 没有保留这两个实体；实现按
邻近源码逐字段重建，但特别保留 MSVC 与 PowerPC 位域分配方向不同所要求的重排，不能
把 packed word 当作普通 `uint32` 只反转四个字节。精确 DLL 生成的 tetra surface 被
复制后转换成大端数据，测试用独立解码器核对 surface/root offsets、四个共享点、所有
triangle/pierce/material 位和带符号 opposite-edge index。这个测试也纠正了一个错误的
测试假设：跨编译器位域转换不是在 Windows 对象上连续调用两次即可还原。

`byte_swap_all` 的递归范围保持邻近 Windows 实现，而没有擅自扩展格式：`swap_points`
为真时转换 root compact hull 的共享点、triangle、edge 与 ledge header，随后后序转换
ledgetree node；为假时只转换 tree 和 surface header。名称虽然容易让人误以为它会枚举
每个 terminal ledge，但邻近实现没有这样做，当前也不把未确认的扩展行为伪装成兼容。

邻近源码把 Group Ident complete destructor 写成空函数，Meta 则只需释放自身 vector。
Ballance 实际保留了此前未命名的 Group Ident 完整/删除析构 RVA `0x14940/0x14950`，以及
Meta 完整/删除析构 RVA `0x14CE0/0x14D20`。三张 filter vtable 都确认析构在槽 2；所有
deleting destructor 都读取完整 32 位 flags、返回原对象指针，而不是 IDB 旧类型中的
`char` 参数。Exclusive Pair 的已命名 RVA `0x14BF0` 也一并修正了该 flags 类型。
公开包装仍使用 defaulted override：从 C++ 析构函数体跳入完整 DLL 析构会使编译器随后
再次析构基类；Group Ident 的本地路径只进入无资源基类，Meta 只释放自身 vector storage，
不拥有也不删除子 filter。精确 environment callback 案例分别验证非 self-delete 的栈
生命周期，以及 Meta 先通知并移除所有外部所有权子项后再安全析构；通过 retail vptr
删除的对象则自然进入已补名的 DLL deleting destructor。

进一步的全库审计不再只抽查上述 filter。第一轮枚举的 39 个 IVP deleting destructor
中有 15 个旧原型带 `int` 返回值、`char` flags 或无类型 `this`：
RVA `0x9810`、`0x9B40`、`0x9C60`、`0xA230`、`0x10410`、`0x14A60`、
`0x159B0`、`0x16200`、`0x162D0`、`0x1DB60`、`0x2FDA0`、`0x2FF60`、
`0x308F0`、`0x37F90`、`0x380E0`。逐体反汇编均符合 MSVC x86 deleting-destructor
形状：先调用完整析构，检查 32 位 flags 的 bit 0，按需调用 `operator delete`，在 EAX
返回原对象并以 `ret 4` 清理参数；装饰名中的 `PAXI` 与此一致。因此统一修正为
`void *__thiscall(具体类型 *self, unsigned int flags)`，而没有把源码或旧 IDB 类型当成
单独依据。随后又从 generic/错误的 `std::locale::facet` 名称中确认 Collision Filter、
Performance Counter、Synapse、Collision Delegator 和 Triangle 五个同形入口；
`Audit-IvpIdbDeletingDestructors.py` 在该阶段报告 45/45、问题 0；后续纳入新确认的 OV 与
Root Mindist 入口后，当前主库报告 47/47、问题 0。

对应的 complete-object 析构也已独立审计。首轮 65 个已命名入口中原有 26 个缺失原型、整数
返回、无类型 `this` 或 owner 错配；尤其七个短函数全被导入成 `IVP_Triangle` 副本。
它们写入的 vptr、构造/析构调用者和次基类偏移分别证明真实 owner 是 `IVP_Controller`、
`IVP_Collision_Filter`、`IVP_PerformanceCounter`、`IVP_Synapse`、
`IVP_Collision_Delegator`、真正的 `IVP_Triangle` 与 `IVP_BetterDebugmanager`。修正后每个
complete destructor 都是具体 owner 指针、无显式栈参数的 `void __thiscall`；新加入的
`Audit-IvpIdbCompleteDestructors.py` 当时报告 65/65、问题 0。后续 OV、hash、memory 与
mindist 所有权入口已经一并加入，当前报告 69/69、问题 0。相关五张此前匿名的基类 vtable
也已按其真实槽位命名，而不是继续沿用源码导入名称。

构造函数采用单独规则，不能机械套用源码中的无返回值写法。MSVC x86 的 complete
constructor 实体会在 EAX 返回传入 ECX 的 `this`；主 IDB 的 93 个已命名 `??0IVP_*`
入口中，28 个原来缺失类型或只剩 `_DWORD`/整数 owner。装饰名给出显式参数类型与 const
属性，`ret N` 约束栈参数宽度，字段访问和调用点再确认顺序；修正后
`Audit-IvpIdbConstructors.py` 报告 93/93、问题 0。唯一不能完整显示的参数是 RVA
`0x10310` 的 `IVP_Attacher_To_Cores<IVP_Controller_Buoyancy> *`：IDA 9.4 legacy parser
无法解析模板拼写，因此 IDB 用 ABI 等价的 `void *attacher_buoyancy`，但函数注释明确保留
具体语义类型；这不是把参数降级成未知来源。

全命名函数类型审计随后把范围从公开接口与生命周期入口扩展到主 IDB 全部 1044 个带
`IVP_`/`ivp_` 名称的函数。十三批依据装饰名、调用清栈、字段访问与相邻调用者共同补齐
Object/Cluster、Friction System、Simulation Unit、core memory/math、Buoyancy 和
Mindist/Minimizer/Event、hash/pair 拓扑、Impact System、OV/delegator/surface collision
链、Compact Ledge/3D Solver 几何核、Linear Constraint Solver 状态机、Surface Builder、
mass center 及 polygon/tetra 的 253 项原型，空原型由 253 降到 0。RVA `0xFE50` 的 Buoyancy simulation
入口语义参数是 `IVP_U_Vector<IVP_Core> *`，但与 RVA `0x10310` 一样受 IDA 9.4 legacy
parser 的模板拼写限制，只能在 IDB 原型中保存 ABI 等价的 `void *`，并在 repeatable
comment 中保留精确语义。这里统计的是可解析 ABI 原型，不宣称 1044 项全部无需复核。

一次直接保存中断曾使主 IDB 的容器索引无法重新打开；`idat.exe` 将它表面化为初始化错误，
直接 `idapro.open_database()` 才报告数据库错误 4。此后主库不再原地应用脚本：事务包装先在
同目录临时库完成当前 878 项重放和十四项关闭后回读审计，全部成功才覆盖提交；失败路径只清理临时
库，不保留备份，也不会改变主库。

`IVP_PerformanceCounter` 的显式空构造和 Simple 的空 complete destructor 已恢复；它们
不增加字段或虚槽。Simple 的零售构造 RVA `0x14FA0` 安装 vtable `0x636B0` 后从 `+0x04`
清零 `0x29` 个 dword，确认完整 `0xA8` 布局。真实 DLL 测试还纠正了一个容易从流程名称
误判的细节：Windows RVA `0x15050` 的 `start_pcount` 只把 `counting +0x10` 设为
`IVP_PE_PSI_START`；`count_PSIs +0x14` 是 RVA `0x14FE0` 收到
`IVP_PE_PSI_UNIVERSE` 时才递增。测试不比较机器相关的 performance-counter 数值，只验证
清零、阶段切换和 PSI 计数时机。IDB 原先把 Simple vtable slot 0 标成析构，并把
`pcount` 误成返回 `__int64`；现已按六个实际目标修正为 start、pcount、stop、环境删除、
reset/print、scalar deleting destructor，同时修正五个保留函数原型。

Car System 的三项邻近接口现在也明确归为零售省略变体，而不是“待补纯虚”：Ballance
导入的 base table 为 28 槽，Raycast Car 为相同前缀加 `do_raycasts` 的 29 槽；邻近 31 槽
版本追加的 `set_powerslide`、`get_booster_time_to_go`、`event_object_deleted` 没有可放置的
Ballance 槽位。具体 Raycast Car 仍可提供非虚兼容 helper，但不能修改 base vtable。

`IVP_Multidimensional_Interpolator` 的七个公开成员现在完整归为第 2 类。Ballance DLL
没有保留任何 class-specific 函数体，因此没有可调用 RVA；但 IDB 导入 UDT 与邻近源码的
DEBUG 形态逐字段一致，完整大小为 `0x40`：两个指针表在 `+0x00/+0x04`，三个 8-bit
维度在 `+0x08..+0x0A`，运行状态在 `+0x0C..+0x28`，四个诊断数组和 exact-hit 计数器
位于 `+0x2C..+0x3C`。公开实现固定保留这一布局，不随 BML 的构建配置缩成邻近 Release
形态；所有嵌套 vector、pointer table 和诊断数组都通过 `physics_RT` 分配/释放边界配对。

算法按邻近 `ivp_multidimensional_interp.cxx` 选择性恢复：Givens 消元、残差限制、
`[-0.2, 1.2]` 权重限制、历史权重排序、FIFO 插入和 `101` 步随机替换均保持。唯一主动
修正的是邻近源码 exact-hit 分支把首权重留在 `0.0f`，会在“新输入等于首历史输入”时
返回全零 solution；这里设为数学和调用意图要求的 `1.0f`，并在浮力案例中验证复用原
solution。另一案例以浸没比例和下落速度插值 lift/damping impulse，检查安全外推拒绝、
诊断计数，以及非仿射的新求解结果确实替换陈旧样本。IDB 只补写上述证据边界注释，
没有把邻近函数名或虚构地址加入函数表。

`IVP_Statisticsmanager_Console_Callback` 也已闭合。Ballance IDB 和邻近声明都表明它只有
`IVP_BetterStatisticsmanager_Callback_Interface` 的一个 vptr，大小 `0x04`，没有附加状态；
DLL 中没有 class-specific body 或可定位的具体 vtable。其公开空构造以及三个私有 override
按完整邻近实现恢复，输出仍经 Better Statistics manager 的三槽接口分派。Host 案例把
`Ball contacts: 42` 这一实际游戏诊断计数送入 manager 并核对 console 文本；IDB 仅增加
证据分级注释，不为链接裁剪函数命名地址。

`IVP_Halfspacesoup` 和 `IVP_SurfaceBuilder_Halfspacesoup` 共七个公开方法现在完整归为
第 2 类。Ballance IDB 中前者是没有尾字段的 `0x08` `IVP_U_Vector<IVP_U_Hesse>` 派生
对象，后者是无状态 `0x01` helper；DLL 没有保留 class-specific 函数体。平面所有权、
近平行平面筛选、三平面交点、体积外点剔除和近点合并按完整邻近源码恢复，最终凸包和
surface 编译则分别进入已经验证的原版 Pointsoup 与 Ledge Soup 入口，没有复制另一版
compact builder。

邻近构造器通过 `IVP_Compact_Ledge_Solver` 从 triangle winding 取得法线再固定取反；不同
compact builder 修订的 winding 假设未在 Ballance 公共格式中形成独立契约。当前实现改用
所有顶点的内部 centroid 选择法线方向，从而直接保证公开文档要求的“法线指向封闭体积”
不变量。精确 DLL 生成的 tetra 案例验证四个平面接受内部点、拒绝外部点，较松平面被
丢弃、较紧平面替换旧平面，并能往返生成原四个顶点、四三角 compact ledge 和带有效
ledgetree root 的 compact surface。IDB 仅记录 `0x08/0x01` 布局及上述证据边界。

`IVP_Compact_Modify` 的 `chop` 和两个 `shrink` overload 也已归为第 2 类。Ballance IDB
中该类型是无状态 `0x01` helper，DLL 没有保留三个 static body；实现没有引入外版私有
结构，而是组合原版 Polygon surface manager 的 terminal-ledge 遍历、上述 Halfspacesoup
以及原版 Pointsoup/Ledge Soup。有效输入的算法与邻近源码一致；额外把空 surface、非单
ledge chop 和零方向明确处理为失败，避免邻近版 assertion/除零成为 Mod 崩溃。

原版 DLL tetra 案例验证 ledge shrink `0.1` 后所有新顶点到每个原 support plane 的距离
至少为 `0.095`；surface shrink 仍生成单个四顶点 terminal ledge；沿 `+X` chop `0.5`
后新 hull 的最小/最大 X 分别是 `0.5/3.0`。这三项只登记邻近算法与 exact-DLL 组合行为，
不登记不存在的零售函数地址。

三方差分会单独检查“原版 DLL 有精确命名函数体，但当前公开方法没有进入零售调用层”的
项目。普通方法现已清零：新增接入的短函数覆盖 point/float-point 运算、材质查询、浮力
表面、gravity/controller 查询、constraint/actuator core 集合和 active terminal 更新。
这些方法统一采用原版 RVA 优先、无零售 resolver 时执行等价本地回退，因而 Ballance
进程使用原函数，独立 SDK 测试仍可运行。

其中七项分层构造记录是 `IVP_Actuator_Two_Point`、`IVP_Constraint_Local_Anchor`、
`IVP_U_Active_Value`、`IVP_U_Active_Float`、`IVP_U_Active_Int`、`IVP_U_Active_Terminal_Double` 和
`IVP_U_Active_Terminal_Int` 的 complete-object constructor。普通 C++ 构造函数进入函数体
前已经自动构造基类；再从函数体跳入原版 complete constructor 会二次构造基类并破坏
引用计数或容器状态。因此保留七个已核对 RVA 作为二进制证据，公开构造器按已确认的
Ballance 布局选择性重建。只有找到由 factory/allocating constructor 完整拥有对象构造
序列的入口后，才能安全改成直接零售调用。

最后一轮逐 Address ID 核对还发现六个此前误走本地 fallback 的普通保留实体：
`IVP_Core::union_find_get_father`，Group Ident filter 的 check/environment callback，
Exclusive Pair filter 的 environment callback，以及 Meta filter 的 check/environment callback。
它们现分别直达 RVA `0x1C7D0`、`0x14900`、`0x14990`、`0x14B70`、`0x14CA0`、
`0x14D90`；原回退只在没有零售 resolver 的独立测试环境使用。Group Ident 案例验证同组
拒绝、异组及空组接受，Meta 案例验证对子过滤器的完整分派、AND 合并与删除清空，
Exclusive Pair 案例验证由 DLL allocator 创建后通过原版环境回调和 vtable 自删除。
因此该阶段的精确函数体审计为 372 项直接路由、23 项明确的分层构造/析构或 thunk，合计
403/403，不再有未解释的普通零售函数体。

精确 DLL 测试也必须遵守运行时前提。active manager 的延迟 PSI 调度仍依赖 Ballance
启动阶段建立的全局 active-value 环境；terminal 自身的立即通知则可在仅 `LoadLibrary`
的进程中独立成立。当前 exact-DLL 案例直接修改 double/int 当前值，调用两个原版
`update_*` 入口，验证 listener 身份、值传播和不变值去重；完整 host 依赖图案例继续覆盖
本地 fallback。listener、LU、接触几何和材质/液面案例由 CTest
分别启动进程，模拟 Ballance 对 `physics_RT.dll` 单次加载、全进程驻留的生命周期，避免
局部伪环境在同一进程中污染后续场景。

原版 incremental-LU 行内核还有一个容易被 Debug 构建掩盖的前提：RVA `0x35AE0`
从 `L_matrix` 行地址向下做 8 字节对齐后执行批量清零，Ballance 的 solver allocator 会提供
至少 16 字节对齐的矩阵/vector 缓冲。若独立调用者传入未对齐栈数组，内核可能写到数组
之前并使后续分解结果看似随机。公开结构布局不需要改动；原版运行案例现在显式使用
`alignas(16)` 缓冲，并在 Debug 与 RelWithDebInfo 两种配置验证该 ABI 前提。

`IVP_Vec_PCore` 已按第 2 类恢复。Ballance IDB 中它是纯 `IVP_U_Float_Point`
派生类型，大小 `0x10`，没有新增字段或 vptr；DLL 链接时裁掉了唯一构造器。邻近实现只对
传入的世界空间方向执行 `IVP_Core::m_world_f_core_last_psi` 的逆旋转，所用 Core 矩阵
偏移 `+0x128` 已由多条零售指令和现有布局断言确认。当前实现保留“方向不受平移影响”的
语义；Ballance 转向案例用绕 Z 轴 90 度的 core 姿态验证世界 `+Y` 正确变成 core `+X`。

Grid 家族的运行时读取和 SurfaceManager 已归入第 2 类。六个 IDB 布局与邻近源码逐字段
吻合：template axle `0x0C`、template grid `0x34`、element `0x04`、compact grid header
`0xB0`、builder `0x440`、surface manager `0x08`。原 DLL 没有保留 Grid 类函数体或具体
虚表，因此 header accessors、template 清零和 13 槽 SurfaceManager 由邻近源码重建；
其中双面 triangle 的精确距离门仍调用原版保留的
`IVP_Compact_Ledge_Solver::calc_qlen_PF_F_space`（RVA `0x21550`），没有换成本地近似算法。

原版 DLL 案例把两个原版 Pointsoup 双面 ledge 复制进符合 `0xB0` header、element 表和
ledge offset 表的连续 terrain 数据。局部查询会对同一 cell 的重复 ledge index 去重，
返回地面附近的一个 ledge，并借助 RVA `0x21550` 拒绝正上方十米的同一双面 triangle；
全量 terminal 查询返回两个不同 ledge。质量中心、半径、惯量、surface 类型及借用所有权
也一并验证。

`IVP_GridBuilder_Array` 的两个公开方法现已归入第 2 类。编译器保留确认过的轴映射、逆
变换、包围盒中心/半径、16 字节对齐和相对 offset 表，但没有照搬邻近版本约 900 行的
凸条带优化器；每个 cell 被稳定拆成两个三角形，并逐个交给原版 Pointsoup RVA 构造，
最后复制进单块 `IVP_Compact_Grid` 分配。3×3 上升斜坡案例同时验证反向 row 轴、非零
XYZ 原点、八个原版 compact ledge、逐 cell 引用和真实半径查询。

早期重建虽然用 `std::byte[0x440]` 保住了 builder 尺寸，却没有表达 Ballance 已导入且与
邻近源码一致的私有布局。现已拆回从 `n_rows +0x00`、memory/point 工作区到
`c_point_to_point_index[516] +0x34`、`triangle_count +0x43C` 的真实字段，并增加关键偏移
断言。它不改变上述保守转换算法，也不意味着存在可调用的零售 builder 构造函数。

这带来一项明确而受控的差异：邻近实现会把连续凸三角形合并成 strip，本实现优先保持
碰撞几何正确，因此输出通常更大，也受有符号 16 位 ledge index 上限约束；预计超过
32767 个三角 ledge 时明确返回空，而不是截断索引。另一个有意修正是把
`position_origin_os` 的 height 分量加入顶点；邻近构造器只把 origin 用于 row/column
分量，却又在 grid 逆变换中使用完整 origin，非零高度原点会造成查询空间与 ledge 空间
不一致。当前行为遵循“数组第一个元素位于 position_origin_os”的公开说明。

Concave Polyhedron 的安全子集也开始恢复。Face 是 `0x08` 指针向量，单个 offset 是
`0x04`，Polyhedron 是两个 BigVector 组成的 `0x18`，Convex Subpart 是 `0x08`，参数是
`0x0C`，builder 无状态且为 `0x01`；这些均与 Ballance IDB 逐字段一致。当前已实现 Face
的 index 所有权、Convex Subpart 的 point 所有权，以及把每个索引面交给原版 Pointsoup
编译的 face-soup 路径。精确 DLL 案例用 floor+ramp 两个三角面生成两个 terminal ledge，
不是仅测试容器操作。

这里有一项明确的邻近源码偏差：公开注释要求 `add_offset` 丢弃重复索引，但邻近 `.cxx`
无条件插入。当前实现遵循公开契约并去重；否则重复顶点会直接进入凸包构建。

`IVP_Convex_Decompositor` 和 Concave builder 剩余两个 adapter 现也归入第 2 类，但没有
把外版 GEOMPACK Fortran/C 适配层整体搬入。Ballance IDB 只确认 decompositor 是
`0x01` 无状态 helper，DLL 中没有 GEOMPACK 实体或私有 workspace 可供复用。当前算法先
用所有外向逆时针面的半空间检验输入，并要求每个 face 共面且自身凸、每条有向边恰有
一条反向配对：完整凸体直接保留为一个 pointsoup；凹体只有在顶点平均中心被证明位于
每个面后方时，才将各边界三角形与该 kernel 点连接成无重叠四面体。若无法证明为这种
star-shaped 闭合体，则返回 0 且不产生部分结果。

原版 DLL 案例使用一个封闭方形平台，顶面中心向内下凹 `0.2m`。14 个边界三角形被分成
14 个四点凸 subpart，逐个进入原版 Pointsoup，随后通过原版 Ledge Soup 合成可由 Polygon
manager 枚举 14 个 terminal ledge 的 compact surface。把凹点移到地板以下会使中心离开
至少一个面半空间，测试确认此时安全拒绝。与邻近 GEOMPACK 的差异是：后者意图覆盖一般
非 star-shaped 多面体，当前重建不声称这一能力；这是为了保持输出几何可证明正确，而不
用 face soup 或可能重叠的猜测分块冒充凸分解。

3DS 碰撞网格的两个公开入口也归入第 2 类。Ballance IDB 中 template 是仅含 `scale`
的 `0x04` 结构，builder 是 `0x01` 无状态结构；DLL 没有保留这两个函数体。当前实现只
重建附近源码实际消费的 chunk：main/object-mesh/object/tri-mesh、vertex list 和 face
list。多 object 顶点按缩放后的精确坐标合并，面索引调整到全局 point vector，并保留
附近实现的 T-junction 边插点语义。解析器对所有 chunk 长度、有限浮点数和 face index
做边界检查，不引入附近版本的进程级 parser 函数指针、`setjmp` 状态、材质/贴图输出及
命令行转换器。

原版 DLL 案例生成一个封闭四面体 3DS，以 `2.5` 缩放导入为四点四面，再交给已恢复的
Concave adapter；最终几何由原版 Pointsoup 和 Ledge Soup 编译，Polygon manager 确认
只有一个四点 terminal ledge。把同一文件的 face index 改到点数组之外时，导入明确
返回空且不留下部分 polyhedron。与附近源码的受控差异是：零面积面会在精确坐标合并后
被丢弃，畸形文件不会进入其原本依赖全局状态和 `longjmp` 的旧解析路径。

Q1/Q2 BSP builder 的 7 个公开方法现全部归入第 2 类。IDB 中完整 builder 是 `0x6C`：
四组 BSP count/pointer 到 `+0x20`，两个 sentinel 与 ownership flag 位于
`+0x24..+0x2C`，转换参数和统计到 `+0x5C`，`IVP_U_Vector<intp>` 路径为
`+0x60`，Halfspacesoup 指针为 `+0x68`。相关公开磁盘结构也逐字段吻合：model
`0x40`、lump `0x08`、header `0x7C`、plane `0x14`、node `0x18`、clipnode
`0x08`。DLL 中没有对应函数体。

重建保留附近算法的 Quake `X/-Z/Y` 坐标映射、模型边界扩张、BSP 路径半空间、缩放、
逐面 shrink 和 point-merge 语义，最后仍调用原版 Pointsoup 生成每个 solid brush，并
调用原版 Ledge Soup 合成 surface。测试中的六平面 BSP brush 以 `0.5` 缩放并内缩
`0.1m` 后，原版 ledge 实测为 8 点且三轴边界均为 `±0.9m`；同一数据写成 version-30
BSP 的真实 model/plane/node lumps 后，又通过磁盘加载路径生成可枚举的单 ledge compact
surface。

相对附近实现的安全差异是：文件 lump 必须位于文件内且长度为元素大小的整数倍，版本、
数组数量、model/node/plane index、有限缩放和递归环均被检查；无有效 brush 时 surface
转换返回空，不把空 Ledge Soup 交给 DLL。`unload_q12bsp` 对外借数组只解除引用、不释放，
同时清空内部指针和 count，避免附近实现卸载后仍残留可误用的 stale pointer。

MOPP 必须按层拆开，不能把附近 Havok 目录视作一个整体接口。Ballance IDB 只保留了
`IVP_Compact_Mopp` 的完整 `0x30` header：mass center `+0x00`、inertia `+0x0C`、
radius `+0x18`、MSVC `8/24` deviation/byte-size bitfield `+0x1C`，以及 root、ledge
array、convex hull size 三个 offset/size 到 `+0x28`。这些字段与附近 public header
逐项一致，因此 `get_size`、相对 root accessor 和 header `byte_swap` 已按第 2 类恢复。
独立 big-endian 解码案例验证 float、`0x7F/0x012345` packed field 和三个 offset，未把
同类 Compact Surface 的结果直接当断言。

但 IDB 和 DLL 均没有 `hkMoppCode`、`IVP_SurfaceManager_Mopp`、
`IVP_SurfaceBuilder_Mopp` 类型、虚表或函数实体，附近树也只有三份声明 header，没有
对应实现。特别是 `byte_swap_all` 必须解释并重写 Havok MOPP bytecode；只交换 `0x30`
header 和 compact ledges 会生成表面上合法、实际损坏的数据。因此该方法目前明确
`= delete`，manager/builder 也暂不伪造布局。这是结构确实不受 Ballance DLL 支持的窄
第 3 类边界，不影响普通 Polygon/Grid/Pointsoup/Ledge Soup 路径。

Real Wheels 车辆系统现已按 Ballance 的结构拆掉后期版本成分后归入第 2 类。IDB 中
`IVP_Car_System_Real_Wheels` 是 `0x450`：environment/count 到 `+0x0C`，body/wheel、
constraint solver 和 actuator 数组到 `+0xA7`，两个持久 force 位于 `+0xA8/+0xAC`，
wheel lock、半径/反向标记和调参状态到 `+0x133`，booster/timer/steering 到 `+0x147`，
最后是 Ballance 较短的 `0x308` debug block。邻近版本在两个持久 force 后插入
`car_act_powerslide_back/front`，并在基类增加 `set_powerslide` 虚槽；Ballance 两者都
不存在，因此没有照搬该方法。邻近的两参数 `do_steering(float,bool)` 在 Ballance 是
单参数虚槽，`get_booster_time_to_go` 也只作为不改变虚表的非虚便利查询提供。

IDB 原先还把 concrete vtable 的 slot 0 误标成第二个 `GetCarSystemDebugData`，同时在
slot 27 又保留真正 getter。主库现已修成 slot 0 虚析构、slot 27 getter、slot 28
protected environment-deletion callback；四个 `IVP_DOUBLE` 查询的错误 `long double`
拼写也改为 x86 MSVC 的 64 位 `double`。这些仍标为导入类型证据，因为 DLL 没保留车辆
实体或虚表地址。

完整 host 案例不是 reachability probe：它创建 1200 质量车体、四个 wheel core、四个
suspension、五个 torque、前后 stabilizer、downforce/extra-gravity actuator 和四轮
constraint solver；随后改变压缩阻尼、body-force cap、预紧量、稳定杆、fast-turn 和
单轮 damping，核对单轮角速度以及早期空 throttle/graphics/debug hook，再验证 1.6 轮距、
2.6 轴距、12 的前进速度、Ackermann 外轮角、0.6 倍车身 yaw feedback、四轮总扭矩
810 产生的 -283.5 反扭矩、滑移接触读回，以及 booster 的点火、倒计时和 actuator 自动释放。
同一案例还执行右后轮 handbrake：创建模板时只有 RX 固定、三平移和 RY/RZ 保持自由，
reference/attached 分别是 wheel/body；锁定前把车身角速度的 25% 加入轮体以减轻瞬时惯性，
重复锁定不重复建约束，解锁先清 solver 引用再销毁并注销约束。

该夹具必须满足真实运行时的 Cache Object 前置条件。Ballance 的内联
`IVP_Real_Object::get_cache_object_no_lock()` 在 simulated object 上假定 cache manager 已经
提供有效对象；它不会在 manager 返回空指针后继续做防御检查。测试现为车体、静态对象和
四个轮体分别建立 `IVP_Cache_Object`，保持 object backlink、time code、世界变换和 core
位置一致，并让 `CacheTransformPositionToWorld` 适配执行同一矩阵变换。这样车辆构造中的
anchor 位置确实来自各自的 Ballance 布局 cache，而不是依赖空 manager 或未初始化输出；
Debug 与 RelWithDebInfo 下完整 IVP scoped suite 均为 39/39 通过。

`IVP_U_Quat` 的十个裁剪/inline 数学入口现在有统一的姿态案例。案例把物体局部 `+Z`
转到世界 `+X`，核对单位四元数、逆/除法组合、线性插值、半角诊断和 float 差值估计。
需要特别保留早期 IVP 的 transport 约定：`double[4][4]` overload 注释明确按 OpenGL
column-major 解释，因此 `set_matrix` 写入 C 数组后不转置就交给 `set_quaternion` 会得到
共轭姿态；这不同于零售保留的 `IVP_U_Matrix3` overload，不能为了现代直觉改掉符号。

`IVP_Template_Constraint` 的 34 个公开候选方法现全部具有 host 行为覆盖。机制配置案例不只
检查调用可达：它在同一六轴表中组合 TX free、TY fixed、TZ limited 与 RX fixed、RY free、
RZ limited，分别验证平移/旋转 bulk 和 indexed impulse policy，以及 reference/attached
内嵌矩阵的复制和 selector 清除语义。球铰、万向节、铰链、有限铰链、姿态约束和固定约束
分别核对为 `3T/0R`、`3T/1R`、`3T/2R`、`3T/2R + RZ bounds`、`0T/3R`、`3T/3R`，并
区分 world-space 与 reference-object-space 的保留 DLL 入口。

其中 tense ball socket 不是简单转发 point：附近 inline 会在栈上创建 `IVP_U_Matrix`，调用
保留的 `IVP_U_Matrix3::init3` 建立单位旋转，再把 distance 写到 translation。隔离测试若不
解析该 RVA 会留下零矩阵，产生看似可达但不可用的假阳性；当前适配显式绑定该原版依赖，并
验证 `(-0.5, 1.25, 2)` 位移与单位对角线。这一行为与附近源码及当前 DLL 依赖契约一致，
没有为裁剪 helper 编造新入口。

Local constraint 构造路径又发现一处必须拆层的 ABI 问题。RVA `0x28270` 并非可从普通
C++ 构造函数体调用的 init：指令明确先调用完整 `IVP_Constraint` 构造器，再分别对
`this+0x70`、`this+0xF8` 调用两个 `IVP_Constraint_Local_Anchor` 构造器，设置两组 identity
mapping，最后才调用原先匿名的 RVA `0x284D0` 和 `activate`。旧包装在 C++ 已自动构造这些
子对象后再次进入 `0x28270`，等同二次构造 base/vector/anchor；即使当前 inline vector 尚未
分配堆内存，也不是可接受的对象生命周期 ABI。

最初的安全修正曾把两个使用场景分开：公开构造由 C++ 建立子对象后只进 `0x284D0`，raw
factory 才进完整 `0x28270`。后续 storage-only 审计消除了这一限制：inline two-Core
vector、两个 anchor 和两个 mapping 均已换成 union 存储，公开构造和
`create_constraint_any_solver` 现在都安全进入完整 `0x28270`。`IVP_Constraint` 自身也从
storage-only vector 进入 `0x375B0`，不再由 wrapper 手写 enabled 状态。host 生命周期案例
对直接对象和 raw factory 各确认一次 base、两次 anchor、两组 `0,1,2` mapping、一次
`0x284D0` 和一次 activate，两个对象最终各注销一次。

主 IDB 已直接把 `0x284D0` 命名为中性的
`ivp_constraint_local_initialize_from_template`，原型按 RET 4、唯一 stack 参数和全部字段访问
修为 `void __thiscall(IVP_Constraint_Local *, const IVP_Template_Constraint *)`。这不是恢复一个
并不存在的公开方法，而是标明完成正确分层构造所需的零售内部入口。

`IVP_Template_Real_Object` 也必须按同一原则拆分完整构造。零售 RVA `0x15EF0` 在
`0x15EF4` 首先调用 `IVP_Template_Object::IVP_Template_Object`（RVA `0x15E90`），随后清零
完整 `0x70` 字节对象并写入默认 mass、inertia、damping 和 radius。旧公开包装先由 C++
自动构造基类，再从派生构造函数体跳入 `0x15EF0`，导致基类构造执行两次。当前使用只提供
存储的内部 construction tag 跳过 C++ 侧基类实体，再让 DLL 完整构造体独占这条构造链；
直接构造 `IVP_Template_Object` 时仍调用原版 `0x15E90`。同时移除 `name` 的成员预初始化，
避免进入 DLL 前写对象；仅在没有零售绑定的显式 fallback 中把它置空。公开布局和签名均未变。

同批复核确认 `IVP_Template_Spring` 的 RVA `0x14200` 在 `0x14204` 调用
`IVP_Template_Two_Point` 构造体 `0x13E70`，再清零完整 `0x38` 字节并写入
`break_max_len=1.0e20f`。它原来也从普通派生构造函数体进入完整 DLL 构造体，造成 Two Point
基类重复构造。两层模板现使用相同的 storage-only tag：有 DLL 时由 `0x14200` 独占完整链，
无绑定的 host fallback 才显式写默认字段。领域测试模拟完整 DLL 的内部基类调用并断言
Spring 和 Two Point 各构造一次；旧路径会稳定得到 Two Point 两次。

`IVP_Hull_Manager_Base` 的 RVA `0x1A740` 同样是完整构造体：它先清零 Gradient 的
`+0x00..+0x1F`，再在 `+0x20` 构造 Min List。旧包装虽然已经用匿名 union 避免 C++ 重复
构造 Min List，却仍由 `IVP_Hull_Manager_Base_Gradient()` 在进入 DLL 前清零
`last_vpsi_time +0x00`。Base 现通过 Gradient 的 storage-only tag 进入 DLL，直接构造
Gradient 时仍保留原来的时间零值。poison-storage 门禁在 RVA adapter 被调用的瞬间检查
前八字节仍为 `0xA5`，确保以后不会重新引入隐藏预写。

Active terminal 的完整构造链此前也被过度保守地留在本地复现。零售 Double 构造体
`0x15C50` 明确先调用 `IVP_U_Active_Float` 构造体 `0x15A00`，随后安装 primary/secondary
vptr，并把输入 double 写到 `+0x20` 和 `+0x30`；Int 构造体 `0x15CC0` 同样调用
`0x15A80`，再把输入 int 写到 `+0x1C` 和 `+0x24`。Float/Int 构造体又调用共同的
Active Value 构造逻辑，并在 `+0x0C..+0x14` 建立 dependency vector。因此不能让 C++
先构造 Value 和 vector 后再跳完整 terminal RVA。

当前 Float/Int 提供只占用存储、不触碰名称与 vector 的内部 construction tag，dependency
vector 放入匿名 union 抑制宿主隐式生命周期；terminal 构造函数由 `0x15C50`/`0x15CC0`
独占完整链。直接构造 Float/Int 时仍分别进入 `0x15A00`/`0x15A80`，而无零售 resolver 的
host 测试路径才使用逐字段复现。直接构造 Active Value 也已接回保留的 `0x15980`；只有
被 Float/Int 完整构造体嵌套时才使用 storage-only 路径，避免重复分配名称。析构侧由本地等价层清理 vector，再让共同 Value 析构释放
名称，避免从 C++ 派生析构体跳进会再次析构基类的零售 complete destructor。真实 DLL 的
control-value 场景已覆盖构造、监听器依赖、setter、secondary-base update 和名称释放。

`IVP_Actuator_Two_Point` 也已从保守的本地复现改为调用完整零售构造体 `0x14020`。
该 RVA 首先调用 `IVP_Actuator` 构造体 `0x13F60`，由后者在 `+0x04` 建立 controlled-core
vector；随后通过 MSVC array-constructor iterator 在 `+0x0C/+0x3C` 开始两个 `0x30`
Anchor 的生命周期，才初始化 anchor、筛选可移动 core、登记 controller 并写入
`client_data +0x6C`。因此普通 C++ 基类和数组成员构造后再跳进去会重复建立三组子对象。

当前 Two Point 使用 storage-only Actuator tag，Anchor 数组放入匿名 union，由 DLL 完整
构造体独占上述链；没有 resolver 时的 fallback 才显式 placement-construct 两个 Anchor、
初始化 controlled-core vector，并执行相同的去重/announce 规则。析构仍在本地复现 complete
destructor 的分层语义：先从 controller manager 注销，再逆序析构两个 Anchor，最后让
`IVP_Actuator::~IVP_Actuator` 释放 vector，避免从派生析构函数体进入 `0x14150` 后又被 C++
重复析构基类。力执行器领域测试的零售 adapter 会在 `0x14020` 模拟体内调用一次
`0x13F60`，并锁定两层构造计数均为一、两个 Anchor 析构各一次。

Constraint 三个原先为避免重复构造而保留的本地分层也已完成拆解。`IVP_Constraint` 的
inline two-core vector 现在位于匿名 union；直接构造从原始成员存储进入 `0x375B0`，由该
RVA 设置 `capacity=2/count=0/elems=this+0x10`、安装 vptr 并置 enabled bit。作为 Local
基类时则使用 storage-only tag，让 `0x28270` 内部对 `0x375B0` 的调用独占这一步。

`IVP_Constraint_Local_Anchor` 的公开构造直接进入 `0x28210`，仍只清空 `rot +0x84`、保留
`object +0x80`。Local 的两个 Anchor 与两个三字节 Mapping 全部改用 union storage，因此
公开 `IVP_Constraint_Local(template)` 和 raw-memory factory 都可安全进入完整 `0x28270`：
一次 Constraint base、两次 Anchor、两组 `0,1,2` identity mapping、一次 `0x284D0`
initializer 和一次 `activate`。无 resolver fallback 显式复现同一顺序。析构继续采用分层
本地实现释放 maxforce/rotation、再注销 controller 和清理 inline vector，避免 complete
destructor 与 C++ 基类析构重叠。Host 测试同时执行直接对象和 factory 对象并核对每个对象
的构造计数与最终注销次数；exact-DLL poison 案例则通过公开 Anchor 构造验证只写 `+0x84`。

canonical IDB 同步原位更新了 `0x100375B0`、`0x10028210`、`0x10028270` 的 repeatable
comment，明确公开 API 与 raw factory 现使用相同完整构造链。此次只改三条已验证注释，没有
运行待处理的全量 UDT 修订，也没有创建数据库备份；回读仍识别 93 个具名完整构造器且原型
问题为 0。

## OV Element 宽相生命周期

原 API 只给 `IVP_OV_Element` 留了字段布局和默认析构，实际会错过宽相连接器的完整生命周期。
原版构造器 RVA `0x2DC00` 先安装 construction vtable `0x10063A14`，在 `+0x28` 构造容量
16 的 `IVP_U_FVector<IVP_Collision>`，再安装最终五槽 vtable `0x10063A00`。该表依次为
`get_type 0x2DDA0`、`hull_limit_exceeded_event 0x2DDC0`、匿名 hull-manager 删除回调
`0x2DDB0`、基类 reset no-op `0x1A810` 和 scalar deleting destructor `0x2DC50`。
构造器、字段访问和析构链共同确认对象大小 `0x30`，listener min-list index 在 `+0x04`，
node/hull manager 在 `+0x08/+0x0C`，center/radius 在 `+0x10/+0x20`，Real Object 在
`+0x24`，collision fvector 在 `+0x28`。

RVA `0x2DD00` 虽无原装饰名，但两个 `IVP_Mindist_Manager::recheck_ov_element` 调用点、
`ret 0x0C`、Real Object `+0x18` 的 Environment、Environment `+0x120` 的当前时间，
以及 Hull Manager `0x30180/0x30470` 的 insert/update 调用共同确定它就是
`add_to_hull_manager(IVP_Hull_Manager *, IVP_DOUBLE)`。它首次登记后保存 manager，后续只
更新同一 min-list 元素。`0x2DDE0/0x2DE30` 则直接维护碰撞 fvector；remove 采用尾元素
无序压缩，并修复幸存 collision 的反向索引。

完整析构 RVA `0x2DC70` 先移除 hull event，再经 Environment 通知所有 collision
delegator，最后从 OV tree 移除节点并释放 fvector；`0x2DDB0` 通过最终虚表的 deleting
destructor 删除自身。邻近源码在这些行为上与零售体一致，但它位于内部 broadphase header，
不属于 1575 个 `IVP_EXPORT_PUBLIC` 候选，因此没有被错误计入公开候选完成率。兼容层仍需
公开它，因为 Real Object 和 Mindist 的实际原版 API 会传递、创建和销毁该类型。

当前包装直接进入上述八个原版实体，并用 inactive union storage 避免宿主先构造/后销毁
`+0x28` fvector；class-specific `new/delete` 保证最终 deleting destructor 回到原版
MSVCRT 堆。精确 DLL 案例在 Debug 与 RelWithDebInfo 中验证最终 vptr、两个碰撞连接器的
反向索引压缩、Hull Manager 首次插入/再次更新只保留一个事件，以及 manager 回调完成跨
DLL 删除并清空 min-list。IDB 校正脚本已为四个匿名函数体（含完整/删除析构）、最终 vtable
加入带机器码门禁的名称、原型与生命周期注释；canonical IDB 仍待用户的另一 IDA 会话关闭
后由事务脚本提交。

同一 broadphase 类群中的 `IVP_OV_Tree_Manager` 也已从“只有几个本地 accessor”恢复为原版
生命周期接口。完整构造 RVA `0x2DFF0` 在 `+0x288` 构造 `IVP_OV_Node`，将
`collision_partners/hash_table/environment/root` 放在 `+0x2B0/+0x2B4/+0x2B8/+0x2BC`，
以零售堆创建 256 槽 hash，并填满 `+0x000..+0x287` 的 81 个 binary64 二次幂；对象总大小
由此固定为 `0x2C0`。完整析构 RVA `0x2E0E0` 通过 helper 的虚表删除 hash，再恰好一次
销毁嵌入 node。包装因此将 `search_node` 保持为 inactive union storage，并让 class-specific
`new/delete` 跨 DLL 边界使用同一零售堆。

这里还修正了一个容易反向归属错误的虚表：`IVP_OV_Tree_Manager` 是非多态类；构造器在
单独分配的 `IVP_ov_tree_hash` 上写入的 `0x10063A24` 才是两槽虚表，槽 0 为
`compare 0x37DD0`，槽 1 为 scalar deleting destructor `0x2E0C0`；后者调用完整析构
`0x37D80`，再按 flags 位从零售堆释放。IDB 脚本现以构造写入、析构 vptr 恢复和两槽目标
同时锁定该 owner，不再依据“被哪个构造器引用”误标为 Manager。

公开可调用部分严格保持邻近声明的四项：构造、析构、`insert_ov_element 0x2EC10` 和
`remove_ov_element 0x2EEC0`。`power2` 与坐标 helper 仍是 private inline，之前仅为测试
加入、但邻近接口根本不存在的 `get_root()` 已移除；测试改由只读 raw-layout view 检查
根指针，不污染 API。真实 Ballance DLL 案例插入两个同位置半径 1 的移动对象，确认二者
共享节点、第二次插入返回包含已有对象与自身的候选集，逐个移除后节点和 root 被清理，
最后由精确析构释放 hash 与树。

`IVP_OV_Node` 也不能继续依赖隐式宿主生命周期。零售构造 RVA `0x2DEA0` 故意保留
`data +0x00..+0x13`，只清空 `parent +0x14` 并建立 `children/elements +0x18/+0x20` 两个
vector；private 完整析构 `0x2DEF0` 先从 parent 的 children 中移除自身，再递归删除子节点，
最后释放两个 vector，但不删除 elements 指向的 OV Element。兼容类现恢复同样的 public
constructor/private destructor 可见性，并把两个 vector 改成 inactive union storage，避免
宿主和 DLL 重复构造/析构。host poison-entry 证明进入 constructor 前整个 `0x28` 对象未被
预写；真实 DLL 证明 key 字节保持、parent/vector 状态正确，Tree Manager 案例则覆盖节点
实际创建与清理。加入 Manager 四项、Node 两项、Spring 四项以及 Mindist Manager 完整
构造/析构后，Retail Contract 当时为 664 个 code RVA、660 个直接依赖（659 typed
wrapper、1 adapter-internal）。

## 完整构造/析构与宿主预写差异

对完整公开 umbrella 做 constructor-initializer AST 审计后，又发现一类不是布局偏移、而是
C++ 对象生命周期先后顺序造成的 ABI 偏差：包装构造函数虽然从函数体调用正确的零售 RVA，
但进入函数体前，宿主编译器已经执行了成员默认初始化器或非平凡成员构造。原版 complete
constructor 随后会在同一地址再次建立对象，形成重复构造；析构方向也可能重复释放。

基础容器首先按邻近声明恢复为原始存储语义：`IVP_U_Vector_Base` 与
`IVP_U_BigVector_Base` 不再带三字段默认初始化器，只有公开的具体 `IVP_U_Vector`、
`IVP_U_FVector`、`IVP_U_BigVector` 构造函数显式建立空容器。这不会改变独立容器的行为，
但保证 `IVP_U_Active_Value_Manager` 等由 DLL 完整构造的 owner 不会在入口前被宿主清零。
编译期断言固定两个 base 仍为 trivially default constructible。

逐条指令复核确认以下 complete body 自己拥有成员生命周期：Better Statistics
`0x2DB50` 构造 `+0x04/+0x0C` 两个 vector；Meta Collision Filter `0x14D40/0x14CE0`
构造并销毁 `+0x08` vector；Friction Core Pair `0x1D340` 只构造 `+0x00` vector，仍故意
保留 `span_vector_sum +0x08` 与尾部 core 指针不写；Ledge Soup `0x38290/0x38340`
构造并销毁 `+0x20/+0x28/+0x30/+0x64/+0x6C/+0x94` 六个 vector。对应成员全部改为
匿名 union 的 inactive storage；无零售 resolver 时才由 fallback placement-construct，
有原版 DLL 时则由 complete body 独占构造和析构。

Buoyancy Attacher 的问题更跨两层。`0x104C0` 在 `+0x04` 构造 `IVP_VHash_Store(16)`，
初始化 active-set base 的 `+0x18` 指针，再把 19 个 dword 的 `IVP_Template_Buoyancy` 复制到
`+0x1C`，最终写 `+0x68/+0x6C`。旧 storage tag 仍先构造空 hash，派生成员默认构造又先写
整块 template。现在 hash 和 template 都是 inactive union storage，host poison-entry 测试
确认除编译器必需的临时 vptr 外，`+0x04..+0x6F` 在进入完整 RVA 时仍全部为 `0xA5`。
完整析构 `0x10650` 也已加入 Retail Contract 并由公开析构直接调用；资源为空的本地 base
析构不再重复注销 listener 或销毁 hash。

同轮还移除了 `IVP_U_String_Hash`、`IVP_Material_Manager` 和
`IVP_Collision_Filter_Exclusive_Pair` 的入口前字段预写，并以 raw-storage adapter 检查
`0xB7F0/0xBDA0/0x14B80` 接收到未修改字节。`IVP_Material_Simple` 继续保持
`0xBF00` 的精确部分写集，`IVP_Template_Real_Object` fallback 则按 `0x15EF0` 的完整
`0x70` 写集显式复现。真实 DLL 还验证了 Ballance surface-name hash 的 add/find/remove、
pair filter 的 disable/enable 幂等性、Ledge Soup pointsoup 编译以及上述对象的销毁路径。

Retail Contract 因补入 Meta Filter 与 Buoyancy Attacher 两个保留完整析构，先从
628/218 扩展为 630/220。随后重新反汇编公开审计中的 20 个非直连精确析构，确认其中八个
base layer 可以安全直达完整零售体：Material `0xBE00`、Collision Filter `0x148D0`、
Performance Counter `0x14F70`、Active Value `0x159D0`、Synapse `0x161D0`、
Surface Manager `0x22160`、Collision Delegator `0x2F570` 和 Better Debug Manager
`0x60C70`。前七个无非平凡 base/member 自动析构；Active Value 的零售体自行释放 name，
Float/Int 派生层仍先各自销毁 vector，再由 C++ 恰好进入一次该 base。对应 host 计数测试
要求八个精确入口各执行一次，真实 DLL 案例也覆盖 Mod 侧栈对象的完整析构链。

不能直连的 12 项继续保留：Spring Active/Spring/Two Point、Cluster、Group Ident Filter、
Constraint/Constraint Local、Material Simple、Surface Manager Polygon、Active Float/Int
均会由 complete body 再进入成员或基类销毁，直接嵌套会重复；Template Object 已通过精确
析构 thunk；Mindist Manager 的非多态 complete body 可以安全直连。继续加入
`IVP_Hash`、`IVP_U_Memory`、`IVP_U_Matrix_Cache` 和 `IVP_3D_Solver` 后，再接回上述四个
Environment object-event dispatcher 与 debug-vector 完整删除实体，当前 Retail Contract 为
685 个 code RVA、681 个直接依赖（680 typed wrapper、1 adapter-internal），公开精确函数体为
397 项直连加 12 项分层/thunk，共 409/409。

旧的 228 项直接依赖只是人工挑选的子集，不能证明公开调用层闭合。新的反向审计从
`include/BML/IVP` 全部头文件实际出现的 `Address::*` 出发；加入 Hash、事务内存和
Matrix Cache、3D Solver、四个 object-event dispatcher 与 debug-vector 删除实体后得到
681 个 ID 和 680 个唯一
RVA；唯一别名是 `SimulationUnitEnsureCoresMovement/ClearMovementChecks -> 0x120B0`。
这次先补账 405 个此前已有包装却未进入清单的唯一 RVA，并把正向 contract 与反向 header
集合设为严格相等。审计同时发现旧子集没有检查到两个空 IDB 原型：RVA `0x16E30` 的
`IVP_Mindist_Manager::insert_exact_mindist(IVP_Mindist *)` 由 ECX、`ret 4`、双 synapse
链入和邻近实现共同确认；RVA `0x1DA10` 的 `p_strdup(const char *) -> char *` 由装饰名、
plain `ret`、null 分支以及 `strlen+1` 后调用零售 allocator 的机器码确认。两者现已写回
主 IDB。六个没有原版装饰名、但已有独立函数边界和原型的公开 RVA 继续保持中性匿名标签，
没有为了让符号 manifest 看起来完整而伪造名字。

继续检查原先五个“有 Address、无头文件调用”的条目时，`ivp_rand` 不能归入内部项：邻近
`ivu_types.hxx` 明确把 `ivp_rand/ivp_srand/ivp_srand_read` 列为公开自由函数。Ballance
只保留 `ivp_rand` RVA `0x2FCD0`，其装饰码 `M` 和调用者的 32 位落地都证明返回
`IVP_FLOAT`；旧 IDB 的 `double()` 类型错误。`ivp_srand` 与 `ivp_srand_read` 没有函数
边界，但 retained body 在 RVA `0x685B4` 读写 DLL 自有的 `IVP_RAND_SEED`，布局足以
支持最小重建。因此当前 `ivp_rand` 直接进入 DLL，两个 seed accessor 只访问同一 DLL
全局，不另建 Mod 侧状态。精确 DLL 案例用 seed 12345 验证乘 75、低 16 位缩放、读回
状态和 seed 0 归一化为 1，并在退出前恢复原状态。剩余四个未公开 Address 均有明确
private/protected 装饰访问级别：Environment PSI、两个 Mindist hull helper 和 VHash Store
rehash，不应为了数字而提升可见性。

同一轮补回的 `P_List<T>` 和 `P_String` 不伪造 DLL 实体。`P_List<T>` 是邻近公开头中的
header-only 双字段链表，x86 专门化固定为 `first +0x00`、`len +0x04`、总大小 `0x08`；
Ballance controller-list 领域案例验证头插、前后链修复和长度。`P_String` 是无实例状态的
静态工具，`find_string/string_cmp/uppercase` 按邻近源码的 ASCII 大小写、`?`/`*` 匹配规则
选择性重建；surface 名称和 controller 标识案例验证实际结果。两者的 owner 布局证据记为
not-applicable，不因测试通过而冒充零售函数体或 IDB 布局。

公开 C 风格 utility 也由 `Audit-IvpPublicFreeFunctions.py` 单独审计。34 项候选中 8 项直达
零售 DLL，21 项只在不依赖未知引擎状态时按邻近算法重建，`ivp_calloc_aligned` 由相邻版本
和同一零售 aligned allocator 元数据共同约束，4 项缺少可证明语义或必要结构而保持省略。
`p_free`、`p_strdup`、`p_make_string` 和 token/string 返回值继续在原版 MSVCRT 堆边界内
配对，不能把 Mod 的 UCRT 分配结果交给零售释放入口。

IDA 9.4 的短生命周期 `idapro` 进程偶发在前一个审计刚关闭压缩数据库时返回
`OPEN_RC 4`；本轮还观察到一次临时副本以未初始化 image base 打开。事务原有的 image-base
守卫在任何写入前即拒绝了后者，两个失败都没有触碰 canonical IDB。事务和只读包装器现对
明确的 `OPEN_RC != 0` 做至多四次短退避重试；事务仅额外重试精确的
`REFUSED unexpected image base` 前置守卫，其余 DLL 身份、字节锚点或 ABI 守卫仍首次失败即
终止。这样不会把真实不兼容误当成瞬态锁，同时避免人工反复重跑同一安全事务。

主 IDB 在八个 complete body 上保存 `[BML complete-object lifetime]` 注释，并在上述八个
新直连析构上保存 `[BML direct complete destructor]` 注释；两个独立事务审计逐项回读。
当前 IDB SHA-256 为
`6E1533C73BCB8757EEEC5D8D17DAAA2C3C0B934AC94E780FF8053B353F5F0A79`。umbrella AST 审计
对“调用保留 complete constructor 且自动执行 user-provided member constructor”的组合返回
0 项。Debug 与 RelWithDebInfo 的 39 个活动 IVP 测试均通过。

## 位域与压缩状态字

邻近声明的位域语法不能单独证明 Ballance 的 MSVC x86 分配结果，IDA 导入类型也可能把
同一存储显示成普通标量。因此当前把声明、编译器布局和 IDB 存储分开审计。公开头共有
10 个 owner、22 个真实 C++ 位域；`Audit-IvpBitfieldAbi.py` 让 Clang 以
`i686-pc-windows-msvc` 规则输出每一项的 byte/bit 位置，再与逐项整理的 ABI manifest 比较。

关键差异是 `IVP_Simulation_Unit` 的 `IVP_Movement_Type : 8` 与随后三个 `IVP_BOOL : 2`
虽然 enum 类型不同，仍落在开头同一个四字节分配单元：movement 位于 `+0x00`，三个状态
依次位于 `+0x01` 的 bit `0..1/2..3/4..5`。`IVP_Impact_Solver_Long_Term` 在 `+0x5C`
使用 `8/2/22` 分组；`IVP_VHash` 在 `+0x08..+0x0B` 使用 `24/8`；
`IVP_Actuator_Force` 的两个 push 标志共享 `+0x74` 的 bit 0/1。Constraint、Contact、
Friction System、Local norm 和 Interpolator 的逐项位置也进入同一门禁。

`IVP_Compact_Surface` 现在通过一个匿名 union 同时公开历史
`max_factor_surface_deviation/byte_size` 8/24 位字段和原始 `factor_and_size` 字；两种视图
共享 `+0x1C` 的同一存储。精确 DLL 的 pointsoup 构建案例会同时读取字段、访问器和原始字，
确认三者一致。只暴露受限头部访问的 `IVP_Compact_Mopp` 继续保留历史位域拼法。
`Audit-IvpIdbBitfields.py` 从关闭重开的数据库副本独立检查 11 个 owner、
23 个字段的位偏移/占用宽度和总尺寸；它使用上述 manifest 判断 IDB，而不是反过来把 IDB
当作 ground truth。两个审计当前均为 0 个问题，并已分别接入 CTest 与事务提交门禁。

## 公开字段差分与历史字段视图

方法签名闭合不能代替字段级 ABI。`Audit-IvpPublicFields.py --check` 现在从邻近公开头和当前
umbrella AST 枚举 582 个公开字段，并用 `tools/ivp/public-field-evidence.tsv` 对每个差异强制
分类。567 项名称、规范化类型和相对顺序精确一致；其余 15 项全部有证据，位宽差异、顺序
差异、未解释项和账本陈旧项均为 0。

确认在 Ballance 中没有存储的 12 个后期字段是：

- `IVP_Anomaly_Limits` 的 collision-check 和两个 friction-mass 限制；零售构造及 `0x14`
  布局在 `max_angular_velocity_per_psi` 结束；
- `IVP_CarSystemDebugData_t` 的四个 actuator vector；Ballance debug block 为 `0x308`，包含它的
  raycast-car controller 为 `0x958`，均没有这四项的尾部空间；
- `IVP_Constraint::client_data`；Ballance base 在 `0x18` 结束，Local 的派生字段立即从该处开始；
- `IVP_Controller_Phantom::client_data`、`IVP_Template_Phantom::manage_sleeping_cores`、
  `IVP_Template_Real_Object::pinned` 和 `IVP_Template_Spring::spring_force_only_on_stretch`；各项均由
  零售分配/完整构造写集及后续字段偏移证明不存在。

三项不是“缺字段”，而是为现代 MSVC 保持零售布局所需的存储表示差异：

- `IVP_Core_Fast_PSI::movement_state` 与 `temporarily_unmovable` 分别以 8-bit 存储保留
  `+0x60/+0x61`。把不同 enum 类型照抄成位域会被现代 MSVC 分到不同 32-bit 单元，使完整
  Core 尾部整体错位；这里不能为了源码长得一样而破坏 DLL ABI。
- `IVP_Real_Object_Fast::flags` 使用一个 4-byte 命名视图恢复
  `object_movement_state`、碰撞开关、zero-shift 和 listener 位，同时保留 `raw` 别名供零售
  mask 操作。精确 DLL 的对象变换/listener 案例验证命名位与 `+0x80` 原始字一致。

另外恢复了若干原先只剩 raw 存储的正确公开视图：`IVP_Core_Friction_Info` 的 moveable/
unmoveable union 名称、`IVP_Core_Fast_Static` 的 piling/physical/pinned 等标志、
`IVP_Old_Sync_Rot_Z` 与 `tmp_null.old_sync_info`、Compact Surface 的上述历史位字段和
`reserved/dummy` 别名。`IVP_Extra_Info` 在 canonical IDB 中为 `0x40`、最后一个指针位于
`+0x38`；`IVP_Template_Extra` 为 `0x50`，`info` 从 `+0x10` 开始，因此该公开模板已恢复。
相邻的 `IVP_Actuator_Extra` 位于参考源码 `INTERN_START` 区域，不能因为导入类型存在就扩大
公开 API。

布局证据也按同一原则重新分类。`IVP_Compact_Surface` 不再停留在 IDB-only：零售
`IVP_SurfaceBuilder_Ledge_Soup::allocate_compact_surface` RVA `0x39600` 以 `0x30` 为 header
起点分配，写入 `+0x1C` 的 packed byte size、`+0x20` ledgetree root，并清零
`+0x24/+0x28/+0x2C`；`insert_radius_in_compact_surface` RVA `0x39B30` 又写入
`+0x00` mass center、`+0x0C` inertia、`+0x18` radius 和 `+0x1C` low deviation byte。
两段保留机器码覆盖全部公开字段，exact-DLL tetra/pointsoup 场景再验证实际生成值，因此该
owner 现为 retail-confirmed，而不是因为 IDB 恰好显示 `0x30` 才提升。
该案例也防止用通用几何公式替换零售算法：轴截距为 `3/4/2` 的四面体质心确为
`(0.75, 1.0, 0.5)`，但 Ballance hull builder 写入的 inertia 系数是
`(0.6184659, 0.3693322, 0.6884086)`，并非对象 XYZ 轴解析体积惯量
`(0.75, 0.4875, 0.9375)`。测试曾用后者预期而被原版 DLL 直接证伪，现固定前者并检查
半径、packed size、root offset 和三个清零尾字段。

相反，`IVP_Object_Attach`、`IVP_SurfaceBuilder_Halfspacesoup`、`IVP_Compact_Modify`、
`IVP_SurfaceBuilder_Polyhedron_Concave`、`IVP_Convex_Decompositor` 和
`IVP_SurfaceBuilder_3ds` 的所有公开入口都是 static；没有 helper 实例或 `this` 指针跨过
DLL 边界。它们现归为 layout-not-applicable，IDA 给空类显示的 `0x01` 不再冒充 ABI 布局
证据。加上 `IVP_Contact_Situation` 和五个 event payload 后，当前布局分层为 87 个
retail-confirmed、46 个 IDB-only 和 13 个 not-applicable。

## `IVP_Hash` 的固定宽度键与未实现 Enumerator

邻近 `ivu_hash.hxx/cxx` 中的 `IVP_Hash` 不是已有 `IVP_U_String_Hash` 的别名。零售构造
RVA `0xB670` 明确把 `key_size`、bucket 数、not-found 值和 bucket 指针放在
`+0x00/+0x04/+0x08/+0x0C`，完整对象大小为 `0x10`。旧 IDB 虽有大体正确的函数类型，
却把两个整数参数解释成 `element_size/table_size`；根据构造写入和调用点，现已修正为
`bucket_count/key_size/not_found_value`。

Ballance 保留完整构造/析构、查找和插入四个实体
`0xB670/0xB6A0/0xB6F0/0xB770`，这些操作全部直达 DLL。查找和插入按 `key_size` 对键做
定长 `memcmp`，CRC 也恰好处理这么多字节，而不是遇到 NUL 停止；精确 DLL 案例用三个
包含嵌入零字节的 4-byte key 强制同桶链，验证 add/find、移除中间节点以及其余节点保持。
`hash_index` 在参考版本中本来就是 header-inline，`remove` 在 Ballance 被链接裁掉；当前只按
已确认的 CRC 与 `{next,value,key[]}` 链布局重建这两项，并且节点仍经零售 allocator 释放。

参考头中的 `IVP_Hash_Enumerator::next_element` 自己就标注 “not implemented”，邻近实现和
零售 DLL 都没有函数体。它没有被为了“接口完整”而编造；只有出现 Ballance 专属实体或
调用点证据时才会重新考虑。

## `IVP_U_Memory` 的事务池与内联边界

邻近 `ivu_memory.hxx/cxx` 给出四个指针和两个 16-bit 计数器，但当前布局不是直接照抄：
零售构造/初始化 `0x20260/0x20270`、分配 `0xD110`、事务初始化/回滚
`0x20150/0x201B0` 和扩块 `0x20210` 的字段访问共同确认
`first/last/begin/end +0x00..+0x0C`、signed transaction depth `+0x10`、external-size
`+0x12`，总大小 `0x14`。IDB 中的 `p_Memory_Elem` 与 `IVP_U_Memory` 因而由 guarded
脚本重建为上述精确布局，而不是继续把导入类型当成结论。

Ballance 的完整析构入口 `0x20140` 原先只显示成跳往 `free_mem 0x20290` 的匿名别名；
Environment 析构先调用它、再单独调用零售 `operator delete`，确认它是 non-deleting
complete destructor。当前 API 直接调用这个入口，并让外层 `new/delete` 也留在 DLL 堆。
无外部缓冲时，首块可用区基线为 `0x7FD8`，返回地址按 32 byte 对齐；事务结束释放首块
之后的所有扩展块，再把游标复位到首块。外部缓冲路径把 `size-0x20` 存为 16-bit 可用长度，
整池释放时不释放调用者提供的首块。

参考版本内联的 `start_memory_transaction/end_memory_transaction/get_mem_transaction` 按
同一 `+0x10` 计数器最小重建；`get_memc` 没有零售函数边界，其完整实现只是调用已保留的
`get_mem` 后清零请求字节，也选择性复现。真实 DLL 案例用 96-byte contact state、64-byte
清零 constraint state 和 `0x9000` matrix workspace 验证对齐、扩块、回滚后首地址复用和
析构释放；另一个 4096-byte caller-owned scratch 案例确认外部首块不会被 DLL 释放。
这些重建没有获得虚构 RVA，也没有引入另一版本的整套 allocator。

## 跨 DLL allocator 与 vptr 所有权

这不是邻近 IVP 版本的源码差异，而是重建头文件与 Ballance 原版 DLL 共存时必须补上的
二进制边界。原版导入表确认 `operator new`（`??2@YAPAXI@Z`）、`operator delete`
（`??3@YAXPAX@Z`）、`malloc` 和 `free` 全部来自旧 `MSVCRT.dll`；当前 Release BML+ 则导入
`VCRUNTIME140.dll` 与 `api-ms-win-crt-heap-l1-1-0.dll`。所以“构造函数和析构函数都能调用”
仍可能是错的：如果 Mod 的 UCRT `new` 分配对象，而完整原版构造体写入零售 vptr，之后
scalar deleting destructor 会从该 vtable 进入 DLL 并调用旧 MSVCRT `operator delete`，形成
跨堆释放。

已确认的代表性指令链包括：Material Simple 构造 RVA `0xBF00` 写入零售 vptr；Debug
Manager 构造 RVA `0x60C30` 写入 `0x10063D34`，其 deleting destructor RVA `0x60C50`
最终跳到原版 delete thunk `0x60756`；`IVP_Core` 虽无 vptr，但 Real Object 构造和 core merge
均在 DLL 内 `new/delete IVP_Core`；Buoyancy Attacher 的完整构造 RVA `0x104C0` 安装零售
派生表，active-set 删除回调最终拥有该对象。

兼容层不增加 allocator 基类或字段，以免改变任何对象布局，而是在经审计的公开 owner 上
声明 class-specific 标量 `operator new/delete`，统一解析原版 `0x6075C/0x60756` 入口。
`new[]` 被删除，因为没有 Ballance array-cookie 证据；placement new 仅供已审核 raw factory，
对应存储不得交给引擎 deleting destructor。工厂专属 `IVP_Environment` 和
`IVP_Controller_Phantom` 只提供原版 delete。Controller/Material/Collision Filter 等可派生
接口把同一策略传给 Mod 派生类；Forcefield 与 Raycast Car 因 protected controller 基类会
隐藏分配器，另在完整对象的 public 区重新发布相同操作。

精确 DLL 测试不再只看“未崩溃”：它逐对象确认构造后的 vptr 位于已校验的 `0x81000`
零售映像内，再经实际 deleting destructor、environment callback 或 active-set 生命周期释放。
独立的计数测试枚举 26 个公开分配类型和两个工厂释放类型，要求每次原版分配恰好对应一次
原版释放；核心集合拥有 Forcefield 的领域案例也直接断言动态派生对象恰好走一次零售分配和
一次零售释放。另在 `/Gz`、`/Gr` 下编译完整 C gateway，防止默认调用约定改变跨 DLL 栈清理。

## 维护规则

后续每发现一处差异，在本文追加：邻近源码文件和成员/槽位、原版 DLL 证据、确认的
尺寸或偏移，以及兼容层采取的处理。仅由类型名、反编译伪代码或另一版本源码推测的内容
应标成“待确认”，不能放入上面的已确认总览。

可用 `tools/ivp/Inspect-IvpFunction.py` 配合 `idat.exe` 在数据库副本上打印函数边界、
当前 IDA 类型和逐条反汇编；该输出用于交叉验证，仍不自动升级为 ABI 结论。
