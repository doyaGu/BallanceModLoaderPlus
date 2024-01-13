#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <MinHook.h>

#include "CKContext.h"

#include "InputManager.h"
#include "SoundManager.h"
#include "PluginManagerHook.h"
#include "ContextHook.h"
#include "HookUtils.h"

HMODULE g_DllHandle = nullptr;
InputManager *g_InputManager = nullptr;
SoundManager *g_SoundManager = nullptr;

CKERROR CreateInputManager(CKContext *context) {
    CKInitializeParameterTypes(context);
    CKInitializeOperationTypes(context);
    CKInitializeOperationFunctions(context);

    g_InputManager = new InputManager(context);

    return CK_OK;
}

CKERROR RemoveInputManager(CKContext *context) {
    delete g_InputManager;

    CKUnInitializeParameterTypes(context);
    CKUnInitializeOperationTypes(context);

    return CK_OK;
}

CKERROR CreateSoundManager(CKContext *context) {
    g_SoundManager = new SoundManager(context);
    return CK_OK;
}

CKERROR RemoveSoundManager(CKContext *context) {
    delete g_SoundManager;
    return CK_OK;
}

CKPluginInfo g_PluginInfo[3];

PLUGIN_EXPORT int CKGetPluginInfoCount() { return 3; }

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

    g_PluginInfo[1].m_Author = "Virtools";
    g_PluginInfo[1].m_Description = "DirectX Keyboard/Mouse/Joystick Manager";
    g_PluginInfo[1].m_Extension = "";
    g_PluginInfo[1].m_Type = CKPLUGIN_MANAGER_DLL;
    g_PluginInfo[1].m_Version = 0x000001;
    g_PluginInfo[1].m_InitInstanceFct = CreateInputManager;
    g_PluginInfo[1].m_ExitInstanceFct = RemoveInputManager;
    g_PluginInfo[1].m_GUID = CKGUID(0xF787C904, 0);
    g_PluginInfo[1].m_Summary = "DirectX Input Manager";

    g_PluginInfo[2].m_Author = "Virtools";
    g_PluginInfo[2].m_Description = "DirectX Sound Manager";
    g_PluginInfo[2].m_Extension = "";
    g_PluginInfo[2].m_Type = CKPLUGIN_MANAGER_DLL;
    g_PluginInfo[2].m_Version = 0x000001;
    g_PluginInfo[2].m_InitInstanceFct = CreateSoundManager;
    g_PluginInfo[2].m_ExitInstanceFct = RemoveSoundManager;
    g_PluginInfo[2].m_GUID = CKGUID(0x77135393, 0x225c679a);
    g_PluginInfo[2].m_Summary = "DirectX Sound Manager";

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
            if (!CP_HOOK_CLASS_NAME(CKPluginManager)::InitHooks()) {
                utils::OutputDebugA("Fatal: Unable to hook CKPluginManager.\n");
                return FALSE;
            }
            if (!CP_HOOK_CLASS_NAME(CKContext)::InitHooks()) {
                utils::OutputDebugA("Fatal: Unable to hook CKContext.\n");
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
            CP_HOOK_CLASS_NAME(CKContext)::ShutdownHooks();
            CP_HOOK_CLASS_NAME(CKPluginManager)::ShutdownHooks();
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