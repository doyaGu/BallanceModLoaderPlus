#include "BindImc.h"

#include <cassert>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "bml_imc.h"
#include "bml_topics.h"
#include "../ModuleScope.h"
#include "../ScriptExceptionHelper.h"
#include "../ScriptInstance.h"
#include "../ScriptInstanceManager.h"

namespace BML::Scripting {

// SubContext must be visible to both the deleter and the trampoline, so it
// lives in the enclosing namespace before the deleter. The type must be
// complete because deleting an incomplete type with a non-trivial destructor
// is UB per [expr.delete]/5.
struct SubContext {
    ScriptInstanceManager *manager;
    BML_Mod mod_handle;
    std::string topic;
};

void ScriptInstance::SubContextDeleter::operator()(void *p) const {
    delete static_cast<SubContext *>(p);
}

static ScriptInstanceManager *g_ImcManager = nullptr;
static int g_StringTypeId = -1;

// IMC callback trampoline
static void ImcTrampoline(BML_Context, BML_TopicId,
                          const BML_ImcMessage *message, void *user_data) {
    auto *sub_ctx = static_cast<SubContext *>(user_data);
    if (!sub_ctx || !sub_ctx->manager) return;

    auto *inst = sub_ctx->manager->FindByMod(sub_ctx->mod_handle);
    if (!inst || inst->state == ScriptInstance::State::Error) return;

    auto it = inst->event_handlers.find(sub_ctx->topic);
    if (it == inst->event_handlers.end() || it->second.empty()) return;

    ScriptScope scope(inst);

    for (auto &handler : it->second) {
        if (!handler.fn) continue;
        auto *ctx = sub_ctx->manager->AcquireContext();
        if (!ctx) break;

        ctx->Prepare(handler.fn);
        TimeoutGuard timeout(inst->timeout_ms);
        ctx->SetLineCallback(asFUNCTION(TimeoutGuard::LineCallback),
                             &timeout, asCALL_CDECL);

        // Multi-type dispatch based on cached param_type (D7)
        if (handler.fn->GetParamCount() > 0 && message && message->data && message->size > 0) {
            int pt = handler.param_type;
            if (pt == asTYPEID_INT32 && message->size >= sizeof(int)) {
                ctx->SetArgDWord(0, *static_cast<const int *>(message->data));
            } else if (pt == asTYPEID_FLOAT && message->size >= sizeof(float)) {
                ctx->SetArgFloat(0, *static_cast<const float *>(message->data));
            } else if ((pt & ~asTYPEID_OBJHANDLE) == (g_StringTypeId & ~asTYPEID_OBJHANDLE)
                       && g_StringTypeId >= 0) {
                std::string s(static_cast<const char *>(message->data), message->size);
                ctx->SetArgObject(0, &s);
            } else if (pt == asTYPEID_INT32) {
                // size < sizeof(int), skip arg
            }
        }

        int r = asEXECUTION_ERROR;
        try {
            r = ctx->Execute();
        } catch (...) {
            sub_ctx->manager->ReleaseContext(ctx);
            inst->state = ScriptInstance::State::Error;
            break;
        }
        if (r == asEXECUTION_EXCEPTION) {
            LogScriptException(ctx, *inst);
            sub_ctx->manager->ReleaseContext(ctx);
            inst->state = ScriptInstance::State::Error;
            break;
        }
        sub_ctx->manager->ReleaseContext(ctx);
        if (r == asEXECUTION_ABORTED) {
            inst->state = ScriptInstance::State::Error;
            break;
        }
    }
}

// Script API implementations

static void Script_Subscribe(const std::string &topic, const std::string &callbackName) {
    auto *inst = t_CurrentScript;
    if (!inst || !g_Builtins || !g_Builtins->ImcBus || !g_ImcManager) return;

    asIScriptFunction *fn = inst->as_module
        ? inst->as_module->GetFunctionByName(callbackName.c_str()) : nullptr;
    if (!fn) return;

    // Cache the first parameter's type ID for multi-type dispatch
    ScriptInstance::EventHandler handler;
    handler.fn = fn;
    if (fn->GetParamCount() > 0) {
        int typeId = 0;
        fn->GetParam(0, &typeId);
        handler.param_type = typeId;
    }

    inst->event_handlers[topic].push_back(handler);

    // D1: Only create one IMC subscription per topic
    if (inst->topic_subs.find(topic) != inst->topic_subs.end())
        return; // Already subscribed at IMC level

    auto *raw_ctx = new SubContext{g_ImcManager, inst->mod_handle, topic};

    BML_TopicId topic_id = BML_TOPIC_ID_INVALID;
    if (g_Builtins->ImcBus->GetTopicId(topic.c_str(), &topic_id) != BML_RESULT_OK) {
        delete raw_ctx;
        return;
    }

    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
    BML_Subscription sub = nullptr;
    if (g_Builtins->ImcBus->SubscribeEx(topic_id, ImcTrampoline, raw_ctx, &opts, &sub) !=
        BML_RESULT_OK) {
        delete raw_ctx;
        return;
    }

    ScriptInstance::TopicSubscription ts;
    ts.ctx.reset(raw_ctx);
    ts.handle = sub;
    inst->topic_subs[topic] = std::move(ts);
}

static void Script_Unsubscribe(const std::string &topic, const std::string &callbackName) {
    auto *inst = t_CurrentScript;
    if (!inst) return;

    auto handlers_it = inst->event_handlers.find(topic);
    if (handlers_it == inst->event_handlers.end()) return;

    // Remove matching handler(s) by callback name
    auto &handlers = handlers_it->second;
    for (auto it = handlers.begin(); it != handlers.end();) {
        if (it->fn && std::string(it->fn->GetName()) == callbackName)
            it = handlers.erase(it);
        else
            ++it;
    }

    // If no handlers remain for this topic, tear down the IMC subscription
    if (handlers.empty()) {
        inst->event_handlers.erase(handlers_it);
        auto sub_it = inst->topic_subs.find(topic);
        if (sub_it != inst->topic_subs.end()) {
            if (g_Builtins && g_Builtins->ImcBus && sub_it->second.handle)
                g_Builtins->ImcBus->Unsubscribe(sub_it->second.handle);
            inst->topic_subs.erase(sub_it);
        }
    }
}

static void Script_Publish(const std::string &topic) {
    if (!g_Builtins || !g_Builtins->ImcBus) return;
    BML_TopicId id = BML_TOPIC_ID_INVALID;
    if (g_Builtins->ImcBus->GetTopicId(topic.c_str(), &id) != BML_RESULT_OK) return;
    g_Builtins->ImcBus->Publish(id, nullptr, 0);
}

static void Script_PublishString(const std::string &topic, const std::string &data) {
    if (!g_Builtins || !g_Builtins->ImcBus) return;
    BML_TopicId id = BML_TOPIC_ID_INVALID;
    if (g_Builtins->ImcBus->GetTopicId(topic.c_str(), &id) != BML_RESULT_OK) return;
    g_Builtins->ImcBus->Publish(id, data.c_str(), data.size());
}

static void Script_PublishInt(const std::string &topic, int value) {
    if (!g_Builtins || !g_Builtins->ImcBus) return;
    BML_TopicId id = BML_TOPIC_ID_INVALID;
    if (g_Builtins->ImcBus->GetTopicId(topic.c_str(), &id) != BML_RESULT_OK) return;
    g_Builtins->ImcBus->Publish(id, &value, sizeof(value));
}

static void Script_Print(const std::string &message) {
    if (!g_Builtins || !g_Builtins->ImcBus) return;
    BML_TopicId id = BML_TOPIC_ID_INVALID;
    if (g_Builtins->ImcBus->GetTopicId(BML_TOPIC_CONSOLE_OUTPUT, &id) != BML_RESULT_OK) return;

    struct {
        size_t struct_size;
        const char *message_utf8;
        uint32_t flags;
    } event;
    event.struct_size = sizeof(event);
    event.message_utf8 = message.c_str();
    event.flags = 0;
    g_Builtins->ImcBus->Publish(id, &event, sizeof(event));
}

void RegisterImcBindings(asIScriptEngine *engine, ScriptInstanceManager *manager) {
    g_ImcManager = manager;
    g_StringTypeId = engine->GetTypeIdByDecl("string");
    int r;
    r = engine->RegisterGlobalFunction("void bmlSubscribe(const string &in, const string &in)",
                                       asFUNCTION(Script_Subscribe), asCALL_CDECL);
    assert(r >= 0);
    r = engine->RegisterGlobalFunction("void bmlUnsubscribe(const string &in, const string &in)",
                                       asFUNCTION(Script_Unsubscribe), asCALL_CDECL);
    assert(r >= 0);
    r = engine->RegisterGlobalFunction("void bmlPublish(const string &in)",
                                       asFUNCTION(Script_Publish), asCALL_CDECL);
    assert(r >= 0);
    r = engine->RegisterGlobalFunction("void bmlPublish(const string &in, const string &in)",
                                       asFUNCTION(Script_PublishString), asCALL_CDECL);
    assert(r >= 0);
    r = engine->RegisterGlobalFunction("void bmlPublishInt(const string &in, int)",
                                       asFUNCTION(Script_PublishInt), asCALL_CDECL);
    assert(r >= 0);
    r = engine->RegisterGlobalFunction("void bmlPrint(const string &in)",
                                       asFUNCTION(Script_Print), asCALL_CDECL);
    assert(r >= 0);
}

} // namespace BML::Scripting
