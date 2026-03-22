#include "BindTimer.h"

#include <cassert>
#include <string>

#include "bml_timer.h"
#include "../ScriptInstance.h"
#include "../ScriptInstanceManager.h"
#include "../ScriptExceptionHelper.h"
#include "../ModuleScope.h"

namespace BML::Scripting {

// TimerContext must be visible to both the deleter and the anonymous
// namespace trampoline, so it lives in the enclosing namespace.
struct TimerContext {
    ScriptInstanceManager *manager;
    BML_Mod mod_handle;
    std::string callback_name;
};

void ScriptInstance::TimerContextDeleter::operator()(void *p) const {
    delete static_cast<TimerContext *>(p);
}

namespace {

static ScriptInstanceManager *g_TimerManager = nullptr;

static void TimerTrampoline(BML_Context, BML_Timer, void *user_data) {
    auto *tc = static_cast<TimerContext *>(user_data);
    if (!tc || !tc->manager) return;

    auto *inst = tc->manager->FindByMod(tc->mod_handle);
    if (!inst || inst->state == ScriptInstance::State::Error) return;
    if (!inst->as_module) return;

    asIScriptFunction *fn = inst->as_module->GetFunctionByName(tc->callback_name.c_str());
    if (!fn) return;

    ScriptScope scope(inst);
    auto *ctx = tc->manager->AcquireContext();
    if (!ctx) return;

    ctx->Prepare(fn);
    TimeoutGuard timeout(inst->timeout_ms);
    ctx->SetLineCallback(asFUNCTION(TimeoutGuard::LineCallback),
                         &timeout, asCALL_CDECL);

    int r = asEXECUTION_ERROR;
    try { r = ctx->Execute(); } catch (...) {
        tc->manager->ReleaseContext(ctx);
        inst->state = ScriptInstance::State::Error;
        return;
    }
    if (r == asEXECUTION_EXCEPTION) {
        // D4: Log exception before releasing context
        LogScriptException(ctx, *inst);
        tc->manager->ReleaseContext(ctx);
        inst->state = ScriptInstance::State::Error;
        return;
    }
    tc->manager->ReleaseContext(ctx);
    if (r == asEXECUTION_ABORTED) {
        inst->state = ScriptInstance::State::Error;
    }
}

static int Script_SetTimer(int delayMs, const std::string &callbackName) {
    auto *inst = t_CurrentScript;
    if (!inst || !g_Builtins || !g_Builtins->Timer) return -1;
    auto *tc = new TimerContext{g_TimerManager, inst->mod_handle, callbackName};
    BML_Timer timer = nullptr;
    BML_Result r = g_Builtins->Timer->ScheduleOnce(
        inst->mod_handle, static_cast<uint32_t>(delayMs), TimerTrampoline, tc, &timer);
    if (r != BML_RESULT_OK) { delete tc; return -1; }
    inst->timer_contexts.emplace_back(tc);
    return static_cast<int>(reinterpret_cast<uintptr_t>(timer));
}

static int Script_SetInterval(int intervalMs, const std::string &callbackName) {
    auto *inst = t_CurrentScript;
    if (!inst || !g_Builtins || !g_Builtins->Timer) return -1;
    auto *tc = new TimerContext{g_TimerManager, inst->mod_handle, callbackName};
    BML_Timer timer = nullptr;
    BML_Result r = g_Builtins->Timer->ScheduleRepeat(
        inst->mod_handle, static_cast<uint32_t>(intervalMs), TimerTrampoline, tc, &timer);
    if (r != BML_RESULT_OK) { delete tc; return -1; }
    inst->timer_contexts.emplace_back(tc);
    return static_cast<int>(reinterpret_cast<uintptr_t>(timer));
}

static void Script_CancelTimer(int timerId) {
    auto *inst = t_CurrentScript;
    if (!inst || !g_Builtins || !g_Builtins->Timer || timerId < 0) return;
    g_Builtins->Timer->Cancel(
        inst->mod_handle,
        reinterpret_cast<BML_Timer>(static_cast<uintptr_t>(timerId)));
}

} // namespace

void RegisterTimerBindings(asIScriptEngine *engine, ScriptInstanceManager *manager) {
    g_TimerManager = manager;
    int r;
    r = engine->RegisterGlobalFunction("int bmlSetTimer(int delayMs, const string &in callbackName)",
        asFUNCTION(Script_SetTimer), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("int bmlSetInterval(int intervalMs, const string &in callbackName)",
        asFUNCTION(Script_SetInterval), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void bmlCancelTimer(int timerId)",
        asFUNCTION(Script_CancelTimer), asCALL_CDECL); assert(r >= 0);
}

} // namespace BML::Scripting
