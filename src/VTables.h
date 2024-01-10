#ifndef BML_VTABLES_H
#define BML_VTABLES_H

#include "Macros.h"

#include "CKBaseManager.h"
#include "CKInputManager.h"

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

struct CP_CLASS_VTABLE_NAME(CKInputManager) : public CP_CLASS_VTABLE_NAME(CKBaseManager)<CKInputManager> {
    CP_DECLARE_METHOD_PTR(CKInputManager, void, EnableKeyboardRepetition, (CKBOOL iEnable));
    CP_DECLARE_METHOD_PTR(CKInputManager, CKBOOL, IsKeyboardRepetitionEnabled, ());
    CP_DECLARE_METHOD_PTR(CKInputManager, CKBOOL, IsKeyDown, (CKDWORD iKey, CKDWORD *oStamp));
    CP_DECLARE_METHOD_PTR(CKInputManager, CKBOOL, IsKeyUp, (CKDWORD iKey));
    CP_DECLARE_METHOD_PTR(CKInputManager, CKBOOL, IsKeyToggled ,(CKDWORD iKey, CKDWORD *oStamp));
    CP_DECLARE_METHOD_PTR(CKInputManager, void, GetKeyName, (CKDWORD iKey, CKSTRING oKeyName));
    CP_DECLARE_METHOD_PTR(CKInputManager, CKDWORD, GetKeyFromName, (CKSTRING iKeyName));
    CP_DECLARE_METHOD_PTR(CKInputManager, unsigned char *, GetKeyboardState, ());
    CP_DECLARE_METHOD_PTR(CKInputManager, CKBOOL, IsKeyboardAttached, ());
    CP_DECLARE_METHOD_PTR(CKInputManager, int, GetNumberOfKeyInBuffer, ());
    CP_DECLARE_METHOD_PTR(CKInputManager, int, GetKeyFromBuffer, (int i, CKDWORD &oKey, CKDWORD *oTimeStamp));
    CP_DECLARE_METHOD_PTR(CKInputManager, CKBOOL, IsMouseButtonDown, (CK_MOUSEBUTTON iButton));
    CP_DECLARE_METHOD_PTR(CKInputManager, CKBOOL, IsMouseClicked, (CK_MOUSEBUTTON iButton));
    CP_DECLARE_METHOD_PTR(CKInputManager, CKBOOL, IsMouseToggled, (CK_MOUSEBUTTON iButton));
    CP_DECLARE_METHOD_PTR(CKInputManager, void, GetMouseButtonsState, (CKBYTE oStates[4]));
    CP_DECLARE_METHOD_PTR(CKInputManager, void, GetMousePosition, (Vx2DVector &oPosition, CKBOOL iAbsolute));
    CP_DECLARE_METHOD_PTR(CKInputManager, void, GetMouseRelativePosition, (VxVector &oPosition));
    CP_DECLARE_METHOD_PTR(CKInputManager, CKBOOL, IsMouseAttached, ());
    CP_DECLARE_METHOD_PTR(CKInputManager, CKBOOL, IsJoystickAttached, (int iJoystick));
    CP_DECLARE_METHOD_PTR(CKInputManager, void, GetJoystickPosition, (int iJoystick, VxVector *oPosition));
    CP_DECLARE_METHOD_PTR(CKInputManager, void, GetJoystickRotation, (int iJoystick, VxVector *oRotation));
    CP_DECLARE_METHOD_PTR(CKInputManager, void, GetJoystickSliders, (int iJoystick, Vx2DVector *oPosition));
    CP_DECLARE_METHOD_PTR(CKInputManager, void, GetJoystickPointOfViewAngle, (int iJoystick, float *oAngle));
    CP_DECLARE_METHOD_PTR(CKInputManager, CKDWORD, GetJoystickButtonsState, (int iJoystick));
    CP_DECLARE_METHOD_PTR(CKInputManager, CKBOOL, IsJoystickButtonDown, (int iJoystick, int iButton));
    CP_DECLARE_METHOD_PTR(CKInputManager, void, Pause, (CKBOOL pause));
    CP_DECLARE_METHOD_PTR(CKInputManager, void, ShowCursor, (CKBOOL iShow));
    CP_DECLARE_METHOD_PTR(CKInputManager, CKBOOL, GetCursorVisibility, ());
    CP_DECLARE_METHOD_PTR(CKInputManager, VXCURSOR_POINTER, GetSystemCursor, ());
    CP_DECLARE_METHOD_PTR(CKInputManager, void, SetSystemCursor, (VXCURSOR_POINTER cursor));
};

class CKIpionManager : public CKBaseManager {
public:
    virtual void Reset() = 0;
};

struct CP_CLASS_VTABLE_NAME(CKIpionManager) : public CP_CLASS_VTABLE_NAME(CKBaseManager)<CKIpionManager> {
    CP_DECLARE_METHOD_PTR(CKIpionManager, void, Reset, ());
};

#endif // BML_VTABLES_H
