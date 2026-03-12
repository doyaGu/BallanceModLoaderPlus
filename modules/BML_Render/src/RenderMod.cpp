#include "bml_module.h"
#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"
#include "bml_topics.h"
#include "RenderMod.h"
#include "RenderHook.h"

// Forward declarations
static void OnEngineInit(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage* msg, void* user_data);
static void OnEngineShutdown(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage* msg, void* user_data);

static BML_Mod g_ModHandle = nullptr;
static BML_Subscription g_EngineInitSub = nullptr;
static BML_Subscription g_EngineShutdownSub = nullptr;

// Cached topic IDs
static BML_TopicId g_TopicEngineInit = 0;
static BML_TopicId g_TopicEngineShutdown = 0;

namespace {

void ReleaseSubscriptions() {
    if (g_EngineInitSub) {
        bmlImcUnsubscribe(g_EngineInitSub);
        g_EngineInitSub = nullptr;
    }
    if (g_EngineShutdownSub) {
        bmlImcUnsubscribe(g_EngineShutdownSub);
        g_EngineShutdownSub = nullptr;
    }
}

BML_Result ValidateAttachArgs(const BML_ModAttachArgs* args) {
    if (!args || args->struct_size != sizeof(BML_ModAttachArgs) || args->mod == nullptr || args->get_proc == nullptr) {
        return BML_RESULT_INVALID_ARGUMENT;
    }
    if (args->api_version != BML_MOD_ENTRYPOINT_API_VERSION) {
        return BML_RESULT_VERSION_MISMATCH;
    }
    return BML_RESULT_OK;
}

BML_Result ValidateDetachArgs(const BML_ModDetachArgs* args) {
    if (!args || args->struct_size != sizeof(BML_ModDetachArgs) || args->mod == nullptr) {
        return BML_RESULT_INVALID_ARGUMENT;
    }
    if (args->api_version != BML_MOD_ENTRYPOINT_API_VERSION) {
        return BML_RESULT_VERSION_MISMATCH;
    }
    return BML_RESULT_OK;
}

BML_Result HandleAttach(const BML_ModAttachArgs* args) {
    BML_Result validation = ValidateAttachArgs(args);
    if (validation != BML_RESULT_OK) {
        return validation;
    }

    g_ModHandle = args->mod;

    BML_Result result = bmlLoadAPI(args->get_proc, args->get_proc_by_id);
    if (result != BML_RESULT_OK) {
        return result;
    }

    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Render", "Initializing BML Render Module v0.4.0");

    // Get topic IDs for engine lifecycle events
    bmlImcGetTopicId(BML_TOPIC_ENGINE_INIT, &g_TopicEngineInit);
    bmlImcGetTopicId(BML_TOPIC_ENGINE_END, &g_TopicEngineShutdown);

    // Subscribe to engine lifecycle events
    result = bmlImcSubscribe(g_TopicEngineInit, OnEngineInit, nullptr, &g_EngineInitSub);
    if (result != BML_RESULT_OK) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_Render", "Failed to subscribe to Engine/Init");
        bmlUnloadAPI();
        return result;
    }

    result = bmlImcSubscribe(g_TopicEngineShutdown, OnEngineShutdown, nullptr, &g_EngineShutdownSub);
    if (result != BML_RESULT_OK) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_Render", "Failed to subscribe to Engine/Shutdown");
        ReleaseSubscriptions();
        bmlUnloadAPI();
        return result;
    }

    // Initialize render hooks immediately (CK2_3D.dll should be loaded)
    if (!BML_Render::InitRenderHook()) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, "BML_Render", 
               "Render hooks not initialized (CK2_3D.dll not ready), will retry on Engine/Init");
    }

    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Render", "BML Render Module initialized successfully");
    return BML_RESULT_OK;
}

BML_Result HandleDetach(const BML_ModDetachArgs* args) {
    BML_Result validation = ValidateDetachArgs(args);
    if (validation != BML_RESULT_OK) {
        return validation;
    }

    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Render", "Shutting down BML Render Module");

    // Shutdown render hooks
    BML_Render::ShutdownRenderHook();

    ReleaseSubscriptions();
    bmlUnloadAPI();
    g_ModHandle = nullptr;
    return BML_RESULT_OK;
}

} // namespace

BML_MODULE_ENTRY BML_Result BML_ModEntrypoint(BML_ModEntrypointCommand cmd, void* data) {
    switch (cmd) {
    case BML_MOD_ENTRYPOINT_ATTACH:
        return HandleAttach(static_cast<const BML_ModAttachArgs*>(data));
    case BML_MOD_ENTRYPOINT_DETACH:
        return HandleDetach(static_cast<const BML_ModDetachArgs*>(data));
    default:
        return BML_RESULT_INVALID_ARGUMENT;
    }
}

// Engine lifecycle event handlers
static void OnEngineInit(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage* msg, void* user_data) {
    (void)ctx; (void)topic; (void)msg; (void)user_data;
    
    // Try to initialize render hooks if not already done
    if (!BML_Render::IsRenderHookActive()) {
        if (BML_Render::InitRenderHook()) {
            bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Render", 
                   "Render hooks initialized on Engine/Init event");
        }
    }
}

static void OnEngineShutdown(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage* msg, void* user_data) {
    (void)ctx; (void)topic; (void)msg; (void)user_data;
    
    // Shutdown render hooks before engine shutdown
    BML_Render::ShutdownRenderHook();
}
