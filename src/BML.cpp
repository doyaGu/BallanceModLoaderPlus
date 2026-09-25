#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <MinHook.h>

#include "CKContext.h"

#include "Behavior/Blocks/HookBlock.h"
#include "Loader/ModManager.h"
#include "HookUtils.h"

CKERROR CreateModManager(CKContext *context) {
    new ModManager(context);
    return CK_OK;
}

CKERROR RemoveModManager(CKContext *context) {
    delete ModManager::GetManager(context);
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
    BML::Behavior::Internal::HookBlock::Register(reg);
}

static LPVOID g_CreateCKBehaviorPrototypeRunTimeTarget = nullptr;
static bool g_MinHookOwned = false;

static bool HookCreateCKBehaviorPrototypeRuntime() {
    HMODULE handle = ::GetModuleHandleA("CK2.dll");
    if (!handle)
        return false;

    LPVOID lpCreateCKBehaviorPrototypeRunTimeProc =
        (LPVOID) ::GetProcAddress(handle, "?CreateCKBehaviorPrototypeRunTime@@YAPAVCKBehaviorPrototype@@PAD@Z");
    LPVOID lpCreateCKBehaviorPrototypeProc =
        (LPVOID) ::GetProcAddress(handle, "?CreateCKBehaviorPrototype@@YAPAVCKBehaviorPrototype@@PAD@Z");
    if (!lpCreateCKBehaviorPrototypeRunTimeProc || !lpCreateCKBehaviorPrototypeProc)
        return false;

    if (MH_CreateHook(lpCreateCKBehaviorPrototypeRunTimeProc, lpCreateCKBehaviorPrototypeProc, nullptr) != MH_OK)
        return false;
    if (MH_EnableHook(lpCreateCKBehaviorPrototypeRunTimeProc) != MH_OK) {
        MH_RemoveHook(lpCreateCKBehaviorPrototypeRunTimeProc);
        return false;
    }
    g_CreateCKBehaviorPrototypeRunTimeTarget = lpCreateCKBehaviorPrototypeRunTimeProc;
    return true;
}

static void UnhookCreateCKBehaviorPrototypeRuntime() {
    if (g_CreateCKBehaviorPrototypeRunTimeTarget) {
        MH_DisableHook(g_CreateCKBehaviorPrototypeRunTimeTarget);
        MH_RemoveHook(g_CreateCKBehaviorPrototypeRunTimeTarget);
        g_CreateCKBehaviorPrototypeRunTimeTarget = nullptr;
    }
}
void BML_ShutdownProcessHooks() {
    UnhookCreateCKBehaviorPrototypeRuntime();

    if (g_MinHookOwned) {
        if (MH_Uninitialize() != MH_OK)
            utils::OutputDebugA("Fatal: Unable to uninitialize MinHook.\n");
        g_MinHookOwned = false;
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        ::DisableThreadLibraryCalls(hModule);
        {
            const MH_STATUS status = MH_Initialize();
            if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
                utils::OutputDebugA("Fatal: Unable to initialize MinHook.\n");
                return FALSE;
            }
            g_MinHookOwned = status == MH_OK;
        }
        if (!HookCreateCKBehaviorPrototypeRuntime()) {
            utils::OutputDebugA("Fatal: Unable to hook CKBehaviorPrototypeRuntime.\n");
            BML_ShutdownProcessHooks();
            return FALSE;
        }
        break;
    case DLL_PROCESS_DETACH:
        if (!lpReserved)
            BML_ShutdownProcessHooks();
        break;
    default:
        break;
    }

    return TRUE;
}
