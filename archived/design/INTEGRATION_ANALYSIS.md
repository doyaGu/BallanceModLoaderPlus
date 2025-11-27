# BML v0.4.0 系统集成深度分析报告

> **创建日期**: 2025-11-23  
> **分析目的**: 全面审查当前实现，制定基于实际代码的集成计划  
> **分析方法**: 源代码审查 + 架构分析 + 依赖关系图

---

## 📊 执行摘要

**当前状态**: 两个独立系统，未集成

1. **BML.dll (Core微内核)** - 完整实现但孤立
2. **BMLPlus.dll (Legacy主DLL)** - v0.3架构，完全独立
3. **集成度**: 0% - 无任何代码调用或数据交换

**关键发现**:
- ✅ Core微内核架构设计完整且实现正确
- ❌ BMLPlus.dll完全未意识到Core的存在
- ❌ 构建系统未建立链接关系
- ⚠️ 独立模块（BML_Input/Render）依赖不存在的事件源

---

## 🔍 第一部分：当前架构深度分析

### 1.1 BML.dll (Core微内核) - 完整实现审查

#### 1.1.1 入口点分析

**文件**: `src/Core/Export.cpp`
```cpp
extern "C" BML_API void *bmlGetProcAddress(const char *proc_name);
extern "C" BML_API BML_Result bmlAttach(void);
extern "C" BML_API BML_Result bmlLoadModules(void);
extern "C" BML_API void bmlDetach(void);
```

**实现**: ✅ 完整
- `bmlAttach()` → `BML::Core::DiscoverModules()`
- `bmlLoadModules()` → `BML::Core::LoadDiscoveredModules()`
- `bmlDetach()` → `BML::Core::ShutdownMicrokernel()`

**DllMain**: `src/Core/DllMain.cpp` - 仅7行，什么都不做
```cpp
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*reserved*/) {
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(module);
    return TRUE;
}
```

**结论**: Core设计为**被动库**，需要外部主动调用初始化。

#### 1.1.2 微内核流水线

**文件**: `src/Core/Microkernel.cpp` (323行)

**Phase 1: DiscoverModules()**
```cpp
bool DiscoverModules() {
    auto mods_dir = DetectModsDirectory();  // 检测 Mods/ 目录
    auto &state = State();
    return state.runtime.DiscoverAndValidate(mods_dir, state.diagnostics);
}
```

**执行流程**:
1. `DetectModsDirectory()` - 环境变量或exe同级Mods/
2. `ModuleRuntime::DiscoverAndValidate()`:
   - `LoadManifestsFromDirectory()` - 扫描mod.toml
   - `BuildLoadOrder()` - 依赖解析+拓扑排序
   - `Context::RegisterManifest()` - 缓存清单
3. **不加载DLL** - DllMain安全

**Phase 2: LoadDiscoveredModules()**
```cpp
bool LoadDiscoveredModules() {
    auto &state = State();
    return state.runtime.LoadDiscovered(state.diagnostics);
}
```

**执行流程**:
1. `ModuleLoader::LoadModules()`:
   - 按拓扑顺序`LoadLibraryW()`
   - 查找`BML_ModEntrypoint`导出
   - 调用`entrypoint(BML_ATTACH, ...)`
   - 失败则回滚（逆序`DETACH` + `FreeLibrary`）
2. `Context::AddLoadedModule()` - 缓存句柄
3. `HotReloadMonitor::UpdateWatchList()` - 监控mod.toml变化

**关键API**: `ModuleRuntime` (424行)
- `DiscoverAndValidate()` - Phase 1实现
- `LoadDiscovered()` - Phase 2实现
- `ReloadModules()` - 热重载支持
- `Shutdown()` - 清理

**结论**: 流水线设计完整，支持两阶段加载和热重载。

#### 1.1.3 IMC总线实现

**文件**: `src/Core/ImcBus.cpp` (1168行)

