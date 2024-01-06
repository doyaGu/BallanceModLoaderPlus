#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "CKRenderContext.h"
#include "CKParameterManager.h"

#include "InputManager.h"

#include <MinHook.h>

HMODULE g_DllHandle = nullptr;
InputManager *g_InputManager = nullptr;

#define CKOGUID_GETMOUSEPOSITION CKGUID(0x6ea0201, 0x680e3a62)
#define CKOGUID_GETMOUSEX CKGUID(0x53c51abe, 0xeba68de)
#define CKOGUID_GETMOUSEY CKGUID(0x27af3c9f, 0xdbc4eb3)

int CKKeyStringFunc(CKParameter *param, CKSTRING ValueString, CKBOOL ReadFromString) {
    if (!param) return 0;

    CKInputManager *im = (CKInputManager *) param->m_Context->GetManagerByGuid(INPUT_MANAGER_GUID);
    if (ReadFromString) {
        if (!ValueString) return 0;
        CKSTRING name = nullptr;
        if (ValueString[0] != '\0')
            name = (CKSTRING) im->GetKeyFromName(ValueString);
        param->SetValue(&name);
    } else {
        CKDWORD key = 0;
        param->GetValue(&key, FALSE);
        int len = im->GetKeyName(key, ValueString);
        if (len > 1) return len;
    }
    return 0;
}

void CK2dVectorGetMousePos(CKContext *context, CKParameterOut *res, CKParameterIn *p1, CKParameterIn *p2) {
    CKInputManager *im = (CKInputManager *) context->GetManagerByGuid(INPUT_MANAGER_GUID);
    if (im) {
        Vx2DVector pos;
        im->GetMousePosition(pos, TRUE);

        CKParameter *param = p1->GetRealSource();
        if (!param) {
            *(Vx2DVector *) res->GetWriteDataPtr() = pos;
            return;
        }

        CKBOOL absolute = FALSE;
        param->GetValue(&absolute);
        if (absolute) {
            CKRenderContext *rc = context->GetPlayerRenderContext();
            if (rc) {
                HWND hWnd = (HWND) rc->GetWindowHandle();
                POINT pt;
                pt.x = (LONG) pos.x;
                pt.y = (LONG) pos.y;
                ::ScreenToClient(hWnd, &pt);
                if (pt.x >= 0) {
                    int width = rc->GetWidth();
                    if (pt.x >= width)
                        pt.x = width - 1;
                } else {
                    pt.x = 0;
                }
                if (pt.y >= 0) {
                    int height = rc->GetHeight();
                    if (pt.y >= height)
                        pt.y = height - 1;
                } else {
                    pt.y = 0;
                }
                pos.x = (float) pt.x;
                pos.y = (float) pt.y;
            }
        }
        *(Vx2DVector *) res->GetWriteDataPtr() = pos;
    }
}

void CKIntGetMouseX(CKContext *context, CKParameterOut *res, CKParameterIn *p1, CKParameterIn *p2) {
    CKInputManager *im = (CKInputManager *) context->GetManagerByGuid(INPUT_MANAGER_GUID);
    if (im) {
        Vx2DVector pos;
        im->GetMousePosition(pos, TRUE);
        *(int *) res->GetWriteDataPtr() = (int) pos.x;
    }
}

void CKIntGetMouseY(CKContext *context, CKParameterOut *res, CKParameterIn *p1, CKParameterIn *p2) {
    CKInputManager *im = (CKInputManager *) context->GetManagerByGuid(INPUT_MANAGER_GUID);
    if (im) {
        Vx2DVector pos;
        im->GetMousePosition(pos, TRUE);
        *(int *) res->GetWriteDataPtr() = (int) pos.y;
    }
}

void CKInitializeParameterTypes(CKContext *context) {
    CKParameterTypeDesc desc;
    desc.TypeName = "Keyboard Key";
    desc.Guid = CKPGUID_KEY;
    desc.DerivedFrom = CKPGUID_INT;
    desc.Valid = TRUE;
    desc.DefaultSize = 4;
    desc.CreateDefaultFunction = nullptr;
    desc.DeleteFunction = nullptr;
    desc.SaveLoadFunction = nullptr;
    desc.StringFunction = CKKeyStringFunc;
    desc.UICreatorFunction = nullptr;
    desc.dwParam = 0;
    desc.dwFlags = 0;
    desc.Cid = 0;

    CKParameterManager *pm = context->GetParameterManager();
    pm->RegisterParameterType(&desc);
}

