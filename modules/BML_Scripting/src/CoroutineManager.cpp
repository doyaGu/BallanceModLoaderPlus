#include "CoroutineManager.h"

#include "ScriptInstance.h"
#include "ScriptInstanceManager.h"
#include "ScriptExceptionHelper.h"
#include "ModuleScope.h"

namespace BML::Scripting {

CoroutineManager::CoroutineManager(asIScriptEngine *engine,
                                     ScriptInstanceManager *mgr)
    : m_Engine(engine), m_Manager(mgr) {}

CoroutineManager::~CoroutineManager() {
    for (auto &co : m_Suspended) {
        if (co->context) {
            co->context->Abort();
            co->context->Release();
        }
    }
}

void CoroutineManager::Tick() {
    ++m_Frame;

    auto now = std::chrono::steady_clock::now();

    // Partition into ready and still-waiting
    std::vector<std::unique_ptr<SuspendedCoroutine>> ready;
    std::vector<std::unique_ptr<SuspendedCoroutine>> remaining;

    for (auto &co : m_Suspended) {
        bool is_ready = false;
        if (co->use_time) {
            is_ready = (now >= co->resume_time);
        } else {
            is_ready = (m_Frame >= co->resume_frame);
        }

        if (is_ready)
            ready.push_back(std::move(co));
        else
            remaining.push_back(std::move(co));
    }
    m_Suspended = std::move(remaining);

    // Resume ready coroutines
    for (auto &co : ready) {
        if (!co->instance || co->instance->state == ScriptInstance::State::Error) {
            co->context->Abort();
            co->context->Release();
            continue;
        }

        ScriptScope scope(co->instance);

        // Set a fresh timeout for the resumed coroutine. The previous
        // SetLineCallback pointed at a stack-local TimeoutGuard that
        // has since been destroyed.
        TimeoutGuard timeout(co->instance->timeout_ms);
        co->context->SetLineCallback(asFUNCTION(TimeoutGuard::LineCallback),
                                     &timeout, asCALL_CDECL);

        int r = co->context->Execute();

        if (r == asEXECUTION_EXCEPTION) {
            // D4: Log exception before releasing context
            LogScriptException(co->context, *co->instance);
            co->instance->state = ScriptInstance::State::Error;
        } else if (r == asEXECUTION_ABORTED) {
            co->instance->state = ScriptInstance::State::Error;
        } else if (r != asEXECUTION_FINISHED && r != asEXECUTION_SUSPENDED) {
            co->instance->state = ScriptInstance::State::Error;
        }
        // Always release our reference. If the coroutine re-suspended,
        // the yield/wait binding already took a new reference.
        co->context->Release();
    }
}

void CoroutineManager::YieldCurrent() {
    asIScriptContext *ctx = asGetActiveContext();
    if (!ctx) return;

    auto co = std::make_unique<SuspendedCoroutine>();
    co->context = ctx;
    co->instance = t_CurrentScript;
    co->resume_frame = m_Frame + 1;
    co->use_time = false;

    ctx->AddRef(); // Keep alive while suspended
    ctx->Suspend();

    m_Suspended.push_back(std::move(co));
}

void CoroutineManager::WaitFrames(uint32_t frames) {
    asIScriptContext *ctx = asGetActiveContext();
    if (!ctx) return;

    auto co = std::make_unique<SuspendedCoroutine>();
    co->context = ctx;
    co->instance = t_CurrentScript;
    co->resume_frame = m_Frame + frames;
    co->use_time = false;

    ctx->AddRef();
    ctx->Suspend();

    m_Suspended.push_back(std::move(co));
}

void CoroutineManager::WaitSeconds(float seconds) {
    asIScriptContext *ctx = asGetActiveContext();
    if (!ctx) return;

    auto co = std::make_unique<SuspendedCoroutine>();
    co->context = ctx;
    co->instance = t_CurrentScript;
    co->resume_time = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(static_cast<long long>(seconds * 1000.0f));
    co->use_time = true;

    ctx->AddRef();
    ctx->Suspend();

    m_Suspended.push_back(std::move(co));
}

void CoroutineManager::CleanupForInstance(ScriptInstance *inst) {
    for (auto it = m_Suspended.begin(); it != m_Suspended.end();) {
        if ((*it)->instance == inst) {
            (*it)->context->Abort();
            (*it)->context->Release();
            it = m_Suspended.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace BML::Scripting
