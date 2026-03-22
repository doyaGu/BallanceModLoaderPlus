/**
 * @file ModManager.cpp
 * @brief ModLoader CK Manager Implementation
 */

#include "ModManager.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

// CK Managers
#include "CKTimeManager.h"
#include "CKInputManager.h"
#include "CKMessageManager.h"
#include "CKAttributeManager.h"
#include "CKPathManager.h"
#include "CKSoundManager.h"
#include "CKRenderContext.h"
#include "CKBeObject.h"
#include "CK2dEntity.h"
#include "CK3dEntity.h"
#include "CKScene.h"
#include "CKStateChunk.h"
#include "CKGlobals.h"

// BML Core types only (no linking)
#include "bml_builtin_interfaces.h"
#include "bml_interface.hpp"
#include "bml_types.h"
#include "bml_imc.h"
#include "bml_core.h"
#include "bml_engine_events.h"
#include "bml_topics.h"
#include "bml_virtools.h"
#include "bml_virtools_payloads.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

extern void ModLoaderUpdateCore();

// Cached topic IDs for lifecycle events
// Engine lifecycle
static BML_TopicId s_TopicEngineInit = 0;
static BML_TopicId s_TopicEngineEnd = 0;
static BML_TopicId s_TopicEnginePlay = 0;
static BML_TopicId s_TopicEnginePause = 0;
static BML_TopicId s_TopicEngineReset = 0;
static BML_TopicId s_TopicEnginePostReset = 0;
// Frame processing
static BML_TopicId s_TopicPreProcess = 0;
static BML_TopicId s_TopicPostProcess = 0;
// Rendering
static BML_TopicId s_TopicPreRender = 0;
static BML_TopicId s_TopicPostRender = 0;
static BML_TopicId s_TopicPostSpriteRender = 0;

// Command topics
static BML_TopicId s_TopicCmdExitGame = 0;
static BML_TopicId s_TopicCmdSendMessage = 0;
static BML_TopicId s_TopicCmdSetIC = 0;
static BML_TopicId s_TopicCmdRestoreIC = 0;
static BML_TopicId s_TopicCmdShow = 0;

// Command subscription handles
static BML_Subscription s_SubCmdExitGame = nullptr;
static BML_Subscription s_SubCmdSendMessage = nullptr;
static BML_Subscription s_SubCmdSetIC = nullptr;
static BML_Subscription s_SubCmdRestoreIC = nullptr;
static BML_Subscription s_SubCmdShow = nullptr;

// Cached CKContext for command handlers (set during OnCKInit)
static CKContext *s_CKContext = nullptr;

static bml::InterfaceLease<BML_ImcBusInterface> s_ImcBus;
static bml::InterfaceLease<BML_CoreContextInterface> s_CoreContext;
static PFN_BML_GetHostModule s_GetHostModule = nullptr;

// Asset path registration tracking
struct RegisteredAssetPath {
    int path_index;       // DATA_PATH_IDX, BITMAP_PATH_IDX, SOUND_PATH_IDX
    std::string path;
};
static std::vector<RegisteredAssetPath> s_RegisteredAssetPaths;

// Callback type matching CoreApi's bmlEnumerateModuleDirectories
typedef void (*PFN_bmlEnumerateModuleDirectories)(
    void (*callback)(const char *mod_id, const wchar_t *directory,
                     const char *asset_mount, void *user_data),
    void *user_data);

static BML_Mod GetRuntimeOwner();

static bool EnsureRuntimeInterfaces() {
    BML_Mod owner = GetRuntimeOwner();
    if (!owner) {
        return false;
    }

    if (!s_ImcBus) {
        s_ImcBus = bml::AcquireInterface<BML_ImcBusInterface>(owner, BML_IMC_BUS_INTERFACE_ID, 1, 0, 0);
    }
    if (!s_CoreContext) {
        s_CoreContext = bml::AcquireInterface<BML_CoreContextInterface>(owner, BML_CORE_CONTEXT_INTERFACE_ID, 1, 0, 0);
    }
    return static_cast<bool>(s_ImcBus) && static_cast<bool>(s_CoreContext);
}

static BML_Mod GetRuntimeOwner() {
    if (!s_GetHostModule) {
        if (HMODULE bml = ::GetModuleHandleA("BML.dll")) {
            s_GetHostModule = reinterpret_cast<PFN_BML_GetHostModule>(
                ::GetProcAddress(bml, "bmlGetHostModule"));
        }
    }
    return s_GetHostModule ? s_GetHostModule() : nullptr;
}