**核心API**:
```cpp
class ImcBus {
    BML_Result Publish(const char *topic, const void *payload, size_t len, ...);
    BML_Result Subscribe(const char *topic, BML_ImcPubSubHandler handler, ...);
    void Pump(size_t max_messages_per_subscription);
    // ... RPC/Future相关
};
```

**数据结构**:
```cpp
struct BML_Subscription_T {
    std::string topic;
    BML_ImcPubSubHandler handler;
    void *user_data;
    std::unique_ptr<MpscRingBuffer<QueuedMessage*>> queue;  // 每订阅者独立队列
    std::atomic<uint32_t> ref_count;
    std::atomic<bool> closed;
};
```

**Publish流程**:
1. 查找该topic的所有订阅者
2. 复制payload到`QueuedMessage`（inline/heap/external）
3. 入队到每个订阅者的MPSC队列
4. **不立即调用回调** - 等待Pump

**Pump流程**:
```cpp
void Pump(size_t max_messages = 0) {
    for (auto& sub : subscriptions) {
        size_t count = 0;
        while (auto* msg = sub->queue->Dequeue()) {
            sub->handler(msg->Data(), msg->Size(), &msg->info, sub->user_data);
            delete msg;
            if (++count >= max_messages && max_messages != 0)
                break;
        }
    }
}
```

**问题识别**:
- ✅ 无锁队列实现正确（`MpscRingBuffer.h`）
- ❌ **Pump()从未被调用** - 搜索结果：0次调用
- ⚠️ 订阅者回调永远不会触发

**结论**: IMC总线完整实现，但缺少驱动循环。

#### 1.1.4 模组加载器

**文件**: `src/Core/ModuleLoader.cpp` (356行)

**LoadModules流程**:
```cpp
bool LoadModules(const std::vector<ResolvedNode> &order,
                 Context &context,
                 PFN_BML_GetProcAddress get_proc,
                 std::vector<LoadedModule> &out_modules,
                 ModuleLoadError &out_error) {
    for (const auto &node : order) {
        HMODULE dll = LoadLibraryW(path.c_str());
        if (!dll) { /* 错误处理 */ }
        
        auto entrypoint = (PFN_BML_ModEntrypoint)GetProcAddress(dll, "BML_ModEntrypoint");
        if (!entrypoint) { /* 错误 */ }
        
        // 设置TLS当前模组
        Context::SetCurrentModule(mod_handle.get());
        
        BML_Result result = entrypoint(BML_ATTACH, ctx_handle, get_proc);
        if (result != BML_RESULT_OK) {
            // 回滚：逆序DETACH + FreeLibrary
        }
        
        out_modules.push_back({id, manifest, dll, entrypoint, path, std::move(mod_handle)});
    }
}
```

**关键机制**:
1. **TLS当前模组** - 允许模组初始化时调用`bmlGetModId(nullptr)`
2. **原子回滚** - 失败时自动清理已加载的模组
3. **BML_Mod句柄** - 统一管理模组元数据

**结论**: 加载器实现健壮，错误处理完善。

#### 1.1.5 依赖解析器

**文件**: `src/Core/DependencyResolver.cpp` (121行)

**算法**: Kahn拓扑排序 + 最小堆（确定性顺序）
```cpp
bool BuildLoadOrder(const ManifestLoadResult &manifests,
                    std::vector<ResolvedNode> &out_order,
                    std::vector<DependencyWarning> &out_warnings,
                    DependencyResolutionError &out_error) {
    // 1. 构建依赖图
    // 2. 检测重复ID
    // 3. 检测冲突
    // 4. 拓扑排序（stable）
    // 5. 版本约束验证
}
```

**支持特性**:
- ✅ SemVer约束（>=, ^, ~）
- ✅ 可选依赖
- ✅ 循环检测
- ✅ 冲突规则

**结论**: 生产级依赖解析器。

#### 1.1.6 子系统汇总

