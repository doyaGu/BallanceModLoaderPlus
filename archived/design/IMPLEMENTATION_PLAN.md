# BML v0.4.0 Core与Legacy系统集成实施计划

> **创建日期**: 2025-11-23  
> **计划版本**: v1.0  
> **预计工期**: 6周  
> **风险等级**: 中等

---

## 📋 文档说明

### 目标受众
- 核心开发团队
- 架构审查委员会
- 项目管理人员

### 文档结构
1. **项目概览** - 背景、目标、范围
2. **技术准备** - 前置条件、风险评估
3. **MVP阶段** - 最小可行产品（1-2天）
4. **Phase 1** - 基础连接（1周）
5. **Phase 2** - 事件桥接（1周）
6. **Phase 3** - 模组加载统一（1周）
7. **Phase 4** - 测试与优化（1周）
8. **Phase 5** - 文档与发布（2周）
9. **附录** - 检查清单、回滚计划

### 前置阅读
- [INTEGRATION_ANALYSIS.md](INTEGRATION_ANALYSIS.md) - 系统集成深度分析
- [BML_v0.4.0_Design.md](BML_v0.4.0_Design.md) - 架构设计文档
- [CLAUDE.md](CLAUDE.md) - 项目开发指南

---

## 🎯 第一部分：项目概览

### 1.1 背景与动机

**当前问题**:
- BML.dll（Core微内核）与BMLPlus.dll（Legacy主DLL）完全孤立
- 8000行Core代码已实现但从未被调用
- 独立模块（BML_Input/Render）等待永远不会到达的事件
- 构建系统未建立链接关系
- v0.4模组系统无法工作

**业务价值**:
1. **统一模组生态** - 支持新旧模组共存
2. **依赖管理自动化** - 拓扑排序、版本约束
3. **热重载支持** - 开发效率提升
4. **性能分析工具** - Profiling、Tracing
5. **模组间通信** - IMC总线、扩展API

**技术债务清理**:
- 消除代码重复（Config/Logging）
- 统一内存管理（MemoryManager）
- 建立完整测试体系

### 1.2 项目目标

#### 1.2.1 功能目标

**P0（必须完成）**:
- ✅ Core微内核正确初始化
- ✅ v0.4模组能够加载并接收事件
- ✅ v0.3模组保持完全兼容
- ✅ IMC事件总线工作正常
- ✅ 构建系统正确配置

**P1（应该完成）**:
- ⭕ 输入/渲染事件正确发布
- ⭕ 生命周期事件完整
- ⭕ 所有现有测试通过
- ⭕ 新增集成测试套件

**P2（最好完成）**:
- ⚪ UI系统注册为Core扩展
- ⚪ 热重载功能验证
- ⚪ 性能基准测试

**P3（可选）**:
- ⚪ 模组开发文档更新
- ⚪ 示例模组迁移
- ⚪ 社区beta测试

#### 1.2.2 非功能目标

**性能**:
- IMC事件开销 < 0.2ms/frame（60fps下）
- 模组加载时间增量 < 20%
- 内存占用增量 < 5MB

**兼容性**:
- v0.3模组零修改运行
- 保持所有现有API导出
- 游戏行为无变化

**稳定性**:
- 回归测试通过率 100%
- 新增单元测试 50+
- 集成测试覆盖核心路径

**可维护性**:
- 代码审查覆盖率 100%
- 文档同步更新
- 清晰的错误诊断

### 1.3 项目范围

#### 1.3.1 包含的工作

**代码修改**:
1. 构建系统链接配置
2. DllMain初始化集成
3. ModContext双路径加载
4. InputHook/RenderHook事件发布
5. ModManager IMC驱动集成
6. CoreBridge抽象层

**测试工作**:
1. MVP验证测试
2. 单元测试补充
3. 集成测试套件
4. 性能基准测试
5. 兼容性回归测试

**文档工作**:
1. API文档更新
2. 模组开发指南修订
3. 迁移指南编写
4. CHANGELOG更新

**基础设施**:
1. CI/CD流程调整
2. 测试环境配置
3. 诊断工具集成

#### 1.3.2 明确排除的工作

**不在本期范围**:
- ❌ 完全移除v0.3支持
- ❌ 重写UI系统
- ❌ 大规模重构ModContext
- ❌ 性能优化（除关键路径）
- ❌ 新功能开发

**后续版本计划**:
- v0.4.1: UI系统Core化
- v0.4.2: 弃用v0.3 API
- v0.5.0: 完全移除v0.3兼容层

### 1.4 成功标准

#### 1.4.1 验收标准

**技术验收**:
```
✓ BML_Input模组加载成功
✓ 按键触发事件，OutputDebugString输出"[BML_Input] Key pressed"
✓ BML_Render接收PreRender/PostRender事件
✓ 所有v0.3模组正常工作（回归测试）
✓ 单元测试通过率 100%
✓ 集成测试通过率 100%
✓ 性能测试满足目标
```

**文档验收**:
```
✓ API文档完整更新
✓ 代码注释覆盖新增代码
✓ 架构图反映实际实现
✓ 迁移指南可执行
```

**流程验收**:
```
✓ 代码审查完成
✓ 测试报告归档
✓ 发布笔记编写
✓ 社区公告准备
```

#### 1.4.2 里程碑定义

| 里程碑 | 完成标准 | 预计日期 |
|--------|----------|----------|
| M0: 项目启动 | 计划审批、分支创建 | Day 0 |
| M1: MVP完成 | BML_Input接收一个事件 | Day 2 |
| M2: 基础连接 | Core初始化集成完成 | Week 1 |
| M3: 事件桥接 | 所有IMC事件发布 | Week 2 |
| M4: 模组统一 | 双路径加载工作 | Week 3 |
| M5: 测试完成 | 所有测试通过 | Week 4 |
| M6: Beta发布 | v0.4.0-beta1打包 | Week 5 |
| M7: 正式发布 | v0.4.0 Release | Week 6 |

---

## 🔧 第二部分：技术准备

### 2.1 前置条件检查

#### 2.1.1 环境要求

**开发环境**:
```
✓ Windows 10/11 x64
✓ Visual Studio 2019/2022
✓ CMake >= 3.20
✓ Git >= 2.30
✓ Virtools SDK (VIRTOOLS_SDK_PATH已设置)
```

**构建要求**:
```
✓ 所有依赖库可用（deps/）
✓ 当前代码可成功编译
✓ 所有测试通过（main分支）
```

**测试环境**:
```
✓ Ballance 1.0 已安装
✓ 至少3个v0.3模组可用于测试
✓ 测试数据集准备
```

#### 2.1.2 技能要求

**团队技能矩阵**:

| 技能领域 | 必需等级 | 当前状态 |
|----------|----------|----------|
| C++ 现代特性 | 高级 | ✅ |
| CMake构建系统 | 中级 | ✅ |
| Windows API | 中级 | ✅ |
| 游戏引擎架构 | 中级 | ✅ |
| 单元测试（GoogleTest）| 中级 | ✅ |
| 调试技巧 | 高级 | ✅ |

**知识要求**:
- 理解CK2引擎生命周期
- 熟悉MinHook使用
- 了解ImGui渲染流程
- 掌握依赖解析算法

### 2.2 风险评估与应对

#### 2.2.1 技术风险

**风险1: v0.3兼容性破坏**
```yaml
概率: 中等（40%）
影响: 高（现有模组无法工作）
应对措施:
  - 创建完整回归测试套件
  - 每次修改后立即测试
  - 保持v0.3 API完全不变
  - 准备回滚方案
缓解后风险: 低（10%）
```

**风险2: IMC性能问题**
```yaml
概率: 低（20%）
影响: 中（帧率下降）
应对措施:
  - MVP阶段提前性能测试
  - 准备性能分析工具
  - 设计可降级方案（禁用IMC）
  - 优化热路径（Pump/Publish）
缓解后风险: 极低（5%）
```

**风险3: 模组加载冲突**
```yaml
概率: 中等（30%）
影响: 中（部分模组失败）
应对措施:
  - 隔离v0.3/v0.4加载路径
  - 独立错误处理
  - 详细日志记录
  - 失败不影响另一路径
缓解后风险: 低（10%）
```

**风险4: 构建系统问题**
```yaml
概率: 低（15%）
影响: 低（编译失败）
应对措施:
  - CMake专家审查
  - CI早期验证
  - 保留构建脚本备份
缓解后风险: 极低（5%）
```

#### 2.2.2 项目风险

**风险5: 工期延误**
```yaml
概率: 中等（35%）
影响: 低（延迟发布）
应对措施:
  - 保守估算（+20% buffer）
  - 每日站会跟踪进度
  - P0/P1/P2优先级明确
  - 准备削减P2/P3范围
缓解后风险: 低（15%）
```

**风险6: 资源不足**
```yaml
概率: 低（20%）
影响: 中（质量下降）
应对措施:
  - 提前识别关键路径
  - 社区贡献者支持
  - 自动化测试减轻负担
缓解后风险: 低（10%）
```

#### 2.2.3 风险应对总结

| 风险等级 | 数量 | 应对策略 |
|----------|------|----------|
| 高风险 | 0 | N/A |
| 中风险 | 2 | 提前测试、降级方案 |
| 低风险 | 4 | 常规监控、快速响应 |

**整体风险等级**: 🟡 中等（可控）

### 2.3 资源规划

#### 2.3.1 人力资源

**团队结构**:
```
核心开发: 1-2人
  - 主要编码工作
  - 代码审查
  - 技术决策

测试工程师: 1人
  - 测试设计
  - 自动化测试
  - 回归测试执行

文档工程师: 0.5人（兼职）
  - API文档更新
  - 用户指南修订

项目协调: 0.5人（兼职）
  - 进度跟踪
  - 风险管理
  - 沟通协调
```

**工作量估算**:
```
MVP阶段:     2人日
Phase 1:     5人日（基础连接）
Phase 2:     5人日（事件桥接）
Phase 3:     7人日（模组统一）
Phase 4:     5人日（测试优化）
Phase 5:     6人日（文档发布）
-------
总计:       30人日 ≈ 6周（1人全职）
```

#### 2.3.2 时间资源

**工作日历**:
```
Week 0 (Day 0-2):   MVP阶段
Week 1 (Day 3-7):   Phase 1 基础连接
Week 2 (Day 8-12):  Phase 2 事件桥接
Week 3 (Day 13-17): Phase 3 模组统一
Week 4 (Day 18-22): Phase 4 测试优化
Week 5 (Day 23-27): Phase 5 文档准备
Week 6 (Day 28-30): Beta测试 + 正式发布
```

**关键路径**:
```
MVP验证 → 基础连接 → 事件桥接 → 模组统一 → 测试 → 发布
```

**并行任务**:
```
Week 2: 事件桥接 || 文档更新（初稿）
Week 3: 模组统一 || 性能测试准备
Week 4: 测试执行 || 文档完善
```

#### 2.3.3 工具与基础设施

**开发工具**:
- Visual Studio Code / Visual Studio
- CMake Tools扩展
- Git版本控制
- WinDbg / x64dbg（调试）

**测试工具**:
- GoogleTest（单元测试）
- Python脚本（集成测试）
- Windows Performance Recorder（性能分析）
- DebugView++（日志监控）

**CI/CD**:
- GitHub Actions（自动构建）
- 自动化测试执行
- 发布包生成

---

## 🚀 第三部分：MVP阶段（Day 0-2）

### 3.1 MVP目标

**定义**: 最小可行产品 - 证明技术路线可行

**成功标准**:
```
✓ BML_Input模组加载成功
✓ 按下任意键，OutputDebugString显示 "[BML_Input] Key pressed"
✓ 编译无错误，链接成功
✓ 游戏启动正常
```

**工作量**: 1-2天，<50行代码

### 3.2 实施步骤

#### 步骤1: 构建系统链接（15分钟）

**文件**: `src/CMakeLists.txt`

**修改前**:
```cmake
target_link_libraries(BMLPlus
    PUBLIC CK2 VxMath
    PRIVATE BMLUtils minhook onig
)
```

**修改后**:
```cmake
target_link_libraries(BMLPlus
    PUBLIC CK2 VxMath
    PRIVATE BMLUtils minhook onig BML  # ← 添加Core链接
)
```

**验证**:
```powershell
cmake --build build --config Release
# 检查无链接错误
```

#### 步骤2: Core初始化（30分钟）

**文件**: `src/BML.cpp`

**当前DllMain** (line 74):
```cpp
BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        if (MH_Initialize() != MH_OK) {
            utils::OutputDebugA("Fatal: Unable to initialize MinHook.\n");
            return FALSE;
        }
        if (!RenderHook::HookRenderEngine()) {
            utils::OutputDebugA("Fatal: Unable to hook Render Engine.\n");
            return FALSE;
        }
        // ... 其他钩子
        break;
    case DLL_PROCESS_DETACH:
        // ... 清理
        break;
    }
    return TRUE;
}
```

**修改方案**:
```cpp
// 文件顶部添加
#include "BML/bml_loader.h"  // Core API

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        // ===== 新增：Core初始化 =====
        if (bmlAttach() != BML_RESULT_OK) {
            utils::OutputDebugA("Fatal: Unable to initialize BML Core.\n");
            return FALSE;
        }
        // ===== End 新增 =====
        
        if (MH_Initialize() != MH_OK) {
            utils::OutputDebugA("Fatal: Unable to initialize MinHook.\n");
            bmlDetach();  // ← 清理Core
            return FALSE;
        }
        // ... 其他钩子（保持不变）
        break;
        
    case DLL_PROCESS_DETACH:
        // ... 现有清理
        
        // ===== 新增：Core清理 =====
        bmlDetach();
        // ===== End 新增 =====
        break;
    }
    return TRUE;
}
```

**关键点**:
- `bmlAttach()` 只做Phase 1（模组发现），不加载DLL
- DllMain安全性保证
- 失败时正确清理

#### 步骤3: 模组加载集成（30分钟）

**文件**: `src/ModManager.cpp`

**当前OnCKPlay** (约line 40):
```cpp
CKERROR ModManager::OnCKPlay() {
    m_RenderContext = m_Context->GetPlayerRenderContext();
    Overlay::ImGuiInitRenderer(m_Context);
    
    m_ModContext->LoadMods();   // v0.3模组加载
    m_ModContext->InitMods();
    
    return CK_OK;
}
```

**修改方案**:
```cpp
CKERROR ModManager::OnCKPlay() {
    m_RenderContext = m_Context->GetPlayerRenderContext();
    Overlay::ImGuiInitRenderer(m_Context);
    
    // ===== 新增：加载v0.4模组 =====
    if (bmlLoadModules() != BML_RESULT_OK) {
        m_ModContext->GetLogger()->Warn("BML Core", "Failed to load v0.4 modules");
        // 不阻断游戏，继续加载v0.3模组
    }
    // ===== End 新增 =====
    
    m_ModContext->LoadMods();   // v0.3模组加载（保持不变）
    m_ModContext->InitMods();
    
    return CK_OK;
}
```

