/**
 * @file ObjectLoadMod.cpp
 * @brief BML ObjectLoad Module Entry Point
 * 
 * This module provides object loading hooks for BML,
 * intercepting the ObjectLoad behavior to notify mods.
 */

#include "bml_module.h"
#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"
#include "bml_topics.h"
#include "bml_virtools.h"
#include "bml_engine_events.h"
#include "ObjectLoadHook.h"

#include "CKContext.h"

static BML_Mod g_ModHandle = nullptr;
static BML_Subscription g_EngineInitSub = nullptr;
static BML_TopicId g_TopicEngineInit = 0;
static bool g_HookInitialized = false;

// Forward declaration
static void OnEngineInit(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage *msg, void *user_data);

namespace {
    void ReleaseSubscriptions() {
        if (g_EngineInitSub) {
            bmlImcUnsubscribe(g_EngineInitSub);
            g_EngineInitSub = nullptr;
        }
    }

    BML_Result ValidateAttachArgs(const BML_ModAttachArgs *args) {
        if (!args || args->struct_size != sizeof(BML_ModAttachArgs) ||
            args->mod == nullptr || args->get_proc == nullptr) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (args->api_version != BML_MOD_ENTRYPOINT_API_VERSION) {
            return BML_RESULT_VERSION_MISMATCH;
        }
        return BML_RESULT_OK;
    }

    BML_Result ValidateDetachArgs(const BML_ModDetachArgs *args) {
        if (!args || args->struct_size != sizeof(BML_ModDetachArgs) ||
            args->mod == nullptr) {
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

        bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, nullptr,
               "Initializing BML ObjectLoad Module");

        // Subscribe to engine init so we can grab CKContext when it's ready
        bmlImcGetTopicId(BML_TOPIC_ENGINE_INIT, &g_TopicEngineInit);
        result = bmlImcSubscribe(g_TopicEngineInit, OnEngineInit, nullptr, &g_EngineInitSub);
        if (result != BML_RESULT_OK) {
            bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, nullptr,
                   "Failed to subscribe to Engine/Init events");
            bmlUnloadAPI();
            g_ModHandle = nullptr;
            return result;
        }

        return BML_RESULT_OK;
    }

    BML_Result HandleDetach(const BML_ModDetachArgs *args) {
        BML_Result validation = ValidateDetachArgs(args);
        if (validation != BML_RESULT_OK) {
            return validation;
        }

        bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, nullptr,
               "Shutting down BML ObjectLoad Module");

        ReleaseSubscriptions();
        BML_ObjectLoad::ShutdownObjectLoadHook();
        g_HookInitialized = false;

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

static void OnEngineInit(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage *msg, void *user_data) {
    BML_UNUSED(ctx);
    BML_UNUSED(topic);
    BML_UNUSED(user_data);

    if (g_HookInitialized)
        return;

    if (!msg || !msg->data || msg->size < sizeof(BML_EngineInitEvent)) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, nullptr,
               "Engine/Init payload missing for ObjectLoad module");
        return;
    }

    const auto *payload = static_cast<const BML_EngineInitEvent *>(msg->data);
    if (payload->struct_size < sizeof(BML_EngineInitEvent) || !payload->context) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, nullptr,
               "Engine/Init payload invalid for ObjectLoad module");
        return;
    }

    if (BML_ObjectLoad::InitializeObjectLoadHook(payload->context)) {
        g_HookInitialized = true;
        bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, nullptr,
               "ObjectLoad hook initialized on Engine/Init event");
        ReleaseSubscriptions();
    } else {
        bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, nullptr,
               "ObjectLoad hook initialization failed, will retry on next Engine/Init event");
    }
}