static void PumpImcNow() {
    if (EnsureRuntimeInterfaces() && s_ImcBus->Pump) {
        s_ImcBus->Pump(0);
    }
}

/* ========================================================================
 * Game Command Handlers
 * ======================================================================== */

// Recursive IC/Show helpers matching the proven v0.3 pattern
static void SetICRecursive(CKScene *scene, CKBeObject *obj, CKBOOL hierarchy);
static void RestoreICRecursive(CKScene *scene, CKBeObject *obj, CKBOOL hierarchy);
static void ShowRecursive(CKBeObject *obj, CK_OBJECT_SHOWOPTION show, CKBOOL hierarchy);

static void SetICRecursive(CKScene *scene, CKBeObject *obj, CKBOOL hierarchy) {
    if (!obj) return;
    CKStateChunk *chunk = CKSaveObjectState(obj);
    if (chunk)
        scene->SetObjectInitialValue(obj, chunk);

    if (hierarchy) {
        // Check 3D first (CK3dEntity inherits CK2dEntity -- avoids double traversal)
        if (CKIsChildClassOf(obj, CKCID_3DENTITY)) {
            auto *entity = (CK3dEntity *)obj;
            for (int i = 0; i < entity->GetChildrenCount(); i++)
                SetICRecursive(scene, entity->GetChild(i), TRUE);
        } else if (CKIsChildClassOf(obj, CKCID_2DENTITY)) {
            auto *entity = (CK2dEntity *)obj;
            for (int i = 0; i < entity->GetChildrenCount(); i++)
                SetICRecursive(scene, entity->GetChild(i), TRUE);
        }
    }
}

static void RestoreICRecursive(CKScene *scene, CKBeObject *obj, CKBOOL hierarchy) {
    if (!obj) return;
    CKStateChunk *chunk = scene->GetObjectInitialValue(obj);
    if (chunk)
        CKReadObjectState(obj, chunk);

    if (hierarchy) {
        if (CKIsChildClassOf(obj, CKCID_3DENTITY)) {
            auto *entity = (CK3dEntity *)obj;
            for (int i = 0; i < entity->GetChildrenCount(); i++)
                RestoreICRecursive(scene, entity->GetChild(i), TRUE);
        } else if (CKIsChildClassOf(obj, CKCID_2DENTITY)) {
            auto *entity = (CK2dEntity *)obj;
            for (int i = 0; i < entity->GetChildrenCount(); i++)
                RestoreICRecursive(scene, entity->GetChild(i), TRUE);
        }
    }
}

static void ShowRecursive(CKBeObject *obj, CK_OBJECT_SHOWOPTION show, CKBOOL hierarchy) {
    if (!obj) return;
    obj->Show(show);

    if (hierarchy) {
        if (CKIsChildClassOf(obj, CKCID_3DENTITY)) {
            auto *entity = (CK3dEntity *)obj;
            for (int i = 0; i < entity->GetChildrenCount(); i++)
                ShowRecursive(entity->GetChild(i), show, TRUE);
        } else if (CKIsChildClassOf(obj, CKCID_2DENTITY)) {
            auto *entity = (CK2dEntity *)obj;
            for (int i = 0; i < entity->GetChildrenCount(); i++)
                ShowRecursive(entity->GetChild(i), show, TRUE);
        }
    }
}

static void OnCmdExitGame(BML_Context, BML_TopicId, const BML_ImcMessage *, void *) {
    if (s_CKContext) {
        HWND hwnd = (HWND)s_CKContext->GetMainWindow();
        if (hwnd) {
            ::PostMessageA(hwnd, WM_CLOSE, 0, 0);
        }
    }
}

static void OnCmdSendMessage(BML_Context, BML_TopicId, const BML_ImcMessage *message, void *) {
    if (!message || !message->data || message->size == 0) return;
    const auto *raw = static_cast<const char *>(message->data);
    // Verify null-termination -- size must include the terminator
    if (raw[message->size - 1] != '\0') return;
    if (!s_ImcBus || !s_ImcBus->Publish) return;

    BML_TopicId consoleTopic = 0;
    s_ImcBus->GetTopicId(BML_TOPIC_CONSOLE_OUTPUT, &consoleTopic);
    if (consoleTopic) {
        if (BML_Mod owner = GetRuntimeOwner()) {
            s_ImcBus->Publish(owner, consoleTopic, raw, message->size);
        }
    }
}