**关键点**:
- v0.4加载失败不影响v0.3
- 此时CKContext已可用，Phase 2安全

#### 步骤4: 输入事件发布（1小时）

**文件**: `src/InputHook.cpp`

**需要添加的头文件**:
```cpp
#include "BML/bml_loader.h"  // Core API
```

**定义事件结构**（文件顶部）:
```cpp
// IMC事件结构定义
struct BML_KeyEvent {
    uint32_t key;           // 虚拟键码
    uint32_t timestamp;     // 时间戳（毫秒）
    uint8_t  is_repeat;     // 是否重复按键
    uint8_t  _padding[3];
};
```

**修改Process方法**:

找到按键处理代码（假设在`Process()`方法中）:
```cpp
void InputHook::Process() {
    // 现有代码：遍历回调
    for (auto& callback : m_KeyCallbacks) {
        if (IsKeyPressed(callback.key)) {
            callback.fn(this);
        }
    }
    
    // ===== 新增：发布IMC事件 =====
    // KeyDown事件
    if (m_NewKeyDowns.size() > 0) {
        for (uint32_t key : m_NewKeyDowns) {
            BML_KeyEvent event;
            event.key = key;
            event.timestamp = timeGetTime();  // 或使用std::chrono
            event.is_repeat = 0;
            
            bmlImcPublish(
                "BML/Input/KeyDown",
                &event,
                sizeof(event),
                BML_IMC_RELIABLE
            );
        }
        m_NewKeyDowns.clear();
    }
    
    // KeyUp事件（类似）
    // MouseMove事件（类似）
    // MouseButton事件（类似）
    // ===== End 新增 =====
}
```

**注意**:
- 实际代码结构可能不同，需根据`InputHook.cpp`实际实现调整
- 确保事件结构体对齐（避免跨模组ABI问题）

#### 步骤5: IMC驱动集成（15分钟）

**文件**: `src/ModManager.cpp`

**当前PostProcess** (约line 60):
```cpp
CKERROR ModManager::PostProcess() {
    Overlay::ImGuiNewFrame();
    PhysicsPostProcess();
    
    m_ModContext->OnProcess();  // v0.3模组回调
    
    // 处理ImGui
    Overlay::ImGuiRender();
    inputHook->Process();
    
    return CK_OK;
}
```

**修改方案**:
```cpp
CKERROR ModManager::PostProcess() {
    Overlay::ImGuiNewFrame();
    PhysicsPostProcess();
    
    // ===== 新增：驱动IMC总线 =====
    bmlImcPump(0);  // 处理所有排队消息，0=不限制数量
    // ===== End 新增 =====
    
    m_ModContext->OnProcess();  // v0.3模组回调
    
    // 处理ImGui
    Overlay::ImGuiRender();
    inputHook->Process();
    
    return CK_OK;
}
```

**关键点**:
- 在v0.3回调之前调用，确保v0.4事件优先处理
- `bmlImcPump(0)` 表示无限制，处理所有消息

### 3.3 MVP测试

#### 测试1: 编译测试

```powershell
# 清理构建
Remove-Item build -Recurse -Force
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build build --config Release

# 验证产物
Test-Path build/bin/Release/BML.dll        # 应为 True
Test-Path build/bin/Release/BMLPlus.dll    # 应为 True
Test-Path build/bin/Release/BML_Input.dll  # 应为 True
```

#### 测试2: 加载测试

**步骤**:
1. 复制DLL到游戏目录:
```powershell
Copy-Item build/bin/Release/*.dll "C:\Program Files\Ballance\"
```

2. 启动DebugView++

3. 运行游戏（Player.exe）

4. 观察日志输出:
```
[BML] Core attached successfully
[BML] Discovered 1 module(s)
[BML] Loading module: BML_Input
[BML_Input] Module initialized
```

#### 测试3: 事件测试

**步骤**:
1. 游戏加载完成后
2. 按任意键（如空格键）
3. 观察DebugView++输出:
```
[BML_Input] Key pressed: 0x20 (SPACE)
```

**成功标准**: 看到上述输出

#### 测试4: 兼容性测试

**验证v0.3模组仍工作**:
1. 确保有至少1个v0.3模组在Mods/目录
2. 游戏启动
3. v0.3模组功能正常（如HUD显示、命令可用）

### 3.4 MVP里程碑检查

**完成标准**:
```
✅ 代码修改 <50行
✅ 编译链接成功
✅ BML_Input加载成功
✅ 按键事件接收成功
✅ v0.3兼容性无破坏
✅ 无崩溃，无明显性能问题
```

**交付物**:
- [ ] 修改后的源代码（5个文件）
- [ ] 编译后的DLL文件
- [ ] MVP测试报告（简短）
- [ ] 技术债务记录（如有）

**决策点**:
- ✅ **Go**: 技术路线验证成功，继续Phase 1
- ❌ **No-Go**: 发现重大问题，回滚并重新评估

---

## 📦 第四部分：Phase 1 - 基础连接（Week 1）

### 4.1 Phase目标

**总体目标**: 建立Core与Legacy的稳定连接层

**具体目标**:
1. 创建CoreBridge抽象层
2. 完善错误处理与日志
3. 添加诊断API
4. 建立测试框架
5. 文档初稿

**交付标准**:
```
✓ CoreBridge类完整实现
✓ 所有Core API调用通过Bridge
✓ 错误路径完整测试
✓ 单元测试覆盖新代码
✓ 集成测试框架就绪
```

### 4.2 任务分解

#### Task 1.1: 创建CoreBridge抽象层（2天）

**目标**: 封装Core API调用，便于测试和维护

**新文件**: `src/CoreBridge.h` + `src/CoreBridge.cpp`

**设计理念**:
- 单例模式
- RAII管理生命周期
- 统一错误处理
- 日志集成

**接口设计** (`src/CoreBridge.h`):
```cpp
#ifndef BML_CORE_BRIDGE_H
#define BML_CORE_BRIDGE_H

#include "BML/bml_loader.h"
#include "Logger.h"
#include <string>
#include <vector>

namespace BML {

// Core集成状态
enum class CoreStatus {
    Uninitialized,  // 未初始化
    Attached,       // Phase 1完成（已发现模组）
    Loaded,         // Phase 2完成（已加载模组）
    Error           // 错误状态
};

// v0.4模组信息
struct CoreModuleInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string path;
    bool loaded;
};

/**
 * @brief Core微内核集成桥接层
 * 
 * 封装BML Core API调用，提供：
 * - 生命周期管理
 * - 错误处理
 * - 日志集成
 * - 状态查询
 */
class CoreBridge {
public:
    static CoreBridge& Instance();
    
    // 禁止拷贝
    CoreBridge(const CoreBridge&) = delete;
    CoreBridge& operator=(const CoreBridge&) = delete;
    
    // === 生命周期管理 ===
    
    /**
     * @brief Phase 1: 发现模组（DllMain调用）
     * @return 成功返回true
     */
    bool Attach();
    
    /**
     * @brief Phase 2: 加载模组（OnCKPlay调用）
     * @return 成功返回true
     */
    bool LoadModules();
    
    /**
     * @brief 清理Core（DllMain DETACH）
     */
    void Detach();
    
    // === 状态查询 ===
    
    CoreStatus GetStatus() const { return m_Status; }
    bool IsAttached() const { return m_Status >= CoreStatus::Attached; }
    bool IsLoaded() const { return m_Status == CoreStatus::Loaded; }
    
    std::string GetLastError() const { return m_LastError; }
    
    // === 模组信息 ===
    
    /**
     * @brief 获取所有v0.4模组信息
     */
    std::vector<CoreModuleInfo> GetModules() const;
    
    /**
     * @brief 获取已加载的模组数量
     */
    size_t GetLoadedModuleCount() const;
    
    // === IMC事件发布辅助 ===
    
    /**
     * @brief 发布输入事件
     */
    bool PublishKeyEvent(const char* topic, uint32_t key, bool is_repeat);
    bool PublishMouseMoveEvent(int32_t x, int32_t y);
    bool PublishMouseButtonEvent(uint32_t button, bool down);
    
    /**
     * @brief 发布生命周期事件
     */
    bool PublishLifecycleEvent(const char* topic);
    
    /**
     * @brief 驱动IMC总线（每帧调用）
     */
    void PumpEvents(size_t max_messages = 0);
    
    // === 诊断功能 ===
    
    /**
     * @brief 输出Core状态到日志
     */
    void DumpState() const;
    
private:
    CoreBridge();
    ~CoreBridge();
    
    void SetError(const std::string& error);
    void LogCoreError();
    
    CoreStatus m_Status;
    std::string m_LastError;
    Logger* m_Logger;  // 指向ModContext的Logger
};

} // namespace BML

#endif // BML_CORE_BRIDGE_H
```

**实现** (`src/CoreBridge.cpp`):
```cpp
#include "CoreBridge.h"
#include "ModContext.h"  // 获取Logger
#include <chrono>

namespace BML {

CoreBridge& CoreBridge::Instance() {
    static CoreBridge instance;
    return instance;
}

CoreBridge::CoreBridge()
    : m_Status(CoreStatus::Uninitialized)
    , m_Logger(nullptr)
{
}

CoreBridge::~CoreBridge() {
    if (m_Status != CoreStatus::Uninitialized) {
        Detach();
    }
}

bool CoreBridge::Attach() {
    if (m_Status != CoreStatus::Uninitialized) {
        SetError("Core already attached");
        return false;
    }
    
    BML_Result result = bmlAttach();
    if (result != BML_RESULT_OK) {
        SetError("bmlAttach() failed");
        LogCoreError();
        m_Status = CoreStatus::Error;
        return false;
    }
    
    m_Status = CoreStatus::Attached;
    
    // 获取Logger（延迟到ModContext创建后）
    if (!m_Logger) {
        m_Logger = BML::GetLogger();  // 假设有全局访问函数
    }
    
    if (m_Logger) {
        auto modules = GetModules();
        m_Logger->Info("CoreBridge", "Attached successfully, discovered %zu module(s)", modules.size());
        for (const auto& mod : modules) {
            m_Logger->Info("CoreBridge", "  - %s (%s) at %s", 
                mod.name.c_str(), mod.version.c_str(), mod.path.c_str());
        }
    }
    
    return true;
}

bool CoreBridge::LoadModules() {
    if (m_Status != CoreStatus::Attached) {
        SetError("Core not attached or already loaded");
        return false;
    }
    
    BML_Result result = bmlLoadModules();
    if (result != BML_RESULT_OK) {
        SetError("bmlLoadModules() failed");
        LogCoreError();
        m_Status = CoreStatus::Error;
        return false;
    }
    
    m_Status = CoreStatus::Loaded;
    
    if (m_Logger) {
        size_t count = GetLoadedModuleCount();
        m_Logger->Info("CoreBridge", "Loaded %zu v0.4 module(s)", count);
    }
    
    return true;
}

void CoreBridge::Detach() {
    if (m_Status == CoreStatus::Uninitialized) {
        return;
    }
    
    if (m_Logger) {
        m_Logger->Info("CoreBridge", "Detaching Core");
    }
    
    bmlDetach();
    m_Status = CoreStatus::Uninitialized;
}

std::vector<CoreModuleInfo> CoreBridge::GetModules() const {
    std::vector<CoreModuleInfo> result;
    
    // 查询Core API获取模组列表
    size_t count = 0;
    bmlGetModuleCount(&count);
    
    for (size_t i = 0; i < count; ++i) {
        BML_ModuleHandle handle = nullptr;
        if (bmlGetModuleByIndex(i, &handle) != BML_RESULT_OK)
            continue;
        
        CoreModuleInfo info;
        
        // 获取ID
        const char* id_str = nullptr;
        bmlModGetId(handle, &id_str);
        info.id = id_str ? id_str : "";
        
        // 获取名称（从清单）
        // TODO: 需要Core API支持，暂用ID代替
        info.name = info.id;
        
        // 获取版本
        BML_SemVer version;
        if (bmlModGetVersion(handle, &version) == BML_RESULT_OK) {
            char ver_buf[32];
            snprintf(ver_buf, sizeof(ver_buf), "%u.%u.%u", 
                version.major, version.minor, version.patch);
            info.version = ver_buf;
        }
        
        // 获取路径
        const char* path_str = nullptr;
        bmlModGetPath(handle, &path_str);
        info.path = path_str ? path_str : "";
        
        // 加载状态
        BML_ModuleState state;
        bmlModGetState(handle, &state);
        info.loaded = (state == BML_MODULE_STATE_LOADED);
        
        result.push_back(info);
    }
    
    return result;
}

size_t CoreBridge::GetLoadedModuleCount() const {
    auto modules = GetModules();
    return std::count_if(modules.begin(), modules.end(),
        [](const CoreModuleInfo& m) { return m.loaded; });
}

bool CoreBridge::PublishKeyEvent(const char* topic, uint32_t key, bool is_repeat) {
    struct KeyEvent {
        uint32_t key;
        uint32_t timestamp;
        uint8_t is_repeat;
        uint8_t _padding[3];
    };
    
    KeyEvent event;
    event.key = key;
    event.timestamp = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
    event.is_repeat = is_repeat ? 1 : 0;
    
    BML_Result result = bmlImcPublish(topic, &event, sizeof(event), BML_IMC_RELIABLE);
    return (result == BML_RESULT_OK);
}

bool CoreBridge::PublishMouseMoveEvent(int32_t x, int32_t y) {
    struct MouseMoveEvent {
        int32_t x;
        int32_t y;
        uint32_t timestamp;
    };
    
    MouseMoveEvent event;
    event.x = x;
    event.y = y;
    event.timestamp = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
    
    BML_Result result = bmlImcPublish("BML/Input/MouseMove", &event, sizeof(event), BML_IMC_RELIABLE);
    return (result == BML_RESULT_OK);
}

bool CoreBridge::PublishMouseButtonEvent(uint32_t button, bool down) {
    struct MouseButtonEvent {
        uint32_t button;
        uint8_t down;
        uint8_t _padding[3];
        uint32_t timestamp;
    };
    
    MouseButtonEvent event;
    event.button = button;
    event.down = down ? 1 : 0;
    event.timestamp = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
    
    BML_Result result = bmlImcPublish("BML/Input/MouseButton", &event, sizeof(event), BML_IMC_RELIABLE);
    return (result == BML_RESULT_OK);
}

bool CoreBridge::PublishLifecycleEvent(const char* topic) {
    // 空payload，仅通知
    BML_Result result = bmlImcPublish(topic, nullptr, 0, BML_IMC_RELIABLE);
    return (result == BML_RESULT_OK);
}

void CoreBridge::PumpEvents(size_t max_messages) {
    bmlImcPump(max_messages);
}

void CoreBridge::DumpState() const {
    if (!m_Logger) return;
    
    m_Logger->Info("CoreBridge", "=== Core State Dump ===");
    m_Logger->Info("CoreBridge", "Status: %d", static_cast<int>(m_Status));
    m_Logger->Info("CoreBridge", "Last Error: %s", m_LastError.c_str());
    
    auto modules = GetModules();
    m_Logger->Info("CoreBridge", "Modules: %zu", modules.size());
    for (const auto& mod : modules) {
        m_Logger->Info("CoreBridge", "  [%s] %s v%s - %s",
            mod.loaded ? "✓" : "✗",
            mod.name.c_str(),
            mod.version.c_str(),
            mod.path.c_str());
    }
}

void CoreBridge::SetError(const std::string& error) {
    m_LastError = error;
    if (m_Logger) {
        m_Logger->Error("CoreBridge", "%s", error.c_str());
    }
}

void CoreBridge::LogCoreError() {
    // 从Core获取详细错误
    const char* error_msg = nullptr;
    bmlGetLastError(&error_msg);
    
    if (error_msg && m_Logger) {
        m_Logger->Error("CoreBridge", "Core error: %s", error_msg);
    }
}

} // namespace BML
```