| 子系统 | 文件 | 行数 | 状态 | 备注 |
|--------|------|------|------|------|
| ApiRegistry | ApiRegistry.cpp | 125 | ✅ | 线程安全，调用计数 |
| Context | Context.cpp | 250 | ✅ | 句柄管理，TLS |
| ImcBus | ImcBus.cpp | 1168 | ⚠️ | 实现完整但未Pump |
| ModuleLoader | ModuleLoader.cpp | 356 | ✅ | 回滚机制健壮 |
| DependencyResolver | DependencyResolver.cpp | 121 | ✅ | 拓扑排序正确 |
| MemoryManager | MemoryManager.cpp | 200 | ✅ | 池分配+统计 |
| SyncManager | SyncManager.cpp | 300 | ✅ | Mutex/RwLock/Atomics |
| DiagnosticManager | DiagnosticManager.cpp | 150 | ✅ | TLS错误上下文 |
| ProfilingManager | ProfilingManager.cpp | 322 | ✅ | Chrome Tracing |
| ConfigStore | ConfigStore.cpp | 565 | ✅ | Per-mod TOML |
| Logging | Logging.cpp | 304 | ✅ | Per-mod日志文件 |

**总计**: ~8000行代码，19个核心类，116个API。

---

### 1.2 BMLPlus.dll (Legacy主DLL) - 架构分析

#### 1.2.1 入口点分析

**文件**: `src/BML.cpp` (1457行)

**DllMain实现**:
```cpp
BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        g_DllHandle = hModule;
        MH_Initialize();  // MinHook初始化
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        MH_Uninitialize();
    }
    return TRUE;
}
```

**观察**:
- ✅ 使用MinHook做函数钩子
- ❌ **未调用Core任何API**
- ❌ 未调用`bmlAttach()`

**导出的旧API**:
```cpp
void BML_GetVersion(int *major, int *minor, int *patch);
void *BML_Malloc(size_t size);  // 直接用malloc
void BML_Free(void *ptr);       // 直接用free
char *BML_Strdup(const char *str);
// ... 20+个v0.3导出函数
```

**问题**:
- 未使用`MemoryManager`
- 未使用`bmlGetProcAddress`
- 完全独立的API命名空间

#### 1.2.2 ModContext - 核心管理器

**文件**: `src/ModContext.cpp` (1709行)

**生命周期**:
```cpp
bool ModContext::Init() {
    InitDirectories();
    InitLogger();
    // 初始化Oniguruma正则
    // 获取CK2 Managers
    // 初始化Hooks (MinHook)
    // 创建ImGui上下文
    SetFlags(BML_INITED);
}

bool ModContext::LoadMods() {
    RegisterBuiltinMods();  // BMLMod, NewBallTypeMod
    
    // 扫描 Mods/ 目录
    std::wstring path = m_LoaderDir + L"\\Mods";
    ExploreMods(path, modPaths);
    
    for (auto &modPath : modPaths) {
        IMod *mod = LoadMod(modPath);  // LoadLibraryW + GetProcAddress("BMLEntry")
        if (mod) {
            mod->OnLoad();
            m_Mods.push_back(mod);
        }
    }
    
    SetFlags(BML_MODS_LOADED);
}
```

**LoadMod实现**:
```cpp
IMod *ModContext::LoadMod(const std::wstring &path) {
    HMODULE dll = LoadLibraryW((path + L"\\MyMod.dll").c_str());
    if (!dll) return nullptr;
    
    typedef IMod *(*BMLEntry)(IBML *);
    auto entry = (BMLEntry)GetProcAddress(dll, "BMLEntry");
    if (!entry) return nullptr;
    
    IMod *mod = entry(this);  // 传递ModContext作为IBML*
    return mod;
}
```

**关键观察**:
- ❌ 未检查`mod.toml`
- ❌ 未调用`ModuleDiscovery`
- ❌ 未使用`DependencyResolver`
- ❌ 未调用`BML_ModEntrypoint` (v0.4入口点)
- ✅ 调用旧的`BMLEntry()` + `IMod::OnLoad()`