static void OnCmdSetIC(BML_Context, BML_TopicId, const BML_ImcMessage *message, void *) {
    if (!message || !message->data || message->size < sizeof(BML_SetICCommand) || !s_CKContext) return;
    const auto *cmd = static_cast<const BML_SetICCommand *>(message->data);
    if (!cmd->target) return;
    CKScene *scene = s_CKContext->GetCurrentScene();
    if (!scene) return;
    SetICRecursive(scene, cmd->target, cmd->hierarchy);
}

static void OnCmdRestoreIC(BML_Context, BML_TopicId, const BML_ImcMessage *message, void *) {
    if (!message || !message->data || message->size < sizeof(BML_RestoreICCommand) || !s_CKContext) return;
    const auto *cmd = static_cast<const BML_RestoreICCommand *>(message->data);
    if (!cmd->target) return;
    CKScene *scene = s_CKContext->GetCurrentScene();
    if (!scene) return;
    RestoreICRecursive(scene, cmd->target, cmd->hierarchy);
}

static void OnCmdShow(BML_Context, BML_TopicId, const BML_ImcMessage *message, void *) {
    if (!message || !message->data || message->size < sizeof(BML_ShowCommand)) return;
    const auto *cmd = static_cast<const BML_ShowCommand *>(message->data);
    if (!cmd->target) return;
    ShowRecursive(cmd->target, static_cast<CK_OBJECT_SHOWOPTION>(cmd->show_option), cmd->hierarchy);
}

static void SubscribeCommandTopics() {
    if (!EnsureRuntimeInterfaces() || !s_ImcBus->Subscribe || !s_ImcBus->GetTopicId)
        return;

    auto subscribe = [](const char *topic, BML_TopicId &id, BML_Subscription &sub,
                        BML_ImcHandler handler) {
        s_ImcBus->GetTopicId(topic, &id);
        if (id && !sub) {
            BML_Mod owner = GetRuntimeOwner();
            if (owner) {
                s_ImcBus->Subscribe(owner, id, handler, nullptr, &sub);
            }
        }
    };

    subscribe(BML_TOPIC_COMMAND_EXIT_GAME,   s_TopicCmdExitGame,   s_SubCmdExitGame,   OnCmdExitGame);
    subscribe(BML_TOPIC_COMMAND_SEND_MESSAGE, s_TopicCmdSendMessage, s_SubCmdSendMessage, OnCmdSendMessage);
    subscribe(BML_TOPIC_COMMAND_SET_IC,      s_TopicCmdSetIC,      s_SubCmdSetIC,      OnCmdSetIC);
    subscribe(BML_TOPIC_COMMAND_RESTORE_IC,  s_TopicCmdRestoreIC,  s_SubCmdRestoreIC,  OnCmdRestoreIC);
    subscribe(BML_TOPIC_COMMAND_SHOW,        s_TopicCmdShow,       s_SubCmdShow,       OnCmdShow);
}

static void UnsubscribeCommandTopics() {
    if (!s_ImcBus || !s_ImcBus->Unsubscribe) return;

    auto unsub = [](BML_Subscription &sub) {
        if (sub) { s_ImcBus->Unsubscribe(sub); sub = nullptr; }
    };

    unsub(s_SubCmdExitGame);
    unsub(s_SubCmdSendMessage);
    unsub(s_SubCmdSetIC);
    unsub(s_SubCmdRestoreIC);
    unsub(s_SubCmdShow);
}

ModManager::ModManager(CKContext *context)
    : CKBaseManager(context, MOD_MANAGER_GUID, (CKSTRING) "Mod Manager") {
    context->RegisterNewManager(this);
    (void) EnsureRuntimeInterfaces();
    OutputDebugStringA("ModManager: Created.\n");
}

ModManager::~ModManager() {
    s_ImcBus.Reset();
    s_CoreContext.Reset();
    OutputDebugStringA("ModManager: Destroyed.\n");
}