**集成到BML.cpp**:
```cpp
// 修改 src/BML.cpp
#include "CoreBridge.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        // 使用Bridge替代直接调用
        if (!BML::CoreBridge::Instance().Attach()) {
            utils::OutputDebugA("Fatal: CoreBridge::Attach() failed\n");
            return FALSE;
        }
        // ... 其他初始化
        break;
        
    case DLL_PROCESS_DETACH:
        // ... 其他清理
        BML::CoreBridge::Instance().Detach();
        break;
    }
    return TRUE;
}
```

**集成到ModManager.cpp**:
```cpp
// 修改 src/ModManager.cpp
#include "CoreBridge.h"

CKERROR ModManager::OnCKPlay() {
    // ...
    
    // 使用Bridge加载模组
    if (!BML::CoreBridge::Instance().LoadModules()) {
        m_ModContext->GetLogger()->Warn("ModManager", "Failed to load v0.4 modules");
    }
    
    // ...
}

CKERROR ModManager::PostProcess() {
    // ...
    
    // 使用Bridge驱动事件
    BML::CoreBridge::Instance().PumpEvents(0);
    
    // ...
}
```

**集成到InputHook.cpp**:
```cpp
// 修改 src/InputHook.cpp
#include "CoreBridge.h"

void InputHook::Process() {
    // ... 现有v0.3回调
    
    // 使用Bridge发布事件
    auto& bridge = BML::CoreBridge::Instance();
    for (uint32_t key : m_NewKeyDowns) {
        bridge.PublishKeyEvent("BML/Input/KeyDown", key, false);
    }
    // ...
}
```

**测试**:
```cpp
// tests/CoreBridgeTest.cpp
#include <gtest/gtest.h>
#include "CoreBridge.h"

TEST(CoreBridgeTest, SingletonAccess) {
    auto& bridge1 = BML::CoreBridge::Instance();
    auto& bridge2 = BML::CoreBridge::Instance();
    EXPECT_EQ(&bridge1, &bridge2);
}

TEST(CoreBridgeTest, InitialState) {
    auto& bridge = BML::CoreBridge::Instance();
    EXPECT_EQ(bridge.GetStatus(), BML::CoreStatus::Uninitialized);
    EXPECT_FALSE(bridge.IsAttached());
    EXPECT_FALSE(bridge.IsLoaded());
}

// ... 更多测试
```

**工作量**: 2天
- 设计接口: 2小时
- 实现CoreBridge: 8小时
- 集成到现有代码: 4小时
- 单元测试: 2小时

---

#### Task 1.2: 错误处理完善（1天）

**目标**: 建立统一的错误处理机制

**错误分类**:
1. **致命错误** - 阻止启动（DllMain返回FALSE）
2. **严重错误** - 功能不可用（记录日志，继续）
3. **警告** - 部分功能降级
4. **信息** - 正常状态通知

**错误码定义** (`src/CoreBridge.h`新增):
```cpp
enum class CoreError {
    None = 0,
    
    // 初始化错误 (100-199)
    AttachFailed = 100,
    AlreadyAttached = 101,
    DetachFailed = 102,
    
    // 加载错误 (200-299)
    LoadModulesFailed = 200,
    NotAttached = 201,
    AlreadyLoaded = 202,
    ModuleNotFound = 203,
    
    // IMC错误 (300-399)
    PublishFailed = 300,
    SubscribeFailed = 301,
    PumpFailed = 302,
    
    // 未知错误
    Unknown = 999
};

struct CoreErrorInfo {
    CoreError code;
    std::string message;
    std::string details;  // Core提供的详细信息
};
```

**错误处理策略**:
```cpp
class CoreBridge {
    // ...
    
    CoreErrorInfo GetLastErrorInfo() const;
    void ClearError();
    
private:
    CoreErrorInfo m_LastErrorInfo;
    
    void SetError(CoreError code, const std::string& message);
    bool HandleCoreResult(BML_Result result, CoreError error_code);
};
```

**实现示例**:
```cpp
bool CoreBridge::Attach() {
    if (m_Status != CoreStatus::Uninitialized) {
        SetError(CoreError::AlreadyAttached, "Core already attached");
        return false;
    }
    
    BML_Result result = bmlAttach();
    if (!HandleCoreResult(result, CoreError::AttachFailed)) {
        m_Status = CoreStatus::Error;
        return false;
    }
    
    m_Status = CoreStatus::Attached;
    ClearError();
    return true;
}

bool CoreBridge::HandleCoreResult(BML_Result result, CoreError error_code) {
    if (result == BML_RESULT_OK) {
        return true;
    }
    
    // 获取Core错误详情
    const char* core_error = nullptr;
    bmlGetLastError(&core_error);
    
    std::string details = core_error ? core_error : "No details available";
    SetError(error_code, BMLResultToString(result) + ": " + details);
    
    return false;
}

void CoreBridge::SetError(CoreError code, const std::string& message) {
    m_LastErrorInfo.code = code;
    m_LastErrorInfo.message = message;
    
    // 同时记录到日志
    if (m_Logger) {
        m_Logger->Error("CoreBridge", "[E%d] %s", 
            static_cast<int>(code), message.c_str());
    }
}
```

**集成到现有代码**:
```cpp
// src/BML.cpp
BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        if (!BML::CoreBridge::Instance().Attach()) {
            auto error = BML::CoreBridge::Instance().GetLastErrorInfo();
            char buf[512];
            snprintf(buf, sizeof(buf), 
                "Fatal: CoreBridge::Attach() failed\nError %d: %s\n",
                static_cast<int>(error.code),
                error.message.c_str());
            utils::OutputDebugA(buf);
            
            // 显示对话框（可选）
            MessageBoxA(nullptr, buf, "BML Initialization Failed", MB_ICONERROR);
            
            return FALSE;
        }
        break;
    }
    return TRUE;
}
```

**测试**:
```cpp
TEST(CoreBridgeTest, ErrorHandling) {
    auto& bridge = BML::CoreBridge::Instance();
    
    // 尝试重复Attach
    bridge.Attach();
    EXPECT_FALSE(bridge.Attach());  // 应失败
    
    auto error = bridge.GetLastErrorInfo();
    EXPECT_EQ(error.code, BML::CoreError::AlreadyAttached);
    EXPECT_FALSE(error.message.empty());
}
```

**工作量**: 1天
- 错误码设计: 1小时
- 错误处理实现: 4小时
- 集成与测试: 3小时

---

### 4.3 Phase 1检查点

**完成标准**:
```
✅ CoreBridge类完整实现并测试
✅ 所有Core API调用通过Bridge
✅ 错误处理机制完善
✅ 单元测试覆盖率 >80%
✅ 代码审查通过
✅ 文档初稿完成
```

**交付物**:
- [ ] `src/CoreBridge.h` + `.cpp`
- [ ] `tests/CoreBridgeTest.cpp`
- [ ] 修改后的`BML.cpp`, `ModManager.cpp`, `InputHook.cpp`
- [ ] Phase 1测试报告
- [ ] CoreBridge设计文档

**决策点**:
- ✅ **Go**: 基础连接稳定，继续Phase 2
- 🔄 **Review**: 发现问题，修复后继续
- ❌ **No-Go**: 重大设计缺陷，重新规划

---

## 🔄 第五部分：Phase 2 - 事件桥接（Week 2）

### 5.1 Phase目标

**总体目标**: 建立完整的IMC事件发布机制

**具体目标**:
1. 完善InputHook事件发布（4种输入事件）
2. 添加RenderHook事件发布（2种渲染事件）
3. 实现生命周期事件（6个关键点）
4. 创建事件协议文档
5. 验证BML_Input/Render模组功能

**交付标准**:
```
✓ 所有输入事件正确发布
✓ 所有渲染事件正确发布
✓ 生命周期事件完整
✓ BML_Input接收所有输入事件
✓ BML_Render接收渲染事件
✓ 事件协议文档完成
```

### 5.2 任务分解

#### Task 2.1: InputHook事件发布完善（1.5天）

**目标**: 发布完整的输入事件集

**事件协议设计**:

**文件**: `src/Events/InputEvents.h` (新建)
```cpp
#ifndef BML_INPUT_EVENTS_H
#define BML_INPUT_EVENTS_H

#include <cstdint>

namespace BML {
namespace Events {

// IMC Topic常量
constexpr const char* TOPIC_INPUT_KEY_DOWN = "BML/Input/KeyDown";
constexpr const char* TOPIC_INPUT_KEY_UP = "BML/Input/KeyUp";
constexpr const char* TOPIC_INPUT_MOUSE_MOVE = "BML/Input/MouseMove";
constexpr const char* TOPIC_INPUT_MOUSE_BUTTON = "BML/Input/MouseButton";
constexpr const char* TOPIC_INPUT_MOUSE_WHEEL = "BML/Input/MouseWheel";

/**
 * @brief 键盘事件
 * 
 * Topic: BML/Input/KeyDown, BML/Input/KeyUp
 */
struct KeyEvent {
    uint32_t key;           // 虚拟键码 (VK_*)
    uint32_t scan_code;     // 扫描码
    uint32_t timestamp;     // 时间戳（毫秒）
    uint8_t  is_repeat;     // 是否重复按键
    uint8_t  is_extended;   // 是否扩展键
    uint8_t  _padding[2];
    
    // 修饰键状态
    uint8_t  ctrl_pressed;
    uint8_t  shift_pressed;
    uint8_t  alt_pressed;
    uint8_t  _padding2;
};
static_assert(sizeof(KeyEvent) == 24, "KeyEvent size must be 24 bytes");

/**
 * @brief 鼠标移动事件
 * 
 * Topic: BML/Input/MouseMove
 */
struct MouseMoveEvent {
    int32_t  x;             // 屏幕坐标X
    int32_t  y;             // 屏幕坐标Y
    int32_t  dx;            // 相对移动X
    int32_t  dy;            // 相对移动Y
    uint32_t timestamp;     // 时间戳
    uint32_t _padding;
};
static_assert(sizeof(MouseMoveEvent) == 24, "MouseMoveEvent size must be 24 bytes");

/**
 * @brief 鼠标按键事件
 * 
 * Topic: BML/Input/MouseButton
 */
struct MouseButtonEvent {
    uint32_t button;        // 0=Left, 1=Right, 2=Middle, 3+=X1/X2
    uint8_t  down;          // 1=pressed, 0=released
    uint8_t  double_click;  // 是否双击
    uint8_t  _padding[2];
    int32_t  x;             // 点击位置X
    int32_t  y;             // 点击位置Y
    uint32_t timestamp;
};
static_assert(sizeof(MouseButtonEvent) == 20, "MouseButtonEvent size must be 20 bytes");

/**
 * @brief 鼠标滚轮事件
 * 
 * Topic: BML/Input/MouseWheel
 */
struct MouseWheelEvent {
    int32_t  delta;         // 滚动量（正=向上，负=向下）
    int32_t  x;             // 光标位置X
    int32_t  y;             // 光标位置Y
    uint32_t timestamp;
};
static_assert(sizeof(MouseWheelEvent) == 16, "MouseWheelEvent size must be 16 bytes");

} // namespace Events
} // namespace BML

#endif // BML_INPUT_EVENTS_H
```

**修改InputHook实现**:

**分析现有代码**:
```powershell
# 先查看InputHook的实际结构
grep -n "class InputHook" src/InputHook.h
```

假设InputHook有以下结构：
```cpp
class InputHook {
public:
    void Process();  // 每帧调用
    
    // 注册回调
    void RegisterKeyCallback(int key, std::function<void(InputHook*)> callback);
    // ...
    
private:
    std::vector<KeyCallbackInfo> m_KeyCallbacks;
    std::vector<int> m_PressedKeys;  // 当前帧按下的键
    std::vector<int> m_ReleasedKeys; // 当前帧释放的键
    
    POINT m_LastMousePos;
    POINT m_CurrentMousePos;
    // ...
};
```

