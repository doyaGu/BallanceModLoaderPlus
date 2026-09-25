#include "Loader/ModManager.h"

#include "Loader/ModContext.h"

#include "BML/InputHook.h"
#include "Hooks/HookLifecycle.h"
#include "Hooks/InputCursor.h"
#include "Hooks/RenderHook.h"
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
    StopRuntime();
    ModContext::Destroy(m_ModContext);
}

CKERROR ModManager::OnCKInit() {
    return m_ModContext->Init() ? CK_OK : CKERR_NOTINITIALIZED;
}

CKERROR ModManager::OnCKEnd() {
    const bool stopped = StopRuntime();
    const bool shutdown = m_ModContext->Shutdown();
    return stopped && shutdown ? CK_OK : CKERR_NOTINITIALIZED;
}

CKERROR ModManager::OnCKPlay() {
    if (m_Context->IsReseted() && m_Context->GetCurrentLevel() != nullptr &&
        !m_RenderContext && !StartRuntime())
        return CKERR_NOTINITIALIZED;

    return CK_OK;
}

CKERROR ModManager::OnCKReset() {
    const bool stopped = StopRuntime();
    if (m_ModContext)
        m_ModContext->ResetVirtoolsWorld();

    return stopped ? CK_OK : CKERR_NOTINITIALIZED;
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

bool ModManager::StartRuntime() {
    if (!m_ModContext || m_RenderContext)
        return m_RenderContext != nullptr;

    CKRenderContext *renderContext = m_Context->GetPlayerRenderContext();
    if (!renderContext)
        return false;

    if (!RenderHook::Attach(renderContext)) {
        if (RenderHook::IsSkipRenderAvailable()) {
            m_ModContext->GetLogger()->Error("A render hook from an earlier context is still installed");
            return false;
        }
        m_ModContext->GetLogger()->Warn("Render skip is unavailable for this render context");
    }

    if (!Overlay::ImGuiInitRenderer(m_Context)) {
        RenderHook::Detach();
        m_ModContext->GetLogger()->Error("Failed to initialize the ImGui renderer backend");
        return false;
    }

    m_RenderContext = renderContext;
    Overlay::ImGuiContextScope scope;
    if (!m_ModContext->GetModLoader().Start()) {
        Overlay::ImGuiShutdownRenderer(m_Context);
        RenderHook::Detach();
        m_RenderContext = nullptr;
        return false;
    }

    SynchronizeUiFonts();
    Overlay::ImGuiNewFrame();
    return true;
}

bool ModManager::StopRuntime() {
    if (!m_ModContext)
        return RenderHook::Detach();

    bool stopped = true;
    if (m_RenderContext) {
        Overlay::ImGuiContextScope scope;
        Overlay::ImGuiEndFrame();
        SetOverlayCursorVisible(false);
        stopped = m_ModContext->GetModLoader().Stop();
        Overlay::ImGuiShutdownRenderer(m_Context);
        m_RenderContext = nullptr;
    } else {
        stopped = m_ModContext->GetModLoader().Stop();
    }

    const bool detached = RenderHook::Detach();
    if (!detached && m_ModContext->GetLogger())
        m_ModContext->GetLogger()->Warn("Render skip hook could not be detached cleanly");
    return stopped && detached;
}

CKERROR ModManager::PostProcess() {
    if (!m_ModContext || !m_ModContext->IsInited() || !m_RenderContext)
        return CK_OK;

    // The scope covers the whole frame so that whatever a mod draws goes to the loader's
    // context, but the physics, mod, and input work below is not drawing and still has to
    // run on a frame that has no context to draw into.
    Overlay::ImGuiContextScope scope;
    ImGuiFrameCompletion frameCompletion;

    RunPhysicsPostProcess();

    m_ModContext->ProcessVirtoolsFrame();

    m_ModContext->OnProcess();

    auto *inputHook = m_ModContext->GetInputManager();
    if (!inputHook)
        return CK_OK;

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

CKERROR ModManager::OnPreRender(CKRenderContext *dev) {
    RenderHook::ApplyWidescreenProjection(dev);
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
