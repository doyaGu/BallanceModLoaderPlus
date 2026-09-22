# `physics_RT` 使用 API 覆盖清单

本页把“至少覆盖所有 `physics_RT` 使用的 API”定义为一个可重复检查的闭合集合。分析
对象固定为 Ballance 原版 `BuildingBlocks\physics_RT.dll`。在 IDA 中先枚举插件适配层
主代码区的全部直接 `call`，再逐项排除 CK SDK、CRT 和适配层自身目标；之后把同属这些
函数的异常清理尾块纳入，得到最初的 65 个目标。当前公共 API 又加入 19 个 Local
constraint、27 个矩阵/线性约束求解入口、12 个 constraint 基类缺省修改入口、2 个
core-reaction 求解入口和 1 个全局 collision-tolerance 入口，统一形成
129 个目标；随后加入 `IVP_Object` 两条构造路径与完整析构、此前匿名的
`IVP_U_Min_List` 构造/析构，以及 Phantom 的完整构造/析构，统一形成
**136 个已追踪的 IVP 零售目标 RVA**；对象 attach/detach 生命周期复核又接入
`unlink_contact_points` 与 `update_exact_mindist_events_of_object` 两个原版受保护入口；
随后批量接入原版保留的数学、材质、浮力、控制器、模板和 active-value 短函数，
随后继续纳入已核对的 object/core 生命周期、碰撞过滤、surface manager、性能统计与
active value 实体、Grid 的原版 triangle-distance 内核和 aligned allocator，形成过一版
228 项的人工子集。反向扫描全部公开头文件后确认该口径漏记了大量已经存在的 DLL
包装；继续核对已登记但未使用的地址后又恢复了公开自由函数 `ivp_rand`；本轮再接入
Phantom mindist 进入/离开两个 protected 引擎入口；宽相复核又接入 `IVP_OV_Element`
的完整构造/析构、Hull Manager 调度、三个虚回调和碰撞索引维护；随后补入
`IVP_OV_Tree_Manager` 的完整构造/析构与 element 插入/移除，以及 `IVP_OV_Node` 的完整
构造/private 完整析构；Spring 链又接入完整构造、断裂监听派发、Active Float 回调和
Active 完整构造；Mindist Manager 又接入原版完整构造与此前匿名的完整析构；固定宽度
二进制键 `IVP_Hash` 接入四个保留体；求解器事务池 `IVP_U_Memory` 接入构造、析构、
分配、初始化、扩块和释放等七个新增保留体；连续碰撞使用的 `IVP_U_Matrix_Cache`
又接入原版 `p_init`；`IVP_3D_Solver` 再接入三个连续碰撞保留体。当前共有
**681 个公开 `Address` 标识、680 个唯一公开函数 RVA**；加上 1 个只供适配层使用的
析构目标，Retail Contract 合计 **681 个唯一直接依赖目标**。唯一公开别名是
`SimulationUnitEnsureCoresMovement` 与 `SimulationUnitClearMovementChecks`，二者共同指向
RVA `0x120B0`。另行枚举主代码区的 217 条
间接 `call`，按
反编译对象类型确认并建账的条目现有 **14 个 IVP vtable 调用点**。

逐项清单位于 `tools/ivp/physics-rt-api-coverage.tsv`，间接调用逐项清单位于
`tools/ivp/physics-rt-indirect-api-coverage.tsv`；两者现在都是
`tools/ivp/retail-contract.json` 的生成视图，
可用 `tools\ivp\Test-IvpApiCoverage.ps1` 一起检查。依赖目标当前结果是：

| 处置 | 数量 | 含义 |
| --- | ---: | --- |
| `typed-wrapper` | 680 | 有 Ballance 专用类型声明，并通过版本锁定 RVA 调用原函数 |
| `reconstructed-inline` | 0 | 这 680 个公开目标均不需要用邻近源码替换原函数 |
| `adapter-internal` | 1 | 只服务于适配层的 non-deleting 析构步骤，不作为独立 API 公开 |
| 待处理 | 0 | — |

唯一非公开目标 RVA `0xBD00` 是适配层自建 polygon surface manager 的
non-deleting 析构步骤；对外应使用 `IVP_SurfaceManager` 的虚析构合约。

RVA `0xB0F0` 则不能按 IDA 导入的 `IVP_Triangle::~IVP_Triangle` 名称处理：函数体把
对象 vptr 恢复为原版 `IVP_Controller` 基类表，且只由 RVA `0x47A0` 函数的异常清理
尾块引用，因此它实际是 `IVP_Controller` 的 non-deleting destructor。当前公共虚析构
会调用这个原版入口。

14 个间接调用点另行闭合：两个使用
`IVP_Listener_Collision` 的 deleting destructor，一个使用 `IVP_Controller` destructor，
两个通过继承槽删除 `IVP_Constraint`，另两个分别调用 `IVP_Material` 与
`IVP_SurfaceManager` 的 deleting destructor；另外七个来自 collision listener 的全局与
对象私有派发，覆盖 post-collision、object-deleted、friction-created 和
friction-deleted 槽。对应的原版槽位和调用签名已经进入公共
头文件；material 的第 5 槽是带标志参数的 MSVC scalar-deleting destructor，当前通过
真正的 C++ 虚析构触发该槽，而不是继续暴露手写函数指针表。