**ModContext职责**:
1. 模组生命周期管理
2. 命令注册与执行
3. 配置加载/保存（使用旧的Config类）
4. 依赖关系管理（手动注册）
5. 日志管理
6. DataShare实例管理

**问题汇总**:
- 完全独立的模组加载流程
- 不支持v0.4模组（无mod.toml解析）
- 不支持依赖解析
- 不支持版本约束

#### 1.2.3 ModManager - CK2集成层

**文件**: `src/ModManager.cpp` (110行)

**关键回调**:
```cpp
CKERROR ModManager::OnCKPlay() {
    // 游戏开始运行
    m_RenderContext = m_Context->GetPlayerRenderContext();
    Overlay::ImGuiInitRenderer(m_Context);
    
    m_ModContext->LoadMods();   // ← 加载v0.3模组
    m_ModContext->InitMods();   // 调用 IMod::OnInit()
}

CKERROR ModManager::PostProcess() {
    Overlay::ImGuiNewFrame();
    PhysicsPostProcess();
    
    m_ModContext->OnProcess();  // 调用所有 IMod::OnProcess()
    
    // 处理ImGui光标
    // 处理输入
    Overlay::ImGuiRender();
    inputHook->Process();
    
    return CK_OK;
}

CKERROR ModManager::OnPostRender(CKRenderContext *dev) {
    m_ModContext->OnRender(dev);  // 调用 IMod::OnRender()
}
```

**观察**:
- ❌ **未调用`bmlLoadModules()`**
- ❌ **未调用`ImcBus::Pump()`**
- ✅ 正确集成CK2生命周期
- ✅ ImGui渲染流程正确

#### 1.2.4 InputHook - 输入管理

**文件**: `src/InputHook.cpp` (约300行)

**当前实现**:
```cpp
void InputHook::Process() {
    // 处理键盘/鼠标输入
    for (auto& callback : m_KeyCallbacks) {
        if (IsKeyPressed(callback.key)) {
            callback.fn(this);
        }
    }
}
```

**问题**:
- ❌ **未发布IMC事件**
- ❌ 未调用`bmlImcPublish("BML/Input/KeyDown", ...)`
- ✅ 回调机制工作正常（v0.3模组使用）

#### 1.2.5 RenderHook - 渲染钩子

**文件**: `src/RenderHook.cpp` (约200行)

```cpp
void ExecutePreRenderHooks(CKRenderContext* dev) {
    for (auto& hook : s_PreRenderHooks) {
        hook(dev);
    }
}

void ExecutePostRenderHooks(CKRenderContext* dev) {
    for (auto& hook : s_PostRenderHooks) {
        hook(dev);
    }
}
```

**问题**:
- ❌ **未发布IMC事件**
- ❌ 未调用`bmlImcPublish("BML/Game/PreRender", ...)`

#### 1.2.6 UI系统审查

**HUD系统**: `src/HUD.cpp` (约800行)
- 基于ImGui的可编程HUD
- 支持Text/Image/ProgressBar/Container
- 支持ANSI彩色文本（AnsiText.cpp）
- 完全独立实现

**CommandBar**: `src/CommandBar.cpp` (约200行)
- 命令行界面
- 命令历史
- 自动补全

**ModMenu**: `src/ModMenu.cpp` (约300行)
- 模组配置UI
- 树状菜单结构

**MessageBoard**: `src/MessageBoard.cpp` (约150行)
- 临时消息显示
- 自动淡出

**观察**:
- ✅ UI系统完整且功能强大
- ❌ 未注册为Core扩展
- ❌ 不支持通过IMC控制

---

### 1.3 独立模块分析

#### 1.3.1 BML_Input模组

**文件**: `modules/BML_Input/InputMod.cpp` (168行)

