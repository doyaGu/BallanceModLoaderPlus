#include "ModManager.h"

#include "ModContext.h"

#include "BML/InputHook.h"
#include "Overlay.h"

ModManager::ModManager(CKContext *context) : CKBaseManager(context, MOD_MANAGER_GUID, (CKSTRING) "Mod Manager") {
    m_ModContext = new ModContext(m_Context);
    context->RegisterNewManager(this);
}

ModManager::~ModManager() {
    delete m_ModContext;
}

CKERROR ModManager::OnCKInit() {
    m_ModContext->Init();
    return CK_OK;
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

    return CK_OK;
}

CKERROR ModManager::PreProcess() {
    Overlay::ImGuiContextScope scope;

    Overlay::ImGuiNewFrame();

    return CK_OK;
}

extern void PhysicsPostProcess();

CKERROR ModManager::PostProcess() {
    Overlay::ImGuiContextScope scope;

    PhysicsPostProcess();

    m_ModContext->OnProcess();

    auto *inputHook = m_ModContext->GetInputManager();
    ImGuiIO &io = ImGui::GetIO();

    const bool uiNeedsMouse = io.WantCaptureMouse;
    if (uiNeedsMouse) {
        io.MouseDrawCursor = false;
        if (!inputHook->GetCursorVisibility())
            inputHook->ShowCursor(TRUE);
    } else {
        io.MouseDrawCursor = false;
        if (inputHook->GetCursorVisibility())
            inputHook->ShowCursor(FALSE);
    }

    Overlay::ImGuiRender();
    inputHook->Process();
    return CK_OK;
}

CKERROR ModManager::OnPostRender(CKRenderContext *dev) {
    m_ModContext->OnRender(dev);

    return CK_OK;
}

CKERROR ModManager::OnPostSpriteRender(CKRenderContext *dev) {
    Overlay::ImGuiOnRender();

    return CK_OK;
}
