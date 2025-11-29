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

// BML Core types only (no linking)
#include "bml_types.h"
#include "bml_imc.h"
#include "bml_core.h"
#include "bml_engine_events.h"
#include "bml_virtools.h"

// External function from Entry.cpp to get BML API
extern void* ModLoaderGetProcAddress(const char* name);

// IMC function pointers (loaded from Core)
static PFN_BML_ImcGetTopicId s_ImcGetTopicId = nullptr;
static PFN_BML_ImcPublish s_ImcPublish = nullptr;
static PFN_BML_ImcPump s_ImcPump = nullptr;

// Context function pointers (loaded from Core)
static PFN_BML_ContextSetUserData s_ContextSetUserData = nullptr;
static PFN_BML_GetGlobalContext s_GetGlobalContext = nullptr;

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

ModManager::ModManager(CKContext *context)
    : CKBaseManager(context, MOD_MANAGER_GUID, (CKSTRING) "Mod Manager") {
    context->RegisterNewManager(this);
    OutputDebugStringA("ModManager: Created.\n");
}

ModManager::~ModManager() {
    OutputDebugStringA("ModManager: Destroyed.\n");
}

void ModManager::InitializeIMCTopics() {
    // Load IMC API from Core
    s_ImcGetTopicId = (PFN_BML_ImcGetTopicId) ModLoaderGetProcAddress("bmlImcGetTopicId");
    s_ImcPublish = (PFN_BML_ImcPublish) ModLoaderGetProcAddress("bmlImcPublish");
    s_ImcPump = (PFN_BML_ImcPump) ModLoaderGetProcAddress("bmlImcPump");

    if (!s_ImcGetTopicId || !s_ImcPublish || !s_ImcPump) {
        OutputDebugStringA("ModManager: Warning - Failed to load IMC API.\n");
        return;
    }

    // Register topic IDs - Engine lifecycle
    s_ImcGetTopicId("BML/Engine/Init", &s_TopicEngineInit);
    s_ImcGetTopicId("BML/Engine/End", &s_TopicEngineEnd);
    s_ImcGetTopicId("BML/Engine/Play", &s_TopicEnginePlay);
    s_ImcGetTopicId("BML/Engine/Pause", &s_TopicEnginePause);
    s_ImcGetTopicId("BML/Engine/Reset", &s_TopicEngineReset);
    s_ImcGetTopicId("BML/Engine/PostReset", &s_TopicEnginePostReset);
    // Frame processing
    s_ImcGetTopicId("BML/Engine/PreProcess", &s_TopicPreProcess);
    s_ImcGetTopicId("BML/Engine/PostProcess", &s_TopicPostProcess);
    // Rendering
    s_ImcGetTopicId("BML/Engine/PreRender", &s_TopicPreRender);
    s_ImcGetTopicId("BML/Engine/PostRender", &s_TopicPostRender);
    s_ImcGetTopicId("BML/Engine/PostSpriteRender", &s_TopicPostSpriteRender);

    OutputDebugStringA("ModManager: IMC topics initialized.\n");
}

