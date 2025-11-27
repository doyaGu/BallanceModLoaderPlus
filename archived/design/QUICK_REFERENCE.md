# BML v0.4.0 快速参考：系统集成

## 🔗 关键集成点

### 1. 生命周期管理

```
游戏启动 (DLL_PROCESS_ATTACH)
  ↓
bmlAttach()  ← BML.cpp:DllMain
  ├─ 模组发现（扫描Mods/*/mod.toml）
  ├─ 依赖解析（拓扑排序）
  └─ 注册Core API（116个函数）
  ↓
CKContext创建
  ↓
ModManager::OnCKPlay()
  ├─ ModContext::LoadMods()  ← v0.3旧模组
  ├─ bmlLoadModules()         ← v0.4新模组
  │   └─ 调用 BML_ModEntrypoint(ATTACH, ...)
  ├─ bmlGetProcAddress("bmlImcPublish")
  ├─ bmlGetProcAddress("bmlImcPump")
  └─ ImcPublish("BML/Game/Init")
  ↓
游戏循环
  ↓
ModManager::PostProcess() (每帧)
  ├─ ImcPump(100)              ← 驱动消息分发
  ├─ ModContext::OnProcess()   ← v0.3回调
  ├─ ImcPublish("BML/Game/Process", deltaTime)
  └─ InputHook::Process()
      └─ ImcPublish("BML/Input/*")
  ↓
ModManager::OnPostRender()
  ├─ ImcPublish("BML/Game/PreRender")
  ├─ ModContext::OnRender()    ← v0.3回调
  └─ ImcPublish("BML/Game/PostRender")
  ↓
游戏退出 (DLL_PROCESS_DETACH)
  ↓
bmlDetach()
  └─ 卸载所有v0.4模组
```

---

## 📡 IMC事件列表

### 游戏生命周期
| Topic | 触发点 | Payload | 订阅者 |
|-------|--------|---------|--------|
| `BML/Game/Init` | OnCKPlay (首次) | 无 | 所有模组 |
| `BML/Game/Process` | PostProcess (每帧) | `float` deltaTime | 所有模组 |
| `BML/Game/PreRender` | OnPostRender (前) | 无 | BML_Render |
| `BML/Game/PostRender` | OnPostRender (后) | 无 | BML_Render |

### 输入事件
| Topic | 触发点 | Payload | 订阅者 |
|-------|--------|---------|--------|
| `BML/Input/KeyDown` | InputHook::Process | `BML_KeyDownEvent` | BML_Input |
| `BML/Input/KeyUp` | InputHook::Process | `BML_KeyUpEvent` | BML_Input |
| `BML/Input/MouseButton` | InputHook::Process | `BML_MouseButtonEvent` | BML_Input |
| `BML/Input/MouseMove` | InputHook::Process | `BML_MouseMoveEvent` | BML_Input |

---

## 📦 事件结构体

### BML_KeyDownEvent
```cpp
struct BML_KeyDownEvent {
    uint32_t key_code;      // Virtual key code (VK_*)
    uint32_t scan_code;     // Hardware scan code (TODO)
    uint32_t timestamp;     // Milliseconds (TODO)
    bool repeat;            // Auto-repeat flag
};
```

### BML_KeyUpEvent
```cpp
struct BML_KeyUpEvent {
    uint32_t key_code;
    uint32_t scan_code;
    uint32_t timestamp;
};
```

### BML_MouseButtonEvent
```cpp
struct BML_MouseButtonEvent {
    uint32_t button;        // 0=left, 1=right, 2=middle
    bool down;              // true=pressed, false=released
    uint32_t timestamp;
};
```

### BML_MouseMoveEvent
```cpp
struct BML_MouseMoveEvent {
    float x;                // Absolute X position
    float y;                // Absolute Y position
    float rel_x;            // Relative X delta
    float rel_y;            // Relative Y delta
    bool absolute;          // Position type
};
```

---

## 🔌 API加载流程

### v0.4模组标准流程

```cpp
// 1. 定义 bml_module.h (提供宏定义)
#include "bml_module.h"

// 2. 实现Loader (自动生成函数指针)
#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"

// 3. 实现入口点
BML_MODULE_ENTRY BML_Result BML_ModEntrypoint(
    BML_ModEntrypointCommand cmd, 
    void* data
) {
    switch (cmd) {
    case BML_MOD_ENTRYPOINT_ATTACH: {
        auto* args = static_cast<const BML_ModAttachArgs*>(data);
        
        // a. 加载所有API函数指针
        BML_Result result = bmlLoadAPI(args->get_proc);
        if (result != BML_RESULT_OK) return result;
        
        // b. 订阅IMC事件
        BML_Subscription sub;
        result = bmlImcSubscribe("BML/Input/KeyDown", 
                                  OnKeyDown, nullptr, &sub);
        
        // c. 注册扩展 (可选)
        result = bmlExtensionRegister("MyExtension", 
                                       1, 0, &myApi, sizeof(myApi));
        
        return BML_RESULT_OK;
    }
    
    case BML_MOD_ENTRYPOINT_DETACH: {
        // 清理：关闭订阅、注销扩展
        bmlImcSubscriptionClose(sub);
        bmlImcSubscriptionRelease(sub);
        bmlExtensionUnregister("MyExtension");
        bmlUnloadAPI();
        return BML_RESULT_OK;
    }
    
    default:
        return BML_RESULT_INVALID_ARGUMENT;
    }
}
```

---

## 🧪 验证清单

### 编译时检查 ✅
- [x] BML.dll 链接成功
- [x] BMLPlus.dll 链接BML.lib成功
- [x] BML_Input.dll/BML_Render.dll 编译成功
- [x] 无链接错误（__imp__bml*符号已解析）
- [x] mod.toml 格式正确

### 运行时检查 ⚠️ (需游戏环境)
- [ ] bmlAttach() 成功发现模组
- [ ] bmlLoadModules() 成功加载DLL
- [ ] BML_ModEntrypoint(ATTACH) 被调用
- [ ] IMC事件正确分发到订阅者
- [ ] v0.3旧模组仍能正常运行
- [ ] 日志输出到 logs/<mod-id>.log

### 性能检查 ⏳
- [ ] ImcBus::Pump() < 1ms/frame
- [ ] InputHook::Process() < 0.5ms/frame
- [ ] 模组加载时间 < 100ms

---

## 🐛 调试技巧

### 1. 查看模组加载日志
```powershell
# BMLPlus输出到DebugView
# 搜索关键字：
# - "bmlAttach"
# - "bmlLoadModules"
# - "BML_ModEntrypoint"
```

### 2. 验证IMC事件
```cpp
// 在订阅回调中添加日志
void OnKeyDown(const char* topic, const void* data, 
                size_t size, const BML_ImcMessageInfo* info, 
                void* user_data) {
    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, 
           "MyMod", "KeyDown event received!");
}
```

### 3. 检查API加载
```cpp
BML_Result result = bmlLoadAPI(args->get_proc);
if (result != BML_RESULT_OK) {
    // 哪个API加载失败了？
    auto* diagnostics = bmlGetBootstrapDiagnostics();
    // 检查 diagnostics->warnings/errors
}
```

---

## 📚 参考文档

- **设计文档**: `BML_v0.4.0_Design.md`
- **集成状态**: `INTEGRATION_STATUS.md`
- **API参考**: `docs/api/`
- **开发指南**: `docs/developer-guide/`
- **示例模组**: `modules/BML_Input/`, `modules/BML_Render/`

---

**最后更新**: 2025-11-24  
**版本**: v0.4.0-alpha (90%完成)