## 覆盖范围

| 闭包成员 | 数量 | 含义 |
| --- | ---: | --- |
| 公开头文件实际使用的 `Address` 标识 | 681 | 从 `include/BML/IVP/**/*.h` 反向扫描，不靠人工挑选；包含 protected/private 引擎入口 |
| 唯一公开函数 RVA | 680 | 一个已登记的双名称别名按 DLL 边界只计一次 |
| 适配层内部析构目标 | 1 | RVA `0xBD00`，不作为普通 IVP API 公开 |
| 已登记但当前未公开调用的 code RVA | 4 | 均为 protected/private 引擎内部实体，保留分析证据 |
| 间接 vtable 调用点 | 14 | 逐调用点核对 owner、槽位和签名 |

这一定义刻意没有把 DLL 内的每个私有求解器都叫作“API”。修正后的只读符号目录包含 IDA
数据库里的 1600 个命名函数，可供继续研究；但只有已经确认调用约定、字段布局和生命周期的
入口才进入 `BML/IVP/` 公共头文件。这样既覆盖原插件实际依赖，也不会把另一修订的 IVP
源码整体伪装成 Ballance 的 ABI。

## 校验方法

下列 Player 示例使用 `BML_BALLANCE_ROOT` 指定游戏安装目录。运行源码审计前，
请将 `IVP_REFERENCE_ROOT` 设为对照 IVP 源码目录的绝对路径；示例从 `mods/IVP` 运行。

覆盖检查同时验证：681 个直接依赖 RVA 唯一；681 个 Address ID 映射为恰好 680 个
typed-wrapper RVA，反向和正向均无遗漏。canonical IDB 已通过事务提交全部 681 项直接目标
原型，包括 Phantom 两项、Core worst-case mass、匿名 OV Element、Spring、Mindist Manager、
`IVP_Hash`、`IVP_U_Memory`、Matrix Cache initializer 与三个 3D Solver 入口；关闭重开后的回读结果为
681/681。九个链接后
无装饰名的公开目标继续使用中性分析标签，不会为了填满 manifest
而伪造原版符号；14 个间接 call site 各自有对应 vtable
合约；没有 `pending` 项。其他没有独立 DLL 函数体的方法仍可能是经验证的内联重建，
但不会用它们冒充这 680 个可直接调用原版实现的入口。

```powershell
tools\ivp\Test-IvpApiCoverage.ps1
tools\ivp\Test-RetailPhysicsImage.ps1 `
  -BallanceRoot $env:BML_BALLANCE_ROOT
```

第二项另外校验用户指定的原版 DLL 的 SHA-256、PE 身份和指令锚点。邻近源码与原版
不一致的内容见[差异账本](ivp-retail-differences.md)。

## 公共方法级审计

上面的 681/681 是公开头文件与 DLL 调用目标的双向闭包，不等于 IVP 的完整接口。
`Audit-IvpPublicInterface.py` 提供的是另一项更窄的指标：邻近源码中带
`//IVP_EXPORT_PUBLIC` 标记、并且声明实体确实位于该头文件中的**公开成员方法候选集**。
审计会排除仅为了让聚合翻译单元通过编译而传递包含的内部类型，也会排除邻近 SDK 的
`ive_graphics.hxx` 示例/图形桥接层，并按源码的 `INTERN_START/INTERN_END` 围栏排除
明确标成 internal 的声明。当前候选集来自 57 个参考头文件、252 个类/类型和 1575 个
公开成员方法。候选分母仍只取这些头，但 virtual override 解析会读取传递包含的基类声明；
这样不会把未标记的 `ivp_car_system.hxx` 扩进公开分母，也不会把它的旧式无 `virtual`
override 误算成新槽。

截至 2026-09-07，声明覆盖结果为：1549 个精确签名、2 个经确认的版本签名差异、24 个由
Ballance ABI 证据证明必须省略的后续版本接口。原版 DLL 中有函数体但尚未接入的普通候选、
待补纯虚接口、普通 declaration-only 和未分类差异现在均为 0。24 个省略项中包含 17 个
缺少 Ballance manager/builder 布局与实现支撑的 MOPP 接口；它们现在是逐项登记的零售
省略，而不是可以从邻近源码直接搬入的“待补”项。
1549/1575 只表示**邻近公开声明兼容数量**，
不再称为完成度、ABI 准确率或原版行为验证率。两个零售确认签名变体分别是：Ballance 的
`IVP_Anomaly_Manager::inter_penetration` 只有三个参数，而邻近源码多一个
`IVP_DOUBLE`；`IVP_Car_System::do_steering` 只有一个 `IVP_FLOAT`，邻近源码又增加了
一个 `bool`。这里保留零售 ABI，而不是为得到“精确”数字采用错误签名。候选全集还包含
已确认不属于 Ballance 修订的接口，因此剩余项不能机械复制；例如 Spring 的
`get_only_stretch` 只能作为恒为 `IVP_FALSE` 的非虚兼容查询提供，不能增加原版不存在的
字段或 vtable 槽。差异项会逐步从“缺失”转为明确的“不适用”记录。