void ModManager::RegisterVirtoolsObjects() {
    // Load Context API from Core
    s_ContextSetUserData = (PFN_BML_ContextSetUserData) ModLoaderGetProcAddress("bmlContextSetUserData");
    s_GetGlobalContext = (PFN_BML_GetGlobalContext) ModLoaderGetProcAddress("bmlGetGlobalContext");

    if (!s_ContextSetUserData || !s_GetGlobalContext) {
        OutputDebugStringA("ModManager: Warning - Failed to load Context API.\n");
        return;
    }

    BML_Context ctx = s_GetGlobalContext();
    if (!ctx) {
        OutputDebugStringA("ModManager: Warning - No global context available.\n");
        return;
    }

    // Register CKContext
    s_ContextSetUserData(ctx, BML_VIRTOOLS_KEY_CKCONTEXT, m_Context, nullptr);

    // Register managers via GUID lookup
    auto *inputMgr = m_Context->GetManagerByGuid(INPUT_MANAGER_GUID);
    if (inputMgr) {
        s_ContextSetUserData(ctx, BML_VIRTOOLS_KEY_INPUTMANAGER, inputMgr, nullptr);
    }
    auto *timeMgr = m_Context->GetTimeManager();
    if (timeMgr) {
        s_ContextSetUserData(ctx, BML_VIRTOOLS_KEY_TIMEMANAGER, timeMgr, nullptr);
    }
    auto *msgMgr = m_Context->GetManagerByGuid(MESSAGE_MANAGER_GUID);
    if (msgMgr) {
        s_ContextSetUserData(ctx, BML_VIRTOOLS_KEY_MESSAGEMANAGER, msgMgr, nullptr);
    }
    auto *attrMgr = m_Context->GetManagerByGuid(ATTRIBUTE_MANAGER_GUID);
    if (attrMgr) {
        s_ContextSetUserData(ctx, BML_VIRTOOLS_KEY_ATTRIBUTEMANAGER, attrMgr, nullptr);
    }
    auto *pathMgr = m_Context->GetManagerByGuid(PATH_MANAGER_GUID);
    if (pathMgr) {
        s_ContextSetUserData(ctx, BML_VIRTOOLS_KEY_PATHMANAGER, pathMgr, nullptr);
    }
    auto *soundMgr = m_Context->GetManagerByGuid(SOUND_MANAGER_GUID);
    if (soundMgr) {
        s_ContextSetUserData(ctx, BML_VIRTOOLS_KEY_SOUNDMANAGER, soundMgr, nullptr);
    }

    // Register main window HWND
    HWND mainHwnd = (HWND) m_Context->GetMainWindow();
    if (mainHwnd) {
        s_ContextSetUserData(ctx, BML_VIRTOOLS_KEY_MAINHWND, mainHwnd, nullptr);
    }

    OutputDebugStringA("ModManager: Virtools objects registered.\n");
}

void ModManager::PublishLifecycleEvent(const char *topic) {
    if (!s_ImcPublish) return;

    BML_TopicId topicId = 0;
    if (s_ImcGetTopicId) {
        s_ImcGetTopicId(topic, &topicId);
    }

    if (topicId) {
        s_ImcPublish(topicId, nullptr, 0);
    }
}

void ModManager::PublishProcessEvent(float deltaTime) {
    if (!s_ImcPublish || !s_TopicPostProcess) return;
    s_ImcPublish(s_TopicPostProcess, &deltaTime, sizeof(float));
}

CKERROR ModManager::OnCKInit() {
    OutputDebugStringA("ModManager: OnCKInit.\n");

    // Initialize IMC topics
    InitializeIMCTopics();

    // Register Virtools objects to BML context for modules to access
    RegisterVirtoolsObjects();

    // Publish engine init event with CKContext payload
    if (s_ImcPublish && s_TopicEngineInit) {
        BML_EngineInitEvent payload{};
        payload.struct_size = sizeof(payload);
        payload.context = m_Context;
        payload.main_window = m_Context ? m_Context->GetMainWindow() : nullptr;
        s_ImcPublish(s_TopicEngineInit, &payload, sizeof(payload));
    }

    return CK_OK;
}

CKERROR ModManager::OnCKEnd() {
    OutputDebugStringA("ModManager: OnCKEnd.\n");

    // Publish engine end event
    if (s_ImcPublish && s_TopicEngineEnd) {
        s_ImcPublish(s_TopicEngineEnd, nullptr, 0);
    }

    m_EngineReady = false;
    m_RenderContext = nullptr;

    return CK_OK;
}

