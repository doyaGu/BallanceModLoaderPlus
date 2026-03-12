/**
 * @file RenderHook.cpp
 * @brief CK2 Render Engine Hook Implementation for BML_Render Module
 * 
 * Self-contained render hook that provides:
 * - CKRenderContext::Render VTable hook
 * - CKRenderContext::UpdateProjection member function hook
 * - Pre/Post render IMC event publishing
 */

#include "RenderHook.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <MinHook.h>

#include "bml_loader.h"
#include "bml_topics.h"

namespace BML_Render {

//-----------------------------------------------------------------------------
// Utility Functions
//-----------------------------------------------------------------------------

template<typename T>
static T ForceReinterpretCast(void* base, size_t offset) {
    void* p = static_cast<char*>(base) + offset;
    return *reinterpret_cast<T*>(&p);
}

static void* GetModuleBaseAddress(const char* moduleName) {
    HMODULE hModule = GetModuleHandleA(moduleName);
    return hModule ? reinterpret_cast<void*>(hModule) : nullptr;
}

static uint32_t UnprotectRegion(void* region, size_t size) {
    DWORD oldProtect = 0;
    VirtualProtect(region, size, PAGE_EXECUTE_READWRITE, &oldProtect);
    return oldProtect;
}

static void ProtectRegion(void* region, size_t size, uint32_t protection) {
    DWORD oldProtect;
    VirtualProtect(region, size, protection, &oldProtect);
}

template<typename T>
static void LoadVTable(void** vtablePtr, T& dest) {
    memcpy(&dest, *vtablePtr, sizeof(T));
}

template<typename T>
static void SaveVTable(void** vtablePtr, const T& src) {
    uint32_t oldProtect = UnprotectRegion(*vtablePtr, sizeof(T));
    memcpy(*vtablePtr, &src, sizeof(T));
    ProtectRegion(*vtablePtr, sizeof(T), oldProtect);
}

template<typename T, typename F>
static void HookVirtualMethod(T** instance, F hookFunc, size_t vtableIndex) {
    void** vtable = *reinterpret_cast<void***>(instance);
    uint32_t oldProtect = UnprotectRegion(&vtable[vtableIndex], sizeof(void*));
    vtable[vtableIndex] = *reinterpret_cast<void**>(&hookFunc);
    ProtectRegion(&vtable[vtableIndex], sizeof(void*), oldProtect);
}

//-----------------------------------------------------------------------------
// Static State
//-----------------------------------------------------------------------------

bool CP_HOOK_CLASS_NAME(CKRenderContext)::s_DisableRender = false;
bool CP_HOOK_CLASS_NAME(CKRenderContext)::s_EnableWidescreenFix = false;
CKRenderContextVTable<CKRenderContext> CP_HOOK_CLASS_NAME(CKRenderContext)::s_VTable = {};

// UpdateProjection method pointers for MinHook
CP_DEFINE_METHOD_PTRS(CP_HOOK_CLASS_NAME(CKRenderContext), UpdateProjection);

static bool s_Initialized = false;

// IMC topic IDs
static BML_TopicId s_TopicPreRender = 0;
static BML_TopicId s_TopicPostRender = 0;

//-----------------------------------------------------------------------------
// Hook Implementations
//-----------------------------------------------------------------------------

CKERROR CP_HOOK_CLASS_NAME(CKRenderContext)::RenderHook(CK_RENDER_FLAGS Flags) {
    if (s_DisableRender)
        return CK_OK;

    // Publish pre-render event
    if (s_TopicPreRender != 0) {
        bmlImcPublish(s_TopicPreRender, &Flags, sizeof(Flags));
    }

    // Call original Render method via saved VTable
    auto originalRender = s_VTable.Render;
    CKERROR result = CP_CALL_METHOD_PTR(this, originalRender, Flags);

    // Publish post-render event
    if (s_TopicPostRender != 0) {
        bmlImcPublish(s_TopicPostRender, &Flags, sizeof(Flags));
    }

    return result;
}

CKBOOL CP_HOOK_CLASS_NAME(CKRenderContext)::UpdateProjection(CKBOOL force) {
    if (!force && m_ProjectionUpdated)
        return TRUE;
    if (!m_RasterizerContext)
        return FALSE;

    const auto aspect = (float)((double)m_ViewportData.ViewWidth / (double)m_ViewportData.ViewHeight);
    if (m_Perspective) {
        // Apply widescreen fix if enabled
        const float fov = s_EnableWidescreenFix ? 
            atan2f(tanf(m_Fov * 0.5f) * 0.75f * aspect, 1.0f) * 2.0f : m_Fov;
        m_ProjectionMatrix.Perspective(fov, aspect, m_NearPlane, m_FarPlane);
    } else {
        m_ProjectionMatrix.Orthographic(m_Zoom, aspect, m_NearPlane, m_FarPlane);
    }

    m_RasterizerContext->SetTransformMatrix(VXMATRIX_PROJECTION, m_ProjectionMatrix);
    m_RasterizerContext->SetViewport(&m_ViewportData);
    m_ProjectionUpdated = TRUE;

    VxRect rect(0.0f, 0.0f, (float)m_Settings.m_Rect.right, (float)m_Settings.m_Rect.bottom);
    auto *background = Get2dRoot(TRUE);
    background->SetRect(rect);
    auto *foreground = Get2dRoot(FALSE);
    foreground->SetRect(rect);

    return TRUE;
}

//-----------------------------------------------------------------------------
// Hook Install/Uninstall
//-----------------------------------------------------------------------------

bool CP_HOOK_CLASS_NAME(CKRenderContext)::Hook(void *base) {
    if (!base)
        return false;

    // Get VTable pointer at offset 0x86AF8 in CK2_3D.dll
    auto *table = ForceReinterpretCast<CKRenderContextVTable<CKRenderContext>*>(base, 0x86AF8);
    
    // Save original VTable
    LoadVTable<CKRenderContextVTable<CKRenderContext>>(reinterpret_cast<void**>(&table), s_VTable);

    // Hook Render method via VTable
    constexpr size_t RenderVTableIndex = offsetof(CKRenderContextVTable<CKRenderContext>, Render) / sizeof(void*);
    HookVirtualMethod(&table, &CP_HOOK_CLASS_NAME(CKRenderContext)::RenderHook, RenderVTableIndex);

    // Hook UpdateProjection via MinHook (non-virtual member function at offset 0x6C68D)
    CP_FUNC_TARGET_PTR_NAME(UpdateProjection) = ForceReinterpretCast<CP_FUNC_TYPE_NAME(UpdateProjection)>(base, 0x6C68D);
    
    if (MH_CreateHook(*reinterpret_cast<LPVOID*>(&CP_FUNC_TARGET_PTR_NAME(UpdateProjection)),
                      *reinterpret_cast<LPVOID*>(&CP_FUNC_PTR_NAME(UpdateProjection)),
                      reinterpret_cast<LPVOID*>(&CP_FUNC_ORIG_PTR_NAME(UpdateProjection))) != MH_OK ||
        MH_EnableHook(*reinterpret_cast<LPVOID*>(&CP_FUNC_TARGET_PTR_NAME(UpdateProjection))) != MH_OK) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, "BML_Render",
               "Failed to hook UpdateProjection - widescreen fix may not work");
        // Non-fatal, continue with Render hook only
    }

    return true;
}

