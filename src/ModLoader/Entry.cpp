/**
 * @file Entry.cpp
 * @brief ModLoader.dll Entry Point
 * 
 * This is the main entry point for ModLoader.dll, which provides:
 * - Virtools CK2 plugin registration (CKGetPluginInfo, RegisterBehaviorDeclarations)
 * - MinHook initialization for engine hooks
 * - Core render hooks and behavior prototype hooks
 * 
 * ModLoader.dll replaces the engine integration functionality that was
 * previously in BMLPlus.dll, separating engine concerns from mod compatibility.
 * 
 * Note: ModLoader dynamically loads BML.dll APIs to avoid static linking.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <MinHook.h>

#include "CKContext.h"

// BML Core types only (no linking)
#include "bml_bootstrap.h"
#include "bml_types.h"
#include "bml_errors.h"

#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"

#include "ModManager.h"

//-----------------------------------------------------------------------------
// BML API Dynamic Loading
//-----------------------------------------------------------------------------

static HMODULE s_hBML = nullptr;

// Function pointer types
typedef BML_Result (*PFN_bmlBootstrap)(const BML_BootstrapConfig *config);
typedef void (*PFN_bmlShutdown)(void);
typedef BML_BootstrapState (*PFN_bmlGetBootstrapState)(void);
typedef void (*PFN_bmlUpdate)(void);
typedef void* (*PFN_bmlGetProcAddress)(const char*);

// Function pointers
static PFN_bmlBootstrap s_bmlBootstrap = nullptr;
static PFN_bmlShutdown s_bmlShutdown = nullptr;
static PFN_bmlGetBootstrapState s_bmlGetBootstrapState = nullptr;
static PFN_bmlUpdate s_bmlUpdate = nullptr;
static PFN_bmlGetProcAddress s_bmlGetProcAddress = nullptr;

static bool LoadBMLAPI() {
    s_hBML = ::LoadLibraryA("BML.dll");
    if (!s_hBML) {
        OutputDebugStringA("ModLoader: Fatal - Unable to load BML.dll.\n");
        return false;
    }

    s_bmlBootstrap = (PFN_bmlBootstrap)::GetProcAddress(s_hBML, "bmlBootstrap");
    s_bmlShutdown = (PFN_bmlShutdown)::GetProcAddress(s_hBML, "bmlShutdown");
    s_bmlGetBootstrapState = (PFN_bmlGetBootstrapState)::GetProcAddress(s_hBML, "bmlGetBootstrapState");
    s_bmlUpdate = (PFN_bmlUpdate)::GetProcAddress(s_hBML, "bmlUpdate");
    s_bmlGetProcAddress = (PFN_bmlGetProcAddress)::GetProcAddress(s_hBML, "bmlGetProcAddress");

    if (!s_bmlBootstrap || !s_bmlShutdown || !s_bmlGetBootstrapState || !s_bmlGetProcAddress) {
        OutputDebugStringA("ModLoader: Fatal - BML.dll missing required exports.\n");
        ::FreeLibrary(s_hBML);
        s_hBML = nullptr;
        return false;
    }

    return true;
}

static void UnloadBMLAPI() {
    s_bmlBootstrap = nullptr;
    s_bmlShutdown = nullptr;
    s_bmlGetBootstrapState = nullptr;
    s_bmlUpdate = nullptr;
    s_bmlGetProcAddress = nullptr;

    if (s_hBML) {
        ::FreeLibrary(s_hBML);
        s_hBML = nullptr;
    }
}

void ModLoaderUpdateCore() {
    if (s_bmlUpdate) {
        s_bmlUpdate();
    }
}

//-----------------------------------------------------------------------------
// CK Manager Factory Functions
//-----------------------------------------------------------------------------

static CKERROR CreateModManager(CKContext *context) {
    new ModManager(context);
    return CK_OK;
}

static CKERROR RemoveModManager(CKContext *context) {
    delete ModManager::GetManager(context);
    return CK_OK;
}

//-----------------------------------------------------------------------------
// Virtools Plugin Exports
//-----------------------------------------------------------------------------

CKPluginInfo g_PluginInfo[2];

PLUGIN_EXPORT int CKGetPluginInfoCount() {
    return 2;
}

PLUGIN_EXPORT CKPluginInfo *CKGetPluginInfo(int Index) {
    // Plugin 0: HookBlock Building Block
    g_PluginInfo[0].m_Author = "Kakuty";
    g_PluginInfo[0].m_Description = "Building blocks for hooking";
    g_PluginInfo[0].m_Extension = "";
    g_PluginInfo[0].m_Type = CKPLUGIN_BEHAVIOR_DLL;
    g_PluginInfo[0].m_Version = 0x000001;
    g_PluginInfo[0].m_InitInstanceFct = nullptr;
    g_PluginInfo[0].m_ExitInstanceFct = nullptr;
    g_PluginInfo[0].m_GUID = CKGUID(0x3a086b4d, 0x2f4a4f01);
    g_PluginInfo[0].m_Summary = "Building blocks for hooking";

    // Plugin 1: Mod Manager
    g_PluginInfo[1].m_Author = "Kakuty";
    g_PluginInfo[1].m_Description = "Mod Manager";
    g_PluginInfo[1].m_Extension = "";
    g_PluginInfo[1].m_Type = CKPLUGIN_MANAGER_DLL;
    g_PluginInfo[1].m_Version = 0x000001;
    g_PluginInfo[1].m_InitInstanceFct = CreateModManager;
    g_PluginInfo[1].m_ExitInstanceFct = RemoveModManager;
    g_PluginInfo[1].m_GUID = MOD_MANAGER_GUID;
    g_PluginInfo[1].m_Summary = "Mod Manager";

    return &g_PluginInfo[Index];
}

PLUGIN_EXPORT void RegisterBehaviorDeclarations(XObjectDeclarationArray *reg);

void RegisterBehaviorDeclarations(XObjectDeclarationArray *reg) {
    BML_BootstrapConfig config = BML_BOOTSTRAP_CONFIG_INIT;
    if (!s_bmlBootstrap || s_bmlBootstrap(&config) != BML_RESULT_OK) {
        OutputDebugStringA("ModLoader: Fatal - Unable to bootstrap modules.\n");
        if (s_bmlShutdown) s_bmlShutdown();
        return;
    }

    if (s_bmlGetBootstrapState && s_bmlGetBootstrapState() != BML_BOOTSTRAP_STATE_READY) {
        OutputDebugStringA("ModLoader: Warning - Bootstrap completed without reaching ready state.\n");
    }

    RegisterBehavior(reg, FillBehaviorHookBlockDecl);
}

//-----------------------------------------------------------------------------
// Behavior Prototype Hook
//-----------------------------------------------------------------------------

/**
 * @brief Hook to redirect CreateCKBehaviorPrototypeRunTime to CreateCKBehaviorPrototype
 * 
 * This hook allows runtime behavior prototypes to be created with the same
 * functionality as design-time prototypes, enabling behavior graph instrumentation.
 */
