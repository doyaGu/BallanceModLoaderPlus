/**
 * @file UIMod.cpp
 * @brief BML_UI Module Entry Point and API Implementation
 * 
 * This module provides the core UI infrastructure for BML:
 * - ImGui overlay lifecycle management (internal)
 * - ANSI palette and text rendering (public API)
 * - Win32 input hooks for ImGui (internal)
 * 
 * ImGui is exported directly from this DLL.
 * Public APIs are registered through BML's API registry for ABI stability.
 */

#include "bml_module.h"
#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"
#include "bml_ui.h"
#include "bml_extension.h"
#include "bml_engine_events.h"
#include "bml_topics.h"

#include "Overlay.h"
#include "AnsiPalette.h"
#include "AnsiText.h"

#include "CKContext.h"

//=============================================================================
// Module State
//=============================================================================

static BML_Mod g_ModHandle = nullptr;
static BML_Subscription g_EngineInitSub = nullptr;
static BML_Subscription g_EngineShutdownSub = nullptr;
static BML_Subscription g_PreProcessSub = nullptr;
static BML_Subscription g_PostProcessSub = nullptr;
static BML_Subscription g_PreRenderSub = nullptr;
static BML_Subscription g_PostRenderSub = nullptr;

// Cached topic IDs
static BML_TopicId g_TopicEngineInit = 0;
static BML_TopicId g_TopicEngineShutdown = 0;
static BML_TopicId g_TopicPreProcess = 0;
static BML_TopicId g_TopicPostProcess = 0;
static BML_TopicId g_TopicPreRender = 0;
static BML_TopicId g_TopicPostRender = 0;
static BML_TopicId g_TopicUIDraw = 0;

// Module state
static bool g_Initialized = false;
static CKContext* g_CKContext = nullptr;

// Global palette instance
static AnsiPalette g_GlobalPalette;

//=============================================================================
// Public API Implementation Functions
//=============================================================================

// ---- Core ----

static void* API_GetImGuiContext() {
    return Overlay::GetImGuiContext();
}

// ---- ANSI Palette ----

static void* API_GetGlobalPalette() {
    return &g_GlobalPalette;
}

static void API_PaletteEnsureInit(void* palette) {
    if (palette) {
        static_cast<AnsiPalette*>(palette)->EnsureInitialized();
    }
}

static int API_PaletteReload(void* palette) {
    if (palette) {
        return static_cast<AnsiPalette*>(palette)->ReloadFromFile() ? 1 : 0;
    }
    return 0;
}

static int API_PaletteGetColor(void* palette, int index, uint32_t* out_color) {
    if (palette && out_color) {
        ImU32 col;
        if (static_cast<AnsiPalette*>(palette)->GetColor(index, col)) {
            *out_color = col;
            return 1;
        }
    }
    return 0;
}

static int API_PaletteIsActive(void* palette) {
    if (palette) {
        return static_cast<AnsiPalette*>(palette)->IsActive() ? 1 : 0;
    }
    return 0;
}

static void API_PaletteSetDirProvider(PFN_BML_UI_LoaderDirProvider provider) {
    AnsiPalette::SetLoaderDirProvider(reinterpret_cast<AnsiPalette::LoaderDirProvider>(provider));
}

static void API_PaletteSetLogProvider(PFN_BML_UI_LogProvider provider) {
    AnsiPalette::SetLoggerProvider(reinterpret_cast<AnsiPalette::LogProvider>(provider));
}

// ---- ANSI Text Rendering ----

static void API_RenderAnsiText(const char* text) {
    if (text && g_Initialized) {
        Overlay::ImGuiContextScope scope;
        g_GlobalPalette.EnsureInitialized();
        AnsiText::TextUnformatted(text);
    }
}

static void API_RenderAnsiTextEx(const char* text, void* palette) {
    if (text && g_Initialized) {
        Overlay::ImGuiContextScope scope;
        AnsiPalette* pal = palette ? static_cast<AnsiPalette*>(palette) : &g_GlobalPalette;
        pal->EnsureInitialized();
        AnsiText::SetPreResolvePalette(pal);
        AnsiText::TextUnformatted(text);
        AnsiText::SetPreResolvePalette(nullptr);
    }
}

static void API_CalcAnsiTextSize(const char* text, float* out_width, float* out_height) {
    if (text && g_Initialized) {
        Overlay::ImGuiContextScope scope;
        ImVec2 size = AnsiText::CalcTextSize(text);
        if (out_width) *out_width = size.x;
        if (out_height) *out_height = size.y;
    } else {
        if (out_width) *out_width = 0.0f;
        if (out_height) *out_height = 0.0f;
    }
}

static void* API_ParseAnsiText(const char* text) {
    if (!text) return nullptr;
    // Placeholder for future implementation
    return nullptr;
}

static void API_FreeAnsiSegments(void* segments) {
    (void)segments;
}

//=============================================================================
// API Registration
//=============================================================================

