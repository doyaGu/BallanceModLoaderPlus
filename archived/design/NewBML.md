# 1. 背景与目标

## 1.1 背景：为什么要重新设计 BML 的扩展架构

* **历史包袱**：早期 Ballance/BML 的扩展以“直接注入 + 共享进程”的方式为主，脚本/钩子/资源管理彼此耦合、缺少统一通信与版本边界。一旦底层渲染或输入路径变化（例如 D3D9 在部分系统上通过 D3D9On12 适配层运行），上层扩展就容易“错挂”或互相干扰，难以维护与演进。
* **对标成熟生态的做法**：

  * **OpenXR Loader** 以“清单发现（manifests）→ 按策略选择**唯一**Runtime → 按需装载组件”的方式，把运行时的选择与实现隔离；甚至允许用环境变量 `XR_RUNTIME_JSON` 明确覆盖，方便开发与调试。这条思路证明了“**Loader 选型** + **Manifest 驱动** + **延迟加载**”在平台级扩展中的可行性与实用性。([registry.khronos.org][1])
  * **Node-API（N-API）** 用“**稳定 C ABI** + 版本/能力探测 + 函数表尾插”的方式，让原生扩展在引擎内部升级（V8 等更换/更新）后仍能运行，最大化二进制兼容。对于需要长期维护的模组生态，这是被充分验证的解耦策略。([nodejs.org][2])
  * **VS Code 扩展模型** 用 `activationEvents` 声明式“**按需激活**”，把冷启动开销降到最低，也让扩展的生命周期与事件源天然对齐。([code.visualstudio.com][3])
  * **LMAX Disruptor** 把高吞吐的进程内通信收敛为“有界 RingBuffer + Sequence/Barrier（gating）”语义，天然支持**低延迟**与**背压可控**。作为我们 IMC 的基础模型，它能给出可证明的并发语义与成熟实践。([lmax-exchange.github.io][4])
  * **依赖求解** 方面，**PubGrub** 被多个包管理器采用，既追求求解效率，也强调**人类可读的冲突解释**，适合我们做模组依赖/冲突的自动诊断。([Medium][5])
  * **清单格式** 选择 **TOML 1.0**，它目标是“最小、可读、易映射数据结构”的稳定规范，利于手写与机器解析并存。([toml.io][6])

这些做法共同指向一个结论：**把“发现/选择/装配/生命周期”与“运行期能力”解耦**，并为“脚本、钩子、资源、通信”分别设定清晰且稳定的边界，是长期可维护的唯一途径。

---

## 1.2 设计总纲：微内核 + 外置脚本宿主 + 统一 IMC

为满足长期演进与生态扩展，我们确立下列总纲：

1. **微内核 Runtime**：仅保留“稳定 C ABI、IMC（Intra-Mod Communication）总线、句柄化资源、能力/权限、调度/指标、激活代理”这些**不会随图形/输入细节频繁变化**的核心功能；其余全部插件化。这个做法与 Node-API 的“ABI 稳定层”异曲同工。([nodejs.org][2])
2. **脚本引擎不内嵌**：把脚本执行环境做成独立的 **Script-Host Mod**，Runtime 仅通过一个**极小的 Scripting SPI** 对接（创建/销毁 VM、加载模块、泵微任务、IMC 桥接、获取指标等）。默认实现选用 QuickJS，但保持**可替换**与（将来需要时的）**进程外隔离**能力。此举让 Runtime 本身保持稳定与轻量，避免把具体引擎细节烧进 ABI。
3. **IMC v2：有界/可背压/可观测**：以 **Disruptor** 的 RingBuffer/Sequence/Barrier 语义为蓝本，明确队列容量、gating、以及 `block | drop-oldest | drop-newest | priority-shed` 等策略，并输出每主题的吞吐/时延/丢弃指标，作为运维与调参依据。([lmax-exchange.github.io][4])
4. **清单与依赖的可复现**：以 TOML 1.0 作为清单格式，配合 **PubGrub** 进行依赖求解，并输出带**来源与完整性哈希**的锁文件，在不同机器/时间点得到一致的装配结果与可解释的冲突报告。([toml.io][6])
5. **按需激活与热重载**：借鉴 VS Code 的 `activationEvents`，将“事件→激活”固化为声明式契约；热重载遵循“拓扑逆序关停→替换/重装→广播”的不变量，降低资源泄漏与状态错乱风险。([code.visualstudio.com][3])
6. **Adapter-Mods（Hook 也是 Mod）**：所有 detour/vtable/平台特定采集均外置到适配器模块，并把“渲染/输入/关卡”等异质信号**归一化**为标准 IMC 事件，防止把不稳定细节引入 Runtime。

---

## 1.3 目标（Goals）

* **G1：长期 ABI 稳定**

  * 对模组暴露的 ABI 在主版本内保持兼容；新增能力只**尾插**函数与特性位，不重排/不复用。参考 Node-API 的“ABI 稳定、引擎解耦”原则。([nodejs.org][2])
* **G2：统一与可验证的通信语义**

  * IMC v2 必须是**有界**的，具备**背压策略**与**指标**；对延迟敏感路径可配置策略，对高吞吐路径可落地基准测试。([lmax-exchange.github.io][4])
* **G3：脚本可替换、可限额、可中断**

  * 脚本通过 Script-Host 接入；支持内存/栈限额与时间片调度、可被中断；默认不暴露 I/O，按能力白名单逐项开放。
* **G4：可复现装配**

  * 依赖求解产出锁文件，包含来源与完整性哈希；任一节点可重建相同装配状态；冲突提供人类可读解释。([pubgrub-rs.github.io][7])
* **G5：按需激活与快速冷启动**

  * 通过 `activation` 声明只在所需事件发生时加载/启动模组，缩短冷启动时间并降低内存占用。([code.visualstudio.com][3])
* **G6：安全与可审计**

  * Loader 按清单/签名/完整性校验**安全装载**；运行期输出 IMC/脚本/资源的指标，便于诊断与审计。
* **G7：开发者体验（DX）**

  * 提供“开发模式”支持**多 Runtime/多 Script-Host 命名空间并行**，快速 A/B；工具链给出 `doctor`（清单与依赖体检）与 `bench`（消息/脚本基准）。

---

## 1.4 非目标（Non-Goals）

* **进程间分布式**：当前阶段不做跨进程/跨机器的远程 IMC；必要时由适配器或宿主自行封装 IPC/网络。
* **把具体脚本引擎固化进 ABI**：QuickJS 是默认实现，但**不**在 Runtime ABI 中固化任一引擎的类型或行为，以免锁死演进路线。
* **过度中心化的配额/配给**：仅提供必要的配额/限额与指标，不引入复杂的全局资源调度层，保持内核小而稳。

---

## 1.5 成功判据（Success Criteria）

* **兼容性**：老模组在新内核上无需重编译即可运行（若未使用新增特性）。([nodejs.org][2])
* **性能**：IMC v2 在常见负载（渲染帧事件/输入事件/关卡事件）下具备**低 GC 压力、低抖动**；在高压基准中队列策略可控（掉包可观测）。([lmax-exchange.github.io][4])
* **可维护性**：脚本崩溃/超时不会拖垮整体（可中断/可隔离），依赖冲突能定位并修复，渲染后端切换时上层无需改动。
* **可复现**：同一锁文件在不同机器/时间点安装得到一致的组件图。([pubgrub-rs.github.io][7])

> 小结：我们以 **OpenXR Loader 的清单/选择模式**、**Node-API 的 ABI 稳定哲学**、**VS Code 的懒激活模型** 与 **Disruptor 的并发语义** 为支点，建立起一个 **微内核 + 外置脚本宿主 + 有界 IMC** 的新一代 BML 扩展框架，使其既能**快速迭代**又能**长期稳定**。([registry.khronos.org][1])

[1]: https://registry.khronos.org/OpenXR/specs/1.0/loader.html?utm_source=chatgpt.com "OpenXR™ Loader - Design and Operation - Khronos Registry"
[2]: https://nodejs.org/api/n-api.html?utm_source=chatgpt.com "Node-API | Node.js v25.0.0 Documentation"
[3]: https://code.visualstudio.com/api/references/activation-events?utm_source=chatgpt.com "Activation Events | Visual Studio Code Extension API"
[4]: https://lmax-exchange.github.io/disruptor/user-guide/index.html?utm_source=chatgpt.com "LMAX Disruptor User Guide"
[5]: https://nex3.medium.com/pubgrub-next-generation-version-solving-2fb6470504f?utm_source=chatgpt.com "PubGrub: Next-Generation Version Solving"
[6]: https://toml.io/en/v1.0.0?utm_source=chatgpt.com "TOML: English v1.0.0"
[7]: https://pubgrub-rs.github.io/pubgrub/pubgrub/?utm_source=chatgpt.com "pubgrub - Rust"


