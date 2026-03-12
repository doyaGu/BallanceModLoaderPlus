/**
 * @file VTables.h
 * @brief CKInputManager VTable structure for BML_Input module
 * 
 * Self-contained VTable definitions without BMLPlus dependencies.
 */

#ifndef BML_INPUT_VTABLES_H
#define BML_INPUT_VTABLES_H

#include "CKInputManager.h"
#include "Macros.h"

// CKBaseManager VTable
template <class T>
struct CP_CLASS_VTABLE_NAME(CKBaseManager) {
    CP_DECLARE_METHOD_PTR(T, void, Destructor, ());
    CP_DECLARE_METHOD_PTR(T, CKStateChunk *, SaveData, (CKFile *SavedFile));
    CP_DECLARE_METHOD_PTR(T, CKERROR, LoadData, (CKStateChunk *chunk, CKFile *LoadedFile));
    CP_DECLARE_METHOD_PTR(T, CKERROR, PreClearAll, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, PostClearAll, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, PreProcess, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, PostProcess, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, SequenceAddedToScene, (CKScene *scn, CK_ID *objids, int count));
    CP_DECLARE_METHOD_PTR(T, CKERROR, SequenceRemovedFromScene, (CKScene *scn, CK_ID *objids, int count));
    CP_DECLARE_METHOD_PTR(T, CKERROR, PreLaunchScene, (CKScene *OldScene, CKScene *NewScene));
    CP_DECLARE_METHOD_PTR(T, CKERROR, PostLaunchScene, (CKScene *OldScene, CKScene *NewScene));
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnCKInit, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnCKEnd, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnCKReset, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnCKPostReset, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnCKPause, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnCKPlay, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, SequenceToBeDeleted, (CK_ID *objids, int count));
    CP_DECLARE_METHOD_PTR(T, CKERROR, SequenceDeleted, (CK_ID *objids, int count));
    CP_DECLARE_METHOD_PTR(T, CKERROR, PreLoad, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, PostLoad, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, PreSave, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, PostSave, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnPreCopy, (CKDependenciesContext &context));
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnPostCopy, (CKDependenciesContext &context));
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnPreRender, (CKRenderContext *dev));
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnPostRender, (CKRenderContext *dev));
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnPostSpriteRender, (CKRenderContext *dev));
    CP_DECLARE_METHOD_PTR(T, int, GetFunctionPriority, (CKMANAGER_FUNCTIONS Function));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetValidFunctionsMask, ());
};

// CKInputManager VTable (extends CKBaseManager)
template <class T>
struct CP_CLASS_VTABLE_NAME(CKInputManager) : public CP_CLASS_VTABLE_NAME(CKBaseManager)<CKInputManager> {
    CP_DECLARE_METHOD_PTR(T, void, EnableKeyboardRepetition, (CKBOOL iEnable));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsKeyboardRepetitionEnabled, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsKeyDown, (CKDWORD iKey, CKDWORD *oStamp));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsKeyUp, (CKDWORD iKey));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsKeyToggled, (CKDWORD iKey, CKDWORD *oStamp));
    CP_DECLARE_METHOD_PTR(T, void, GetKeyName, (CKDWORD iKey, CKSTRING oKeyName));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetKeyFromName, (CKSTRING iKeyName));
    CP_DECLARE_METHOD_PTR(T, unsigned char *, GetKeyboardState, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsKeyboardAttached, ());
    CP_DECLARE_METHOD_PTR(T, int, GetNumberOfKeyInBuffer, ());
    CP_DECLARE_METHOD_PTR(T, int, GetKeyFromBuffer, (int i, CKDWORD &oKey, CKDWORD *oTimeStamp));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsMouseButtonDown, (CK_MOUSEBUTTON iButton));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsMouseClicked, (CK_MOUSEBUTTON iButton));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsMouseToggled, (CK_MOUSEBUTTON iButton));
    CP_DECLARE_METHOD_PTR(T, void, GetMouseButtonsState, (CKBYTE oStates[4]));
    CP_DECLARE_METHOD_PTR(T, void, GetMousePosition, (Vx2DVector &oPosition, CKBOOL iAbsolute));
    CP_DECLARE_METHOD_PTR(T, void, GetMouseRelativePosition, (VxVector &oPosition));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsMouseAttached, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsJoystickAttached, (int iJoystick));
    CP_DECLARE_METHOD_PTR(T, void, GetJoystickPosition, (int iJoystick, VxVector *oPosition));
    CP_DECLARE_METHOD_PTR(T, void, GetJoystickRotation, (int iJoystick, VxVector *oRotation));
    CP_DECLARE_METHOD_PTR(T, void, GetJoystickSliders, (int iJoystick, Vx2DVector *oPosition));
    CP_DECLARE_METHOD_PTR(T, void, GetJoystickPointOfViewAngle, (int iJoystick, float *oAngle));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetJoystickButtonsState, (int iJoystick));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsJoystickButtonDown, (int iJoystick, int iButton));
    CP_DECLARE_METHOD_PTR(T, void, Pause, (CKBOOL pause));
    CP_DECLARE_METHOD_PTR(T, void, ShowCursor, (CKBOOL iShow));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, GetCursorVisibility, ());
    CP_DECLARE_METHOD_PTR(T, VXCURSOR_POINTER, GetSystemCursor, ());
    CP_DECLARE_METHOD_PTR(T, void, SetSystemCursor, (VXCURSOR_POINTER cursor));
};

#endif // BML_INPUT_VTABLES_H
