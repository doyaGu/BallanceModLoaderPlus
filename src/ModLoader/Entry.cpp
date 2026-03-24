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

#include <filesystem>

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
typedef BML_Result (*PFN_bmlRuntimeCreate)(const BML_RuntimeConfig *config, BML_Runtime *out_runtime);
typedef BML_Result (*PFN_bmlRuntimeDiscoverModules)(BML_Runtime runtime);
typedef BML_Result (*PFN_bmlRuntimeLoadModules)(BML_Runtime runtime);
typedef void (*PFN_bmlRuntimeDestroy)(BML_Runtime runtime);
typedef void (*PFN_bmlRuntimeUpdate)(BML_Runtime runtime);
typedef const BML_Services *(*PFN_bmlRuntimeGetServices)(BML_Runtime runtime);

// Function pointers
static PFN_bmlRuntimeCreate s_bmlRuntimeCreate = nullptr;
static PFN_bmlRuntimeDiscoverModules s_bmlRuntimeDiscoverModules = nullptr;
static PFN_bmlRuntimeLoadModules s_bmlRuntimeLoadModules = nullptr;
static PFN_bmlRuntimeDestroy s_bmlRuntimeDestroy = nullptr;
static PFN_bmlRuntimeUpdate s_bmlRuntimeUpdate = nullptr;
static PFN_bmlRuntimeGetServices s_bmlRuntimeGetServices = nullptr;
static BML_Runtime s_Runtime = nullptr;
static const BML_Services *s_Services = nullptr;

const BML_Services *GetModLoaderServices() {
    return s_Services;
}

static bool LoadBMLAPI() {
    // Resolve BML.dll path relative to this DLL's directory to prevent DLL hijacking.
    // ModLoader.dll lives in BuildingBlocks/, BML.dll lives in Bin/.
    HMODULE hSelf = nullptr;
    ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(&LoadBMLAPI), &hSelf);
    wchar_t selfPath[MAX_PATH]{};
    ::GetModuleFileNameW(hSelf, selfPath, MAX_PATH);
    std::filesystem::path bmlPath = std::filesystem::path(selfPath).parent_path() / L".." / L"Bin" / L"BML.dll";
    bmlPath = bmlPath.lexically_normal();
    s_hBML = ::LoadLibraryW(bmlPath.c_str());
    if (!s_hBML) {
        OutputDebugStringA("ModLoader: Fatal - Unable to load BML.dll.\n");
        return false;
    }

    s_bmlRuntimeCreate = (PFN_bmlRuntimeCreate)::GetProcAddress(s_hBML, "bmlRuntimeCreate");
    s_bmlRuntimeDiscoverModules =
        (PFN_bmlRuntimeDiscoverModules)::GetProcAddress(s_hBML, "bmlRuntimeDiscoverModules");
    s_bmlRuntimeLoadModules =
        (PFN_bmlRuntimeLoadModules)::GetProcAddress(s_hBML, "bmlRuntimeLoadModules");
    s_bmlRuntimeDestroy = (PFN_bmlRuntimeDestroy)::GetProcAddress(s_hBML, "bmlRuntimeDestroy");
    s_bmlRuntimeUpdate = (PFN_bmlRuntimeUpdate)::GetProcAddress(s_hBML, "bmlRuntimeUpdate");
    s_bmlRuntimeGetServices =
        (PFN_bmlRuntimeGetServices)::GetProcAddress(s_hBML, "bmlRuntimeGetServices");

    if (!s_bmlRuntimeCreate || !s_bmlRuntimeDiscoverModules || !s_bmlRuntimeLoadModules ||
        !s_bmlRuntimeDestroy || !s_bmlRuntimeGetServices) {
        OutputDebugStringA("ModLoader: Fatal - BML.dll missing required exports.\n");
        ::FreeLibrary(s_hBML);
        s_hBML = nullptr;
        return false;
    }

    return true;
}