证据现已拆成互不替代、允许重叠的维度。严格账本的当前下限如下：

| 证据维度 | 已登记数量 | 分母 | 精确定义 |
| --- | ---: | ---: | --- |
| 当前精确声明 | 1549 | 1575 个候选方法 | 当前头文件与邻近公开声明的 owner、方法名和规范化签名一致 |
| Ballance 版本签名差异 | 2 | 1575 | 当前声明故意采用 Ballance ABI，而不采用邻近签名 |
| Ballance 省略的后续接口 | 24 | 1575 | ABI 证据表明不存在或缺少所需结构支撑，保持缺席才兼容；误加回当前接口会成为审计错误 |
| virtual 属性匹配 | 1541 | 1575 | 精确声明同时保持 virtual/非 virtual ABI；另有 3 项零售确认变体和 5 项 IDA 导入变体，未解释不匹配为 0 |
| 新增虚表槽顺序匹配 | 245 | 252 个 owner | 4 个 owner 采用零售确认变体，未解释 mismatch 为 0；其余 owner 没有可比较的当前新增槽 |
| IDB 精确装饰名函数体 | 403 | 1575 | 原 DLL 中的函数边界被当前 IDB 归属为邻近装饰名；名字仍受 IDB 质量约束 |
| 二进制确认的函数体变体 | 13 | 1575 | 包括 Ballance 签名/折叠函数体、经反汇编归属的析构入口、Core worst-case effective mass 的无 pinned 分支体和 object-space compact-ledge 的 void-return 变体；每项均单独登记，不从邻近装饰名直接推定 |
| 零售确认的内联体 | 5 | 1575 | `IVP_Real_Object` 的 locking/no-lock cache accessor 分别由匿名共享实体 RVA `0x1A190` 和 Ray Solver Object 构造 RVA `0x22030` 中的展开指令确认；`IVP_U_Vector` 分配构造由 RVA `0x24C20`、Active Float dependency 增删由 RVA `0x15BA0/0x15BE0` 确认；不伪装成装饰名 DLL 导出 |
| 零售直接调用目标 | 398 | 1575 | 公开候选方法的零售 RVA 同时属于 680 项类型化直接依赖闭包；protected/internal 引擎入口与自由函数不进入公开成员方法候选分母 |
| 零售间接 vtable 调用 | 6 个方法 | 1575 | 对应 14 个逐项登记的间接调用点；同一方法可有多个调用点 |
| 邻近头文件内联定义 | 499 | 1575 | Clang AST 在公开头文件及其平台实现头中看到函数体；只证明参考算法可见 |
| 邻近源码推定基础 | 284 | 1575 | 当前实现明确参考了邻近头文件或 `.cxx` 算法；不证明 Ballance 采用相同版本 |
| 零售确认 owner 布局 | 87 | 252 个 owner | 有 Ballance 指令字段访问、分配尺寸或 vtable/调用点交叉证据 |
| 仅 IDB 导入布局 | 46 | 252 | 只有导入 UDT、邻近声明和编译器布局一致性，明确不算零售确认 |
| 布局不适用 | 13 | 252 | static-only helper、header-only utility 或 Ballance 省略 owner，没有跨边界实例布局 |
| host 行为测试方法 | 527 | 1575 | 方法由确定性 x86 host 领域测试直接覆盖，包括完整 Real Wheels 四轮车辆控制周期、约束模板/Local 构造、姿态组合/矩阵 transport、cache locking/no-locking 与淘汰保护、动态/固定内联 `IVP_U_Vector` 生命周期、Active Float dependency 生命周期、接触最坏有效质量，以及精确 compact-ledge 射线命中/未命中与凸体分支；不算游戏内验证 |
| Ballance Player 方法 | 15 | 1575 | 方法已进入真实 Player 场景，并检查领域结果而非只检查可达性 |

ABI 门禁不把 IDB 类型当作最终真值。方法审计涉及的 252 个 owner 中，87 个有零售整体
布局证据，46 个只有 IDB 导入布局证据，13 个明确不涉及实例布局；没有整体证据分类的
owner 不会仅凭头文件可编译或一个 `sizeof` 断言被算成“准确”。`IVP_Compact_Surface`
已由零售 RVA `0x39600` 的完整 `0x30` header 分配/尾部写入和 RVA `0x39B30` 的
质量中心、惯量、半径、deviation 写入从 IDB-only 提升为 retail-confirmed；exact-DLL
tetra/pointsoup 场景逐字段验证生成结果。已知另外三个机器码支持的原版修正仍是：
`IVP_Constraint` 为 `0x18`（IDB 为 `0x1C`）、
`IVP_Constraint_Local` 为 `0x190`（IDB 为 `0x198`）、`IVP_Statistic_Manager` 为
`0x60`（IDB 为 `0x58`）。本轮在主 IDB 中逐字段注释了 `IVP_Object_Attach` 和三个
constraint-car 类型；原先缺失的 `IVP_Complex_Simple` 则只按邻近公开头添加 0x34
source-only 定义，明确不提升为零售布局证据。直接基类顺序、virtual 属性与新虚槽顺序
另行审计，当前未解释不匹配均为 0。
所有 version-locked member RVA 经 `__thiscall` 桥接，静态/自由入口经 `__cdecl` 桥接；
IVP Retail Contract 同时生成 `Calls.h` 使用的地址视图和覆盖表，共同固定调用目标，
不能在 x64 构建中误用。

