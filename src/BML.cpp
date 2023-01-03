#include <Windows.h>

#include "BMLAll.h"

#include "ModLoader.h"
#include "HookManager.h"

#include "MinHook.h"

namespace ExecuteBB {
    void Init();
}

void RegisterCallbacks(HookManager *hookManager) {
    hookManager->AddOnCKInitCallBack([](CKContext *context, void *) {
        ModLoader::GetInstance().Init(context);
    }, nullptr, FALSE);

    hookManager->AddOnCKEndCallBack([](CKContext *, void *) {
        auto &loader = ModLoader::GetInstance();
        if (loader.IsInitialized()) {
            loader.UnloadMods();
            loader.Release();
        }
    }, nullptr, FALSE);

    hookManager->AddOnCKResetCallBack([](CKContext *, void *) {
        ModLoader::GetInstance().SetReset();
    }, nullptr, FALSE);

    hookManager->AddOnCKResetCallBack([](CKContext *, void *) {
        ModLoader::GetInstance().SetReset();
    }, nullptr, FALSE);

    hookManager->AddOnCKPostResetCallBack([](CKContext *, void *) {
        auto &loader = ModLoader::GetInstance();
        if (loader.GetModCount() == 0) {
            ExecuteBB::Init();
            loader.LoadMods();
        }
    }, nullptr, FALSE);

    hookManager->AddPostProcessCallBack([](CKContext *context, void *) {
        ModLoader::GetInstance().OnProcess();
    }, nullptr, FALSE);

    hookManager->AddPreRenderCallBack([](CKRenderContext *dev, void *) {
        ModLoader::GetInstance().OnPreRender(dev);
    }, nullptr, FALSE);

    hookManager->AddPostRenderCallBack([](CKRenderContext *dev, void *) {
        ModLoader::GetInstance().OnPostRender(dev);
    }, nullptr, FALSE);
}

CKERROR CreateNewHookManager(CKContext *context) {
    RegisterCallbacks(new HookManager(context));

    return CK_OK;
}

CKERROR RemoveHookManager(CKContext *context) {
    HookManager *man = HookManager::GetManager(context);
    delete man;

    return CK_OK;
}

CKPluginInfo g_PluginInfo[2];

PLUGIN_EXPORT int CKGetPluginInfoCount() { return 2; }

PLUGIN_EXPORT CKPluginInfo *CKGetPluginInfo(int Index) {
    g_PluginInfo[0].m_Author = "Kakuty";
    g_PluginInfo[0].m_Description = "Building blocks for hooking";
    g_PluginInfo[0].m_Extension = "";
    g_PluginInfo[0].m_Type = CKPLUGIN_BEHAVIOR_DLL;
    g_PluginInfo[0].m_Version = BML_MAJOR_VER << 16 | BML_MINOR_VER;
    g_PluginInfo[0].m_InitInstanceFct = nullptr;
    g_PluginInfo[0].m_ExitInstanceFct = nullptr;
    g_PluginInfo[0].m_GUID = CKGUID(0x3a086b4d, 0x2f4a4f01);
    g_PluginInfo[0].m_Summary = "Building blocks for hooking";

    g_PluginInfo[1].m_Author = "Kakuty";
    g_PluginInfo[1].m_Description = "Hook Manager";
    g_PluginInfo[1].m_Extension = "";
    g_PluginInfo[1].m_Type = CKPLUGIN_MANAGER_DLL;
    g_PluginInfo[1].m_Version = 0x000001;
    g_PluginInfo[1].m_InitInstanceFct = CreateNewHookManager;
    g_PluginInfo[1].m_ExitInstanceFct = RemoveHookManager;
    g_PluginInfo[1].m_GUID = BML_HOOKMANAGER_GUID;
    g_PluginInfo[1].m_Summary = "Virtools Hook Manager";

    return &g_PluginInfo[Index];
}

PLUGIN_EXPORT void RegisterBehaviorDeclarations(XObjectDeclarationArray *reg);

void RegisterBehaviorDeclarations(XObjectDeclarationArray *reg) {
    RegisterBehavior(reg, FillBehaviorHookBlockDecl);

    auto &loader = ModLoader::GetInstance();
    loader.PreloadMods();
    loader.RegisterBBs(reg);
}

bool HookCreateCKBehaviorPrototypeRuntime() {
    HMODULE handle = ::GetModuleHandleA("CK2.dll");
    LPVOID lpCreateCKBehaviorPrototypeRunTimeProc = (LPVOID) ::GetProcAddress(handle,
        "?CreateCKBehaviorPrototypeRunTime@@YAPAVCKBehaviorPrototype@@PAD@Z");
    LPVOID lpCreateCKBehaviorPrototypeProc = (LPVOID) ::GetProcAddress(handle,
        "?CreateCKBehaviorPrototype@@YAPAVCKBehaviorPrototype@@PAD@Z");
    if (MH_CreateHook(lpCreateCKBehaviorPrototypeRunTimeProc, lpCreateCKBehaviorPrototypeProc, nullptr) != MH_OK ||
        MH_EnableHook(lpCreateCKBehaviorPrototypeRunTimeProc) != MH_OK) {
        return false;
    }
    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            if (MH_Initialize() != MH_OK)
                return FALSE;
            if (!HookCreateCKBehaviorPrototypeRuntime())
                return FALSE;
            break;
        case DLL_PROCESS_DETACH:
            if (MH_Uninitialize() != MH_OK)
                return FALSE;
            break;
    }
    return TRUE;
}