# 2. 总体架构

## 2.1 分层与数据流（同进程，微内核形态）

```
[Game Process]
    └─ Loader
         ├─ 清单发现/依赖求解/安全装载/锁文件
         ├─ 选择唯一 Runtime（生产）/ 多实例（开发）
         └─ 生命周期与激活编排（Activation Broker）
               │
               ▼
          Runtime（微内核）
         ├─ 稳定 C ABI（函数表 + feature bits）
         ├─ IMC v2（Event/Call/Stream，有界+背压+指标）
         ├─ 句柄化资源（代际句柄）
         ├─ 能力/权限 + 调度/时间片
         └─ Scripting SPI（供 Script-Host Mod 挂接）
               │
   ┌─────────────┴─────────────┐
   ▼                           ▼
Adapter-Mods（Hook端口）   Script-Host Mod（默认 QuickJS）
   │                           │
（D3D9/D3D9On12/输入…）   承载脚本 VM、配额与中断、IMC↔脚本桥
   │                           │
   └─────────── IMC v2 统一事件/调用/流 ───────────┘
```

* Loader 只“发现/求解/装配/编排”，按清单选择组件、延迟加载；该模式直接借鉴 OpenXR **Loader 通过清单选择运行时**、按需激活的做法（其在桌面系统通过 JSON manifest 发现组件，并在真正需要时才加载目标库）。([Khronos Registry][1])
* Runtime 是极小内核，对上暴露稳定 C ABI；对下通过 Scripting SPI 接 Script-Host，对侧通过 IMC v2 与所有 Mod/Adapter 协作。ABI 稳定与“函数表+特性位探测”的哲学来自 Node-API 思路，可在内部实现演进时维持二进制兼容。([code.visualstudio.com][2])
* IMC v2 的并发与背压语义采用 Disruptor 的**RingBuffer + Sequence/Barrier（gating sequences）**模型，既保证低延迟也可控丢弃/阻塞策略，并能公开每主题的吞吐与时延指标用于运维。([lmax-exchange.github.io][3])

## 2.2 生产形态与开发形态

* **生产形态**：进程内**单 Runtime + 单 Script-Host**，确定性最强；Loader 用锁文件固定解析结果；运行期仅按激活规则启动必要的 Mod，降低冷启动与驻留成本。OpenXR 生态同样坚持“**选择一个活动运行时**”，并允许用环境变量覆盖（如 `XR_RUNTIME_JSON` 指向特定 manifest），我们在开发工具链中采纳相同思路。([openxr-tutorial.com][4])
* **开发/灰度形态**：可启用**多命名空间**（多 Runtime/多 Script-Host 并行）做 A/B 或回归；各自有隔离的 IMC 命名空间与指标汇报，便于比对与回滚。OpenXR 社区讨论与工具也表明以“列出/切换活动运行时”的方式进行调试是常见需求。([Khronos Forums][5])

## 2.3 组件发现、求解与锁定

* **清单与格式**：所有组件（Runtime、Script-Host、Adapter-Mod、Regular Mod）以 **TOML 1.0** 清单声明包元数据、能力需求、激活规则等；TOML 强调“可读 + 一义性映射”的目标，适合人手编辑与机器解析。([toml.io][6])
* **依赖求解**：采用 **SemVer + PubGrub**，得到可解释的冲突原因与确定的版本集合；Rust 社区与多语言实现已将 PubGrub 用于包管理器并强调“失败时给出**人类可读解释**”，我们直接对齐该目标。([pubgrub-rs.github.io][7])
* **锁文件**：Loader 产出 `bml.lock`，记录每个组件的精确版本、来源与完整性；安装严格按照锁文件复现依赖闭包，确保“同仓同结果”。（本节引用的 PubGrub 文档亦强调可复现和冲突解释的重要性。）([pubgrub-rs.github.io][7])

## 2.4 生命周期与按需激活（Activation Broker）

* 激活模型对齐 VS Code 的 **activationEvents**：组件在清单中声明“何时激活”（例如命令触发、某类事件出现），Loader/Runtime 的 Activation Broker 监听底层标准化事件（如 `Level:Loaded:*`、`Render:*`），在命中规则时才装载并启动目标 Mod。此模式已在 VS Code 生态被大规模验证，用以降低冷启动与内存占用。([code.visualstudio.com][2])

## 2.5 IMC v2 的放置与边界

* **进程内通信专用**：IMC v2 仅服务**同进程** Mod 之间的事件/调用/流；跨进程或网络由 Adapter 或上层自行封装。
* **强并发语义**：每个主题是**固定容量** RingBuffer；消费者用 gating sequences 反压生产者；可配置 `block / drop-oldest / drop-newest / priority-shed`；并公开指标（`published/dropped/latency p50/p95/p99`）。这些术语与机制可在 Disruptor 官方用户指南与 Javadoc 中查到明确定义。([lmax-exchange.github.io][3])

## 2.6 安全装载与系统约束（Windows 重点）

* Loader 调用 `SetDefaultDllDirectories` 并在装载组件时使用 `LoadLibraryEx(..., LOAD_LIBRARY_SEARCH_* )` 与**绝对路径**，避免 CWD 搜索与 DLL 劫持；这是我们在架构层面对安全边界的最低基线，后续在安全章节给出更细策略。（Windows 官方与行业经验推荐的做法在多渠道文档中反复强调。）([toml.io][6])

---

**小结**：总体架构以“**OpenXR 式 Loader**”的清单/选择/延迟加载为骨架，配合“**Node-API 式 ABI 稳定**”与“**Disruptor 式 IMC 语义**”，再用“**VS Code 式按需激活**”控制生命周期；下层 Hook 统一收敛为 Adapter-Mods，上层脚本通过可替换的 Script-Host Mod 接入。整个系统既明确边界、又保留演进余量，并以可观测与安全装载作为运行期护栏。 ([Khronos Registry][1])

[1]: https://registry.khronos.org/OpenXR/specs/1.0/loader.html?utm_source=chatgpt.com "OpenXR™ Loader - Design and Operation - Khronos Registry"
[2]: https://code.visualstudio.com/api/references/activation-events?utm_source=chatgpt.com "Activation Events | Visual Studio Code Extension API"
[3]: https://lmax-exchange.github.io/disruptor/user-guide/index.html?utm_source=chatgpt.com "LMAX Disruptor User Guide"
[4]: https://openxr-tutorial.com/linux/opengl/1-introduction.html?utm_source=chatgpt.com "1 Introduction — OpenXR Tutorial documentation"
[5]: https://community.khronos.org/t/openxr-loader-how-to-select-runtime/108524?utm_source=chatgpt.com "OpenXR Loader: How to select runtime"
[6]: https://toml.io/en/v1.0.0?utm_source=chatgpt.com "TOML: English v1.0.0"
[7]: https://pubgrub-rs.github.io/pubgrub/pubgrub/?utm_source=chatgpt.com "pubgrub - Rust"


# 3. Loader 设计

## 3.1 职责与边界

**Loader** 是装配与编排层：负责「发现清单 → 解析与校验 → 依赖求解 → 生成锁文件 → 选择并装载唯一 Runtime 与 Script-Host（生产形态） → 生命周期与按需激活 → 安全加载」。它本身不做 Hook、不跑脚本、不提供运行期服务；这些分别由 Adapter-Mods 与 Runtime/Script-Host 负责。参考 OpenXR 的做法，Loader 通过**清单/manifest**延迟加载组件，直到应用/事件触发才真正装入目标库，从而降低冷启动并保持实现可替换性。([Khronos Registry][1])

---

## 3.2 清单发现与解析（TOML 1.0 + 强校验）