void CKInitializeOperationTypes(CKContext *context) {
    CKParameterManager *pm = context->GetParameterManager();
    pm->RegisterOperationType(CKOGUID_GETMOUSEPOSITION, "Get Mouse Position");
    pm->RegisterOperationType(CKOGUID_GETMOUSEX, "Get Mouse X");
    pm->RegisterOperationType(CKOGUID_GETMOUSEY, "Get Mouse Y");
}

void CKInitializeOperationFunctions(CKContext *context) {
    CKParameterManager *pm = context->GetParameterManager();

    CKGUID PGuidNone(0x1cb10760, 0x419f50c5);
    CKGUID PGuidBool(0x1ad52a8e, 0x5e741920);
    CKGUID PGuidInt(0x5a5716fd, 0x44e276d7);
    CKGUID PGuid2DVector(0x4efcb34a, 0x6079e42f);

    CKGUID OGuidGetMousePosition(0x6ea0201, 0x680e3a62);
    CKGUID OGuidGetMouseX(0x53c51abe, 0xeba68de);
    CKGUID OGuidGetMouseY(0x27af3c9f, 0xdbc4eb3);

    pm->RegisterOperationFunction(OGuidGetMouseX, PGuidInt, PGuidNone, PGuidNone, CKIntGetMouseX);
    pm->RegisterOperationFunction(OGuidGetMouseY, PGuidInt, PGuidNone, PGuidNone, CKIntGetMouseY);
    pm->RegisterOperationFunction(OGuidGetMousePosition, PGuid2DVector, PGuidNone, PGuidNone, CK2dVectorGetMousePos);
    pm->RegisterOperationFunction(OGuidGetMousePosition, PGuid2DVector, PGuidBool, PGuidNone, CK2dVectorGetMousePos);
}

void CKUnInitializeParameterTypes(CKContext *context) {
    CKParameterManager *pm = context->GetParameterManager();
    pm->UnRegisterParameterType(CKPGUID_KEY);
}

void CKUnInitializeOperationTypes(CKContext *context) {
    CKParameterManager *pm = context->GetParameterManager();
    pm->UnRegisterOperationType(CKOGUID_GETMOUSEPOSITION);
    pm->UnRegisterOperationType(CKOGUID_GETMOUSEX);
    pm->UnRegisterOperationType(CKOGUID_GETMOUSEY);
}


CKERROR CreateNewManager(CKContext *context) {
    CKInitializeParameterTypes(context);
    CKInitializeOperationTypes(context);
    CKInitializeOperationFunctions(context);

    g_InputManager = new InputManager(context);

    return CK_OK;
}

CKERROR RemoveManager(CKContext *context) {
    delete g_InputManager;

    CKUnInitializeParameterTypes(context);
    CKUnInitializeOperationTypes(context);

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

    g_PluginInfo[1].m_Author = "Virtools";
    g_PluginInfo[1].m_Description = "DirectX Keyboard/Mouse/Joystick Manager";
    g_PluginInfo[1].m_Extension = "";
    g_PluginInfo[1].m_Type = CKPLUGIN_MANAGER_DLL;
    g_PluginInfo[1].m_Version = 0x000001;
    g_PluginInfo[1].m_InitInstanceFct = CreateNewManager;
    g_PluginInfo[1].m_ExitInstanceFct = RemoveManager;
    g_PluginInfo[1].m_GUID = INPUT_MANAGER_GUID;
    g_PluginInfo[1].m_Summary = "DirectX Input Manager";
    return &g_PluginInfo[Index];
}

PLUGIN_EXPORT void RegisterBehaviorDeclarations(XObjectDeclarationArray *reg);

void RegisterBehaviorDeclarations(XObjectDeclarationArray *reg) {
    RegisterBehavior(reg, FillBehaviorHookBlockDecl);
}

bool HookCreateCKBehaviorPrototypeRuntime() {
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
            g_DllHandle = hModule;
            if (MH_Initialize() != MH_OK)
                return FALSE;
            if (!HookCreateCKBehaviorPrototypeRuntime())
                return FALSE;
            break;
        case DLL_PROCESS_DETACH:
            g_DllHandle = nullptr;
            if (MH_Uninitialize() != MH_OK)
                return FALSE;
            break;
        default:
            break;
    }
    return TRUE;
}