**初始化代码**:
```cpp
BML_Result BML_ENTRYPOINT(BML_ATTACH, BML_Context ctx, PFN_BML_GetProcAddress get_proc) {
    // 加载API
    if (!BML_LoadAPI(get_proc)) {
        return BML_RESULT_FAIL;
    }
    
    // 订阅输入事件
    bmlImcSubscribe("BML/Input/KeyDown", OnKeyDown, nullptr, &g_KeyDownSub);
    bmlImcSubscribe("BML/Input/KeyUp", OnKeyUp, nullptr, &g_KeyUpSub);
    bmlImcSubscribe("BML/Input/MouseMove", OnMouseMove, nullptr, &g_MouseMoveSub);
    bmlImcSubscribe("BML/Input/MouseButton", OnMouseButton, nullptr, &g_MouseButtonSub);
    
    // 注册扩展API
    BML_ExtensionAPI api = { /* BML_EXT_Input v1.0 */ };
    bmlExtensionRegister("BML_EXT_Input", &ver1_0, "Input control API", &api, sizeof(api));
    
    return BML_RESULT_OK;
}
```

**回调实现**:
```cpp
void OnKeyDown(const void *payload, size_t size, const BML_ImcMessageInfo *info, void *user_data) {
    auto *event = (const KeyEvent*)payload;
    // 处理按键事件
    OutputDebugStringA("[BML_Input] Key pressed\n");
}
```

**问题**:
- ✅ 代码正确，订阅机制正确
- ❌ **永远不会接收到事件** - BMLPlus不发布
- ❌ 扩展API注册成功但无人调用

#### 1.3.2 BML_Render模组

**文件**: `modules/BML_Render/RenderMod.cpp` (82行)

```cpp
BML_Result BML_ENTRYPOINT(BML_ATTACH, ...) {
    bmlImcSubscribe("BML/Game/PreRender", OnPreRender, nullptr, &g_PreRenderSub);
    bmlImcSubscribe("BML/Game/PostRender", OnPostRender, nullptr, &g_PostRenderSub);
    return BML_RESULT_OK;
}
```

**问题**: 同BML_Input，事件源不存在。

---

### 1.4 构建系统分析

#### 1.4.1 CMakeLists.txt结构

**根目录**: `CMakeLists.txt`
```cmake
project(BallanceModLoaderPlus)
add_subdirectory(src)
add_subdirectory(modules)
add_subdirectory(tests)
```

**src/CMakeLists.txt**:
```cmake
add_subdirectory(Core)  # 生成 BML.dll

add_library(BMLPlus SHARED
    ${BML_SOURCES}
    ${IMGUI_SOURCES}
)

target_link_libraries(BMLPlus
    PUBLIC CK2 VxMath
    PRIVATE BMLUtils minhook onig
    # ❌ 缺少: BML (Core微内核)
)
```

**问题**:
- ❌ **BMLPlus未链接BML.dll**
- ❌ 无法调用`bmlGetProcAddress`等函数
- ❌ 链接器无法找到符号

#### 1.4.2 输出结构

