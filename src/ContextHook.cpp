#include "ContextHook.h"

#include <MinHook.h>

#include "ModLoader.h"

CP_DEFINE_FUNCTION_PTRS(CKCreateContext)
CP_DEFINE_FUNCTION_PTRS(CKCloseContext)

CKERROR CP_FUNC_HOOK_NAME(CKCreateContext)(CKContext **iContext, WIN_HANDLE iWin, int iRenderEngine, CKDWORD Flags) {
    CKERROR err = CP_CALL_FUNCTION_ORIG(CKCreateContext, iContext, iWin, iRenderEngine, Flags);
    ModLoader::GetInstance().Init(*iContext);
    return err;
}

CKERROR CP_FUNC_HOOK_NAME(CKCloseContext)(CKContext *iContext) {
    ModLoader::GetInstance().Shutdown();
    return CP_CALL_FUNCTION_ORIG(CKCloseContext, iContext);
}

CP_DEFINE_METHOD_PTRS(CKContext, Play)
CP_DEFINE_METHOD_PTRS(CKContext, Pause)
CP_DEFINE_METHOD_PTRS(CKContext, Reset)
CP_DEFINE_METHOD_PTRS(CKContext, Process)
CP_DEFINE_METHOD_PTRS(CKContext, ClearAll)

CP_DEFINE_METHOD_PTRS(CKContext, SetInterfaceMode)

CP_DEFINE_METHOD_PTRS(CKContext, OutputToConsole)
CP_DEFINE_METHOD_PTRS(CKContext, RefreshBuildingBlocks)

CP_DEFINE_METHOD_PTRS(CKContext, ShowSetup)
CP_DEFINE_METHOD_PTRS(CKContext, ChooseObject)
CP_DEFINE_METHOD_PTRS(CKContext, Select)
CP_DEFINE_METHOD_PTRS(CKContext, SendInterfaceMessage)

CP_DEFINE_METHOD_PTRS(CKContext, UICopyObjects)
CP_DEFINE_METHOD_PTRS(CKContext, UIPasteObjects)

CP_DEFINE_METHOD_PTRS(CKContext, ExecuteManagersOnPreRender)
CP_DEFINE_METHOD_PTRS(CKContext, ExecuteManagersOnPostRender)
CP_DEFINE_METHOD_PTRS(CKContext, ExecuteManagersOnPostSpriteRender)

#define CP_CONTEXT_METHOD_NAME(Name) CP_HOOK_CLASS_NAME(CKContext)::CP_FUNC_HOOK_NAME(Name)

CKERROR CP_CONTEXT_METHOD_NAME(Play)() {
    return CP_CALL_METHOD_ORIG(Play);
}

CKERROR CP_CONTEXT_METHOD_NAME(Pause)() {
    return CP_CALL_METHOD_ORIG(Pause);
}

CKERROR CP_CONTEXT_METHOD_NAME(Reset)() {
    ModLoader::GetInstance().OnCKReset();
    CKERROR err = CP_CALL_METHOD_ORIG(Reset);
    ModLoader::GetInstance().OnCKPostReset();
    return err;
}

CKERROR CP_CONTEXT_METHOD_NAME(Process)() {
    CKERROR err = CP_CALL_METHOD_ORIG(Process);
    ModLoader::GetInstance().PostProcess();
    return err;
}

CKERROR CP_CONTEXT_METHOD_NAME(ClearAll)() {
    return CP_CALL_METHOD_ORIG(ClearAll);
}

void CP_CONTEXT_METHOD_NAME(SetInterfaceMode)(CKBOOL mode, CKUICALLBACKFCT CallBackFct, void *data) {
    CP_CALL_METHOD_ORIG(SetInterfaceMode, mode, CallBackFct, data);
}

CKERROR CP_CONTEXT_METHOD_NAME(OutputToConsole)(CKSTRING str, CKBOOL bBeep) {
    return CP_CALL_METHOD_ORIG(OutputToConsole, str, bBeep);
}

CKERROR CP_CONTEXT_METHOD_NAME(RefreshBuildingBlocks)(const XArray<CKGUID> &iGuids) {
    return CP_CALL_METHOD_ORIG(RefreshBuildingBlocks, iGuids);
}