static bool HookCreateCKBehaviorPrototypeRuntime() {
    HMODULE handle = ::GetModuleHandleA("CK2.dll");
    if (!handle) {
        return false;
    }

    LPVOID lpCreateCKBehaviorPrototypeRunTimeProc =
        (LPVOID) ::GetProcAddress(handle, "?CreateCKBehaviorPrototypeRunTime@@YAPAVCKBehaviorPrototype@@PAD@Z");
    LPVOID lpCreateCKBehaviorPrototypeProc =
        (LPVOID) ::GetProcAddress(handle, "?CreateCKBehaviorPrototype@@YAPAVCKBehaviorPrototype@@PAD@Z");

    if (!lpCreateCKBehaviorPrototypeRunTimeProc || !lpCreateCKBehaviorPrototypeProc) {
        return false;
    }

    if (MH_CreateHook(lpCreateCKBehaviorPrototypeRunTimeProc, lpCreateCKBehaviorPrototypeProc, nullptr) != MH_OK ||
        MH_EnableHook(lpCreateCKBehaviorPrototypeRunTimeProc) != MH_OK) {
        return false;
    }
    return true;
}

static bool UnhookCreateCKBehaviorPrototypeRuntime() {
    HMODULE handle = ::GetModuleHandleA("CK2.dll");
    if (!handle) {
        return false;
    }

    LPVOID lpCreateCKBehaviorPrototypeRunTimeProc =
        (LPVOID) ::GetProcAddress(handle, "?CreateCKBehaviorPrototypeRunTime@@YAPAVCKBehaviorPrototype@@PAD@Z");

    if (!lpCreateCKBehaviorPrototypeRunTimeProc) {
        return false;
    }

    MH_DisableHook(lpCreateCKBehaviorPrototypeRunTimeProc);
    MH_RemoveHook(lpCreateCKBehaviorPrototypeRunTimeProc);
    return true;
}

//-----------------------------------------------------------------------------
// DLL Entry Point
//-----------------------------------------------------------------------------

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH: {
        if (!LoadBMLAPI()) {
            return FALSE;
        }

        BML_BootstrapConfig config = BML_BOOTSTRAP_CONFIG_INIT;
        config.flags = BML_BOOTSTRAP_FLAG_SKIP_DISCOVERY | BML_BOOTSTRAP_FLAG_SKIP_LOAD;
        if (!s_bmlBootstrap || s_bmlBootstrap(&config) != BML_RESULT_OK) {
            OutputDebugStringA("ModLoader: Fatal - Unable to initialize BML Core bootstrap.\n");
            UnloadBMLAPI();
            return FALSE;
        }
        if (!s_bmlGetProcAddress || BML_BOOTSTRAP_LOAD(s_bmlGetProcAddress) != BML_RESULT_OK) {
            OutputDebugStringA("ModLoader: Fatal - Unable to load bootstrap interface surface.\n");
            if (s_bmlShutdown) s_bmlShutdown();
            UnloadBMLAPI();
            return FALSE;
        }

        if (MH_Initialize() != MH_OK) {
            OutputDebugStringA("ModLoader: Fatal - Unable to initialize MinHook.\n");
            if (s_bmlShutdown) s_bmlShutdown();
            UnloadBMLAPI();
            return FALSE;
        }

        if (!HookCreateCKBehaviorPrototypeRuntime()) {
            OutputDebugStringA("ModLoader: Fatal - Unable to hook CKBehaviorPrototypeRuntime.\n");
            MH_Uninitialize();
            if (s_bmlShutdown) s_bmlShutdown();
            UnloadBMLAPI();
            return FALSE;
        }

        OutputDebugStringA("ModLoader: Initialized successfully.\n");
        break;
    }

    case DLL_PROCESS_DETACH:
        // Cleanup in reverse order
        UnhookCreateCKBehaviorPrototypeRuntime();

        if (MH_Uninitialize() != MH_OK) {
            OutputDebugStringA("ModLoader: Warning - Unable to uninitialize MinHook.\n");
        }

        if (s_bmlShutdown) s_bmlShutdown();
        bmlUnloadAPI();

        UnloadBMLAPI();

        OutputDebugStringA("ModLoader: Shutdown complete.\n");
        break;

    default:
        break;
    }

    return TRUE;
}
