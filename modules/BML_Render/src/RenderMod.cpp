/**
 * @file RenderMod.cpp
 * @brief BML_Render module -- render hook implementation and module entry point
 *
 * Contains:
 * - CKRenderContext VTable/MinHook hook implementations
 * - Public render hook API (InitRenderHook, ShutdownRenderHook, etc.)
 * - RenderMod class and BML_DEFINE_MODULE
 */

#define BML_LOADER_IMPLEMENTATION
#include "bml_hook_module.hpp"
#include "bml_config_bind.hpp"
#include "bml_game_topics.h"
#include "bml_virtools.h"
#include "bml_virtools.hpp"

#include <MinHook.h>

#include "RenderHook.h"
#include "HookUtils.h"
#include "bml_services.hpp"
#include "CKTimeManager.h"

// ============================================================================
// Render Hook Implementation (formerly RenderHook.cpp)
// ============================================================================

namespace BML_Render {

namespace {
static const bml::ModuleServices *s_Services = nullptr;
} // namespace

//-----------------------------------------------------------------------------
// Static State
//-----------------------------------------------------------------------------

bool CKRenderContextHook::s_DisableRender = false;
bool CKRenderContextHook::s_EnableWidescreenFix = false;
CKRenderContextVTable<CKRenderContext> CKRenderContextHook::s_VTable = {};

// UpdateProjection method pointers for MinHook
CKRenderContextHook::UpdateProjectionFn CKRenderContextHook::s_UpdateProjectionHookPtr =
    reinterpret_cast<CKRenderContextHook::UpdateProjectionFn>(&CKRenderContextHook::UpdateProjection);
CKRenderContextHook::UpdateProjectionFn CKRenderContextHook::s_UpdateProjectionOrigPtr = nullptr;
CKRenderContextHook::UpdateProjectionFn CKRenderContextHook::s_UpdateProjectionTargetPtr = nullptr;

static bool s_Initialized = false;

//-----------------------------------------------------------------------------
// Hook Implementations
//-----------------------------------------------------------------------------

CKERROR CKRenderContextHook::RenderHook(CK_RENDER_FLAGS Flags) {
    if (s_DisableRender)
        return CK_OK;

    // Call original Render method via saved VTable
    auto originalRender = s_VTable.Render;
    return (this->*originalRender)(Flags);
}

CKBOOL CKRenderContextHook::UpdateProjection(CKBOOL force) {
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

// CKRenderContext::Render VTable slot index within CKRenderContextVTable
static constexpr size_t kRenderSlot =
    offsetof(CKRenderContextVTable<CKRenderContext>, Render) / sizeof(void *);

bool CKRenderContextHook::Hook(void *base) {
    if (!base)
        return false;

    // Get VTable pointer at offset 0x86AF8 in CK2_3D.dll
    auto *table = utils::ForceReinterpretCast<CKRenderContextVTable<CKRenderContext> *>(base, 0x86AF8);

    // Save original VTable
    utils::LoadVTable<CKRenderContextVTable<CKRenderContext>>(&table, s_VTable);

    // Hook Render method via VTable
    utils::HookVirtualMethod(&table, &CKRenderContextHook::RenderHook, kRenderSlot);

    // Hook UpdateProjection via MinHook (non-virtual member function at offset 0x6C68D)
    s_UpdateProjectionTargetPtr = utils::ForceReinterpretCast<UpdateProjectionFn>(base, 0x6C68D);

    if (MH_CreateHook(*reinterpret_cast<LPVOID*>(&s_UpdateProjectionTargetPtr),
                      *reinterpret_cast<LPVOID*>(&s_UpdateProjectionHookPtr),
                      reinterpret_cast<LPVOID*>(&s_UpdateProjectionOrigPtr)) != MH_OK ||
        MH_EnableHook(*reinterpret_cast<LPVOID*>(&s_UpdateProjectionTargetPtr)) != MH_OK) {
        s_Services->Log().Warn("Failed to hook UpdateProjection - widescreen fix may not work");
        // Non-fatal, continue with Render hook only
    }

    return true;
}

bool CKRenderContextHook::Unhook(void *base) {
    if (!base)
        return false;

    // Restore VTable
    auto *table = utils::ForceReinterpretCast<CKRenderContextVTable<CKRenderContext> *>(base, 0x86AF8);
    utils::SaveVTable<CKRenderContextVTable<CKRenderContext>>(&table, s_VTable);

    // Remove UpdateProjection hook
    if (s_UpdateProjectionTargetPtr) {
        MH_DisableHook(*reinterpret_cast<void**>(&s_UpdateProjectionTargetPtr));
        MH_RemoveHook(*reinterpret_cast<void**>(&s_UpdateProjectionTargetPtr));
    }

    return true;
}

//-----------------------------------------------------------------------------
// Public API
//-----------------------------------------------------------------------------

bool InitRenderHook(const bml::ModuleServices &services) {
    if (s_Initialized)
        return true;

    s_Services = &services;

    void *base = utils::GetModuleBaseAddress("CK2_3D.dll");
    if (!base) {
        s_Services->Log().Error("Failed to get CK2_3D.dll base address");
        return false;
    }

    // Initialize MinHook
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        s_Services->Log().Error("Failed to initialize MinHook");
        return false;
    }

    // Install hooks
    if (!CKRenderContextHook::Hook(base)) {
        s_Services->Log().Error("Failed to install render hooks");
        return false;
    }

    s_Initialized = true;
    s_Services->Log().Info("Render engine hooks initialized (Render + UpdateProjection)");
    return true;
}

void ShutdownRenderHook() {
    if (!s_Initialized) {
        return;
    }

    void *base = utils::GetModuleBaseAddress("CK2_3D.dll");
    if (base) {
        CKRenderContextHook::Unhook(base);
    }

    s_Initialized = false;
    s_Services->Log().Info("Render engine hooks shutdown");
    s_Services = nullptr;
}

void DisableRender(bool disable) {
    CKRenderContextHook::s_DisableRender = disable;
}

void EnableWidescreenFix(bool enable) {
    CKRenderContextHook::s_EnableWidescreenFix = enable;
}

bool IsRenderHookActive() {
    return s_Initialized;
}

} // namespace BML_Render