void ModManager::InitializeIMCTopics() {
    if (!EnsureRuntimeInterfaces() || !s_ImcBus->GetTopicId || !s_ImcBus->Publish || !s_ImcBus->Pump) {
        OutputDebugStringA("ModManager: Warning - Failed to load IMC API.\n");
        return;
    }

    // Register topic IDs - Engine lifecycle
    s_ImcBus->GetTopicId(BML_TOPIC_ENGINE_INIT, &s_TopicEngineInit);
    s_ImcBus->GetTopicId(BML_TOPIC_ENGINE_END, &s_TopicEngineEnd);
    s_ImcBus->GetTopicId(BML_TOPIC_ENGINE_PLAY, &s_TopicEnginePlay);
    s_ImcBus->GetTopicId(BML_TOPIC_ENGINE_PAUSE, &s_TopicEnginePause);
    s_ImcBus->GetTopicId(BML_TOPIC_ENGINE_RESET, &s_TopicEngineReset);
    s_ImcBus->GetTopicId(BML_TOPIC_ENGINE_POST_RESET, &s_TopicEnginePostReset);
    // Frame processing
    s_ImcBus->GetTopicId(BML_TOPIC_ENGINE_PRE_PROCESS, &s_TopicPreProcess);
    s_ImcBus->GetTopicId(BML_TOPIC_ENGINE_POST_PROCESS, &s_TopicPostProcess);
    // Rendering
    s_ImcBus->GetTopicId(BML_TOPIC_ENGINE_PRE_RENDER, &s_TopicPreRender);
    s_ImcBus->GetTopicId(BML_TOPIC_ENGINE_POST_RENDER, &s_TopicPostRender);
    s_ImcBus->GetTopicId(BML_TOPIC_ENGINE_POST_SPRITE_RENDER, &s_TopicPostSpriteRender);

    OutputDebugStringA("ModManager: IMC topics initialized.\n");
}

void ModManager::RegisterVirtoolsObjects() {
    if (!EnsureRuntimeInterfaces() || !s_CoreContext->SetUserData || !s_CoreContext->GetGlobalContext) {
        OutputDebugStringA("ModManager: Warning - Failed to load Context API.\n");
        return;
    }

    BML_Context ctx = s_CoreContext->GetGlobalContext();
    if (!ctx) {
        OutputDebugStringA("ModManager: Warning - No global context available.\n");
        return;
    }

    // Register CKContext
    s_CoreContext->SetUserData(ctx, BML_VIRTOOLS_KEY_CKCONTEXT, m_Context, nullptr);

    // Register managers via GUID lookup
    auto *inputMgr = m_Context->GetManagerByGuid(INPUT_MANAGER_GUID);
    if (inputMgr) {
        s_CoreContext->SetUserData(ctx, BML_VIRTOOLS_KEY_INPUTMANAGER, inputMgr, nullptr);
    }
    auto *timeMgr = m_Context->GetTimeManager();
    if (timeMgr) {
        s_CoreContext->SetUserData(ctx, BML_VIRTOOLS_KEY_TIMEMANAGER, timeMgr, nullptr);
    }
    auto *msgMgr = m_Context->GetManagerByGuid(MESSAGE_MANAGER_GUID);
    if (msgMgr) {
        s_CoreContext->SetUserData(ctx, BML_VIRTOOLS_KEY_MESSAGEMANAGER, msgMgr, nullptr);
    }
    auto *attrMgr = m_Context->GetManagerByGuid(ATTRIBUTE_MANAGER_GUID);
    if (attrMgr) {
        s_CoreContext->SetUserData(ctx, BML_VIRTOOLS_KEY_ATTRIBUTEMANAGER, attrMgr, nullptr);
    }
    auto *pathMgr = m_Context->GetManagerByGuid(PATH_MANAGER_GUID);
    if (pathMgr) {
        s_CoreContext->SetUserData(ctx, BML_VIRTOOLS_KEY_PATHMANAGER, pathMgr, nullptr);
    }
    auto *soundMgr = m_Context->GetManagerByGuid(SOUND_MANAGER_GUID);
    if (soundMgr) {
        s_CoreContext->SetUserData(ctx, BML_VIRTOOLS_KEY_SOUNDMANAGER, soundMgr, nullptr);
    }

    // Register main window HWND
    HWND mainHwnd = (HWND) m_Context->GetMainWindow();
    if (mainHwnd) {
        s_CoreContext->SetUserData(ctx, BML_VIRTOOLS_KEY_MAINHWND, mainHwnd, nullptr);
    }

    OutputDebugStringA("ModManager: Virtools objects registered.\n");
}