这些数字不能相加。例如一个方法可以同时有零售函数体、直接调用点、确认布局和 Player
行为证据；另一个方法即使签名精确，也可能这些证据全部没有。布局证据记录的是方法所属
owner 的对象布局，不自动证明该方法的算法。host/Player 数量也是严格白名单下限：没有
逐项登记就保持 `false`，不会因为同类的另一个方法通过测试而批量升级。

几个容易混淆的实际条目现在会显示为：

| 方法 | 函数体 | 调用点 | owner 布局 | 邻近源码基础 | host | Player |
| --- | --- | --- | --- | --- | --- | --- |
| `IVP_Real_Object::ensure_in_simulation` | `idb-exact-name` | `direct-target` | `retail-confirmed` | 否 | 否 | 是 |
| `IVP_Ray_Solver_Os::check_ray_against_compact_ledge_os` | 无 | 无 | `retail-confirmed` | 是 | 否 | 是 |
| `IVP_Actuator_Suspension::do_simulation_controller` | 无 | 无 | `ida-import-only` | 是 | 是 | 否 |
| `IVP_Template_Controller_Golem` 构造 | 无 | 无 | `ida-import-only` | 是 | 否 | 否 |
| `IVP_Controller_Golem::set_prime_position` | 无 | 无 | `ida-import-only` | 是 | 是 | 否 |
| `IVP_Constraint_Fixed_Keyframed::set_prime_position_Ros` | 无 | 无 | `ida-import-only` | 是 | 是 | 否 |
| `IVP_Forcefield::~IVP_Forcefield` | 无 | 无 | `ida-import-only` | 是 | 是 | 否 |
| `IVP_Controller_Raycast_Car::activate_booster` | 无 | 无 | `ida-import-only` | 是 | 是 | 否 |
| `IVP_Anomaly_Manager::inter_penetration` | `binary-confirmed-variant` | 无 | `retail-confirmed` | 否 | 否 | 否 |
| `IVP_Inline_Math::isqrt_float` | `idb-exact-name` | `direct-target` | `not-applicable` | 否 | 是 | 否 |

这样 `Suspension` 或 Golem 的 host 测试通过不会把 IDB-only 布局升级为零售确认，
而 object-space ray 即使没有独立 DLL 函数体，仍可单独保留真实关卡行为证据。
`IVP_Inline_Math` 的公开成员全部是 static，故 `not-applicable` 表示该 owner 根本没有
实例布局；它既不是未确认，也不会被虚增为零售布局证据。同一规则现在也用于
`IVP_Object_Attach`、`IVP_SurfaceBuilder_Halfspacesoup`、`IVP_Compact_Modify`、
`IVP_SurfaceBuilder_Polyhedron_Concave`、`IVP_Convex_Decompositor` 和
`IVP_SurfaceBuilder_3ds`：它们的公开入口全部是 static，导入的空类 `0x01` 不是跨 DLL ABI。

逐项人工证据账本位于 `tools/ivp/public-api-evidence.tsv`。方法输出新增
`retail_body_evidence`、`retail_callsite_evidence`、`owner_layout_evidence`、
`nearby_header_definition`、`nearby_source_basis`、`host_behavior_test` 和
`ballance_player_test` 七列；
`disposition` 继续只回答声明是否存在，避免破坏已有缺口统计。聚合输出另提供
`summary-tsv`，不再从单一百分比推断可靠性。类级输出还核对直接基类顺序以及所有
访问级别下新虚槽的追加顺序；模板基类的类型参数会先实例化再比较覆盖槽，避免把
`IVP_Listener_Set_Active<T>` 的覆盖误算成派生类新增槽。这些检查当前都没有未解释不匹配。

公开字段和自由函数现在也有独立分母，不再藏在“方法覆盖”之外：

- `Audit-IvpPublicFields.py --check` 枚举 582 个公开字段。567 项名称、规范化类型和相对顺序
  精确一致；`tools/ivp/public-field-evidence.tsv` 对其余 15 项逐条解释，其中 12 项为
  Ballance 布局中确认不存在的后续字段，3 项为保持相同 ABI 的存储表示差异。位宽差异、
  顺序差异、未解释差异和陈旧账本项均为 0。
- 已恢复的历史字段视图包括 Core friction union、Core static flags、old-sync 指针、
  Real Object 嵌套 flags、Compact Surface 的 `max_factor_surface_deviation/byte_size` 8/24
  位字段，以及由 IDB `0x40/0x50` 布局支持的 `IVP_Extra_Info` / `IVP_Template_Extra::info`。
  `IVP_Core_Fast_PSI` 的两个 8-bit 状态采用独立字节，不强行使用现代 MSVC 会拆成多个
  32-bit 分配单元的混合 enum 位域；这是显式登记的 ABI 等价表示。
