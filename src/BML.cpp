
#include "CKContext.h"

#include "BML/Defines.h"
#include "Behavior/Blocks/HookBlock.h"
#include "Loader/ModManager.h"

CKERROR CreateModManager(CKContext *context) {
    new ModManager(context);
    return CK_OK;
}

CKERROR RemoveModManager(CKContext *context) {
    delete ModManager::GetManager(context);
    return CK_OK;
}

CKPluginInfo g_PluginInfo[2];

extern "C" BML_EXPORT int BML_CDECL CKGetPluginInfoCount() {
    return 2;
}

extern "C" BML_EXPORT CKPluginInfo *BML_CDECL CKGetPluginInfo(int Index) {
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

extern "C" BML_EXPORT void BML_CDECL RegisterBehaviorDeclarations(XObjectDeclarationArray *reg) {
    BML::Behavior::Internal::HookBlock::Register(reg);
}