CKERROR ModManager::OnCKPause() {
    OutputDebugStringA("ModManager: OnCKPause.\n");

    // Publish engine pause event
    if (s_ImcPublish && s_TopicEnginePause) {
        s_ImcPublish(s_TopicEnginePause, nullptr, 0);
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
        if (s_ContextSetUserData && s_GetGlobalContext) {
            BML_Context ctx = s_GetGlobalContext();
            if (ctx && m_RenderContext) {
                s_ContextSetUserData(ctx, BML_VIRTOOLS_KEY_RENDERCONTEXT, m_RenderContext, nullptr);
                HWND renderHwnd = (HWND) m_RenderContext->GetWindowHandle();
                if (renderHwnd) {
                    s_ContextSetUserData(ctx, BML_VIRTOOLS_KEY_RENDERHWND, renderHwnd, nullptr);
                }
            }
        }

        // Publish engine play event with CKRenderContext payload
        if (s_ImcPublish && s_TopicEnginePlay) {
            BML_EnginePlayEvent payload{};
            payload.struct_size = sizeof(payload);
            payload.context = m_Context;
            payload.render_context = m_RenderContext;
            payload.render_window = m_RenderContext ? m_RenderContext->GetWindowHandle() : nullptr;
            payload.is_resume = BML_FALSE;
            s_ImcPublish(s_TopicEnginePlay, &payload, sizeof(payload));
        }
    }

    return CK_OK;
}

CKERROR ModManager::OnCKReset() {
    if (m_Context->GetCurrentLevel() != nullptr && m_RenderContext) {
        OutputDebugStringA("ModManager: OnCKReset.\n");

        // Publish engine reset event
        if (s_ImcPublish && s_TopicEngineReset) {
            s_ImcPublish(s_TopicEngineReset, nullptr, 0);
        }

        m_RenderContext = nullptr;
        m_EngineReady = false;
    }

    return CK_OK;
}

CKERROR ModManager::OnCKPostReset() {
    OutputDebugStringA("ModManager: OnCKPostReset.\n");

    // Publish engine post-reset event
    if (s_ImcPublish && s_TopicEnginePostReset) {
        s_ImcPublish(s_TopicEnginePostReset, nullptr, 0);
    }

    return CK_OK;
}

CKERROR ModManager::PreProcess() {
    // Pump IMC bus first to handle any pending messages
    if (s_ImcPump) {
        s_ImcPump(100);
    }

    // Publish pre-process event
    if (s_ImcPublish && s_TopicPreProcess) {
        s_ImcPublish(s_TopicPreProcess, nullptr, 0);
    }

    return CK_OK;
}

CKERROR ModManager::PostProcess() {
    // Calculate delta time
    CKTimeManager *timeMgr = m_Context->GetTimeManager();
    float currentTime = timeMgr ? timeMgr->GetTime() : 0.0f;
    float deltaTime = (m_LastTime > 0.0f) ? (currentTime - m_LastTime) : 0.0f;
    m_LastTime = currentTime;

    // Publish post-process event with delta time
    if (s_ImcPublish && s_TopicPostProcess) {
        s_ImcPublish(s_TopicPostProcess, &deltaTime, sizeof(float));
    }

    return CK_OK;
}

CKERROR ModManager::OnPreRender(CKRenderContext *dev) {
    // Publish pre-render event
    if (s_ImcPublish && s_TopicPreRender) {
        s_ImcPublish(s_TopicPreRender, dev, sizeof(void *));
    }

    return CK_OK;
}

CKERROR ModManager::OnPostRender(CKRenderContext *dev) {
    // Publish post-render event
    if (s_ImcPublish && s_TopicPostRender) {
        s_ImcPublish(s_TopicPostRender, dev, sizeof(void *));
    }

    return CK_OK;
}

CKERROR ModManager::OnPostSpriteRender(CKRenderContext *dev) {
    // Publish post-sprite-render event (for overlay rendering)
    if (s_ImcPublish && s_TopicPostSpriteRender) {
        s_ImcPublish(s_TopicPostSpriteRender, dev, sizeof(void *));
    }

    return CK_OK;
}
