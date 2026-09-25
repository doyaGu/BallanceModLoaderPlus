#include "Hooks/HookLifecycle.h"

#include <exception>

#include "BML/InputHook.h"
#include "Hooks/RenderHook.h"

HookLifecycle::HookLifecycle(ModContext &context)
    : m_Context(context), m_Render(std::make_unique<RenderHook>()) {}

HookLifecycle::~HookLifecycle() {
    if (!Detach())
        std::terminate();
}

bool HookLifecycle::Attach(CKInputManager *inputManager) {
    if (m_Input || OwnsObjectLoad() || OwnsPhysicsPostProcess() || OwnsPhysicalize())
        return m_Input && InputHookOwnsActive(*m_Input) && OwnsObjectLoad() && OwnsPhysicalize();

    auto input = std::make_unique<InputHook>(inputManager);
    if (!InputHookOwnsActive(*input))
        return false;
    m_Input = std::move(input);

    if (!AttachObjectLoad() || !AttachPhysicalize()) {
        Detach();
        return false;
    }
    return true;
}

bool HookLifecycle::Detach() {
    bool detached = true;

    if (!DetachRender())
        detached = false;

    if (m_Input) {
        if (DetachInputHook(*m_Input))
            m_Input.reset();
        else
            detached = false;
    }

    if (!DetachObjectLoad())
        detached = false;
    if (!DetachPhysicalize())
        detached = false;
    return detached;
}

bool HookLifecycle::AttachRender(CKRenderContext *renderContext) {
    return m_Render && m_Render->Attach(renderContext);
}

bool HookLifecycle::DetachRender() {
    return !m_Render || m_Render->Detach();
}

HookSnapshot HookLifecycle::Inspect() const {
    HookSnapshot snapshot;
    snapshot.Input = m_Input && InputHookOwnsActive(*m_Input);
    snapshot.ObjectLoad = OwnsObjectLoad();
    snapshot.PhysicsPostProcess = OwnsPhysicsPostProcess();
    snapshot.Physicalize = OwnsPhysicalize();
    snapshot.RenderSkip = m_Render && m_Render->IsAttached();
    return snapshot;
}
