#ifndef BML_HOOKS_HOOK_LIFECYCLE_H
#define BML_HOOKS_HOOK_LIFECYCLE_H

#include <memory>

class CKInputManager;
class CKRenderContext;
class InputHook;
class ModContext;
class RenderHook;
struct CKBehaviorContext;

bool InputHookOwnsActive(const InputHook &hook);
bool DetachInputHook(InputHook &hook);

struct HookSnapshot {
    bool Input = false;
    bool ObjectLoad = false;
    bool PhysicsPostProcess = false;
    bool Physicalize = false;
    bool RenderSkip = false;
};

class HookLifecycle {
public:
    explicit HookLifecycle(ModContext &context);
    ~HookLifecycle();

    HookLifecycle(const HookLifecycle &) = delete;
    HookLifecycle &operator=(const HookLifecycle &) = delete;

    bool Attach(CKInputManager *inputManager);
    bool Detach();
    bool AttachRender(CKRenderContext *renderContext);
    bool DetachRender();

    InputHook *GetInput() const { return m_Input.get(); }
    void RunPhysicsPostProcess();
    HookSnapshot Inspect() const;

private:
    bool AttachObjectLoad();
    bool DetachObjectLoad();
    bool OwnsObjectLoad() const;

    bool AttachPhysicalize();
    bool DetachPhysicalize();
    bool OwnsPhysicsPostProcess() const;
    bool OwnsPhysicalize() const;

    static int ObjectLoad(const CKBehaviorContext &context);
    static int Physicalize(const CKBehaviorContext &context);

    static HookLifecycle *s_ObjectLoadOwner;
    static HookLifecycle *s_PhysicsOwner;

    ModContext &m_Context;
    std::unique_ptr<InputHook> m_Input;
    std::unique_ptr<RenderHook> m_Render;
};

#endif // BML_HOOKS_HOOK_LIFECYCLE_H
