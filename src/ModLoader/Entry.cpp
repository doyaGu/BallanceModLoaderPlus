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
#include "bml_types.h"
#include "bml_errors.h"

#include "ModManager.h"

//-----------------------------------------------------------------------------
// BML API Dynamic Loading
//-----------------------------------------------------------------------------

static HMODULE s_hBML = nullptr;

// Function pointer types
typedef BML_Result (*PFN_bmlAttach)(void);
typedef BML_Result (*PFN_bmlDiscoverModules)(void);
typedef BML_Result (*PFN_bmlLoadModules)(void);
typedef void (*PFN_bmlDetach)(void);
typedef void* (*PFN_bmlGetProcAddress)(const char*);

// Function pointers
static PFN_bmlAttach s_bmlAttach = nullptr;
static PFN_bmlDiscoverModules s_bmlDiscoverModules = nullptr;
static PFN_bmlLoadModules s_bmlLoadModules = nullptr;
static PFN_bmlDetach s_bmlDetach = nullptr;
static PFN_bmlGetProcAddress s_bmlGetProcAddress = nullptr;

static bool LoadBMLAPI() {
    s_hBML = ::LoadLibraryA("BML.dll");
    if (!s_hBML) {
        OutputDebugStringA("ModLoader: Fatal - Unable to load BML.dll.\n");
        return false;
    }

    s_bmlAttach = (PFN_bmlAttach)::GetProcAddress(s_hBML, "bmlAttach");
    s_bmlDiscoverModules = (PFN_bmlDiscoverModules)::GetProcAddress(s_hBML, "bmlDiscoverModules");
    s_bmlLoadModules = (PFN_bmlLoadModules)::GetProcAddress(s_hBML, "bmlLoadModules");
    s_bmlDetach = (PFN_bmlDetach)::GetProcAddress(s_hBML, "bmlDetach");
    s_bmlGetProcAddress = (PFN_bmlGetProcAddress)::GetProcAddress(s_hBML, "bmlGetProcAddress");

    if (!s_bmlAttach || !s_bmlDiscoverModules || !s_bmlLoadModules || !s_bmlDetach || !s_bmlGetProcAddress) {
        OutputDebugStringA("ModLoader: Fatal - BML.dll missing required exports.\n");
        ::FreeLibrary(s_hBML);
        s_hBML = nullptr;
        return false;
    }

    return true;
}

static void UnloadBMLAPI() {
    s_bmlAttach = nullptr;
    s_bmlDiscoverModules = nullptr;
    s_bmlLoadModules = nullptr;
    s_bmlDetach = nullptr;
    s_bmlGetProcAddress = nullptr;

    if (s_hBML) {
        ::FreeLibrary(s_hBML);
        s_hBML = nullptr;
    }
}

// Accessor for ModManager to get bmlGetProcAddress
void* ModLoaderGetProcAddress(const char* name) {
    return s_bmlGetProcAddress ? s_bmlGetProcAddress(name) : nullptr;
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
    // Discover modules
    if (!s_bmlDiscoverModules || s_bmlDiscoverModules() != BML_RESULT_OK) {
        OutputDebugStringA("ModLoader: Fatal - Unable to discover modules.\n");
        if (s_bmlDetach) s_bmlDetach();
        return;
    }

    // Load discovered modules
    if (!s_bmlLoadModules || s_bmlLoadModules() != BML_RESULT_OK) {
        OutputDebugStringA("ModLoader: Warning - Failed to load some modules.\n");
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
    case DLL_PROCESS_ATTACH:
        // Phase 0: Load BML.dll dynamically
        if (!LoadBMLAPI()) {
            return FALSE;
        }

        // Phase 1: Initialize BML Core microkernel
        if (!s_bmlAttach || s_bmlAttach() != BML_RESULT_OK) {
            OutputDebugStringA("ModLoader: Fatal - Unable to initialize BML Core microkernel.\n");
            UnloadBMLAPI();
            return FALSE;
        }

        // Phase 2: Initialize MinHook
        if (MH_Initialize() != MH_OK) {
            OutputDebugStringA("ModLoader: Fatal - Unable to initialize MinHook.\n");
            if (s_bmlDetach) s_bmlDetach();
            UnloadBMLAPI();
            return FALSE;
        }

        // Phase 3: Hook behavior prototype creation
        if (!HookCreateCKBehaviorPrototypeRuntime()) {
            OutputDebugStringA("ModLoader: Fatal - Unable to hook CKBehaviorPrototypeRuntime.\n");
            MH_Uninitialize();
            if (s_bmlDetach) s_bmlDetach();
            UnloadBMLAPI();
            return FALSE;
        }

        OutputDebugStringA("ModLoader: Initialized successfully.\n");
        break;

    case DLL_PROCESS_DETACH:
        // Cleanup in reverse order
        UnhookCreateCKBehaviorPrototypeRuntime();

        if (MH_Uninitialize() != MH_OK) {
            OutputDebugStringA("ModLoader: Warning - Unable to uninitialize MinHook.\n");
        }

        // Shutdown Core microkernel
        if (s_bmlDetach) s_bmlDetach();

        // Unload BML.dll
        UnloadBMLAPI();

        OutputDebugStringA("ModLoader: Shutdown complete.\n");
        break;

    default:
        break;
    }

    return TRUE;
}
