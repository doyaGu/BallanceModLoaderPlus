#include <Windows.h>

#include "BMLAll.h"

#include "ModLoader.h"
#include "ModManager.h"
#include "RegisterBB.h"

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

PLUGIN_EXPORT void RegisterBehaviorDeclarations(XObjectDeclarationArray *reg);

void RegisterBuildingBlockHooks(XObjectDeclarationArray *reg);

void RegisterBehaviorDeclarations(XObjectDeclarationArray *reg) {
    RegisterBuildingBlockHooks(reg);

    auto &loader = ModLoader::GetInstance();
    loader.PreloadMods();
    loader.RegisterModBBs(reg);
}

std::vector<std::unique_ptr<BuildingBlockHook>> g_BuildingBlockHooks;

void RegisterBuildingBlockHooks(XObjectDeclarationArray *reg) {
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnPreStartMenu", "PreStartMenu Hook.", BML_ONPRESTARTMENU_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPreStartMenu(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook( "BML OnPostStartMenu", "PostStartMenu Hook.", BML_ONPOSTSTARTMENU_GUID,
                                                            [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPostStartMenu(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnExitGame", "ExitGame Hook.", BML_ONEXITGAME_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnExitGame(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnPreLoadLevel", "PreLoadLevel Hook.", BML_ONPRELOADLEVEL_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPreLoadLevel(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnPostLoadLevel", "PostLoadLevel Hook.", BML_ONPOSTLOADLEVEL_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPostLoadLevel(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnStartLevel", "StartLevel Hook.", BML_ONSTARTLEVEL_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnStartLevel(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnPreResetLevel", "PreResetLevel Hook.", BML_ONPRERESETLEVEL_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPreResetLevel(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnPostResetLevel", "PostResetLevel Hook.", BML_ONPOSTRESETLEVEL_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPostResetLevel(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnPauseLevel", "PauseLevel Hook.", BML_ONPAUSELEVEL_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPauseLevel(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnUnpauseLevel", "UnpauseLevel Hook.", BML_ONUNPAUSELEVEL_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnUnpauseLevel(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnPreExitLevel", "PreExitLevel Hook.", BML_ONPREEXITLEVEL_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPreExitLevel(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnPostExitLevel", "PostExitLevel Hook.", BML_ONPOSTEXITLEVEL_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPostExitLevel(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnPreNextLevel", "PreNextLevel Hook.", BML_ONPRENEXTLEVEL_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPreNextLevel(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnPostNextLevel", "PostNextLevel Hook.", BML_ONPOSTNEXTLEVEL_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPostNextLevel(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnDead", "Dead Hook.", BML_ONDEAD_GUID, [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnDead(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnPreEndLevel", "PreEndLevel Hook.", BML_ONPREENDLEVEL_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPreEndLevel(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnPostEndLevel", "PostEndLevel Hook.", BML_ONPOSTENDLEVEL_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPostEndLevel(); return CK_OK; }));

    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnCounterActive", "CounterActive Hook.", BML_ONCOUNTERACTIVE_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnCounterActive(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnCounterInactive", "CounterInactive Hook.", BML_ONCOUNTERINACTIVE_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnCounterInactive(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnBallNavActive", "BallNavActive Hook.", BML_ONBALLNAVACTIVE_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnBallNavActive(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnBallNavInactive", "BallNavInactive Hook.", BML_ONBALLNAVINACTIVE_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnBallNavInactive(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnCamNavActive", "CamNavActive Hook.", BML_ONCAMNAVACTIVE_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnCamNavActive(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnCamNavInactive", "CamNavInactive Hook.", BML_ONCAMNAVINACTIVE_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnCamNavInactive(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnBallOff", "BallOff Hook.", BML_ONBALLOFF_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnBallOff(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnPreCheckpoint", "PreCheckpoint Hook.", BML_ONPRECHECKPOINT_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPreCheckpointReached(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnPostCheckpoint", "PostCheckpoint Hook.", BML_ONPOSTCHECKPOINT_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPostCheckpointReached(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnLevelFinish", "LevelFinish Hook.", BML_ONLEVELFINISH_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnLevelFinish(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnGameOver", "GameOver Hook.", BML_ONGAMEOVER_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnGameOver(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnExtraPoint", "ExtraPoint Hook.", BML_ONEXTRAPOINT_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnExtraPoint(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnPreSubLife", "PreSubLife Hook.", BML_ONPRESUBLIFE_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPreSubLife(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML ONPostSubLife", "PostSubLife Hook.", BML_ONPOSTSUBLIFE_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPostSubLife(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnPreLifeUp", "PreLifeUp Hook.", BML_ONPRELIFEUP_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPreLifeUp(); return CK_OK; }));
    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML OnPostLifeUp", "PostLifeUp Hook.", BML_ONPOSTLIFEUP_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OnPostLifeUp(); return CK_OK; }));

    g_BuildingBlockHooks.push_back(CreateBuildingBlockHook("BML ModsMenu", "Show BML Mods Menu.", BML_MODSMENU_GUID,
                                                           [](const CKBehaviorContext &behcontext) { ModLoader::GetInstance().OpenModsMenu(); return CK_OK; }));

    for (auto &hook : g_BuildingBlockHooks) {
        hook->Register(reg);
    }
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