static void UnloadBMLAPI() {
    s_bmlRuntimeCreate = nullptr;
    s_bmlRuntimeDiscoverModules = nullptr;
    s_bmlRuntimeLoadModules = nullptr;
    s_bmlRuntimeDestroy = nullptr;
    s_bmlRuntimeUpdate = nullptr;
    s_bmlRuntimeGetServices = nullptr;
    s_Runtime = nullptr;
    s_Services = nullptr;

    if (s_hBML) {
        ::FreeLibrary(s_hBML);
        s_hBML = nullptr;
    }
}

void ModLoaderUpdateCore() {
    if (s_bmlRuntimeUpdate && s_Runtime) {
        s_bmlRuntimeUpdate(s_Runtime);
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
    if (!s_Runtime || !s_bmlRuntimeDiscoverModules || s_bmlRuntimeDiscoverModules(s_Runtime) != BML_RESULT_OK) {
        OutputDebugStringA("ModLoader: Fatal - Unable to bootstrap modules.\n");
        if (s_bmlRuntimeDestroy && s_Runtime) {
            s_bmlRuntimeDestroy(s_Runtime);
            s_Runtime = nullptr;
        }
        return;
    }
    if (!s_bmlRuntimeLoadModules || s_bmlRuntimeLoadModules(s_Runtime) != BML_RESULT_OK) {
        OutputDebugStringA("ModLoader: Fatal - Unable to load modules.\n");
        if (s_bmlRuntimeDestroy && s_Runtime) {
            s_bmlRuntimeDestroy(s_Runtime);
            s_Runtime = nullptr;
        }
        return;
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

        BML_RuntimeConfig config = BML_RUNTIME_CONFIG_INIT;
        if (!s_bmlRuntimeCreate || s_bmlRuntimeCreate(&config, &s_Runtime) != BML_RESULT_OK) {
            OutputDebugStringA("ModLoader: Fatal - Unable to create BML runtime.\n");
            UnloadBMLAPI();
            return FALSE;
        }
        const BML_Services *services =
            s_bmlRuntimeGetServices ? s_bmlRuntimeGetServices(s_Runtime) : nullptr;
        if (!services) {
            OutputDebugStringA("ModLoader: Fatal - Unable to acquire runtime services.\n");
            if (s_bmlRuntimeDestroy && s_Runtime) {
                s_bmlRuntimeDestroy(s_Runtime);
                s_Runtime = nullptr;
            }
            UnloadBMLAPI();
            return FALSE;
        }
        s_Services = services;
        bmlBindServices(services);

        if (MH_Initialize() != MH_OK) {
            OutputDebugStringA("ModLoader: Fatal - Unable to initialize MinHook.\n");
            if (s_bmlRuntimeDestroy && s_Runtime) {
                s_bmlRuntimeDestroy(s_Runtime);
                s_Runtime = nullptr;
            }
            UnloadBMLAPI();
            return FALSE;
        }

        if (!HookCreateCKBehaviorPrototypeRuntime()) {
            OutputDebugStringA("ModLoader: Fatal - Unable to hook CKBehaviorPrototypeRuntime.\n");
            MH_Uninitialize();
            if (s_bmlRuntimeDestroy && s_Runtime) {
                s_bmlRuntimeDestroy(s_Runtime);
                s_Runtime = nullptr;
            }
            UnloadBMLAPI();
            return FALSE;
        }

        OutputDebugStringA("ModLoader: Initialized successfully.\n");
        break;
    }

    case DLL_PROCESS_DETACH:
        s_Services = nullptr;
        // Cleanup in reverse order
        UnhookCreateCKBehaviorPrototypeRuntime();

        if (MH_Uninitialize() != MH_OK) {
            OutputDebugStringA("ModLoader: Warning - Unable to uninitialize MinHook.\n");
        }

        if (s_bmlRuntimeDestroy && s_Runtime) {
            s_bmlRuntimeDestroy(s_Runtime);
            s_Runtime = nullptr;
        }
        bmlUnloadAPI();

        UnloadBMLAPI();

        OutputDebugStringA("ModLoader: Shutdown complete.\n");
        break;

    default:
        break;
    }

    return TRUE;
}
