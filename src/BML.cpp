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

static bool EnsureProcessHooks();
static CKERROR RetainProcessHooks(CKContext *context);
static CKERROR ReleaseProcessHooks(CKContext *context);

PLUGIN_EXPORT int CKGetPluginInfoCount() {
    return EnsureProcessHooks() ? 2 : 0;
}

PLUGIN_EXPORT CKPluginInfo *CKGetPluginInfo(int Index) {
    g_PluginInfo[0].m_Author = "Kakuty";
    g_PluginInfo[0].m_Description = "Building blocks for hooking";
    g_PluginInfo[0].m_Extension = "";
    g_PluginInfo[0].m_Type = CKPLUGIN_BEHAVIOR_DLL;
    g_PluginInfo[0].m_Version = 0x000001;
    g_PluginInfo[0].m_InitInstanceFct = RetainProcessHooks;
    g_PluginInfo[0].m_ExitInstanceFct = ReleaseProcessHooks;
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
    if (!EnsureProcessHooks()) {
        utils::OutputDebugA("Fatal: Unable to initialize BML process hooks.\n");
        return;
    }
    BML::Behavior::Internal::HookBlock::Register(reg);
}

static LPVOID g_CreateCKBehaviorPrototypeRunTimeTarget = nullptr;
static bool g_CreateCKBehaviorPrototypeRunTimeEnabled = false;
static bool g_MinHookOwned = false;
static unsigned int g_ProcessHookUsers = 0;
static SRWLOCK g_ProcessHooksLock = SRWLOCK_INIT;
static bool ShutdownProcessHooksLocked();

static bool HookCreateCKBehaviorPrototypeRuntime() {
    if (g_CreateCKBehaviorPrototypeRunTimeEnabled)
        return true;

    if (!g_CreateCKBehaviorPrototypeRunTimeTarget) {
        HMODULE handle = ::GetModuleHandleA("CK2.dll");
        if (!handle)
            return false;

        LPVOID lpCreateCKBehaviorPrototypeRunTimeProc =
            (LPVOID) ::GetProcAddress(handle, "?CreateCKBehaviorPrototypeRunTime@@YAPAVCKBehaviorPrototype@@PAD@Z");
        LPVOID lpCreateCKBehaviorPrototypeProc =
            (LPVOID) ::GetProcAddress(handle, "?CreateCKBehaviorPrototype@@YAPAVCKBehaviorPrototype@@PAD@Z");
        if (!lpCreateCKBehaviorPrototypeRunTimeProc || !lpCreateCKBehaviorPrototypeProc)
            return false;

        if (MH_CreateHook(lpCreateCKBehaviorPrototypeRunTimeProc,
                          lpCreateCKBehaviorPrototypeProc, nullptr) != MH_OK) {
            return false;
        }
        g_CreateCKBehaviorPrototypeRunTimeTarget = lpCreateCKBehaviorPrototypeRunTimeProc;
    }

    const MH_STATUS enabled = MH_EnableHook(g_CreateCKBehaviorPrototypeRunTimeTarget);
    if (enabled != MH_OK && enabled != MH_ERROR_ENABLED) {
        const MH_STATUS removed = MH_RemoveHook(g_CreateCKBehaviorPrototypeRunTimeTarget);
        if (removed == MH_OK || removed == MH_ERROR_NOT_CREATED)
            g_CreateCKBehaviorPrototypeRunTimeTarget = nullptr;
        return false;
    }
    g_CreateCKBehaviorPrototypeRunTimeEnabled = true;
    return true;
}

static bool EnsureProcessHooksLocked() {
    if (g_CreateCKBehaviorPrototypeRunTimeEnabled)
        return true;

    const MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
        return false;
    if (status == MH_OK)
        g_MinHookOwned = true;

    if (HookCreateCKBehaviorPrototypeRuntime())
        return true;

    ShutdownProcessHooksLocked();
    return false;
}

static bool EnsureProcessHooks() {
    ::AcquireSRWLockExclusive(&g_ProcessHooksLock);
    const bool initialized = EnsureProcessHooksLocked();
    ::ReleaseSRWLockExclusive(&g_ProcessHooksLock);
    return initialized;
}

static bool UnhookCreateCKBehaviorPrototypeRuntime() {
    if (!g_CreateCKBehaviorPrototypeRunTimeTarget) {
        g_CreateCKBehaviorPrototypeRunTimeEnabled = false;
        return true;
    }

    const MH_STATUS disabled = MH_DisableHook(g_CreateCKBehaviorPrototypeRunTimeTarget);
    if (disabled != MH_OK && disabled != MH_ERROR_DISABLED &&
        disabled != MH_ERROR_NOT_CREATED) {
        return false;
    }
    g_CreateCKBehaviorPrototypeRunTimeEnabled = false;

    const MH_STATUS removed = MH_RemoveHook(g_CreateCKBehaviorPrototypeRunTimeTarget);
    if (removed != MH_OK && removed != MH_ERROR_NOT_CREATED)
        return false;

    g_CreateCKBehaviorPrototypeRunTimeTarget = nullptr;
    return true;
}

static bool ShutdownProcessHooksLocked() {
    if (!UnhookCreateCKBehaviorPrototypeRuntime())
        return false;

    if (g_MinHookOwned) {
        if (MH_Uninitialize() != MH_OK) {
            utils::OutputDebugA("Fatal: Unable to uninitialize MinHook.\n");
            return false;
        }
        g_MinHookOwned = false;
    }
    return true;
}

static CKERROR RetainProcessHooks(CKContext *) {
    ::AcquireSRWLockExclusive(&g_ProcessHooksLock);
    const bool initialized = EnsureProcessHooksLocked();
    if (initialized)
        ++g_ProcessHookUsers;
    ::ReleaseSRWLockExclusive(&g_ProcessHooksLock);
    return initialized ? CK_OK : CKERR_NOTINITIALIZED;
}

static CKERROR ReleaseProcessHooks(CKContext *) {
    ::AcquireSRWLockExclusive(&g_ProcessHooksLock);
    if (g_ProcessHookUsers != 0)
        --g_ProcessHookUsers;
    const bool stopped = g_ProcessHookUsers != 0 || ShutdownProcessHooksLocked();
    ::ReleaseSRWLockExclusive(&g_ProcessHooksLock);
    return stopped ? CK_OK : CKERR_NOTINITIALIZED;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID) {
    if (fdwReason == DLL_PROCESS_ATTACH)
        ::DisableThreadLibraryCalls(hModule);

    return TRUE;
}
