/**
 * @file InputMod.cpp
 * @brief BML_Input Module Entry Point
 * 
 * This module:
 * - Hooks CKInputManager to intercept input
 * - Publishes input events via IMC (keyboard, mouse)
 * - Provides BML_EXT_Input extension for device blocking
 */

#include "bml_module.h"
#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"
#include "bml_virtools.h"
#include "bml_engine_events.h"
#include "bml_topics.h"

#include "InputHook.h"
#include "InputExtension.h"

#include "CKContext.h"
#include "CKInputManager.h"

// Defined in InputExtension.cpp
extern BML_InputExtension *GetInputExtension();

namespace {
    //-----------------------------------------------------------------------------
    // Module State
    //-----------------------------------------------------------------------------

    BML_Mod g_ModHandle = nullptr;
    bool g_HooksInitialized = false;

    // IMC subscriptions
    BML_Subscription g_TickSub = nullptr;
    BML_TopicId g_TopicTick = 0;
    BML_Subscription g_EngineInitSub = nullptr;
    BML_TopicId g_TopicEngineInit = 0;

    //-----------------------------------------------------------------------------
    // Forward Declarations
    //-----------------------------------------------------------------------------

    void OnTick(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage *msg, void *user_data);
    void OnEngineInit(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage *msg, void *user_data);

    //-----------------------------------------------------------------------------
    // Helper Functions
    //-----------------------------------------------------------------------------

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
        if (!args || args->struct_size != sizeof(BML_ModDetachArgs) || args->mod == nullptr) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (args->api_version != BML_MOD_ENTRYPOINT_API_VERSION) {
            return BML_RESULT_VERSION_MISMATCH;
        }
        return BML_RESULT_OK;
    }

    void ReleaseResources() {
        if (g_TickSub) {
            bmlImcUnsubscribe(g_TickSub);
            g_TickSub = nullptr;
        }
        if (g_EngineInitSub) {
            bmlImcUnsubscribe(g_EngineInitSub);
            g_EngineInitSub = nullptr;
        }

        if (g_HooksInitialized) {
            BML_Input::ShutdownInputHook();
            g_HooksInitialized = false;
        }
    }

    //-----------------------------------------------------------------------------
    // Module Entry Points
    //-----------------------------------------------------------------------------

    BML_Result HandleAttach(const BML_ModAttachArgs *args) {
        BML_Result validation = ValidateAttachArgs(args);
        if (validation != BML_RESULT_OK) {
            return validation;
        }

        g_ModHandle = args->mod;

        // Load BML API
        BML_Result result = bmlLoadAPI(args->get_proc, args->get_proc_by_id);
        if (result != BML_RESULT_OK) {
            return result;
        }

        BML_Context ctx = bmlGetGlobalContext();
        bmlLog(ctx, BML_LOG_INFO, "BML_Input",
               "Initializing BML Input Module v0.4.0");

        // Subscribe to Engine/Init to obtain CKContext when available
        bmlImcGetTopicId(BML_TOPIC_ENGINE_INIT, &g_TopicEngineInit);
        result = bmlImcSubscribe(g_TopicEngineInit, OnEngineInit, nullptr, &g_EngineInitSub);
        if (result != BML_RESULT_OK) {
            bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_Input",
                   "Failed to subscribe to Engine/Init events");
            bmlUnloadAPI();
            g_ModHandle = nullptr;
            return result;
        }

        // Subscribe to tick events to process input each frame
        bmlImcGetTopicId(BML_TOPIC_ENGINE_POST_PROCESS, &g_TopicTick);
        result = bmlImcSubscribe(g_TopicTick, OnTick, nullptr, &g_TickSub);
        if (result != BML_RESULT_OK) {
            bmlLog(ctx, BML_LOG_WARN, "BML_Input",
                   "Failed to subscribe to tick events (non-fatal)");
        }

        // Register extension API
        BML_InputExtension *input_ext = GetInputExtension();
        BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
        desc.name = "BML_EXT_Input";
        desc.version = bmlMakeVersion(1, 0, 0);
        desc.api_table = input_ext;
        desc.api_size = sizeof(BML_InputExtension);
        desc.description = "BML Input System Extension";

        result = bmlExtensionRegister(&desc);
        if (result != BML_RESULT_OK) {
            bmlLog(ctx, BML_LOG_WARN, "BML_Input",
                   "Failed to register BML_EXT_Input extension (non-fatal)");
        } else {
            bmlLog(ctx, BML_LOG_INFO, "BML_Input", "Registered BML_EXT_Input v1.0 extension");
        }

         bmlLog(ctx, BML_LOG_INFO, "BML_Input",
             "BML Input Module initialized - waiting for Engine/Init");
        return BML_RESULT_OK;
    }

    BML_Result HandleDetach(const BML_ModDetachArgs *args) {
        BML_Result validation = ValidateDetachArgs(args);
        if (validation != BML_RESULT_OK) {
            return validation;
        }

        bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Input",
               "Shutting down BML Input Module");

        bmlExtensionUnregister("BML_EXT_Input");
        ReleaseResources();
        bmlUnloadAPI();
        g_ModHandle = nullptr;

        return BML_RESULT_OK;
    }

    //-----------------------------------------------------------------------------
    // Event Handlers
    //-----------------------------------------------------------------------------

    void OnTick(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage *msg, void *user_data) {
        (void) ctx;
        (void) topic;
        (void) msg;
        (void) user_data;

        // Process input and publish events
        BML_Input::ProcessInput();
    }

    void OnEngineInit(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage *msg, void *user_data) {
        BML_UNUSED(ctx);
        BML_UNUSED(topic);
        BML_UNUSED(user_data);

        if (g_HooksInitialized)
            return;

        if (!msg || !msg->data || msg->size < sizeof(BML_EngineInitEvent)) {
            bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, "BML_Input",
                   "Engine/Init payload missing for input module");
            return;
        }

        const auto *payload = static_cast<const BML_EngineInitEvent *>(msg->data);
        if (payload->struct_size < sizeof(BML_EngineInitEvent) || !payload->context) {
            bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, "BML_Input",
                   "Engine/Init payload invalid for input module");
            return;
        }

        CKInputManager *inputManager = static_cast<CKInputManager *>(
            payload->context->GetManagerByGuid(INPUT_MANAGER_GUID));
        if (!inputManager) {
            bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, "BML_Input",
                   "CKInputManager not available yet - retrying next Engine/Init");
            return;
        }

        if (BML_Input::InitInputHook(inputManager)) {
            g_HooksInitialized = true;
            bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Input",
                   "Input hooks initialized on Engine/Init event");
            if (g_EngineInitSub) {
                bmlImcUnsubscribe(g_EngineInitSub);
                g_EngineInitSub = nullptr;
            }
        } else {
            bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_Input",
                   "Input hook initialization failed; will retry on next Engine/Init event");
        }
    }
} // namespace

//-----------------------------------------------------------------------------
// Module Entrypoint
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
