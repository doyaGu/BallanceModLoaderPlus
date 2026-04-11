#include "RenderHook.h"

#include "CK2dEntity.h"

#include <MinHook.h>

#include "VTables.h"
#include "HookUtils.h"

#define CP_ADD_METHOD_HOOK(Name, Base, Offset) \
        { CP_FUNC_TARGET_PTR_NAME(Name) = utils::ForceReinterpretCast<CP_FUNC_TYPE_NAME(Name)>(Base, Offset); } \
        if ((MH_CreateHook(*reinterpret_cast<LPVOID *>(&CP_FUNC_TARGET_PTR_NAME(Name)), \
                           *reinterpret_cast<LPVOID *>(&CP_FUNC_PTR_NAME(Name)), \
                            reinterpret_cast<LPVOID *>(&CP_FUNC_ORIG_PTR_NAME(Name))) != MH_OK || \
            MH_EnableHook(*reinterpret_cast<LPVOID *>(&CP_FUNC_TARGET_PTR_NAME(Name))) != MH_OK)) \
                return false;

#define CP_REMOVE_METHOD_HOOK(Name) \
    MH_DisableHook(*reinterpret_cast<void **>(&CP_FUNC_TARGET_PTR_NAME(Name))); \
    MH_RemoveHook(*reinterpret_cast<void **>(&CP_FUNC_TARGET_PTR_NAME(Name)));

// CKRenderContext

bool CP_HOOK_CLASS_NAME(CKRenderContext)::s_DisableRender = false;
bool CP_HOOK_CLASS_NAME(CKRenderContext)::s_EnableWidescreenFix = false;
CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext> CP_HOOK_CLASS_NAME(CKRenderContext)::s_VTable = {};
CP_DEFINE_METHOD_PTRS(CP_HOOK_CLASS_NAME(CKRenderContext), UpdateProjection);

#define CP_RENDER_CONTEXT_METHOD_NAME(Name) CP_HOOK_CLASS_NAME(CKRenderContext)::CP_FUNC_HOOK_NAME(Name)

CKERROR CP_RENDER_CONTEXT_METHOD_NAME(Render)(CK_RENDER_FLAGS Flags) {
    if (s_DisableRender)
        return CK_OK;

    return CP_CALL_METHOD_PTR(this, s_VTable.Render, Flags);
}

CKBOOL CP_HOOK_CLASS_NAME(CKRenderContext)::UpdateProjection(CKBOOL force) {
    if (!force && m_ProjectionUpdated)
        return TRUE;
    if (!m_RasterizerContext)
        return FALSE;

    const auto aspect = (float) ((double) m_ViewportData.ViewWidth / (double) m_ViewportData.ViewHeight);
    if (m_Perspective) {
        const float fov = s_EnableWidescreenFix ? atan2f(tanf(m_Fov * 0.5f) * 0.75f * aspect, 1.0f) * 2.0f : m_Fov;
        m_ProjectionMatrix.Perspective(fov, aspect, m_NearPlane, m_FarPlane);
    } else {
        m_ProjectionMatrix.Orthographic(m_Zoom, aspect, m_NearPlane, m_FarPlane);
    }

    m_RasterizerContext->SetTransformMatrix(VXMATRIX_PROJECTION, m_ProjectionMatrix);
    m_RasterizerContext->SetViewport(&m_ViewportData);
    m_ProjectionUpdated = TRUE;

    VxRect rect(0.0f, 0.0f, (float) m_Settings.m_Rect.right, (float) m_Settings.m_Rect.bottom);
    auto *background = Get2dRoot(TRUE);
    background->SetRect(rect);
    auto *foreground = Get2dRoot(FALSE);
    foreground->SetRect(rect);

    return TRUE;
}

static void *s_RenderOriginalSlots[4] = {};
static size_t s_RenderHookedSlotIndices[4] = {};
static size_t s_RenderHookedSlotCount = 0;

bool CP_HOOK_CLASS_NAME(CKRenderContext)::Hook(void *base) {
    if (!base)
        return false;

    s_RenderHookedSlotCount = 0;
    auto *table = utils::ForceReinterpretCast<CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext> *>(base, 0x86AF8);
    utils::LoadVTable<CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext> >(&table, s_VTable);

    {
        size_t slotIndex = offsetof(CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext>, Render) / sizeof(void*);
        void *original = utils::HookVirtualMethod(&table,
            &CP_HOOK_CLASS_NAME(CKRenderContext)::CP_FUNC_HOOK_NAME(Render), slotIndex);
        s_RenderOriginalSlots[s_RenderHookedSlotCount] = original;
        s_RenderHookedSlotIndices[s_RenderHookedSlotCount] = slotIndex;
        ++s_RenderHookedSlotCount;
    }

    CP_ADD_METHOD_HOOK(UpdateProjection, base, 0x6C68D);

    return true;
}

bool CP_HOOK_CLASS_NAME(CKRenderContext)::Unhook(void *base) {
    if (!base || s_RenderHookedSlotCount == 0)
        return false;

    auto *table = utils::ForceReinterpretCast<CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext> *>(base, 0x86AF8);
    void **vtable = reinterpret_cast<void **>(table);

    size_t maxSlot = 0;
    for (size_t i = 0; i < s_RenderHookedSlotCount; ++i) {
        if (s_RenderHookedSlotIndices[i] > maxSlot)
            maxSlot = s_RenderHookedSlotIndices[i];
    }
    size_t regionSize = (maxSlot + 1) * sizeof(void *);

    uint32_t oldProtect = utils::UnprotectRegion(vtable, regionSize);
    if (oldProtect) {
        for (size_t i = 0; i < s_RenderHookedSlotCount; ++i) {
            vtable[s_RenderHookedSlotIndices[i]] = s_RenderOriginalSlots[i];
        }
        utils::ProtectRegion(vtable, regionSize, oldProtect);
    }
    s_RenderHookedSlotCount = 0;

    CP_REMOVE_METHOD_HOOK(UpdateProjection);

    return true;
}

namespace RenderHook {
    bool HookRenderEngine() {
        void *base = utils::GetModuleBaseAddress("CK2_3D.dll");
        if (!base)
            return false;

        CP_HOOK_CLASS_NAME(CKRenderContext)::Hook(base);

        return true;
    }

    bool UnhookRenderEngine() {
        void *base = utils::GetModuleBaseAddress("CK2_3D.dll");
        if (!base)
            return false;

        CP_HOOK_CLASS_NAME(CKRenderContext)::Unhook(base);

        return true;
    }

    void DisableRender(bool disable) {
        CP_HOOK_CLASS_NAME(CKRenderContext)::s_DisableRender = disable;
    }

    void EnableWidescreenFix(bool enable) {
        CP_HOOK_CLASS_NAME(CKRenderContext)::s_EnableWidescreenFix = enable;
    }
}