- `Audit-IvpPublicFreeFunctions.py --check` 覆盖 34/34 个公开自由函数：8 个直接调用零售
  DLL，21 个按邻近算法在已验证 Ballance 布局上重建，1 个组合相邻保留原语，4 个因
  Ballance 缺少所需结构支撑而保持省略。它们不计入 1575 个成员方法分母。

这些大型 AST 审计由 CTest 使用 64 位 Python 运行，并有 180 秒测试上限；公共 AST 子进程
另设 150 秒超时，避免 32 位 Python 内存耗尽后遗留 Clang 进程。

1575 不是“完整 IVP API”的单一分母：它不统计公开字段、枚举、自由函数、隐式成员和
模板实例，也不自动纳入由 Ballance 的 environment/phantom 等公开对象暴露出来的内部
ABI。后者单独作为 **Ballance 可达内部 ABI** 追踪；在逐项建立零售调用、布局或 vtable
证据闭包之前不报告虚假的百分比。因此目前分别报告 648/648 的已验证零售函数目标闭包、
1549 个精确签名加 2 个零售签名变体和 24 个确认省略的 1575 方法候选集、582 项字段审计、
34 项自由函数审计，以及持续扩展但暂不设总分母的 Ballance 可达内部 ABI。

本次还修正了审计定义本身：旧统计误把源码 `INTERN_START/INTERN_END` 围栏内的 20 个
成员计入“公开”分母，包括明确写着 demo-only 的 `IVP_Actuator_Extra` 九个方法、
`IVP_Extra_Info` 构造，以及 3 个 material 和 7 个 math 内部方法。修正后精确项也同步
减少 10 个，绝不是只缩分母；这些已实现的零售可达方法仍保留在当前头文件中，只是不再
冒充邻近源码的公开接口。

按“邻近公开声明已提供”的口径，当前代表类型包括：

- `IVP_U_Point` 58/58、`IVP_U_Float_Point` 42/42；
- `IVP_U_Matrix3` 41/41、`IVP_U_Matrix` 37/37、`IVP_U_Quat` 29/29；
- `IVP_Template_Anchor` 9/9、`IVP_Template_Constraint` 34/34；
- `IVP_Controller_Motion` 19/19、`IVP_Listener_PSI` 2/2；
- `IVP_Ray_Solver` 10/10、`IVP_Ray_Solver_Group` 5/5、
  `IVP_Ray_Solver_Min` 5/5、`IVP_Ray_Solver_Min_Hash` 3/3、
  `IVP_Ray_Solver_Os` 4/4；
- active value 的 value/float/int/两个 terminal 与 manager 全部闭合，分别为
  5/5、8/8、7/7、4/4、4/4、18/18；Sine/Square/Pulse、Add/Sub/Add-Multiple/Mult、
  Limit/Test-Range/Switch 十种公开表达式节点也均已达到各自 4/4 或 5/5；
- `IVP_MI_Vector` 11/11、`IVP_Multidimensional_Interpolator` 7/7、
  `IVP_Buoyancy_Input` 1/1、
  `IVP_Buoyancy_Output` 1/1、`IVP_Template_Buoyancy` 1/1；
- `IVP_Statisticsmanager_Console_Callback` 1/1；其三槽私有 callback 实现可由
  Better Statistics manager 正常分派；
- `IVP_Halfspacesoup` 4/4、`IVP_SurfaceBuilder_Halfspacesoup` 3/3；可从原版 compact
  ledge 提取 inward planes，并往返生成 points、ledge 和 surface；
- `IVP_Compact_Modify` 3/3；支持单凸体切片以及 ledge/surface 等距收缩；
- `IVP_Cache_Object` 9/9、`IVP_Event_Sim` 2/2、`IVP_Time_Event` 2/2；
- `IVP_Collision` 7/7、`IVP_Collision_Delegator` 4/4、
  `IVP_Collision_Delegator_Root` 3/3；
- 邻近源码未列入 57 个公开头文件审计、但由 environment/phantom 实际暴露的内部层也已
  恢复：`IVP_Mindist_Base`/`IVP_Mindist` 的 `0x78/0x88` 布局、
  `IVP_Mindist_Manager` 的 `0x18` 布局，以及 20 个 manager 和 9 个 mindist 的
  version-locked 零售入口；
- `IVP_Controller_Phantom` 8/8；构造与完整析构直接进入零售函数，保持 `0x40` 的 Ballance
  布局，并由 real-object 转换路径管理关联状态和容器生命周期；
  `IVP_Listener_Phantom` 6/6、`IVP_Contact_Point_API` 4/4；
- `IVP_Synapse` 8/8、`IVP_Standard_Gravity_Controller` 6/6、
  `IVP_Statistic_Manager` 3/3、`IVP_Freeze_Manager` 2/2；
- `IVP_Actuator` 6/6、`IVP_Actuator_Two_Point` 5/5；Force、Torque 与 rotational motor
  三族也已闭合：普通类分别为 4/4、4/4、6/6，Active 类为 1/1、2/2、2/2，三个模板
  构造及 `IVP_Environment::create_force/create_torque/create_rotmot` 均已提供；
- 独立于普通 Spring 的 `IVP_Controller_Stiff_Spring` 11/11、Active 2/2，两个模板均
  1/1；质量适配的一维恢复/阻尼冲量、断裂监听和三路 Active 参数均已重建；