bool CP_HOOK_CLASS_NAME(CKRenderContext)::Unhook(void *base) {
    if (!base)
        return false;

    // Restore VTable
    auto *table = ForceReinterpretCast<CKRenderContextVTable<CKRenderContext>*>(base, 0x86AF8);
    SaveVTable<CKRenderContextVTable<CKRenderContext>>(reinterpret_cast<void**>(&table), s_VTable);

    // Remove UpdateProjection hook
    if (CP_FUNC_TARGET_PTR_NAME(UpdateProjection)) {
        MH_DisableHook(*reinterpret_cast<void**>(&CP_FUNC_TARGET_PTR_NAME(UpdateProjection)));
        MH_RemoveHook(*reinterpret_cast<void**>(&CP_FUNC_TARGET_PTR_NAME(UpdateProjection)));
    }

    return true;
}

//-----------------------------------------------------------------------------
// Public API
//-----------------------------------------------------------------------------

bool InitRenderHook() {
    if (s_Initialized)
        return true;

    void *base = GetModuleBaseAddress("CK2_3D.dll");
    if (!base) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_Render",
               "Failed to get CK2_3D.dll base address");
        return false;
    }

    // Initialize MinHook
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_Render",
               "Failed to initialize MinHook");
        return false;
    }

    // Get topic IDs for events
    bmlImcGetTopicId(BML_TOPIC_ENGINE_PRE_RENDER, &s_TopicPreRender);
    bmlImcGetTopicId(BML_TOPIC_ENGINE_POST_RENDER, &s_TopicPostRender);

    // Install hooks
    if (!CP_HOOK_CLASS_NAME(CKRenderContext)::Hook(base)) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_Render",
               "Failed to install render hooks");
        return false;
    }

    s_Initialized = true;
    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Render",
           "Render engine hooks initialized (Render + UpdateProjection)");
    return true;
}

void ShutdownRenderHook() {
    if (!s_Initialized)
        return;

    void *base = GetModuleBaseAddress("CK2_3D.dll");
    if (base) {
        CP_HOOK_CLASS_NAME(CKRenderContext)::Unhook(base);
    }

    s_Initialized = false;
    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Render",
           "Render engine hooks shutdown");
}

void DisableRender(bool disable) {
    CP_HOOK_CLASS_NAME(CKRenderContext)::s_DisableRender = disable;
}

void EnableWidescreenFix(bool enable) {
    CP_HOOK_CLASS_NAME(CKRenderContext)::s_EnableWidescreenFix = enable;
}

bool IsRenderHookActive() {
    return s_Initialized;
}

} // namespace BML_Render