* **发现**：从预设目录/注册表/环境变量中枚举各类组件（Runtime、Script-Host、Adapter-Mods、Regular Mods）的清单。
* **格式**：统一采用 **TOML 1.0**，便于手写与多语言解析；目标是“一义性映射到字典”的最小规范。([toml.io][2])
* **校验**：将 TOML 映射为中间 JSON，再用 JSON Schema/CUE 做强校验与 IDE 自动补全（规范外延）。
* **延迟激活**：清单中声明 activation 规则，Loader 只在命中时才加载目标（见 §3.6）。VS Code 的 `activationEvents` 证明了此模式在规模化生态下的有效性。([code.visualstudio.com][3])

---

## 3.3 依赖与版本求解（SemVer + PubGrub）

* **约束表达**：各组件使用 SemVer 范围（如 `^1.4`, `>=1.2,<2.0`）。
* **算法**：采用 **PubGrub** 求解，优点是冲突时能给出**人类可读的根因解释**（回溯推导 prior cause），便于开发者理解“谁拉高/卡住了版本”。([pubgrub-rs-guide.pages.dev][4])
* **性能考量**：PubGrub 在大型约束集上能记忆冲突根因避免重复尝试，适合模组生态的多依赖场景。([ActiveState][5])

---

## 3.4 锁文件（可复现装配 + 完整性）

生成 `bml.lock`，记录：**精确版本**、**来源**、**启用特性**与每个工件的 **SHA-256 完整性哈希**。安装时严格按锁文件重放，确保“同仓同结果”；这与现代包管理器的可复现安装目标一致（规范外延，配合 §3.3 的冲突解释）。([pubgrub-rs-guide.pages.dev][4])

---

## 3.5 Runtime / Script-Host 选择策略

* **生产形态**：每进程选择并装载**唯一 Runtime 与唯一 Script-Host**，保持确定性。
* **开发/灰度**：允许通过环境变量/开发开关指定候选清单并运行多套命名空间用于 A/B；该思路参考 OpenXR 的运行时优先级与 `XR_RUNTIME_JSON` 覆盖机制。([Khronos Forums][6])

---

## 3.6 生命周期与按需激活（Activation Broker）

* **状态机**：`INSTALLED → RESOLVED → STARTING → ACTIVE → STOPPING`（规范外延）。
* **触发**：Loader 订阅底层标准化事件（如 `Level:Loaded:*`、`Render:*`），当事件与清单的激活条件匹配时，调用模块的 `Start()`；与 VS Code 的 `activationEvents` 一致：事件发生才执行 `activate()`，卸载时执行 `deactivate()`。([code.visualstudio.com][3])

---

## 3.7 安全加载（Windows 关键路径）

为降低 DLL 劫持与意外搜索路径风险，Loader 必须：

* 进程启动即调用 **`SetDefaultDllDirectories`**；
* 动态加载统一使用 **`LoadLibraryEx` + `LOAD_LIBRARY_SEARCH_*`** 标志（如 `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS`）；
* 坚持**绝对路径**，禁用 CWD 搜索；必要时对工件做签名/哈希核验（与 §3.4 对齐）。这些做法是微软官方推荐的 DLL 搜索顺序与最佳实践。([微软学习][7])

---

## 3.8 与 Runtime 的握手（ABI 稳定契约）

* Loader 装载 Runtime 后，调用 `GetInterface(major, minor, &feature_bits, &vtable)` 完成**主次版本与能力协商**；
* **主版本不兼容**直接拒绝；**次版本**仅允许**尾部追加**函数指针并通过 `feature_bits`/非空指针探测；
* 该“稳定 C ABI + 版本/能力探测 + 函数表尾插”的理念与 **Node-API** 的 ABI 稳定哲学一致，确保后续引擎/内部实现可演进而不破坏既有二进制模块。([Node.js][8])

---

## 3.9 与 IMC/Script-Host/Adapter 的交界

* Loader 只做**编排**：它通过 Activation Broker 连接“底层事件 → 模块激活”，把运行期通信（IMC v2）与资源/脚本执行交给 Runtime 与 Script-Host；
* Adapter-Mods 作为“端口适配器”发布标准化事件，避免 Loader/Runtime 触碰不稳定的 Hook 细节；
* 脚本执行完全由 Script-Host 管理（配额、时间片、中断），Loader 只负责选择/装载与生命周期。该分工保证“核心极小、边界清晰”，便于长期维护与替换（对标上述各生态的分层做法）。([Khronos Registry][1])

---

## 3.10 关键数据结构与产物

* **Manifest（TOML 1.0）**：包元数据、依赖、能力、激活规则、入口点等。([toml.io][2])
* **解算图**：PubGrub 输出的有向无环图（含版本与来源）。([pubgrub-rs-guide.pages.dev][4])
* **`bml.lock`**：冻结的装配结果 + 完整性哈希。
* **装载计划**：拓扑顺序下的 `Load → Resolve → Start` 与热重载的**逆序关停**列表（规范外延）。

> 小结：Loader 以 **Manifest 驱动 + PubGrub 求解 + 锁文件固定 + 安全加载** 为主轴，借鉴 **OpenXR Loader** 的清单与选择机制、**VS Code** 的按需激活、**Node-API** 的 ABI 稳定哲学与 **Disruptor** 的并发语义（后续章节详述 IMC v2），为 BML 提供一个可复现、可审计且长期可演进的装配面。([Khronos Registry][1])

[1]: https://registry.khronos.org/OpenXR/specs/1.0/loader.html?utm_source=chatgpt.com "OpenXR™ Loader - Design and Operation - Khronos Registry"
[2]: https://toml.io/en/v1.0.0?utm_source=chatgpt.com "TOML: English v1.0.0"
[3]: https://code.visualstudio.com/api/references/activation-events?utm_source=chatgpt.com "Activation Events | Visual Studio Code Extension API"
[4]: https://pubgrub-rs-guide.pages.dev/internals/conflict_resolution?utm_source=chatgpt.com "Conflict resolution"
[5]: https://www.activestate.com/blog/dependency-resolution-optimization-activestates-approach/?utm_source=chatgpt.com "Dependency Resolution Optimization"
[6]: https://community.khronos.org/t/openxr-loader-how-to-select-runtime/108524?utm_source=chatgpt.com "OpenXR Loader: How to select runtime"
[7]: https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-setdefaultdlldirectories?utm_source=chatgpt.com "SetDefaultDllDirectories function (libloaderapi.h)"
[8]: https://nodejs.org/api/n-api.html?utm_source=chatgpt.com "Node-API | Node.js v25.0.0 Documentation"


# 4. Runtime（微内核）

## 4.1 角色与稳定边界

Runtime 只承担“**稳定内核**”职责：对上暴露 **稳定 C ABI 的函数表**与**feature 位**；对侧提供 **IMC v2**、**句柄化资源**、**能力/权限检查**、**调度/时间片**与**激活代理**；对下通过一套极小的 **Scripting SPI** 对接外部 Script-Host（默认 QuickJS 方案由独立 Mod 提供）。ABI 策略采纳与 Node-API 相同的理念：接口随版本演进但 **ABI 长期稳定**，新增能力仅“尾插”并以 feature 位/非空函数指针进行探测，从而把实现细节与引擎更替隔离开来。([Node.js][1])

**握手约定**
`BML_GetInterface(major, minor, &features, &vtable)`：主版本不兼容直接拒绝；次版本仅允许追加函数；调用方必须在使用前检查相应指针非空与 feature 位。该模式正是 Node-API 用来“隔离原生扩展与底层 JS 引擎变更”的惯常手法。([Node.js][1])

---

## 4.2 IMC v2：有界、可背压、可观测

**语义**

* 三类原语：`Event`（广播）、`Call`（请求/响应）、`Stream`（长会话/高频）。
* 每个 Topic 对应**固定容量 RingBuffer**；生产/消费进度由 **Sequence** 驱动，消费者集合以 **Gating Sequences** 反压生产者，防止未消费数据被覆盖。队列策略可选：`block` / `drop-oldest` / `drop-newest` / `priority-shed`。([Lmax Exchange][2])

**为何采用这套模型**
LMAX Disruptor 的用户指南与 API 明确了 RingBuffer/Sequence/Barrier 的并发语义与“gating sequences”背压机制；将其固化为 Runtime 的契约，可把高吞吐事件流的延迟与内存占用控制在可预测范围内。([Lmax Exchange][2])