- `IVP_Template_Check_Dist` 1/1、`IVP_Anchor_Check_Dist` 6/6、
  `IVP_Actuator_Check_Dist` 4/4，并补齐 `IVP_Environment::create_check_dist`；两端 anchor
  的 hull min-list 注册、阈值跨越、Active 输出与删除通知构成完整生命周期；
- `IVP_Template_Four_Point` 1/1、`IVP_Actuator_Four_Point` 3/3、
  `IVP_Template_Stabilizer` 1/1、`IVP_Actuator_Stabilizer` 2/2，并补齐 environment factory；
  Four Point 会去重登记可移动 core 并真正注册 controller，修复邻近死代码中的反向判断
  和漏注册；
- `IVP_Template_Suspension` 1/1、`IVP_Actuator_Suspension` 5/5，并补齐
  `IVP_Environment::create_suspension`；运行时保持 Ballance 的 `0xA0` 布局、reduced virtual
  mass 适配、压缩/回弹双阻尼及只对车身侧限幅的车辆悬挂规则；
- `IVP_Attacher_To_Cores<ATTACH_T>` 2/2；已有 core、活动集合增删和集合销毁都会保持
  “每个 core 恰有一个 attachment”的生命周期约束。`IVP_Template_Controller_Golem` 与
  `IVP_Template_Constraint_Fixed_Keyframed` 的公开构造也已闭合，并分别保持 IDB 回读确认的
  `0x38/0x24` 布局；
- `IVP_Forcefield` 1/1：恢复 protected listener/independent-controller 多继承和 `0x10`
  IDB/source 布局；已有 core、活动集合增删、控制器登记/注销及 owner-set 自删除均有
  确定性 x86 host 场景。DLL 未保留类专属函数体或 vtable，因此不提升为零售代码确认；
- raycast car ABI 已恢复到可派生 controller：`IVP_Template_Car_System` 为 `0x304`，wheel/temporary/
  axis 为 `0x7C/0x98/0x04`，单 core 内嵌 vector 为 `0x0C`；四轮配置默认值、Ackermann
  转向几何和 vector 从内嵌单元迁移到引擎分配存储均有 x86 host 场景。`IVP_Car_System`
  按 Ballance IDB 的 28 槽版本声明，调试块为 `0x308`；主 `0x958` raycast controller
  31/31 个邻近公开声明已覆盖，保持 Car System + Controller Dependent 的双基类偏移，
  并选择性重建轮射线准备、接触、稳定杆、悬挂、转向力、轮胎力、增压器和 manager
  生命周期。Mod 派生类只需实现 `do_raycasts`。具体类函数体和 vtable 在原 DLL 中均被
  裁掉，因此算法仍严格标成 IDB/source + host，而不是零售代码确认。31 项中的
  `get_wheel_position` 在邻近树也只有声明、没有实现，Ballance 无 body/callsite；当前只
  以 `= delete` 保留接口边界，不发明“返回哪一个 wheel”的语义；
- early real-wheel constraint car 也已恢复：`IVP_Constraint_Car_Object`、
  `IVP_Constraint_Solver_Car`、builder 分别为 `0xB0/0xA0/0x30`，12/12 个邻近公开方法
  均有选择性实现和 host 行为证据。四轮案例使用 8x8 有效质量逆矩阵执行一次 PSI，验证
  各轮 x/z 接触点速度收敛、系统线性动量守恒、五个 core 的 controller 登记和完整解绑；
  类专属函数体和具体 vtable 未在零售 DLL 中保留，因此证据仍是 IDB/source + host；
- `IVP_Controller_Golem` 5/5：保持导入 UDT 与邻近声明一致的 `0x130` 布局和单个新增
  `resolve_for_problem` 虚槽；移动目标外推、逐轴推力/力矩限幅，以及距离/姿态越界交给
  Mod 恢复策略的路径均有确定性 x86 host 测试，但不冒充已被链接裁掉的零售函数体；
- `IVP_Constraint_Fixed_Keyframed` 5/5：保持 IDB 回读与邻近声明一致的 `0xE0` 布局；
  位置目标在 reference-object 坐标系中外推，姿态目标以相对四元数插值，线性和角向
  修正通过 Ballance `IVP_Core`/cache/controller-manager ABI 施加。类专属函数体和 vtable
  已被裁掉，因此算法仍标为邻近源码基础；相对位置与相对姿态各有确定性 x86 host 案例；
- `IVP_Constraint` 22/22、`IVP_Constraint_Local` 20/20、
  `IVP_Constraint_Local_Anchor` 2/2；
- 原版保留函数体的矩阵求解入口已全部接入；在确认对象布局、`double` 元素宽度与
  Ballance 行宽规则后，又逐项重建了短矩阵操作、四个诊断输出和 Crout LU 求逆族。
  声明覆盖为 `IVP_Great_Matrix_Many_Zero` 33/33、`IVP_Incr_L_U_Matrix` 27/27、
  `IVP_Complex_Simple` 7/7、`IVP_Linear_Constraint_Solver` 1/1；这不等于全部可调用：
  邻近树和 Ballance 都没有实现的 1 个历史 allocating constructor、4 个 complex/LP
  great-matrix 入口、10 个 index-LU/调试入口和全部 7 个 `IVP_Complex_Simple` 算法只保留
  以 `= delete` 精确拒绝，不用另一版本行为填空；
