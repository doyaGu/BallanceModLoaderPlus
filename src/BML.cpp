﻿#include <Windows.h>

#include "BMLAll.h"

#include "ModLoader.h"
#include "ModManager.h"

#include "MinHook.h"

#ifdef CK_LIB
#define RegisterBehaviorDeclarations    Register_BML_BehaviorDeclarations
#define InitInstance                    _BML_InitInstance
#define ExitInstance                    _BML_ExitInstance
#define CreateNewManager                CreateNewModManager
#define RemoveManager                   RemoveModManager
#define CKGetPluginInfoCount            CKGet_BML_PluginInfoCount
#define CKGetPluginInfo                 CKGet_BML_PluginInfo
#define g_PluginInfo                    g_BML_PluginInfo
#else
#define RegisterBehaviorDeclarations    RegisterBehaviorDeclarations
#define InitInstance                    InitInstance
#define ExitInstance                    ExitInstance
#define CreateNewManager                CreateNewManager
#define RemoveManager                   RemoveManager
#define CKGetPluginInfoCount            CKGetPluginInfoCount
#define CKGetPluginInfo                 CKGetPluginInfo
#define g_PluginInfo                    g_PluginInfo
#endif

CKERROR InitInstance(CKContext *context) {
    auto &loader = ModLoader::GetInstance();
    loader.Init(context);
    if (!loader.IsInitialized())
        return CKERR_NOTINITIALIZED;

    return CK_OK;
}

CKERROR ExitInstance(CKContext *context) {
    auto &loader = ModLoader::GetInstance();
    if (loader.IsInitialized())
        loader.Release();

    return CK_OK;
}

CKERROR CreateNewManager(CKContext *context) {
    new ModManager(context);

    return CK_OK;
}

CKERROR RemoveManager(CKContext *context) {
    ModManager *man = ModManager::GetManager(context);
    if (man) delete man;

    return CK_OK;
}

CKPluginInfo g_BML_PluginInfo[2];

PLUGIN_EXPORT int CKGetPluginInfoCount() { return 2; }

PLUGIN_EXPORT CKPluginInfo *CKGetPluginInfo(int Index) {
    g_BML_PluginInfo[0].m_Author = "Gamepiaynmo";
    g_BML_PluginInfo[0].m_Description = "Ballance Mod Loader";
    g_BML_PluginInfo[0].m_Extension = "";
    g_BML_PluginInfo[0].m_Type = CKPLUGIN_BEHAVIOR_DLL;
    g_BML_PluginInfo[0].m_Version = BML_MAJOR_VER << 16 | BML_MINOR_VER;
    g_BML_PluginInfo[0].m_InitInstanceFct = InitInstance;
    g_BML_PluginInfo[0].m_ExitInstanceFct = ExitInstance;
    g_BML_PluginInfo[0].m_GUID = BML_MODLOADER_GUID;
    g_BML_PluginInfo[0].m_Summary = "Mod Loader for Ballance";

    g_BML_PluginInfo[1].m_Author = "Kakuty";
    g_BML_PluginInfo[1].m_Description = "Ballance Mod Manager";
    g_BML_PluginInfo[1].m_Extension = "";
    g_BML_PluginInfo[1].m_Type = CKPLUGIN_MANAGER_DLL;
    g_BML_PluginInfo[1].m_Version = 0x000001;
    g_BML_PluginInfo[1].m_InitInstanceFct = CreateNewManager;
    g_BML_PluginInfo[1].m_ExitInstanceFct = RemoveManager;
    g_BML_PluginInfo[1].m_GUID = BML_MODMANAGER_GUID;
    g_BML_PluginInfo[1].m_Summary = "Mod Manager for Ballance";

    return &g_BML_PluginInfo[Index];
}

PLUGIN_EXPORT void RegisterBehaviorDeclarations(XObjectDeclarationArray *reg) {
    RegisterBBs(reg);

    auto &loader = ModLoader::GetInstance();
    loader.PreloadMods();
    loader.RegisterModBBs(reg);
}

bool HookCreateCKBehaviorPrototypeRuntime() {
    VxSharedLibrary sl;
    sl.Attach(GetModuleHandle("CK2.dll"));
    LPVOID lpCreateCKBehaviorPrototypeRunTimeProc = sl.GetFunctionPtr(
        TOCKSTRING("?CreateCKBehaviorPrototypeRunTime@@YAPAVCKBehaviorPrototype@@PAD@Z"));
    LPVOID lpCreateCKBehaviorPrototypeProc = sl.GetFunctionPtr(
        TOCKSTRING("?CreateCKBehaviorPrototype@@YAPAVCKBehaviorPrototype@@PAD@Z"));
    if (MH_CreateHook(lpCreateCKBehaviorPrototypeRunTimeProc, lpCreateCKBehaviorPrototypeProc, NULL) != MH_OK ||
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