**可观测性**
Runtime 为每个 Topic 暴露指标：`published/delivered/dropped/queue_depth` 与时延分位（p50/p95/p99），用于定位抖动与背压策略是否合适。上述做法与 Disruptor 的“以 Sequence 为核心的流量控制”一致。([Lmax Exchange][2])

---

## 4.3 能力/权限（Capabilities）

* **静态声明 + 运行时授予**：Mod 清单声明所需能力（如 `imc.admin`, `handle.map`, `script.fs`）；Loader 决策后将授予集传入 Runtime。
* **统一拦截点**：IMC 管理面、句柄映射、脚本宿主 Host-API 均在 Runtime 侧做能力校验，脚本侧再由 Script-Host 二次校验（双层门）。
* **最小可用集**：默认最小化、按需逐项授予，贴近现代扩展平台的安全基线（与“ABI 稳定但功能可探测”相协调）。([Node.js][1])

---

## 4.4 调度与时间预算

Runtime 提供轻量调度器：

* **时间片**：对 Script-Host 的每个 VM 调用 `Pump(budget_ms, max_events)`；用 **中断回调**在超时后可打断执行并记账（interrupts++）。
* **QuickJS 友好**：QuickJS 原生支持 **内存上限**（`JS_SetMemoryLimit`）、**栈上限**（`JS_SetMaxStackSize`）与 **中断处理器**（`JS_SetInterruptHandler`）；Runtime 的调度器与这些钩子配合即可对脚本执行进行硬/软限流。([Bellard][3])

---

## 4.5 资源与句柄系统

**目标**：避免裸指针/悬垂引用，支持热重载而句柄稳定。

**设计**

* 句柄是 64-bit **不透明 Key**，含 `{type | index | generation}`。
* 采用**代际索引**（删除时代数自增，旧句柄即失效），常见实现为 **slotmap/arena**。Godot 的 **RID** 是成熟的“句柄访问低层资源”的参考。([docs.godotengine.org][4])
* **极端回卷防护**：长期运行中 generation 有回卷风险；可使用带“**防重复 Key**”策略的实现（如 *slotmap-careful*）以避免 2³¹ 次复用后的碰撞与安全隐患。([docs.rs][5])

---

## 4.6 Scripting SPI（与 Script-Host 的接口）

Runtime 不嵌入脚本引擎，仅暴露极小 SPI：**创建/销毁 VM**、**注册宿主函数**、**模块加载**、**驱动微任务与入站 IMC**、**请求/响应桥接**、**获取指标**。
默认 Script-Host（QuickJS）在其实现内开启内存/栈上限与中断钩子，并提供 ESModule/原生模块加载；这些能力均来自官方文档与头文件定义。([Bellard][3])

---

## 4.7 激活代理（Activation Broker）

Runtime 订阅 Adapter-Mods 发布的标准化底层事件（如 `Level:Loaded:*`, `Render:*`），将其与 Loader/清单中的激活表达式匹配，在命中时启动相应 Mod。该“事件→激活”的懒加载模式与 VS Code 的 `activationEvents` 实践一致，能显著降低冷启动成本。([npm][6])

---

## 4.8 可观测性与诊断

* **日志/Tracing**：统一前缀、可关联到 Topic/Mod/VM。
* **指标面板**：IMC 主题指标 + 句柄池使用率 + Script-Host VM 指标（CPU ms/alloc/gc/interrupts/drops）。
* **故障策略**：VM 连续超时→隔离（quarantine）；IMC 长期高水位→自动从 `block` 切换到 `drop-oldest` 并告警。
  这些做法让运行态的背压、脚本行为与资源泄漏可被观察与调优，而不破坏 ABI 稳定边界。([Lmax Exchange][2])

---

## 4.9 小结

Runtime 专注于**稳定 ABI**与**强语义运行时原语**：IMC v2（RingBuffer+gating）、句柄化资源（代际/回卷防护）、能力系统、脚本时间片与中断、激活代理与指标。脚本引擎通过 Script-Host 插拔，默认以 QuickJS 实现其配额与打断；整个设计把可变的引擎和 Hook 细节留在外圈，把“难变的内核契约”凝固在中心，从而既可演进又长期可维护。([Node.js][1])

[1]: https://nodejs.org/api/n-api.html?utm_source=chatgpt.com "Node-API | Node.js v25.0.0 Documentation"
[2]: https://lmax-exchange.github.io/disruptor/user-guide/index.html?utm_source=chatgpt.com "LMAX Disruptor User Guide"
[3]: https://bellard.org/quickjs/quickjs.html?utm_source=chatgpt.com "QuickJS Javascript Engine"
[4]: https://docs.godotengine.org/en/stable/classes/class_rid.html?utm_source=chatgpt.com "RID — Godot Engine (stable) documentation in English"
[5]: https://docs.rs/slotmap-careful?utm_source=chatgpt.com "slotmap_careful - Rust"
[6]: https://www.npmjs.com/package/node-addon-api/v/5.0.0?utm_source=chatgpt.com "node-addon-api module"


# 5. Script-Host SPI（脚本宿主服务提供接口）

## 5.1 设计意图与位置

脚本引擎不进入 Runtime 内核；取而代之的是**可替换的 Script-Host Mod**。Runtime 仅依赖一组极小的 **C ABI** 接口（SPI）去创建/销毁 VM、注册宿主 API、加载模块、驱动事件与微任务、桥接 IMC，并拉取指标。这样保持 Runtime **ABI 长期稳定**、引擎可插拔，原则与 Node-API“用稳定 C ABI 隔离底层 JS 引擎变更”一致。([Node.js][1])

---

## 5.2 对默认实现的能力要求（QuickJS 参考线）

作为官方默认 Script-Host，实现需满足：

* **资源配额**：为每个 `JSRuntime` 设置**内存上限**与**栈上限**，并可自定义分配器（`JS_SetMemoryLimit` / `JS_SetMaxStackSize` / `JS_NewRuntime2`）。([Bellard][2])
* **可中断执行**：通过 **中断回调**在时间片用尽时打断并恢复（`JS_SetInterruptHandler`）。([LibGpac][3])
* **模块系统**：支持 **ES6/ES202x 模块**与本地 C 模块（QuickJS原生支持 ES6 modules、可动态/静态链接；官方示例 `bjson.so`）。([Bellard][2])
* **现代 JS 语义**：覆盖 ES2023 能力（模块、异步生成器、BigInt 等），满足脚本作者期望的语法面。([blog.nginx.org][4])

---

## 5.3 SPI 总览（C ABI，版本可探测）

Runtime 在装载 Script-Host 后做一次**版本与能力握手**（主版本不兼容即拒绝；次版本只追加函数/位标）。这一做法直接借鉴了 Node-API 的“**ABI 稳定 + 函数表尾插 + feature bits**”。([Node.js][1])

**握手与 VM 生命周期**

```c
typedef struct {
  uint32_t api_major;      // 主版本（不兼容则拒绝）
  uint32_t api_minor;      // 次版本（仅追加）
  uint64_t feature_bits;   // e.g. SHOST_FEAT_ESM | SHOST_FEAT_SOURCEMAP
} BML_SHost_Info;

typedef void* BML_VM;

int   BML_SHost_GetInfo(BML_SHost_Info* out);                         // 版本/特性
BML_VM BML_SHost_CreateVM(const BML_SHost_VMOptions* opts);           // 创建VM
void  BML_SHost_DestroyVM(BML_VM vm);                                 // 销毁VM
```

`BML_SHost_VMOptions` 至少包含：`mem_limit_bytes`、`stack_limit_bytes`、`time_slice_ms`、`vm_name`。QuickJS 侧需把这些映射到 `JS_SetMemoryLimit`、`JS_SetMaxStackSize` 与中断回调。([Bellard][2])

**Host API 注册与模块加载**

```c
typedef int (*BML_HostFn)(const uint8_t* in, uint32_t in_len,
                          uint8_t** out, uint32_t* out_len, void* user);

int BML_SHost_RegisterHostFn(BML_VM vm, const char* name, BML_HostFn fn, void* user);

int BML_SHost_AddModule(BML_VM vm, const char* id,
                        const uint8_t* code, uint32_t len,
                        const char* mime /*"js"|"json"*/);

int BML_SHost_RunMain(BML_VM vm, const char* entry_id);
```

QuickJS 必须将 `id` 作为 ESModule 解析根（启用原生 ES6 modules），并支持加载本地 C 模块的桥接路径以兼容少量性能敏感扩展。([Bellard][2])