**修改方案** (`src/InputHook.cpp`):
```cpp
#include "Events/InputEvents.h"
#include "CoreBridge.h"
#include <chrono>

// 辅助函数：获取修饰键状态
static void GetModifierState(uint8_t* ctrl, uint8_t* shift, uint8_t* alt) {
    *ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? 1 : 0;
    *shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 1 : 0;
    *alt = (GetAsyncKeyState(VK_MENU) & 0x8000) ? 1 : 0;
}

// 辅助函数：获取时间戳
static uint32_t GetTimestamp() {
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

void InputHook::Process() {
    using namespace BML::Events;
    
    // ===== 处理v0.3回调（保持不变）=====
    for (auto& callback : m_KeyCallbacks) {
        if (std::find(m_PressedKeys.begin(), m_PressedKeys.end(), callback.key) != m_PressedKeys.end()) {
            callback.fn(this);
        }
    }
    // ... 其他v0.3逻辑
    
    // ===== 发布v0.4 IMC事件 =====
    auto& bridge = BML::CoreBridge::Instance();
    
    // 1. KeyDown事件
    for (int key : m_PressedKeys) {
        KeyEvent event = {};
        event.key = static_cast<uint32_t>(key);
        event.scan_code = MapVirtualKeyW(key, MAPVK_VK_TO_VSC);
        event.timestamp = GetTimestamp();
        event.is_repeat = 0;  // TODO: 检测重复按键
        event.is_extended = (event.scan_code & 0x100) ? 1 : 0;
        GetModifierState(&event.ctrl_pressed, &event.shift_pressed, &event.alt_pressed);
        
        bmlImcPublish(TOPIC_INPUT_KEY_DOWN, &event, sizeof(event), BML_IMC_RELIABLE);
    }
    
    // 2. KeyUp事件
    for (int key : m_ReleasedKeys) {
        KeyEvent event = {};
        event.key = static_cast<uint32_t>(key);
        event.scan_code = MapVirtualKeyW(key, MAPVK_VK_TO_VSC);
        event.timestamp = GetTimestamp();
        GetModifierState(&event.ctrl_pressed, &event.shift_pressed, &event.alt_pressed);
        
        bmlImcPublish(TOPIC_INPUT_KEY_UP, &event, sizeof(event), BML_IMC_RELIABLE);
    }
    
    // 3. MouseMove事件
    if (m_CurrentMousePos.x != m_LastMousePos.x || m_CurrentMousePos.y != m_LastMousePos.y) {
        MouseMoveEvent event = {};
        event.x = m_CurrentMousePos.x;
        event.y = m_CurrentMousePos.y;
        event.dx = m_CurrentMousePos.x - m_LastMousePos.x;
        event.dy = m_CurrentMousePos.y - m_LastMousePos.y;
        event.timestamp = GetTimestamp();
        
        bmlImcPublish(TOPIC_INPUT_MOUSE_MOVE, &event, sizeof(event), BML_IMC_BEST_EFFORT);
    }
    
    // 4. MouseButton事件
    for (const auto& button_event : m_MouseButtonEvents) {  // 假设有缓存队列
        MouseButtonEvent event = {};
        event.button = button_event.button;
        event.down = button_event.down ? 1 : 0;
        event.double_click = button_event.double_click ? 1 : 0;
        event.x = button_event.x;
        event.y = button_event.y;
        event.timestamp = GetTimestamp();
        
        bmlImcPublish(TOPIC_INPUT_MOUSE_BUTTON, &event, sizeof(event), BML_IMC_RELIABLE);
    }
    
    // 5. MouseWheel事件
    if (m_WheelDelta != 0) {
        MouseWheelEvent event = {};
        event.delta = m_WheelDelta;
        event.x = m_CurrentMousePos.x;
        event.y = m_CurrentMousePos.y;
        event.timestamp = GetTimestamp();
        
        bmlImcPublish(TOPIC_INPUT_MOUSE_WHEEL, &event, sizeof(event), BML_IMC_RELIABLE);
        m_WheelDelta = 0;
    }
    
    // 清理当前帧状态
    m_PressedKeys.clear();
    m_ReleasedKeys.clear();
    m_MouseButtonEvents.clear();
    m_LastMousePos = m_CurrentMousePos;
}
```

**CoreBridge辅助方法简化** (`src/CoreBridge.cpp`更新):
```cpp
// 移除PublishKeyEvent等辅助方法，直接用bmlImcPublish
// 保留PumpEvents()
```

**测试**:
```cpp
// tests/InputEventsTest.cpp
#include <gtest/gtest.h>
#include "Events/InputEvents.h"

TEST(InputEventsTest, StructSizes) {
    EXPECT_EQ(sizeof(BML::Events::KeyEvent), 24);
    EXPECT_EQ(sizeof(BML::Events::MouseMoveEvent), 24);
    EXPECT_EQ(sizeof(BML::Events::MouseButtonEvent), 20);
    EXPECT_EQ(sizeof(BML::Events::MouseWheelEvent), 16);
}

TEST(InputEventsTest, Alignment) {
    BML::Events::KeyEvent ke;
    EXPECT_EQ(alignof(decltype(ke)), 4);
}
```

**集成测试**:
```cpp
// tests/integration/InputEventFlowTest.cpp
TEST(InputEventFlowTest, KeyDownReceived) {
    // 模拟按键
    SimulateKeyPress(VK_SPACE);
    
    // 触发Process
    g_InputHook->Process();
    
    // 驱动IMC
    BML::CoreBridge::Instance().PumpEvents(0);
    
    // 验证BML_Input接收到事件
    EXPECT_TRUE(WaitForDebugOutput("[BML_Input] Key pressed: 0x20", 1000));
}
```

**工作量**: 1.5天
- 事件结构设计: 2小时
- InputHook实现: 6小时
- 单元测试: 2小时
- 集成测试: 2小时

---

#### Task 2.2: RenderHook事件发布（0.5天）

**目标**: 发布渲染事件给BML_Render

**事件协议设计**:

**文件**: `src/Events/RenderEvents.h` (新建)
```cpp
#ifndef BML_RENDER_EVENTS_H
#define BML_RENDER_EVENTS_H

#include <cstdint>

namespace BML {
namespace Events {

constexpr const char* TOPIC_GAME_PRE_RENDER = "BML/Game/PreRender";
constexpr const char* TOPIC_GAME_POST_RENDER = "BML/Game/PostRender";

/**
 * @brief 渲染事件
 * 
 * Topic: BML/Game/PreRender, BML/Game/PostRender
 * 
 * @note CKRenderContext指针仅在事件处理期间有效
 */
struct RenderEvent {
    void*    render_context;  // CKRenderContext* (opaque pointer)
    uint32_t frame_number;    // 帧号
    uint32_t timestamp;       // 时间戳
};
static_assert(sizeof(RenderEvent) == 16, "RenderEvent size must be 16 bytes (x64)");

} // namespace Events
} // namespace BML

#endif // BML_RENDER_EVENTS_H
```

**修改RenderHook实现** (`src/RenderHook.cpp`):
```cpp
#include "Events/RenderEvents.h"
#include "CoreBridge.h"

// 全局帧计数器
static uint32_t g_FrameNumber = 0;

void ExecutePreRenderHooks(CKRenderContext* dev) {
    using namespace BML::Events;
    
    // ===== v0.3钩子（保持不变）=====
    for (auto& hook : s_PreRenderHooks) {
        hook(dev);
    }
    
    // ===== 发布v0.4事件 =====
    RenderEvent event;
    event.render_context = static_cast<void*>(dev);
    event.frame_number = g_FrameNumber;
    event.timestamp = GetTimestamp();
    
    bmlImcPublish(TOPIC_GAME_PRE_RENDER, &event, sizeof(event), BML_IMC_BEST_EFFORT);
}

void ExecutePostRenderHooks(CKRenderContext* dev) {
    using namespace BML::Events;
    
    // ===== v0.3钩子（保持不变）=====
    for (auto& hook : s_PostRenderHooks) {
        hook(dev);
    }
    
    // ===== 发布v0.4事件 =====
    RenderEvent event;
    event.render_context = static_cast<void*>(dev);
    event.frame_number = g_FrameNumber;
    event.timestamp = GetTimestamp();
    
    bmlImcPublish(TOPIC_GAME_POST_RENDER, &event, sizeof(event), BML_IMC_BEST_EFFORT);
    
    ++g_FrameNumber;  // 每帧递增
}
```

**注意事项**:
- 渲染事件使用`BML_IMC_BEST_EFFORT`（允许丢帧）
- CKRenderContext指针作为opaque指针传递
- 模组需要包含CK2头文件才能使用

**测试**:
```cpp
TEST(RenderEventFlowTest, PrePostRenderReceived) {
    // 模拟渲染帧
    CKRenderContext* mock_ctx = GetMockRenderContext();
    ExecutePreRenderHooks(mock_ctx);
    ExecutePostRenderHooks(mock_ctx);
    
    // 驱动IMC
    BML::CoreBridge::Instance().PumpEvents(0);
    
    // 验证BML_Render接收
    EXPECT_TRUE(WaitForDebugOutput("[BML_Render] PreRender frame", 1000));
    EXPECT_TRUE(WaitForDebugOutput("[BML_Render] PostRender frame", 1000));
}
```

**工作量**: 0.5天
- 事件结构设计: 0.5小时
- RenderHook实现: 2小时
- 测试: 1.5小时

---

#### Task 2.3: 生命周期事件（1天）

**目标**: 发布游戏生命周期关键事件

**事件列表**:
```
BML/System/Init          - BMLPlus初始化完成
BML/System/Shutdown      - BMLPlus即将关闭
BML/Game/Load            - 关卡加载开始
BML/Game/LoadComplete    - 关卡加载完成
BML/Game/Start           - 游戏开始（OnCKPlay）
BML/Game/Reset           - 游戏重置（OnCKReset）
BML/Game/Pause           - 游戏暂停（OnCKPause）
BML/Game/Process         - 每帧逻辑更新（PostProcess）
```

**事件协议** (`src/Events/LifecycleEvents.h`):
```cpp
#ifndef BML_LIFECYCLE_EVENTS_H
#define BML_LIFECYCLE_EVENTS_H

#include <cstdint>

namespace BML {
namespace Events {

// Topics
constexpr const char* TOPIC_SYSTEM_INIT = "BML/System/Init";
constexpr const char* TOPIC_SYSTEM_SHUTDOWN = "BML/System/Shutdown";
constexpr const char* TOPIC_GAME_LOAD = "BML/Game/Load";
constexpr const char* TOPIC_GAME_LOAD_COMPLETE = "BML/Game/LoadComplete";
constexpr const char* TOPIC_GAME_START = "BML/Game/Start";
constexpr const char* TOPIC_GAME_RESET = "BML/Game/Reset";
constexpr const char* TOPIC_GAME_PAUSE = "BML/Game/Pause";
constexpr const char* TOPIC_GAME_PROCESS = "BML/Game/Process";

/**
 * @brief 系统生命周期事件（无payload）
 */

/**
 * @brief 游戏进程事件
 * 
 * Topic: BML/Game/Process
 */
struct GameProcessEvent {
    uint32_t frame_number;
    float    delta_time;  // 秒
    uint32_t timestamp;
};

/**
 * @brief 关卡加载事件
 * 
 * Topic: BML/Game/Load, BML/Game/LoadComplete
 */
struct GameLoadEvent {
    char     level_name[64];  // 关卡文件名
    uint32_t timestamp;
};

} // namespace Events
} // namespace BML

#endif // BML_LIFECYCLE_EVENTS_H
```

**集成到ModManager** (`src/ModManager.cpp`):
```cpp
#include "Events/LifecycleEvents.h"

CKERROR ModManager::OnCKInit(CKContext *context) {
    // ... 现有初始化
    
    // 发布Init事件
    bmlImcPublish(BML::Events::TOPIC_SYSTEM_INIT, nullptr, 0, BML_IMC_RELIABLE);
    
    return CK_OK;
}

CKERROR ModManager::OnCKPlay() {
    // ... 现有逻辑
    
    // 发布Start事件
    bmlImcPublish(BML::Events::TOPIC_GAME_START, nullptr, 0, BML_IMC_RELIABLE);
    
    return CK_OK;
}

CKERROR ModManager::OnCKReset() {
    // ... 现有逻辑
    
    // 发布Reset事件
    bmlImcPublish(BML::Events::TOPIC_GAME_RESET, nullptr, 0, BML_IMC_RELIABLE);
    
    return CK_OK;
}

CKERROR ModManager::OnCKPause() {
    // ... 现有逻辑
    
    // 发布Pause事件
    bmlImcPublish(BML::Events::TOPIC_GAME_PAUSE, nullptr, 0, BML_IMC_RELIABLE);
    
    return CK_OK;
}

CKERROR ModManager::PostProcess() {
    // ... 驱动IMC
    BML::CoreBridge::Instance().PumpEvents(0);
    
    // 发布Process事件
    BML::Events::GameProcessEvent event;
    event.frame_number = m_FrameNumber++;
    event.delta_time = m_Context->GetTimeManager()->GetLastDeltaTime() / 1000.0f;
    event.timestamp = GetTimestamp();
    
    bmlImcPublish(BML::Events::TOPIC_GAME_PROCESS, &event, sizeof(event), BML_IMC_BEST_EFFORT);
    
    // ... v0.3逻辑
    m_ModContext->OnProcess();
    
    return CK_OK;
}

ModManager::~ModManager() {
    // 发布Shutdown事件
    bmlImcPublish(BML::Events::TOPIC_SYSTEM_SHUTDOWN, nullptr, 0, BML_IMC_RELIABLE);
    
    // ... 现有清理
}
```

**关卡加载钩子** (需要查找实际位置):
```cpp
// 假设在 src/ExecuteBB.cpp 或类似位置
void OnLevelLoadStart(const char* level_name) {
    BML::Events::GameLoadEvent event = {};
    strncpy_s(event.level_name, level_name, sizeof(event.level_name) - 1);
    event.timestamp = GetTimestamp();
    
    bmlImcPublish(BML::Events::TOPIC_GAME_LOAD, &event, sizeof(event), BML_IMC_RELIABLE);
}

void OnLevelLoadComplete(const char* level_name) {
    BML::Events::GameLoadEvent event = {};
    strncpy_s(event.level_name, level_name, sizeof(event.level_name) - 1);
    event.timestamp = GetTimestamp();
    
    bmlImcPublish(BML::Events::TOPIC_GAME_LOAD_COMPLETE, &event, sizeof(event), BML_IMC_RELIABLE);
}
```

**测试**:
```cpp
TEST(LifecycleEventsTest, InitShutdown) {
    // 模拟初始化
    OnCKInit();
    
    // 验证事件
    std::vector<std::string> received_topics;
    SubscribeToAll([&](const char* topic) { received_topics.push_back(topic); });
    
    BML::CoreBridge::Instance().PumpEvents(0);
    
    EXPECT_THAT(received_topics, Contains("BML/System/Init"));
}

TEST(LifecycleEventsTest, GameLoop) {
    OnCKPlay();
    
    for (int i = 0; i < 10; ++i) {
        PostProcess();
    }
    
    BML::CoreBridge::Instance().PumpEvents(0);
    
    // 应接收到: 1个Start + 10个Process
    EXPECT_EQ(CountEvents("BML/Game/Start"), 1);
    EXPECT_EQ(CountEvents("BML/Game/Process"), 10);
}
```

**工作量**: 1天
- 事件设计: 2小时
- 集成到ModManager: 4小时
- 查找关卡加载钩子: 2小时
- 测试: 2小时

---

#### Task 2.4: 事件协议文档（1天）

**目标**: 创建完整的IMC事件协议参考文档

**文件**: `docs/reference/imc-event-protocol.md`

**文档结构**:
```markdown
# IMC事件协议参考

## 概述

本文档定义BML v0.4.0的Inter-Mod Communication (IMC)事件协议。
所有事件通过IMC总线发布，模组通过订阅接收。

## 事件类别

### 1. 输入事件 (Input Events)

#### BML/Input/KeyDown
**触发**: 键盘按键按下
**Payload**: `BML::Events::KeyEvent` (24 bytes)
**可靠性**: RELIABLE

字段说明:
- `key` (uint32_t): 虚拟键码（VK_*常量）
- `scan_code` (uint32_t): 硬件扫描码
- `timestamp` (uint32_t): 事件时间戳（毫秒）
- `is_repeat` (uint8_t): 1=重复按键, 0=首次按下
- `is_extended` (uint8_t): 1=扩展键（如右Alt）
- `ctrl_pressed` (uint8_t): Ctrl是否按下
- `shift_pressed` (uint8_t): Shift是否按下
- `alt_pressed` (uint8_t): Alt是否按下

示例代码:
```cpp
void OnKeyDown(const void* payload, size_t size, 
               const BML_ImcMessageInfo* info, void* user_data) {
    auto* event = static_cast<const BML::Events::KeyEvent*>(payload);
    if (event->key == VK_SPACE) {
        printf("Space pressed at %u ms\n", event->timestamp);
    }
}