void ModManager::PublishLifecycleEvent(const char *topic) {
    if (!EnsureRuntimeInterfaces() || !s_ImcBus->Publish) return;

    BML_TopicId topicId = 0;
    if (s_ImcBus->GetTopicId) {
        s_ImcBus->GetTopicId(topic, &topicId);
    }

    if (topicId) {
        if (BML_Mod owner = GetRuntimeOwner()) {
            s_ImcBus->Publish(owner, topicId, nullptr, 0);
        }
    }
}

void ModManager::PublishProcessEvent(float deltaTime) {
    if (!EnsureRuntimeInterfaces() || !s_ImcBus->Publish || !s_TopicPostProcess) return;
    if (BML_Mod owner = GetRuntimeOwner()) {
        s_ImcBus->Publish(owner, s_TopicPostProcess, &deltaTime, sizeof(float));
    }
}

static void TryAddAssetPath(CKPathManager *pm, int pathIndex, const std::string &path) {
    if (!pm || path.empty()) return;
    std::error_code ec;
    if (!std::filesystem::is_directory(path, ec)) return;

    XString xPath(path.c_str());
    if (pm->GetPathIndex(pathIndex, xPath) != -1) return;

    pm->AddPath(pathIndex, xPath);
    s_RegisteredAssetPaths.push_back({pathIndex, path});
}

static void RegisterModAssetPathsCallback(const char *mod_id,
                                           const wchar_t *directory,
                                           const char *asset_mount,
                                           void *user_data) {
    auto *pm = static_cast<CKPathManager *>(user_data);
    if (!pm || !directory) return;

    // Only register paths for mods that declare [assets] in their manifest
    if (!asset_mount || asset_mount[0] == '\0') return;

    namespace fs = std::filesystem;
    fs::path base = fs::path(directory) / asset_mount;

    std::string baseStr = base.string();
    if (!baseStr.empty() && baseStr.back() != '\\') baseStr += '\\';

    // Register standard CK2 asset subdirectories
    TryAddAssetPath(pm, DATA_PATH_IDX, baseStr);

    std::string entities = baseStr + "3D Entities\\";
    TryAddAssetPath(pm, DATA_PATH_IDX, entities);

    std::string physics = baseStr + "3D Entities\\PH\\";
    TryAddAssetPath(pm, DATA_PATH_IDX, physics);

    std::string textures = baseStr + "Textures\\";
    TryAddAssetPath(pm, BITMAP_PATH_IDX, textures);

    std::string sounds = baseStr + "Sounds\\";
    TryAddAssetPath(pm, SOUND_PATH_IDX, sounds);
}

void ModManager::RegisterModAssetPaths() {
    auto *pathMgr = static_cast<CKPathManager *>(
        m_Context->GetManagerByGuid(PATH_MANAGER_GUID));
    if (!pathMgr) {
        OutputDebugStringA("ModManager: Warning - No CKPathManager for asset registration.\n");
        return;
    }

    // Resolve the enumerate function via BML's GetProcAddress export
    HMODULE hBML = ::GetModuleHandleA("BML.dll");
    if (!hBML) {
        OutputDebugStringA("ModManager: Warning - BML.dll not loaded for asset registration.\n");
        return;
    }
    auto bmlGetProc = reinterpret_cast<void *(*)(const char *)>(
        ::GetProcAddress(hBML, "bmlGetProcAddress"));
    if (!bmlGetProc) {
        OutputDebugStringA("ModManager: Warning - bmlGetProcAddress not found.\n");
        return;
    }
    auto enumFn = reinterpret_cast<PFN_bmlEnumerateModuleDirectories>(
        bmlGetProc("bmlEnumerateModuleDirectories"));
    if (!enumFn) {
        OutputDebugStringA("ModManager: Warning - bmlEnumerateModuleDirectories not available.\n");
        return;
    }

    s_RegisteredAssetPaths.clear();
    enumFn(RegisterModAssetPathsCallback, pathMgr);

    if (!s_RegisteredAssetPaths.empty()) {
        char msg[128];
        std::snprintf(msg, sizeof(msg), "ModManager: Registered %zu asset search paths.\n",
                 s_RegisteredAssetPaths.size());
        OutputDebugStringA(msg);
    }
}