static const BML_UI_ApiTable g_UIApiTable = {
    sizeof(BML_UI_ApiTable),
    BML_UI_API_VERSION,

    // Core
    reinterpret_cast<PFN_bmlUIGetImGuiContext>(API_GetImGuiContext),

    // ANSI Palette
    reinterpret_cast<PFN_bmlUIGetGlobalPalette>(API_GetGlobalPalette),
    API_PaletteEnsureInit,
    API_PaletteReload,
    API_PaletteGetColor,
    API_PaletteIsActive,
    API_PaletteSetDirProvider,
    API_PaletteSetLogProvider,

    // ANSI Text
    API_RenderAnsiText,
    API_RenderAnsiTextEx,
    API_CalcAnsiTextSize,
    reinterpret_cast<PFN_bmlUIParseAnsiText>(API_ParseAnsiText),
    API_FreeAnsiSegments,
};

static bool RegisterUIApis() {
    // Register BML_UI as an extension so consumers can use bmlExtensionLoad
    BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
    desc.name = BML_UI_EXTENSION_NAME;
    desc.version = bmlMakeVersion(0, 4, 0);
    desc.api_table = &g_UIApiTable;
    desc.api_size = sizeof(BML_UI_ApiTable);
    desc.description = "BML UI module providing ImGui overlay and ANSI text rendering";

    BML_Result result = bmlExtensionRegister(&desc);
    if (result != BML_RESULT_OK) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_UI",
               "Failed to register BML_UI extension (result=%d)", result);
        return false;
    }

    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_UI", "Public APIs registered via Extension system");
    return true;
}

//=============================================================================
// Event Handlers (Internal)
//=============================================================================

static void OnEngineInit(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage* msg, void* user_data) {
    (void)ctx;
    (void)topic;
    (void)user_data;

    if (!msg || !msg->data || msg->size < sizeof(BML_EngineInitEvent)) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, "BML_UI", "Invalid Engine/Init message");
        return;
    }

    auto* payload = static_cast<const BML_EngineInitEvent*>(msg->data);
    if (payload->struct_size < sizeof(BML_EngineInitEvent) || !payload->context) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, "BML_UI", "Engine/Init message has null CKContext");
        return;
    }

    g_CKContext = payload->context;

    // Create ImGui context
    if (!Overlay::ImGuiCreateContext()) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_UI", "Failed to create ImGui context");
        return;
    }

    // Initialize platform backend
    if (!Overlay::ImGuiInitPlatform(g_CKContext)) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_UI", "Failed to initialize ImGui platform backend");
        Overlay::ImGuiDestroyContext();
        return;
    }

    // Initialize renderer backend
    if (!Overlay::ImGuiInitRenderer(g_CKContext)) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_UI", "Failed to initialize ImGui renderer backend");
        Overlay::ImGuiShutdownPlatform(g_CKContext);
        Overlay::ImGuiDestroyContext();
        return;
    }

    // Initialize global palette
    g_GlobalPalette.EnsureInitialized();

    g_Initialized = true;
    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_UI", "ImGui initialized successfully");
}

static void OnEngineShutdown(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage* msg, void* user_data) {
    (void)ctx;
    (void)topic;
    (void)msg;
    (void)user_data;

    if (g_CKContext && Overlay::GetImGuiContext()) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_UI", "Shutting down ImGui");
        Overlay::ImGuiShutdownRenderer(g_CKContext);
        Overlay::ImGuiShutdownPlatform(g_CKContext);
        Overlay::ImGuiDestroyContext();
    }

    g_CKContext = nullptr;
    g_Initialized = false;
}

static void OnPreProcess(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage* msg, void* user_data) {
    (void)ctx;
    (void)topic;
    (void)msg;
    (void)user_data;

    if (g_Initialized) {
        Overlay::ImGuiContextScope scope;
        Overlay::ImGuiNewFrame();
    }
}

static void OnPostProcess(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage* msg, void* user_data) {
    (void)ctx;
    (void)topic;
    (void)msg;
    (void)user_data;

    if (g_Initialized) {
        Overlay::ImGuiContextScope scope;
        if (g_TopicUIDraw) {
            bmlImcPublish(g_TopicUIDraw, nullptr, 0);
        }
        Overlay::ImGuiEndFrame();
    }
}

static void OnPreRender(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage* msg, void* user_data) {
    (void)ctx;
    (void)topic;
    (void)msg;
    (void)user_data;

    if (g_Initialized) {
        Overlay::ImGuiRender();
    }
}

static void OnPostRender(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage* msg, void* user_data) {
    (void)ctx;
    (void)topic;
    (void)msg;
    (void)user_data;

    if (g_Initialized) {
        Overlay::ImGuiOnRender();
    }
}