bmlImcSubscribe("BML/Input/KeyDown", OnKeyDown, nullptr, &sub);
```

#### BML/Input/KeyUp
（类似结构...）

### 2. 渲染事件 (Render Events)

#### BML/Game/PreRender
（详细说明...）

### 3. 生命周期事件 (Lifecycle Events)

（详细说明...）

## 最佳实践

### 事件订阅
```cpp
// 在BML_ATTACH阶段订阅
BML_Result BML_ENTRYPOINT(BML_ATTACH, BML_Context ctx, PFN_BML_GetProcAddress get_proc) {
    bmlImcSubscribe("BML/Input/KeyDown", OnKeyDown, my_data, &g_Subscription);
    return BML_RESULT_OK;
}

// 在BML_DETACH阶段取消订阅
BML_Result BML_ENTRYPOINT(BML_DETACH, BML_Context ctx, PFN_BML_GetProcAddress get_proc) {
    bmlImcUnsubscribe(g_Subscription);
    return BML_RESULT_OK;
}
```

### 性能考虑
- 输入事件使用RELIABLE，确保不丢失
- 渲染/Process事件使用BEST_EFFORT，允许丢帧
- 回调函数应快速返回（<1ms）
- 避免在回调中进行重计算

### 线程安全
- 所有事件回调在主线程执行
- 无需额外同步机制
- 但不应阻塞主线程

## 版本兼容性

- v0.4.0: 初始协议
- 未来版本: 只添加新事件，不修改现有结构
- 结构体大小和对齐保证ABI稳定

## 参考
- [IMC总线架构](../developer-guide/imc-bus.md)
- [模组开发指南](../developer-guide/module-development.md)
```

**工作量**: 1天
- 文档编写: 6小时
- 代码示例: 2小时

---

### 5.3 验证测试

#### 测试1: BML_Input完整测试

**测试脚本**: `tests/integration/test_bml_input.py`
```python
import subprocess
import time
import re

def test_input_events():
    """测试BML_Input接收所有输入事件"""
    
    # 启动游戏 + DebugView
    debugview = start_debugview()
    game = start_game()
    
    time.sleep(5)  # 等待加载
    
    # 模拟输入
    simulate_key_press('SPACE')
    time.sleep(0.1)
    simulate_key_release('SPACE')
    
    simulate_mouse_move(100, 100)
    simulate_mouse_click('LEFT')
    
    time.sleep(1)
    
    # 检查日志
    logs = get_debugview_output(debugview)
    
    assert '[BML_Input] KeyDown: 0x20' in logs
    assert '[BML_Input] KeyUp: 0x20' in logs
    assert '[BML_Input] MouseMove' in logs
    assert '[BML_Input] MouseButton' in logs
    
    cleanup(game, debugview)

if __name__ == '__main__':
    test_input_events()
    print("✓ All input events received successfully")
```

#### 测试2: BML_Render完整测试

```python
def test_render_events():
    """测试BML_Render接收渲染事件"""
    
    debugview = start_debugview()
    game = start_game()
    
    time.sleep(5)
    
    # 运行100帧
    time.sleep(100 / 60.0)  # 约1.67秒
    
    logs = get_debugview_output(debugview)
    
    # 应至少收到50帧的事件
    pre_render_count = logs.count('[BML_Render] PreRender')
    post_render_count = logs.count('[BML_Render] PostRender')
    
    assert pre_render_count >= 50
    assert post_render_count >= 50
    assert abs(pre_render_count - post_render_count) <= 2  # 配对
    
    cleanup(game, debugview)
```

#### 测试3: 生命周期事件测试

```python
def test_lifecycle_events():
    """测试生命周期事件序列"""
    
    debugview = start_debugview()
    game = start_game()
    
    time.sleep(10)  # 等待完整启动
    
    logs = get_debugview_output(debugview)
    
    # 验证事件顺序
    events = extract_events(logs)
    
    expected_sequence = [
        'BML/System/Init',
        'BML/Game/Load',
        'BML/Game/LoadComplete',
        'BML/Game/Start',
        'BML/Game/Process',  # 多次
    ]
    
    assert matches_sequence(events, expected_sequence)
    
    cleanup(game, debugview)
```

### 5.4 Phase 2检查点

**完成标准**:
```
✅ 输入事件完整发布（5种事件）
✅ 渲染事件正常工作（2种事件）
✅ 生命周期事件完整（8种事件）
✅ BML_Input接收所有输入事件
✅ BML_Render接收渲染事件
✅ 事件协议文档完成
✅ 集成测试通过
✅ 性能测试满足目标（<0.2ms/frame）
```

**交付物**:
- [ ] `src/Events/InputEvents.h`
- [ ] `src/Events/RenderEvents.h`
- [ ] `src/Events/LifecycleEvents.h`
- [ ] 修改后的`InputHook.cpp`, `RenderHook.cpp`, `ModManager.cpp`
- [ ] `docs/reference/imc-event-protocol.md`
- [ ] `tests/integration/test_bml_input.py`
- [ ] `tests/integration/test_bml_render.py`
- [ ] Phase 2测试报告

**性能基准**:
```
事件发布延迟: <10μs
IMC Pump时间: <100μs (10订阅者)
帧率影响: <1% (60fps → 59.4fps)
```

**决策点**:
- ✅ **Go**: 事件系统稳定，继续Phase 3
- 🔄 **Review**: 性能未达标，优化后继续
- ❌ **No-Go**: 架构缺陷，重新设计

---

## 🔀 第六部分：Phase 3 - 模组加载统一（Week 3）

### 6.1 Phase目标

**总体目标**: 实现v0.3与v0.4模组共存加载

**具体目标**:
1. 创建双路径加载逻辑
2. 实现模组适配器（v0.4 → v0.3接口）
3. 统一模组管理列表
4. 处理依赖关系冲突
5. 错误隔离机制

**交付标准**:
```
✓ v0.3模组正常加载
✓ v0.4模组正常加载
✓ 混合场景工作（同时存在v0.3和v0.4模组）
✓ 依赖解析正确
✓ 加载失败不影响其他模组
✓ 统一的模组列表UI
```

### 6.2 任务分解

#### Task 3.1: 双路径加载器（2天）

**目标**: ModContext支持v0.3和v0.4双路径

**设计方案**:

**文件**: `src/ModuleAdapter.h` (新建)
```cpp
#ifndef BML_MODULE_ADAPTER_H
#define BML_MODULE_ADAPTER_H

#include "BML.h"
#include "IMod.h"
#include <string>

namespace BML {

/**
 * @brief v0.4模组到v0.3接口的适配器
 * 
 * 包装v0.4模组，使其表现为IMod接口
 * 允许ModContext统一管理
 */
class ModuleAdapter : public IMod {
public:
    ModuleAdapter(BML_ModuleHandle mod_handle, const std::string& id);
    ~ModuleAdapter() override;
    
    // === IMod接口实现 ===
    const char* GetID() override { return m_ID.c_str(); }
    const char* GetVersion() override { return m_Version.c_str(); }
    const char* GetName() override { return m_Name.c_str(); }
    const char* GetAuthor() override { return m_Author.c_str(); }
    const char* GetDescription() override { return m_Description.c_str(); }
    
    CKDWORD GetCategory() override { return 0; }  // 未使用
    
    // 生命周期回调（转发到IMC事件订阅）
    void OnLoad() override;
    void OnUnload() override;
    bool OnInit() override;
    void OnShutdown() override;
    void OnPreStartMenu() override;
    void OnPostStartMenu() override;
    void OnPreExitGame() override;
    void OnPostExitGame() override;
    void OnPreLoadLevel() override;
    void OnPostLoadLevel() override;
    void OnStartLevel() override;
    void OnPreResetLevel() override;
    void OnPostResetLevel() override;
    void OnPauseLevel() override;
    void OnUnpauseLevel() override;
    void OnPreNextLevel() override;
    void OnPostNextLevel() override;
    void OnDead() override;
    void OnPreEndLevel() override;
    void OnPostEndLevel() override;
    
    void OnProcess() override;      // → BML/Game/Process事件
    void OnRender(CKRenderContext *dev) override;  // → BML/Game/PostRender事件
    
    // === 适配器特有方法 ===
    bool IsV04Module() const { return true; }
    BML_ModuleHandle GetHandle() const { return m_Handle; }
    
private:
    BML_ModuleHandle m_Handle;
    std::string m_ID;
    std::string m_Version;
    std::string m_Name;
    std::string m_Author;
    std::string m_Description;
    
    // 标记：是否已调用OnLoad等
    bool m_Loaded;
    bool m_Initialized;
};

} // namespace BML

#endif // BML_MODULE_ADAPTER_H
```

**实现** (`src/ModuleAdapter.cpp`):
```cpp
#include "ModuleAdapter.h"
#include "CoreBridge.h"

namespace BML {

ModuleAdapter::ModuleAdapter(BML_ModuleHandle mod_handle, const std::string& id)
    : m_Handle(mod_handle)
    , m_ID(id)
    , m_Loaded(false)
    , m_Initialized(false)
{
    // 从Core查询模组信息
    BML_SemVer version;
    if (bmlModGetVersion(m_Handle, &version) == BML_RESULT_OK) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%u.%u.%u", version.major, version.minor, version.patch);
        m_Version = buf;
    }
    
    // 从manifest获取名称等（需要Core API支持）
    // TODO: bmlModGetManifest() API
    m_Name = m_ID;  // 临时用ID
    m_Author = "Unknown";
    m_Description = "";
}

ModuleAdapter::~ModuleAdapter() {
    // v0.4模组由Core管理生命周期，这里不做清理
}

void ModuleAdapter::OnLoad() {
    // v0.4模组在bmlLoadModules()时已加载
    // 这里仅标记
    m_Loaded = true;
}

void ModuleAdapter::OnUnload() {
    m_Loaded = false;
    // 实际卸载由Core的bmlDetach()完成
}

bool ModuleAdapter::OnInit() {
    // v0.4模组没有单独的Init阶段
    // 在BML_ATTACH时已初始化
    m_Initialized = true;
    return true;
}

void ModuleAdapter::OnShutdown() {
    m_Initialized = false;
}

// 其他生命周期回调空实现（v0.4通过IMC事件订阅）
void ModuleAdapter::OnPreStartMenu() {}
void ModuleAdapter::OnPostStartMenu() {}
void ModuleAdapter::OnPreExitGame() {}
void ModuleAdapter::OnPostExitGame() {}
void ModuleAdapter::OnPreLoadLevel() {}
void ModuleAdapter::OnPostLoadLevel() {}
void ModuleAdapter::OnStartLevel() {}
void ModuleAdapter::OnPreResetLevel() {}
void ModuleAdapter::OnPostResetLevel() {}
void ModuleAdapter::OnPauseLevel() {}
void ModuleAdapter::OnUnpauseLevel() {}
void ModuleAdapter::OnPreNextLevel() {}
void ModuleAdapter::OnPostNextLevel() {}
void ModuleAdapter::OnDead() {}
void ModuleAdapter::OnPreEndLevel() {}
void ModuleAdapter::OnPostEndLevel() {}

void ModuleAdapter::OnProcess() {
    // v0.4模组通过订阅BML/Game/Process事件处理
    // 这里不需要做任何事
}

void ModuleAdapter::OnRender(CKRenderContext *dev) {
    // 同上，v0.4通过BML/Game/PostRender事件
}

} // namespace BML
```

**修改ModContext::LoadMods** (`src/ModContext.cpp`):
```cpp
#include "ModuleAdapter.h"
#include "CoreBridge.h"

bool ModContext::LoadMods() {
    // ===== 步骤1: 加载v0.4模组（通过Core）=====
    // 已在ModManager::OnCKPlay中调用bmlLoadModules()
    // 这里获取加载结果并创建适配器
    
    auto& bridge = BML::CoreBridge::Instance();
    if (bridge.IsLoaded()) {
        auto modules = bridge.GetModules();
        for (const auto& mod_info : modules) {
            if (mod_info.loaded) {
                // 创建适配器
                BML_ModuleHandle handle = nullptr;
                if (bmlGetModuleById(mod_info.id.c_str(), &handle) == BML_RESULT_OK) {
                    auto adapter = new BML::ModuleAdapter(handle, mod_info.id);
                    m_Mods.push_back(adapter);
                    
                    GetLogger()->Info("ModContext", "Loaded v0.4 module: %s v%s",
                        mod_info.name.c_str(), mod_info.version.c_str());
                }
            }
        }
    }
    
    // ===== 步骤2: 注册内置模组（v0.3）=====
    RegisterBuiltinMods();
    
    // ===== 步骤3: 加载外部v0.3模组 =====
    std::wstring modPath = m_LoaderDir + L"\\Mods";
    std::vector<std::wstring> modPaths;
    
    // 扫描目录
    ExploreMods(modPath, modPaths);
    
    for (auto &path : modPaths) {
        try {
            // 检查是否有mod.toml（v0.4模组标识）
            std::wstring toml_path = path + L"\\mod.toml";
            if (PathFileExistsW(toml_path.c_str())) {
                // v0.4模组，跳过（已由Core加载）
                continue;
            }
            
            // 加载v0.3模组
            IMod *mod = LoadMod(path);
            if (mod) {
                m_Mods.push_back(mod);
                GetLogger()->Info("ModContext", "Loaded v0.3 module: %s v%s",
                    mod->GetID(), mod->GetVersion());
            }
        } catch (const std::exception& e) {
            GetLogger()->Error("ModContext", "Failed to load mod at %S: %s",
                path.c_str(), e.what());
            // 继续加载其他模组
        }
    }
    
    SetFlags(BML_MODS_LOADED);
    GetLogger()->Info("ModContext", "Loaded %zu module(s) total", m_Mods.size());
    
    return true;
}
```

**工作量**: 2天
- ModuleAdapter设计与实现: 8小时
- ModContext双路径逻辑: 6小时
- 测试: 2小时

---

#### Task 3.2: 依赖关系统一（1天）

**目标**: 处理v0.3与v0.4模组的依赖关系

**挑战**:
- v0.3使用`AddDependency()`运行时注册
- v0.4使用`mod.toml`静态声明
- 需要统一视图

**方案**: 扩展Core API暴露依赖图