// ============================================================================
// RenderMod Module Class
// ============================================================================

class RenderMod : public bml::HookModule {
    bool m_WidescreenFix = false;
    bool m_UnlockFrameRate = false;
    int32_t m_MaxFrameRate = 0;
    bml::ConfigBindings m_Cfg;

    void ApplyConfig() {
        BML_Render::EnableWidescreenFix(m_WidescreenFix);

        auto *timeManager = bml::virtools::GetTimeManager(Services());
        if (!timeManager) return;

        if (m_UnlockFrameRate) {
            timeManager->ChangeLimitOptions(CK_FRAMERATE_FREE);
            return;
        }

        if (m_MaxFrameRate > 0) {
            timeManager->ChangeLimitOptions(CK_FRAMERATE_LIMIT);
            timeManager->SetFrameRateLimit(static_cast<float>(m_MaxFrameRate));
            return;
        }

        timeManager->ChangeLimitOptions(CK_FRAMERATE_SYNC);
    }

    const char *HookLogCategory() const override { return "BML_Render"; }

    bool InitHook(CKContext *) override {
        if (!BML_Render::InitRenderHook(Services())) return false;
        m_Cfg.Refresh(Services().Config());
        ApplyConfig();
        return true;
    }

    void ShutdownHook() override {
        BML_Render::ShutdownRenderHook();
    }

    BML_Result OnModuleAttach() override {
        m_Cfg.Bind("Graphics", "WidescreenFix",   m_WidescreenFix,  false);
        m_Cfg.Bind("Graphics", "UnlockFrameRate",  m_UnlockFrameRate, false);
        m_Cfg.Bind("Graphics", "SetMaxFrameRate",  m_MaxFrameRate,    0);
        m_Cfg.Sync(Services().Config());
        ApplyConfig();

        m_Subs.Add(BML_TOPIC_ENGINE_END, [this](const bml::imc::Message &) {
            BML_Render::ShutdownRenderHook();
            m_HookReady = false;
        });

        m_Subs.Add(BML_TOPIC_GAME_MENU_POST_START, [this](const bml::imc::Message &) {
            m_Cfg.Refresh(Services().Config());
            ApplyConfig();
        });
        m_Subs.Add(BML_TOPIC_GAME_LEVEL_START, [this](const bml::imc::Message &) {
            m_Cfg.Refresh(Services().Config());
            ApplyConfig();
        });

        return BML_RESULT_OK;
    }
};

BML_DEFINE_MODULE(RenderMod)