CKERROR CP_CONTEXT_METHOD_NAME(ShowSetup)(CK_ID id) {
    return CP_CALL_METHOD_ORIG(ShowSetup, id);
}

CK_ID CP_CONTEXT_METHOD_NAME(ChooseObject)(void *dialogParentWnd) {
    return CP_CALL_METHOD_ORIG(ChooseObject, dialogParentWnd);
}

CKERROR CP_CONTEXT_METHOD_NAME(Select)(const XObjectArray &o, CKBOOL clearSelection) {
    return CP_CALL_METHOD_ORIG(Select, o, clearSelection);
}

CKDWORD CP_CONTEXT_METHOD_NAME(SendInterfaceMessage)(CKDWORD reason, CKDWORD param1, CKDWORD param2) {
    return CP_CALL_METHOD_ORIG(SendInterfaceMessage, reason, param1, param2);
}

CKERROR CP_CONTEXT_METHOD_NAME(UICopyObjects)(const XObjectArray &iObjects, CKBOOL iClearClipboard) {
    return CP_CALL_METHOD_ORIG(UICopyObjects, iObjects, iClearClipboard);
}

CKERROR CP_CONTEXT_METHOD_NAME(UIPasteObjects)(const XObjectArray &iObjects) {
    return CP_CALL_METHOD_ORIG(UIPasteObjects, iObjects);
}

void CP_CONTEXT_METHOD_NAME(ExecuteManagersOnPreRender)(CKRenderContext *dev) {
    CP_CALL_METHOD_ORIG(ExecuteManagersOnPreRender, dev);
}

void CP_CONTEXT_METHOD_NAME(ExecuteManagersOnPostRender)(CKRenderContext *dev) {
    CP_CALL_METHOD_ORIG(ExecuteManagersOnPostRender, dev);
    ModLoader::GetInstance().OnPostRender(dev);
}

void CP_CONTEXT_METHOD_NAME(ExecuteManagersOnPostSpriteRender)(CKRenderContext *dev) {
    CP_CALL_METHOD_ORIG(ExecuteManagersOnPostSpriteRender, dev);
    ModLoader::GetInstance().OnPostSpriteRender(dev);
}

