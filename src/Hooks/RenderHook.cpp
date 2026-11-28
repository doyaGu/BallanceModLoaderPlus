#include "Hooks/RenderHook.h"

#include "CKAll.h"
#include <exception>

struct RenderHook::Impl {
    CKContext *Owner = nullptr;
    CKObjectHandle ContextHandle{};
    bool SavedAdjustFov = false;
    bool SkipPending = false;
    static Impl *Active;
    static bool WidescreenFixEnabled;

    CKRenderContext *Context() const {
        return Owner ? CKRenderContext::Cast(Owner->GetObjectByHandle(ContextHandle)) : nullptr;
    }

    static void RenderOptions(CKRenderContext *context, CK_RENDER_FLAGS *flags, void *argument) {
        auto *self = static_cast<Impl *>(argument);
        if (self == Active && self->Context() == context && self->SkipPending) {
            self->SkipPending = false;
            *flags = static_cast<CK_RENDER_FLAGS>(*flags | CK_RENDER_SKIPALL);
        }
    }
};

RenderHook::Impl *RenderHook::Impl::Active = nullptr;
bool RenderHook::Impl::WidescreenFixEnabled = false;

RenderHook::RenderHook() : m_Impl(std::make_unique<Impl>()) {}
RenderHook::~RenderHook() {
    if (!Detach())
        std::terminate();
}

bool RenderHook::Attach(CKRenderContext *context) {
    if (!context || !m_Impl || (Impl::Active && Impl::Active != m_Impl.get()))
        return false;
    if (m_Impl->Context() == context)
        return context->SetRenderOptionsCallBack(Impl::RenderOptions, m_Impl.get()) != FALSE;
    if (!Detach())
        return false;
    if (!context->SetRenderOptionsCallBack(Impl::RenderOptions, m_Impl.get()))
        return false;
    m_Impl->Owner = context->GetCKContext();
    m_Impl->ContextHandle = m_Impl->Owner->GetObjectHandle(context->GetID());
    m_Impl->SavedAdjustFov = (context->GetCurrentRenderOptions() & CK_RENDER_ADJUSTFOV) != 0;
    Impl::Active = m_Impl.get();
    context->ChangeCurrentRenderOptions(
        Impl::WidescreenFixEnabled ? CK_RENDER_ADJUSTFOV : 0,
        Impl::WidescreenFixEnabled ? 0 : CK_RENDER_ADJUSTFOV);
    return true;
}

bool RenderHook::Detach() {
    if (!m_Impl)
        return true;
    if (CKRenderContext *context = m_Impl->Context()) {
        if (!context->RemoveRenderOptionsCallBack(Impl::RenderOptions, m_Impl.get()))
            return false;
        context->ChangeCurrentRenderOptions(
            m_Impl->SavedAdjustFov ? CK_RENDER_ADJUSTFOV : 0,
            m_Impl->SavedAdjustFov ? 0 : CK_RENDER_ADJUSTFOV);
    }
    if (Impl::Active == m_Impl.get())
        Impl::Active = nullptr;
    m_Impl->SkipPending = false;
    m_Impl->Owner = nullptr;
    m_Impl->ContextHandle = {};
    return true;
}

bool RenderHook::IsAttached() const {
    return m_Impl && Impl::Active == m_Impl.get() && m_Impl->Context();
}

bool RenderHook::IsSkipRenderAvailable() {
    return Impl::Active && Impl::Active->Context();
}

void RenderHook::SkipNextRender() {
    if (IsSkipRenderAvailable())
        Impl::Active->SkipPending = true;
}

void RenderHook::ResetPendingSkip(CKContext *context) {
    if (Impl::Active && Impl::Active->Owner == context)
        Impl::Active->SkipPending = false;
}

void RenderHook::EnableWidescreenFix(bool enable) {
    Impl::WidescreenFixEnabled = enable;
    if (IsWidescreenFixAvailable()) {
        Impl::Active->Context()->ChangeCurrentRenderOptions(
            enable ? CK_RENDER_ADJUSTFOV : 0, enable ? 0 : CK_RENDER_ADJUSTFOV);
    }
}

bool RenderHook::IsWidescreenFixAvailable() {
    return Impl::Active && Impl::Active->Context();
}

RenderHook::WidescreenStatus RenderHook::GetWidescreenStatus() {
    if (!Impl::WidescreenFixEnabled)
        return WidescreenStatus::Disabled;
    return IsWidescreenFixAvailable() ? WidescreenStatus::Active : WidescreenStatus::Pending;
}