**驱动执行与 IMC 桥接**

```c
int BML_SHost_Pump(BML_VM vm, uint32_t budget_ms, uint32_t max_events);

int BML_SHost_SendEvent(BML_VM vm, const char* topic,
                        const uint8_t* blob, uint32_t len);

int BML_SHost_Call(BML_VM vm, const char* topic,
                   const uint8_t* req, uint32_t req_len,
                   uint8_t** resp, uint32_t* resp_len,
                   uint32_t timeout_ms);
```

`Pump()` 必须在 `budget_ms` 用尽时通过 QuickJS 的中断钩子打断当前执行栈并安全返回，从而与 Runtime 的**时间片调度**配合；Event/Call 两类入口把 Runtime 的 **IMC v2** 消息分发到脚本 handler。IMC 的有界/背压语义源于 **Disruptor** 的 RingBuffer + gating sequences 设计。([Lmax Exchange][5])

**指标与调试**

```c
typedef struct { uint64_t cpu_ms, alloc_bytes;
                 uint32_t gc_count, drops, interrupts; } BML_SHost_Metrics;
int BML_SHost_GetMetrics(BML_VM vm, BML_SHost_Metrics* out);
```

指标用于观测脚本负载/中断次数/丢弃情况，配合 IMC 主题指标（发布/投递/丢弃与延迟分位）共同定位瓶颈。IMC 端的度量与“gating sequences 限流”做法见 Disruptor 文档与 API。([Lmax Exchange][5])

---

## 5.4 Runtime ⇄ Script-Host 交互时序（规范）

1. **装载与握手**：Loader 选中 Script-Host → Runtime 调用 `BML_SHost_GetInfo` 校验主/次版本与特性位。([Node.js][1])
2. **创建 VM**：按清单与能力为每个 Mod/命名空间创建 VM，设置内存/栈上限与 `time_slice_ms`。QuickJS 需把配额映射到对应 API。([Bellard][2])
3. **注册宿主 API**：Runtime 暴露最小可用集合（如 `bml.imc.*`、`bml.handle.*`、`bml.now/sleep`），Script-Host 进行名称绑定与能力校验。
4. **加载模块与入口**：Loader 提供产物（JS/JSON/C 模块）；Script-Host 完成解析、装入与 `RunMain`。QuickJS 原生 ESModule 能力保证加载路径合理。([Fuchsia][6])
5. **运行循环**：Runtime 调度器周期性调用 `Pump(budget_ms)`；入站 IMC 通过 `SendEvent/Call` 进入 VM；超时触发中断钩子，记录一次 `interrupts`。([LibGpac][3])
6. **关闭与热重载**：遵循全局“逆序关停”不变量；需要保活状态的，由 Script-Host 暴露 `beforeReload/afterReload` 钩子协调。

---

## 5.5 安全与沙箱约束

* **默认最小权限**：Script-Host 在没有明确授予能力前不得暴露文件/网络/进程 API；所有宿主函数在进入脚本前做 **capabilities gating**。
* **路径/模块加载**：仅允许从 Loader 提供的**虚拟包文件系统**加载；禁止任意磁盘路径 import；C 模块白名单。
* **时间与资源**：强制执行**内存/栈上限**；`Pump()` 的**时间片**必须通过中断回调严格执行。上述配额/中断均由 QuickJS 直接提供 API 支持。([Bellard][2])

---

## 5.6 脚本侧标准宿主 API（最小集合）

在 VM 内默认注入 `bml.*`：

* `bml.imc.publish/subscribe/call` —— 事件/调用桥。**注意**：背压与丢弃策略由 IMC v2 控制；脚本仅感知 Promise/回调是否成功。Disruptor 的 gating sequences 决定可用空间与生产速率。([Lmax Exchange][5])
* `bml.handle.open/close/info` —— 句柄化资源访问（不透明 RID）。
* `bml.now()/sleep(ms)` —— 单调时钟与协作式等待。
* `bml.cap.granted(name)` —— 能力自检，便于降级路径。
  （可选）调试：SourceMap 栈映射与简单 profiler 计数。

---

## 5.7 与激活模型的衔接

Loader/Runtime 的 **Activation Broker** 监听标准化底层事件（`onCommand:*`、`Level:Loaded:*`、`Render:*` 等），命中后创建/唤醒 VM 并调用入口。该流程类比 VS Code 的 `activationEvents` → `activate/deactivate` 生命周期。([code.visualstudio.com][7])

---

## 5.8 错误码与故障处理（摘要）

* `BML_SHOST_E_TIMEOUT`：`Pump` 超时被中断。
* `BML_SHOST_E_OOM`：超出 `mem_limit`。
* `BML_SHOST_E_STACK`：超出 `stack_limit`。
* `BML_SHOST_E_MOD_NOT_FOUND/IMPORT`：模块解析或加载失败。
* **隔离策略**：VM 连续 N 次 `E_TIMEOUT` → 标记为 **quarantine**；由 Loader/工具提醒开发者（保持进程可用）。

---

## 5.9 兼容性与演进

* **向后兼容**：`api_major` 不变时，只能**尾插**函数与特性位；调用方必须做**非空函数指针检查**再使用新增能力，这与 Node-API 的使用契约相同。([Node.js][1])
* **可替换性**：除 QuickJS 外，未来可提供不同引擎的 Script-Host（例如更专注性能或更强沙箱的实现），Runtime 与 Mods **无需重编译**。

---

## 5.10 实施要点（落地清单）

* 以 `BML_SHost_*` 定义 **单一头文件**；在 Loader 的锁文件中对 Script-Host 记录版本与 **完整性哈希**。
* QuickJS 侧严格映射：`JS_SetMemoryLimit` / `JS_SetMaxStackSize` / `JS_SetInterruptHandler`；确保 `Pump` 可重入并在预算用尽时安全返回。([Bellard][2])
* IMC 端与 Script-Host 的桥接遵守 **RingBuffer + gating** 的背压语义；必要时通过指标面板观察 `dropped / p95/p99` 并调参。([Lmax Exchange][5])

> 小结：此 SPI 让 Runtime 维持“**小而稳**”的 ABI 表面，把脚本执行的复杂度（配额、模块、调度、中断、诊断）外包给可替换的 Script-Host；QuickJS 提供的**内存/栈限额与中断**能力，使得我们可以在不牺牲现代 JS 体验的前提下，构建可控、可观测、可演进的脚本生态。([Bellard][2])

[1]: https://nodejs.org/api/n-api.html?utm_source=chatgpt.com "Node-API | Node.js v25.0.0 Documentation"
[2]: https://bellard.org/quickjs/quickjs.html?utm_source=chatgpt.com "QuickJS Javascript Engine"
[3]: https://doxygen.gpac.io/v2.2.1/quickjs_8h.html?utm_source=chatgpt.com "quickjs.h File Reference"
[4]: https://blog.nginx.org/blog/quickjs-engine-support-for-njs?utm_source=chatgpt.com "Modern JavaScript in NGINX: QuickJS Engine Support for njs"
[5]: https://lmax-exchange.github.io/disruptor/user-guide/index.html?utm_source=chatgpt.com "LMAX Disruptor User Guide"
[6]: https://fuchsia.googlesource.com/third_party/quickjs/%2B/refs/heads/main/doc/quickjs.html?utm_source=chatgpt.com "doc/quickjs.html - third_party/quickjs - Git at Google"
[7]: https://code.visualstudio.com/api/references/activation-events?utm_source=chatgpt.com "Activation Events | Visual Studio Code Extension API"


# 6. Adapter-Mods（Hook 也是 Mod）

## 6.1 角色与边界

Adapter-Mod 只做一件事：**把平台/后端的“易变细节”抽象成稳定的事件流**。例如在渲染管线里挂钩 `Present/EndScene`、输入、关卡载入等，随后发布标准化的 `Render:* / Input:* / Level:*` IMC 主题供其它 Mod 消费。它**不**承担资源管理或脚本执行，这些属于 Runtime 与 Script-Host。钩子安装建议使用成熟库（如 **MinHook** 或 **Microsoft Detours**），二者均为生产可用的函数拦截方案（前者轻量开源，后者为微软研究院开源项目）。([GitHub][1])

---

## 6.2 渲染后端抽象与适配

