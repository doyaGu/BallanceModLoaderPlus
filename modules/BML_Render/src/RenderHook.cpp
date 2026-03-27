/**
 * @file RenderHook.cpp
 * @brief CK2_3D.dll hook implementation for BML_Render
 *
 * Two hooks:
 * 1. CKRenderContext::Render (VTable slot) -- render skip
 * 2. CKRenderContext::UpdateProjection (MinHook) -- widescreen FOV pre-correction
 */

#include "RenderHook.h"
#include "CKRenderContextInternal.h"

#include <MinHook.h>

#include "HookUtils.h"
#include "bml_services.hpp"

namespace {

// ============================================================================
// State
// ============================================================================

const bml::ModuleServices *g_Services = nullptr;
bool g_Initialized = false;
bool g_DisableRender = false;
bool g_EnableWidescreenFix = false;

// ============================================================================
// Render VTable hook (render skip)
// ============================================================================

// Partial CKRenderContext VTable layout -- just enough to derive slot indices.
template <typename T>
struct CKRenderContextVTable {
    void *pad[17];  // CKObject virtual methods
    void (T::*AddObject)(CKRenderObject *);
    void (T::*AddObjectWithHierarchy)(CKRenderObject *);
    void (T::*RemoveObject)(CKRenderObject *);
    CKBOOL (T::*IsObjectAttached)(CKRenderObject *);
    const XObjectArray &(T::*Compute3dRootObjects)();
    const XObjectArray &(T::*Compute2dRootObjects)();
    CK2dEntity *(T::*Get2dRoot)(CKBOOL);
    void (T::*DetachAll)();
    void (T::*ForceCameraSettingsUpdate)();
    void (T::*PrepareCameras)(CK_RENDER_FLAGS);
    CKERROR (T::*Clear)(CK_RENDER_FLAGS, CKDWORD);
    CKERROR (T::*DrawScene)(CK_RENDER_FLAGS);
    CKERROR (T::*BackToFront)(CK_RENDER_FLAGS);
    CKERROR (T::*Render)(CK_RENDER_FLAGS);
};

static constexpr size_t kRenderSlot =
    offsetof(CKRenderContextVTable<CKRenderContext>, Render) / sizeof(void *);
void *g_OriginalRender = nullptr;

struct RenderThunk {
    CKERROR Hook(CK_RENDER_FLAGS Flags) {
        if (g_DisableRender)
            return CK_OK;
        // Call original via raw VTable entry
        using Fn = CKERROR (__thiscall *)(CKRenderContext *, CK_RENDER_FLAGS);
        return reinterpret_cast<Fn>(g_OriginalRender)(reinterpret_cast<CKRenderContext *>(this), Flags);
    }
};

// ============================================================================
// UpdateProjection MinHook (widescreen FOV pre-correction)
// ============================================================================

using UpdateProjectionFn = CKBOOL (BML_Render::CKRenderContextInternal::*)(CKBOOL);
UpdateProjectionFn g_UpdateProjectionOrigPtr = nullptr;
UpdateProjectionFn g_UpdateProjectionTargetPtr = nullptr;

// Pre-correction: temporarily replace m_Fov with the widescreen-adjusted
// value, then let the engine's own UpdateProjection do all the work.
// This way every side effect (projection matrix, viewport, 2D root rects,
// m_ProjectionUpdated flag) is computed correctly by the engine itself.
struct UpdateProjectionThunk : BML_Render::CKRenderContextInternal {
    CKBOOL Hook(CKBOOL force) {
        auto *self = reinterpret_cast<BML_Render::CKRenderContextInternal *>(this);

        if (!g_EnableWidescreenFix || !self->m_Perspective)
            return (self->*g_UpdateProjectionOrigPtr)(force);

        // Invalidate cache so the engine recalculates with our adjusted FOV
        self->m_ProjectionUpdated = FALSE;

        // Compute Hor+ FOV: preserve vertical FOV from 4:3, widen for actual aspect
        float aspect = static_cast<float>(self->m_ViewportData.ViewWidth)
                     / static_cast<float>(self->m_ViewportData.ViewHeight);
        float origFov = self->m_Fov;
        self->m_Fov = 2.0f * atanf(tanf(origFov * 0.5f) * 0.75f * aspect);

        CKBOOL result = (self->*g_UpdateProjectionOrigPtr)(force);

        self->m_Fov = origFov;
        return result;
    }
};

static UpdateProjectionFn g_UpdateProjectionHookPtr =
    reinterpret_cast<UpdateProjectionFn>(&UpdateProjectionThunk::Hook);

// ============================================================================
// CK2_3D.dll offsets
// ============================================================================

constexpr size_t kVTableOffset = 0x86AF8;
constexpr size_t kUpdateProjectionOffset = 0x6C68D;

// ============================================================================
// Install / Uninstall
// ============================================================================

bool InstallHooks(void *base) {
    // 1) Render VTable hook
    // HookVirtualMethod reads *instance as VTable pointer.  The global VTable
    // array IS at base+kVTableOffset, so we pass &ptr where ptr holds that address.
    void *vtable = static_cast<char *>(base) + kVTableOffset;
    g_OriginalRender = utils::HookVirtualMethod(&vtable, &RenderThunk::Hook, kRenderSlot);

    // 2) UpdateProjection MinHook
    g_UpdateProjectionTargetPtr =
        utils::ForceReinterpretCast<UpdateProjectionFn>(base, kUpdateProjectionOffset);

    if (MH_CreateHook(*reinterpret_cast<LPVOID *>(&g_UpdateProjectionTargetPtr),
                      *reinterpret_cast<LPVOID *>(&g_UpdateProjectionHookPtr),
                      reinterpret_cast<LPVOID *>(&g_UpdateProjectionOrigPtr)) != MH_OK ||
        MH_EnableHook(*reinterpret_cast<LPVOID *>(&g_UpdateProjectionTargetPtr)) != MH_OK) {
        g_Services->Log().Warn("Failed to hook UpdateProjection - widescreen fix unavailable");
    }

    return true;
}

void UninstallHooks(void *base) {
    // Restore Render VTable slot
    if (g_OriginalRender) {
        void *vtable = static_cast<char *>(base) + kVTableOffset;
        utils::HookVirtualMethod(&vtable, g_OriginalRender, kRenderSlot);
    }

    // Remove UpdateProjection MinHook
    if (g_UpdateProjectionTargetPtr) {
        MH_DisableHook(*reinterpret_cast<void **>(&g_UpdateProjectionTargetPtr));
        MH_RemoveHook(*reinterpret_cast<void **>(&g_UpdateProjectionTargetPtr));
    }
}

} // anonymous namespace

// ============================================================================
// Public API
// ============================================================================

namespace BML_Render {

bool InitRenderHook(const bml::ModuleServices &services) {
    if (g_Initialized)
        return true;

    g_Services = &services;

    void *base = utils::GetModuleBaseAddress("CK2_3D.dll");
    if (!base) {
        g_Services->Log().Error("CK2_3D.dll base address not found");
        return false;
    }

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        g_Services->Log().Error("MinHook initialization failed");
        return false;
    }

    if (!InstallHooks(base)) {
        g_Services->Log().Error("Render hook installation failed");
        return false;
    }

    g_Initialized = true;
    return true;
}

void ShutdownRenderHook() {
    if (!g_Initialized)
        return;

    void *base = utils::GetModuleBaseAddress("CK2_3D.dll");
    if (base)
        UninstallHooks(base);

    g_Initialized = false;
    g_Services = nullptr;
}

void DisableRender(bool disable) { g_DisableRender = disable; }
void EnableWidescreenFix(bool enable) { g_EnableWidescreenFix = enable; }
bool IsRenderHookActive() { return g_Initialized; }

} // namespace BML_Render