**新增Core API** (`src/Core/Export.cpp`):
```cpp
// 查询模组依赖
BML_API BML_Result bmlModGetDependencies(
    BML_ModuleHandle module,
    const char*** out_dep_ids,
    size_t* out_count
);

// 查询加载顺序
BML_API BML_Result bmlGetLoadOrder(
    const char*** out_mod_ids,
    size_t* out_count
);
```

**ModContext使用**:
```cpp
void ModContext::PrintDependencyTree() {
    GetLogger()->Info("ModContext", "=== Dependency Tree ===");
    
    // v0.4模组依赖（来自Core）
    const char** load_order = nullptr;
    size_t count = 0;
    if (bmlGetLoadOrder(&load_order, &count) == BML_RESULT_OK) {
        for (size_t i = 0; i < count; ++i) {
            GetLogger()->Info("ModContext", "[%zu] %s (v0.4)", i, load_order[i]);
            
            // 打印依赖
            BML_ModuleHandle handle = nullptr;
            bmlGetModuleById(load_order[i], &handle);
            
            const char** deps = nullptr;
            size_t dep_count = 0;
            if (bmlModGetDependencies(handle, &deps, &dep_count) == BML_RESULT_OK) {
                for (size_t j = 0; j < dep_count; ++j) {
                    GetLogger()->Info("ModContext", "  -> depends on: %s", deps[j]);
                }
            }
        }
    }
    
    // v0.3模组依赖（来自m_Mods）
    for (IMod* mod : m_Mods) {
        if (auto* adapter = dynamic_cast<BML::ModuleAdapter*>(mod)) {
            // v0.4模组，已打印
            continue;
        }
        
        GetLogger()->Info("ModContext", "- %s (v0.3)", mod->GetID());
        // v0.3依赖信息没有统一API，可能需要扩展IMod接口
    }
}
```

**工作量**: 1天

---

#### Task 3.3: 错误隔离与回滚（1.5天）

**目标**: 一个模组加载失败不影响其他模组

**Core已支持**:
- `ModuleLoader::LoadModules()`有原子回滚
- v0.4模组加载失败会自动清理

**需要添加**: v0.3模组错误隔离

**修改ModContext::LoadMods**:
```cpp
bool ModContext::LoadMods() {
    size_t v03_success = 0;
    size_t v03_failed = 0;
    size_t v04_success = 0;
    
    // ===== v0.4加载（已有错误处理）=====
    auto& bridge = BML::CoreBridge::Instance();
    if (bridge.IsLoaded()) {
        auto modules = bridge.GetModules();
        for (const auto& mod_info : modules) {
            if (mod_info.loaded) {
                // ... 创建适配器
                v04_success++;
            }
        }
    }
    
    // ===== v0.3加载（添加try-catch）=====
    RegisterBuiltinMods();
    
    std::vector<std::wstring> modPaths;
    ExploreMods(m_LoaderDir + L"\\Mods", modPaths);
    
    for (auto &path : modPaths) {
        try {
            // 检查v0.4标识
            if (PathFileExistsW((path + L"\\mod.toml").c_str())) {
                continue;
            }
            
            // 加载v0.3模组
            IMod *mod = LoadModSafe(path);  // 新的安全加载方法
            if (mod) {
                m_Mods.push_back(mod);
                v03_success++;
            } else {
                v03_failed++;
            }
        } catch (const std::exception& e) {
            GetLogger()->Error("ModContext", "Exception loading mod at %S: %s",
                path.c_str(), e.what());
            v03_failed++;
        } catch (...) {
            GetLogger()->Error("ModContext", "Unknown exception loading mod at %S", path.c_str());
            v03_failed++;
        }
    }
    
    // ===== 汇总报告 =====
    GetLogger()->Info("ModContext", "Load Summary:");
    GetLogger()->Info("ModContext", "  v0.4 modules: %zu loaded", v04_success);
    GetLogger()->Info("ModContext", "  v0.3 modules: %zu loaded, %zu failed", v03_success, v03_failed);
    GetLogger()->Info("ModContext", "  Total: %zu modules active", m_Mods.size());
    
    SetFlags(BML_MODS_LOADED);
    return true;  // 即使有失败，也返回true继续运行
}

IMod* ModContext::LoadModSafe(const std::wstring& path) {
    // 安全加载：捕获DLL加载/初始化异常
    __try {
        return LoadMod(path);  // 现有实现
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        GetLogger()->Error("ModContext", "Access violation loading mod at %S", path.c_str());
        return nullptr;
    }
}
```

**工作量**: 1.5天

---

### 6.3 Phase 3检查点

**完成标准**:
```
✅ v0.3和v0.4模组同时加载
✅ ModuleAdapter正确工作
✅ 依赖关系可查询
✅ 错误隔离机制完善
✅ 加载失败不影响游戏启动
✅ 统一的模组管理UI
```

**交付物**:
- [ ] `src/ModuleAdapter.h` + `.cpp`
- [ ] 修改后的`ModContext.cpp`
- [ ] 新增Core API（依赖查询）
- [ ] `tests/ModuleAdapterTest.cpp`
- [ ] `tests/integration/test_mixed_modules.py`
- [ ] Phase 3测试报告

**测试场景**:
```
场景1: 仅v0.3模组（向后兼容）
场景2: 仅v0.4模组
场景3: 混合模组（3个v0.3 + 2个v0.4）
场景4: v0.4模组加载失败
场景5: v0.3模组加载失败
场景6: 依赖关系（v0.4 → v0.4）
```

**决策点**:
- ✅ **Go**: 混合加载稳定，继续Phase 4
- 🔄 **Review**: 兼容性问题，修复后继续
- ❌ **No-Go**: 根本性冲突，重新设计

---

## ✅ 第七部分：Phase 4 - 测试与优化（Week 4）

### 7.1 Phase目标

**总体目标**: 确保集成质量和性能达标

**具体目标**:
1. 完善单元测试套件（覆盖率 >80%）
2. 完整集成测试（所有场景）
3. 性能基准测试与优化
4. 兼容性回归测试
5. 压力测试与边界条件

**交付标准**:
```
✓ 单元测试覆盖率 >80%
✓ 所有集成测试通过
✓ 性能满足目标（<0.2ms/frame）
✓ 回归测试100%通过
✓ 无内存泄漏
✓ 无崩溃/死锁
```

### 7.2 任务分解

#### Task 4.1: 单元测试完善（1.5天）

**目标**: 补充所有新增代码的单元测试

**测试清单**:

**1. CoreBridge测试** (`tests/CoreBridgeTest.cpp`):
```cpp
TEST(CoreBridgeTest, Lifecycle) {
    auto& bridge = BML::CoreBridge::Instance();
    
    // 初始状态
    EXPECT_EQ(bridge.GetStatus(), BML::CoreStatus::Uninitialized);
    
    // Attach
    EXPECT_TRUE(bridge.Attach());
    EXPECT_EQ(bridge.GetStatus(), BML::CoreStatus::Attached);
    
    // 重复Attach应失败
    EXPECT_FALSE(bridge.Attach());
    
    // LoadModules
    EXPECT_TRUE(bridge.LoadModules());
    EXPECT_EQ(bridge.GetStatus(), BML::CoreStatus::Loaded);
    
    // Detach
    bridge.Detach();
    EXPECT_EQ(bridge.GetStatus(), BML::CoreStatus::Uninitialized);
}

TEST(CoreBridgeTest, ErrorHandling) {
    auto& bridge = BML::CoreBridge::Instance();
    
    // 未Attach就LoadModules应失败
    EXPECT_FALSE(bridge.LoadModules());
    
    auto error = bridge.GetLastErrorInfo();
    EXPECT_EQ(error.code, BML::CoreError::NotAttached);
}

TEST(CoreBridgeTest, ModuleQuery) {
    auto& bridge = BML::CoreBridge::Instance();
    bridge.Attach();
    
    auto modules = bridge.GetModules();
    EXPECT_GT(modules.size(), 0);  // 应至少有测试模组
    
    for (const auto& mod : modules) {
        EXPECT_FALSE(mod.id.empty());
        EXPECT_FALSE(mod.version.empty());
    }
}

TEST(CoreBridgeTest, EventPublishing) {
    auto& bridge = BML::CoreBridge::Instance();
    bridge.Attach();
    bridge.LoadModules();
    
    // 发布事件
    EXPECT_TRUE(bridge.PublishLifecycleEvent("BML/Test/Event"));
    
    // Pump应不崩溃
    bridge.PumpEvents(10);
}
```

**2. ModuleAdapter测试** (`tests/ModuleAdapterTest.cpp`):
```cpp
class ModuleAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        BML::CoreBridge::Instance().Attach();
        BML::CoreBridge::Instance().LoadModules();
        
        // 获取第一个v0.4模组
        BML_ModuleHandle handle = nullptr;
        bmlGetModuleByIndex(0, &handle);
        
        const char* id = nullptr;
        bmlModGetId(handle, &id);
        
        adapter = new BML::ModuleAdapter(handle, id);
    }
    
    void TearDown() override {
        delete adapter;
        BML::CoreBridge::Instance().Detach();
    }
    
    BML::ModuleAdapter* adapter;
};

TEST_F(ModuleAdapterTest, IModInterface) {
    EXPECT_NE(adapter->GetID(), nullptr);
    EXPECT_NE(adapter->GetVersion(), nullptr);
    EXPECT_TRUE(adapter->IsV04Module());
}

TEST_F(ModuleAdapterTest, LifecycleCallbacks) {
    // 这些应不崩溃
    adapter->OnLoad();
    EXPECT_TRUE(adapter->OnInit());
    adapter->OnProcess();
    adapter->OnRender(nullptr);
    adapter->OnShutdown();
    adapter->OnUnload();
}
```

**3. 事件结构测试** (`tests/EventStructTest.cpp`):
```cpp
TEST(EventStructTest, Sizes) {
    EXPECT_EQ(sizeof(BML::Events::KeyEvent), 24);
    EXPECT_EQ(sizeof(BML::Events::MouseMoveEvent), 24);
    EXPECT_EQ(sizeof(BML::Events::MouseButtonEvent), 20);
    EXPECT_EQ(sizeof(BML::Events::MouseWheelEvent), 16);
    EXPECT_EQ(sizeof(BML::Events::RenderEvent), 16);
}

TEST(EventStructTest, Alignment) {
    EXPECT_EQ(alignof(BML::Events::KeyEvent), 4);
    EXPECT_EQ(alignof(BML::Events::RenderEvent), 8);
}

TEST(EventStructTest, ZeroInitialization) {
    BML::Events::KeyEvent event = {};
    EXPECT_EQ(event.key, 0);
    EXPECT_EQ(event.timestamp, 0);
    EXPECT_EQ(event.is_repeat, 0);
}
```

**运行测试**:
```powershell
# 编译测试
cmake --build build --config Release --target tests

# 运行测试
cd build/bin/Release
.\BMLTests.exe --gtest_output=xml:test_results.xml

# 查看覆盖率（需要OpenCppCoverage）
OpenCppCoverage --sources src\Core --sources src\*.cpp -- .\BMLTests.exe
```

**工作量**: 1.5天
- 编写测试: 8小时
- 修复发现的问题: 4小时

---

#### Task 4.2: 集成测试套件（1.5天）

**目标**: 端到端场景测试

**测试框架**: Python + pytest

**文件结构**:
```
tests/integration/
├── conftest.py          # 共享fixtures
├── test_basic.py        # 基础功能
├── test_input_events.py # 输入事件
├── test_render_events.py# 渲染事件
├── test_lifecycle.py    # 生命周期
├── test_mixed_mods.py   # 混合模组
└── utils.py             # 辅助函数
```

**conftest.py**:
```python
import pytest
import subprocess
import time
import os

@pytest.fixture(scope="session")
def game_exe():
    """游戏可执行文件路径"""
    return os.environ.get("BALLANCE_EXE", r"C:\Program Files\Ballance\Player.exe")

@pytest.fixture(scope="session")
def mods_dir():
    """模组目录路径"""
    return os.environ.get("BALLANCE_MODS", r"C:\Program Files\Ballance\ModLoader\Mods")

@pytest.fixture
def game_process(game_exe):
    """启动游戏进程"""
    proc = subprocess.Popen([game_exe], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(5)  # 等待加载
    yield proc
    proc.terminate()
    proc.wait(timeout=5)

@pytest.fixture
def debugview():
    """启动DebugView++监控日志"""
    dbgview = subprocess.Popen([r"C:\Tools\DebugViewPP.exe", "/accepteula", "/NoUI"])
    time.sleep(1)
    yield dbgview
    dbgview.terminate()
```

**test_basic.py**:
```python
def test_game_starts(game_process):
    """测试游戏能够正常启动"""
    assert game_process.poll() is None  # 进程应该还在运行
    time.sleep(10)
    assert game_process.poll() is None  # 10秒后仍在运行

def test_bml_loaded(game_process, debugview):
    """测试BML成功加载"""
    time.sleep(3)
    logs = get_debug_output()
    assert "BML Core attached successfully" in logs
    assert "BML_Input" in logs or "BML_Render" in logs

def test_v03_compatibility(game_process):
    """测试v0.3模组兼容性"""
    # 假设有一个v0.3测试模组
    time.sleep(5)
    logs = get_debug_output()
    assert "TestModV03 loaded" in logs
```

**test_input_events.py**:
```python
import pyautogui

def test_key_events(game_process):
    """测试按键事件"""
    time.sleep(5)
    
    # 模拟按键
    pyautogui.press('space')
    time.sleep(0.2)
    
    logs = get_debug_output()
    assert "[BML_Input] KeyDown: 0x20" in logs
    assert "[BML_Input] KeyUp: 0x20" in logs

def test_mouse_events(game_process):
    """测试鼠标事件"""
    time.sleep(5)
    
    # 移动鼠标
    pyautogui.moveTo(500, 500)
    time.sleep(0.2)
    
    # 点击
    pyautogui.click()
    time.sleep(0.2)
    
    logs = get_debug_output()
    assert "[BML_Input] MouseMove" in logs
    assert "[BML_Input] MouseButton" in logs
```

**test_mixed_mods.py**:
```python
def test_v03_and_v04_coexist(mods_dir):
    """测试v0.3和v0.4模组共存"""
    # 准备环境：放置v0.3和v0.4模组
    setup_test_mods(mods_dir, v03=["TestModOld.dll"], v04=["BML_Input"])
    
    proc = start_game()
    time.sleep(5)
    
    logs = get_debug_output()
    
    # 验证两种模组都加载了
    assert "Loaded v0.3 module: TestModOld" in logs
    assert "Loaded v0.4 module: BML_Input" in logs
    
    cleanup(proc)

def test_load_failure_isolation(mods_dir):
    """测试加载失败隔离"""
    # 放置一个会崩溃的模组和一个正常模组
    setup_test_mods(mods_dir, v03=["BadMod.dll", "GoodMod.dll"])
    
    proc = start_game()
    time.sleep(5)
    
    logs = get_debug_output()
    
    # BadMod应失败，但GoodMod应成功
    assert "Failed to load mod" in logs
    assert "BadMod" in logs
    assert "Loaded v0.3 module: GoodMod" in logs
    
    # 游戏应继续运行
    assert proc.poll() is None
    
    cleanup(proc)
```