- `IVP_VecFPU` 6/6：这六个公开行运算在原版中全部被内联，当前标量实现采用已确认的
  单 `IVP_DOUBLE` 单元语义，并由接触矩阵测试覆盖；
- `IVP_Environment` 78/78、`IVP_Real_Object` 61/61；此前缺少的 environment event/draw/
  merge 和 real-object beam/unmovable/recompile/delete-vicinity 方法均已按原签名补齐；
  两个 recompile 入口现已调用完整的 Core 质量/材质重编译路径；
  `delete_and_check_vicinity` 也会先通过零售 mindist/friction/Core 入口修复邻域，再进入
  原版虚析构 helper。`beam_object_to_new_position` 现已选择性重建：复合对象姿态会先转换
  为 core 姿态，再通过原版 time/quaternion/mindist/broadphase/hull helper 完成安全更新；
  休眠对象完整碰撞重建与活动对象重复更新快路径均有独立测试。
  `change_unmovable_flag` 现也按完整事务可调用：共享 core 的全部接触先断开，活动 core
  先冻结，movable/static friction 表示按各自所有权释放；切成静态时用零售分配器创建
  独立的 `0x24` Simulation Unit、清掉旧 controller 并重新安装 gravity controller，反向
  切回可动时则保留 unit/controller 并销毁静态 friction hash。两条确定性领域案例核对了
  事务顺序。对象局部 listener 的四个注册/注销入口已有完整生命周期场景。
  `IVP_Object_Attach` 3/3 已选择性重建：attach 会同步运动状态、迁移 Core 成员并保持世界
  姿态，detach 会用零售分配器创建独立 Core、保留表面线速度/角速度并提交下一 PSI，
  reposition 会重建 hull/cache/mindist。三条路径均有复合对象生命周期测试；
- `IVP_Core` 作为 Ballance 可达内部 ABI 单独审计，现已达到 **83/83 精确声明**，其中
  **82/83 可调用**。44 项有与邻近声明完全相同的零售装饰名函数体，另有完整析构函数由
  机器码单独确认；本轮从匿名零售函数中恢复
  `stop_physical_movement`、`reset_freeze_check_values`、
  `init_core_for_simulation`、`synchronize_with_rot_z`、`calc_calc` 和
  `update_exact_mindist_events_of_core`，并接入原版 RVA。`clip_velocity`、
  `apply_velocity_limit`、两种 all-object revive、延迟 revive 和已被源码显式禁用的
  collision merge 采用短内联重建。`revive_adjacent_to_unmoveable` 与
  `values_changed_recalc_redundants` 使用零售保留的 contact refresh、material、virtual-mass、
  friction fusion 和 transaction-memory 入口，只在本地保留邻近版本的短遍历；当前仅废弃
  merged-core 拆分路径 `set_matrizes_and_speed` 以 `= delete` 明确拒绝。该 83 项不属于 1575 个
  `IVP_EXPORT_PUBLIC` 候选分母，两个口径不混算；
- Simulation Unit 同样不在 1575 公开分母内。`split_sim_unit`、`fusion_simulation_unities`、
  `sim_unit_sort_controllers` 和 `sim_unit_calc_redundants` 直接进入精确 thiscall 零售入口，
  `union_find_get_father` 按 Core `tmp +0x228` 的 8 指令 walk 内联。确定性 host resolver
  按解码后的控制流验证二分量拆分、三分量尾循环拆分、fuse 注销 donor unit，以及
  controller 去重与 priority 排序；模型只存在于测试中，不作为生产替代实现。这些 intern
  方法不抬高上表 527 个公开 host 测试计数；
- `IVP_Listener_Collision` 采用 Ballance 的 5 槽零售 vtable；方法签名审计为 7/10，
  邻近修订多出的 pre-collision 与两个 friction-pair 槽被逐项标成
  `retail-omitted-variant`；若误加回当前接口，审计会报告 ABI violation。
- `IVP_Listener_Check_Dist_Event` 2/2、`IVP_Listener_Stiff_Spring` 1/1。
- `IVP_Universe_Manager` 5/5、`IVP_Universe_Manager_Settings` 1/1；四槽回调顺序和阈值
  读取均由零售调用点确认，未加入不存在的虚析构槽。