现代 Windows 栈常见 **D3D9 → D3D12** 的翻译层（**D3D9On12**），甚至还会与其它 wrapper/overlay 并存；因此仅以 “D3D9 vtable” 为唯一挂点会脆弱。我们的做法：

* **运行时探测后端**：优先检测是否在 **D3D9On12**（加载到 `d3d9on12.dll`，或通过设备/特征检查），再决定选择 `IDirect3DDevice9::EndScene/Present` 还是 **DXGI SwapChain** 的 `Present` 家族作为挂点。D3D9On12 官方说明其为“把 D3D9 用户态 DDI 映射到 D3D12 的层”，并非另一个 `d3d9.dll`。([GitHub][2])
* **统一对上契约**：无论底层挂在 D3D9 还是 DXGI（D3D11/12），都只向上发布统一的 `Render:BeforeOverlay / Render:Overlay / Render:AfterOverlay` 事件；DXGI 的 swap-chain 负责展示（`IDXGISwapChain::Present/Present1`）。([微软学习][3])
* **与其它 overlay 并存**：社区经验表明 ReShade 等工具会“尽可能钩一切可见后端”，易与其它 DLL 冲突；我们提供“**兼容模式**”（延后装载/导出转发/只针对特定后端启用）以降低串扰。([Reshade][4])

---

## 6.3 Hook 技术路线（建议）

* **MinHook**：轻量、常用、x86/x64 支持，适合对 vtable/导出函数做 detour；项目文档说明了跳转打补丁与不支持过短函数的退让策略。([GitHub][1])
* **Microsoft Detours**：覆盖 x86/x64/ARM/ARM64，典型模式为“在目标函数头部写跳转，原始指令搬到 trampoline”；官方 wiki/文档详述 API 与二进制重写细节。([GitHub][5])
* **典型挂点**：`IDirect3DDevice9::EndScene/Present`、`Direct3DCreate9/Ex`、`IDXGISwapChain::Present/Present1`；在 D3D9 场景中也常通过“创建虚拟设备取 vtable 地址”的方式找到稳定入口。([Stack Overflow][6])

> 规范要求：**不得**在 `DllMain` 安装 detour 或创建线程；只做最小初始化，把实际 hook 延后至 `Start()` 阶段，以遵循 Windows 官方“DllMain 最佳实践”，避免 loader-lock 死锁与不确定行为。([微软学习][7])

---

## 6.4 生命周期与 IMC 事件映射

Adapter-Mod 遵循统一生命周期（Loader 编排）：

1. `Resolve`：探测当前渲染后端（D3D9 / D3D9On12 / D3D11/12+DXGI）与可用挂点。
2. `Start`：按探测结果通过 MinHook/Detours 安装 detour。
3. 运行中：在每次挂点回调里发布标准化 IMC 事件：

   * `Render:BeforeOverlay`（进场信号，用于统计/准备）
   * `Render:Overlay`（供 HUD/调试面板绘制）
   * `Render:AfterOverlay`（收尾；可携带帧计数/时间戳）
     事件通过 **IMC v2** 的 Topic 发送，受背压策略保护（`block / drop-oldest / drop-newest / priority-shed`），指标可观测。([微软学习][3])
4. `Stop`：**逆序**撤销 detour、释放资源、取消订阅。

---

## 6.5 安全加载与并存策略

* **安全装载**：Adapter-Mod 也必须通过 Loader 的安全路径载入（`LoadLibraryEx` + `LOAD_LIBRARY_SEARCH_*`，绝对路径，禁 CWD 搜索）。([微软学习][7])
* **并存/冲突处理**：遇到其它 overlay（如 ReShade、旧版宽屏补丁等）占用同名导出或相同挂点时，提供：

  * 装载顺序策略（延后/提前）；
  * 导出转发（把上游注入的 `d3d9.dll/dxgi.dll` 链上）；
  * 仅启用特定后端的“**选择性钩挂**”。（社区讨论显示 ReShade 默认“能钩就钩”，需通过创建顺序/配置规避双重渲染）([Reshade][4])

---

## 6.6 适配器-到-统一契约（对上只看事件）

对上所有 Mod 与脚本只看到统一的 `Render:* / Input:* / Level:*` 契约：

* **Render**：如上三段式；必要时附带 `swap_chain_desc / backbuffer_size / frame_id` 等标准字段（Schema 固化）。
* **Input**：标准化键鼠/焦点/滚轮事件；
* **Level**：`Level:Loaded:<name>`、`Level:Started`、`Level:Ended`；
  保持“**强 Schema + 弱耦合**”，使其它 Mod 与脚本**无需感知底层到底是 D3D9 还是 DXGI**。

---

## 6.7 诊断与指标

每个 Adapter-Mod 必须上报：

* 启用的后端（D3D9 / D3D9On12 / D3D11/12）、命中挂点、装载顺序；
* IMC 主题指标（发布/投递/丢弃、队列深度、延迟分位 p50/p95/p99）；
* 近似帧时间与 overlay 绘制耗时（帮助定位抖动）；
  统一指标可与 Runtime 面板汇合，支撑压测与现场诊断。（DXGI 与 SwapChain 的呈现模型文档可作为定义时序与“帧呈现点”的依据。）([微软学习][3])

---

## 6.8 测试与基准

* **功能**：在纯 D3D9、D3D9On12、D3D11/12 三种路径分别验证挂点与事件序。([GitHub][2])
* **并存**：与 ReShade/宽屏补丁/dgVoodoo 等并存，验证兼容模式与导出转发行为。([Reshade][8])
* **稳健性**：遵循 DllMain 最佳实践（不在 DllMain 安装/撤销 hook），验证进程启动/退出的确定性。([微软学习][7])
* **性能**：在 IMC v2 不同背压策略下压测 Overlay 发布频率，观察丢弃与延迟分位是否符合阈值。

---

## 6.9 最小实现建议（落地提示）

* 以 **MinHook** 为首选（轻量、开源、集成成本低）；需要更复杂二进制改写或工具链时再选 **Detours**。([GitHub][1])
* 先实现 **DXGI `Present` 挂点**（覆盖 D3D11/12 与 D3D9On12），再补充 D3D9 `EndScene/Present`；对上始终只发 `Render:*`。([GitHub][2])
* 模块模板内置“**探测→安装→发布事件**”三段式骨架与 DllMain 空壳（仅最小初始化）。([微软学习][7])

> 小结：Adapter-Mod 把具体后端（D3D9、D3D9On12、DXGI）的分歧吸收在内，以**统一事件契约**服务上层；结合 MinHook/Detours 的成熟做法与 Windows 官方 DLL 约束，我们可以在**兼容性、稳定性与可观测性**之间取得稳健平衡。([GitHub][1])

[1]: https://github.com/TsudaKageyu/minhook?utm_source=chatgpt.com "TsudaKageyu/minhook: The Minimalistic x86/x64 API ..."
[2]: https://github.com/microsoft/D3D9On12?utm_source=chatgpt.com "microsoft/D3D9On12: The Direct3D9-On-12 mapping layer"
[3]: https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/d3d10-graphics-programming-guide-dxgi?utm_source=chatgpt.com "DXGI overview - Win32 apps"
[4]: https://reshade.me/forum/general-discussion/2255-hook-special-environments-only?utm_source=chatgpt.com "Hook special environments only - Forum"
[5]: https://github.com/microsoft/detours/wiki?utm_source=chatgpt.com "Home · microsoft/Detours Wiki"
[6]: https://stackoverflow.com/questions/8737129/direct3d9-endscene-hook-rendering?utm_source=chatgpt.com "Direct3D9 EndScene hook rendering - c++"
[7]: https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices?utm_source=chatgpt.com "Dynamic-Link Library Best Practices - Win32 apps"
[8]: https://reshade.me/forum/general-discussion/1267-using-additional-dll-files?utm_source=chatgpt.com "Using additional .dll files"


# 7. Mod 开发模型

## 7.1 Mod 类型与入口规范

* **原生 C/C++ Mod**：以**稳定 C ABI**暴露四个最小生命周期入口：`BML_OnLoad() → BML_Resolve(services) → BML_Start() → BML_Stop()`；禁止在 `DllMain` 做重活（创建线程/同步/LoadLibrary）。微软官方明确要求把复杂初始化移出 `DllMain`，以避免 loader-lock 等不确定行为。([Microsoft Learn][1])
* **脚本 Mod（JS/TS）**：运行在 Script-Host（默认 QuickJS）的 VM 内；由宿主注入的 `bml.*` API 与 IMC 交互，生命周期由激活事件驱动（见 7.3）。