//=============================================================================
// Module Lifecycle
//=============================================================================

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
    if (g_PreProcessSub) {
        bmlImcUnsubscribe(g_PreProcessSub);
        g_PreProcessSub = nullptr;
    }
    if (g_PostProcessSub) {
        bmlImcUnsubscribe(g_PostProcessSub);
        g_PostProcessSub = nullptr;
    }
    if (g_PreRenderSub) {
        bmlImcUnsubscribe(g_PreRenderSub);
        g_PreRenderSub = nullptr;
    }
    if (g_PostRenderSub) {
        bmlImcUnsubscribe(g_PostRenderSub);
        g_PostRenderSub = nullptr;
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

    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_UI", "Initializing BML UI Module v0.4.0");

    // Install Win32 message hooks for ImGui input handling
    if (!Overlay::ImGuiInstallWin32Hooks()) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_UI", "Failed to install Win32 hooks for ImGui");
        bmlUnloadAPI();
        return BML_RESULT_INTERNAL_ERROR;
    }

    // Register public APIs
    if (!RegisterUIApis()) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_UI", "Failed to register UI APIs");
        Overlay::ImGuiUninstallWin32Hooks();
        bmlUnloadAPI();
        return BML_RESULT_INTERNAL_ERROR;
    }

    // Get topic IDs for engine lifecycle and render events
    bmlImcGetTopicId(BML_TOPIC_ENGINE_INIT, &g_TopicEngineInit);
    bmlImcGetTopicId(BML_TOPIC_ENGINE_END, &g_TopicEngineShutdown);
    bmlImcGetTopicId(BML_TOPIC_ENGINE_PRE_PROCESS, &g_TopicPreProcess);
    bmlImcGetTopicId(BML_TOPIC_ENGINE_POST_PROCESS, &g_TopicPostProcess);
    bmlImcGetTopicId(BML_TOPIC_ENGINE_POST_RENDER, &g_TopicPreRender);
    bmlImcGetTopicId(BML_TOPIC_ENGINE_POST_SPRITE_RENDER, &g_TopicPostRender);
    bmlImcGetTopicId(BML_TOPIC_UI_DRAW, &g_TopicUIDraw);

    // Subscribe to engine lifecycle events
    result = bmlImcSubscribe(g_TopicEngineInit, OnEngineInit, nullptr, &g_EngineInitSub);
    if (result != BML_RESULT_OK) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_UI", "Failed to subscribe to Engine/Init");
        Overlay::ImGuiUninstallWin32Hooks();
        bmlUnloadAPI();
        return result;
    }

    result = bmlImcSubscribe(g_TopicEngineShutdown, OnEngineShutdown, nullptr, &g_EngineShutdownSub);
    if (result != BML_RESULT_OK) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_UI", "Failed to subscribe to Engine/Shutdown");
        ReleaseSubscriptions();
        Overlay::ImGuiUninstallWin32Hooks();
        bmlUnloadAPI();
        return result;
    }

    // Subscribe to process events for ImGui frame lifecycle
    bmlImcSubscribe(g_TopicPreProcess, OnPreProcess, nullptr, &g_PreProcessSub);
    bmlImcSubscribe(g_TopicPostProcess, OnPostProcess, nullptr, &g_PostProcessSub);

    // Subscribe to render events
    bmlImcSubscribe(g_TopicPreRender, OnPreRender, nullptr, &g_PreRenderSub);
    bmlImcSubscribe(g_TopicPostRender, OnPostRender, nullptr, &g_PostRenderSub);

    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_UI", "BML UI Module initialized - waiting for engine events");
    return BML_RESULT_OK;
}

BML_Result HandleDetach(const BML_ModDetachArgs* args) {
    BML_Result validation = ValidateDetachArgs(args);
    if (validation != BML_RESULT_OK) {
        return validation;
    }

    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_UI", "Shutting down BML UI Module");

    // Shutdown ImGui if still initialized
    if (g_CKContext && Overlay::GetImGuiContext()) {
        Overlay::ImGuiShutdownRenderer(g_CKContext);
        Overlay::ImGuiShutdownPlatform(g_CKContext);
        Overlay::ImGuiDestroyContext();
    }

    // Uninstall Win32 hooks
    Overlay::ImGuiUninstallWin32Hooks();

    // Unregister extension before releasing subscriptions and API
    bmlExtensionUnregister(BML_UI_EXTENSION_NAME);

    ReleaseSubscriptions();
    bmlUnloadAPI();
    g_ModHandle = nullptr;
    g_CKContext = nullptr;
    g_Initialized = false;

    return BML_RESULT_OK;
}

} // anonymous namespace

//=============================================================================
// Module Entry Point
//=============================================================================

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

// DEPRECATED: Direct DLL exports kept for backward compatibility.
// New consumers should use bmlExtensionLoad(BML_UI_EXTENSION_NAME, ...) instead.
extern "C" __declspec(dllexport) const BML_UI_ApiTable* bmlUIGetApiTable() {
    return &g_UIApiTable;
}

// DEPRECATED: Use bmlExtensionLoad(BML_UI_EXTENSION_NAME, ...) to get the API table,
// then call apiTable->GetImGuiContext() instead.
extern "C" __declspec(dllexport) void* bmlUIGetImGuiContext() {
    return API_GetImGuiContext();
}