**build/bin/Release/**:
```
BML.dll              # Core微内核
BMLPlus.dll          # Legacy主DLL
BML_Input.dll        # 独立模组
BML_Render.dll       # 独立模组
BMLCoreDriver.exe    # Core测试程序
```

**问题**:
- BML.dll和BMLPlus.dll完全独立
- 运行时无依赖关系
- Player.exe加载BMLPlus.dll，BML.dll从未加载

---

## 🎯 第二部分：集成障碍识别

### 2.1 技术障碍清单

| # | 障碍 | 影响 | 复杂度 | 优先级 |
|---|------|------|--------|--------|
| 1 | BMLPlus未链接BML.dll | 🔴 | 低 | P0 |
| 2 | DllMain未调用bmlAttach | 🔴 | 低 | P0 |
| 3 | ModContext::LoadMods未调用Core | 🔴 | 中 | P0 |
| 4 | InputHook未发布IMC事件 | 🔴 | 中 | P1 |
| 5 | RenderHook未发布IMC事件 | 🔴 | 中 | P1 |
| 6 | PostProcess未调用ImcBus::Pump | 🔴 | 低 | P1 |
| 7 | v0.3/v0.4模组加载冲突 | 🟡 | 高 | P2 |
| 8 | UI系统未注册Core扩展 | 🟡 | 中 | P3 |
| 9 | 内存分配未统一 | 🟢 | 低 | P4 |

### 2.2 架构冲突分析

#### 2.2.1 模组加载冲突

**v0.3模组**:
- 导出`BMLEntry(IBML*)`
- 接收`ModContext*`作为IBML接口
- 调用`IMod::OnLoad/OnInit/OnProcess/OnRender`
- 手动注册依赖关系

**v0.4模组**:
- 导出`BML_ModEntrypoint(BML_ModPhase, BML_Context, PFN_BML_GetProcAddress)`
- 接收`BML_Context`（Core的Context句柄）
- 使用IMC订阅事件
- `mod.toml`声明依赖

**冲突点**:
1. **入口点不同**: `BMLEntry` vs `BML_ModEntrypoint`
2. **上下文不同**: `IBML*` vs `BML_Context`
3. **生命周期不同**: 回调函数 vs 事件订阅
4. **依赖系统不同**: 运行时注册 vs 清单声明

**解决方案**:
- 需要**双路径加载**逻辑
- v0.3模组继续用`ModContext::LoadMod`
- v0.4模组用`Core::ModuleLoader`
- 统一到`m_Mods`列表（需要适配器）

#### 2.2.2 生命周期冲突

**v0.3生命周期**:
```
OnLoad → OnInit → OnProcess(每帧) → OnRender → OnShutdown → OnUnload
```

**v0.4生命周期**:
```
BML_ATTACH → 订阅事件 → 事件回调(异步) → BML_DETACH
```

**问题**:
- v0.3是**同步回调**驱动
- v0.4是**异步事件**驱动
- 需要桥接层将v0.4事件转换为v0.3回调

#### 2.2.3 IMC事件协议缺失

**当前状态**:
- BML_Input期望接收`BML/Input/KeyDown`
- BMLPlus的InputHook只调用回调函数
- **事件从未发布**

**需要定义的事件**:
```cpp
// 输入事件
struct KeyEvent {
    uint32_t key;
    uint32_t timestamp;
};
"BML/Input/KeyDown"     → KeyEvent
"BML/Input/KeyUp"       → KeyEvent
"BML/Input/MouseMove"   → { int32_t x, y; uint32_t timestamp; }
"BML/Input/MouseButton" → { uint32_t button; bool down; uint32_t timestamp; }

// 渲染事件
"BML/Game/PreRender"    → { CKRenderContext* dev; }
"BML/Game/PostRender"   → { CKRenderContext* dev; }

// 生命周期事件
"BML/System/Init"       → {}
"BML/Game/Process"      → {}
"BML/System/Shutdown"   → {}
```

---

## 🔧 第三部分：可行性评估

### 3.1 现有代码可复用性

#### 3.1.1 Core微内核 (复用度: 100%)

**无需修改的组件**:
- ApiRegistry - 完全可用
- ImcBus - 完全可用
- ModuleLoader - 完全可用
- DependencyResolver - 完全可用
- 所有子系统（Memory/Sync/Diagnostic/Profiling/Config/Logging）

**需要少量修改**:
- `Microkernel.cpp` - 添加诊断输出回调
- `Export.cpp` - 添加状态查询API

**结论**: Core代码质量高，无需重构。

#### 3.1.2 Legacy BMLPlus (复用度: 80%)

**无需修改的组件**:
- UI系统（HUD/CommandBar/ModMenu/MessageBoard）
- Overlay（ImGui集成）
- 大部分工具函数（StringUtils/PathUtils）
- MinHook集成（HookUtils）

**需要修改的组件**:
1. **ModContext::LoadMods** - 添加v0.4路径
   - 估算: 50行新代码
   - 复杂度: 中等
   
2. **InputHook::Process** - 添加IMC发布
   - 估算: 5个事件 × 3行 = 15行
   - 复杂度: 简单
   
3. **RenderHook** - 添加IMC发布
   - 估算: 2个事件 × 3行 = 6行
   - 复杂度: 简单
   
4. **ModManager::PostProcess** - 添加Pump调用
   - 估算: 1行
   - 复杂度: 简单

**结论**: Legacy代码大部分保留，改动集中在5个文件。

#### 3.1.3 独立模块 (复用度: 100%)

**BML_Input/BML_Render**:
- 代码完全正确
- 等待BMLPlus发布事件即可工作
- 无需修改

### 3.2 性能影响评估

#### 3.2.1 IMC事件开销

**Publish开销**:
- 查找订阅者: O(n) where n=订阅者数量
- 复制payload: 小于256字节用内联，否则堆分配
- 入队: MPSC无锁，O(1)
- **估算**: <10μs per event

**Pump开销**:
- 遍历所有订阅者队列
- 调用回调函数
- **估算**: <100μs per frame（10个订阅者）

**每帧事件数**:
- 输入: 0-10个（按键/鼠标）
- 渲染: 2个（PreRender/PostRender）
- 总计: 2-12个事件

**总开销**: <200μs/frame = 0.2ms/frame

**结论**: 对60fps（16.6ms/frame）影响<2%，可接受。

#### 3.2.2 模组加载性能

**Core加载器**:
- 依赖解析: O(n²) worst case
- DLL加载: 依赖操作系统
- **估算**: 10个模组 ~100ms

**Legacy加载器**:
- 目录扫描: O(n)
- DLL加载: 同上
- **估算**: 10个模组 ~80ms

**双路径开销**: +20ms（可接受）

### 3.3 兼容性风险

#### 3.3.1 ABI兼容性

**v0.3模组依赖**:
- `BMLEntry(IBML*)` - ✅ 保留
- `IMod`接口 - ✅ 保留
- `ModContext`指针 - ✅ 保留
- 所有v0.3 API导出 - ✅ 保留

**风险**: **低** - 完全向后兼容

#### 3.3.2 行为兼容性

**变化点**:
1. 模组加载顺序可能改变（新增v0.4模组）
2. 性能特性略有差异（+0.2ms/frame）
3. 日志格式可能不同

**缓解**:
- 保持v0.3模组加载顺序不变
- 性能差异在误差范围内
- 日志格式保持兼容

**风险**: **低**

---

## 📋 第四部分：实施计划纲要

### 4.1 最小化集成路径（MVP）

**目标**: 让BML_Input能接收到一个按键事件

**步骤**:
1. 修改`src/CMakeLists.txt`链接BML.dll (1行)
2. 修改`src/BML.cpp` DllMain调用bmlAttach/bmlDetach (3行)
3. 修改`src/ModManager.cpp` OnCKPlay调用bmlLoadModules (1行)
4. 修改`src/InputHook.cpp`发布KeyDown事件 (3行)
5. 修改`src/ModManager.cpp` PostProcess调用ImcBus::Pump (1行)

**工作量**: <50行代码，1-2小时

**验收标准**:
- BML_Input模组加载成功
- 按任意键，OutputDebugString输出"[BML_Input] Key pressed"

### 4.2 完整集成路径

**阶段1: 基础连接** (1周)
- Task 1.1: 构建系统链接
- Task 1.2: Core初始化集成
- Task 1.3: 创建CoreBridge抽象层

**阶段2: 事件桥接** (1周)
- Task 2.1: InputHook事件发布
- Task 2.2: RenderHook事件发布
- Task 2.3: 生命周期事件
- Task 2.4: IMC Pump集成

**阶段3: 模组加载统一** (1周)
- Task 3.1: 双路径加载逻辑
- Task 3.2: 模组适配器
- Task 3.3: 依赖关系统一

**阶段4: 测试与优化** (1周)
- Task 4.1: 集成测试
- Task 4.2: 性能基准测试
- Task 4.3: 兼容性测试

**总工期**: 4周

### 4.3 风险应对

| 风险 | 概率 | 影响 | 应对措施 |
|------|------|------|---------|
| v0.3兼容性破坏 | 中 | 高 | 完整回归测试套件 |
| IMC性能问题 | 低 | 中 | 提前基准测试，可降级 |
| 模组加载冲突 | 中 | 中 | 隔离错误，独立路径 |
| 构建系统问题 | 低 | 低 | CMake专家审查 |

---

## 📊 第五部分：决策建议

### 5.1 技术债务评估

**当前技术债务**:
1. 两个独立DLL无集成 - 严重
2. 代码重复（Config/Logging） - 中等
3. 测试覆盖率不足 - 中等
4. 文档不完整 - 中等

**集成收益**:
1. 统一模组系统 - 高价值
2. 依赖管理自动化 - 高价值
3. 热重载支持 - 中价值
4. 性能分析工具 - 中价值

**ROI**: 高 - 建议执行集成

### 5.2 实施建议

**推荐方案**: 渐进式集成

**Phase 1**: MVP集成（1周）
- 验证技术可行性
- 快速反馈
- 风险可控

**Phase 2**: 完整集成（3周）
- 基于MVP经验
- 迭代改进
- 持续测试

**Phase 3**: 优化与文档（2周）
- 性能调优
- 文档完善
- 社区反馈

**总计**: 6周完整落地

### 5.3 不建议方案

**❌ 大爆炸重写**:
- 风险极高
- 周期长（3-6个月）
- 容易失败

**❌ 抛弃Legacy**:
- 破坏v0.3兼容性
- 社区反对
- 不现实

---

## 📝 附录

### A. 关键文件清单

**Core微内核** (src/Core/):
```
Export.cpp (36行) - 导出入口
Microkernel.cpp (323行) - 初始化流程
ApiRegistry.cpp (125行) - API注册表
Context.cpp (250行) - 全局上下文
ImcBus.cpp (1168行) - 消息总线
ModuleLoader.cpp (356行) - 模组加载
DependencyResolver.cpp (121行) - 依赖解析
```

**Legacy主DLL** (src/):
```
BML.cpp (1457行) - DllMain + 旧API导出
ModContext.cpp (1709行) - 核心管理器
ModManager.cpp (110行) - CK2集成
InputHook.cpp (~300行) - 输入管理
RenderHook.cpp (~200行) - 渲染钩子
```

**构建系统**:
```
CMakeLists.txt (根)
src/CMakeLists.txt (主DLL)
src/Core/CMakeLists.txt (Core DLL)
modules/CMakeLists.txt (独立模块)
```

### B. 术语表

| 术语 | 定义 |
|------|------|
| Core微内核 | BML.dll，提供基础API的轻量级核心 |
| Legacy主DLL | BMLPlus.dll，v0.3兼容层 |
| IMC | Inter-Mod Communication，模组间通信总线 |
| ModContext | v0.3核心管理器类 |
| IBML | v0.3接口，提供给模组的API |
| BML_Context | v0.4句柄，Core的上下文 |
| ModManifest | mod.toml解析结果 |
| DependencyResolver | 依赖解析器，拓扑排序 |

### C. 参考资料

- [BML v0.4.0设计文档](BML_v0.4.0_Design.md)
- [Core API文档](docs/api/)
- [模组开发指南](docs/developer-guide/)
- [测试报告](tests/)

---

## 🎯 下一步行动

**立即行动** (今日):
1. 审查本文档
2. 确认技术路线
3. 决策：MVP先行 or 完整计划

**本周行动** (Week 1):
- [ ] 创建`integration-phase5`分支
- [ ] 执行MVP集成（<50行代码）
- [ ] 验证BML_Input接收事件

**本月目标** (4周):
- [ ] 完成完整集成
- [ ] 通过所有测试
- [ ] 发布v0.4.0-beta1

---

**文档版本**: v1.0  
**最后更新**: 2025-11-23  
**作者**: AI系统分析  
**状态**: 待审查