void ModManager::UnregisterModAssetPaths() {
    if (s_RegisteredAssetPaths.empty()) return;

    auto *pathMgr = static_cast<CKPathManager *>(
        m_Context->GetManagerByGuid(PATH_MANAGER_GUID));
    if (!pathMgr) return;

    for (const auto &entry : s_RegisteredAssetPaths) {
        XString xPath(entry.path.c_str());
        int idx = pathMgr->GetPathIndex(entry.path_index, xPath);
        if (idx >= 0) {
            pathMgr->RemovePath(entry.path_index, idx);
        }
    }
    s_RegisteredAssetPaths.clear();
}

CKERROR ModManager::OnCKInit() {
    OutputDebugStringA("ModManager: OnCKInit.\n");

    // Initialize IMC topics
    InitializeIMCTopics();

    // Cache CKContext for command handlers
    s_CKContext = m_Context;

    // Register Virtools objects to BML context for modules to access
    RegisterVirtoolsObjects();

    // Subscribe to game command topics
    SubscribeCommandTopics();

    // Register mod asset search paths with CKPathManager
    RegisterModAssetPaths();

    // Publish engine init event with CKContext payload
    if (EnsureRuntimeInterfaces() && s_ImcBus->Publish && s_TopicEngineInit) {
        BML_EngineInitEvent payload{};
        payload.struct_size = sizeof(payload);
        payload.context = m_Context;
        payload.main_window = m_Context ? m_Context->GetMainWindow() : nullptr;
        if (BML_Mod owner = GetRuntimeOwner()) {
            s_ImcBus->Publish(owner, s_TopicEngineInit, &payload, sizeof(payload));
        }
        PumpImcNow();
    }

    return CK_OK;
}

CKERROR ModManager::OnCKEnd() {
    OutputDebugStringA("ModManager: OnCKEnd.\n");

    // Unsubscribe command handlers
    UnsubscribeCommandTopics();
    s_CKContext = nullptr;

    // Unregister mod asset paths before shutdown
    UnregisterModAssetPaths();

    // Publish engine end event
    if (EnsureRuntimeInterfaces() && s_ImcBus->Publish && s_TopicEngineEnd) {
        if (BML_Mod owner = GetRuntimeOwner()) {
            s_ImcBus->Publish(owner, s_TopicEngineEnd, nullptr, 0);
        }
        PumpImcNow();
    }

    m_EngineReady = false;
    m_RenderContext = nullptr;

    return CK_OK;
}

CKERROR ModManager::OnCKPause() {
    OutputDebugStringA("ModManager: OnCKPause.\n");

    // Publish engine pause event
    if (EnsureRuntimeInterfaces() && s_ImcBus->Publish && s_TopicEnginePause) {
        if (BML_Mod owner = GetRuntimeOwner()) {
            s_ImcBus->Publish(owner, s_TopicEnginePause, nullptr, 0);
        }
        PumpImcNow();
    }

    return CK_OK;
}

CKERROR ModManager::OnCKPlay() {
    // First play after reset - initialize render context
    if (m_Context->IsReseted() && m_Context->GetCurrentLevel() != nullptr && !m_RenderContext) {
        m_RenderContext = m_Context->GetPlayerRenderContext();
        m_EngineReady = true;

        OutputDebugStringA("ModManager: OnCKPlay - Engine ready.\n");

        // Register render context and render HWND to BML context
        if (EnsureRuntimeInterfaces() && s_CoreContext->SetUserData && s_CoreContext->GetGlobalContext) {
            BML_Context ctx = s_CoreContext->GetGlobalContext();
            if (ctx && m_RenderContext) {
                s_CoreContext->SetUserData(ctx, BML_VIRTOOLS_KEY_RENDERCONTEXT, m_RenderContext, nullptr);
                HWND renderHwnd = (HWND) m_RenderContext->GetWindowHandle();
                if (renderHwnd) {
                    s_CoreContext->SetUserData(ctx, BML_VIRTOOLS_KEY_RENDERHWND, renderHwnd, nullptr);
                }
            }
        }

        // Publish engine play event with CKRenderContext payload
        if (EnsureRuntimeInterfaces() && s_ImcBus->Publish && s_TopicEnginePlay) {
            BML_EnginePlayEvent payload{};
            payload.struct_size = sizeof(payload);
            payload.context = m_Context;
            payload.render_context = m_RenderContext;
            payload.render_window = m_RenderContext ? m_RenderContext->GetWindowHandle() : nullptr;
            payload.is_resume = BML_FALSE;
            if (BML_Mod owner = GetRuntimeOwner()) {
                s_ImcBus->Publish(owner, s_TopicEnginePlay, &payload, sizeof(payload));
            }
            PumpImcNow();
        }
    }

    return CK_OK;
}