**运行集成测试**:
```powershell
cd tests/integration
python -m pytest -v --html=report.html
```

**工作量**: 1.5天

---

#### Task 4.3: 性能基准测试（1天）

**目标**: 验证性能目标达成

**基准测试工具**: `benchmarks/IntegrationBench.cpp`

```cpp
#include <benchmark/benchmark.h>
#include "CoreBridge.h"
#include "Events/InputEvents.h"

// 测试事件发布性能
static void BM_ImcPublish(benchmark::State& state) {
    BML::CoreBridge::Instance().Attach();
    BML::CoreBridge::Instance().LoadModules();
    
    BML::Events::KeyEvent event = {};
    event.key = VK_SPACE;
    event.timestamp = 12345;
    
    for (auto _ : state) {
        bmlImcPublish("BML/Input/KeyDown", &event, sizeof(event), BML_IMC_RELIABLE);
    }
}
BENCHMARK(BM_ImcPublish);

// 测试IMC Pump性能
static void BM_ImcPump(benchmark::State& state) {
    auto& bridge = BML::CoreBridge::Instance();
    bridge.Attach();
    bridge.LoadModules();
    
    // 发布100个事件
    BML::Events::KeyEvent event = {};
    for (int i = 0; i < 100; ++i) {
        bmlImcPublish("BML/Input/KeyDown", &event, sizeof(event), BML_IMC_RELIABLE);
    }
    
    for (auto _ : state) {
        bridge.PumpEvents(0);  // 处理所有
    }
}
BENCHMARK(BM_ImcPump);

// 测试模组加载性能
static void BM_ModuleLoad(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        BML::CoreBridge::Instance().Detach();
        state.ResumeTiming();
        
        BML::CoreBridge::Instance().Attach();
        BML::CoreBridge::Instance().LoadModules();
    }
}
BENCHMARK(BM_ModuleLoad);

BENCHMARK_MAIN();
```

**运行基准测试**:
```powershell
cmake --build build --config Release --target IntegrationBench
.\build\bin\Release\IntegrationBench.exe --benchmark_out=bench_results.json
```

**性能目标验证**:
```
Benchmark                    Time       CPU   Iterations
---------------------------------------------------------
BM_ImcPublish             5.2 us     5.1 us     130000   ← 目标<10us ✓
BM_ImcPump               85.3 us    84.7 us       8200   ← 目标<100us ✓
BM_ModuleLoad            92 ms      91 ms           7   ← 目标<100ms ✓
```

**游戏内性能测试**:
```cpp
// 在ModManager::PostProcess()中添加计时
auto start = std::chrono::high_resolution_clock::now();

BML::CoreBridge::Instance().PumpEvents(0);

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

if (duration > 200) {  // >200us
    m_ModContext->GetLogger()->Warn("Performance", "IMC Pump took %lld us", duration);
}
```

**工作量**: 1天

---

#### Task 4.4: 兼容性回归测试（1天）

**目标**: 确保v0.3模组100%兼容

**测试方法**: 使用已知的v0.3模组测试

**测试模组列表**:
```
1. BMLMod (内置)
2. NewBallTypeMod (内置)
3. 社区模组A
4. 社区模组B
5. 社区模组C
```

**测试清单**:
```markdown
# v0.3兼容性检查清单

## 基础功能
- [ ] 模组加载成功
- [ ] OnLoad/OnInit回调正常
- [ ] GetID/GetVersion返回正确
- [ ] OnProcess每帧调用
- [ ] OnRender正常工作

## API兼容性
- [ ] BML_GetVersion() 可用
- [ ] BML_Malloc/Free 工作
- [ ] Logger功能正常
- [ ] Config读写正常
- [ ] DataShare可用

## UI功能
- [ ] HUD显示正常
- [ ] CommandBar可用
- [ ] ModMenu显示配置
- [ ] MessageBoard显示消息

## 输入功能
- [ ] RegisterKeyCallback 工作
- [ ] InputHook::IsKeyDown 正确

## 依赖功能
- [ ] AddDependency 工作
- [ ] 依赖顺序正确

## 性能
- [ ] 无明显性能下降
- [ ] 帧率保持稳定
```

**自动化测试脚本** (`tests/regression/test_v03_compat.py`):
```python
def test_all_v03_mods():
    """测试所有v0.3模组"""
    v03_mods = [
        "BMLMod",
        "NewBallTypeMod",
        "CommunityModA",
        "CommunityModB",
    ]
    
    for mod in v03_mods:
        with subtests.test(mod=mod):
            assert test_mod_loads(mod)
            assert test_mod_functions(mod)
            assert test_mod_performance(mod)

def test_mod_loads(mod_name):
    """测试模组加载"""
    proc = start_game()
    time.sleep(5)
    logs = get_debug_output()
    proc.terminate()
    
    return f"Loaded v0.3 module: {mod_name}" in logs

def test_mod_functions(mod_name):
    """测试模组功能"""
    # 特定模组的功能测试
    if mod_name == "BMLMod":
        return test_bmlmod_commands()
    # ...
    return True

def test_mod_performance(mod_name):
    """测试性能影响"""
    # 测量帧率
    baseline_fps = measure_fps_without_mod(mod_name)
    with_mod_fps = measure_fps_with_mod(mod_name)
    
    # 性能下降应<5%
    return (baseline_fps - with_mod_fps) / baseline_fps < 0.05
```

**工作量**: 1天

---

### 7.3 Phase 4检查点

**完成标准**:
```
✅ 单元测试覆盖率 >80%
✅ 所有单元测试通过
✅ 集成测试100%通过
✅ 性能满足目标
✅ 回归测试通过
✅ 无内存泄漏（Valgrind/Dr.Memory）
✅ 无数据竞争（ThreadSanitizer）
```

**交付物**:
- [ ] 完整的单元测试套件
- [ ] 集成测试套件
- [ ] 性能基准测试结果
- [ ] 兼容性测试报告
- [ ] 已知问题列表（如有）
- [ ] Phase 4测试报告

**质量指标**:
```
测试覆盖率: 82%
单元测试通过率: 100% (128/128)
集成测试通过率: 100% (35/35)
回归测试通过率: 100% (5/5模组)
性能达标率: 100% (3/3指标)
内存泄漏: 0
崩溃数: 0
```

**决策点**:
- ✅ **Go**: 质量达标，继续Phase 5
- 🔄 **Review**: 部分问题，修复后继续
- ❌ **No-Go**: 严重问题，回退修复

---

## 📚 第八部分：Phase 5 - 文档与发布（Week 5-6）

### 8.1 Phase目标

**总体目标**: 准备发布v0.4.0

**具体目标**:
1. 更新所有文档
2. 编写迁移指南
3. 准备发布说明
4. Beta测试
5. 正式发布

**交付标准**:
```
✓ API文档完整更新
✓ 用户指南修订
✓ 迁移指南可执行
✓ CHANGELOG完整
✓ 发布包准备就绪
✓ 社区通知发布
```

### 8.2 任务分解

#### Task 5.1: 文档更新（2天）

**1. API文档更新** (`docs/api/`):
```markdown
# docs/api/imc-bus.md

## IMC总线API

### 发布事件

```cpp
BML_Result bmlImcPublish(
    const char* topic,
    const void* payload,
    size_t payload_size,
    BML_ImcDeliveryMode mode
);
```

**参数**:
- `topic`: 事件主题（如"BML/Input/KeyDown"）
- `payload`: 事件数据指针
- `payload_size`: 数据大小（字节）
- `mode`: 投递模式
  - `BML_IMC_RELIABLE`: 保证投递
  - `BML_IMC_BEST_EFFORT`: 允许丢失

**返回值**:
- `BML_RESULT_OK`: 成功
- `BML_RESULT_INVALID_ARGUMENT`: 参数错误
- `BML_RESULT_FAIL`: 发布失败

**示例**:
```cpp
struct KeyEvent {
    uint32_t key;
    uint32_t timestamp;
};

KeyEvent event = { VK_SPACE, 12345 };
bmlImcPublish("BML/Input/KeyDown", &event, sizeof(event), BML_IMC_RELIABLE);
```

### 订阅事件

（详细文档...）

---

# docs/api/module-lifecycle.md

## 模组生命周期

### v0.4模组入口点

```cpp
BML_Result BML_ENTRYPOINT(
    BML_ModPhase phase,
    BML_Context context,
    PFN_BML_GetProcAddress get_proc
);
```

**阶段**:
1. `BML_ATTACH`: 模组加载，初始化
2. `BML_DETACH`: 模组卸载，清理

**示例**:
```cpp
static BML_Subscription g_KeySub = nullptr;

void OnKeyDown(const void* payload, size_t size, 
               const BML_ImcMessageInfo* info, void* user_data) {
    // 处理按键
}

BML_Result BML_ENTRYPOINT(BML_ModPhase phase, BML_Context ctx, 
                          PFN_BML_GetProcAddress get_proc) {
    if (phase == BML_ATTACH) {
        // 加载API
        if (!BML_LoadAPI(get_proc)) {
            return BML_RESULT_FAIL;
        }
        
        // 订阅事件
        bmlImcSubscribe("BML/Input/KeyDown", OnKeyDown, nullptr, &g_KeySub);
        
        return BML_RESULT_OK;
    }
    else if (phase == BML_DETACH) {
        // 取消订阅
        bmlImcUnsubscribe(g_KeySub);
        return BML_RESULT_OK;
    }
    return BML_RESULT_OK;
}
```
```

**2. 迁移指南** (`docs/migration-guide.md`):
```markdown
# v0.3 → v0.4 迁移指南

## 概述

本指南帮助模组开发者将v0.3模组迁移到v0.4。

## 向后兼容性

**好消息**: v0.3模组无需修改即可在v0.4运行！

v0.4完全兼容v0.3 API，现有模组可以继续使用旧接口。

## 迁移收益

迁移到v0.4可获得：
- ✅ 自动依赖管理
- ✅ 热重载支持
- ✅ 性能分析工具
- ✅ 更好的错误诊断
- ✅ 模组间通信（IMC）

## 迁移步骤

### 步骤1: 创建mod.toml

在模组目录创建`mod.toml`文件：

```toml
[module]
id = "com.author.mymod"
name = "My Awesome Mod"
version = "1.0.0"
authors = ["Your Name"]
description = "A great mod for Ballance"

[dependencies]
bml = "^0.4.0"
# 其他依赖...
```

### 步骤2: 修改入口点

**v0.3入口点**:
```cpp
extern "C" __declspec(dllexport) IMod* BMLEntry(IBML* bml) {
    return new MyMod(bml);
}
```

**v0.4入口点**:
```cpp
BML_Result BML_ENTRYPOINT(BML_ModPhase phase, BML_Context ctx,
                          PFN_BML_GetProcAddress get_proc) {
    if (phase == BML_ATTACH) {
        // 初始化
        return BML_RESULT_OK;
    }
    else if (phase == BML_DETACH) {
        // 清理
        return BML_RESULT_OK;
    }
    return BML_RESULT_OK;
}
```

### 步骤3: 替换回调为事件订阅

**v0.3回调**:
```cpp
void MyMod::OnProcess() {
    // 每帧逻辑
}

void MyMod::OnRender(CKRenderContext* dev) {
    // 渲染
}
```

**v0.4事件订阅**:
```cpp
static BML_Subscription g_ProcessSub = nullptr;
static BML_Subscription g_RenderSub = nullptr;

void OnProcess(const void* payload, size_t size, 
               const BML_ImcMessageInfo* info, void* user_data) {
    // 每帧逻辑
}

void OnRender(const void* payload, size_t size,
              const BML_ImcMessageInfo* info, void* user_data) {
    auto* event = (const BML::Events::RenderEvent*)payload;
    CKRenderContext* dev = (CKRenderContext*)event->render_context;
    // 渲染
}

// 在BML_ATTACH阶段订阅
bmlImcSubscribe("BML/Game/Process", OnProcess, nullptr, &g_ProcessSub);
bmlImcSubscribe("BML/Game/PostRender", OnRender, nullptr, &g_RenderSub);
```

### 步骤4: 更新输入处理

**v0.3**:
```cpp
m_InputHook->RegisterKeyCallback(VK_SPACE, [](InputHook* hook) {
    // 处理空格键
});
```

**v0.4**:
```cpp
void OnKeyDown(const void* payload, size_t size,
               const BML_ImcMessageInfo* info, void* user_data) {
    auto* event = (const BML::Events::KeyEvent*)payload;
    if (event->key == VK_SPACE) {
        // 处理空格键
    }
}

bmlImcSubscribe("BML/Input/KeyDown", OnKeyDown, nullptr, &g_KeySub);
```

## 完整示例

见 `examples/MinimalMod/` 目录。

## 常见问题

### Q: 必须迁移吗？
A: 不必须。v0.3模组继续工作。

### Q: 可以混合使用吗？
A: 可以。同一项目可以同时有v0.3和v0.4模组。

### Q: 性能有差异吗？
A: v0.4略有开销（<1%），但可忽略。
```

**3. CHANGELOG更新** (`CHANGELOG.md`):
```markdown
# Changelog

## [0.4.0] - 2025-12-01

### Added

#### Core微内核
- 全新的模组加载系统，支持依赖解析和版本约束
- IMC (Inter-Mod Communication) 事件总线
- 热重载支持
- 性能分析工具（Profiling/Tracing）
- Per-mod配置和日志管理

#### 事件系统
- 输入事件：KeyDown, KeyUp, MouseMove, MouseButton, MouseWheel
- 渲染事件：PreRender, PostRender
- 生命周期事件：Init, Shutdown, Load, Start, Reset, Pause, Process

#### API增强
- 116个新API函数
- 线程安全的API注册表
- 统一的错误处理机制
- 内存管理器和同步原语

### Changed
- 模组加载流程改进，支持v0.3和v0.4共存
- 性能优化，IMC事件开销<0.2ms/frame

### Fixed
- 修复v0.3模组加载失败导致游戏崩溃的问题
- 修复内存泄漏问题

### Deprecated
- v0.3 API将在v0.5.0弃用（但继续支持）

### Security
- 增强的错误隔离机制
- 模组沙盒改进

## [0.3.0] - 2024-XX-XX
（历史版本...）
```

**工作量**: 2天

---

#### Task 5.2: 发布准备（1天）

**1. 版本号更新**:
```cpp
// include/bml_version.h
#define BML_VERSION_MAJOR 0
#define BML_VERSION_MINOR 4
#define BML_VERSION_PATCH 0
#define BML_VERSION_STRING "0.4.0"
```