新增实现有针对领域语义的独立测试：共享 friction system 中只删除指定对象对且保留无关
接触、radar 的 exact/hull 筛选和对象方向、共享 core 的冻结迁移、event/controller/
core/object/environment 的整条时间重基准，以及十三个 actuator 案例中的双锚点反向冲量、
轴向 torque、功率到限幅 torque 的换算、速度上限、Stiff Spring 的恢复/阻尼冲量与断裂、
Check Dist 的阈值跨越和 hull 更新、Stabilizer 的双 anchor-pair 距离耦合、Suspension 的
压缩/回弹阻尼、车身力限幅与 reduced virtual mass、动态 core 集合 attachment 的增删/
关闭生命周期、Forcefield 的逐 core controller 登记/注销和 owner-set 自删除、四轮车辆
配置默认值、Ackermann 转向几何、raycast-car core vector 的内嵌/扩容边界、输出 terminal
与 Active 依赖解绑；raycast controller 还用四轮双轴场景检查轮距/轴距、悬挂调参、前轮
转向、后轮扭矩唤醒、锁轮、增压器重入限制、debug-ray 数据和 `this+4` controller 生命周期；
空中 PSI 场景会实际执行四条轮射线，在无命中时验证满伸/零压力、额外重力、booster 速度
和两个计时器，而不是只构造对象；real-wheel constraint-car 案例另用一个车身和四个轮
刚体执行 8x8 x/z 约束求解，检查逐轮接触速度收敛、总动量守恒与 controller/core 生命周期；
另有两个 Fixed-Keyframed 约束案例验证
reference frame 中的位置跟随、相对姿态修正、等量反向冲量和 controller 生命周期。
Core 策略案例还检查超限线速度/角速度经 anomaly policy 修正、逐轴 `spin_clipping`
截断，以及 `apply_velocity_limit` 对当前与待提交速度的邻近版本规则。
Core 重编译案例进一步覆盖运行时材质变化后的质量/惯量冗余值更新、对象 movement-state
同步、不同 friction system 合并、所有接触的材质与 virtual-mass 重算、静态邻接 core
唤醒，以及静态—静态接触删除和 transaction-memory 成对收尾。
受控对象删除案例验证可移动对象先唤醒 simulation unit，静态对象先发现邻近 mindist、
增长 friction system、刷新相邻接触，再进入原版 scalar-deleting-destructor helper。
对象 beam 案例还验证休眠复合对象的 object-to-core 位姿转换、碰撞缓存失效、exact/invalid
mindist 重算、broadphase 重建和 hull 增长/reset 顺序；第二条活动对象路径验证 repeated-call
优化会保留瞬时速度用于旋转/hull 估算，同时跳过昂贵的 broadphase 重建。
Simulation Unit split/fuse 案例另验证二分量/三分量 core 归属、fuse 后 donor unit 从
manager 注销、controller 去重以及按 vtable 第 5 槽 priority 排序；这些 intern 方法
不计入上表 527 个公开 host 测试方法。
Friction Solver 案例使用两个质量和逆转动惯量不同的 core，验证接触法线上的同步/异步
等量反向冲量保持总线性动量，并按各自 contact arm 更新两侧角速度；同时检查穿透方向才
启用 20 倍 gap correction。它验证的是 Ballance friction 求解中实际使用的短 inline
不变量，不把 constructor 可达性当作测试。
它们与四个真实 Player 用例分开统计，不用
模拟 resolver 的单次可达性冒充游戏内行为证据。

摩擦矩阵测试还构造了两个共享同一活动 core、但该 core 分别位于接触第一侧和第二侧的
long-term contact。它执行原版 `calc_solver_PSI` 中确认过的 column-builder 内联算法，验证
同一个试探冲量会以相反方向耦合到两条接触方程，并确认写入的是行主序矩阵的同一列。
这覆盖 Ballance 落地多点接触时矩阵非对角耦合所需的符号和索引规则，不是仅检查函数能否
调用的 smoke test。

接触拓扑案例还建立两条 pair 组成的三 core 连通链，并放置一个不在任何 pair 中的隔离
core，验证 `core_is_found_in_pairs` 对链端、中间节点和隔离节点的判断。这对应 union-find
决定是否需要拆分 friction system 前的成员关系检查；同时锁定过时
`get_controlled_cores` 不会误改调用者 vector，避免把它和真实虚函数
`get_associated_controlled_cores` 混为一谈。

可重复生成审计结果：

```powershell
python tools\ivp\Audit-IvpPublicInterface.py `
  --reference-root "$env:IVP_REFERENCE_ROOT" `
  --format classes-tsv

python tools\ivp\Audit-IvpPublicInterface.py `
  --reference-root "$env:IVP_REFERENCE_ROOT" `
  --format summary-tsv

# 单独闭合记录/枚举按值、隐藏返回指针和浮点别名 ABI
python tools\ivp\Audit-IvpByValueAbi.py `
  --reference-root "$env:IVP_REFERENCE_ROOT"

# 由 i686-MSVC 编译布局闭合全部公开 IVP 位域
python tools\ivp\Audit-IvpBitfieldAbi.py

# 在关闭重开的 canonical IDB 中复核 binary64 对齐、七个精确布局和 Environment
& tools\ivp\Invoke-IvpIdbReadOnly.ps1 `
  -Script tools\ivp\Audit-IvpIdbBinary64Alignment.py

# 独立回读位域/压缩状态的 IDB 存储位置，不把 IDB 当作 ground truth
& tools\ivp\Invoke-IvpIdbReadOnly.ps1 `
  -Script tools\ivp\Audit-IvpIdbBitfields.py

# 单独复算不进入 1575 分母的 IVP_Core 公开可见内部面
python tools\ivp\Audit-IvpPublicInterface.py `
  --reference-root "$env:IVP_REFERENCE_ROOT" `
  --include-core-internal --owner IVP_Core --format summary-tsv
```
