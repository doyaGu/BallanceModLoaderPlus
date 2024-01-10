#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <MinHook.h>

#include "CKContext.h"

#include "ModManager.h"
#include "HookUtils.h"

HMODULE g_DllHandle = nullptr;
ModManager *g_ModManager = nullptr;

CKERROR CreateModManager(CKContext *context) {
    g_ModManager = new ModManager(context);

    return CK_OK;
}

CKERROR RemoveModManager(CKContext *context) {
    delete g_ModManager;

    return CK_OK;
}

CKPluginInfo g_PluginInfo[2];

PLUGIN_EXPORT int CKGetPluginInfoCount() { return 2; }

PLUGIN_EXPORT CKPluginInfo *CKGetPluginInfo(int Index) {
    g_PluginInfo[0].m_Author = "Kakuty";
    g_PluginInfo[0].m_Description = "Building blocks for hooking";
    g_PluginInfo[0].m_Extension = "";
    g_PluginInfo[0].m_Type = CKPLUGIN_BEHAVIOR_DLL;
    g_PluginInfo[0].m_Version = 0x000001;
    g_PluginInfo[0].m_InitInstanceFct = nullptr;
    g_PluginInfo[0].m_ExitInstanceFct = nullptr;
    g_PluginInfo[0].m_GUID = CKGUID(0x3a086b4d, 0x2f4a4f01);
    g_PluginInfo[0].m_Summary = "Building blocks for hooking";

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
    RegisterBehavior(reg, FillBehaviorHookBlockDecl);
}

static bool HookCreateCKBehaviorPrototypeRuntime() {
    HMODULE handle = ::GetModuleHandleA("CK2.dll");
    LPVOID lpCreateCKBehaviorPrototypeRunTimeProc =
            (LPVOID) ::GetProcAddress(handle, "?CreateCKBehaviorPrototypeRunTime@@YAPAVCKBehaviorPrototype@@PAD@Z");
    LPVOID lpCreateCKBehaviorPrototypeProc =
            (LPVOID) ::GetProcAddress(handle, "?CreateCKBehaviorPrototype@@YAPAVCKBehaviorPrototype@@PAD@Z");
    if (MH_CreateHook(lpCreateCKBehaviorPrototypeRunTimeProc, lpCreateCKBehaviorPrototypeProc, nullptr) != MH_OK ||
        MH_EnableHook(lpCreateCKBehaviorPrototypeRunTimeProc) != MH_OK) {
        return false;
    }
    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            if (MH_Initialize() != MH_OK) {
                utils::OutputDebugA("Fatal: Unable to initialize MinHook.\n");
                return FALSE;
            }
            if (!HookCreateCKBehaviorPrototypeRuntime()) {
                utils::OutputDebugA("Fatal: Unable to hook CKBehaviorPrototypeRuntime.\n");
                return FALSE;
            }
            g_DllHandle = hModule;
            break;
        case DLL_PROCESS_DETACH:
            g_DllHandle = nullptr;
            if (MH_Uninitialize() != MH_OK) {
                utils::OutputDebugA("Fatal: Unable to uninitialize MinHook.\n");
                return FALSE;
            }
            break;
        default:
            break;
    }

    return TRUE;
}