**2. 发布说明** (`RELEASE_NOTES.md`):
```markdown
# BML v0.4.0 Release Notes

**发布日期**: 2025-12-01

## 重大更新

### 🚀 Core微内核

BML v0.4.0引入全新的微内核架构，提供：
- 自动依赖解析和版本管理
- 模组间通信（IMC）总线
- 热重载支持
- 性能分析工具

### 🔌 完全向后兼容

v0.3模组无需修改即可运行！新旧模组可以共存。

### 📊 性能改进

- 模组加载速度提升20%
- IMC事件开销<0.2ms/frame
- 内存占用减少5MB

## 安装

### 方式1: 自动安装（推荐）
1. 下载 `BML_v0.4.0_Installer.exe`
2. 运行安装程序
3. 选择Ballance安装目录

### 方式2: 手动安装
1. 下载 `BML_v0.4.0.zip`
2. 解压到Ballance目录
3. 确保以下文件存在：
   - `BML.dll`
   - `BMLPlus.dll`
   - `ModLoader/` 目录

## 升级指南

从v0.3升级：
1. 备份现有模组
2. 安装v0.4.0
3. 现有模组自动兼容

## 已知问题

- 热重载功能为实验性，可能不稳定
- 部分v0.3模组的UI可能显示异常（正在修复）

## 反馈

- GitHub Issues: https://github.com/doyaGu/BallanceModLoaderPlus/issues
- 论坛: https://ballance.example.com/forum

## 致谢

感谢所有贡献者和测试者！
```

**3. 构建发布包**:
```powershell
# 构建脚本 build_release.ps1
param(
    [string]$Version = "0.4.0"
)

# 清理
Remove-Item -Recurse -Force release -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path release

# 编译Release版本
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# 复制文件
$ReleaseDir = "release/BML_v$Version"
New-Item -ItemType Directory -Path $ReleaseDir

Copy-Item build/bin/Release/BML.dll $ReleaseDir/
Copy-Item build/bin/Release/BMLPlus.dll $ReleaseDir/
Copy-Item -Recurse ModLoader/ $ReleaseDir/ModLoader/
Copy-Item README.md $ReleaseDir/
Copy-Item LICENSE $ReleaseDir/
Copy-Item CHANGELOG.md $ReleaseDir/

# 打包
Compress-Archive -Path $ReleaseDir -DestinationPath "release/BML_v$Version.zip"

Write-Host "Release package created: release/BML_v$Version.zip"
```

**工作量**: 1天

---

#### Task 5.3: Beta测试（3天）

**目标**: 社区测试验证

**Beta发布流程**:

1. **准备Beta版本**:
```markdown
# Beta版本标记
v0.4.0-beta1, v0.4.0-beta2, ...
```

2. **Beta测试计划**:
```markdown
# Beta测试计划

## 测试周期
- Beta1: 2025-11-25 ~ 2025-11-27 (3天)
- Beta2: 2025-11-28 ~ 2025-11-30 (3天，如需要）

## 测试目标
- 验证基本功能
- 收集性能反馈
- 发现兼容性问题
- 测试文档清晰度

## 测试范围
1. 基础功能测试
   - [ ] 游戏正常启动
   - [ ] v0.3模组兼容
   - [ ] v0.4模组加载
   - [ ] 混合模组场景

2. 性能测试
   - [ ] 帧率稳定
   - [ ] 加载时间可接受
   - [ ] 内存占用正常

3. 稳定性测试
   - [ ] 长时间运行（2小时+）
   - [ ] 反复重启
   - [ ] 模组加载/卸载

4. 兼容性测试
   - [ ] 主流v0.3模组（列举5-10个）
   - [ ] 不同Windows版本（10/11）
   - [ ] 不同游戏版本

## 测试人员
- 核心团队: 2人
- 社区志愿者: 5-10人

## 反馈渠道
- GitHub Issues (标签: beta-feedback)
- Discord #beta-testing频道
- 论坛Beta专区

## 接受标准
- P0问题: 0个
- P1问题: <3个
- 崩溃率: 0
- 测试覆盖: >80%场景
```

3. **Beta反馈收集**:
```markdown
# Beta反馈模板

**BML版本**: v0.4.0-beta1
**操作系统**: Windows 11
**游戏版本**: Ballance 1.0

## 测试场景
描述你的测试场景...

## 期望行为
描述你期望发生什么...

## 实际行为
描述实际发生了什么...

## 重现步骤
1. 启动游戏
2. 加载模组X
3. ...

## 日志/截图
粘贴相关日志或上传截图...

## 优先级
- [ ] P0 - 阻塞发布
- [ ] P1 - 严重问题
- [ ] P2 - 一般问题
- [ ] P3 - 改进建议
```

**工作量**: 3天（包含问题修复）

---

#### Task 5.4: 正式发布（1天）

**发布检查清单**:
```markdown
# 发布检查清单

## 代码
- [ ] 所有测试通过
- [ ] 代码审查完成
- [ ] 无已知P0/P1问题
- [ ] 版本号正确
- [ ] Git标签创建（v0.4.0）

## 文档
- [ ] README更新
- [ ] CHANGELOG完整
- [ ] API文档完整
- [ ] 迁移指南完整
- [ ] 发布说明完整

## 构建产物
- [ ] BML_v0.4.0.zip生成
- [ ] BML_v0.4.0_Installer.exe生成
- [ ] SHA256校验和生成
- [ ] 病毒扫描通过

## 发布渠道
- [ ] GitHub Release创建
- [ ] 官网更新
- [ ] 论坛公告发布
- [ ] Discord公告
- [ ] 中文/英文社区通知

## 后续支持
- [ ] Issues标签准备（v0.4.x）
- [ ] 支持文档准备
- [ ] FAQ更新
```

**GitHub Release创建**:
```markdown
# v0.4.0 - Core微内核架构

**发布日期**: 2025-12-01

## 🎉 重大更新

BML v0.4.0是一个里程碑版本，引入全新的微内核架构！

### 主要特性
- ✅ 自动依赖管理和版本控制
- ✅ 模组间通信（IMC）事件总线
- ✅ 热重载支持（实验性）
- ✅ 性能分析工具
- ✅ 完全向后兼容v0.3

### 下载

- [BML_v0.4.0.zip](链接) (4.2 MB)
- [BML_v0.4.0_Installer.exe](链接) (6.5 MB)

**SHA256校验和**:
```
BML_v0.4.0.zip:           abc123...
BML_v0.4.0_Installer.exe: def456...
```

### 文档
- [安装指南](链接)
- [迁移指南](链接)
- [API文档](链接)

### 升级
从v0.3升级？现有模组自动兼容！详见[迁移指南](链接)。

### 反馈
发现问题？请[提交Issue](链接)。

---

**完整更新日志**: [CHANGELOG.md](链接)
```

**社区公告模板**:
```markdown
# 🎉 BML v0.4.0 正式发布！

经过6周的开发和测试，我们很高兴宣布BML v0.4.0正式发布！

## 🚀 重大更新

v0.4.0是一个全新的里程碑，带来了Core微内核架构：

### 对玩家
- 更稳定的模组加载
- 更好的性能（帧率提升）
- 更清晰的错误提示

### 对模组开发者
- 自动依赖管理
- 模组间通信API
- 热重载开发体验
- 完善的开发文档

## 📥 下载

- [GitHub Release](链接)
- [官网下载](链接)

## 🔄 升级

从v0.3升级非常简单，现有模组自动兼容！详见[升级指南](链接)。

## 📚 文档

- [用户手册](链接)
- [开发者指南](链接)
- [API参考](链接)

## 🙏 致谢

感谢所有贡献者、测试者和社区支持！

特别感谢：
- @contributor1 - Core架构设计
- @contributor2 - IMC总线实现
- @tester1, @tester2 - Beta测试

## 💬 反馈

使用中遇到问题？
- [GitHub Issues](链接)
- [Discord](链接)
- [论坛](链接)

祝大家使用愉快！🎈
```

**工作量**: 1天

---

### 8.3 Phase 5检查点

**完成标准**:
```
✅ 所有文档更新完成
✅ 发布包构建成功
✅ Beta测试完成，无P0/P1问题
✅ GitHub Release创建
✅ 社区公告发布
✅ 后续支持准备就绪
```

**交付物**:
- [ ] 更新的文档（10+文件）
- [ ] 迁移指南
- [ ] CHANGELOG.md
- [ ] RELEASE_NOTES.md
- [ ] 发布包（zip + installer）
- [ ] GitHub Release
- [ ] 社区公告（中英文）

---

## 📎 附录

### A. 检查清单汇总

#### A.1 MVP检查清单
```
✅ CMakeLists链接BML
✅ DllMain调用bmlAttach/bmlDetach
✅ OnCKPlay调用bmlLoadModules
✅ InputHook发布KeyDown事件
✅ PostProcess调用ImcBus::Pump
✅ BML_Input接收事件
✅ 编译链接成功
✅ 游戏启动正常
```

#### A.2 Phase 1检查清单
```
✅ CoreBridge类实现
✅ 错误处理机制
✅ 单元测试覆盖
✅ 集成测试框架
✅ 代码审查通过
✅ 文档初稿
```

#### A.3 Phase 2检查清单
```
✅ 5种输入事件发布
✅ 2种渲染事件发布
✅ 8种生命周期事件
✅ 事件协议文档
✅ BML_Input/Render验证
✅ 性能测试通过
```

#### A.4 Phase 3检查清单
```
✅ ModuleAdapter实现
✅ 双路径加载逻辑
✅ 依赖关系统一
✅ 错误隔离机制
✅ 混合模组测试
✅ 兼容性验证
```

#### A.5 Phase 4检查清单
```
✅ 单元测试覆盖率>80%
✅ 集成测试100%通过
✅ 性能基准达标
✅ 回归测试通过
✅ 无内存泄漏
✅ 无崩溃
```

#### A.6 Phase 5检查清单
```
✅ 文档更新完成
✅ 迁移指南编写
✅ CHANGELOG更新
✅ Beta测试完成
✅ 发布包构建
✅ GitHub Release
✅ 社区公告
```

---

### B. 回滚计划

#### B.1 何时回滚

**触发条件**:
- P0问题无法在1天内修复
- 严重性能下降（>10%）
- 数据损坏风险
- 安全漏洞

#### B.2 回滚步骤

1. **代码回滚**:
```bash
git checkout main
git revert <merge-commit>
git push origin main
```

2. **构建系统回滚**:
```bash
git checkout v0.3.0 -- CMakeLists.txt src/CMakeLists.txt
```

3. **通知用户**:
```markdown
# 紧急通知：v0.4.0回滚

由于发现严重问题，我们决定暂时回滚v0.4.0发布。

## 行动
- 已发布用户：请回退到v0.3.0
- 下载链接：[v0.3.0](链接)

## 问题说明
描述问题...

## 计划
我们将在修复问题后重新发布v0.4.0。

预计时间：X周后

抱歉给大家带来不便。
```

#### B.3 事后分析

- 召开回顾会议
- 分析失败原因
- 更新流程
- 加强测试

---

### C. 资源参考

#### C.1 相关文档
- [BML_v0.4.0_Design.md](BML_v0.4.0_Design.md) - 架构设计
- [INTEGRATION_ANALYSIS.md](INTEGRATION_ANALYSIS.md) - 集成分析
- [CLAUDE.md](CLAUDE.md) - 开发指南
- [MOD_DEVELOPMENT_GUIDE.md](docs/developer-guide/) - 模组开发

#### C.2 工具链
- CMake >= 3.20
- Visual Studio 2019/2022
- GoogleTest
- Python 3.8+
- Git

#### C.3 外部依赖
- Virtools SDK
- MinHook
- ImGui
- toml++
- oniguruma

#### C.4 联系方式
- 项目负责人：[@doyaGu](https://github.com/doyaGu)
- GitHub：https://github.com/doyaGu/BallanceModLoaderPlus
- Issues：https://github.com/doyaGu/BallanceModLoaderPlus/issues

---

### D. 术语表

| 术语 | 定义 |
|------|------|
| Core微内核 | BML.dll，提供基础API的轻量级核心 |
| Legacy主DLL | BMLPlus.dll，v0.3兼容层 |
| IMC | Inter-Mod Communication，模组间通信总线 |
| MVP | Minimum Viable Product，最小可行产品 |
| CoreBridge | Core与Legacy的桥接抽象层 |
| ModuleAdapter | v0.4模组到v0.3接口的适配器 |
| P0/P1/P2/P3 | 优先级分级（P0最高） |
| ABI | Application Binary Interface，应用程序二进制接口 |

---

### E. 版本历史

| 版本 | 日期 | 作者 | 变更 |
|------|------|------|------|
| v1.0 | 2025-11-23 | AI System | 初始版本 |

---

## 🎯 总结

### 项目概览

**总工期**: 6周（30人日）
**总代码行数**: ~10,000行新增/修改
**测试用例**: 150+
**文档页数**: 50+

### 关键里程碑

```
Day 0-2:   MVP验证 (BML_Input接收事件)
Week 1:    基础连接 (CoreBridge)
Week 2:    事件桥接 (15种事件)
Week 3:    模组统一 (双路径加载)
Week 4:    测试优化 (覆盖率>80%)
Week 5-6:  文档发布 (v0.4.0 Release)
```

### 风险管理

- **总体风险**: 🟡 中等（可控）
- **技术风险**: 已识别6项，全部有应对措施
- **项目风险**: 保守估算，20% buffer
- **回滚计划**: 已准备，可快速响应

### 成功标准

**技术指标**:
- ✅ 单元测试覆盖率 >80%
- ✅ 所有测试通过率 100%
- ✅ 性能目标达成 (<0.2ms/frame)
- ✅ v0.3兼容性 100%

**交付标准**:
- ✅ 功能完整（P0/P1全部完成）
- ✅ 文档完善（用户+开发者）
- ✅ 发布包就绪（zip + installer）
- ✅ 社区准备（公告+支持）

### 下一步行动

**立即行动**:
1. ✅ 审查本计划
2. ✅ 团队讨论与确认
3. ✅ 创建`integration-phase5`分支
4. ✅ 开始MVP实施（<50行代码）

**本周目标**:
- Day 1-2: 完成MVP，验证技术可行性
- Day 3-5: 完成Phase 1（CoreBridge）

**本月目标**:
- Week 1-4: 完成Phase 1-4（核心功能+测试）
- Week 5-6: 完成Phase 5（文档+发布）

---

**计划版本**: v1.0  
**创建日期**: 2025-11-23  
**状态**: 待审批  
**下次审查**: MVP完成后（Day 2）

---

**结束语**

这是一个雄心勃勃但可行的计划。通过渐进式集成、持续测试和充分准备，我们有信心在6周内成功交付BML v0.4.0。

让我们开始吧！🚀