bool CP_HOOK_CLASS_NAME(CKContext)::InitHooks() {
#define CP_ADD_METHOD_HOOK(Name, Module, Symbol) \
        { FARPROC proc = ::GetProcAddress(::GetModuleHandleA(Module), Symbol); \
          CP_FUNC_TARGET_PTR_NAME(Name) = *(CP_FUNC_TYPE_NAME(Name) *) &proc; } \
        if ((MH_CreateHook(*reinterpret_cast<LPVOID *>(&CP_FUNC_TARGET_PTR_NAME(Name)), \
                           *reinterpret_cast<LPVOID *>(&CP_FUNC_PTR_NAME(Name)), \
                            reinterpret_cast<LPVOID *>(&CP_FUNC_ORIG_PTR_NAME(Name))) != MH_OK || \
            MH_EnableHook(*reinterpret_cast<LPVOID *>(&CP_FUNC_TARGET_PTR_NAME(Name))) != MH_OK)) \
                return false;

    CP_ADD_METHOD_HOOK(CKCreateContext, "CK2.dll", "?CKCreateContext@@YAJPAPAVCKContext@@PAXHK@Z")
    CP_ADD_METHOD_HOOK(CKCloseContext, "CK2.dll", "?CKCloseContext@@YAJPAVCKContext@@@Z")

    CP_ADD_METHOD_HOOK(Play, "CK2.dll", "?Play@CKContext@@QAEJXZ")
    CP_ADD_METHOD_HOOK(Pause, "CK2.dll", "?Pause@CKContext@@QAEJXZ")
    CP_ADD_METHOD_HOOK(Reset, "CK2.dll", "?Reset@CKContext@@QAEJXZ")
    CP_ADD_METHOD_HOOK(Process, "CK2.dll", "?Process@CKContext@@QAEJXZ")
    CP_ADD_METHOD_HOOK(ClearAll, "CK2.dll", "?ClearAll@CKContext@@QAEJXZ")

    CP_ADD_METHOD_HOOK(SetInterfaceMode, "CK2.dll", "?SetInterfaceMode@CKContext@@QAEXHP6AJAAUCKUICallbackStruct@@PAX@Z1@Z")

    CP_ADD_METHOD_HOOK(OutputToConsole, "CK2.dll", "?OutputToConsole@CKContext@@QAEJPADH@Z")
    CP_ADD_METHOD_HOOK(RefreshBuildingBlocks, "CK2.dll", "?RefreshBuildingBlocks@CKContext@@QAEJABV?$XArray@UCKGUID@@@@@Z")

    CP_ADD_METHOD_HOOK(ShowSetup, "CK2.dll", "?ShowSetup@CKContext@@QAEJK@Z")
    CP_ADD_METHOD_HOOK(ChooseObject, "CK2.dll", "?ChooseObject@CKContext@@QAEKPAX@Z")
    CP_ADD_METHOD_HOOK(Select, "CK2.dll", "?Select@CKContext@@QAEJABVXObjectArray@@H@Z")
    CP_ADD_METHOD_HOOK(SendInterfaceMessage, "CK2.dll", "?SendInterfaceMessage@CKContext@@QAEKKKK@Z")

    CP_ADD_METHOD_HOOK(UICopyObjects, "CK2.dll", "?UICopyObjects@CKContext@@QAEJABVXObjectArray@@H@Z")
    CP_ADD_METHOD_HOOK(UIPasteObjects, "CK2.dll", "?UIPasteObjects@CKContext@@QAEJABVXObjectArray@@@Z")

    CP_ADD_METHOD_HOOK(ExecuteManagersOnPreRender, "CK2.dll", "?ExecuteManagersOnPreRender@CKContext@@QAEXPAVCKRenderContext@@@Z")
    CP_ADD_METHOD_HOOK(ExecuteManagersOnPostRender, "CK2.dll", "?ExecuteManagersOnPostRender@CKContext@@QAEXPAVCKRenderContext@@@Z")
    CP_ADD_METHOD_HOOK(ExecuteManagersOnPostSpriteRender, "CK2.dll", "?ExecuteManagersOnPostSpriteRender@CKContext@@QAEXPAVCKRenderContext@@@Z")

    return true;

#undef CP_ADD_METHOD_HOOK
}

void CP_HOOK_CLASS_NAME(CKContext)::ShutdownHooks() {
#define CP_REMOVE_METHOD_HOOK(Name) \
    MH_DisableHook(*reinterpret_cast<void **>(&CP_FUNC_TARGET_PTR_NAME(Name))); \
    MH_RemoveHook(*reinterpret_cast<void **>(&CP_FUNC_TARGET_PTR_NAME(Name)));

    CP_REMOVE_METHOD_HOOK(CKCreateContext)
    CP_REMOVE_METHOD_HOOK(CKCloseContext)

    CP_REMOVE_METHOD_HOOK(Play)
    CP_REMOVE_METHOD_HOOK(Pause)
    CP_REMOVE_METHOD_HOOK(Reset)
    CP_REMOVE_METHOD_HOOK(Process)
    CP_REMOVE_METHOD_HOOK(ClearAll)

    CP_REMOVE_METHOD_HOOK(SetInterfaceMode)

    CP_REMOVE_METHOD_HOOK(OutputToConsole)
    CP_REMOVE_METHOD_HOOK(RefreshBuildingBlocks)

    CP_REMOVE_METHOD_HOOK(ShowSetup)
    CP_REMOVE_METHOD_HOOK(ChooseObject)
    CP_REMOVE_METHOD_HOOK(Select)
    CP_REMOVE_METHOD_HOOK(SendInterfaceMessage)

    CP_REMOVE_METHOD_HOOK(UICopyObjects)
    CP_REMOVE_METHOD_HOOK(UIPasteObjects)

    CP_REMOVE_METHOD_HOOK(ExecuteManagersOnPreRender)
    CP_REMOVE_METHOD_HOOK(ExecuteManagersOnPostRender)
    CP_REMOVE_METHOD_HOOK(ExecuteManagersOnPostSpriteRender)

#undef CP_REMOVE_METHOD_HOOK
}