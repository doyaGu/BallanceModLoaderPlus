#include "ScriptInstanceManager.h"
#include "CoroutineManager.h"
#include "ScriptExceptionHelper.h"

#include <filesystem>
#include <string>

#include "bml_module_runtime.h"
#include "ModuleScope.h"
#include "BytecodeCache.h"
#include "add_on/scriptbuilder/scriptbuilder.h"

namespace BML::Scripting {

// Helper: unsubscribe and clear all IMC topic subscriptions for an instance.
static void CleanupTopicSubs(ScriptInstance &inst) {
    for (auto &[topic, ts] : inst.topic_subs) {
        if (g_Services && g_Services->ImcBus && ts.handle)
            g_Services->ImcBus->Unsubscribe(ts.handle);
    }
    inst.topic_subs.clear();
    inst.event_handlers.clear();
}

ScriptInstanceManager::ScriptInstanceManager(asIScriptEngine *engine)
    : m_Engine(engine) {}

ScriptInstanceManager::~ScriptInstanceManager() {
    // Detach any instances that weren't properly shut down via RuntimeProvider.
    // This prevents dangling IMC subscriptions pointing to freed SubContexts.
    std::vector<BML_Mod> remaining;
    remaining.reserve(m_Instances.size());
    for (auto &[mod, inst] : m_Instances)
        remaining.push_back(mod);
    for (auto mod : remaining)
        Detach(mod);

    for (auto *ctx : m_ContextPool)
        ctx->Release();
    m_ContextPool.clear();
}

asIScriptContext *ScriptInstanceManager::AcquireContext() {
    if (!m_ContextPool.empty()) {
        auto *ctx = m_ContextPool.back();
        m_ContextPool.pop_back();
        return ctx;
    }
    return m_Engine ? m_Engine->CreateContext() : nullptr;
}

void ScriptInstanceManager::ReleaseContext(asIScriptContext *ctx) {
    if (!ctx) return;
    // If the context was suspended (coroutine took ownership via AddRef),
    // don't pool it - just release our reference.
    if (ctx->GetState() == asEXECUTION_SUSPENDED) {
        ctx->Release();
        return;
    }

    ctx->ClearLineCallback();
    ctx->Unprepare();
    m_ContextPool.push_back(ctx);
}

BML_Result ScriptInstanceManager::CompileAndAttach(
        BML_Mod mod, const BML_Services *services,
        const char *entry_path, const char *module_dir) {
    if (!mod || !services || !entry_path || !module_dir)
        return BML_RESULT_INVALID_ARGUMENT;
    if (!m_Engine)
        return BML_RESULT_NOT_INITIALIZED;

    auto inst = std::make_unique<ScriptInstance>();
    inst->mod_handle = mod;
    inst->services = services;
    inst->entry_file = std::filesystem::path(entry_path);
    inst->module_dir = std::filesystem::path(module_dir);

    // Get mod id through the runtime interface (BML_Mod is opaque to modules)
    const char *id_str = nullptr;
    if (g_Services && g_Services->Module &&
        g_Services->Module->GetModId(mod, &id_str) == BML_RESULT_OK && id_str) {
        inst->mod_id = id_str;
    } else {
        inst->mod_id = inst->entry_file.stem().string();
    }

    // Parse [scripting] config from manifest custom fields
    if (g_Services && g_Services->Module) {
        int64_t timeout_val = 0;
        if (g_Services->Module->GetManifestInt(mod, "scripting.timeout_ms", &timeout_val) == BML_RESULT_OK
            && timeout_val > 0) {
            inst->timeout_ms = static_cast<uint32_t>(timeout_val);
        }
    }

    // Compile
    if (!CompileScript(*inst))
        return BML_RESULT_FAIL;

    // Resolve lifecycle callbacks
    ResolveCallbacks(*inst);
    inst->state = ScriptInstance::State::Compiled;

    // Invoke OnAttach
    BML_Result result = BML_RESULT_OK;
    if (inst->fn_OnAttach) {
        result = InvokeLifecycle(*inst, inst->fn_OnAttach);
    }

    if (result != BML_RESULT_OK) {
        // Clean up any coroutines or timers spawned during the failed
        // OnAttach (e.g. script yielded, causing SUSPENDED -> FAIL).
        // The ScriptInstance is about to be destroyed; dangling
        // pointers in CoroutineManager would be use-after-free.
        if (m_Coroutines) {
            m_Coroutines->CleanupForInstance(inst.get());
        }
        CleanupTopicSubs(*inst);
        inst->timer_contexts.clear();
        m_Engine->DiscardModule(inst->mod_id.c_str());
        inst->as_module = nullptr;
        return result;
    }

    inst->state = ScriptInstance::State::Attached;
    ScriptInstance *stored = inst.get();
    m_Instances[mod] = std::move(inst);

    result = RunInitCallback(*stored, true);
    if (result != BML_RESULT_OK) {
        Detach(mod);
        return result;
    }

    return BML_RESULT_OK;
}

BML_Result ScriptInstanceManager::PrepareDetach(BML_Mod mod) {
    // PrepareDetach is called during normal module shutdown.
    // Scripts don't gate normal shutdown --always allow.
    // OnPrepareReload is only consulted during Reload().
    (void)mod;
    return BML_RESULT_OK;
}

BML_Result ScriptInstanceManager::Detach(BML_Mod mod) {
    auto it = m_Instances.find(mod);
    if (it == m_Instances.end())
        return BML_RESULT_OK;

    auto &inst = *it->second;

    // Call OnDetach
    if (inst.fn_OnDetach && inst.state != ScriptInstance::State::Error) {
        InvokeLifecycle(inst, inst.fn_OnDetach);
    }

    // Clean up coroutines for this instance
    if (m_Coroutines) {
        m_Coroutines->CleanupForInstance(&inst);
    }

    // Unsubscribe all IMC topics
    CleanupTopicSubs(inst);
    inst.timer_contexts.clear();

    // Release AS resources
    if (inst.as_module) {
        m_Engine->DiscardModule(inst.mod_id.c_str());
        inst.as_module = nullptr;
    }

    inst.state = ScriptInstance::State::Unloaded;
    m_Instances.erase(it);
    return BML_RESULT_OK;
}

BML_Result ScriptInstanceManager::Reload(BML_Mod mod) {
    auto *inst = FindByMod(mod);
    if (!inst)
        return BML_RESULT_NOT_FOUND;

    // Try compile to temp module first (validation)
    std::string temp_name = inst->mod_id + "_reload_tmp";
    CScriptBuilder builder;
    int r = builder.StartNewModule(m_Engine, temp_name.c_str());
    if (r < 0) return BML_RESULT_FAIL;

    r = builder.AddSectionFromFile(inst->entry_file.string().c_str());
    if (r < 0) {
        m_Engine->DiscardModule(temp_name.c_str());
        return BML_RESULT_FAIL;
    }

    r = builder.BuildModule();
    if (r < 0) {
        m_Engine->DiscardModule(temp_name.c_str());
        return BML_RESULT_FAIL; // Compile error; old version stays
    }

    // Validation passed, discard temp
    m_Engine->DiscardModule(temp_name.c_str());

    // D5: Notify script before tearing down old state
    if (inst->fn_OnPrepareReload) {
        ScriptScope scope(inst);
        BML_Result prepare_result = InvokeCallback(*inst, inst->fn_OnPrepareReload, false);
        if (prepare_result != BML_RESULT_OK) {
            return prepare_result;
        }
    }

    // Detach old
    if (inst->fn_OnDetach) {
        InvokeLifecycle(*inst, inst->fn_OnDetach);
    }

    // Clean up coroutines for this instance
    if (m_Coroutines) {
        m_Coroutines->CleanupForInstance(inst);
    }

    // Clean up all kernel state (IMC subscriptions, timers, hooks,
    // interfaces, leases) attributed to this mod handle so the new
    // script version starts with a clean slate. This must happen
    // before freeing timer/sub contexts so that the timer entries
    // referencing those user_data pointers are cancelled first.
    if (inst->services && inst->services->HostRuntime &&
        inst->services->HostRuntime->CleanupModuleState) {
        inst->services->HostRuntime->CleanupModuleState(
            inst->services->HostRuntime->Context,
            inst->mod_handle);
    }

    // Unsubscribe all IMC topics for this instance
    CleanupTopicSubs(*inst);
    inst->timer_contexts.clear();

    // Discard old module
    m_Engine->DiscardModule(inst->mod_id.c_str());
    inst->as_module = nullptr;
    inst->fn_OnAttach = nullptr;
    inst->fn_OnDetach = nullptr;
    inst->fn_OnInit = nullptr;
    inst->fn_OnPrepareReload = nullptr;

    // Recompile for real
    if (!CompileScript(*inst)) {
        inst->state = ScriptInstance::State::Error;
        return BML_RESULT_FAIL;
    }

    ResolveCallbacks(*inst);

    // Re-attach
    BML_Result result = BML_RESULT_OK;
    if (inst->fn_OnAttach) {
        result = InvokeLifecycle(*inst, inst->fn_OnAttach);
    }

    if (result != BML_RESULT_OK) {
        if (m_Coroutines)
            m_Coroutines->CleanupForInstance(inst);
        CleanupTopicSubs(*inst);
        inst->timer_contexts.clear();
        inst->state = ScriptInstance::State::Error;
        return result;
    }

    inst->state = ScriptInstance::State::Attached;
    result = RunInitCallback(*inst, true);
    if (result != BML_RESULT_OK) {
        return result;
    }

    return BML_RESULT_OK;
}

ScriptInstance *ScriptInstanceManager::FindByMod(BML_Mod mod) {
    auto it = m_Instances.find(mod);
    return (it != m_Instances.end()) ? it->second.get() : nullptr;
}

bool ScriptInstanceManager::CompileScript(ScriptInstance &inst) {
    // Try loading from bytecode cache first
    asIScriptModule *cached = nullptr;
    if (TryLoadFromCache(m_Engine, inst.mod_id.c_str(), inst.entry_file, &cached)) {
        inst.as_module = cached;
        return true;
    }

    // Cache miss or invalid: compile from source
    CScriptBuilder builder;
    int r = builder.StartNewModule(m_Engine, inst.mod_id.c_str());
    if (r < 0) return false;

    r = builder.AddSectionFromFile(inst.entry_file.string().c_str());
    if (r < 0) {
        // D3: discard module after StartNewModule succeeded
        m_Engine->DiscardModule(inst.mod_id.c_str());
        return false;
    }

    r = builder.BuildModule();
    if (r < 0) {
        // D3: discard module after StartNewModule succeeded
        m_Engine->DiscardModule(inst.mod_id.c_str());
        return false;
    }

    inst.as_module = m_Engine->GetModule(inst.mod_id.c_str());
    if (!inst.as_module) return false;

    // D2: Collect all section paths for dependency-aware cache
    std::vector<std::string> section_paths;
    unsigned int section_count = builder.GetSectionCount();
    section_paths.reserve(section_count);
    for (unsigned int i = 0; i < section_count; ++i) {
        section_paths.push_back(builder.GetSectionName(i));
    }

    // Save bytecode cache for next load
    SaveToCache(inst.as_module, inst.entry_file, section_paths);
    return true;
}

void ScriptInstanceManager::ResolveCallbacks(ScriptInstance &inst) {
    if (!inst.as_module) return;
    inst.fn_OnAttach = inst.as_module->GetFunctionByName("OnAttach");
    inst.fn_OnDetach = inst.as_module->GetFunctionByName("OnDetach");
    inst.fn_OnInit = inst.as_module->GetFunctionByName("OnInit");
    inst.fn_OnPrepareReload = inst.as_module->GetFunctionByName("OnPrepareReload");
}

BML_Result ScriptInstanceManager::InvokeCallback(
        ScriptInstance &inst, asIScriptFunction *fn, bool markErrorOnFailure) {
    if (!fn)
        return BML_RESULT_OK;

    auto *ctx = AcquireContext();
    if (!ctx) return BML_RESULT_OUT_OF_MEMORY;

    ctx->Prepare(fn);

    TimeoutGuard timeout(inst.timeout_ms);
    ctx->SetLineCallback(asFUNCTION(TimeoutGuard::LineCallback),
                         &timeout, asCALL_CDECL);

    int r = asEXECUTION_ERROR;
    try {
        r = ctx->Execute();
    } catch (...) {
        ReleaseContext(ctx);
        if (markErrorOnFailure) {
            inst.state = ScriptInstance::State::Error;
        }
        return BML_RESULT_INTERNAL_ERROR;
    }

    if (r == asEXECUTION_EXCEPTION) {
        if (markErrorOnFailure) {
            // D4: Log exception before releasing context
            LogScriptException(ctx, inst);
        }
        ReleaseContext(ctx);
        if (markErrorOnFailure) {
            inst.state = ScriptInstance::State::Error;
        }
        return BML_RESULT_INTERNAL_ERROR;
    }

    ReleaseContext(ctx);

    if (r == asEXECUTION_ABORTED) {
        if (markErrorOnFailure) {
            inst.state = ScriptInstance::State::Error;
        }
        return BML_RESULT_TIMEOUT;
    }

    return (r == asEXECUTION_FINISHED) ? BML_RESULT_OK : BML_RESULT_FAIL;
}

BML_Result ScriptInstanceManager::InvokeLifecycle(
        ScriptInstance &inst, asIScriptFunction *fn) {
    if (!fn) {
        return BML_RESULT_OK;
    }

    ScriptScope scope(&inst);
    return InvokeCallback(inst, fn);
}

BML_Result ScriptInstanceManager::RunInitCallback(
        ScriptInstance &inst, bool fail_if_ready) {
    if (!m_VirtoolsReady || !inst.fn_OnInit) {
        return BML_RESULT_OK;
    }

    BML_Result result = InvokeLifecycle(inst, inst.fn_OnInit);
    if (result == BML_RESULT_OK) {
        inst.state = ScriptInstance::State::InitDone;
        return BML_RESULT_OK;
    }

    LogInitFailure(inst, result);
    return fail_if_ready ? result : BML_RESULT_OK;
}

void ScriptInstanceManager::LogInitFailure(
        const ScriptInstance &inst, BML_Result result) const {
    if (!g_Services || !g_Services->Logging || !g_Services->Logging->Log) {
        return;
    }

    g_Services->Logging->Log(inst.mod_handle, BML_LOG_WARN, "script",
                                  "[%s] OnInit failed with result %d",
                                  inst.mod_id.c_str(), static_cast<int>(result));
}

// Common execute path for public dispatch functions.
// `setupArgs` is called after Prepare() to set arguments on the context.
BML_Result ScriptInstanceManager::InvokeExternal(
        ScriptInstance &inst, asIScriptFunction *fn,
        const std::function<void(asIScriptContext *)> &setupArgs) {
    ScriptScope scope(&inst);
    auto *ctx = AcquireContext();
    if (!ctx) return BML_RESULT_OUT_OF_MEMORY;

    ctx->Prepare(fn);
    if (setupArgs)
        setupArgs(ctx);

    TimeoutGuard timeout(inst.timeout_ms);
    ctx->SetLineCallback(asFUNCTION(TimeoutGuard::LineCallback),
                         &timeout, asCALL_CDECL);

    int r = asEXECUTION_ERROR;
    try { r = ctx->Execute(); } catch (...) {
        ReleaseContext(ctx);
        inst.state = ScriptInstance::State::Error;
        return BML_RESULT_INTERNAL_ERROR;
    }
    if (r == asEXECUTION_EXCEPTION) {
        LogScriptException(ctx, inst);
        ReleaseContext(ctx);
        inst.state = ScriptInstance::State::Error;
        return BML_RESULT_INTERNAL_ERROR;
    }
    ReleaseContext(ctx);
    if (r == asEXECUTION_ABORTED) {
        inst.state = ScriptInstance::State::Error;
        return BML_RESULT_TIMEOUT;
    }
    return (r == asEXECUTION_FINISHED) ? BML_RESULT_OK : BML_RESULT_FAIL;
}

BML_Result ScriptInstanceManager::InvokeByName(BML_Mod mod, const char *func_name) {
    auto *inst = FindByMod(mod);
    if (!inst || !inst->as_module || !func_name) return BML_RESULT_INVALID_ARGUMENT;
    auto *fn = inst->as_module->GetFunctionByName(func_name);
    if (!fn) return BML_RESULT_NOT_FOUND;
    return InvokeExternal(*inst, fn, nullptr);
}

BML_Result ScriptInstanceManager::InvokeByNameInt(BML_Mod mod, const char *func_name, int arg) {
    auto *inst = FindByMod(mod);
    if (!inst || !inst->as_module || !func_name) return BML_RESULT_INVALID_ARGUMENT;
    auto *fn = inst->as_module->GetFunctionByName(func_name);
    if (!fn) return BML_RESULT_NOT_FOUND;
    return InvokeExternal(*inst, fn, [fn, arg](asIScriptContext *ctx) {
        if (fn->GetParamCount() > 0)
            ctx->SetArgDWord(0, static_cast<asDWORD>(arg));
    });
}

BML_Result ScriptInstanceManager::InvokeByNameString(BML_Mod mod, const char *func_name, const char *arg) {
    auto *inst = FindByMod(mod);
    if (!inst || !inst->as_module || !func_name) return BML_RESULT_INVALID_ARGUMENT;
    auto *fn = inst->as_module->GetFunctionByName(func_name);
    if (!fn) return BML_RESULT_NOT_FOUND;
    return InvokeExternal(*inst, fn, [fn, arg](asIScriptContext *ctx) {
        if (fn->GetParamCount() > 0 && arg) {
            std::string s(arg);
            ctx->SetArgObject(0, &s);
        }
    });
}

void *ScriptInstanceManager::GetModulePtr(BML_Mod mod) {
    auto *inst = FindByMod(mod);
    return inst ? inst->as_module : nullptr;
}

bool ScriptInstanceManager::IsScript(BML_Mod mod) {
    return FindByMod(mod) != nullptr;
}

void ScriptInstanceManager::InitAll() {
    for (auto &[mod, inst] : m_Instances) {
        if (inst->state == ScriptInstance::State::Attached && inst->fn_OnInit) {
            RunInitCallback(*inst, false);
        }
    }
}

BML_Result ScriptInstanceManager::ReloadById(const std::string &mod_id) {
    for (auto &[mod, inst] : m_Instances) {
        if (inst->mod_id == mod_id) {
            return Reload(mod);
        }
    }
    return BML_RESULT_NOT_FOUND;
}

void ScriptInstanceManager::ReloadAll() {
    // Collect keys first to avoid iterator invalidation
    std::vector<BML_Mod> mods;
    mods.reserve(m_Instances.size());
    for (auto &[mod, inst] : m_Instances) {
        mods.push_back(mod);
    }
    for (auto *mod : mods) {
        Reload(mod);
    }
}

} // namespace BML::Scripting
