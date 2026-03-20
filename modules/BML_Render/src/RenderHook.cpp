/**
 * @file RenderHook.cpp
 * @brief CK2 Render Engine Hook Implementation for BML_Render Module
 * 
 * Self-contained render hook that provides:
 * - CKRenderContext::Render VTable hook
 * - CKRenderContext::UpdateProjection member function hook
 */

#include "RenderHook.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#include <MinHook.h>

#include "HookUtils.h"
#include "bml_services.hpp"

namespace BML_Render {

namespace {
const bml::ModuleServices *s_ModServices = nullptr;

void Log(BML_LogSeverity severity, const char *message) {
    if (!s_ModServices || !message) {
        return;
    }

    switch (severity) {
        case BML_LOG_INFO:
            s_ModServices->Log().Info(message);
            break;
        case BML_LOG_WARN:
            s_ModServices->Log().Warn(message);
            break;
        case BML_LOG_ERROR:
            s_ModServices->Log().Error(message);
            break;
        default:
            break;
    }
}
} // namespace

//-----------------------------------------------------------------------------
// Static State
//-----------------------------------------------------------------------------

bool CP_HOOK_CLASS_NAME(CKRenderContext)::s_DisableRender = false;
bool CP_HOOK_CLASS_NAME(CKRenderContext)::s_EnableWidescreenFix = false;
CKRenderContextVTable<CKRenderContext> CP_HOOK_CLASS_NAME(CKRenderContext)::s_VTable = {};

// UpdateProjection method pointers for MinHook
CP_DEFINE_METHOD_PTRS(CP_HOOK_CLASS_NAME(CKRenderContext), UpdateProjection);

static bool s_Initialized = false;

//-----------------------------------------------------------------------------
// Hook Implementations
//-----------------------------------------------------------------------------

CKERROR CP_HOOK_CLASS_NAME(CKRenderContext)::RenderHook(CK_RENDER_FLAGS Flags) {
    if (s_DisableRender)
        return CK_OK;

    // Call original Render method via saved VTable
    auto originalRender = s_VTable.Render;
    return CP_CALL_METHOD_PTR(this, originalRender, Flags);
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
    auto *table = utils::ForceReinterpretCast<CKRenderContextVTable<CKRenderContext> *>(base, 0x86AF8);
    
    // Save original VTable
    utils::LoadVTable<CKRenderContextVTable<CKRenderContext>>(&table, s_VTable);

    // Hook Render method via VTable
    constexpr size_t RenderVTableIndex = offsetof(CKRenderContextVTable<CKRenderContext>, Render) / sizeof(void*);
    utils::HookVirtualMethod(&table, &CP_HOOK_CLASS_NAME(CKRenderContext)::RenderHook, RenderVTableIndex);

    // Hook UpdateProjection via MinHook (non-virtual member function at offset 0x6C68D)
    CP_FUNC_TARGET_PTR_NAME(UpdateProjection) = utils::ForceReinterpretCast<CP_FUNC_TYPE_NAME(UpdateProjection)>(base, 0x6C68D);
    
    if (MH_CreateHook(*reinterpret_cast<LPVOID*>(&CP_FUNC_TARGET_PTR_NAME(UpdateProjection)),
                      *reinterpret_cast<LPVOID*>(&CP_FUNC_PTR_NAME(UpdateProjection)),
                      reinterpret_cast<LPVOID*>(&CP_FUNC_ORIG_PTR_NAME(UpdateProjection))) != MH_OK ||
        MH_EnableHook(*reinterpret_cast<LPVOID*>(&CP_FUNC_TARGET_PTR_NAME(UpdateProjection))) != MH_OK) {
        Log(BML_LOG_WARN, "Failed to hook UpdateProjection - widescreen fix may not work");
        // Non-fatal, continue with Render hook only
    }

    return true;
}

bool CP_HOOK_CLASS_NAME(CKRenderContext)::Unhook(void *base) {
    if (!base)
        return false;

    // Restore VTable
    auto *table = utils::ForceReinterpretCast<CKRenderContextVTable<CKRenderContext> *>(base, 0x86AF8);
    utils::SaveVTable<CKRenderContextVTable<CKRenderContext>>(&table, s_VTable);

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

bool InitRenderHook(const bml::ModuleServices &services) {
    if (s_Initialized)
        return true;

    s_ModServices = &services;

    void *base = utils::GetModuleBaseAddress("CK2_3D.dll");
    if (!base) {
        Log(BML_LOG_ERROR, "Failed to get CK2_3D.dll base address");
        return false;
    }

    // Initialize MinHook
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        Log(BML_LOG_ERROR, "Failed to initialize MinHook");
        return false;
    }

    // Install hooks
    if (!CP_HOOK_CLASS_NAME(CKRenderContext)::Hook(base)) {
        Log(BML_LOG_ERROR, "Failed to install render hooks");
        return false;
    }

    s_Initialized = true;
    Log(BML_LOG_INFO, "Render engine hooks initialized (Render + UpdateProjection)");
    return true;
}

void ShutdownRenderHook() {
    if (!s_Initialized) {
        return;
    }

    void *base = utils::GetModuleBaseAddress("CK2_3D.dll");
    if (base) {
        CP_HOOK_CLASS_NAME(CKRenderContext)::Unhook(base);
    }

    s_Initialized = false;
    Log(BML_LOG_INFO, "Render engine hooks shutdown");
    s_ModServices = nullptr;
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