CKERROR ModManager::OnCKReset() {
    if (m_Context->GetCurrentLevel() != nullptr && m_RenderContext) {
        OutputDebugStringA("ModManager: OnCKReset.\n");

        // Publish engine reset event
        if (EnsureRuntimeInterfaces() && s_ImcBus->Publish && s_TopicEngineReset) {
            if (BML_Mod owner = GetRuntimeOwner()) {
                s_ImcBus->Publish(owner, s_TopicEngineReset, nullptr, 0);
            }
            PumpImcNow();
        }

        m_RenderContext = nullptr;
        m_EngineReady = false;
    }

    return CK_OK;
}

CKERROR ModManager::OnCKPostReset() {
    OutputDebugStringA("ModManager: OnCKPostReset.\n");

    // Publish engine post-reset event
    if (EnsureRuntimeInterfaces() && s_ImcBus->Publish && s_TopicEnginePostReset) {
        if (BML_Mod owner = GetRuntimeOwner()) {
            s_ImcBus->Publish(owner, s_TopicEnginePostReset, nullptr, 0);
        }
        PumpImcNow();
    }

    return CK_OK;
}

CKERROR ModManager::PreProcess() {
    // Pump IMC bus first to handle any pending messages
    if (EnsureRuntimeInterfaces() && s_ImcBus->Pump) {
        s_ImcBus->Pump(100);
    }

    // Publish pre-process event
    if (EnsureRuntimeInterfaces() && s_ImcBus->Publish && s_TopicPreProcess) {
        if (BML_Mod owner = GetRuntimeOwner()) {
            s_ImcBus->Publish(owner, s_TopicPreProcess, nullptr, 0);
        }
        PumpImcNow();
    }

    return CK_OK;
}

CKERROR ModManager::PostProcess() {
    // Drive deferred core work on the main loop (hot reload debounce/reload processing).
    ModLoaderUpdateCore();

    // Publish delta time in seconds to match original HUD/UI timing semantics.
    CKTimeManager *timeMgr = m_Context->GetTimeManager();
    float deltaTime = timeMgr ? (timeMgr->GetLastDeltaTime() / 1000.0f) : 0.0f;

    // Publish post-process event with delta time
    if (EnsureRuntimeInterfaces() && s_ImcBus->Publish && s_TopicPostProcess) {
        if (BML_Mod owner = GetRuntimeOwner()) {
            s_ImcBus->Publish(owner, s_TopicPostProcess, &deltaTime, sizeof(float));
        }
        PumpImcNow();
    }

    return CK_OK;
}

CKERROR ModManager::OnPreRender(CKRenderContext *dev) {
    // Publish pre-render event
    if (EnsureRuntimeInterfaces() && s_ImcBus->Publish && s_TopicPreRender) {
        if (BML_Mod owner = GetRuntimeOwner()) {
            s_ImcBus->Publish(owner, s_TopicPreRender, &dev, sizeof(dev));
        }
        PumpImcNow();
    }

    return CK_OK;
}

CKERROR ModManager::OnPostRender(CKRenderContext *dev) {
    // Publish post-render event
    if (EnsureRuntimeInterfaces() && s_ImcBus->Publish && s_TopicPostRender) {
        if (BML_Mod owner = GetRuntimeOwner()) {
            s_ImcBus->Publish(owner, s_TopicPostRender, &dev, sizeof(dev));
        }
        PumpImcNow();
    }

    return CK_OK;
}

CKERROR ModManager::OnPostSpriteRender(CKRenderContext *dev) {
    // Publish post-sprite-render event (for overlay rendering)
    if (EnsureRuntimeInterfaces() && s_ImcBus->Publish && s_TopicPostSpriteRender) {
        if (BML_Mod owner = GetRuntimeOwner()) {
            s_ImcBus->Publish(owner, s_TopicPostSpriteRender, &dev, sizeof(dev));
        }
        PumpImcNow();
    }

    return CK_OK;
}
