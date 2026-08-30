#include "Loader/ModManager.h"

#include "Loader/ModContext.h"

#include "BML/InputHook.h"
#include "UI/Overlay.h"

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

        m_ModContext->LoadMods();
        m_ModContext->InitMods();

        Overlay::ImGuiNewFrame();
    }

    return CK_OK;
}

CKERROR ModManager::OnCKReset() {
    if (m_Context->GetCurrentLevel() != nullptr && m_RenderContext) {
        Overlay::ImGuiContextScope scope;
        Overlay::ImGuiEndFrame();

        m_ModContext->ShutdownMods();
        m_ModContext->UnloadMods();

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

    Overlay::ImGuiNewFrame();

    return CK_OK;
}

extern void PhysicsPostProcess();

CKERROR ModManager::PostProcess() {
    if (!m_ModContext || !m_ModContext->IsInited() || !m_RenderContext)
        return CK_OK;

    // The scope covers the whole frame so that whatever a mod draws goes to the loader's
    // context, but the physics, mod, and input work below is not drawing and still has to
    // run on a frame that has no context to draw into.
    Overlay::ImGuiContextScope scope;

    PhysicsPostProcess();

    m_ModContext->ProcessVirtoolsFrame();

    m_ModContext->OnProcess();

    auto *inputHook = m_ModContext->GetInputManager();
    if (!inputHook)
        return CK_OK;

    if (scope.IsActive()) {
        ImGuiIO &io = ImGui::GetIO();

        static bool cursorVisibilityChanged = false;
        if (io.WantCaptureMouse) {
            if (!inputHook->GetCursorVisibility()) {
                inputHook->ShowCursor(TRUE);
                cursorVisibilityChanged = true;
            }
        } else {
            if (cursorVisibilityChanged) {
                if (inputHook->GetCursorVisibility()) {
                    inputHook->ShowCursor(FALSE);
                    cursorVisibilityChanged = false;
                }
            }
        }

        Overlay::ImGuiRender();
    }

    inputHook->Process();
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