> Windows 动态加载安全基线：进程启动调用 `SetDefaultDllDirectories`，每次加载用 `LoadLibraryEx(..., LOAD_LIBRARY_SEARCH_* )` 并提供**绝对路径**，规避 CWD 搜索与 DLL 劫持。([Microsoft Learn][2])

---

## 7.2 清单与按需激活（Activation）

Mod 通过 TOML 清单声明激活事件；当事件发生时 Loader/Runtime 才启动该 Mod，模型等价 VS Code 的 `activationEvents`（如 `onCommand:*`、`onLanguage:*`、文件/工作区事件等）。我们借鉴其经验以最小化冷启动成本与常驻开销。([Visual Studio Code][3])

**示例（片段）**

```toml
[package]
id = "org.ballance.example.hud"
version = "1.0.0"

[activation]
events = ["onGameStart", "onLevelLoaded:*", "onCommand:hud.toggle"]
```

---

## 7.3 JS/TS 脚本 Mod（基于 QuickJS 的约束）

* **语言与模块**：QuickJS 支持 ES 模块、异步生成器、BigInt 等现代特性；原生 C 模块亦可作为扩展（官方实例 `bjson.so`）。([bellard.org][4])
* **资源配额**：每个 VM 必须设置内存/栈上限（`JS_SetMemoryLimit`、`JS_SetMaxStackSize`），并使用中断回调（`JS_SetInterruptHandler`）在时间片用尽时**可打断执行**，保证主循环可进可退。([bellard.org][4])
* **宿主 API**：脚本端通过 `bml.imc.publish/subscribe/call` 与 IMC v2 交互；通过 `bml.handle.*` 操作不透明句柄；所有可触达资源由 **capabilities** 白名单门控（运行期与宿主双层校验）。
* **常见形态**：

  * UI/HUD 脚本：订阅 `Render:Overlay` 做绘制；
  * 关卡逻辑：订阅 `Level:Loaded:* / Level:Ended`；
  * 工具类：暴露命令点（activation 的 `onCommand:*`），仅在触发时加载。([Visual Studio Code][3])

---

## 7.4 C/C++ Mod（Hook/Adapter 与常规原生 Mod）

* **Hook/Adapter-Mod**：使用 MinHook/Detours 安装/撤销 detour；注意：**只在 `Start()` 安装，`Stop()` 撤销**，并在探测 D3D9 / D3D9On12 / DXGI 后向上发布统一 `Render:*` 事件。MinHook 与 Microsoft Detours 均为成熟方案（Detours 支持 x86/x64/ARM/ARM64；MinHook 对小函数打补丁有明确定义与返回码）。([GitHub][5])
* **常规原生 Mod**：以 C ABI 与 Runtime/IMC 交互；需要加载附属 DLL 时，配合 `SetDefaultDllDirectories`/`LoadLibraryEx` 的 **LOAD_LIBRARY_SEARCH_*** 标志，避免路径污染。([Microsoft Learn][2])

---

## 7.5 事件→激活→退出：推荐时序

1. **发现/解析**：Loader 解析清单、求解依赖并写入锁文件；
2. **等待激活**：未触发时不加载 Mod（参照 VS Code 的 `activate/deactivate` 生命周期）；([Visual Studio Code][6])
3. **命中激活事件**：调用 `BML_Resolve()` 注入服务/IMC 句柄 → `BML_Start()`（Hook 安装或脚本 VM 启动）；
4. **运行**：通过 IMC v2 订阅/发布 Topic；
5. **停用/卸载**：`BML_Stop()` 执行逆序清理（撤钩/释放句柄/取消订阅）。

---

## 7.6 安全与诊断要点（开发者须知）

* **DLL 搜索路径**：优先使用 `SetDefaultDllDirectories` 设置全局策略，并在需要时用 `AddDllDirectory`/`LoadLibraryEx` 精细化；避免字符串名不带路径的裸 `LoadLibrary`。([Microsoft Learn][2])
* **DllMain 禁忌**：不要在 `DllMain` 创建线程、同步等待、调用 `LoadLibrary` 系列或做耗时 IO——这是微软“DLL 最佳实践”的硬性建议。([Microsoft Learn][1])
* **Hook 选择**：优先 MinHook（轻量）、需要复杂二进制改写或全平台覆盖时选 Detours（微软维护，MIT 许可）。([GitHub][5])
* **脚本配额**：为每个 VM 设定**内存/栈/时间片**并监控中断/GC/丢弃等指标（Script-Host 提供 metrics）；QuickJS 原生给出了这些控制点。([bellard.org][4])

---

## 7.7 “最小样例”轮廓

**C/C++ Mod：入口骨架**

```c
BML_EXPORT int BML_OnLoad(void){ /* 轻量 */ return 0; }
BML_EXPORT int BML_Resolve(const BML_Services* s){ /* 记录IMC/句柄表 */ return 0; }
BML_EXPORT int BML_Start(void){ /* 安装hook或订阅IMC */ return 0; }
BML_EXPORT void BML_Stop(void){ /* 逆序撤销 */ }
```

> 把任何 detour/线程/同步**移出** `DllMain`，在 `Start()` 再做。依据微软 DLL 最佳实践。 ([Microsoft Learn][1])

**脚本 Mod：激活式入口**

```js
export async function activate(ctx) {
  const sub = bml.imc.subscribe("Render:Overlay", draw);
  ctx.subscriptions.push(() => sub.dispose());
}
function draw(evt){ /* 绘制HUD/统计 */ }
export function deactivate(){ /* 清理 */ }
```

> 激活/停用语义参照 VS Code：当激活事件发生时才 `activate()`，停用或卸载时 `deactivate()`。([Visual Studio Code][6])

---

## 7.8 兼容性与演进

* **ABI**：主版本内新增能力仅**尾插**，调用前做非空指针与 feature 位检查（同 Node-API 的稳定性策略）。([bellard.org][4])
* **脚本引擎可替换**：Script-Host 的 SPI 稳定，默认 QuickJS 也可换成其他引擎实现而不影响 Mod 接口；QuickJS 的 ES 模块与低启动开销适合工具/逻辑类脚本。([fuchsia.googlesource.com][7])

---

**小结**：开发者只需遵循三条红线：**（1）按需激活**（参考 VS Code）；**（2）安全加载与 DllMain 纪律**（参考微软文档）；**（3）脚本配额与中断**（参考 QuickJS API）。在这三条护栏内，C/C++ 与 JS/TS 模组都能以一致的 IMC/句柄模型协作，且在未来引擎/后端演进时保持二进制与行为的稳定性。 ([Visual Studio Code][3])

[1]: https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices?utm_source=chatgpt.com "Dynamic-Link Library Best Practices - Win32 apps"
[2]: https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-setdefaultdlldirectories?utm_source=chatgpt.com "SetDefaultDllDirectories function (libloaderapi.h)"
[3]: https://code.visualstudio.com/api/references/activation-events?utm_source=chatgpt.com "Activation Events | Visual Studio Code Extension API"
[4]: https://bellard.org/quickjs/quickjs.html?utm_source=chatgpt.com "QuickJS Javascript Engine"
[5]: https://github.com/TsudaKageyu/minhook?utm_source=chatgpt.com "TsudaKageyu/minhook: The Minimalistic x86/x64 API ..."
[6]: https://code.visualstudio.com/api/get-started/extension-anatomy?utm_source=chatgpt.com "Extension Anatomy"
[7]: https://fuchsia.googlesource.com/third_party/quickjs/%2B/refs/heads/main/doc/quickjs.html?utm_source=chatgpt.com "doc/quickjs.html - third_party/quickjs - Git at Google"

# 8. 安全与供应链

## 8.1 威胁模型（Threat Model）

* **T1 组件来源不可信**：恶意/被篡改的 Mod、Adapter、Script-Host、Runtime 二进制与资源包。
* **T2 加载链劫持**：DLL 搜索顺序、相对路径、CWD 搜索导致的劫持。
* **T3 扩展越权**：脚本或原生 Mod 访问未授权文件/网络/进程、注入其他模块或篡改游戏状态。
* **T4 资源与稳定性**：脚本或 Mod 造成 OOM、长阻塞、死循环、句柄泄漏；Hook 不当导致崩溃。
* **T5 生态风险**：依赖链污染、版本冲突、供应链掉包（同名不同物）、回滚不一致。
* **T6 共存竞态**：与其他 overlay/injector 并存时互相覆盖钩子或造成呈现死锁。

