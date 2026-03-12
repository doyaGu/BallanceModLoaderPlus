#include "bml_module.h"
#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"
#include "EventTopics.h"
#include "EventHook.h"

static BML_Mod g_ModHandle = nullptr;
static BML_Subscription g_ScriptLoadSub = nullptr;
static BML_TopicId g_TopicScriptLoad = 0;

// Forward declaration for script load handler
static void OnScriptLoad(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage *msg, void *user_data);

namespace {
    void ReleaseSubscriptions() {
        if (g_ScriptLoadSub) {
            bmlImcUnsubscribe(g_ScriptLoadSub);
            g_ScriptLoadSub = nullptr;
        }
    }

    BML_Result ValidateAttachArgs(const BML_ModAttachArgs *args) {
        if (!args || args->struct_size != sizeof(BML_ModAttachArgs) || args->mod == nullptr || args->get_proc ==
            nullptr) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (args->api_version != BML_MOD_ENTRYPOINT_API_VERSION) {
            return BML_RESULT_VERSION_MISMATCH;
        }
        return BML_RESULT_OK;
    }

    BML_Result ValidateDetachArgs(const BML_ModDetachArgs *args) {
        if (!args || args->struct_size != sizeof(BML_ModDetachArgs) || args->mod == nullptr) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (args->api_version != BML_MOD_ENTRYPOINT_API_VERSION) {
            return BML_RESULT_VERSION_MISMATCH;
        }
        return BML_RESULT_OK;
    }

    BML_Result HandleAttach(const BML_ModAttachArgs *args) {
        BML_Result validation = ValidateAttachArgs(args);
        if (validation != BML_RESULT_OK) {
            return validation;
        }

        g_ModHandle = args->mod;

        BML_Result result = bmlLoadAPI(args->get_proc, args->get_proc_by_id);
        if (result != BML_RESULT_OK) {
            return result;
        }

        bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Event", "Initializing BML Event Module v0.4.0");

        // Subscribe to script load events to hook into game scripts
        bmlImcGetTopicId("ObjectLoad/LoadScript", &g_TopicScriptLoad);
        result = bmlImcSubscribe(g_TopicScriptLoad, OnScriptLoad, nullptr, &g_ScriptLoadSub);
        if (result != BML_RESULT_OK) {
            bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, "BML_Event",
                   "Failed to subscribe to LoadScript, event hooks may not work");
        }

        bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Event",
               "BML Event Module initialized - waiting for script load events");

        return BML_RESULT_OK;
    }

    BML_Result HandleDetach(const BML_ModDetachArgs *args) {
        BML_Result validation = ValidateDetachArgs(args);
        if (validation != BML_RESULT_OK) {
            return validation;
        }

        bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Event", "Shutting down BML Event Module");

        BML_Event::ShutdownEventHooks();
        ReleaseSubscriptions();
        bmlUnloadAPI();
        g_ModHandle = nullptr;
        return BML_RESULT_OK;
    }
} // anonymous namespace

//-----------------------------------------------------------------------------
// Module Entry Point
//-----------------------------------------------------------------------------

BML_MODULE_ENTRY BML_Result BML_ModEntrypoint(BML_ModEntrypointCommand cmd, void *data) {
    switch (cmd) {
    case BML_MOD_ENTRYPOINT_ATTACH:
        return HandleAttach(static_cast<const BML_ModAttachArgs *>(data));
    case BML_MOD_ENTRYPOINT_DETACH:
        return HandleDetach(static_cast<const BML_ModDetachArgs *>(data));
    default:
        return BML_RESULT_INVALID_ARGUMENT;
    }
}

//-----------------------------------------------------------------------------
// Script Load Event Handler
//-----------------------------------------------------------------------------

// Script load message payload structure
struct ScriptLoadPayload {
    const char *scriptName;
    CKBehavior *script;
    CKContext *context;
};

static void OnScriptLoad(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage *msg, void *user_data) {
    (void) ctx;
    (void) topic;
    (void) user_data;

    if (!msg || !msg->data || msg->size < sizeof(ScriptLoadPayload)) {
        return;
    }

    auto *payload = static_cast<const ScriptLoadPayload *>(msg->data);
    if (!payload->script || !payload->context) {
        return;
    }

    // Initialize event hooks if not already done
    BML_Event::InitEventHooks(payload->context);

    const char *name = payload->scriptName;
    CKBehavior *script = payload->script;

    // Hook into known game scripts
    if (strcmp(name, "Level Manager") == 0 || strcmp(name, "Gameplay.nmo Level Manager") == 0) {
        BML_Event::RegisterBaseEventHandler(script);
    } else if (strcmp(name, "Gameplay_Ingame") == 0) {
        BML_Event::RegisterGameplayIngame(script);
    } else if (strcmp(name, "Gameplay_Energy") == 0) {
        BML_Event::RegisterGameplayEnergy(script);
    } else if (strcmp(name, "Gameplay_Events") == 0) {
        BML_Event::RegisterGameplayEvents(script);
    }
}
