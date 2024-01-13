#ifndef HOOKS_CONTEXTHOOK_H
#define HOOKS_CONTEXTHOOK_H

#include "CKContext.h"

#include "Macros.h"

CKERROR CP_FUNC_HOOK_NAME(CKCreateContext)(CKContext **iContext, WIN_HANDLE iWin, int iRenderEngine, CKDWORD Flags);
CKERROR CP_FUNC_HOOK_NAME(CKCloseContext)(CKContext *iContext);

CP_DECLARE_FUNCTION_PTRS(CKERROR, CKCreateContext, (CKContext **iContext, WIN_HANDLE iWin, int iRenderEngine, CKDWORD Flags));
CP_DECLARE_FUNCTION_PTRS(CKERROR, CKCloseContext, (CKContext *iContext));

CP_HOOK_CLASS(CKContext) {
public:
    // Engine runtime
    CP_DECLARE_METHOD_HOOK(CKERROR, Play, ());
    CP_DECLARE_METHOD_HOOK(CKERROR, Pause, ());
    CP_DECLARE_METHOD_HOOK(CKERROR, Reset, ());
    CP_DECLARE_METHOD_HOOK(CKERROR, Process, ());
    CP_DECLARE_METHOD_HOOK(CKERROR, ClearAll, ());

    // IHM
    CP_DECLARE_METHOD_HOOK(void, SetInterfaceMode, (CKBOOL mode, CKUICALLBACKFCT CallBackFct, void *data));

    CP_DECLARE_METHOD_HOOK(CKERROR, OutputToConsole, (CKSTRING str, CKBOOL bBeep));
    CP_DECLARE_METHOD_HOOK(CKERROR, RefreshBuildingBlocks, (const XArray<CKGUID> &iGuids));

    CP_DECLARE_METHOD_HOOK(CKERROR, ShowSetup, (CK_ID id));
    CP_DECLARE_METHOD_HOOK(CK_ID, ChooseObject, (void *dialogParentWnd));
    CP_DECLARE_METHOD_HOOK(CKERROR, Select, (const XObjectArray &o, CKBOOL clearSelection));
    CP_DECLARE_METHOD_HOOK(CKDWORD, SendInterfaceMessage, (CKDWORD reason, CKDWORD param1, CKDWORD param2));

    CP_DECLARE_METHOD_HOOK(CKERROR, UICopyObjects, (const XObjectArray &iObjects, CKBOOL iClearClipboard));
    CP_DECLARE_METHOD_HOOK(CKERROR, UIPasteObjects, (const XObjectArray &oObjects));

    //	Render Engine Implementation Specific
    CP_DECLARE_METHOD_HOOK(void, ExecuteManagersOnPreRender, (CKRenderContext *dev));
    CP_DECLARE_METHOD_HOOK(void, ExecuteManagersOnPostRender, (CKRenderContext *dev));
    CP_DECLARE_METHOD_HOOK(void, ExecuteManagersOnPostSpriteRender, (CKRenderContext *dev));

    // Method Pointers

    CP_DECLARE_METHOD_PTRS(CKContext, CKERROR, Play, ());
    CP_DECLARE_METHOD_PTRS(CKContext, CKERROR, Pause, ());
    CP_DECLARE_METHOD_PTRS(CKContext, CKERROR, Reset, ());
    CP_DECLARE_METHOD_PTRS(CKContext, CKERROR, Process, ());
    CP_DECLARE_METHOD_PTRS(CKContext, CKERROR, ClearAll, ());

    CP_DECLARE_METHOD_PTRS(CKContext, void, SetInterfaceMode, (CKBOOL mode, CKUICALLBACKFCT CallBackFct, void * data));

    CP_DECLARE_METHOD_PTRS(CKContext, CKERROR, OutputToConsole, (CKSTRING str, CKBOOL bBeep));
    CP_DECLARE_METHOD_PTRS(CKContext, CKERROR, RefreshBuildingBlocks, (const XArray<CKGUID> &iGuids));

    CP_DECLARE_METHOD_PTRS(CKContext, CKERROR, ShowSetup, (CK_ID id));
    CP_DECLARE_METHOD_PTRS(CKContext, CK_ID, ChooseObject, (void * dialogParentWnd));
    CP_DECLARE_METHOD_PTRS(CKContext, CKERROR, Select, (const XObjectArray &o, CKBOOL clearSelection));
    CP_DECLARE_METHOD_PTRS(CKContext, CKDWORD, SendInterfaceMessage, (CKDWORD reason, CKDWORD param1, CKDWORD param2));

    CP_DECLARE_METHOD_PTRS(CKContext, CKERROR, UICopyObjects, (const XObjectArray &iObjects, CKBOOL iClearClipboard));
    CP_DECLARE_METHOD_PTRS(CKContext, CKERROR, UIPasteObjects, (const XObjectArray &oObjects));

    CP_DECLARE_METHOD_PTRS(CKContext, void, ExecuteManagersOnPreRender, (CKRenderContext *dev));
    CP_DECLARE_METHOD_PTRS(CKContext, void, ExecuteManagersOnPostRender, (CKRenderContext *dev));
    CP_DECLARE_METHOD_PTRS(CKContext, void, ExecuteManagersOnPostSpriteRender, (CKRenderContext *dev));

    static bool InitHooks();
    static void ShutdownHooks();
};

#endif /* HOOKS_CONTEXTHOOK_H */