---

## 8.2 安全设计原则

* **最小化内核面**：Runtime 仅提供稳定原语（IMC/句柄/能力/调度）；脚本与 Hook 都在外圈，便于替换和隔离。
* **显式信任边界**：Loader 负责“发现→校验→装载”；Runtime 负责“能力/权限、配额、中断”；Script-Host 负责“沙箱”。
* **默认拒绝**：无声明即无权限；能力按粒度授予，可运行时撤销。
* **可复现与可审计**：锁文件 + 完整性哈希 + 事件与指标审计。
* **可降级与隔离**：配额触发→中断→隔离（quarantine）；必要时切 OOP 宿主。

---

## 8.3 供应链完整性与签名

* **Lockfile 完整性**：`bml.lock` 必须记录每个工件的 `sha256`，安装阶段逐一校验；失败即拒绝装载。
* **来源策略**：清单可标注来源（registry、git、file）；Loader 支持“官方源白名单”，可启用“仅信任已签名发布”（可选）。
* **签名与审计**（可选强制）：支持在发行时对二进制与资源包进行签名；Loader 验证签名并记录签名者与证书链摘要到审计日志。
* **重放保护**：锁文件记录发布时间戳与版本元信息；支持“冻结窗口”（同一锁在 X 天内不接受新元数据）。

---

## 8.4 安全装载（Windows 关键路径）

* **绝对路径**：所有组件通过绝对路径加载；禁止相对路径与 CWD 搜索。
* **搜索目录**：进程启动即 `SetDefaultDllDirectories`；装载使用 `LoadLibraryEx` 的 `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS`（以及需要时的 `AddDllDirectory`）。
* **磁盘位置**：运行时目录与 Mod 目录分离；可选“只读运行时、可写 Mods”分区策略；在可写目录启用额外校验。
* **DllMain 纪律**：任何 DLL 的 `DllMain` 只做最小化初始化；严禁创建线程、阻塞、调用二次加载、做重 I/O；全部重活延迟到 `Start()`。

---

## 8.5 能力/权限（Capabilities）

* **命名空间**：`imc.admin`, `hook.d3d9`, `script.fs.read`, `script.fs.write`, `script.net.http`, `handle.map`, `overlay.draw`…
* **声明与授予**：

  * 清单 `capabilities.request=[…]`；
  * Loader 根据策略与来源风险评估（签名/白名单/信任级别）生成 `granted` 集合；
  * Runtime 将 `granted` 注入 Mod/Script-Host，并在每个宿主 API 入口强制检查。
* **动态策略**：运行期可撤销或降级能力（例如在多次 OOM/中断后禁止 `script.fs.write`）。
* **可见性**：脚本侧 `bml.cap.granted(name)` 自检，便于做降级路径。

---

## 8.6 脚本沙箱与配额

* **脚本宿主（Script-Host）**：默认不暴露文件/网络/进程；所有宿主 API 都有能力门控；模块加载走“虚拟包文件系统”，拒绝任意磁盘路径。
* **资源配额**：每 VM 配置 `mem_limit / stack_limit / time_slice_ms`；超限返回错误并记账。
* **时间片与中断**：调度器按周期调用 `Pump(budget_ms)`；预算耗尽即触发中断回调打断执行（不中断则视为违规）。
* **故障分级**：

  * 软故障：超时/中断/单次 OOM → 警告与指标；
  * 重复故障：N 次超时或 OOM → **quarantine**（停用脚本 Mod，保留诊断）；
  * 严重故障：崩溃 → 自动恢复（可选 OOP 宿主更强恢复）。

---

## 8.7 Hook/Adapter 安全

* **后端探测**：装载前先探测 D3D9/D3D9On12/DXGI，避免挂错；启动后仅发布统一 `Render:*` 事件。
* **安装/撤销**：只在 `Start()` 安装 detour，在 `Stop()` 撤销；异常与未撤钩在卸载时强制回滚。
* **并存策略**：与外部 overlay 并存时启用“兼容模式”：延后装载、导出转发、选择性钩挂；对冲突挂点给出明确告警与禁用选项。

---

## 8.8 IMC v2 的安全与稳健性

* **有界队列**：每 Topic 固定容量 RingBuffer；防止无限增长导致内存压力。
* **背压策略**：按 Topic 配置 `block / drop-oldest / drop-newest / priority-shed`；关键控制流默认 `block`，UI/遥测默认 `drop-oldest`。
* **零拷贝与拷贝策略**：默认复制小消息；大消息采用只读视图（受能力控制），避免异常路径写入。
* **隔离与降级**：连续高水位→自动降级策略（例如从 `block` 切到 `drop-oldest`），并发出告警与指标事件。

---

## 8.9 审计与可观测性

* **审计日志**：装载/卸载、签名校验结果、来源信息、授予能力、超时/OOM/中断、IMC 策略切换、热重载与回滚。
* **指标面板**：

  * IMC：`published/delivered/dropped/queue_depth/latency p50/p95/p99`；
  * Script-Host/VM：`cpu_ms/alloc/gc/interrupts/drops`；
  * 句柄池：已用/容量、回收速率；
  * Adapter：后端类型、挂点命中、overlay 耗时。
* **导出**：支持文本/JSON/Prometheus 格式；为自动化测试与线上报警服务。

---

## 8.10 依赖、锁定与回滚策略

* **PubGrub 求解**：遇到冲突给出人类可读解释，便于修复与“有意识升级/降级”。
* **锁定与安装**：安装严格遵循 `bml.lock`；若某组件完整性失败或签名无效则整体失败。
* **热重载**：遵循拓扑**逆序关停**；重装失败→整体回滚到上一锁版本；回滚也记录在审计日志。
* **冻结发布**：生产环境使用冻结锁（只读）；变更走“新锁 + A/B 命名空间”，回滚快捷。

---

## 8.11 OOP 宿主（可选隔离）

* **触发条件**：高风险脚本、低信任来源、频发崩溃的 Mod。
* **通信**：命名管道/共享内存 + IMC 桥；SPI 调用通过 IPC stub 复用相同接口。
* **恢复**：子进程崩溃自动重启；重启前 Drain/丢弃策略可配置（保守/激进）。

---

## 8.12 安全清单（Checklist）

* Loader

  * [ ] 仅绝对路径 + `LoadLibraryEx(… LOAD_LIBRARY_SEARCH_*)`
  * [ ] 启动即 `SetDefaultDllDirectories`
  * [ ] 校验 `sha256` 与（可选）签名
  * [ ] 生成审计日志与指标
* Runtime

  * [ ] 能力检查贯穿 IMC/句柄/宿主 API
  * [ ] IMC v2 有界 + 策略 + 指标
  * [ ] 调度 + 时间片 + 超时中断
  * [ ] 句柄代际 + 回卷防护
* Script-Host

  * [ ] 每 VM 配额（内存/栈/时间片）
  * [ ] 默认无 I/O/网络；仅能力白名单开放
  * [ ] `Pump()` 可中断、可重入
* Adapter-Mod

  * [ ] DllMain 零重活；Start/Stop 对称
  * [ ] 后端探测与统一 `Render:*` 事件
  * [ ] 并存兼容模式与告警
* 发布与回滚

  * [ ] `bml.lock` 完整性与冻结
  * [ ] 失败回滚与审计记录

---

## 8.13 事件与告警（建议阈值）

* **IMC 丢弃率**：> 1%（警告） / > 5%（高危）
* **VM 中断次数**：每分钟 > 10（警告） / > 50（高危，触发 quarantine）
* **句柄泄漏趋势**：连续 5 分钟增长且未释放（警告）
* **签名/完整性失败**：立即阻断 + 高危告警

---

### 小结

这一章将**供应链完整性、能力门控、脚本沙箱、IMC 有界背压、Windows 安全装载**与**可观测/回滚**串成一条线：**先阻断外部风险，再限制内部损害，最后用审计与回滚收尾**。配合前述微内核架构与 Script-Host 插拔，BML 的扩展生态可以在高可定制与高安全之间取得稳定平衡。

