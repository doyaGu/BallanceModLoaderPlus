#include "Loader/ModManager.h"

#include "Loader/ModContext.h"

#include "BML/InputHook.h"
#include "Hooks/InputCursor.h"
#include "UI/Overlay.h"
#include "UI/FontRuntime.h"
#if BML_ENABLE_UI_AUTOMATION
#include "player/UiAutomation.h"
#endif

namespace {
class ImGuiFrameCompletion {
public:
    ~ImGuiFrameCompletion() {
        Overlay::ImGuiEndFrame();
    }
};
} // namespace

ModManager::ModManager(CKContext *context) : CKBaseManager(context, MOD_MANAGER_GUID, (CKSTRING) "Mod Manager") {
    m_ModContext = new ModContext(m_Context);
    context->RegisterNewManager(this);
}

ModManager::~ModManager() {
    delete m_ModContext;
}

CKERROR ModManager::OnCKInit() {
    return m_ModContext->Init() ? CK_OK : CKERR_NOTINITIALIZED;
}

CKERROR ModManager::OnCKEnd() {
    m_ModContext->Shutdown();
    return CK_OK;
}

CKERROR ModManager::OnCKPlay() {
    if (m_Context->IsReseted() && m_Context->GetCurrentLevel() != nullptr && !m_RenderContext) {
        m_RenderContext = m_Context->GetPlayerRenderContext();

        Overlay::ImGuiInitRenderer(m_Context);
        Overlay::ImGuiContextScope scope;

        m_ModContext->GetModLoader().Start();

        SynchronizeUiFonts();
        Overlay::ImGuiNewFrame();
    }

    return CK_OK;
}

CKERROR ModManager::OnCKReset() {
    if (m_Context->GetCurrentLevel() != nullptr && m_RenderContext) {
        Overlay::ImGuiContextScope scope;
        Overlay::ImGuiEndFrame();

        if (auto *input = m_ModContext->GetInputManager())
            SetOverlayCursorVisible(false);

        m_ModContext->GetModLoader().Stop();

        Overlay::ImGuiShutdownRenderer(m_Context);

        m_RenderContext = nullptr;
    }
    if (m_ModContext)
        m_ModContext->ResetVirtoolsWorld();

    return CK_OK;
}

CKERROR ModManager::PreClearAll() {
    if (m_ModContext)
        m_ModContext->ResetVirtoolsWorld();
    return CK_OK;
}

CKERROR ModManager::SequenceToBeDeleted(CK_ID *objids, int count) {
    if (m_ModContext)
        m_ModContext->VirtoolsObjectsToBeDeleted(objids, count);
    return CK_OK;
}

CKERROR ModManager::PreProcess() {
    if (!m_ModContext || !m_ModContext->IsInited() || !m_RenderContext)
        return CK_OK;

    Overlay::ImGuiContextScope scope;

    SynchronizeUiFonts();
    Overlay::ImGuiNewFrame();

    return CK_OK;
}

void ModManager::SynchronizeUiFonts() {
    if (!m_ModContext || !m_RenderContext)
        return;

    BML::UI::FontRuntime *fonts = m_ModContext->GetUiFontRuntime();
    ImGuiContext *imgui = Overlay::GetImGuiContext();
    if (!fonts || !imgui)
        return;

    fonts->Synchronize(*imgui, static_cast<float>(m_RenderContext->GetHeight()));
    const BML::UI::FontRuntimeSnapshot &snapshot = fonts->Inspect();
    if (snapshot.Generation == 0 || snapshot.Generation == m_ReportedUiFontGeneration)
        return;

    m_ReportedUiFontGeneration = snapshot.Generation;
    m_ModContext->GetLogger()->Info(
        "Applied UI font generation %llu "
        "(Unicode scalars: %s, common emoji: %s, color emoji: %s)",
        static_cast<unsigned long long>(snapshot.Generation),
        snapshot.SupportsUnicodeScalars ? "yes" : "no",
        snapshot.SupportsCommonEmoji ? "yes" : "no",
        snapshot.SupportsColorEmoji ? "yes" : "no");
    for (const std::string &diagnostic : snapshot.Diagnostics)
        m_ModContext->GetLogger()->Warn("UI font: %s", diagnostic.c_str());
}

extern void PhysicsPostProcess();

CKERROR ModManager::PostProcess() {
    if (!m_ModContext || !m_ModContext->IsInited() || !m_RenderContext)
        return CK_OK;

    // The scope covers the whole frame so that whatever a mod draws goes to the loader's
    // context, but the physics, mod, and input work below is not drawing and still has to
    // run on a frame that has no context to draw into.
    Overlay::ImGuiContextScope scope;
    ImGuiFrameCompletion frameCompletion;

    PhysicsPostProcess();

    m_ModContext->ProcessVirtoolsFrame();

    m_ModContext->OnProcess();

    auto *inputHook = m_ModContext->GetInputManager();
    if (!inputHook) {
        // PreProcess may already have opened an ImGui frame. Input can vanish
        // during reset/unload; never leave that frame open for the next
        // NewFrame(), which is a hard ImGui lifecycle assertion.
        Overlay::ImGuiEndFrame();
        return CK_OK;
    }

    if (scope.IsActive()) {
        SetOverlayCursorVisible(ImGui::GetIO().WantCaptureMouse);
        Overlay::ImGuiRender();
    } else {
        SetOverlayCursorVisible(false);
    }

    inputHook->Process();
#if BML_ENABLE_UI_AUTOMATION
    // Test actions can reset or exit Virtools. Run them only after Render and
    // after the final per-frame pointer use, never from inside ImGuiRender().
    UiAutomation::AdvanceFrame();
#endif
    return CK_OK;
}

CKERROR ModManager::OnPostRender(CKRenderContext *dev) {
    if (!m_ModContext || !m_ModContext->IsInited() || !m_RenderContext)
        return CK_OK;

    m_ModContext->OnRender(dev);

    return CK_OK;
}

CKERROR ModManager::OnPostSpriteRender(CKRenderContext *dev) {
    if (!m_ModContext || !m_ModContext->IsInited() || !m_RenderContext)
        return CK_OK;

    Overlay::ImGuiOnRender();
    return CK_OK;
}
